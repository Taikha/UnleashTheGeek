#include <array>
#include <cassert>
#include <iostream>
#include <vector>
#include <map>
#include <algorithm>

using namespace std;

//*********************************  UTILS  **********************************************

//----------------------------------Point----------------------------------------------------------
struct Point {
    int x{-1}, y{-1};

    int distance(const Point& oth) { return abs(x - oth.x) + abs(y - oth.y); }
    ostream& dump(ostream& ioOut) const {
        ioOut << x << " " << y;
        return ioOut;
    }
};
ostream& operator<<(ostream& ioOut, const Point& obj) { return obj.dump(ioOut); }

//*********************************  GAME STATE  **********************************************

//----------------------------------Constants----------------------------------------------------------
enum class Type : int { NONE = 0, ROBOT, RADAR, TRAP, ORE, HOLE };
enum class ActionType : int { WAIT = 0, MOVE, DIG, REQUEST };

static constexpr int MAX_PLAYERS = 2;
static constexpr int MAX_WIDTH = 30;
static constexpr int MAX_HEIGHT = 15;
static constexpr int MAX_ROBOTS = 5;

//----------------------------------Cell----------------------------------------------------------
struct Cell : Point {
    bool hole{false};
    bool oreVisible{false};
    int ore{-1};
    int score{0};
    
    void update(Point p, int _ore, bool _oreVisible, int _hole) {
        hole = _hole;
        if(_oreVisible){
            ore = _ore;
        }
        oreVisible = _oreVisible;
        x = p.x;
        y = p.y;
    }
};

//----------------------------------Entity----------------------------------------------------------
struct Entity : Point {
    int id{0};
    Type type{Type::NONE};
    Type item{Type::NONE};
    int owner{0};

    Entity() = default;
    Entity(int _id, Type _type, Point p, Type _item, int _owner) : Point{p}, id{_id}, type{_type}, item{_item}, owner{_owner} {}
    void update(int _id, Type _type, Point p, Type _item, int _owner) {
        x = p.x;
        y = p.y;
        id = _id;
        type = _type;
        owner = _owner;
        item = _item;
    }
};

//----------------------------------Robot----------------------------------------------------------
struct Robot : Entity {
    bool isDead() const { return x == -1 && y == -1; }
};

//----------------------------------Player----------------------------------------------------------
struct Player {
    array<Robot, MAX_ROBOTS> robots;
    
    int ore{0};
    int cooldownRadar{0}, cooldownTrap{0};
    int owner{0};

    void updateRobot(int id, Point p, Type item, int owner) {
        int idxOffset{0};
        if (id >= MAX_ROBOTS) { idxOffset = MAX_ROBOTS; }
        robots.at(id - idxOffset).update(id, Type::ROBOT, p, item, owner);
        owner = owner;
    }
    void updateOre(int _owner, int _ore) {
        ore = _ore;
        owner = _owner;
    }
    void updateCooldown(int radar, int trap) {
        cooldownRadar = radar;
        cooldownTrap = trap;
    }
};

//----------------------------------Game----------------------------------------------------------
// global variable
vector<Point> dangerPointList; // danger list dont ever go there
vector<Point> holePointList; // danger list dont ever go there

vector<bool> suspectID(MAX_ROBOTS, false); // suspicious for enemy robot spend 1 turn at home

struct Game {
    array<array<Cell, MAX_HEIGHT>, MAX_WIDTH> grid;
    array<Player, MAX_PLAYERS> players;
    vector<Entity> radars;
    vector<Entity> traps;
    vector<Point> digPointList;
    vector<Point> oreReduceList;
    vector<Point> newHoleList;
    vector<vector<Point>> bombList;
    int turn = -1;
    int fakeCD = 0;

    Game() { reset(); }
    Cell& get(int x, int y) { return grid.at(x).at(y); }
    Cell& get(Point p) { return grid.at(p.x).at(p.y); }
    void reset() {
        if(fakeCD > 0) fakeCD--;
        turn++;
        radars.reserve(20);
        radars.clear();
        traps.reserve(30);
        traps.clear();
        digPointList.reserve(20);
        digPointList.clear();
        // track ore reduced
        oreReduceList.reserve(10);
        oreReduceList.clear();
        // track new hole
        newHoleList.reserve(10);
        newHoleList.clear();
    }
    void updateOre(int owner, int ore) { players.at(owner).updateOre(owner, ore); }
    void updateCooldown(int owner, int radar, int trap) { players.at(owner).updateCooldown(radar, trap); }
    void updateCell(int x, int y, const string& ore, int hole) {
        int oreAmount{-1};
        bool oreVisible{false};
        Point p{x, y};
        // within radar
        if (ore != "?") {
            oreAmount = stoi(ore);
            oreVisible = true;
            // update available Tile
            if(oreAmount > 0){
                digPointList.push_back(p);   
            }
            // someone digging this tile
            if(get(p).ore != -1 && oreAmount < get(p).ore ){
                // record this hole first for further analysis
                oreReduceList.push_back(p);
            }
        }
        
        // new hole
        if(hole == 1 && get(p).hole == false){
            // change score for radar placement
            for(int _x = x-4; _x <= x+4; _x++){
                for(int _y = y-4; _y <= y+4; _y++){
                    if(_x < 1 || _x >= MAX_WIDTH) continue;
                    if(_y < 0 || _y >= MAX_HEIGHT) continue;
                    // for distance smaller than 5 valid point
                    if(Point({x,y}).distance(Point({_x,_y})) < 5){
                        // no hole score
                        get(_x,_y).score -= 4 - Point({x,y}).distance(Point({_x,_y}));
                    }
                }
            }
            // if suspicious id around
            // record this hole first for further analysis
            newHoleList.push_back(p);
        }
        
        get(p).update(p, oreAmount, oreVisible, hole);
    }
    void updateEntity(int id, int type, int x, int y, int _item) {
        // item
        Type item{Type::NONE};
        switch (_item) {  //-1 for NONE, 2 for RADAR, 3 for TRAP, 4 ORE
        case -1: item = Type::NONE; break;
        case 2:  item = Type::RADAR; break;
        case 3:  item = Type::TRAP; break;
        case 4:  item = Type::ORE; break;
        default: assert(false);
        }
        Point p{x, y};
        switch (type) {  // 0 for your robot, 1 for other robot, 2 for radar, 3 for trap
        case 0:
        case 1: players.at(type).updateRobot(id, p, item, type); break;
        case 2: radars.emplace_back(id, Type::RADAR, p, item, 0); break;
        case 3: traps.emplace_back(id, Type::TRAP, p, item, 0); break;
        default: assert(false);
        }
    }
};

//*********************************  GAME SIMULATION  **************************************************

//----------------------------------Action----------------------------------------------------------
struct Action {
    static const array<string, 4> LABELS_ACTIONS;

    Point dest;
    ActionType type{ActionType::WAIT};
    Type item{Type::NONE};
    string message;

    void wait(string _message = "") {
        dest = Point{0, 0};
        type = ActionType::WAIT;
        item = Type::NONE;
        message = _message;
    }
    void move(Point _dest, string _message = "") {
        dest = _dest;
        type = ActionType::MOVE;
        item = Type::NONE;
        message = _message;
    }
    void dig(Point _dest, string _message = "") {
        dest = _dest;
        type = ActionType::DIG;
        item = Type::NONE;
        message = _message;
    }
    void request(Type _item, string _message = "") {
        dest = Point{0, 0};
        type = ActionType::REQUEST;
        item = _item;
        message = _message;
    }
    ostream& dump(ostream& ioOut) const {
        ioOut << LABELS_ACTIONS.at((int)(type));
        if (type == ActionType::MOVE || type == ActionType::DIG) { ioOut << " " << dest; }
        if (type == ActionType::REQUEST && item == Type::RADAR) { ioOut << " RADAR"; }
        if (type == ActionType::REQUEST && item == Type::TRAP) { ioOut << " TRAP"; }
        if (message != "") { ioOut << " " << message; }
        return ioOut;
    }
};
const array<string, 4> Action::LABELS_ACTIONS{"WAIT", "MOVE", "DIG", "REQUEST"};

ostream& operator<<(ostream& ioOut, const Action& obj) { return obj.dump(ioOut); }

array<Point, MAX_ROBOTS> prevEnemyPos;
array<Point, MAX_ROBOTS> prevOurPos;
array<Action, MAX_ROBOTS> prevActions;

int roll(int min, int max)
{
   // x is in [0,1[
   double x = rand()/static_cast<double>(RAND_MAX+1); 

   // [0,1[ * (max - min) + min is in [min,max[
   int that = min + static_cast<int>( x * (max - min) );

   return abs(that);
}

bool CheckRadar(vector<Entity>& radars, Point _pt){
    bool nearbyRadar = false;
    for(int i = 0; i < radars.size(); i++){
        if(_pt.distance(Point{radars.at(i).x, radars.at(i).y}) < 5){
            nearbyRadar = true;
            break;
        }
    }
    return nearbyRadar;
}

int AdditionalRadarScore(Game& game, Point _pt){
    int score = 0;
    for(int _x = _pt.x-4; _x <= _pt.x+4; _x++){
        for(int _y = _pt.y-4; _y <= _pt.y+4; _y++){
            if(_x < 1 || _x >= MAX_WIDTH) continue;
            if(_y < 0 || _y >= MAX_HEIGHT) continue;
            // for distance smaller than 5 valid point
            if(Point({_x,_y}).distance(_pt) < 5){
                // undiscover score
                if(game.get(_x,_y).ore == -1)
                    score++;
                // no visibility score
                if(game.get(_x,_y).oreVisible == false)
                    score++;
            }
        }
    }
    return score;
}

bool CheckTrap(vector<Entity>& traps, Point _pt){
    bool trapFound = false;
    for(int j = 0; j < traps.size(); j++){
        if(_pt.distance(Point{traps.at(j).x, traps.at(j).y}) <= 0){
            trapFound = true;
            break;
        }
    }
    return trapFound;
}

void LinkedTraps(vector<Point>& v, vector<Entity>& traps, Point _pt){
    for(int j = 0; j < traps.size(); j++){
        if(traps.at(j).distance(Point{_pt.x, _pt.y}) == 1){
            int index = -1;
            for(int i = 0; i < v.size(); i++){
                if(v.at(i).distance(traps.at(j))==0){
                    index = i;
                    break;
                }
            }
            if(index == -1){
                v.push_back(traps.at(j));
                LinkedTraps(v, traps, traps.at(j));
            }
        }
    }
}

void LinkedDangerTraps(vector<Point>& v, Point _pt){
    for(int j = 0; j < dangerPointList.size(); j++){
        if(dangerPointList.at(j).distance(Point{_pt.x, _pt.y}) == 1){
            int index = -1;
            for(int i = 0; i < v.size(); i++){
                if(v.at(i).distance(dangerPointList.at(j))==0){
                    index = i;
                    break;
                }
            }
            if(index == -1){
                v.push_back(dangerPointList.at(j));
                LinkedDangerTraps(v, dangerPointList.at(j));
            }
        }
    }
}

bool IsSafety(vector<Entity>& traps, Player& me, Player& enemy, array<Action, MAX_ROBOTS>& actions, Point _pt){
    // if one of enemy robot in explosion range, only one robot can go in that range
    bool isSafe = true;
    bool alreadyOne = false;
    bool enemyNear = false;
    vector<Point> linkTraps;
    vector<Point> linkDangerTraps;
    
    for(int j = 0; j < traps.size(); j++){
        if(traps.at(j).distance(Point{_pt.x, _pt.y}) <= 1){
            // find all trap linked to this trap
            linkTraps.push_back(traps.at(j));
            LinkedTraps(linkTraps, traps, traps.at(j));
            
            for(int i = 0; i < MAX_ROBOTS; i++){
                for(int j = 0; j < linkTraps.size(); j++){
                    if(me.robots.at(i).distance(linkTraps.at(j)) <= 1 || actions.at(i).dest.distance(linkTraps.at(j)) <= 1){
                        alreadyOne = true;
                        break;
                    }
                }
            }
            
            for(int i = 0; i < MAX_ROBOTS; i++){
                for(int j = 0; j < linkTraps.size(); j++){
                    if(enemy.robots.at(i).distance(linkTraps.at(j)) <= 5 && enemy.robots.at(i).distance(linkTraps.at(j)) < prevEnemyPos.at(i).distance(linkTraps.at(j))){
                        enemyNear = true;
                        break;
                    }
                    if(enemy.robots.at(i).distance(linkTraps.at(j)) <= 1){
                        enemyNear = true;
                        break;
                    }
                }
            }
            if(alreadyOne && enemyNear){
                isSafe = false;
                return isSafe;
            }
                
        }
    }
    
    isSafe = true;
    alreadyOne = false;
    enemyNear = false;
    
    for(int j = 0; j < dangerPointList.size(); j++){
        if(dangerPointList.at(j).distance(Point{_pt.x, _pt.y}) <= 1){
            // find all trap linked to this trap
            linkDangerTraps.push_back(dangerPointList.at(j));
            LinkedDangerTraps(linkDangerTraps, dangerPointList.at(j));
            
            for(int i = 0; i < MAX_ROBOTS; i++){
                for(int j = 0; j < linkDangerTraps.size(); j++){
                    if(me.robots.at(i).distance(linkDangerTraps.at(j)) <= 1 || actions.at(i).dest.distance(linkDangerTraps.at(j)) <= 1){
                        alreadyOne = true;
                        break;
                    }
                }
            }
            
            for(int i = 0; i < MAX_ROBOTS; i++){
                for(int j = 0; j < linkDangerTraps.size(); j++){
                    if(enemy.robots.at(i).distance(linkDangerTraps.at(j)) <= 5 && enemy.robots.at(i).distance(linkDangerTraps.at(j)) < prevEnemyPos.at(i).distance(linkDangerTraps.at(j))){
                        enemyNear = true;
                        break;
                    }
                    if(enemy.robots.at(i).distance(linkDangerTraps.at(j)) <= 1){
                        enemyNear = true;
                        break;
                    }
                }
            }
            if(alreadyOne && enemyNear){
                isSafe = false;
                return isSafe;
            }
                
        }
    }
    
    for(int j = 0; j < MAX_ROBOTS; j++){
        if(suspectID.at(j) && _pt.distance(enemy.robots.at(suspectID.at(j))) <= 1){
            isSafe = false;
            return isSafe;
        }
    }
    
    return isSafe;
}

bool IsNewHole(Game& game, Point _pt){
    bool isNew = false;
    for(int j = 0; j < game.newHoleList.size(); j++){
        if(game.newHoleList.at(j).distance(Point{_pt.x, _pt.y}) <= 0){
            isNew = true;
            break;
        }
    }
    return isNew;
}

bool IsReduce(Game& game, Point _pt){
    bool isReduce = false;
    for(int j = 0; j < game.oreReduceList.size(); j++){
        if(game.oreReduceList.at(j).distance(Point{_pt.x, _pt.y}) <= 0){
            isReduce = true;
            break;
        }
    }
    return isReduce;
}

bool IsDanger(Point _pt){
    bool danger = false;
    for(int j = 0; j < dangerPointList.size(); j++){
        if(_pt.distance(Point{dangerPointList.at(j).x, dangerPointList.at(j).y}) <= 0){
            danger = true;
            break;
        }
    }

    return danger;
}

void InsertDangerPt(Point _pt){
    // this tile is cause by enemy 100%
    bool exist = false;
    for(int k = 0; k < dangerPointList.size(); k++){
        if(dangerPointList.at(k).distance(_pt) == 0)
        {
            exist = true;
            break;
        }
    }
    if(!exist) 
        dangerPointList.push_back(_pt); // mean something is here
}

void RemoveDangerPt(Point _pt){
    // this tile is cause by enemy 100%
    int index = -1;
    for(int k = 0; k < dangerPointList.size(); k++){
        if(dangerPointList.at(k).distance(_pt) == 0)
        {
            index = k;
            break;
        }
    }
    if(index != -1) 
        dangerPointList.erase(dangerPointList.begin() + index); // release point
}

void InsertHolePt(Point _pt){
    // this tile is cause by enemy 100%
    bool exist = false;
    for(int k = 0; k < holePointList.size(); k++){
        if(holePointList.at(k).distance(_pt) == 0)
        {
            exist = true;
            break;
        }
    }
    if(!exist) 
        holePointList.push_back(_pt); // mean something is here
}

void RemoveHolePt(Point _pt){
    // this tile is cause by enemy 100%
    int index = -1;
    for(int k = 0; k < holePointList.size(); k++){
        if(holePointList.at(k).distance(_pt) == 0)
        {
            index = k;
            break;
        }
    }
    if(index != -1) 
        holePointList.erase(holePointList.begin() + index); // release point
}


bool CheckExplosion(Game& game, Player& me, Player& enemy, Point _pt,int targetVictim=-1){
    bool kamikaze = false;
    
    vector<bool> myRobots(MAX_ROBOTS, false);
    vector<bool> enemyRobots(MAX_ROBOTS, false);
    // if our robot beside bomb 
    
    vector<Point> linkTraps;
    
    for(int j = 0; j < game.traps.size(); j++){
        if(game.traps.at(j).distance(Point{_pt.x, _pt.y}) == 0){
            // find all trap linked to this trap
            linkTraps.push_back(game.traps.at(j));
            LinkedTraps(linkTraps, game.traps, game.traps.at(j));
            cerr << "linked:" << linkTraps.size() << "\n";
        }
    }
    
    for(int i = 0; i < MAX_ROBOTS; i++){
        for(int j = 0; j < linkTraps.size(); j++){
            if(me.robots.at(i).distance(Point({linkTraps.at(j).x, linkTraps.at(j).y})) <= 1){
                myRobots.at(i) = true;
            }
            if(enemy.robots.at(i).distance(Point({linkTraps.at(j).x, linkTraps.at(j).y})) <= 1){
                enemyRobots.at(i) = true;
            }
        }
        /*
        for(int j = 0; j < dangerPointList.size(); j++){
            if(dangerPointList.at(j).x == 1 && me.robots.at(i).distance(Point({dangerPointList.at(j).x, dangerPointList.at(j).y})) <= 1){
                myRobots.at(i) = true;
            }
            if(dangerPointList.at(j).x == 1 && enemy.robots.at(i).distance(Point({dangerPointList.at(j).x, dangerPointList.at(j).y})) <= 1){
                enemyRobots.at(i) = true;
            }
        }*/
    }
    
    // check how many of ours beside
    int headCountOur = 0;
    int headCountEnemy = 0;
    for(int i = 0; i < MAX_ROBOTS; i++){
        if(myRobots.at(i) == true)
            headCountOur++;
        if(enemyRobots.at(i) == true)
            headCountEnemy++;
    }
    
    if(targetVictim != -1){
        if(headCountEnemy > headCountOur){
            kamikaze = true;
        }
    }
    else{
        cerr << "Target Victim:" << targetVictim << "\n";
        if(headCountEnemy > targetVictim){
            kamikaze = true;
        }
    }
    
    return kamikaze;
}

Point SetMovePoint(Game& game, Player& me, array<Action, MAX_ROBOTS>& actions, Point _dest, int id){
    // if out of range 
    vector<Point> candidateList;
        
    int minDist = -1;
    Point movePoint = Point{-1,-1};
    for(int x = me.robots.at(id).x - 4; x <= me.robots.at(id).x + 4; x++){
        for(int y = me.robots.at(id).y - 4; y <= me.robots.at(id).y + 4; y++){
            if(me.robots.at(id).distance(Point{x,y})<=4){
                candidateList.push_back(Point{x,y});
            }
        }
    }
    for(int i = 0; i < candidateList.size(); i++){
        bool isSafe = IsSafety(game.traps, me, game.players.at(1), actions, candidateList.at(i));
        if(isSafe){
            if(minDist == -1){
                movePoint = candidateList.at(i);
                minDist = candidateList.at(i).distance(_dest);
            }
            if(candidateList.at(i).distance(_dest) < minDist){
                movePoint = candidateList.at(i);
                minDist = candidateList.at(i).distance(_dest);
            }
        }
    }
    return movePoint;
}

void NewRadarPoint(Game& game, Player& me, array<Action, MAX_ROBOTS>& actions, int id, vector<Point> radarPointList){
    // move to a highscore point
    int minDistance = -1;
    bool nearbyRadar = false;
    Point highPoint{0,0};
    
    //cerr << "RadarPointList:" << radarPointList.size() << "\n";
    if(radarPointList.size() > 0){
        for(int j = 0; j < radarPointList.size(); j++){
            // check any robot with radar going this point
            bool someOneGoing = false;
            for(int i=0; i < MAX_ROBOTS; i++){
                if(i == id) continue;
                if(radarPointList.at(j).distance(Point{actions.at(i).dest.x,actions.at(i).dest.y}) == 0 &&
                    actions.at(i).type == ActionType::DIG &&
                    me.robots.at(i).item == Type::RADAR){
                    someOneGoing = true;
                    break;
                }
            }
            // check any nearby radar
            nearbyRadar = CheckRadar(game.radars, Point{radarPointList.at(j).x,radarPointList.at(j).y});
            bool isSafe = IsSafety(game.traps, me, game.players.at(1), actions, Point{radarPointList.at(j).x,radarPointList.at(j).y});
            if(!someOneGoing && isSafe && !nearbyRadar && game.get(radarPointList.at(j).x,radarPointList.at(j).y).hole != true){
                // check within range or not, if not return a middle point
                if(minDistance == -1){
                    highPoint = Point{radarPointList.at(j).x, radarPointList.at(j).y};
                    minDistance = me.robots.at(id).distance(highPoint);
                }
                if(me.robots.at(id).distance(Point{radarPointList.at(j).x, radarPointList.at(j).y}) < minDistance){
                    highPoint = Point{radarPointList.at(j).x, radarPointList.at(j).y};
                    minDistance = me.robots.at(id).distance(highPoint);
                }
            }
            //cerr << "nearbyRadar:" << nearbyRadar << " some1:" << someOneGoing << " Point x:" << radarPointList.at(j).x << " y:" << radarPointList.at(j).y << " min:"<< minDistance <<"\n";
        }
    }
    
    actions.at(id).dig(highPoint, "OTW W/Radar");
}

Point NewDigPoint(Game& game, Player& me, array<Action, MAX_ROBOTS>& actions, int id, float highScore){
    // move to a highscore point
    int minDistance = -1;
    bool trapFound = false;
    bool dangerFlag = false;
    bool isSafe = false;
    bool enemyCloser = false;
    bool speardOut = false;
    vector<Point> candidateList;
    vector<int> oreCountList;
    Point digPoint{0,0};

    // if no valid hole
    if(candidateList.size() <= 0){
        // scan dig list
        for(int j=0; j<game.digPointList.size(); j++){
            // skip trap tile
            trapFound = CheckTrap(game.traps, Point{game.digPointList.at(j).x,game.digPointList.at(j).y});
            dangerFlag = IsDanger(Point{game.digPointList.at(j).x,game.digPointList.at(j).y});
            isSafe = IsSafety(game.traps, me, game.players.at(1), actions, Point{game.digPointList.at(j).x,game.digPointList.at(j).y});
            if(!trapFound && !dangerFlag && isSafe){
                // check how many robot going
                int count = 0;
                for(int i=0; i < MAX_ROBOTS; i++){
                    if(i == id) continue;
                    if((game.digPointList.at(j).distance(Point{actions.at(i).dest.x,actions.at(i).dest.y}) == 0)||
                        (me.robots.at(i).distance(game.digPointList.at(j)) == 0)){
                        count++;
                    }
                }
                // if still have vacancy go there
                if(count <= 0){
                    candidateList.push_back(Point{game.digPointList.at(j).x,game.digPointList.at(j).y});
                    oreCountList.push_back(game.get(game.digPointList.at(j).x, game.digPointList.at(j).y).ore);
                }
            }
        }
    }
    
    if(candidateList.size() <= 0){
        // if someone with radar exist, x is ahead go to its dest
        for(int i=0; i < MAX_ROBOTS; i++){
            if(i == id) continue;
            if(me.robots.at(i).item == Type::RADAR && 
                actions.at(i).type == ActionType::DIG &&
                me.robots.at(i).distance(actions.at(i).dest) <= me.robots.at(id).distance(actions.at(i).dest)){
                    candidateList.push_back(actions.at(i).dest);
                    oreCountList.push_back(game.get(actions.at(i).dest.x, actions.at(i).dest.y).ore);
            }
        }
    }
    
    // if still no valid candidate
    if(candidateList.size() <= 0){
        // scan general tile for ore -1 with no hole with high score
        float score = 0;
        for(int x = 1; x < MAX_WIDTH; x++){
            for(int y = 0; y < MAX_HEIGHT; y++){
                score = game.get(x,y).score + AdditionalRadarScore(game, Point{x,y});
                isSafe = IsSafety(game.traps, me, game.players.at(1), actions, Point{x,y});
                if(score > highScore*0.90 && isSafe && game.get(x,y).hole != true && game.get(x,y).ore == -1){
                    // check how many robot going
                    bool someoneGoing = false;
                    for(int i=0; i < MAX_ROBOTS; i++){
                        if(i == id) continue;
                        if((game.get(x,y).distance(Point{actions.at(i).dest.x,actions.at(i).dest.y}) == 0)||
                            (me.robots.at(i).distance(game.get(x,y)) == 0)){
                            someoneGoing = true;
                        }
                    }
                    if(!someoneGoing){
                        candidateList.push_back(Point{x,y});
                        oreCountList.push_back(game.get(x,y).ore);
                    }
                }
            }
        }
    }
    
    
    // if still no valid candidate
    if(candidateList.size() <= 0){
        // scan general tile for ore -1 with no hole with high score
        int tileCount = 0;
        for(int x = 1; x < MAX_WIDTH; x++){
            for(int y = 0; y < MAX_HEIGHT; y++){
                isSafe = IsSafety(game.traps, me, game.players.at(1), actions, Point{x,y});
                if(game.get(x,y).hole != true && isSafe && game.get(x,y).ore == -1){
                    // check how many robot going
                    bool someoneGoing = false;
                    for(int i=0; i < MAX_ROBOTS; i++){
                        if(i == id) continue;
                        if((game.get(x,y).distance(Point{actions.at(i).dest.x,actions.at(i).dest.y}) == 0)||
                            (me.robots.at(i).distance(game.get(x,y)) == 0)){
                            someoneGoing = true;
                        }
                    }
                    if(!someoneGoing){
                        candidateList.push_back(Point{x,y});
                        oreCountList.push_back(game.get(x,y).ore);
                    }
                    tileCount++;
                }
            }
        }
    }
  
    // if not carry anything
    for(int j=0; j<candidateList.size(); j++){
        if(minDistance == -1) {
            //highScore = game.get(Point({game.digPointList.at(j).x, game.digPointList.at(j).y})).score;
            minDistance = me.robots.at(id).distance(candidateList.at(j));
            digPoint = Point({candidateList.at(j).x, candidateList.at(j).y});
        }
        if(me.robots.at(id).distance(candidateList.at(j)) < minDistance){
            minDistance = me.robots.at(id).distance(candidateList.at(j));
            digPoint = Point({candidateList.at(j).x, candidateList.at(j).y});
        }
    }
    
    //cerr << "Dig Point x:" << digPoint.x << " y:" << digPoint.y << " Dist:" << minDistance <<"\n";
    
    return digPoint;
}

void NewTrapPoint(Game& game, Player& me, Player& enemy, array<Action, MAX_ROBOTS>& actions, int id){
    // check outside x = 1
    bool dangerFlag = false;
    bool trapFound = false;
    bool isSafe = false;
    bool otherGoing = false;
    bool noEnemyBeside = false;
    vector<Point> candidateList;
    Point _p{-1,-1};
    int minY = -1;
    int newCont = -1;
    int wall = 1;
    // priority set up first
    // 1, 3, 5, 7, 9, 11,13
    while(_p.x == -1){
        for(int y=2; y<MAX_HEIGHT-3; y++){
            dangerFlag = IsDanger(Point{wall, y});
            trapFound = CheckTrap(game.traps, Point{wall, y});
            isSafe = IsSafety(game.traps, me, enemy, actions, Point{wall,y});
            
            otherGoing = false;
            for(int i=0; i<MAX_ROBOTS; i++){
                if(id == i) continue;
                if(actions.at(i).dest.distance(Point{wall,y}) <= 1||me.robots.at(i).distance(Point{wall,y}) <= 1)
                    otherGoing = true;
            }
            noEnemyBeside = false;
            for(int i=0; i<MAX_ROBOTS; i++){
                if(enemy.robots.at(i).distance(Point{wall,y}) <= 1)
                    noEnemyBeside = true;
            }
            if(me.robots.at(id).item == Type::TRAP){
                if(!dangerFlag && !trapFound && !otherGoing && isSafe){
                    candidateList.push_back(Point{wall,y});
                }
            }
            else{
                if(!dangerFlag && !trapFound && !otherGoing && isSafe && game.get(wall,y).hole != true){
                    candidateList.push_back(Point{wall,y});
                }
            }
        }
    
        for(int i=0; i < candidateList.size(); i++){
            if(minY == -1){
                _p = candidateList.at(i);
                minY = me.robots.at(id).distance(_p);
            }
            if(me.robots.at(id).distance(candidateList.at(i)) < minY){
                _p = candidateList.at(i);
                minY = me.robots.at(id).distance(_p);
            }
        }
        wall++;
    }
    
    actions.at(id).dig(_p, "DIG TRAP");
}

void MoveBack(Game& game, Player& me, array<Action, MAX_ROBOTS>& actions, int id, Type _type){
    vector<Point> candidateList;
    int minDist = -1;
    Point homePoint = Point{-1,-1};
    for(int y=0;y<MAX_HEIGHT;y++){
        bool isSafe = IsSafety(game.traps, me, game.players.at(1), actions, Point{0, y});
        if(isSafe)
        {
            candidateList.push_back(Point{0,y});
        }
    }
    for(int z=0; z<candidateList.size(); z++)
    {
        if(minDist == -1) {
            homePoint = candidateList.at(z);
            minDist = candidateList.at(z).distance(homePoint);
        }
        if(candidateList.at(z).distance(homePoint) < minDist){
            homePoint = candidateList.at(z);
            minDist = candidateList.at(z).distance(homePoint);
        }
    }
    if(homePoint.distance(me.robots.at(id)) >4)
    {
        Point thisPt = SetMovePoint(game, me, actions, homePoint, id);
        if(thisPt.x != -1){
            actions.at(id).move(thisPt, "GO HOME");
        }
        else{
            bool isSafe = IsSafety(game.traps, me, game.players.at(1), actions, me.robots.at(id));
            if(isSafe)
                actions.at(id).wait("Wait HOME");
            else
            {
                for(int x=me.robots.at(id).x-4; x <= me.robots.at(id).x+4; x++){
                    if(x < 0) continue;
                    bool newSafe = IsSafety(game.traps, me, game.players.at(1), actions, Point{x, me.robots.at(id).y});
                    if(newSafe)
                        actions.at(id).move(Point{x, me.robots.at(id).y}, "Evade!");
                    else{
                        for(int y=me.robots.at(id).y-4; y < me.robots.at(id).y+4; y++){
                            if(y < 0||y >= MAX_HEIGHT-1) continue; 
                            newSafe = IsSafety(game.traps, me, game.players.at(1), actions, Point{me.robots.at(id).x, y});
                            if(newSafe)
                                actions.at(id).move(Point{me.robots.at(id).x, y}, "Evade!");
                        }
                    }
                }
            }
        }
        
    }
    else
    {
        if(_type == Type::RADAR){
            actions.at(id).request(Type::RADAR, "GO HOME URGENT");
        }
        else{
            actions.at(id).move(homePoint, "GO HOME");
        }
    }
}

//*********************************  AI  *****************************************************************
bool destroyFirst = true;
bool noMinePlayer = false;

array<Action, MAX_ROBOTS> getActions(Game& game) {
    array<Action, MAX_ROBOTS> actions;
    Player& me{game.players.at(0)};
    Player& enemy{game.players.at(1)};
    
    //*********************************  ORE & new hole Analysis  *****************************************************************
    // for orereducelist and newholelist that cause by us remove it
    vector<int> cleanUp;
    cleanUp.clear();
    for(int j = 0; j < game.oreReduceList.size(); j++){
        // is our robot dig here
        int index = -1;
        for(int k = 0; k < MAX_ROBOTS; k++){
            // previous action dig here and robot now beside here
            if(prevActions.at(k).dest.distance(game.oreReduceList.at(j)) == 0 && prevActions.at(k).type == ActionType::DIG &&
                me.robots.at(k).distance(game.oreReduceList.at(j)) <= 1 && me.robots.at(k).distance(prevOurPos.at(k)) == 0){
                index = j;
                break;
            }
        }
        if(index != -1) 
            cleanUp.push_back(index);
    }
    // remove item in clean up
    for(int j = 0; j < cleanUp.size(); j++){
        game.oreReduceList.erase(game.oreReduceList.begin()+cleanUp.at(j));
    }
    
    cleanUp.clear();
    for(int j = 0; j < game.newHoleList.size(); j++){
        // is our robot dig here
        int index = -1;
        for(int k = 0; k < MAX_ROBOTS; k++){
            // previous action dig here and robot now beside here
            if(prevActions.at(k).dest.distance(game.newHoleList.at(j)) == 0 && prevActions.at(k).type == ActionType::DIG &&
                me.robots.at(k).distance(game.newHoleList.at(j)) <= 1 && me.robots.at(k).distance(prevOurPos.at(k)) == 0){
                index = j;
                break;
            }
        }
        if(index != -1) 
            cleanUp.push_back(index);
    }
    // remove item in clean up
    for(int j = 0; j < cleanUp.size(); j++){
        game.newHoleList.erase(game.newHoleList.begin()+cleanUp.at(j));
    }
    
    //*********************************  Enemy Analysis  *****************************************************************
    // compare Enemy position to prev
    for(int i = 0; i < MAX_ROBOTS; i++){
        // it spend 1 turn at home not moving, might get radar or trap or fraud, put into suspect
        if(enemy.robots.at(i).distance(prevEnemyPos.at(i)) == 0 && enemy.robots.at(i).x == 0 && !suspectID.at(i)){
            suspectID.at(i) = true;
            if(suspectID.at(i))
                cerr << "SuspectID:" << i << " added\n";
        }
        else if(enemy.robots.at(i).distance(prevEnemyPos.at(i)) == 0 && enemy.robots.at(i).x == 0 && suspectID.at(i)){
             for(int j = 0; j < game.newHoleList.size(); j++){
                // if enemy around the new hole
                if(enemy.robots.at(i).distance(game.newHoleList.at(j)) <= 1){
                    // is our robot dig here
                    bool digHere = false;
                    for(int k = 0; k < MAX_ROBOTS; k++){
                        // previous action dig here and robot now stop here 1turn
                        if(prevActions.at(k).dest.distance(game.newHoleList.at(j)) == 0 && prevActions.at(k).type == ActionType::DIG &&
                            me.robots.at(k).distance(game.newHoleList.at(j)) <= 1 && me.robots.at(k).distance(prevOurPos.at(k)) == 0){
                            digHere = true;
                            break;
                        }
                    }
                    
                    // if our robot didn't dig here
                    if(!digHere){
                        InsertDangerPt(game.newHoleList.at(j)); // trap or radar here
                        suspectID.at(i) = false; // drop suspect
                        cerr << "SuspectID:" << i << " dropped\n";
                    }
                }
            }
        }
        if(enemy.robots.at(i).distance(prevEnemyPos.at(i)) != 0 && prevEnemyPos.at(i).x > 0 && enemy.robots.at(i).x == 0){
            suspectID.at(i) = false;
            cerr << "SuspectID:" << i << " dropped\n";
        }
        
        // it suspect spend 1 turn outside not moving we want to know where he put trap/radar
        if(enemy.robots.at(i).distance(prevEnemyPos.at(i)) == 0 && enemy.robots.at(i).x > 0 && suspectID.at(i)){
            // what happen to up/down/left/right/center
            for(int x = enemy.robots.at(i).x-1; x <= enemy.robots.at(i).x+1; x++){
                for(int y = enemy.robots.at(i).y-1; y <= enemy.robots.at(i).y+1; y++){
                    //up down left right center tile
                    if(x-1 < 1 || x+1 >= MAX_WIDTH-1) continue;
                    if(y-1 < 0 || y+1 >= MAX_HEIGHT-1) continue;
                    if(enemy.robots.at(i).distance(Point{x,y}) <=1){
                        // if not under our radar
                        if(game.get(enemy.robots.at(i).x, enemy.robots.at(i).y).oreVisible != true){
                            // if new hole appear here
                            // if not our robot
                            // if more than 1 enemy robot here
                            // if no other robot stop nearby this adjacent tile
                            for(int j = 0; j < game.newHoleList.size(); j++){
                                // if enemy around the new hole
                                if(enemy.robots.at(i).distance(game.newHoleList.at(j)) <= 1){
                                    // is our robot dig here
                                    bool digHere = false;
                                    for(int k = 0; k < MAX_ROBOTS; k++){
                                        // previous action dig here and robot now stop here 1turn
                                        if(prevActions.at(k).dest.distance(game.newHoleList.at(j)) == 0 && prevActions.at(k).type == ActionType::DIG &&
                                            me.robots.at(k).distance(game.newHoleList.at(j)) <= 1 && me.robots.at(k).distance(prevOurPos.at(k)) == 0){
                                            digHere = true;
                                            break;
                                        }
                                    }
                                    
                                    // if our robot didn't dig here
                                    if(!digHere){
                                        InsertDangerPt(game.newHoleList.at(j)); // trap or radar here
                                        suspectID.at(i) = false; // drop suspect
                                    }
                                }
                            }
                        }
                        else{
                            // if new hole appear here
                            // new hole list
                            for(int j = 0; j < game.newHoleList.size(); j++){
                                // if enemy around the new hole
                                if(enemy.robots.at(i).distance(game.newHoleList.at(j)) <= 1){
                                    // is our robot dig here
                                    bool digHere = false;
                                    for(int k = 0; k < MAX_ROBOTS; k++){
                                        // previous action dig here and robot now stop here 1turn
                                        if(prevActions.at(k).dest.distance(game.newHoleList.at(j)) == 0 && prevActions.at(k).type == ActionType::DIG &&
                                            me.robots.at(k).distance(game.newHoleList.at(j)) <= 1 && me.robots.at(k).distance(prevOurPos.at(k)) == 0){
                                            digHere = true;
                                            break;
                                        }
                                    }
                                    
                                    // if our robot didn't dig here
                                    if(!digHere){
                                        InsertDangerPt(game.newHoleList.at(j)); // trap or radar here
                                        suspectID.at(i) = false; // drop suspect
                                    }
                                }
                            }
                            
                            // scan reduce list
                            for(int j = 0; j < game.oreReduceList.size(); j++){
                                // if enemy around the ore reduce tile
                                if(enemy.robots.at(i).distance(game.oreReduceList.at(j)) <= 1){
                                   // is our robot dig here last round
                                    bool digHere = false;
                                    for(int k = 0; k < MAX_ROBOTS; k++){
                                        // previous action dig here and robot now stop here 1turn
                                        if(prevActions.at(k).dest.distance(game.oreReduceList.at(j)) == 0 && prevActions.at(k).type == ActionType::DIG &&
                                            me.robots.at(k).distance(game.oreReduceList.at(j)) <= 1 && me.robots.at(k).distance(prevOurPos.at(k)) == 0){
                                            digHere = true;
                                            break;
                                        }
                                    }
                                    // if our robot not dig here last round
                                    if(!digHere){
                                        InsertDangerPt(game.oreReduceList.at(j));
                                        suspectID.at(i) = false; // drop suspect
                                    }
                                }
                            }
                        }
                        
                    }
                }
            }
                
            // if still suspect all hole add to danger list
            if(suspectID.at(i) == true){
                for(int x = enemy.robots.at(i).x-1; x <= enemy.robots.at(i).x+1; x++){
                    for(int y = enemy.robots.at(i).y-1; y <= enemy.robots.at(i).y+1; y++){
                        //up down left right center tile
                        if(x < 1 || x >= MAX_WIDTH-1) continue;
                        if(y < 0 || y >= MAX_HEIGHT-1) continue;
                        if(enemy.robots.at(i).distance(Point{x,y}) <=1){
                            // those tile not under radar add to list
                            bool inReduceList = false;
                            for(int j = 0; j < game.oreReduceList.size(); j++){
                                if(game.oreReduceList.at(j).distance(Point{x,y}) == 0){
                                    inReduceList = true;
                                }
                            }
                            bool inNewHoleList = false;
                            for(int j = 0; j < game.newHoleList.size(); j++){
                                if(game.newHoleList.at(j).distance(Point{x,y}) == 0){
                                    inNewHoleList = true;
                                }
                            }
                            if(game.get(x,y).oreVisible != true && !inNewHoleList){
                                InsertDangerPt(Point{x,y});
                            }
                            if(game.get(x,y).oreVisible == true && game.get(x,y).hole == true && !inNewHoleList && !inReduceList){
                                InsertDangerPt(Point{x,y});
                            }
                        }
                    }
                }
            }
        }
    }
    //*********************************  MAP Analysis End *****************************************************************
    
    //*********************************  INIT START *****************************************************************
    // init all actions
    for(int i = 0; i < MAX_ROBOTS; i++){
        if(prevActions.at(i).message == "DESTROY" && me.robots.at(i).distance(prevOurPos.at(i)) != 0){
            actions.at(i).dig(prevActions.at(i).dest, "DESTROY");
        }
        else if(prevActions.at(i).message == "DESTROY" && me.robots.at(i).distance(prevOurPos.at(i)) == 0 && me.robots.at(i).distance(prevActions.at(i).dest) <= 1){
            actions.at(i).wait("READY");
            int index = -1;
            for(int j = 0;  j < dangerPointList.size(); j++){
                if(dangerPointList.at(j).distance(prevActions.at(i).dest) == 0){
                    index = j;
                }
            }
            if(index != -1){
                dangerPointList.erase(dangerPointList.begin() + index);
                cerr << "remove index:" << index << "\n";
            }
        }
        else if(prevActions.at(i).type == ActionType::WAIT && prevActions.at(i).message == "Gimme HOLE" && me.robots.at(i).x == 0){
            // give him a hole
            me.robots.at(i).item = Type::HOLE;
            cerr << "Given HOLE \n";
            game.fakeCD = 5;
            actions.at(i).wait("READY");
        }
        else if(prevActions.at(i).type == ActionType::MOVE && prevActions.at(i).message == "OTW HOLE"){
            me.robots.at(i).item = Type::HOLE;
            actions.at(i).wait("READY");
        }
        else{
            actions.at(i).wait("READY");
        }
        if(me.robots.at(i).item == Type::HOLE){
            cerr << "ROBOT " << i << " holding hole\n";
        }
    }
    for(int i = 0; i < MAX_ROBOTS; i++){
        // for robot dead
        if(me.robots.at(i).x == -1){
            actions.at(i).wait("DEAD");
        }
    }
    
    // smart code here
    if (game.players.at(0).ore <= 0) {
        cerr << "time to collect stuf!\n";
    }
    //*********************************  Radar Start  *****************************************************************
    // map visibility check any radar needed
    bool radarNeeded = false;
    bool radarUrgent = false;
    
    float highestScore = 0;
    vector<Point> radarPointList;
    for(int y = 0; y < MAX_HEIGHT; y++){
        for(int x = 1; x < MAX_WIDTH; x++){
            // check current highest score
            float score = game.get(x,y).score + AdditionalRadarScore(game, Point{x,y});
            bool nearbyRadar = CheckRadar(game.radars, Point{x,y});
            // check any nearby radar
            
            if(score > highestScore && game.get(x,y).hole != true && !nearbyRadar)
                highestScore = score;
        }
        //cerr << "\n";
    }
    cerr << "Highest Score: " << highestScore << "\n";
    // all candidate that score within range 0.95
    for(int y = 0; y < MAX_HEIGHT; y++){
        for(int x = 1; x < MAX_WIDTH; x++){
            // check current highest score
            float score = game.get(x,y).score + AdditionalRadarScore(game, Point{x,y});
            bool nearbyRadar = CheckRadar(game.radars, Point{x,y});
            if(score > highestScore*0.95 && game.get(x,y).hole != true && !nearbyRadar)
                radarPointList.push_back(Point{x,y});
        }
    }
    if(radarPointList.size() > 0)
        radarNeeded = true;
    
    // safe digPoint
    int safePoint = 0;
    for(int i=0; i<game.digPointList.size(); i++){
        if(game.get(game.digPointList.at(i).x,game.digPointList.at(i).y).hole != true){
            safePoint++;
        }
    }
    if(safePoint <= 10 && radarNeeded){
        radarUrgent = true;
    }
    
    if(safePoint > 20){
        radarNeeded = false;
    }
    
    // if someone already carry radar
    for(int i = 0; i < MAX_ROBOTS; i++){
        if(me.robots.at(i).item == Type::RADAR){
            radarUrgent = false;
            break;
        }
    }
    
    cerr << "Radar Needed:" << radarNeeded << " Urgent:" << radarUrgent << "\n";
    cerr << "DigPointList: " << game.digPointList.size() << "\n";
    
    if(radarNeeded && me.cooldownRadar == 0){
        // if radar available and no one request
        bool requestByOther = false;
        for(int j = 0; j < MAX_ROBOTS; j++){
            if(actions.at(j).type == ActionType::REQUEST && actions.at(j).item == Type::RADAR){
                requestByOther = true;
                break;
            }
        }
        if(!requestByOther){
            int distY = -1;
            int id = -1;
            for(int i = 0; i < MAX_ROBOTS; i++){
                // if at base
                if(me.robots.at(i).x == 0 && me.robots.at(i).item != Type::TRAP){
                    if(id == -1){
                        id = i;
                        distY = abs(me.robots.at(i).y - 7);
                        cerr << "distY:" << distY << "\n";
                    }
                    if(abs(me.robots.at(i).y - 7) < distY){
                        id = i;
                        distY = abs(me.robots.at(i).y - 7);
                        cerr << "distY:" << distY << "\n";
                    }
                }
            }
            // if have a valid robot
            if(id != -1){
                actions.at(id).request(Type::RADAR, "GIMME RADAR!");
            }
            else{
                // if this is a urgent request
                if(radarUrgent){
                    int id = -1;
                    // find free robot that closest to base
                    for(int j = 0; j < MAX_ROBOTS; j++){
                        if((me.robots.at(j).item == Type::NONE||me.robots.at(j).item == Type::ORE) && me.robots.at(j).x > 0 &&
                        actions.at(j).message != "DESTROY" && actions.at(j).item != Type::TRAP){
                            if(id == -1) id = j;
                            if(me.robots.at(j).x < me.robots.at(id).x)
                                id = j;
                        }
                    }
                    if(id != -1)
                        MoveBack(game, me, actions, id, Type::RADAR);
                }
            }
        }
    }
    
    if(game.turn == 10){
        cerr << dangerPointList.size() << " dangerPt!";
        if(dangerPointList.size()<=2){
            noMinePlayer = true;
        }
    }
    /*if(destroyFirst == true && game.turn == 5){
        // if there is 2 suspect
        
        vector<Point> toDestroy;
        // furthest danger
        int minX = -1;
        Point thePt = Point{-1,-1};
        for(int j = 0; j < dangerPointList.size(); j++){
            if(minX == -1){
                thePt = dangerPointList.at(j);
            }
            if(dangerPointList.at(j).x > thePt.x){
                thePt = dangerPointList.at(j);
            }
            //cerr << "DangerPT " << j << ":" << dangerPointList[j].x << "," << dangerPointList.at(j).y << "\n";
        }
        if(thePt.x != -1){
            for(int i = 0; i < MAX_ROBOTS; i++){
                if(me.robots.at(i).item == Type::NONE && actions.at(i).message == "READY"){
                    cerr << "Destroy:" << thePt.x << "," << thePt.y << "\n";
                    bool requestByOther = false;
                    for(int k = 0; k < MAX_ROBOTS; k++){
                        if(actions.at(k).type == ActionType::DIG && actions.at(k).message == "DESTROY"){
                            if(k == i) continue;
                            requestByOther = true;
                            break;
                        }
                    }
                    if(!requestByOther){
                        actions.at(i).dig(thePt, "DESTROY");
                        destroyFirst = false;
                        break;
                    }
                }
            }
        }
    }
    if(noMinePlayer == true){
        for(int i = 0; i < MAX_ROBOTS; i++){
            if(me.robots.at(i).item == Type::NONE && actions.at(i).message == "READY"){
                for(int j = 0; j < dangerPointList.size(); j++){
                    //cerr << j << ":" << dangerPointList.at(j).x << "," << dangerPointList.at(j).y << "\n";
                    // any enemyrobot around
                    bool theyAlso = false;
                    for(int k = 0; k < MAX_ROBOTS; k++){
                        if(enemy.robots.at(k).distance(dangerPointList.at(j)) <= 1){
                            theyAlso = true;
                            break;
                        }
                    }
                    if(me.robots.at(i).distance(dangerPointList.at(j)) <= 1 && theyAlso){
                        actions.at(i).dig(dangerPointList.at(j), "DESTROY");
                    }
                }
            }
        }
    }
    */
    //*********************************  Radar End  *****************************************************************
    for(int i = 0; i < MAX_ROBOTS; i++){
        // for robot dead
        if(me.robots.at(i).x == -1){
            actions.at(i).wait("Dead");
        }
//*********************************  Rbbot In Base  *****************************************************************
        else if(me.robots.at(i).x == 0){
            //*********************************  Radar Carrier Start  *****************************************************************
            // if robot carry radar move to best radar point
            if(me.robots.at(i).item == Type::RADAR && actions.at(i).message == "READY"){
                NewRadarPoint(game, me, actions, i, radarPointList);
            }
            //*********************************  Radar Carrier End  *****************************************************************
            else if(me.robots.at(i).item == Type::TRAP && actions.at(i).message == "READY"){
                // if on mission wall 1
                if(game.traps.size() < 10){
                    NewTrapPoint(game, me, enemy, actions, i);
                }
                else{
                    Point newPoint = NewDigPoint(game, me, actions, i, highestScore);
                    actions.at(i).dig(newPoint, "OTW TRAP");
                }
                
            }
            else if(me.robots.at(i).item == Type::HOLE && actions.at(i).message == "READY"){
                // if on mission wall 1
                if(game.traps.size() < 10){
                    NewTrapPoint(game, me, enemy, actions, i);
                }
                else{
                    Point newPoint = NewDigPoint(game, me, actions, i, highestScore);
                    actions.at(i).dig(newPoint, "OTW HOLE");
                }
            }
            else if(me.robots.at(i).item == Type::NONE && actions.at(i).message == "READY"){
                //*********************************  TRAP Assigned End *****************************************************************
                bool requestByOther = false;
                for(int j = 0; j < MAX_ROBOTS; j++){
                    if(actions.at(j).type == ActionType::REQUEST && actions.at(j).item == Type::TRAP){
                        if(j == i) continue;
                        requestByOther = true;
                        break;
                    }
                }
                // more alive enemy?
                int countMe = 0;
                int countYou = 0;
                for(int j = 0; j < MAX_ROBOTS; j++){
                    if(me.robots.at(j).x != -1){
                        countMe++;
                    }
                    if(enemy.robots.at(j).x != -1){
                        countYou++;
                    }
                }
                
                // if got any tile worth putting
                if(me.cooldownTrap == 0 && !requestByOther && countMe <= countYou && countMe > 2){
                    // check on robot at base
                    cerr << countMe << " vs " << countYou << "\n";
                    if(game.traps.size() < 10)
                        actions.at(i).request(Type::TRAP, "Gimme Trap");

                }
                //*********************************  TRAP Assigned End *****************************************************************
                
                //*********************************  FAKE Assigned Start *****************************************************************
                bool fakeByOther = false;
                for(int j = 0; j < MAX_ROBOTS; j++){
                    if(actions.at(j).type == ActionType::WAIT && actions.at(j).message == "Gimme HOLE"){
                        if(j == i) continue;
                        fakeByOther = true;
                        break;
                    }
                }
                
                // if got any tile worth putting
                if(game.fakeCD == 0 && !fakeByOther && countMe <= countYou && countMe > 2){
                    // check on robot at base
                    if(game.traps.size() < 10)
                        actions.at(i).wait("Gimme HOLE");
                    
                }
                //*********************************  FAKE Assigned End *****************************************************************
                
                
                // default action if still no action
                if(actions.at(i).message == "READY"){
                    Point newPoint = NewDigPoint(game, me, actions, i, highestScore);
                    actions.at(i).dig(newPoint, "OTW");
                }
            }
        }
//*********************************  Rbbot Not In Base  *****************************************************************    
        else{
            if(me.robots.at(i).item == Type::RADAR && actions.at(i).message == "READY"){
                //if reach destination
                NewRadarPoint(game, me, actions, i, radarPointList);
            }
            else if(me.robots.at(i).item == Type::TRAP && actions.at(i).message == "READY"){
                // if on mission wall 1
                if(game.traps.size() <= 10){
                    NewTrapPoint(game, me, enemy, actions, i);
                }
                else{
                    Point newPoint = NewDigPoint(game, me, actions, i, highestScore);
                    actions.at(i).dig(newPoint, "OTW TRAP");
                }
            }
            else if(me.robots.at(i).item == Type::HOLE && actions.at(i).message == "READY"){
                // if on mission wall 1
                if(game.traps.size() <= 10){
                    NewTrapPoint(game, me, enemy, actions, i);
                }
                else{
                    Point newPoint = NewDigPoint(game, me, actions, i, highestScore);
                    actions.at(i).dig(newPoint, "OTW HOLE");
                }
            }
            else if(me.robots.at(i).item == Type::NONE && actions.at(i).message == "READY"){
                //if reach destination
                Point newPoint = NewDigPoint(game, me, actions, i, highestScore);
                actions.at(i).dig(newPoint, "OTW");
            }
            // override if carry ore
            else if(me.robots.at(i).item == Type::ORE && actions.at(i).message == "READY"){
                vector<Point> candidateList;
                int minDist = -1;
                Point homePoint = Point{-1,-1};
                for(int y=0;y<MAX_HEIGHT;y++){
                    bool isSafe = IsSafety(game.traps, me, game.players.at(1), actions, Point{0, y});
                    if(isSafe)
                    {
                        candidateList.push_back(Point{0,y});
                    }
                }
                for(int z=0; z<candidateList.size(); z++)
                {
                    //cerr << "homelist: " << candidateList.at(z).x << "," << candidateList.at(z).y << "\n";
                    if(minDist == -1) {
                        homePoint = candidateList.at(z);
                        minDist = candidateList.at(z).distance(me.robots.at(i));
                    }
                    if(candidateList.at(z).distance(me.robots.at(i)) < minDist){
                        homePoint = candidateList.at(z);
                        minDist = candidateList.at(z).distance(me.robots.at(i));
                    }
                }
                if(homePoint.distance(me.robots.at(i)) >4)
                {
                    Point thisPt = SetMovePoint(game, me, actions, homePoint, i);
                    if(thisPt.x != -1){
                        actions.at(i).move(thisPt, "GO HOME");
                    }
                    else{
                        bool isSafe = IsSafety(game.traps, me, game.players.at(1), actions, me.robots.at(i));
                        if(isSafe)
                            actions.at(i).wait("Wait HOME");
                        else
                        {
                            for(int x=me.robots.at(i).x-4; x <= me.robots.at(i).x+4; x++){
                                if(x < 0) continue;
                                bool newSafe = IsSafety(game.traps, me, game.players.at(1), actions, Point{x, me.robots.at(i).y});
                                if(newSafe)
                                    actions.at(i).move(Point{x, me.robots.at(i).y}, "Evade!");
                                else{
                                    for(int y=me.robots.at(i).y-4; y < me.robots.at(i).y+4; y++){
                                        if(y < 0||y >= MAX_HEIGHT-1) continue; 
                                        newSafe = IsSafety(game.traps, me, game.players.at(1), actions, Point{me.robots.at(i).x, y});
                                        if(newSafe)
                                            actions.at(i).move(Point{me.robots.at(i).x, y}, "Evade!");
                                    }
                                }
                            }
                        }
                    }
                    
                }
                else
                {
                    actions.at(i).move(homePoint, "GO HOME");
                }
            }
        }
    }
    
    // kamikaze
    for(int j = 0; j < MAX_ROBOTS; j++){
        // beside a bomb?
        for(int i = 0; i< game.traps.size(); i++){
            if(me.robots.at(j).distance(game.traps.at(i)) <= 1){
                // check worth explosion or not
                if(me.ore < enemy.ore){
                    bool kamikaze = CheckExplosion(game, me, enemy, game.traps.at(i),2);
                    if(kamikaze){
                        actions.at(j).dig(game.traps.at(i), "KAMIKAZE");
                    }
                }
                else{
                    bool kamikaze = CheckExplosion(game, me, enemy, game.traps.at(i),1);
                    if(kamikaze){
                        actions.at(j).dig(game.traps.at(i), "KAMIKAZE");
                    }
                }
            }
        }
    }
    /*
    // for enemy that coming back
    int count = 0;
    vector<Point> PotentialTrap;
    for(int k=0; k<MAX_ROBOTS; k++){
        // this enemy coming back & the Y of this robot have a trap
        bool trapFound = CheckTrap(game.traps, Point{1,enemy.robots.at(k).y});
        if(enemy.robots.at(k).x < prevEnemyPos.at(k).x && trapFound && enemy.robots.at(k).distance(Point{1,enemy.robots.at(k).y}) <=5){
            count++;
            PotentialTrap.push_back(Point{1,enemy.robots.at(k).y});
        }
    }
    // if atleast 2 coming back
    if(count >= 2){
        for(int z = 0; z < PotentialTrap.size(); z++){
            vector<Point> linkTraps;
            for(int j = 0; j < game.traps.size(); j++){
                if(game.traps.at(j).distance(PotentialTrap.at(z)) == 0){
                    // find all trap linked to this trap
                    linkTraps.push_back(game.traps.at(j));
                    LinkedTraps(linkTraps, game.traps, game.traps.at(j));
                    cerr << "linked:" << linkTraps.size() << "\n";
                }
            }
            int index = -1;
            for(int j = 0; j < linkTraps.size(); j++){
                int trapCount = 0;
                for(int i = 0; i < PotentialTrap.size(); i++){
                    // if this linktraps contain two potential trap
                    if(linkTraps.at(j).distance(PotentialTrap.at(i)) == 0){
                        trapCount++;
                    }
                }
                if(trapCount >= 2){
                    index = j;
                    break;
                }
            }
            // ther is point worth going back
            if(index != -1){
                bool someoneGoing = false;
                for(int i = 0; i < MAX_ROBOTS; i++){
                    if(actions.at(i).message == "OTW KAMIKAZE"){
                        someoneGoing = true;
                        break;
                    }
                }
                if(!someoneGoing){
                    for(int i = 0; i < MAX_ROBOTS; i++){
                        if(me.robots.at(i).item != Type::RADAR){
                            for(int j = 0; j < linkTraps.size(); j++){
                                if(me.robots.at(i).distance(linkTraps.at(j)) <=5){
                                    actions.at(i).move(linkTraps.at(j), "OTW KAMIKAZE");
                                    goto KAMIKAZE;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    KAMIKAZE:
        cerr<< "test";
    */
    // rigged 1 two turn
    for(int j = 0; j < MAX_ROBOTS; j++){
        if(game.turn == 1){
            if(actions.at(j).type != ActionType::REQUEST && actions.at(j).message != "Gimme HOLE"){
                // check worth explosion or not
                actions.at(j).wait("Dreaming..");
            }
        }
        if(game.turn == 2){
            if(me.robots.at(j).x == 0 && me.robots.at(j).item == Type::NONE){
                // check worth explosion or not
                int chance = 1 + (rand() % 100);
                if(chance < 50);
                    actions.at(j).dig(Point{1, me.robots.at(j).y}, "sync attack !");
            }
            if(me.robots.at(j).x == 0 && me.robots.at(j).item == Type::TRAP){
                // check worth explosion or not
                actions.at(j).dig(Point{1, me.robots.at(j).y}, "sync attack !");
            }
        }
    }
    
    // check any robot going wrong way, use default dig point
    for(int j = 0; j < MAX_ROBOTS; j++){
        if((me.robots.at(j).item == Type::RADAR||me.robots.at(j).item == Type::TRAP) && 
            actions.at(j).type == ActionType::DIG &&
            actions.at(j).dest.x == 0){
            Point newPoint = NewDigPoint(game, me, actions, j, highestScore);
            actions.at(j).dig(newPoint, "OTW Left4Dead");
        }
        
        // update enemy position
        prevEnemyPos.at(j) = Point({enemy.robots.at(j).x, enemy.robots.at(j).y});
        prevOurPos.at(j) = Point({me.robots.at(j).x, me.robots.at(j).y});
    }
    
    prevActions = actions;
    // end smart code
    return actions;
}

//*********************************  MAIN  *****************************************************************

int main() {
    Game game;

    // weighted graph
    for(int x=1; x < MAX_WIDTH; x++){
        for(int y=0; y < MAX_HEIGHT; y++){
            int score = 0;
            
            for(int _x = x-4; _x <= x+4; _x++){
                for(int _y = y-4; _y <= y+4; _y++){
                    if(_x < 1 || _x >= MAX_WIDTH) continue;
                    if(_y < 0 || _y >= MAX_HEIGHT) continue;
                    // for distance smaller than 5 valid point
                    if(Point({x,y}).distance(Point({_x,_y})) < 5){
                        // no hole score
                        score += 4 - Point({x,y}).distance(Point({_x,_y}));
                    }
                }
            }
                        
            game.grid.at(x).at(y).score = score;
            //cerr << score << ",";
        }
        //cerr << "\n";
    }
    
    // global inputs
    int width;
    int height;  // size of the map
    cin >> width >> height;
    cin.ignore();

    // game loop
    while (1) {
        
        game.reset();
        // first loop local inputs
        int myOre;
        int enemyOre;
        cin >> myOre >> enemyOre;
        cin.ignore();
        game.updateOre(0, myOre);
        game.updateOre(1, enemyOre);

        // other loop local inputs
        for (int i = 0; i < height; i++) {
            for (int j = 0; j < width; j++) {
                string ore;  // amount of ore or "?" if unknown
                int hole;           // 1 if cell has a hole
                cin >> ore >> hole;
                cin.ignore();
                game.updateCell(j, i, ore, hole);
            }
        }
        int entityCount;    // number of visible entities
        int radarCooldown;  // turns left until a new radar can be requested
        int trapCooldown;   // turns left until a new trap can be requested
        cin >> entityCount >> radarCooldown >> trapCooldown;
        cin.ignore();
        game.updateCooldown(0, radarCooldown, trapCooldown);
        for (int i = 0; i < entityCount; i++) {
            int id;    // unique id of the entity
            int type;  // 0 for your robot, 1 for other robot, 2 for radar, 3 for trap
            int x;
            int y;     // position of the entity
            int item;  // if this entity is a robot, the item it is carrying (-1 for NONE, 2 for RADAR, 3 for TRAP, 4 for ORE)
            cin >> id >> type >> x >> y >> item;
            cin.ignore();
            game.updateEntity(id, type, x, y, item);
        }

        // AI ------------------------------------------------------------------
        auto actions{getActions(game)};
        // AI ------------------------------------------------------------------

        for (const Action& action : actions) {
            // Write an action using cout. DON'T FORGET THE "<< endl"
            // To debug: cerr << "Debug messages..." << endl;

            cout << action << "\n";  // WAIT|MOVE x y|REQUEST item
        }
    }
}
