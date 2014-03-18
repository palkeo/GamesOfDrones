#include <iostream>
#include <cmath>
#include <vector>
#include <limits>
#include <queue>
#include <set>
#include <algorithm>

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

    std::vector<Drone*> drones;
    std::vector<Drone*> drones_going;
    char team;

    protected:
    float occupation_score;
    char id;

    public:

    Zone(char id_) : occupation_score(1)
    {
        id = id_;
    }

    void update(char team_)
    {
        team = team_;

        occupation_score = OCCUPATION_SCORE_TAU*occupation_score + (1.f - OCCUPATION_SCORE_TAU)*drones.size();
    }
};

class Drone : public Point
{
    public:

    static const int SPEED;

    char id;
    char team;
    int old_x, old_y;
    Zone* going_to;
    Zone* inside;

    Drone(char id_, char team_) : id(id_), team(team_), old_x(-1), going_to(NULL), inside(NULL)
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

    vector<Zone*> zones;
    vector< vector<Drone*>* > teams;
    vector<Drone*> drones;
    char my_team;
    char nb_teams;
    char nb_drones;
    int turn;

    Game() : turn(0)
    {
        int nb_players, nbd, nb_zones, team;
        cin >> nb_players >> team >> nbd >> nb_zones;

        my_team = team;
        nb_drones = nbd;
        nb_teams = nb_players;

        for(char z = 0; z < nb_zones; ++z)
            zones.push_back(new Zone(z));

        for(char p = 0; p < nb_players; ++p)
        {
            vector<Drone*>* dt = new vector<Drone*>();
            for(char d = 0; d < nb_drones; ++d)
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
    
    struct Action
    {
        float absolute_score;
        float relative_score;
        std::vector<std::pair<Drone*, Zone*> > moves;

        Action(float as, float rs) : absolute_score(as), relative_score(rs)
        {
        }

        Action() : absolute_score(0), relative_score(0)
        {
        }

        inline bool operator<(const Action& other) const
        {
            return relative_score < other.relative_score;
        }
    };
    
    Action recurse(std::set<Zone*> available_zones, std::set<Drone*> available_drones)
    {
        if(available_zones.size() == 0 || available_drones.size() == 0)
            return Action();

        priority_queue<Action> actions;

        for(Zone* zone : available_zones)
        {
            // calculate the actions
            auto distance_cmp = [&] (Drone* a, Drone* b) { return a-> distance(zone) < b->distance(zone); };

            vector<Drone*> my_drones(available_drones.begin(), available_drones.end());
            sort(my_drones.begin(), my_drones.end(), distance_cmp);

            vector<Drone*> foe_drones;
            for(auto t = drones.begin(); t != drones.end(); t++)
            {
                if((*t)->team != my_team)
                    foe_drones.push_back(*t);
            }
            sort(foe_drones.begin(), foe_drones.end(), distance_cmp);

            float score = 0;
            bool is_mine = (my_team == zone->team);

            char my_count = 0;
            char foe_count = 0;
            vector<char> foe_count_table;
            for(char i = 0; i < nb_teams; ++i)
                foe_count_table.push_back(0);

            for(int i = 1; i < 200 - turn; ++i)
            {
                int distance = i * Drone::SPEED + Zone::RADIUS;
                while((! my_drones.empty()) && my_drones[0]->distance(zone) <= distance)
                {
                    my_count++;
                    my_drones.erase(my_drones.begin());
                }
                while((! foe_drones.empty()) && foe_drones[0]->distance(zone) <= distance)
                {
                    if(++foe_count_table[foe_drones[0]->team] > foe_count)
                        foe_count++;
                    foe_drones.erase(foe_drones.begin());
                }

                is_mine = my_count > foe_count || (my_count == foe_count && is_mine);
                score += int(is_mine)
            }

        }

        Action best_action;
        int i = 0;
        while((! actions.empty()) && true)
        {
            Action action = actions.top();
            actions.pop();

            std::set<Zone*> sub_available_zones = available_zones;
            sub_available_zones.erase(action.moves[0].second);

            std::set<Drone*> sub_available_drones = available_drones;
            for(std::pair<Drone*, Zone*> j : action.moves)
                sub_available_drones.erase(j.first);

            Action subaction = recurse(sub_available_zones, sub_available_drones);
            subaction.absolute_score += action.absolute_score;

            if(subaction.absolute_score > best_action.absolute_score)
            {
                best_action = subaction;
                best_action.moves.insert(best_action.moves.end(), action.moves.begin(), action.moves.end());
            }
            
            i++;
        }

        return best_action;
    }

    void play()
    {
        Action result = recurse(std::set<Zone*>(zones.begin(), zones.end()), std::set<Drone*>(teams[my_team]->begin(), teams[my_team]->end()));
        for(char i = 0; i < nb_drones; ++i)
        {
            bool found = false;
            for(std::pair<Drone*, Zone*> p : result.moves)
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
                cerr << "[Warning] Drone " << i << " have nothing to do." << endl;
                cout << "2000 900" << endl;
            }
        }
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

