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
static void obstacle_policy_tests() {
    wall_path_policy_tests();
    neutral_breach_policy_tests();
    nth_counter_tests();
    wall_lock_tests();
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
