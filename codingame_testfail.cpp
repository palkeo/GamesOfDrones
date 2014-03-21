#include <iostream>
#include <cmath>
#include <vector>
#include <limits>
#include <queue>
#include <set>
#include <algorithm>
#include <chrono>
#include <memory>

using namespace std;

class Point
{
    public:

    int x;
    int y;

    Point() : x(0), y(0)
    {
    }

    Point(int x_, int y_) : x(x_), y(y_)
    {
    }

    float distance(const Point other) const
    {
        float dx = x - other.x;
        float dy = y - other.y;
        return sqrt(dx*dx + dy*dy);
    }

    float distance(const Point* other) const
    {
        float dx = x - other->x;
        float dy = y - other->y;
        return sqrt(dx*dx + dy*dy);
    }

};

class Zone;

class Drone : public Point
{
    public:

    static const int SPEED;

    int id;
    int team;
    int old_x, old_y;

    Drone(int id_, int team_) : id(id_), team(team_), old_x(-1)
    {
    }

    void update(int nx, int ny)
    {
        if(old_x == -1)
        {
            old_x = nx;
            old_y = ny;
        }
        else
        {
            old_x = x;
            old_y = y;
        }
        x = nx;
        y = ny;
    }

    float distance_direction_point(const Point* point) const
    {
        if(x == old_x && y == old_y)
            return distance(point);
        float u = ((point->x - old_x)*(x - old_x) + (point->y - old_y)*(y - old_y)) / float((x-old_x)*(x-old_x) + (y-old_y)*(y-old_y));
        if(u < 0)
            u = 0;
        float dx = old_x + u*(x-old_x) - point->x;
        float dy = old_y + u*(y-old_y) - point->y;
        return sqrt(dx*dx + dy*dy);
    }

    void move_to(Point& p, int d)
    {
        float angle = atan2(p.y - y, p.x - x);
        float nd = min(float(d), distance(p));
        x += cos(angle)*nd;
        y += sin(angle)*nd;
    }
};

class Zone : public Point
{
    public:

    static const int RADIUS;
    static const float OCCUPATION_SCORE_TAU;

    int team;
    float occupation_score;
    int id;

    Zone(int id_, int x_, int y_) : occupation_score(1), id(id_)
    {
        x = x_;
        y = y_;
    }

    Zone() : id(-1)
    {
    }

    void update_occupation_score(unsigned char drones_inside)
    {
        occupation_score = OCCUPATION_SCORE_TAU*occupation_score + (1.f - OCCUPATION_SCORE_TAU)*drones_inside;
    }
};

struct State
{
    vector<Zone> zones;
    vector<vector<Drone> > teams;

    int turn;
    int my_team;

    State(int team) : turn(0), my_team(team) {}

};

class Game
{
    public:

    static const chrono::milliseconds MAX_TIME;
    static const float TAU;
    static const int NB_TURNS;

    int nb_teams;
    int nb_drones;
    unsigned int nb_zones;
    bool print_dump;

    shared_ptr<State> state;

    vector<Zone> best_action;
    float best_action_score;

    unsigned int nb_evals;
    chrono::time_point<chrono::steady_clock> turn_time_start;

    Game(bool print_dump_) : print_dump(print_dump_)
    {
        // paramètres

        int my_team;
        cin >> nb_teams >> my_team >> nb_drones >> nb_zones;

        state = shared_ptr<State>(new State(my_team));

        // zones

        for(unsigned char z = 0; z < nb_zones; ++z)
        {
            int x, y;
            cin >> x >> y;
            state->zones.push_back(Zone(z, x, y));
        }

        // création des drones

        for(int p = 0; p < nb_teams; ++p)
        {
            vector<Drone> dt;
            for(int d = 0; d < nb_drones; ++d)
                dt.push_back(Drone(d, p));
            state->teams.push_back(dt);
        }

        for(int d = 0; d < nb_drones; ++d)
            best_action.push_back(Zone());
    }

    void update()
    {
        state->turn++;

        // update zones
        for(Zone& zone : state->zones)
        {
            cin >> zone.team;
        }
        
        // update drones
        for(vector<Drone>& team : state->teams)
        {
            for(Drone& drone : team)
            {
                int nx, ny;
                cin >> nx >> ny;
                drone.update(nx, ny);
            }
        }

        for(Zone& zone : state->zones)
        {
            unsigned char drones_inside = 0;
            for(vector<Drone>& team : state->teams)
            for(Drone& drone : team)
            {
                if(zone.distance(drone) <= Zone::RADIUS)
                    drones_inside++;
            }
            zone.update_occupation_score(drones_inside);
        }
    }

    // AI BEGIN
    
    inline void eval_action(vector<unsigned char>& nb_drones_by_zones)
    {
        nb_evals++;

        State s = *state;

        // have my drones head to the zones.
        {
            set<Drone*> my_drones_moved;
            for(unsigned char i = 0; i < s.zones.size(); i++)
            {
                Zone& zone = s.zones[i];
                auto dist_cmp = [&zone] (Drone* a, Drone* b) { return a->distance(zone) > b->distance(zone); };
                priority_queue<Drone*, vector<Drone*>, decltype(dist_cmp)> p(dist_cmp);
                for(Drone& drone : s.teams[s.my_team])
                {
                    if(! my_drones_moved.count(&drone))
                        p.push(&drone);
                }

                for(unsigned char nb = 0; nb < nb_drones_by_zones[i]; nb++)
                {
                    p.top()->move_to(zone, Drone::SPEED);
                    my_drones_moved.insert(p.top());
                    p.pop();
                }
            }
        }

        float score = 0;
        
        for(Zone& zone : s.zones)
        {
            auto dist_cmp = [&zone] (Drone* a, Drone* b) { return a->distance(zone) > b->distance(zone); };
            priority_queue<Drone*, vector<Drone*>, decltype(dist_cmp)> p(dist_cmp);
            for(vector<Drone>& drones : s.teams)
            for(Drone& drone : drones)
                p.push(&drone);

            bool is_mine = (zone.team == s.my_team);

            unsigned char my_count = 0;
            unsigned char foe_count = 0;
            unsigned char foe_count_table[4] = {0, 0, 0, 0};
            
            int t = 1;

            while(t < 1000000)
            {
                int dist = t * Drone::SPEED + Zone::RADIUS;
                float min_dist = 1000000000;

                while((!p.empty()) && (min_dist = p.top()->distance(zone)) <= dist)
                {
                    if(p.top()->team == s.my_team)
                        my_count++;
                    else if(++foe_count_table[p.top()->team] > foe_count)
                        foe_count++;
                    p.pop();
                }

                int increment = ceil((min_dist - Zone::RADIUS) / float(Drone::SPEED));

                is_mine = my_count > foe_count || (my_count == foe_count && is_mine);
                score += (int(is_mine) + (my_count - foe_count)*1e-2) * (exp(-t*TAU) - exp(-(t+increment)*TAU));

                t += increment;
            }
        }

        if(score < best_action_score)
        {
            set<Drone*> my_drones_moved;
            for(unsigned char i = 0; i < s.zones.size(); i++)
            {
                Zone& zone = s.zones[i];
                auto dist_cmp = [&zone] (Drone* a, Drone* b) { return a->distance(zone) > b->distance(zone); };
                priority_queue<Drone*, vector<Drone*>, decltype(dist_cmp)> p(dist_cmp);
                for(Drone& drone : s.teams[s.my_team])
                {
                    if(! my_drones_moved.count(&drone))
                        p.push(&drone);
                }

                for(unsigned char nb = 0; nb < nb_drones_by_zones[i]; nb++)
                {
                    best_action[p.top()->id] = zone;
                    my_drones_moved.insert(p.top());
                    p.pop();
                }
            }
        }
    }
    
    void find_best_action(vector<unsigned char> nb_drones_by_zones, unsigned char s)
    {
        if(nb_drones_by_zones.size() == nb_zones - 1)
        {
            nb_drones_by_zones.push_back(s);
            eval_action(nb_drones_by_zones);
            return;
        }

        for(unsigned char i = 0; i <= s; i++)
        {
            vector<unsigned char> dbz = nb_drones_by_zones;
            dbz.push_back(i);
            find_best_action(dbz, s - i);
        }
    }
    
    void play()
    {
        turn_time_start = chrono::steady_clock::now();

        best_action_score = numeric_limits<float>::max();
        nb_evals = 0;
        find_best_action(vector<unsigned char>(), nb_drones);

        if(print_dump)
        {
            cerr << "---- DUMP ----" << endl;
            cerr << nb_teams << " " << state->my_team << " " << nb_drones << " " << nb_zones << endl;
            for(Zone& zone : state->zones)
                cerr << zone.x << " " << zone.y << endl;
            for(Zone& zone : state->zones)
                cerr << zone.team << endl;
            for(vector<Drone>& v : state->teams)
            {
                for(Drone& drone : v)
                    cerr << drone.x << " " << drone.y << endl;
            }
            cerr << "---- END DUMP ----" << endl << endl;
        }

        cerr << "[Info] nb_evals = " << nb_evals << ", time = " << chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - turn_time_start).count() << " ms" << endl;

        for(Zone& zone : best_action)
            cout << zone.x << " " << zone.y << endl;
    }
};


int main(int argc, char **argv)
{
    Game g(argc == 1);

    bool oneshot = argc > 1 && string(argv[1]) == "oneshot";
    do
    {
        g.update();
        g.play();
    }
    while(! oneshot);
    return 0;
}

const int Zone::RADIUS = 100;
const float Zone::OCCUPATION_SCORE_TAU = 0.99;
const int Drone::SPEED = 100;
const chrono::milliseconds Game::MAX_TIME = chrono::milliseconds(90);
const float Game::TAU = 0.02;
const int Game::NB_TURNS = 200;
