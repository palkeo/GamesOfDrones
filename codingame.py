import math
import sys

WAR_WEIGHT_TAU = 0.99

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
        self.war_weight = 1
        super().__init__(*args, **kwargs)

    def drones_inside(self):
        yield from filter(lambda drone: self.distance(drone) <= 100, self.game.drones)

    def drones_needed(self):
        by_team = [0 for _ in self.game.teams]
        for drone in self.game.drones:
            if drone.team == self.game.my_team:
                continue

            if drone.zone_objective() == self:
                by_team[drone.team] += 1

        return max(by_team) + int(self.team != self.game.my_team)
      
    def update(self):
        self.turns += 1

        by_team = [0 for _ in self.game.teams]
        for drone in self.game.drones:
            if drone.zone_inside() == self:
                by_team[drone.team] += 1

        self.war_weight = WAR_WEIGHT_TAU*self.war_weight + (1 - WAR_WEIGHT_TAU)*max(by_team)

class Drone(Zone):
    def __init__(self, drone_id, *args, **kwargs):
        self.drone_id = drone_id
        self.old_x, self.old_y = None, None
        super().__init__(*args, **kwargs)
    
    def update(self, x, y):
        self.old_x, self.old_y = self.x or x, self.y or y
        self.x, self.y = x, y
    
    def direction(self):
        return Point(self.x - self.old_x, self.y - self.old_y)

    def zone_inside(self):
        for z in self.game.zones:
            if self.distance(z) <= 100:
                return z
        return None
    
    def zone_objective(self):
        direction = self.direction()
        if direction.x == 0 and direction.y == 0:
            return self.zone_inside()

        min_d = None
        min_z = None
        for z in self.game.zones:
            p = self.projection(z)
            if p.distance(z) <= 100:
                d = self.distance(p)
                if min_d is None or d < min_d:
                    min_d = d
                    min_z = z
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

        default_zone = None
        for z in self.zones:
            if default_zone is None or z.war_weight > default_zone.war_weight:
                default_zone = z

        vascript
            
            03/17 02:31:06
                
                    
                    44,50
                    11
                        
                        S010|
        commands = [default_zone for _ in self.teams[self.my_team]]

        zones = set(self.zones)
        drones = set(self.teams[self.my_team])
        while drones:
            best_score = None
            best_action = None
            
            for zone in zones:
                needed = zone.drones_needed()
                if len(drones) < needed:
                    continue
                
                nearest_drones = sorted(drones, key=lambda drone: drone.distance(zone))[:needed]
                
                weight = (max(drone.distance(zone) for drone in nearest_drones) if nearest_drones else 0)
                weight *= zone.war_weight
                
                score = (needed, weight)
                
                if best_score is None or score < best_score:
                    best_score = score
                    best_action = (nearest_drones, zone)
            
            if best_action is None:
                print("WTF", file=sys.stderr)
                break
            
            d, zone = best_action
            zones.remove(zone)
            for i in d:
                commands[i.drone_id] = zone
            drones.difference_update(d)

        for command in commands:
            print("%s %s" % (command.x, command.y))


g = Game()
g.run()
