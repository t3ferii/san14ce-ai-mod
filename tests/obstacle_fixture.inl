static unsigned wall_hex(unsigned x,unsigned y) {return y*wall_path::width+x;}
// U39 neutral_breach: two-pass Dijkstra (walls closed / a neutral wall crossable at its demolition cost).
static void neutral_breach_policy_tests() {
    static unsigned char walk[wall_path::cells];
    const unsigned from=wall_hex(90,100),to=wall_hex(110,100),cut=wall_hex(100,100);
    // A corridor whose only way runs through the wall: closed = none, the breach is chosen at the wall.
    for(auto &c:walk)c=neutral_breach::blocked;
    for(unsigned x=90;x<=110;++x)walk[wall_hex(x,100)]=neutral_breach::open_cell;
    walk[cut]=neutral_breach::wall;
    neutral_breach::Plan p=neutral_breach::plan(walk,from,to,neutral_breach::penalty(500,100));
    check(p.closed==neutral_breach::none && p.open!=neutral_breach::none,"neutral_breach: a corridor wall blocks every closed way");
    check(neutral_breach::breach(p) && p.wall_hex==cut,"neutral_breach: the blocking wall is breached");
    // Open field with one wall cell: the walk around is shorter than the demolition, so no breach.
    for(auto &c:walk)c=neutral_breach::open_cell;
    walk[cut]=neutral_breach::wall;
    p=neutral_breach::plan(walk,from,to,neutral_breach::penalty(500,100));
    check(p.closed!=neutral_breach::none && !neutral_breach::breach(p),"neutral_breach: a wall with a short way around is left standing");
    // A wall line across the whole map column: only the breach reaches the goal.
    for(unsigned y=0;y<wall_path::width;++y)walk[wall_hex(100,y)]=neutral_breach::wall;
    p=neutral_breach::plan(walk,from,to,neutral_breach::penalty(800,100));
    check(p.closed==neutral_breach::none && neutral_breach::breach(p) && p.wall_hex%wall_path::width==100,"neutral_breach: a full wall line is breached once");
    // A long detour (walls with a gap far away) against a cheap wall: breach when it clearly saves steps.
    for(unsigned y=0;y<wall_path::width;++y)walk[wall_hex(100,y)]=y<200?neutral_breach::wall:neutral_breach::open_cell;
    p=neutral_breach::plan(walk,from,to,neutral_breach::penalty(350,350));
    check(p.closed!=neutral_breach::none && neutral_breach::breach(p),"neutral_breach: a 100-hex detour loses to a one-day demolition");
    // local38: a slow demolition (350 chain, 10 a day) no longer vetoes a long detour.
    {
        std::vector<unsigned char> chain(wall_path::cells,neutral_breach::open_cell);
        for(unsigned y=70;y<=130;++y)chain[wall_hex(100,y)]=neutral_breach::wall;
        const neutral_breach::Plan c=neutral_breach::plan(chain.data(),from,to,neutral_breach::penalty(350,10));
        check(c.open>=c.closed && c.closed>=c.free+6 && neutral_breach::breach(c),"neutral_breach: a long detour breaches a slowly demolished chain");
    }
    {   // local38b: terrain weights. A short wall whose hex detour is small, but the detour crosses slow ground (999 vs 240).
        std::vector<unsigned char> chain(wall_path::cells,neutral_breach::open_cell);
        std::vector<unsigned short> cost(wall_path::cells,240);
        for(unsigned y=98;y<=102;++y)chain[wall_hex(100,y)]=neutral_breach::wall;
        for(unsigned y=80;y<=120;++y)for(unsigned x=94;x<=106;++x)if(y!=100)cost[wall_hex(x,y)]=999;
        const neutral_breach::Plan flat=neutral_breach::plan(chain.data(),from,to,1);
        const neutral_breach::Plan slow=neutral_breach::plan(chain.data(),from,to,1,cost.data(),240);
        check(!neutral_breach::breach(flat) && neutral_breach::breach(slow),"neutral_breach: a detour over slow terrain breaches although it is short in hexes");
    }
    walk[wall_hex(100,101)]=neutral_breach::open_cell;
    p=neutral_breach::plan(walk,from,to,neutral_breach::penalty(350,350));
    check(!neutral_breach::breach(p),"neutral_breach: a gap next to the way removes the breach");
    check(neutral_breach::penalty(500,100)==22 && neutral_breach::penalty(350,0)>neutral_breach::penalty(350,350),"neutral_breach: penalty follows demolition days");
    // Unreachable goal (start boxed in by blocked cells): no plan, no breach.
    for(auto &c:walk)c=neutral_breach::blocked;
    walk[from]=neutral_breach::open_cell;
    p=neutral_breach::plan(walk,from,to,1);
    check(p.closed==neutral_breach::none && p.open==neutral_breach::none && !neutral_breach::breach(p),"neutral_breach: unreachable goal makes no plan");
}// PW15: path_wall no longer searches the grid; these tests cover the shared
// serves() mapping, the waypoint choice and the surviving helpers.
static void wall_path_policy_tests() {
    {
        unsigned out[6];
        unsigned interior=wall_hex(100,100),count=wall_path::neighbours(interior,out);
        check(count==6,"wall_path: an interior cell has six neighbours");
        for(unsigned i=0;i<count;++i)check(wall_path::distance(interior,out[i])==1,"wall_path: neighbours are one hex away");
        unsigned corner=wall_path::neighbours(0,out);
        check(corner==2 || corner==3,"wall_path: a map corner has two or three neighbours");
        for(unsigned i=0;i<corner;++i)check(wall_path::distance(0,out[i])==1,"wall_path: corner neighbours are one hex away");
    }
    {
        struct WallAreasMock {
            unsigned home[12],next[12][8];
            WallAreasMock() {
                for(unsigned i=0;i<12;++i) {home[i]=0;for(unsigned k=0;k<8;++k)next[i][k]=0;}
                home[5]=3;home[6]=4;home[7]=3;home[9]=8;
                next[5][0]=6;next[5][1]=7;
                next[10][0]=11;home[10]=wall_path::max_city+5;home[11]=wall_path::max_city+9;
            }
            unsigned base(unsigned a) const {return a<12?home[a]:0;}
            unsigned neighbour(unsigned a,unsigned i) const {return a<12 && i<8?next[a][i]:0;}
        } areas;
        std::vector<unsigned> areas_with_wall(1,5);
        std::array<bool,wall_path::max_city> flagged=wall_path::wall_cities(areas_with_wall,areas);
        check(flagged[3],"wall cities: the base city of a wall area is flagged");
        check(flagged[4],"wall cities: the base city of an adjacent area is flagged");
        check(!flagged[8],"wall cities: an unrelated city stays unflagged");
        check(!flagged[0],"wall cities: city 0 is never flagged");
        flagged=wall_path::wall_cities(std::vector<unsigned>(),areas);
        bool any=false;
        for(unsigned i=1;i<wall_path::max_city;++i)any=any || flagged[i];
        check(!any,"wall cities: no wall area flags no city");
        areas_with_wall[0]=10;
        flagged=wall_path::wall_cities(areas_with_wall,areas);
        any=false;
        for(unsigned i=1;i<wall_path::max_city;++i)any=any || flagged[i];
        check(!any,"wall cities: a base city above max_city is ignored");
        check(wall_path::serves(5,3,areas),"wall path serves: the area's own base city is served");
        check(wall_path::serves(5,4,areas),"wall path serves: an adjacent area's base city is served");
        check(!wall_path::serves(5,8,areas),"wall path serves: an unrelated city is not served");
        check(!wall_path::serves(0,3,areas) && !wall_path::serves(5,0,areas),
              "wall path serves: area/city 0 is never served");
    }
    {
        const unsigned route_from=wall_hex(10,100),route_goal=wall_hex(40,100);
        check(wall_path::pick_waypoint(std::vector<unsigned>(),route_from,route_goal)==-1,
              "waypoint: no neutral wall yields -1");
        check(wall_path::pick_waypoint({wall_hex(20,100),wall_hex(30,100)},route_from,route_goal)==0,
              "waypoint: equal detours prefer the wall nearer the army");
        check(wall_path::pick_waypoint({wall_hex(30,100),wall_hex(20,100)},route_from,route_goal)==1,
              "waypoint: the smaller distance from the army wins over the index");
        check(wall_path::pick_waypoint({wall_hex(25,101),wall_hex(25,99)},route_from,route_goal)==0,
              "waypoint: a full tie keeps the lower index");
        check(wall_path::pick_waypoint({wall_hex(25,120),wall_hex(20,100)},route_from,route_goal)==1,
              "waypoint: a straight wall beats a detouring one");
    }
    {   // local226 tower_reach: wall / facility stands outside hostile 망루 (1..2) and 투석대 (2..3) range
        using tower_reach::Tower;
        const unsigned target=wall_hex(25,100),from=wall_hex(10,100),none=wall_path::cells;
        auto any=[](unsigned){return true;};
        const std::vector<Tower> no_towers;
        unsigned s=tower_reach::stand(target,from,1,no_towers,any);
        check(s<none && wall_path::distance(s,target)==1 && wall_path::distance(from,s)==wall_path::distance(from,target)-1,
              "tower reach: without towers the melee stand is the ring hex nearest the army");
        check(tower_reach::stand(target,wall_hex(26,100),1,no_towers,any)==wall_hex(26,100),
              "tower reach: an army already next to the wall stays where it is");
        const std::vector<Tower> over={{target,1,2}};
        check(tower_reach::stand(target,from,2,over,any)==none,
              "tower reach: every stand under a watchtower's fire - no stand (the wall is left alone)");
        const std::vector<Tower> catapult={{target,2,3}};
        s=tower_reach::stand(target,from,1,catapult,any);
        check(s<none && wall_path::distance(s,target)==1,"tower reach: a catapult does not hit its own neighbours (range 2..3)");
        const std::vector<Tower> west={{wall_hex(23,100),1,2}};
        s=tower_reach::stand(target,from,1,west,any);
        check(s<none && !tower_reach::covered(s,west) && wall_path::distance(s,target)==1,
              "tower reach: the near side under a watchtower gives way to a safe far-side stand");
        const unsigned only=wall_hex(25,102);
        check(tower_reach::stand(target,from,2,no_towers,[&](unsigned h){return h==only;})==only,
              "tower reach: only hexes the army may stand on are chosen");
        check(tower_reach::stand(target,from,1,no_towers,[](unsigned){return false;})==none,
              "tower reach: no open hex - no stand");
    }
}
static void nth_counter_tests() {
    static wall_path::NthCounter counter;
    counter.clear();
    unsigned ranks[10];
    for(unsigned i=0;i<10;++i)ranks[i]=counter.rank(7,1000+i,100);
    bool ordered=true;
    for(unsigned i=0;i<10;++i)if(ranks[i]!=i+1)ordered=false;
    check(ordered,"fifth counter: ten armies of one origin rank 1..10");
    unsigned searching=0;
    for(unsigned i=0;i<10;++i)if(ranks[i]%wall_path::NthCounter::period==0)++searching;
    check(searching==2 && ranks[4]%wall_path::NthCounter::period==0 && ranks[9]%wall_path::NthCounter::period==0,
          "fifth counter: only the 5th and 10th armies search");
    check(counter.rank(7,1002,110)==3,"fifth counter: an army keeps its rank on a later check");
    check(counter.rank(8,2000,100)==1,"fifth counter: another origin counts separately");
    check(counter.rank(7,3000,221)==1,"fifth counter: entries older than 120 days are dropped before ranking");
    check(counter.rank(7,3000,230)==1,"fifth counter: a new army keeps the rank it got after the window dropped old entries");
    counter.clear();
    check(counter.rank(7,1000,100)==1,"fifth counter: clear() resets every origin");
}
// PW22: idle -> locked(id) -> done, the release reasons and the daily reissue rule.
static void wall_lock_tests() {
    using namespace wall_lock;
    check(!holds(idle) && holds(locked) && holds(done),
          "wall lock: only locked and done hold the army");
    check(reissue(idle,1,9,5,5,true)==false,"wall lock: an idle army never reissues");
    check(reissue(locked,1,9,5,5,true),"wall lock: a diverted locked order is reissued");
    check(reissue(locked,6,11,5,7,true),"wall lock: a different demolition target is reissued");
    check(!reissue(locked,6,11,5,5,true),"wall lock: the correct demolition order is kept");
    check(!reissue(locked,6,11,5,5,false),"wall lock: a fallen building is never reissued");
    check(release_reason(true,true,true,true,100)==none,"wall lock: a standing matching building keeps the lock");
    check(release_reason(true,true,true,true,0)==destroyed,"wall lock: zero durability releases as destroyed");
    check(release_reason(true,true,false,false,0)==destroyed,"wall lock: a missing building releases as destroyed");
    check(release_reason(true,true,true,false,100)==changed,"wall lock: a moved/retyped/owned building releases as changed");
    check(release_reason(true,false,true,true,100)==army_gone,"wall lock: a lost army releases");
    check(release_reason(false,true,true,true,100)==feature_off,"wall lock: the feature switch releases");
    check(after_release(locked,none)==locked,"wall lock: no release reason keeps the lock");
    check(after_release(locked,destroyed)==done && after_release(locked,army_gone)==done &&
          after_release(locked,changed)==done,"wall lock: a fallen or lost mission ends as done");
    check(after_release(locked,feature_off)==idle,"wall lock: switching the feature off returns to idle");
}
// docs/NEUTRAL_WALL_AGE.md 8절: records, score and ties, 6-hex candidates, only_wall, siege round robin and cap, sidecar codec,
// the chase-hold predicate.
#include "../src/wall_age_shared.h"
static void wall_age_policy_tests() {
    using namespace wall_age;
    static WallAgeRecord rec[kWallAgeIds];
    for(auto &r:rec)r=WallAgeRecord{};
    const unsigned a=wall_hex(50,50),b=wall_hex(52,50),c=wall_hex(60,60);
    Live day0[]={{100,a,9},{101,b,10},{102,c,16},{40,a,9},{103,b,5}};   // id 40 and kind 5 are no neutral walls of the table
    check(reconcile(rec,-1,day0,5,100)==3 && rec[100].kind==9 && rec[100].born==100 && rec[101].kind==10 && rec[102].kind==16,"wall_age: new walls start on the first day seen");
    check(!rec[40].kind && !rec[103].kind,"wall_age: ids below 62 and other kinds are not recorded");
    count(rec[100],a,9,105);count(rec[100],a,9,106);
    Live day1[]={{100,a,9},{101,b,9},{102,c,16}};                        // 101 rebuilt as another kind
    check(reconcile(rec,100,day1,3,130)==1 && rec[100].born==100 && rec[100].count==2,"wall_age: the same wall keeps age and count");
    check(rec[101].kind==9 && rec[101].born==130 && rec[101].count==0,"wall_age: another wall on the id starts again");
    Live day2[]={{100,a,9},{101,b,9}};
    check(reconcile(rec,130,day2,2,140)==0 && !rec[102].kind,"wall_age: a lost wall is cleared");
    check(reconcile(rec,140,day2,2,90)==2 && rec[100].born==90 && rec[100].count==0,"wall_age: an earlier date (new game, old save) clears everything first");
    // age and score
    WallAgeRecord r{};start(r,a,9,0);
    check(age(r,1089)==108 && age(r,1090)==109 && !siege_target(age(r,1089)) && siege_target(age(r,1090)) && !siege_target(age(r,370)),"wall_age: older than 108 순 (3 years) = siege target");
    r.count=5;check(score(r,370)==32,"wall_age: score = age - count");
    WallAgeRecord fresh{};count(fresh,a,9,200);
    check(fresh.kind==9 && fresh.count==1 && fresh.born==200,"wall_age: a wall counted before its first reconcile starts today with count 1");
    // the pick is not idempotent: once counted, the same wall scores lower the next day (an assigned army keeps its wall:
    // route_hook never re-picks while the entry lives)
    WallAgeRecord x{},y{};start(x,a,9,0);start(y,b,9,0);x.count=0;y.count=0;
    std::vector<Candidate> two={{100,a,score(x,100),0},{101,b,score(y,100),2}};
    check(pick(two,100)==0,"wall_age: equal score keeps the original wall");
    count(x,a,9,100);
    two[0].score=score(x,101);two[1].score=score(y,101);
    check(pick(two,100)==1,"wall_age: after its count the next order goes to the other wall");
    // ties after the original: nearer, then the lower id
    std::vector<Candidate> ties={{100,a,1,0},{105,b,3,3},{104,c,3,3},{103,c,3,2}};
    check(pick(ties,100)==3,"wall_age: equal best score - nearer to the original first");
    ties[3].range=3;check(pick(ties,100)==3,"wall_age: equal range - lower id");
    check(pick(std::vector<Candidate>{},100)==-1,"wall_age: no candidate");
    // 6-hex candidates, the original first, 8 at most
    std::vector<Live> ring;
    for(unsigned k=0;k<12;++k)ring.push_back({200+k,wall_hex(70+k,80),9});
    ring.push_back({300,wall_hex(70,95),10});                            // 15 rows away
    const std::vector<Candidate> cand=nearby(205,wall_hex(75,80),ring.data(),ring.size());
    bool inside=true;for(const Candidate &k:cand)inside=inside && k.range<=near_hexes && k.id!=300;
    check(cand.size()==max_candidates && cand[0].id==205 && cand[0].range==0 && inside,"wall_age: up to 8 walls within 6 hexes, the original first");
    check(cand[1].range==1 && cand[2].range==1 && cand[1].id<cand[2].id,"wall_age: nearest first, then the lower id");
    // only_wall: a second wall line where only one cell is a wall in a corridor
    {
        std::vector<unsigned char> walk(wall_path::cells,neutral_breach::blocked);
        const unsigned from=wall_hex(90,100),to=wall_hex(110,100);
        for(unsigned x0=90;x0<=110;++x0)walk[wall_hex(x0,100)]=neutral_breach::open_cell;
        for(unsigned x0=90;x0<=110;++x0)walk[wall_hex(x0,104)]=neutral_breach::open_cell;   // a parallel corridor
        for(unsigned y0=100;y0<=104;++y0)walk[wall_hex(110,y0)]=neutral_breach::open_cell;
        for(unsigned y0=100;y0<=104;++y0)walk[wall_hex(90,y0)]=neutral_breach::open_cell;
        const unsigned w1=wall_hex(100,100),w2=wall_hex(100,104);
        walk[w1]=neutral_breach::wall;walk[w2]=neutral_breach::wall;    // both corridors walled: closed = none
        const neutral_breach::Plan pl=neutral_breach::plan(walk.data(),from,to,0);
        check(pl.closed==neutral_breach::none && neutral_breach::breach(pl),"wall_age: two walled corridors, closed = none");
        const unsigned alt=neutral_breach::search(walk.data(),from,to,0,false,nullptr,nullptr,nullptr,w2);
        check(alt!=neutral_breach::none && opens(pl.closed,alt,1),"wall_age: the other corridor's wall alone opens the way");
        const unsigned off=neutral_breach::search(walk.data(),from,to,0,false,nullptr,nullptr,nullptr,wall_hex(100,102));
        check(off==neutral_breach::none && !opens(pl.closed,off,1),"wall_age: a cell that is no wall on the way opens nothing");
        walk[w1]=neutral_breach::open_cell;                               // the first corridor is open: closed exists and is short
        const unsigned closed=neutral_breach::search(walk.data(),from,to,0,false,nullptr);
        const unsigned via=neutral_breach::search(walk.data(),from,to,0,false,nullptr,nullptr,nullptr,w2);
        check(closed!=neutral_breach::none && !opens(closed,via,1),"wall_age: a wall that saves no detour is not a shortcut");
    }
    // siege round robin and the cap of ten
    std::vector<SiegeTarget> t={{100,a,40,0,a},{101,b,50,0,b},{102,c,50,1,c}};
    check(siege_pick(t)==1,"wall_age siege: fewest units first, then the higher score");
    t[1].assigned=1;check(siege_pick(t)==0,"wall_age siege: round robin moves on");
    t[0].assigned=1;check(siege_pick(t)==1,"wall_age siege: equal counts and scores - the lower id");
    std::vector<SiegeTarget> one={{100,a,40,0,a}};unsigned out=0;
    while(siege_room(out) && siege_pick(one)==0) {++one[0].assigned;++out;}
    check(out==siege_cap && one[0].assigned==10,"wall_age siege: one target takes all ten, never more");
    // sidecar codec: round trip, binding mismatch, truncated
    {
        static WallAgeRecord src[kWallAgeIds],dst[kWallAgeIds];
        for(auto &q:src)q=WallAgeRecord{};
        start(src[62],a,9,10);src[62].count=3;start(src[3000],c,16,-5);
        uint8_t bind[32],other[32];for(unsigned k=0;k<32;++k) {bind[k]=static_cast<uint8_t>(k);other[k]=static_cast<uint8_t>(k^1);}
        const std::vector<uint8_t> bytes=wall_age_codec::encode(bind,777,src,kWallAgeIds);
        int32_t last=-1;
        check(bytes.size()==wall_age_codec::kHead+2*wall_age_codec::kEntry && !memcmp(bytes.data(),"WAGE",4),"wall_age codec: head and two entries");
        check(wall_age_codec::decode(bytes.data(),bytes.size(),bind,last,dst,kWallAgeIds) && last==777 && dst[62].kind==9 && dst[62].count==3 &&
              dst[62].born==10 && dst[3000].kind==16 && dst[3000].born==-5 && !dst[63].kind,"wall_age codec: round trip");
        last=-1;
        check(!wall_age_codec::decode(bytes.data(),bytes.size(),other,last,dst,kWallAgeIds) && last==-1 && !dst[62].kind,"wall_age codec: another save's binding is refused");
        check(!wall_age_codec::decode(bytes.data(),bytes.size()-1,bind,last,dst,kWallAgeIds),"wall_age codec: a cut file is refused");
    }
    // the chase hold: only a mod wall breaker, while its wall stands, not on the way home, with the switch on
    check(hold_chase(true,true,true,false),"wall_hold_chase: a wall breaker does not chase");
    check(!hold_chase(false,true,true,false) && !hold_chase(true,false,true,false) && !hold_chase(true,true,false,false) && !hold_chase(true,true,true,true),
          "wall_hold_chase: switch off, no wall, fallen wall or going home = 저돌 / engage as the original");
}
static void obstacle_policy_tests() {
    wall_path_policy_tests();
    neutral_breach_policy_tests();
    nth_counter_tests();
    wall_lock_tests();
    wall_age_policy_tests();
    ObstacleHistory h={};
    for(unsigned i=0;i<11;++i) {
        obstacle_sample(h,100+static_cast<int>(i)*10,101+i%2,200,99-static_cast<int>(i%2),100);
        check(obstacle_oscillating(h)==(i==10),"oscillation requires ten complete game turns");
    }
    unsigned count=h.count;obstacle_sample(h,200,102,200,98,100);
    check(h.count==count && h.hex[10]==101,"repeated AI callbacks in one turn cannot accelerate history");
    obstacle_sample(h,210,101,201,100,100);check(h.count==1,"new objective resets old movement history");
    obstacle_sample(h,220,102,201,99,220);check(h.count==1,"actual capture or construction progress resets history");
    obstacle_sample(h,230,101,201,100,220);obstacle_sample(h,210,102,201,99,220);
    check(h.count==1,"loading an earlier date resets motion history");
    obstacle_sample(h,240,101,201,100,220);check(h.count==1,"unobserved intervening turns cannot be guessed as oscillation");
    for(unsigned pattern=0;pattern<5;++pattern) {
        h={};
        for(unsigned i=0;i<11;++i) {
            unsigned hex=pattern==0?101:pattern==1?101+i:pattern==2?120-i:pattern==3?101+i%2:101+i;
            int range=pattern==0?99:pattern==1?99-static_cast<int>(i):pattern==2?80+static_cast<int>(i):pattern==3?99-static_cast<int>(i%2):99-static_cast<int>(i%2);
            obstacle_sample(h,100+static_cast<int>(i)*10,hex,200,range,100);
        }
        check(obstacle_oscillating(h)==(pattern==3),"stationary, normal advance, withdrawal and non-repeating detours do not trigger demolition");
    }
}
