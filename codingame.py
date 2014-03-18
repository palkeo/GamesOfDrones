import math
import sys
import operator
import datetime

WAR_WEIGHT_TAU = 0.99
ZONE_SIZE = 100
DRONE_SPEED = 100

TW = [1]*60

class Point(object):
    def __init__(self, x=0, y=0):
        self.x = x
        self.y = y
        
    def __str__(self):
        return (str(self.x) + ' ' + str(self.y))

    def distance(self, other):
        return math.sqrt((self.x - other.x)**2 + (self.y - other.y)**2)

class Zone(Point):
    def __init__(self, game, team, *args, **kwargs):
        self.team = team
        self.game = game
        self._war_weight = 1
        self.turn = 0
        super().__init__(*args, **kwargs)

    def __repr__(self):
        return 'Zone(%s, %s)' % (self.x, self.y)

    def drones_inside(self):
        yield from filter(lambda drone: self.distance(drone) <= ZONE_SIZE, self.game.drones)

    def drones_needed(self, drones):
        """ Return a list of (score, drone_list), for each possible solution """
        turns_remaining = 201 - self.turn
        drones = sorted(drones, key=lambda drone: drone.distance(self))
        s = []

        for nb_drones in range(1, len(drones) + 1):
            my_drones = drones[:nb_drones]
            my_drones_count = 0
            foe_drones_count = [0 for _ in self.game.teams]
            foe_drones = sorted(filter(lambda d: d.team != self.game.my_team and d.zone_objective() == self, self.game.drones), key=lambda d: d.distance(self))
            score = 0
            is_mine = self.team == self.game.my_team

            for turns, weight in enumerate(TW[:turns_remaining]):
                distance = (turns + 1) * DRONE_SPEED + ZONE_SIZE
                while my_drones and my_drones[0].distance(self) <= distance:
                    my_drones_count += 1
                    my_drones.pop(0)
                while foe_drones and foe_drones[0].distance(self) <= distance:
                    foe_drones_count[foe_drones.pop(0).team] += 1

                fc = max(foe_drones_count)
                is_mine = my_drones_count > fc or (my_drones_count == fc and is_mine)
                score += weight * int(is_mine)

            # TODO: Add an infinite bidule

            if score > 0:
                s.append((score, drones[:nb_drones]))

        return s
      
    def update(self):
        self.turn += 1
        by_team = [0 for _ in self.game.teams]
        for drone in self.game.drones:
            if drone.zone_inside() == self:
                by_team[drone.team] += 1

        self._war_weight = WAR_WEIGHT_TAU*self._war_weight + (1 - WAR_WEIGHT_TAU)*max(by_team)**2

    @property
    def war_weight(self):
        return math.sqrt(self._war_weight)

class Drone(Zone):
    def __init__(self, drone_id, *args, **kwargs):
        self.drone_id = drone_id
        self.old_x, self.old_y = None, None
        self._zone_objective = None
        super().__init__(*args, **kwargs)

    def __repr__(self):
        return 'Drone(%s, %s)' % (self.x, self.y)
    
    def update(self, x, y):
        self.old_x, self.old_y = self.x or x, self.y or y
        self.x, self.y = x, y
        self._zone_objective = None
    
    def direction(self):
        return Point(self.x - self.old_x, self.y - self.old_y)

    def zone_inside(self):
        for z in self.game.zones:
            if self.distance(z) <= ZONE_SIZE:
                return z
        return None
    
    def zone_objective(self):
        if self._zone_objective is not None:
            return self._zone_objective
        
        direction = self.direction()
        if direction.x == 0 and direction.y == 0:
            return self.zone_inside()

        min_d = None
        min_z = None
        for z in self.game.zones:
            p = self.projection(z)
            if p.distance(z) <= ZONE_SIZE:
                d = self.distance(p)
                if min_d is None or d < min_d:
                    min_d = d
                    min_z = z

        self._zone_objective = min_z
        return min_z
        
    def projection(self, zone):
        u = ((zone.x - self.old_x)*(self.x - self.old_x) + (zone.y - self.old_y)*(self.y - self.old_y)) / ((self.x-self.old_x)**2 + (self.y-self.old_y)**2)
        u = max(0., u)
        return Point(self.old_x + u*(self.x - self.old_x), self.old_y + u*(self.y - self.old_y))

class Game(object):
    # zones = [] # list of zones
    # teams = [] # list of drones by team
    # my_team = -1 # my team id
    # drones = []
        
    def __init__(self):
        # p = number of players in the game 2 to 4
        # i = id of your player(0,1,2 or 3)
        # d = number of drones in each team(3 to 11)
        # z = number of zones in map(4 to 8)
    
        p,i,d,z = map(int, input().split()) # enter the input for initialization
        
        self.my_team = i # set my team id
        self.turn = 0
        
        # Enter details for each zone
        self.zones = []
        for areaId in range(z):
            # print 'Enter center of zone'
            s = input() # take as input the center of the zone
            x, y = [int(var) for var in s.split()]
            self.zones.append(Zone(self, None, x, y)) # for each zone create a zone object
        self.zones = tuple(self.zones)

        # assign teams
        self.teams = [[] for teamId in range(p)] # for each team create a team object
        for teamId in range(p):
            d1 = [Drone(droneId, self, teamId, None, None) for droneId in range(d)] # for each drone create a drone object
            self.teams[teamId].extend(d1)
        self.teams = tuple(self.teams)

    def run(self):
        """Function running during each round, takes as input
        zone ownerid and the coordinates of drones of all teams"""
    
        while 1:
      
            for zone in self.zones:
                #print 'Enter zone id'
                zone.team = int(input())

            for team in self.teams:
                for drone in team:
                    # print 'Enter drone points'
                    drone.update(*map(int, input().split()))
            
            for zone in self.zones:
                zone.update()

            self.play()

    @property
    def drones(self):
        l = []
        for team in self.teams:
            l.extend(team)
        return l

    def play(self):
        self.turn += 1
        self.turn_start = datetime.datetime.now()

        default_zone = None
        for z in self.zones:
            if default_zone is None or z.war_weight > default_zone.war_weight:
                default_zone = z

        best_action, best_score = self.recurse(self.zones, self.teams[self.my_team])

        for drone in self.teams[self.my_team]:
            try:
                zone = best_action[drone.drone_id]
            except KeyError:
                print("[Warning] Drone %s without any command." % drone, file=sys.stderr)
                zone = default_zone
            print("%s %s" % (zone.x, zone.y))
    
    def recurse(self, zones, drones):
        if not zones or not drones or (datetime.datetime.now() - self.turn_start).total_seconds() > 0.09:
            return {}, 0

        drones = set(drones)
        zones = set(zones)
        h = []

        for zone in zones:
            for score, needed in zone.drones_needed(drones):
                rscore = score / len(needed)
                h.append((rscore, score, needed, zone))

        #print(len(zones), len(drones), sorted(h, key=operator.itemgetter(0), reverse=True), file=sys.stderr)

        best_score = 0
        best_action = {}
        for _, score, needed, zone in sorted(h, key=operator.itemgetter(0), reverse=True):
            action, subscore = self.recurse(zones.difference((zone,)), drones.difference(needed))
            subscore += score
            if subscore > best_score:
                best_score = subscore
                best_action = action
                best_action.update((i.drone_id, zone) for i in needed)

        return best_action, best_score
                
g = Game()
g.run()
