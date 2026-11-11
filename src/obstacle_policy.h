#pragma once
#include <array>
#include <vector>
#include <limits>
#include <cstdint>
#include <queue>
#include <functional>
#include <utility>
// Eleven observations span ten native 10-day turns. No forecast is inferred.
struct ObstacleHistory {
    unsigned short hex[11], goal;
    int day[11], range[11], progress;
    unsigned count;
};
inline void obstacle_sample(ObstacleHistory &h,int day,unsigned hex,unsigned goal,int range,int progress) {
    if(h.count && (h.goal!=goal || h.progress!=progress || day<h.day[h.count-1] ||
                  day-h.day[h.count-1]>10))h.count=0;
    h.goal=static_cast<unsigned short>(goal);h.progress=progress;
    if(h.count && day-h.day[h.count-1]<10)return;
    if(h.count==11) {
        for(unsigned i=1;i<11;++i) {h.hex[i-1]=h.hex[i];h.day[i-1]=h.day[i];h.range[i-1]=h.range[i];}
        --h.count;
    }
    unsigned i=h.count++;h.hex[i]=static_cast<unsigned short>(hex);h.day[i]=day;h.range[i]=range;
}
inline bool obstacle_oscillating(const ObstacleHistory &h) {
    if(h.count!=11 || h.day[10]-h.day[0]<100 || h.range[0]-h.range[10]>2)return false;
    unsigned reversals=0,revisits=0,moves=0;int previous=0,lo=h.range[0],hi=lo;
    for(unsigned i=1;i<11;++i) {
        if(h.range[i]<lo)lo=h.range[i];if(h.range[i]>hi)hi=h.range[i];
        int change=h.range[i]-h.range[i-1],sign=(change>0)-(change<0);
        if(sign) {if(previous && sign!=previous)++reversals;previous=sign;++moves;}
        for(unsigned j=0;j+1<i;++j)if(h.hex[i]==h.hex[j]) {++revisits;break;}
    }
    return moves>=4 && reversals>=3 && revisits>=3 && hi-lo<=12;
}
namespace wall_path {
constexpr unsigned width=220, cells=220u*220u;
constexpr unsigned max_city=64;
// A provides: unsigned base(unsigned area) (0 = none), unsigned neighbour(unsigned area,unsigned i) (i<8, 0 = none).
// The area itself or one of its eight neighbours belongs to `home` (PW15: shared by
// wall_cities and the path_wall waypoint choice).
template<class A> bool serves(unsigned area,unsigned home,A &areas) {
    if(!area || !home || home>=max_city)return false;
    if(areas.base(area)==home)return true;
    for(unsigned k=0;k<8;++k) {
        unsigned n=areas.neighbour(area,k);
        if(n && areas.base(n)==home)return true;
    }
    return false;
}
template<class A> std::array<bool,max_city> wall_cities(const std::vector<unsigned> &wall_areas,A &areas) {
    std::array<bool,max_city> flagged{};
    for(unsigned i=0;i<wall_areas.size();++i) {
        unsigned w=wall_areas[i];
        if(!w)continue;
        for(unsigned home=1;home<max_city;++home)if(serves(w,home,areas))flagged[home]=true;
    }
    return flagged;
}
// Column-offset hex grid identical to src/progress_geometry.h.
inline int distance(unsigned a,unsigned b) {
    int ax=static_cast<int>(a%width),ay=static_cast<int>(a/width);
    int bx=static_cast<int>(b%width),by=static_cast<int>(b/width);
    int ar=2*ay+((ax&1)==0),br=2*by+((bx&1)==0);
    int dx=bx-ax,dy=br-ar;
    if(dx<0)dx=-dx;if(dy<0)dy=-dy;
    return dy>dx?dx+((dy-dx)>>1):dx;
}
// PW15: the neutral wall every fifth army walks to. Minimum detour
// d(from,w)+d(w,goal); ties favor the wall nearer the army, then the lower
// index. -1 when no wall is available.
inline int pick_waypoint(const std::vector<unsigned> &wall_hexes,unsigned from,unsigned goal) {
    int best=-1,sum=0,nearest=0;
    for(size_t i=0;i<wall_hexes.size();++i) {
        unsigned w=wall_hexes[i];
        if(w>=cells)continue;
        int a=distance(from,w),b=distance(w,goal);
        if(best<0 || a+b<sum || (a+b==sum && a<nearest)) {
            best=static_cast<int>(i);sum=a+b;nearest=a;
        }
    }
    return best;
}
inline unsigned neighbours(unsigned h,unsigned out[6]) {
    unsigned count=0;
    int x=static_cast<int>(h%width),y=static_cast<int>(h/width);
    for(int cx=x-1;cx<=x+1;++cx) {
        if(cx<0 || cx>=static_cast<int>(width))continue;
        for(int cy=y-2;cy<=y+2;++cy) {
            if(cy<0 || cy>=static_cast<int>(width))continue;
            unsigned c=static_cast<unsigned>(cy)*width+static_cast<unsigned>(cx);
            if(c==h || distance(h,c)!=1)continue;
            if(count<6)out[count]=c;
            ++count;
        }
    }
    return count>6?6:count;
}
// User 2026-11-09 (PW14): every fifth army an origin city sends searches once
// for the wall standing on its all-open route; the other four detour instead.
// Counted per origin city over the last 120 game days, memory only (a new game
// or load clears it). An army keeps the rank it got on registration, so its
// 10-day re-check cannot flip between detour and one search.
class NthCounter {
public:
    static constexpr int window_days=120;
    static constexpr unsigned period=5; // user 2026-11-09: every fifth army
    static constexpr unsigned max_entries=64;
    static constexpr unsigned max_origins=512;
    // Returns the army's 1-based registration rank at its origin city. The
    // value is stable for an already-registered army; entries older than
    // window_days are dropped first and a new army takes the next rank.
    unsigned rank(unsigned origin,unsigned army_key,int day) {
        Origin *o=find(origin);
        if(!o)o=create(origin);
        unsigned live=0;
        for(unsigned i=0;i<o->count;++i) {
            if(day-o->entries[i].day>window_days)continue;
            o->entries[live++]=o->entries[i];
        }
        o->count=live;
        for(unsigned i=0;i<o->count;++i)
            if(o->entries[i].key==army_key)return o->entries[i].rank;
        if(o->count>=max_entries) {
            for(unsigned i=1;i<o->count;++i)o->entries[i-1]=o->entries[i];
            --o->count;
        }
        unsigned assigned=o->count+1;
        o->entries[o->count].key=army_key;
        o->entries[o->count].day=day;
        o->entries[o->count].rank=assigned;
        ++o->count;
        return assigned;
    }
    void clear() {origin_count=0;}
private:
    struct Entry {unsigned key;int day;unsigned rank;};
    struct Origin {unsigned id;unsigned count;Entry entries[max_entries];};
    Origin *find(unsigned origin) {
        for(unsigned i=0;i<origin_count;++i)if(origins[i].id==origin)return &origins[i];
        return nullptr;
    }
    Origin *create(unsigned origin) {
        unsigned index=origin_count;
        if(index>=max_origins) {
            // Full: drop the origin that registered an army least recently.
            index=0;
            int oldest=origins[0].entries[origins[0].count-1].day;
            for(unsigned i=1;i<origin_count;++i) {
                int day=origins[i].entries[origins[i].count-1].day;
                if(day<oldest) {oldest=day;index=i;}
            }
        } else {
            ++origin_count;
        }
        Origin *o=&origins[index];
        o->id=origin;o->count=0;
        return o;
    }
    Origin origins[max_origins]{};
    unsigned origin_count=0;
};
}
// PW22 (user 2026-11-09): a path_wall demolition order is a lock. idle -> locked(id)
// while the neutral building stands; done once it falls (or the army disappears), after
// which the original AI decides. The transitions are pure so the fixture drives them.
namespace wall_lock {
enum Status { idle=0, locked=1, done=2 };
enum Release { none=0, destroyed=1, changed=2, army_gone=3, feature_off=4 };
// `same_building` means the live building still matches the recorded hex/type and has no
// owner; `durability` is word(building,0x14).
inline unsigned release_reason(bool feature,bool army_alive,bool valid_building,bool same_building,unsigned durability) {
    if(!feature)return feature_off;
    if(!army_alive)return army_gone;
    if(!valid_building || durability==0)return destroyed;
    if(!same_building)return changed;
    return none;
}
// The locked army re-issues once a game day only when its native order is no longer
// action 6 / kind 11 / the locked building and that building is still standing.
inline bool reissue(Status status,unsigned action,unsigned kind,unsigned target,unsigned lock_id,bool standing) {
    return status==locked && standing && !(action==6 && kind==11 && target==lock_id);
}
inline Status after_release(Status status,unsigned reason) {
    if(reason==none)return status;
    return reason==feature_off?idle:done;
}
inline bool holds(Status status) {return status==locked || status==done;}
}

// neutral_breach (user 2026-11-10 "중립건물 존재 x판정으로 계산하고 최단거리 막고 있으면 뚫기", docs/COMBAT_REDESIGN.md 5절).
// Pure decision on the 220x220 hex grid, no game access. walk[hex]: 0 blocked (terrain or a class 2/3 facility, which the
// original path search skips whatever its owner), 1 open, 2 neutral wall (토성/석벽/철쇄, owner 0). The goal (a city or gate)
// is reached on the goal hex or next to it. closed = cost with walls as they are; open = cost when a neutral wall may be
// crossed for `penalty` extra steps (its demolition time). Breach when the open route is cheaper (or the closed one is
// impossible); wall_hex = the first neutral wall on the open route, seen from the start.
namespace neutral_breach {
constexpr unsigned none=0xFFFFFFFFu;
enum : unsigned char {blocked=0,open_cell=1,wall=2};
struct Plan {unsigned closed=none,open=none,wall_hex=none,free=none,unit=1;};
// weight (local38b): per-cell terrain movement cost (the original BF1B0, about 240 on plain); null = 1 per hex.
inline unsigned search(const unsigned char *walk,unsigned start,unsigned goal,unsigned penalty,bool through,unsigned *wall_hex,
                       const unsigned short *weight=nullptr) {
    using wall_path::cells;
    if(wall_hex)*wall_hex=none;
    if(start>=cells || goal>=cells)return none;
    static thread_local std::vector<unsigned> cost,parent;
    cost.assign(cells,none);parent.assign(cells,none);
    using Item=std::pair<unsigned,unsigned>;
    std::priority_queue<Item,std::vector<Item>,std::greater<Item>> open;
    cost[start]=0;open.push({0,start});
    unsigned reached=none;
    while(!open.empty()) {
        const Item top=open.top();open.pop();
        const unsigned c=top.first,h=top.second;
        if(c!=cost[h])continue;
        if(h==goal || wall_path::distance(h,goal)<=1) {reached=h;break;}
        unsigned out[6];const unsigned n=wall_path::neighbours(h,out);
        for(unsigned i=0;i<n;++i) {
            const unsigned next=out[i];
            unsigned step=weight?(weight[next]?weight[next]:1u):1u;
            if(next!=goal) {
                if(walk[next]==blocked)continue;
                if(walk[next]==wall) {if(!through)continue;step+=penalty;}
            }
            if(c+step<cost[next]) {cost[next]=c+step;parent[next]=h;open.push({c+step,next});}
        }
    }
    if(reached==none)return none;
    if(wall_hex)for(unsigned h=reached;h!=none && h!=start;h=parent[h])if(walk[h]==wall)*wall_hex=h;   // last write = nearest the start
    return cost[reached];
}
// unit = the cost of one plain hex in `weight` units (240 with BF1B0 weights, 1 without); penalty is in hexes.
inline Plan plan(const unsigned char *walk,unsigned start,unsigned goal,unsigned penalty,const unsigned short *weight=nullptr,unsigned unit=1) {
    Plan p;p.unit=unit?unit:1;
    p.closed=search(walk,start,goal,penalty*p.unit,false,nullptr,weight);
    p.open=search(walk,start,goal,penalty*p.unit,true,nullptr,weight);   // with the demolition penalty: logged only
    p.free=search(walk,start,goal,0,true,&p.wall_hex,weight);            // as if no neutral wall stood
    return p;
}
// local38 (user U39 "중립건물 없다고 친 최단거리를 막을 때만", 2026-11-10 "이동력을 크게 너프하면 부수기"): breach when the
// wall-free shortest way crosses a neutral wall and the way around it is clearly longer (>= 6 hexes and >= a third
// more) or impossible. The demolition time no longer vetoes it (a 350 chain cost 54..99 penalty steps).
inline bool breach(const Plan &p) {
    if(p.wall_hex==none || p.free==none)return false;
    if(p.closed==none)return true;
    // local55 (user 2026-11-11 "그 기준 조금만 낮춰"): 6 hexes and a third -> 5 hexes and a quarter.
    const unsigned detour=p.free/4>5*p.unit?p.free/4:5*p.unit;
    return p.closed>=p.free+detour;
}
// Demolition days -> extra steps: about three hexes a day, weighted 1.5 so a breach must save clearly more than it costs.
inline unsigned penalty(unsigned durability,unsigned daily_damage) {
    const unsigned days=(durability+(daily_damage?daily_damage:1)-1)/(daily_damage?daily_damage:1);
    return days*3u*3u/2u;
}
}
