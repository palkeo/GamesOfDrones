#include <iostream>
#include <cmath>
#include <vector>
#include <limits>
#include <queue>
#include <set>
#include <algorithm>
#include <chrono>

#define TIME_PROFILE
//#define PRINT_TREE

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
    Zone* nearest;

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
    int nb_zones;
    int turn;
    bool print_dump;
    int nb_recurse;
    int recurse_width;
    chrono::time_point<chrono::steady_clock> recurse_time_start;

    Game(bool print_dump_) : turn(0), print_dump(print_dump_), recurse_width(10)
    {
        cin >> nb_teams >> my_team >> nb_drones >> nb_zones;

        for(int z = 0; z < nb_zones; ++z)
        {
            int x, y;
            cin >> x >> y;
            zones.push_back(new Zone(z, x, y));
        }

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
                Zone* nz = NULL;
                float nd = numeric_limits<float>::max();
                for(Zone* zone : zones)
                {
                    if(drone->distance(zone) <= Zone::RADIUS)
                        drone->inside = zone;
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
                mz->drones_going.push_back(drone);
                drone->going_to = mz;
                drone->nearest = nz;
            }
        }

        for(Zone* zone : zones)
            zone->update();
    }

    // AI BEGIN
    
    struct ZoneAction
    {
        float absolute_score;
        float relative_score;
        Zone* zone;
        vector<Drone*> drones;

        ZoneAction(float as, float rs, Zone* z) : absolute_score(as), relative_score(rs), zone(z)
        {
        }

        ZoneAction() : absolute_score(0), relative_score(0)
        {
        }

        inline bool operator<(const ZoneAction& other) const
        {
            return relative_score < other.relative_score;
        }
    };

    struct Action
    {
        float score;
        vector<pair<Drone*, Zone*> > moves;

        Action() : score(0)
        {
        }
    };
    
    Action recurse(set<Zone*> available_zones, set<Drone*> available_drones)
    {
#ifdef TIME_PROFILE
        if(available_zones.empty() || available_drones.empty() || chrono::steady_clock::now() - recurse_time_start > MAX_TIME) return Action();
#else
        if(available_zones.empty() || available_drones.empty()) return Action();
#endif

       nb_recurse++;

        priority_queue<ZoneAction> actions;

        for(Zone* zone : available_zones)
        {
            // calculate the actions

            float last_score = 0;
            for(unsigned char my_count_max = 0; my_count_max <= available_drones.size(); my_count_max++)
            {
                float score = 0;
                bool is_mine = (my_team == zone->team);

                auto drones_iter = zone->drones_sorted.begin();

                unsigned char my_count = 0;
                unsigned char foe_count = 0;
                unsigned char foe_count_table[4] = {0, 0, 0, 0};

                int t = 1;

                while(t < 1000000)
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
                            if(((*drones_iter)->going_to == zone || (*drones_iter)->nearest == zone) && ++foe_count_table[(*drones_iter)->team] > foe_count)
                                foe_count++;
                        }
                        drones_iter++;
                    }

                    int increment = ceil((min_dist - Zone::RADIUS) / float(Drone::SPEED));

                    is_mine = my_count > foe_count || (my_count == foe_count && is_mine);
                    score += int(is_mine)*(exp(-t*TAU) - exp(-(t+increment)*TAU));

                    t += increment;
                }

                //score /= zone->occupation_score;
                /*
                if(main_zones.count(zone))
                    score *= 10;
                */

                if(score > last_score)
                {
                    last_score = score;
                    ZoneAction za(score, score / float(my_count ? my_count : 0.1), zone);
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
                if(is_mine)
                    break;
            }

        }

        Action best_action = Action();
        int i = 0;
        while((! actions.empty()) && i < recurse_width)
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

            subaction.score += action.absolute_score;

#ifdef PRINT_TREE
            for(unsigned char tr = available_zones.size(); tr < zones.size(); tr++)
                cout << " ";
            cout << "< " << subaction.score << endl;
#endif

            if(subaction.score > best_action.score)
            {
                best_action = subaction;
                for(Drone* j: action.drones)
                    best_action.moves.push_back(pair<Drone*, Zone*>(j, action.zone));
            }
        }

        return best_action;
    }

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
            if(recurse_width > 5)
                recurse_width = 5;
            else if(recurse_width > 2)
                recurse_width--;
            cerr << "[Warning] Too long. recurse_width decreased." << endl;
        }
        else
            recurse_width++;
#endif

        for(int i = 0; i < nb_drones; ++i)
        {
            bool found = false;
            for(pair<Drone*, Zone*> p : result.moves)
            {
                if(p.first->id == i)
                {
                    cout << p.second->x << " " << p.second->y << endl;
                    found = true;
                    break;
                }
            }
            if(! found)
            {
                Zone* best_zone = zones[0];
                float best_score = numeric_limits<float>::max();
                for(Zone* z : zones)
                {
                    float score = int(z->team == my_team)*10000 + z->distance(drones[i]);
                    if(score < best_score)
                    {
                        best_zone = z;
                        best_score = score;
                    }
                }
                cerr << "[Warning] Drone " << i << " have nothing to do." << endl;
                cout << best_zone->x << " " << best_zone->y << endl;
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
const float Zone::OCCUPATION_SCORE_TAU = 0.99;
const int Drone::SPEED = 100;
const chrono::milliseconds Game::MAX_TIME = chrono::milliseconds(90);
const float Game::TAU = 0.02;
