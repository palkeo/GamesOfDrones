#include <iostream>
#include <cmath>
#include <vector>
#include <limits>
#include <queue>
#include <set>
#include <algorithm>
#include <chrono>

using namespace std;

class Drone;
class Game;
class Zone;

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

class Zone : public Point
{
    public:

    static const int RADIUS;
    static const float OCCUPATION_SCORE_TAU;

    vector<Drone*> drones;
    vector<Drone*> drones_going;
    int team;
    float occupation_score;

    protected:
    int id;

    public:

    Zone(int id_, int x_, int y_) : occupation_score(1), id(id_)
    {
        x = x_;
        y = y_;
    }

    void update(int team_)
    {
        team = team_;

        occupation_score = OCCUPATION_SCORE_TAU*occupation_score + (1.f - OCCUPATION_SCORE_TAU)*drones.size();
    }
};

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

class Game
{
    public:

    static const chrono::milliseconds MAX_TIME;

    vector<Zone*> zones;
    vector< vector<Drone*>* > teams;
    vector<Drone*> drones;
    int my_team;
    int nb_teams;
    int nb_drones;
    int turn;

    int nb_recurse;
    int recurse_width;
    chrono::time_point<chrono::steady_clock> recurse_time_start;

    Game() : turn(0), recurse_width(5)
    {
        int nb_players, nbd, nb_zones, team;
        cin >> nb_players >> team >> nbd >> nb_zones;

        my_team = team;
        nb_drones = nbd;
        nb_teams = nb_players;

        for(int z = 0; z < nb_zones; ++z)
        {
            int x, y;
            cin >> x >> y;
            zones.push_back(new Zone(z, x, y));
        }

        for(int p = 0; p < nb_players; ++p)
        {
            vector<Drone*>* dt = new vector<Drone*>();
            for(int d = 0; d < nb_drones; ++d)
            {
                Drone* pt = new Drone(d, p);
                dt->push_back(pt);
                drones.push_back(pt);
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
            int team;
            cin >> team;
            zone->update(team);
            zone->drones.clear();
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
                    if(zone->distance(drone) <= Zone::RADIUS)
                    {
                        drone->inside = zone;
                        zone->drones.push_back(drone);
                    }
                    if(drone->distance_direction_point(zone) <= md)
                    {
                        md = drone->distance_direction_point(zone);
                        mz = zone;
                    }
                }
                drone->going_to = mz;
                mz->drones_going.push_back(drone);
            }
        }
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
        if(available_zones.size() == 0 || available_drones.size() == 0 || chrono::steady_clock::now() - recurse_time_start > Game::MAX_TIME)
            return Action();
        nb_recurse++;

        priority_queue<ZoneAction> actions;

        for(Zone* zone : available_zones)
        {
            // calculate the actions
            auto distance_cmp = [zone] (Drone* a, Drone* b) { return a-> distance(zone) < b->distance(zone); };

            vector<Drone*> my_drones(available_drones.begin(), available_drones.end());
            sort(my_drones.begin(), my_drones.end(), distance_cmp);

            vector<Drone*> foe_drones;
            for(auto i = zone->drones_going.begin(); i != zone->drones_going.end(); i++)
            {
                if((*i)->team != my_team)
                    foe_drones.push_back(*i);
            }
            sort(foe_drones.begin(), foe_drones.end(), distance_cmp);

            float last_score = 0;
            for(auto my_drones_iter_end = my_drones.begin(); my_drones_iter_end <= my_drones.end(); my_drones_iter_end++)
            {
                float score = 0;
                bool is_mine = (my_team == zone->team);

                auto foe_drones_iter = foe_drones.begin();
                auto my_drones_iter = my_drones.begin();

                int my_count = 0;
                int foe_count = 0;
                vector<int> foe_count_table;
                for(int i = 0; i < nb_teams; ++i)
                    foe_count_table.push_back(0);

                int t = 1;
                int tmax = 60;

                while(t < tmax)
                {
                    int dist = t * Drone::SPEED + Zone::RADIUS;
                    float min_dist = 1000000000;

                    while(my_drones_iter != my_drones_iter_end && (min_dist = (*my_drones_iter)->distance(zone)) <= dist)
                    {
                        my_count++;
                        my_drones_iter++;
                    }

                    float min_dist_foe = 1000000000;
                    while(foe_drones_iter != foe_drones.end() && (min_dist_foe = (*foe_drones_iter)->distance(zone)) <= dist)
                    {
                        if(++foe_count_table[foe_drones[0]->team] > foe_count)
                            foe_count++;
                        foe_drones_iter++;
                    }
                    if(min_dist_foe < min_dist)
                        min_dist = min_dist_foe;

                    int increment = ceil((min_dist - Zone::RADIUS) / float(Drone::SPEED));
                    if(t + increment > tmax)
                        increment = tmax - t;
                    t += increment;

                    is_mine = my_count > foe_count || (my_count == foe_count && is_mine);
                    score += int(is_mine)*increment;
                }

                if(score > last_score)
                {
                    last_score = score;
                    ZoneAction za(score, score / float(my_count ? my_count : 0.1), zone);
                    for(auto j = my_drones.begin(); j != my_drones_iter_end; j++)
                        za.drones.push_back(*j);
                    actions.push(za);
                }
                // For speed !
                if(is_mine)
                    break;
            }

        }

        Action best_action;
        int i = 0;
        while((! actions.empty()) && i < recurse_width)
        {
            ZoneAction action = actions.top();
            actions.pop();

            set<Zone*> sub_available_zones = available_zones;
            sub_available_zones.erase(action.zone);

            set<Drone*> sub_available_drones = available_drones;
            for(Drone* j : action.drones)
                sub_available_drones.erase(j);

            Action subaction = recurse(sub_available_zones, sub_available_drones);
            subaction.score += action.absolute_score;

            if(subaction.score > best_action.score)
            {
                best_action = subaction;
                for(Drone* j: action.drones)
                    best_action.moves.push_back(pair<Drone*, Zone*>(j, action.zone));
            }
            
            i++;
        }

        return best_action;
    }

    void play()
    {
        nb_recurse = 0;
        recurse_time_start = chrono::steady_clock::now();
        Action result = recurse(set<Zone*>(zones.begin(), zones.end()), set<Drone*>(teams[my_team]->begin(), teams[my_team]->end()));

        if(chrono::steady_clock::now() - recurse_time_start > Game::MAX_TIME)
        {
            if(recurse_width > 5)
                recurse_width = 5;
            else if(recurse_width > 2)
                recurse_width--;
            cerr << "[Warning] Too long. recurse_width decreased." << endl;
        }
        else
            recurse_width++;

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
        cerr << "[Info] nb_recurse = " << nb_recurse << ", recurse_width = " << recurse_width << endl;
    }
};


int main()
{
    Game g;
    while(true)
    {
        g.update();
        g.play();
    }
    return 0;
}

const int Zone::RADIUS = 100;
const float Zone::OCCUPATION_SCORE_TAU = 0.99;
const int Drone::SPEED = 100;
const chrono::milliseconds Game::MAX_TIME = chrono::milliseconds(90);
