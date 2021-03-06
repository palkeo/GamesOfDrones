#include <iostream>
#include <cmath>
#include <vector>
#include <limits>
#include <queue>
#include <set>
#include <chrono>
#include <map>

#define TIME_PROFILE
//#define PRINT_TREE

using namespace std;

struct Point
{
    int x;
    int y;

    Point() : x(0), y(0) {}

    Point(int x_, int y_) : x(x_), y(y_) {}

    float distance(const Point& other) const
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

struct Drone : public Point
{
    static const int SPEED;

    int id;
    int team;
    int old_x, old_y;
    Zone* going_to;
    Zone* nearest;

    Drone(int id_, int team_) : id(id_), team(team_), old_x(-1), going_to(NULL) {}

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

struct Zone : public Point
{
    static const int RADIUS;

    vector<Drone*> drones_sorted;

    int team;
    int id;

    Zone(int id_, int x_, int y_) : id(id_)
    {
        x = x_;
        y = y_;
    }

    void update(int my_team)
    {
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
    private:

    vector<Zone*> zones;
    vector< vector<Drone*>* > teams;
    vector<Drone*> drones;
    int my_team;
    int nb_teams;
    int nb_drones;
    int nb_zones;
    int turn;
    bool print_dump;
    int nb_recurse;
    int recurse_width;
    chrono::time_point<chrono::steady_clock> recurse_time_start;

    public:

    static const chrono::milliseconds MAX_TIME;
    static const int NB_TURNS;
    static const double TAU;

    Game(bool print_dump_) : turn(0), print_dump(print_dump_), recurse_width(40)
    {
        cin >> nb_teams >> my_team >> nb_drones >> nb_zones;

        for(int z = 0; z < nb_zones; ++z)
        {
            int x, y;
            cin >> x >> y;
            zones.push_back(new Zone(z, x, y));
        }

        for(int p = 0; p < nb_teams; ++p)
        {
            vector<Drone*>* dt = new vector<Drone*>();
            for(int d = 0; d < nb_drones; ++d)
            {
                Drone* pt = new Drone(d, p);
                dt->push_back(pt);
                drones.push_back(pt);
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
            cin >> zone->team;
        
        // update drones
        for(vector<Drone*>* team : teams)
        {
            for(Drone* drone : *team)
            {
                int nx, ny;
                cin >> nx >> ny;
                drone->update(nx, ny);
 
                Zone* mz = NULL;
                float md = numeric_limits<float>::max();
                Zone* nz = NULL;
                float nd = numeric_limits<float>::max();
                for(Zone* zone : zones)
                {
                    if(drone->distance_direction_point(zone) < md)
                    {
                        md = drone->distance_direction_point(zone);
                        mz = zone;
                    }
                    if(drone->distance(zone) < nd)
                    {
                        nd = drone->distance(zone);
                        nz = zone;
                    }
                }
                drone->going_to = mz;
                drone->nearest = nz;
            }
        }

        for(Zone* zone : zones)
            zone->update(my_team);
    }

    private: // AI BEGIN
    
    struct ZoneAction
    {
        double score;
        Zone* zone;
        vector<Drone*> drones;

        ZoneAction(double s, Zone* z) : score(s), zone(z) {}
        ZoneAction() : score(0) {}

        inline bool operator<(const ZoneAction& other) const
        {
            return score < other.score;
        }
    };

    struct Action
    {
        double score;
        map<Drone*, Zone*> moves;

        Action() : score(0) {}
    };
    
    Action recurse(set<Zone*> available_zones, set<Drone*> available_drones)
    {
#ifdef TIME_PROFILE
        if(available_zones.empty() || chrono::steady_clock::now() - recurse_time_start > MAX_TIME) return Action();
#else
        if(available_zones.empty()) return Action();
#endif

        nb_recurse++;

        priority_queue<ZoneAction> actions;

        int turns_to_simulate = NB_TURNS - turn;

        for(Zone* zone : available_zones)
        {
            // calculate the actions

            double last_score = 0;
            for(unsigned char my_count_max = int(! available_drones.empty()); my_count_max <= min(available_drones.size(), (size_t)ceil(nb_drones * nb_teams / float(nb_zones))); my_count_max++)
            {
                double score = 0;
                bool is_mine = (my_team == zone->team);

                auto drones_iter = zone->drones_sorted.begin();

                unsigned char my_count = 0;
                unsigned char foe_count = 0;
                unsigned char foe_count_table[4] = {0, 0, 0, 0};

                int t = 1;

                while(t < turns_to_simulate)
                {
                    int dist = t * Drone::SPEED + Zone::RADIUS;
                    float min_dist = 1000000000;

                    while(drones_iter != zone->drones_sorted.end() && (min_dist = (*drones_iter)->distance(zone)) <= dist)
                    {
                        if((*drones_iter)->team == my_team)
                        {
                            if(my_count < my_count_max && available_drones.count(*drones_iter))
                                my_count++;
                        }
                        else
                        {
                            if(((*drones_iter)->nearest == zone || (*drones_iter)->going_to == zone) && ++foe_count_table[(*drones_iter)->team] > foe_count)
                                foe_count++;
                        }
                        drones_iter++;
                    }

                    int increment = ceil((min_dist - dist) / float(Drone::SPEED));
                    if(increment + t > turns_to_simulate)
                        increment = turns_to_simulate - t;

                    is_mine = my_count > foe_count || (my_count == foe_count && is_mine);

                    score += int(is_mine)*(exp(t*TAU) - exp((t+increment)*TAU));

                    t += increment;
                }

                if(score > last_score)
                {
                    last_score = score;
                    ZoneAction za(score, zone);
                    my_count = 0;
                    for(auto j = zone->drones_sorted.begin(); j != zone->drones_sorted.end() && my_count < my_count_max; j++)
                    {
                        if((*j)->team == my_team && available_drones.count(*j))
                        {
                            my_count++;
                            za.drones.push_back(*j);
                        }
                    }
                    actions.push(za);
                }

                // For speed !
                // If we have more drones going to this zone, it will be completely useless as it is already to us at the end.
                if(is_mine)
                    break;
            }

        }

        Action best_action = Action();
        int i = 0;
        while((! actions.empty()) && i < recurse_width / 4)
        {
            i++;

            ZoneAction action = actions.top();
            actions.pop();

            set<Zone*> sub_available_zones = available_zones;
            sub_available_zones.erase(action.zone);

            set<Drone*> sub_available_drones = available_drones;
            for(Drone* j : action.drones)
                sub_available_drones.erase(j);

#ifdef PRINT_TREE
            for(unsigned char tr = available_zones.size(); tr < zones.size(); tr++)
                cout << " ";
            cout << "> " << action.zone->id << " <- " << action.drones.size() << ", " << action.absolute_score << endl;
#endif
            Action subaction = recurse(sub_available_zones, sub_available_drones);

            subaction.score += action.score;

#ifdef PRINT_TREE
            for(unsigned char tr = available_zones.size(); tr < zones.size(); tr++)
                cout << " ";
            cout << "< " << subaction.score << endl;
#endif

            if(subaction.score > best_action.score)
            {
                best_action = subaction;
                for(Drone* j: action.drones)
                    best_action.moves[j] = action.zone;
            }
        }

        return best_action;
    }

    Point move_drone(Drone* drone)
    {
        auto cmp_func = [&] (Zone* a, Zone* b) { return a->distance(drone) > b->distance(drone); };
        std::priority_queue<Zone*, vector<Zone*>, decltype(cmp_func)> q(cmp_func);

        size_t nbz = 2 + (drone->id % 2);

        for(Zone* zone : zones)
        {
            if(zone->team == my_team)
                q.push(zone);
        }
        if(q.size() < nbz)
        {
            for(Zone* zone : zones)
                if(zone->team != my_team)
                    q.push(zone);
        }

        int xs = 0;
        int ys = 0;
        for(size_t i = 0; i < nbz; i++)
        {
            xs += q.top()->x;
            ys += q.top()->y;
            q.pop();
        }
        return Point(xs/nbz, ys/nbz);
    }

    Point move_drone(Drone* drone, Zone* zone)
    {
        if(zone->distance(drone) >= Zone::RADIUS)
            return *zone;
        Point o = move_drone(drone);
        float d = zone->distance(o);
        float s = Zone::RADIUS - 2.f - 2*(turn % 2);
        return Point(zone->x + s*(o.x - zone->x)/d, zone->y + s*(o.y - zone->y)/d);
    }

    public:

    void play()
    {
        nb_recurse = 0;
        recurse_time_start = chrono::steady_clock::now();
        Action result = recurse(set<Zone*>(zones.begin(), zones.end()), set<Drone*>(teams[my_team]->begin(), teams[my_team]->end()));

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

#ifdef TIME_PROFILE
        if(chrono::steady_clock::now() - recurse_time_start > MAX_TIME)
        {
            if(recurse_width > 20)
                recurse_width = 20;
            else if(recurse_width > 8)
                recurse_width-=4;
            cerr << "[Warning] Too long. recurse_width decreased." << endl;
        }
        else
            recurse_width++;
#endif

        for(int i = 0; i < nb_drones; ++i)
        {
            auto drone = (*teams[my_team])[i];
            auto it = result.moves.find(drone);
            if(it == result.moves.end())
            {
                cerr << "[Info] Drone " << i << " had nothing to do." << endl;
                Point p = move_drone(drone);
                cout << p.x << " " << p.y << endl;
            }
            else
            {
                Point p = move_drone(drone, it->second);
                cout << p.x << " " << p.y << endl;
            }
        }
        cerr << "[Info] nb_recurse = " << nb_recurse << ", recurse_width = " << recurse_width << ", time = " << chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - recurse_time_start).count() << " ms" << endl;
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
const int Drone::SPEED = 100;
const chrono::milliseconds Game::MAX_TIME = chrono::milliseconds(90);
const int Game::NB_TURNS = 202;
const double Game::TAU = -0.7;
