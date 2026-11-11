// [personnel] local57 officer_rotation (user 2026-11-11), computer forces only (the player's force, its direct and delegated
// corps, is never touched), off = original.
// "해당 세력 통솔순으로 무장들을 나열하고 통솔이 높은 무장은 공성출진/전선으로 이동하고 전선에 있는 통솔이 낮은 무장들은
//  후방으로 ... 한번에 이동시키지 말고 매월 상순마다 조금씩 맞교환" / "군단장이나 군주가 한성에 모여 버리면 군단 설정이
//  풀리거든? ... 군단장을 다른사람주고 이동하도록".
// Original: the only force-wide officer distribution is CForceCommandMoveNode (55310, corps tree N774, first-order corps,
// condition-116 gated). It balances officer COUNTS: weight 8 for cities in the transport-destination list 1 (front
// footholds), 5 for the others, 1 for gates; it moves up to three batches a run from the most over-staffed city to the most
// under-staffed one with the native move command (1AC60 list, 18D50 slot, 1F2630 issue, 1BFFF0 free), takes idle officers
// only (home == location == city, 233BE0/234AB0 idle, not commanding an army, +0x15E bits 0 and 14 clear), never the lord
// or the source corps' governor unless the target is in the same corps, and charges the source corps 1 order
// (DAT 1A4B4D0) per batch. Stats play no part.
// Here, once a month on the first domestic tick of 상순 (day 1..10): per computer force, idle officers in front cities
// (a city owning a region that borders a region of another regular force) sorted by 통솔 ascending and idle officers in rear
// cities sorted by 통솔 descending are paired; while the rear officer leads by >= kGap, the two swap cities with two native
// move commands (head counts stay as they are, so the original count balancing above is not disturbed). At most kPairs
// pairs per force per month. The lord never moves. A corps governor who would leave his corps (or meet the lord or another
// governor) first hands the corps to its highest-통솔 idle officer, as the native corps edit (73ED40) does: corps +0x12 =
// new governor, status (+0xE6) new 2 / old 4, both cities refreshed (284520). Each move costs the source corps the
// original move cost while it has 명령서 left (a corps with none still swaps; the count never goes below 0).
#include "game_addresses.h"
#include <algorithm>
#include <unordered_map>
#include <vector>
namespace officer_rotation {
// local57g/h "친애 끌어당김은 게임시작하고 바로 전선으로 모두적용 ... 1달 상순마다 4쌍씩 ... 통솔이 낮아도 친애면 전선에서
// 뺄 목록에 제외", "처음적용하는 친애는 시작용으로 계산을 ... 달리", "한쪽 세력을 멸망시켜서 더이상 전선이 아니게 되면 ...
// 빠르게 이동", "친애 vs 통솔 2등 어떻게 판단" -> "이대로 해줘".
// San14 has no deputies: 친애 pays off as adjacent units joining combos. A front city is valued by the units it can field:
// N = troops (city +0x3C) / kUnitTroops in [kMinUnits, kMaxUnits] (the floor covers troops transported in before a campaign); score = 통솔 sum of its N strongest officers (any state,
// lord included) + kBond per 친애 pair (234880 either way) among them + kSupportBonus when one of them has an ally-area trait.
// So a 친애 officer beats a stranger whose 통솔 is less than kBond higher. Protected at the front (never sent to the rear):
// ally-area trait holders and officers in 친애 with an anchor (통솔 >= kAnchorLead) of their city.
//  monthly (상순): up to kPairs swaps per force, each the rear-front pair with the largest score gain (>= kGap).
//  new game (first tick, 순 counter G+0x454 <= kStartTurns): front cities by threat (hostile border regions), round-robin: each
//    city takes its best addition (rear officers or spares outside another front city's N) until nothing gains; one-way while
//    the city has fewer officers than N, otherwise a swap with its weakest unprotected officer. Up to kStartMoves per force.
//  front lost (a neighbour fell): for kDrainMonths, up to kDrainPerCity idle officers a month leave such a city one way for the
//    front city they add most to, keeping kKeepRear officers there.
static constexpr unsigned kPairs=4,kStartMoves=200,kStartTurns=2,kGap=10,kMaxCity=51,kAnchorLead=70;
static constexpr unsigned kUnitTroops=5000,kMinUnits=5,kMaxUnits=10,kDrainPerCity=4,kKeepRear=3;
static constexpr int kBond=10,kSupportBonus=8,kDrainMonths=6;
// Ally-area traits (condition 9 = own-force units in range, 21 = own male units): 효장 용장 강장 독장 구심 악주 명경 독려 응원.
static constexpr unsigned kSupport[]={10,11,12,13,21,23,45,46,96};
static constexpr int kCooldown=3;   // months a swapped officer is not picked again (no monthly back-and-forth)
static constexpr int kHeld=12;      // the original AI sent him elsewhere again (it wants him there): leave him for a year
static int done_month=-1;
static bool prev_front[kMaxCity+1],prev_valid;   // last month's front cities (front-lost drain)
static int drain_left[kMaxCity+1];               // months a front-lost city keeps sending officers forward
static Obj done_world=nullptr;
// Officers this module moved: id, target city, month, held. Reset with the world (new game / load).
struct Moved {unsigned short id,target;int month;bool held;};
static std::vector<Moved> moved;
static int span(const Moved &m) {return m.held?kHeld:kCooldown;}
static bool cooling(Obj p,int month) {
    for(const Moved &m:moved)if(m.id==word(p,0x10) && month-m.month<span(m))return true;
    return false;
}
static Obj city_of(unsigned id) {return id && id<=kMaxCity?fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_32E3B0))(world(),id):nullptr;}
static unsigned force_of_city(Obj city) {
    Obj f=valid(city)?fn<Obj(__fastcall *)(Obj)>(GA(GA_22E3E0))(city):nullptr;
    return valid(f)?(*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(f,0)+0x60))(f):0;
}
static Obj corps_of_city(Obj city) {
    if(!valid(city))return nullptr;
    Obj c=(*reinterpret_cast<Obj(__fastcall **)(Obj)>(pointer(city,0)+0x80))(city);
    return valid(c)?c:nullptr;
}
static Obj corps_at(unsigned index) {Obj c=index<0x34?table(0xE0D0,index,51):nullptr;return valid(c)?c:nullptr;}
static int lead(Obj p) {return fn<int(__fastcall *)(Obj,unsigned,unsigned)>(GA(GA_231770))(p,0,0xFFD);}
static bool idle_at_home(Obj p) {
    const unsigned home=word(p,0xE2);
    if(!home || home>kMaxCity || word(p,0xE4)!=home || (word(p,0x15E)&0x4001))return false;
    if(fn<int(__fastcall *)(Obj)>(GA(GA_233BE0))(p)!=0 || fn<int(__fastcall *)(Obj,int)>(GA(GA_234AB0))(p,0)!=0)return false;
    if(valid(fn<Obj(__fastcall *)(Obj)>(GA(GA_22C4B0))(p)))return false;                    // already on a job
    return !valid(fn<Obj(__fastcall *)(Obj,int)>(GA(GA_22BAB0))(p,1));                     // not commanding an army
}
static bool governor(Obj p,Obj *corps_out=nullptr) {
    Obj c=corps_at(p[0xE0]);
    const bool yes=c && c[0x11]!=1 && word(c,0x12)==word(p,0x10);
    if(corps_out)*corps_out=c;
    return yes;
}
// A city with the force's lord (force +0x52, as auto_commands reads it) or a governor of another corps in it.
static bool holds_leader(unsigned city,unsigned force,Obj except_corps) {
    Obj f=force<0x34?pointer(world(),0xDF30+force*8):nullptr;
    Obj lord=f?table(0x158,word(f,0x52),6000):nullptr;
    if(valid(lord) && word(lord,0xE4)==city)return true;
    for(unsigned i=0;i<0x34;++i) {
        Obj c=corps_at(i);
        if(!c || c[0x10]!=force || c==except_corps)continue;
        Obj g=table(0x158,word(c,0x12),6000);
        if(valid(g) && word(g,0xE4)==city)return true;
    }
    return false;
}
// Front: a region of this city borders a region held by another regular force (1..45). threat = hostile border regions.
static void front_cities(bool *front,unsigned *threat) {
    for(unsigned id=1;id<=500;++id) {
        Obj a=table(0x6CAF0,id,500);
        if(!valid(a))continue;
        const unsigned city=a[0x35];
        if(!city || city>kMaxCity)continue;
        const unsigned own=force_of_city(city_of(city));
        if(!own)continue;
        for(unsigned i=0;i<8;++i) {
            const unsigned n=word(a,0x56+i*2);
            if(!(a[0x66]&(1u<<i)) || !n || n>500)continue;
            Obj b=table(0x6CAF0,n,500);
            Obj tile=valid(b)?table(0xE270,word(b,0x3A),48399):nullptr;
            const unsigned owner=tile?tile[0x14]:0;
            // The original hostility test 2CC630: an ally, truce partner or vassal (force +0xEA+other / +0x194) is no front.
            if(owner && owner<=45 && owner!=own && fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(owner,own)!=0) {front[city]=true;++threat[city];}
        }
    }
}
static unsigned move_cost() {const unsigned v=dword(image,GA(GA_1A4B4D0));return v?v:1;}
static bool can_pay(Obj city,unsigned need) {Obj c=corps_of_city(city);return c && c[0x14]>=need;}
static void pay(Obj city) {Obj c=corps_of_city(city);if(c)c[0x14]=static_cast<unsigned char>(c[0x14]>=move_cost()?c[0x14]-move_cost():0);}
// One native move command for one officer (the CForceCommandMoveNode form: +0x10 = 22EF50(source), +0x18 = target).
static bool issue_move(Obj p,Obj from,Obj to) { COST_SCOPE("officer_rotation:issue_move");
    struct Command {Obj vtable;unsigned *handle;Obj source;Obj target;} cmd{image+GA(GA_13466B0),nullptr,nullptr,nullptr};
    fn<void(__fastcall *)(void *)>(GA(GA_1AC60))(&cmd);
    if(!cmd.handle)return false;
    Obj *slot=fn<Obj *(__fastcall *)(Obj,unsigned,int)>(GA(GA_18D50))(image+GA(GA_27D4130),*cmd.handle,1);
    const bool ok=slot!=nullptr;
    if(ok) {
        *slot=p;
        cmd.source=fn<Obj(__fastcall *)(Obj)>(GA(GA_22EF50))(from);cmd.target=to;
        fn<void(__fastcall *)(void *,int)>(GA(GA_1F2630))(&cmd,0);
    }
    fn<void(__fastcall *)(void *)>(GA(GA_1BFFF0))(&cmd);
    // 1F2630 validates the command itself (1F12D0) and may drop it: only a mover that now has a job really left.
    return ok && valid(fn<Obj(__fastcall *)(Obj)>(GA(GA_22C4B0))(p));
}
// The native corps edit's governor change for a corps whose governor is about to leave (73ED40).
static Obj pick_governor(Obj corps,Obj old_governor,const std::vector<Obj> &busy) { COST_SCOPE("officer_rotation:pick_governor");
    Obj best=nullptr;int best_lead=-1;
    const unsigned index=old_governor[0xE0],force=corps[0x10];
    for(unsigned id=1;id<=6000;++id) {
        Obj p=table(0x158,id,6000);
        if(!valid(p) || p==old_governor || p[0xE0]!=index || p[0xE6]==1 || !idle_at_home(p))continue;
        if(std::find(busy.begin(),busy.end(),p)!=busy.end())continue;
        Obj home=city_of(word(p,0xE2));
        if(corps_of_city(home)!=corps || holds_leader(word(p,0xE2),force,corps))continue;
        const int l=lead(p);
        if(l>best_lead) {best=p;best_lead=l;}
    }
    return best;
}
static void hand_over(Obj corps,Obj old_governor,Obj best) { COST_SCOPE("officer_rotation:hand_over");
    [[maybe_unused]] const unsigned index=old_governor[0xE0];
    *reinterpret_cast<unsigned short *>(corps+0x12)=word(best,0x10);
    fn<void(__fastcall *)(Obj,int)>(GA(GA_2435D0))(best,2);
    fn<void(__fastcall *)(Obj,int)>(GA(GA_2435D0))(old_governor,4);
    Obj a=city_of(word(best,0xE2)),b=city_of(word(old_governor,0xE2));
    if(valid(a))fn<void(__fastcall *)(Obj)>(GA(GA_284520))(a);
    if(valid(b) && b!=a)fn<void(__fastcall *)(Obj)>(GA(GA_284520))(b);
#ifndef NO_RUNTIME_LOG
    log_event("officer_rotation_governor",corps[0x10]|(index<<8),word(old_governor,0x10),word(best,0x10));
#endif
}
// A mover that would break his corps: governor leaving the corps' cities, or arriving where the lord/another governor is.
static bool needs_hand_over(Obj p,Obj to,Obj *corps) {
    if(!governor(p,corps))return false;
    return corps_of_city(to)!=*corps || holds_leader(word(to,0x4E),(*corps)[0x10],*corps);
}
// 친애 either way (234880; the merit links add the merit lord 친애 to it), cached per pass.
static std::unordered_map<unsigned,bool> like_cache;
static bool likes(Obj a,Obj b) {
    const unsigned x=word(a,0x10),y=word(b,0x10),key=x<y?(x<<16|y):(y<<16|x);
    const auto it=like_cache.find(key);
    if(it!=like_cache.end())return it->second;
    const auto liked=fn<int(__fastcall *)(Obj,Obj)>(GA(GA_234880));
    const bool r=liked(a,b)!=0 || liked(b,a)!=0;
    like_cache.emplace(key,r);
    return r;
}
static bool support(Obj p) {
    for(unsigned slot=0;slot<9;++slot) {
        const unsigned t=fn<unsigned(__fastcall *)(Obj,unsigned)>(GA(GA_232030))(p,slot);
        for(unsigned s:kSupport)if(t==s)return true;
    }
    return false;
}
struct Entry {Obj p;int lead;unsigned city;bool sup,keep,movable;};
struct Pass {
    unsigned force=0,limit=0,swapped=0,moves=0,bonds=0,no_pay=0,no_heir=0,gap_end=0;
    int month=0;
    const bool *front=nullptr;
    std::vector<Entry> all;               // every officer of the force based in one of its cities
    unsigned units[kMaxCity+1]={};        // front cities: units the city can field
    int base[kMaxCity+1]={};              // front cities: current score
    std::vector<Obj> touched;             // movers and new governors of this pass are not picked again
    bool used(Obj p) const {return std::find(touched.begin(),touched.end(),p)!=touched.end();}
};
// Score of front city c with one officer added and/or one removed (see the header).
static int city_score(const Pass &s,unsigned c,const Entry *add,const Entry *remove) {
    std::vector<const Entry *> m;
    for(const Entry &e:s.all)if(e.city==c && &e!=remove)m.push_back(&e);
    if(add)m.push_back(add);
    const size_t k=(std::min<size_t>)(m.size(),s.units[c]);
    std::partial_sort(m.begin(),m.begin()+k,m.end(),[](const Entry *a,const Entry *b){return a->lead>b->lead;});
    int v=0;bool sup=false;
    for(size_t i=0;i<k;++i) {
        v+=m[i]->lead;sup=sup || m[i]->sup;
        for(size_t j=0;j<i;++j)if(likes(m[i]->p,m[j]->p))v+=kBond;
    }
    return v+(sup?kSupportBonus:0);
}
static unsigned head_count(const Pass &s,unsigned c) {unsigned n=0;for(const Entry &e:s.all)n+=e.city==c?1u:0u;return n;}
// Inside the N strongest of his own city (those lead its units).
static bool in_group(const Pass &s,const Entry &e) {
    unsigned stronger=0;
    for(const Entry &o:s.all)if(o.city==e.city && &o!=&e && (o.lead>e.lead || (o.lead==e.lead && word(o.p,0x10)<word(e.p,0x10))))++stronger;
    return stronger<s.units[e.city];
}
// Up to n removable officers of a front city, weakest first: idle, unprotected, unused.
static std::vector<Entry *> removable(Pass &s,unsigned c,size_t n) {
    std::vector<Entry *> r;
    for(Entry &e:s.all)if(e.city==c && e.movable && !e.keep && !s.used(e.p))r.push_back(&e);
    std::sort(r.begin(),r.end(),[](const Entry *a,const Entry *b){return a->lead!=b->lead?a->lead<b->lead:word(a->p,0x10)<word(b->p,0x10);});
    if(r.size()>n)r.resize(n);
    return r;
}
static void rescore(Pass &s,unsigned c) {if(c && c<=kMaxCity && s.front[c])s.base[c]=city_score(s,c,nullptr,nullptr);}
// 1 통솔, 2 친애 with someone there, 3 support trait.
static unsigned kind_of(const Pass &s,const Entry &r,unsigned c) {
    for(const Entry &e:s.all)if(e.city==c && &e!=&r && likes(r.p,e.p))return 2;
    return r.sup?3:1;
}
// One swap: high (rear) goes to low's front city, low to high's rear city. 0 = done, 1 = skip high, 2 = skip low, 3 = stop.
static int swap_pair(Pass &s,Entry &low,Entry &high,unsigned kind) { COST_SCOPE("officer_rotation:swap_pair");
    Obj front_city=city_of(low.city),rear_city=city_of(high.city);
    if(!valid(front_city) || !valid(rear_city) || front_city==rear_city || s.used(high.p))return 1;
    if(s.used(low.p))return 2;
    // 명령서 is refilled every 순 (2593E0) and large computer corps spend it all in their turn: the swap uses what is left
    // and still goes through when nothing is (pay() stops at 0). no_pay counts those free swaps for the diagnostic log.
    const unsigned cost=move_cost();
    const bool same_corps=corps_of_city(front_city)==corps_of_city(rear_city);
    if(same_corps?!can_pay(rear_city,2*cost):!(can_pay(rear_city,cost) && can_pay(front_city,cost)))++s.no_pay;
    // Both hand-overs are decided before anything is written: a pair either goes through whole or not at all.
    std::vector<Obj> busy=s.touched;busy.push_back(low.p);busy.push_back(high.p);
    Obj corps_high=nullptr,corps_low=nullptr,next_high=nullptr,next_low=nullptr;
    if(needs_hand_over(high.p,front_city,&corps_high) && !(next_high=pick_governor(corps_high,high.p,busy))) {++s.no_heir;return 1;}
    if(next_high)busy.push_back(next_high);
    if(needs_hand_over(low.p,rear_city,&corps_low) && !(next_low=pick_governor(corps_low,low.p,busy))) {++s.no_heir;return 2;}
    if(next_high)hand_over(corps_high,high.p,next_high);
    if(next_low)hand_over(corps_low,low.p,next_low);
    s.touched.push_back(low.p);s.touched.push_back(high.p);
    if(next_high)s.touched.push_back(next_high);
    if(next_low)s.touched.push_back(next_low);
    const bool up=issue_move(high.p,rear_city,front_city);
    if(up) {pay(rear_city);moved.push_back({word(high.p,0x10),word(front_city,0x4E),s.month,false});}
    const bool down=up && issue_move(low.p,front_city,rear_city);
    if(down) {pay(front_city);moved.push_back({word(low.p,0x10),word(rear_city,0x4E),s.month,false});}
#ifndef NO_RUNTIME_LOG
    // hex bits 26..27 = kind: 1 통솔, 2 친애, 3 support trait
    log_event("officer_rotation_swap",s.force|(static_cast<unsigned>(word(rear_city,0x4E))<<8)|(static_cast<unsigned>(word(front_city,0x4E))<<16),
        word(high.p,0x10)|(static_cast<unsigned>(high.lead)<<16),
        word(low.p,0x10)|(static_cast<unsigned>(low.lead&0xFF)<<16)|((up?1u:0u)<<24)|((down?1u:0u)<<25)|(kind<<26));
#endif
    if(!up)return 3;
    const unsigned a=low.city,b=high.city;
    high.city=a;if(down)low.city=b;
    ++s.swapped;if(kind==2)++s.bonds;
    rescore(s,a);rescore(s,b);
    return 0;
}
// One one-way move (start arrangement 4, front-lost drain 5).
static bool move_one(Pass &s,Entry &r,unsigned to_id,unsigned kind) { COST_SCOPE("officer_rotation:move_one");
    (void)kind; // Only the diagnostic log reads it; the public build strips log_event.
    Obj from=city_of(r.city),to=city_of(to_id);
    if(!valid(from) || !valid(to) || from==to || s.used(r.p))return false;
    if(!can_pay(from,move_cost()))++s.no_pay;
    std::vector<Obj> busy=s.touched;busy.push_back(r.p);
    Obj corps=nullptr,next=nullptr;
    s.touched.push_back(r.p);
    if(needs_hand_over(r.p,to,&corps) && !(next=pick_governor(corps,r.p,busy))) {++s.no_heir;return false;}
    if(next) {hand_over(corps,r.p,next);s.touched.push_back(next);}
    const bool ok=issue_move(r.p,from,to);
    if(ok) {pay(from);moved.push_back({word(r.p,0x10),word(to,0x4E),s.month,false});++s.moves;}
#ifndef NO_RUNTIME_LOG
    log_event("officer_rotation_move",s.force|(static_cast<unsigned>(word(from,0x4E))<<8)|(static_cast<unsigned>(word(to,0x4E))<<16),
        word(r.p,0x10)|(static_cast<unsigned>(r.lead)<<16),(ok?1u:0u)|(kind<<4)|(kind_of(s,r,to_id)<<8));
#endif
    if(ok) {const unsigned old=r.city;r.city=to_id;rescore(s,old);rescore(s,to_id);}
    return ok;
}
// Monthly: up to limit swaps, each the rear-front pair with the largest score gain.
static void monthly(Pass &s) {
    while(s.swapped<s.limit) {
        int best=static_cast<int>(kGap)-1;Entry *bf=nullptr,*br=nullptr;unsigned bc=0;
        for(unsigned c=1;c<=kMaxCity;++c) {
            if(!s.front[c] || !s.units[c])continue;
            const std::vector<Entry *> weak=removable(s,c,3);
            for(Entry &r:s.all) {
                if(s.front[r.city] || !r.movable || s.used(r.p))continue;
                for(Entry *f:weak) {
                    const int g=city_score(s,c,&r,f)-s.base[c];
                    if(g>best) {best=g;bf=f;br=&r;bc=c;}
                }
            }
        }
        if(!br) {s.gap_end=1;return;}
        const int r=swap_pair(s,*bf,*br,kind_of(s,*br,bc));
        if(r==3)return;
        if(r!=0) {s.touched.push_back(r==1?br->p:bf->p);}
    }
}
// New game: front cities by threat, round-robin, each takes its best addition until nothing gains.
static void start_pass(Pass &s,const unsigned *threat) {
    std::vector<unsigned> order;
    for(unsigned c=1;c<=kMaxCity;++c)if(s.front[c] && s.units[c])order.push_back(c);
    std::stable_sort(order.begin(),order.end(),[threat](unsigned a,unsigned b){return threat[a]>threat[b];});
    for(bool progress=true;progress && s.moves+s.swapped<s.limit;) {
        progress=false;
        for(unsigned c:order) {
            if(s.moves+s.swapped>=s.limit)break;
            const bool fill=head_count(s,c)<s.units[c];
            const std::vector<Entry *> weak=fill?std::vector<Entry *>{}:removable(s,c,1);
            Entry *f=weak.empty()?nullptr:weak[0];
            int best=f?static_cast<int>(kGap)-1:0;Entry *br=nullptr;
            for(Entry &r:s.all) {
                if(r.city==c || !r.movable || s.used(r.p))continue;
                if(s.front[r.city] && (r.keep || in_group(s,r)))continue;   // another front city keeps its own group
                const int g=city_score(s,c,&r,f)-s.base[c];
                if(g>best) {best=g;br=&r;}
            }
            if(!br)continue;
            const unsigned kind=kind_of(s,*br,c);
            bool ok=false;
            if(f) {
                const int r=swap_pair(s,*f,*br,kind);
                ok=r==0;
                if(r==3)return;
                if(r!=0)s.touched.push_back(r==1?br->p:f->p);
            } else ok=move_one(s,*br,c,4);
            progress=progress || ok || br;
        }
    }
}
// A city that lost its front: up to kDrainPerCity idle officers go one way to the front city they add most to.
static void drain(Pass &s) {
    for(unsigned c=1;c<=kMaxCity;++c) {
        if(!drain_left[c] || s.front[c])continue;
        std::vector<Entry *> out;
        for(Entry &e:s.all)if(e.city==c && e.movable && !s.used(e.p))out.push_back(&e);
        if(out.empty())continue;
        std::sort(out.begin(),out.end(),[](const Entry *a,const Entry *b){return a->lead>b->lead;});
        unsigned sent=0;
        for(Entry *r:out) {
            if(sent>=kDrainPerCity || head_count(s,c)<=kKeepRear)break;
            int best=0;unsigned bc=0;
            for(unsigned t=1;t<=kMaxCity;++t) {
                if(!s.front[t] || !s.units[t])continue;
                const int g=city_score(s,t,r,nullptr)-s.base[t];
                if(g>best) {best=g;bc=t;}
            }
            if(!bc)break;
            if(move_one(s,*r,bc,5))++sent;
        }
    }
}
static unsigned rotate_force(unsigned force,const bool *front,const unsigned *threat,int month,bool start) { COST_SCOPE("officer_rotation:rotate_force");
    Pass s;s.force=force;s.month=month;s.front=front;s.limit=start?kStartMoves:kPairs;
    like_cache.clear();
    for(unsigned id=1;id<=6000;++id) {
        Obj p=table(0x158,id,6000);
        if(!valid(p) || fn<unsigned(__fastcall *)(Obj)>(GA(GA_22E870))(p)!=force)continue;
        const unsigned home=word(p,0xE2);
        if(!home || home>kMaxCity || force_of_city(city_of(home))!=force)continue;
        const bool movable=p[0xE6]!=1 && idle_at_home(p) && !cooling(p,month);
        s.all.push_back({p,lead(p),home,support(p),false,movable});
    }
    for(unsigned c=1;c<=kMaxCity;++c) {
        if(!front[c])continue;
        Obj city=city_of(c);
        if(!valid(city) || force_of_city(city)!=force)continue;
        const unsigned troops=dword(city,0x3C);
        s.units[c]=(std::max)(kMinUnits,(std::min)(kMaxUnits,troops/kUnitTroops));
    }
    // Protected at the front: a support-trait holder, or in 친애 with an anchor of his city.
    [[maybe_unused]] unsigned kept=0,idle_front=0,idle_rear=0;
    for(Entry &e:s.all) {
        if(!front[e.city])continue;
        e.keep=e.sup;
        for(const Entry &q:s.all)if(!e.keep && &q!=&e && q.city==e.city && q.lead>=static_cast<int>(kAnchorLead) && likes(q.p,e.p))e.keep=true;
        kept+=e.keep && e.movable?1u:0u;
    }
    for(const Entry &e:s.all)if(e.movable)(front[e.city]?idle_front:idle_rear)+=1;
    for(unsigned c=1;c<=kMaxCity;++c)rescore(s,c);
    if(!start)drain(s);
    if(start)start_pass(s,threat); else monthly(s);
#ifndef NO_RUNTIME_LOG
    // Front cities after the pass: city | units <<8 | head count <<16 | threat <<24 ; score ; troops.
    if(start || s.month%3==0)
        for(unsigned c=1;c<=kMaxCity;++c)if(s.units[c])
            log_event("officer_rotation_city",c|(s.units[c]<<8)|((std::min)(head_count(s,c),255u)<<16)|((std::min)(threat[c],255u)<<24),
                static_cast<unsigned>(s.base[c]),dword(city_of(c),0x3C));
#endif
#ifndef NO_RUNTIME_LOG
    // force | idle front <<8 | idle rear <<20 ; no_pay | no_heir <<8 | gap stop <<16 | protected <<17 ; swaps | 친애 swaps <<10 | one-way moves <<20
    log_event("officer_rotation_force",force|((std::min)(idle_front,4095u)<<8)|((std::min)(idle_rear,4095u)<<20),
        (std::min)(s.no_pay,255u)|((std::min)(s.no_heir,255u)<<8)|(s.gap_end<<16)|((std::min)(kept,4095u)<<17),
        (std::min)(s.swapped,1023u)|((std::min)(s.bonds,1023u)<<10)|((std::min)(s.moves,1023u)<<20));
#endif
    return s.swapped+s.moves;
}
// Demo play (데모 플레이) hands the player's force to the computer: the controlling-force field (game +0x3A) is 0
// while 2350F0 still names the old player corps, so that force must not be excluded then.
static bool demo_play() {Obj root=world();Obj ctx=root?pointer(root,0x871E8):nullptr;return ctx && ctx[0x3A]==0;}
static unsigned player_force() {
    if(demo_play())return 0;
    for(unsigned i=0;i<0x34;++i) {
        Obj c=corps_at(i);
        if(c && fn<int(__fastcall *)(Obj)>(GA(GA_2350F0))(c)!=0)return c[0x10];
    }
    return 0;
}
// Domestic-tick subscriber (pre): the first tick of a month's 상순 runs every computer force once.
static void tick(Obj) { COST_SCOPE("officer_rotation:tick");
    if(!mod_features.officer_rotation)return;
    Obj root=world();Obj save=root?pointer(root,0x871E8):nullptr;if(!save)return;
    Obj d=save+0x34;
    const int day=static_cast<signed char>(d[3]),month=static_cast<int>(*reinterpret_cast<short *>(d))*12+static_cast<signed char>(d[2]);
    // A world still being built (scenario load: year 0, garbage force ids) is not a game yet.
    if(*reinterpret_cast<short *>(d)<100)return;
    // A new game (fresh world, 순 counter G+0x454 of the date state still <= kStartTurns) is arranged at once on its first
    // tick, whatever the 순. G = world+0x871E8 (the date is at G+0x34..0x37, CVariableGetGameDataNode 8EB70).
    const bool start=done_world!=root && dword(save,0x454)<=kStartTurns;
#ifndef NO_RUNTIME_LOG
    if(done_world!=root)log_event("officer_rotation_world",dword(save,0x454),static_cast<unsigned>(month),start?1u:0u);
#endif
    if(!start && (day<1 || day>10))return;
    if(done_world==root && done_month==month)return;
    if(done_world!=root) {moved.clear();std::fill(std::begin(drain_left),std::end(drain_left),0);prev_valid=false;}
    done_world=root;done_month=month;
    // Movers settled in another city than the one this module sent them to (home != target, not travelling) were moved
    // again by the original AI: do not fight it, hold them for kHeld months.
    for(Moved &m:moved) {
        const int age=month-m.month;
        if(age<1 || m.held)continue;
        Obj p=table(0x158,m.id,6000);
        if(!valid(p))continue;
        const unsigned home=word(p,0xE2),loc=word(p,0xE4);
#ifndef NO_RUNTIME_LOG
        // Where last month's movers are now: target | home <<8 | location <<16 ; months since the move.
        if(age==1)log_event("officer_rotation_followup",m.id,m.target|(home<<8)|(loc<<16),1);
#endif
        if(home && home!=m.target && loc==home) {
            m.held=true;
#ifndef NO_RUNTIME_LOG
            log_event("officer_rotation_held",m.id,m.target|(home<<8),static_cast<unsigned>(age));
#endif
        }
    }
    moved.erase(std::remove_if(moved.begin(),moved.end(),[month](const Moved &m){return month-m.month>=span(m);}),moved.end());
    bool front[kMaxCity+1]={};
    unsigned threat[kMaxCity+1]={};
    front_cities(front,threat);
    // A city that was front last month and is not now (its neighbour fell) sends officers forward for kDrainMonths.
    for(unsigned c=1;c<=kMaxCity;++c) {
        if(drain_left[c]>0)--drain_left[c];
        if(prev_valid && prev_front[c] && !front[c]) {
            drain_left[c]=kDrainMonths;
#ifndef NO_RUNTIME_LOG
            log_event("officer_rotation_front_lost",c,force_of_city(city_of(c)),static_cast<unsigned>(month));
#endif
        }
        prev_front[c]=front[c];
    }
    prev_valid=true;
    const unsigned player=player_force();
    bool seen[46]={};
    [[maybe_unused]] unsigned forces=0,pairs=0;
    for(unsigned id=1;id<=kMaxCity;++id) {
        const unsigned f=force_of_city(city_of(id));
        if(!f || f>45 || f==player || seen[f])continue;
        seen[f]=true;++forces;
        pairs+=rotate_force(f,front,threat,month,start);
    }
#ifndef NO_RUNTIME_LOG
    log_event("officer_rotation_month",static_cast<unsigned>(month),forces|(start?0x100u:0u),pairs);
#endif
}
}
