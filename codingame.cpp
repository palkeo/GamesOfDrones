#include <iostream>
#include <cmath>
#include <vector>
#include <limits>
#include <queue>
#include <set>
#include <algorithm>
#include <chrono>

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
    Zone* going_to;
    Zone* inside;

    Drone(int id_, int team_) : id(id_), team(team_), old_x(-1), going_to(NULL), inside(NULL)
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
};

class Zone : public Point
{
    public:

    static const int RADIUS;
    static const float OCCUPATION_SCORE_TAU;

    vector<Drone*> drones_going;
    vector<Drone*> drones_sorted;

    int team;
    float occupation_score;
    int id;

    Zone(int id_, int x_, int y_) : occupation_score(1), id(id_)
    {
        x = x_;
        y = y_;
    }

    void update()
    {
        unsigned char drones_inside = 0;
        for(auto it = drones_sorted.begin(); it != drones_sorted.end() && distance(*it) <= RADIUS; it++, drones_inside++);
        occupation_score = OCCUPATION_SCORE_TAU*occupation_score + (1.f - OCCUPATION_SCORE_TAU)*drones_inside;

        bool moved;
        do
        {
            moved = false;
            for(unsigned char i = 1; i < drones_sorted.size(); i++)
            {
                if(distance(drones_sorted[i-1]) > distance(drones_sorted[i]))
                {
                    Drone* t = drones_sorted[i-1];
                    drones_sorted[i-1] = drones_sorted[i];
                    drones_sorted[i] = t;
                    moved = true;
                }
            }
        }
        while(moved);
    }
};

class Game
{
    public:

    static const chrono::milliseconds MAX_TIME;
    static const float TAU;

    vector<Zone*> zones;
    set<Zone*> main_zones;
    vector< vector<Drone*>* > teams;
    vector<Drone*> drones;
    int my_team;
    int nb_teams;
    int nb_drones;
    unsigned int nb_zones;
    int turn;
    bool print_dump;

    vector<Zone*> best_action;
    float best_action_score;

    unsigned int nb_evals;
    chrono::time_point<chrono::steady_clock> turn_time_start;

    Game(bool print_dump_) : turn(0), print_dump(print_dump_)
    {
        // paramètres

        cin >> nb_teams >> my_team >> nb_drones >> nb_zones;

        // zones

        for(unsigned char z = 0; z < nb_zones; ++z)
        {
            int x, y;
            cin >> x >> y;
            zones.push_back(new Zone(z, x, y));
        }

        // meilleur triangle

        float best_score = numeric_limits<float>::max();
        for(auto zi1 = zones.begin(); zi1 != zones.end(); zi1++)
        for(auto zi2 = zi1 + 1; zi2 != zones.end(); zi2++)
        for(auto zi3 = zi2 + 1; zi3 != zones.end(); zi3++)
        {
            float c1 = (*zi1)->distance(*zi2);
            float c2 = (*zi2)->distance(*zi3);
            float c3 = (*zi1)->distance(*zi3);
            float score = c1 + c2 + c3 + 2*max(c1, max(c2, c3));
            if(score < best_score)
            {
                main_zones.clear();
                main_zones.insert(*zi1);
                main_zones.insert(*zi2);
                main_zones.insert(*zi3);
                best_score = score;
            }
        }

        // création des drones

        for(int p = 0; p < nb_teams; ++p)
        {
            vector<Drone*>* dt = new vector<Drone*>();
            for(int d = 0; d < nb_drones; ++d)
            {
                Drone* pt = new Drone(d, p);
                dt->push_back(pt);
                drones.push_back(pt);
                best_action.push_back(NULL);
                for(Zone* z : zones)
                    z->drones_sorted.push_back(pt);
            }
            teams.push_back(dt);
        }
    }

    void update()
    {
        turn++;

        // update zones
        for(Zone* zone : zones)
        {
            cin >> zone->team;
            zone->drones_going.clear();
        }
        
        // update drones
        for(vector<Drone*>* team : teams)
        {
            for(Drone* drone : *team)
            {
                int nx, ny;
                cin >> nx >> ny;
                drone->update(nx, ny);
 
                drone->inside = NULL;
                Zone* mz = NULL;
                float md = numeric_limits<float>::max();
                for(Zone* zone : zones)
                {
                    if(drone->distance(zone) <= Zone::RADIUS)
                        drone->inside = zone;
                    if(drone->distance_direction_point(zone) <= md)
                    {
                        md = drone->distance_direction_point(zone);
                        mz = zone;
                    }
                }
                mz->drones_going.push_back(drone);
                drone->going_to = mz;
            }
        }

        for(Zone* zone : zones)
            zone->update();
    }

    // AI BEGIN
    
    inline void eval_action(vector<unsigned char>& drones_by_zones)
    {
        nb_evals++;

    }
    
    void find_best_action(vector<unsigned char> drones_by_zones, unsigned char s)
    {
        if(drones_by_zones.size() == nb_zones - 1)
        {
            drones_by_zones.push_back(s);
            eval_action(drones_by_zones);
            return;
        }

        for(unsigned char i = 0; i <= s; i++)
        {
            vector<unsigned char> dbz = drones_by_zones;
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
            cerr << nb_teams << " " << my_team << " " << nb_drones << " " << nb_zones << endl;
            for(Zone* zone : zones)
                cerr << zone->x << " " << zone->y << endl;
            for(Zone* zone : zones)
                cerr << zone->team << endl;
            for(vector<Drone*>* v : teams)
            {
                for(Drone* drone : *v)
                    cerr << drone->x << " " << drone->y << endl;
            }
            cerr << "---- END DUMP ----" << endl << endl;
        }

        cerr << "[Info] nb_evals = " << nb_evals << ", time = " << chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - turn_time_start).count() << " ms" << endl;

        for(Zone* zone : best_action)
            cout << zone->x << " " << zone->y << endl;
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
