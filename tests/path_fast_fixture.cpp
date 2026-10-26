// User/Claude 2026-10-25: clean single segments remain exact; all waypoints use fresh generations. No game execution.
#include "../src/path_fast.h"
#include <immintrin.h>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <array>
#include <string>
using namespace path_fast_core;
static unsigned checks,open_improvements,reopens;
static void check(bool value,const char *why) {++checks;if(!value){std::fprintf(stderr,"%s\n",why);std::exit(1);}}
constexpr unsigned first=2000;
// 2A7720 decoded switch, including asymmetric top/bottom edge fallbacks.
static unsigned native_neighbour(unsigned id,unsigned d) {
    const unsigned x=id%220,y=id/220;
    switch(d) {
    case 1:return x?(x%2?(y?id-221:cells):id-1):cells;
    case 2:return y?id-220:cells;
    case 3:return x<219?(x%2?(y?id-219:cells):id+1):cells;
    case 4:return x?(!(x%2) && y<219?id+219:id-1):cells;
    case 5:return y<219?id+220:cells;
    case 6:return x<219?(!(x%2) && y<219?id+221:id+1):cells;
    default:return cells;
    }
}
struct Tile {unsigned id;int cost;bool blocked,fort;};
struct Terrain {
    unsigned width=8,height=8;
    std::vector<Tile> tiles;
    std::vector<int> estimates;
    bool reject=false;
    bool native_grid=false,stateful=false;
    unsigned ctx10=0,ctx28=0,ctx2c=0;
};
struct Fake {
    Terrain &map;
    Node *nodes=nullptr;
    unsigned start=0,goal=0,flags=0;
    std::vector<std::uint64_t> trace;
    std::vector<unsigned> terrain_calls,adjust_calls;
    std::array<int,16> aux{};
    unsigned callback_order=0;
    explicit Fake(Terrain &t):map(t),terrain_calls(t.tiles.size()),adjust_calls(t.tiles.size()) {trace.reserve(2048);}
    void event(unsigned op,unsigned a=0,unsigned b=0) {
        trace.push_back((static_cast<std::uint64_t>(op)<<56)|(static_cast<std::uint64_t>(a)<<24)|b);
    }
    unsigned distance(unsigned a,unsigned b) const {
        a-=first;b-=first;
        const int x=static_cast<int>(a%map.width)-static_cast<int>(b%map.width);
        const int y=static_cast<int>(a/map.width)-static_cast<int>(b/map.width);
        return static_cast<unsigned>(std::abs(x)+std::abs(y));
    }
    bool prepare(Node *buffer,unsigned from,unsigned to,unsigned bits) {
        nodes=buffer;start=from;goal=to;flags=bits;
        event(1,from,to);coordinate(from);
        // 18AFA0 for one goal: coordinate, optional initial distance, state=1.
        Node &n=nodes[goal];n.cell=goal;coordinate(goal);
        if(flags&5)n.f=static_cast<int>(distance(goal,start));
        n.state=1;
        return !map.reject;
    }
    void begin() {event(2);}
    unsigned neighbour(unsigned id,unsigned d) {
        event(3,id,d);
        if(map.native_grid)return native_neighbour(id,d);
        static const int dx[]={0,1,0,-1,-1,0,1},dy[]={0,0,1,1,0,-1,-1};
        const unsigned local=id-first;
        const int x=static_cast<int>(local%map.width)+dx[d];
        const int y=static_cast<int>(local/map.width)+dy[d];
        if(x<0 || y<0 || x>=static_cast<int>(map.width) || y>=static_cast<int>(map.height))return cells;
        return first+static_cast<unsigned>(y)*map.width+static_cast<unsigned>(x);
    }
    Tile *tile(unsigned id) {
        event(4,id);
        return id>=first && id-first<map.tiles.size()?&map.tiles[id-first]:nullptr;
    }
    int terrain(Tile *tile_value) {
        event(5,tile_value->id);
        const unsigned visits=terrain_calls[tile_value->id-first]++;
        // Adversarial 2AE6F0 stand-in: a repeated cell can have a different cost.
        return tile_value->cost+(map.stateful?static_cast<int>((visits+callback_order++)%3):0);
    }
    bool allowed(unsigned id,Tile *t,std::int32_t &cost) {
        event(6,id,flags);
        if(t->blocked && !(flags&0xC))return false;
        if(!(flags&0xC) && map.ctx28 && t->id%5==0)return false;
        if(!(flags&0xC) && map.ctx2c && t->id%7==0)cost=add(cost,10000);
        if(!(flags&0xC) && map.ctx10 && t->fort)return false;
        if((flags&0xC) && t->fort)return false;
        if((flags&0x10) && id!=goal && t->fort)return false;
        if((flags&0x21)==1 && t->fort)cost=add(cost,3);
        if(!(flags&0x40) && t->fort && (id&7)==0)return false;
        return true;
    }
    int adjust(int cost,Tile *t) {
        event(7,t->id,static_cast<unsigned>(cost));
        ++adjust_calls[t->id-first];
        if(!map.stateful || !(flags&1))return cost;
        // 1864F0's aux is per search and keyed by region, not cell. First entry
        // invokes movement, later entries reuse it; deliberately order-sensitive.
        const unsigned region=(t->id-first)/8%16;
        if(!aux[region]) {aux[region]=1+static_cast<int>(callback_order++%3);event(12,t->id,region);}
        return cost/aux[region];
    }
    void coordinate(unsigned id) {event(8,id);}
    int heuristic(unsigned id) {
        event(9,id,start);
        // Many ties, plus deliberately inconsistent estimates to exercise closed
        // reopening; preserve CVTTSS2SI even for extreme floating-point inputs.
        const int h=map.estimates[id-first];
        return _mm_cvttss_si32(_mm_mul_ss(_mm_cvtepi32_ps(_mm_cvtsi32_si128(h)),_mm_set_ss(1.25f)));
    }
    void success() {event(10);}
    void cleanup() {event(11);}
};

// Independent reference BST. 18AE60 splices the right subtree onto the
// rightmost node of the left subtree and zeros the detached node's two links.
static void bst_insert(Node *&root,Node *n) {
    Node **at=&root;
    while(*at)at=n->f<(*at)->f?&(*at)->left:&(*at)->right;
    *at=n;
}
static void bst_erase(Node *&root,Node *n) {
    Node **at=&root;
    if(!*at)return;
    while(*at!=n)at=n->f<(*at)->f?&(*at)->left:&(*at)->right;
    if(!n->left)*at=n->right;
    else {
        *at=n->left;Node *last=n->left;
        while(last->right)last=last->right;
        last->right=n->right;
    }
    n->left=n->right=nullptr;
}
static Node *bst_pop(Node *&root) {
    Node **at=&root;
    while((*at)->left)at=&(*at)->left;
    Node *n=*at;*at=n->right;n->left=n->right=nullptr;return n;
}
static Node *reference(std::vector<Node> &buffer,unsigned start,unsigned goal,unsigned flags,Fake &s) {
    // 184485 clears all 48400 records; 183A7C clears the first BD10 bytes again.
    std::memset(buffer.data(),0,buffer.size()*sizeof(Node));
    std::memset(buffer.data(),0,0xBD10);
    if(!s.prepare(buffer.data(),start,goal,flags))return nullptr;
    Node *root=&buffer[goal];s.begin();Node *answer=nullptr;
    while(root) {
        Node *current=bst_pop(root);
        if(current->cell==start) {
            answer=current;Node *n=current;
            do {n->state|=4;n=n->parent;} while(n);
            s.success();break;
        }
        for(unsigned direction=1;direction<=6;++direction) {
            const unsigned id=s.neighbour(current->cell,direction);
            Tile *tile=s.tile(id);
            if(!tile || id>0xBD0F)continue;
            std::int32_t cost=10000;
            if(flags&5)cost=s.terrain(tile);
            if(id!=start && !s.allowed(id,tile,cost))continue;
            Node *node=&buffer[id];
            const int candidate=add(current->g,s.adjust(cost,tile));
            if((node->state&3)!=0 && node->g<=candidate)continue;
            if(node->state&1)++open_improvements;
            if(node->state&2)++reopens;
            node->cell=id;node->parent=current;node->state&=0xFD;
            s.coordinate(current->cell);
            if(node->state&1) {
                bst_erase(root,node);
                node->g=candidate;
                node->f=(flags&5)?add(candidate,s.heuristic(id)):candidate;
                bst_insert(root,node);
            } else {
                node->g=candidate;
                node->f=(flags&5)?add(candidate,s.heuristic(id)):candidate;
                bst_insert(root,node);node->state|=1;
            }
        }
        current->state=static_cast<unsigned char>((current->state&0xFE)|2);
    }
    s.cleanup();return answer;
}
static bool reference_assemble(Result &out,unsigned *destination,Node *node) {
    out.data=destination;out.count=0;out.cost=0;
    if(node) {
        out.cost=add(out.cost,node->g);
        do {out.data[out.count]=node->cell;++out.count;node=node->parent;} while(node);
    }
    return out.count>0;
}
static void compare(Terrain &map,Storage &fast,std::vector<Node> &slow,unsigned start,unsigned goal,unsigned flags) {
    Fake old_s(map),new_s(map);
    Node *old_node=reference(slow,start,goal,flags,old_s);
    Node *new_node=search(fast,start,goal,flags,new_s);
    std::array<unsigned,result_bytes/4+2> before{},after{};
    before.back()=after.back()=0xCAFECAFE;
    Result a{},b{};
    const bool old_ok=reference_assemble(a,before.data(),old_node);
    const bool new_ok=assemble(b,after.data(),new_node);
    check(old_ok==new_ok && a.count==b.count && a.cost==b.cost && before==after,"BST/heap full path, count, cost, return, destination bytes");
    check(old_s.trace==new_s.trace,"same helper sequence, cells, directions, flags and cost inputs");
    check(old_s.terrain_calls==new_s.terrain_calls && old_s.adjust_calls==new_s.adjust_calls && old_s.aux==new_s.aux,
          "stateful terrain/adjust visit counts and per-search region aux cache match");
    for(unsigned i=first;i<first+map.tiles.size();++i) {
        Node clean{};const Node &n=fast.generations[i]==fast.generation?fast.nodes[i]:clean;
        const Node &r=slow[i];
        check(n.cell==r.cell && n.state==r.state && n.g==r.g && n.f==r.f &&
              ((!n.parent && !r.parent) || (n.parent && r.parent && n.parent-fast.nodes.data()==r.parent-slow.data())),
              "all observable node fields equal; private tree pointers excluded by reader audit");
    }
}
static void diagnostics_fixture() {
    for(unsigned interval:{0u,1u,2u,16u,UINT32_MAX}) {
        unsigned sampled=0;
        for(std::uint64_t call=1;call<=1024;++call) {
            const bool yes=verify_call(interval,call);
            check(yes==(interval && call%interval==0),"global ordinal sampling, including delegated ordinals");
            if(yes)++sampled;
        }
        check(sampled==(interval?1024/interval:0),"sample count uses Nth global call, no sample at first call for N>1");
        check(elide_clear(interval)==(interval!=1),"only every-call verification retains native outer clear");
    }
    check(verify_call(16,0x100000010ull) && !verify_call(16,0x100000011ull),"ordinal stays 64-bit across UINT32 rollover");
    std::array<Node,65> original{},actual{};
    for(unsigned i=0;i<65;++i) {
        original[i].cell=actual[i].cell=100+i;
        original[i].g=actual[i].g=64-static_cast<int>(i);
        original[i].parent=i<64?&original[i+1]:nullptr;
        actual[i].parent=i<64?&actual[i+1]:nullptr;
    }
    auto diff=compare(original.data(),actual.data());
    check(!diff.mismatch && diff.original_length==65 && diff.new_length==65,"chain comparison includes every node g and cell");
    actual[2].cell=900;actual[2].g=-9;
    diff=compare(original.data(),actual.data());
    check(diff.mismatch && diff.first==2 && diff.original_cell==102 && diff.new_cell==900,"first difference uses 1843E0 head-to-parent order");
    struct Event {std::string name;unsigned a,b,c;};
    std::vector<Event> events;
    auto emit=[&](const char *name,auto a,auto b,auto c) {
        events.push_back({name,static_cast<unsigned>(a),static_cast<unsigned>(b),static_cast<unsigned>(c)});
    };
    Storage storage;storage.begin();std::vector<Node> buffer(cells);
    const unsigned neighbour=native_neighbour(102,5);
    buffer[neighbour].state=7;buffer[neighbour].g=-1;
    storage.touch(neighbour).state=3;storage.nodes[neighbour].g=0x12345678;
    const unsigned stale=native_neighbour(102,6);
    storage.nodes[stale].state=255;storage.nodes[stale].g=93; // no generation tag: logically zero
    mismatch_detail(32,77,88,0x51,1,2,3,1,-4,original.data(),actual.data(),buffer.data(),storage,diff,emit,native_neighbour);
    check(events.size()==5+64+64+12,"five metadata rows, each chain capped at 64, six neighbours for each first-difference cell");
    check(events[0].name=="path_fast_mm" && events[0].a==32 && events[0].b==77 && events[0].c==88,"mismatch ordinal/start/goal");
    check(events[1].name=="path_fast_mm_flags" && events[1].a==0x51 && events[1].b==1 && events[1].c==2,"flags and ctx10/28");
    check(events[2].name=="path_fast_mm_ctx" && events[2].a==3 && events[2].b==1 && events[2].c==static_cast<unsigned>(-4),"ctx2c, captured validity, captured force without extra helper calls");
    check(events[3].name=="path_fast_mm_cost" && events[3].a==64 && events[3].b==64 && events[3].c==((65u<<16)|65u),"head g and packed lengths");
    check(events[4].a==2 && events[4].b==102 && events[4].c==900,"first mismatch metadata");
    check(events[5+64+2].name=="path_fast_mm_new" && events[5+64+2].c==static_cast<unsigned>(-9),"new chain uses new node g");
    check(events[5+128+4].a==neighbour && events[5+128+4].b==0x07FFFFFF && events[5+128+4].c==0x03345678,"neighbour packs status and low 24 g bits");
    check(events[5+128+5].a==stale && events[5+128+5].c==0,"previous-generation neighbours are zero without touching storage");
    const auto untouched=buffer;
    check(verified_result(1,false,original.data(),actual.data(),buffer.data())==original.data(),"N=1 returns native identity even on equality");
    check(verified_result(16,true,original.data(),actual.data(),buffer.data())==original.data() &&
          !std::memcmp(buffer.data(),untouched.data(),node_bytes),"sample mismatch keeps every original buffer byte and pointer");
    actual[2]=original[2];actual[2].parent=&actual[3];
    Node *published=verified_result(16,false,original.data(),actual.data(),buffer.data());
    check(published==buffer.data()+100 && published->parent==buffer.data()+101,"matching sample materializes new chain in native slot");
    check(!verified_result(16,false,nullptr,nullptr,buffer.data()),"matching failures return null");
    actual[3].g+=1;diff=compare(original.data(),actual.data());
    check(diff.mismatch && diff.first==3 && diff.original_cell==diff.new_cell,"same cell with different g is a mismatch");
    diff=compare(nullptr,actual.data());
    check(diff.mismatch && diff.first==0 && diff.original_cell==cells && diff.new_length==65,"failed original sentinel and unequal chain lengths");
    diff=compare(nullptr,nullptr);check(!diff.mismatch,"both failed searches compare equal");
}
static void queue_fixture() {
    Storage heap(128);heap.begin();std::array<Node,128> tree{};
    std::array<bool,128> open{};Node *root=nullptr;
    std::mt19937 random(0x18AE60);
    for(unsigned op=0;op<20000;++op) {
        const unsigned id=random()%128;
        if(random()%3==0 && root) {
            Node *a=bst_pop(root),*b=heap.pop();const unsigned popped=static_cast<unsigned>(a-tree.data());
            check(popped==b->cell,"signed f plus insertion seq equals BST after erasure/reinsert and equal-f reopen");
            open[popped]=false;
        } else {
            Node &a=tree[id],&b=heap.touch(id);
            if(open[id]) {bst_erase(root,&a);heap.erase(&b);}
            const int f=op%83==0?INT32_MIN:op%89==0?INT32_MAX:static_cast<int>(random()%4)-2;
            a.cell=b.cell=id;a.f=b.f=f;bst_insert(root,&a);heap.insert(&b);open[id]=true;
        }
    }
    while(root)check(bst_pop(root)->cell==heap.pop()->cell,"final BST/heap tie order");
    check(heap.heap.empty(),"queue populations agree");
}
int main() {
    diagnostics_fixture();queue_fixture();
    Storage fast;std::vector<Node> slow(cells);
    std::mt19937 random(0x183A30);
    constexpr unsigned cases=40000;
    for(unsigned test=0;test<cases;++test) {
        Terrain map;map.width=3+random()%10;map.height=3+random()%10;
        const unsigned size=map.width*map.height;
        map.reject=test%97==0;
        for(unsigned i=0;i<size;++i) {
            map.tiles.push_back({first+i,1+static_cast<int>(random()%5),random()%7==0,random()%9==0});
            map.estimates.push_back(static_cast<int>(random()%25));
        }
        const unsigned start=first+random()%size,goal=first+random()%size;
        compare(map,fast,slow,start,goal,test%128);
    }
    check(open_improvements>0 && reopens>0,"random terrain covers open improvement and closed reopening");
    // User 2026-10-25: reproduce equal-length alternatives (26/30/11), actual
    // 2A7720 directions, every low flag combination, ctx branches and helper state.
    for(unsigned length:{26u,30u,11u}) {
        Terrain line;line.width=220;line.height=length;line.native_grid=true;
        for(unsigned i=0;i<line.width*line.height;++i) {
            const unsigned column=(first+i)%220;
            line.tiles.push_back({first+i,1,column<20 || column>22,false});line.estimates.push_back(0);
        }
        // Two lateral steps can be interleaved with vertical steps in many
        // positions: equal g/f alternatives with exactly the observed lengths.
        compare(line,fast,slow,first,first+(length-2)*220+2,1);
        check(compare(slow.data()+first,fast.nodes.data()+first).original_length==length,"target diagnostic path lengths 26,30,11");
    }
    for(unsigned test=0;test<4096;++test) {
        Terrain map;map.width=220;map.height=5;map.native_grid=true;map.stateful=true;
        map.ctx10=(test>>7)&1;map.ctx28=(test>>8)&1;map.ctx2c=(test>>9)&1;
        for(unsigned i=0;i<map.width*map.height;++i) {
            const bool outside=(first+i)%220>29 || (first+i)%220<20;
            map.tiles.push_back({first+i,1+static_cast<int>(random()%3),outside || random()%11==0,random()%9==0});
            map.estimates.push_back(static_cast<int>(random()%8));
        }
        // flags&0xC admits blocked cells in the stand-in, so restrict this map
        // to five rows and use positive costs even with order-dependent callbacks.
        compare(map,fast,slow,first,first+4*220+9,test%128);
    }
    // Deliberately poison all previous node fields. Epoch rollover still starts
    // clean, including IDs beyond 1210 and earlier waypoint residuals.
    fast.generation=UINT32_MAX;
    Terrain one;one.width=one.height=1;one.tiles={{first,1,false,false}};one.estimates={0};
    compare(one,fast,slow,first,first,1);
    check(fast.generation==1,"generation rollover resets tags");
    for(int n:{-1,0,2,3,8}) {
        unsigned original_calls=0;
        auto original=[&](int actual_n) {++original_calls;check(actual_n==n,"delegation keeps original arguments");return 73;};
        const int value=eligible(n,slow.data(),0xBD10,0,first,first)?0:original(n);
        check(value==73 && original_calls==1,"every inner goal_count!=1 delegates exactly once");
    }
    check(eligible(1,slow.data(),0xBD10,0,first,first),"each inner single-goal segment is eligible");
    check(!eligible(1,nullptr,0xBD10,0,first,first) && !eligible(1,slow.data(),0,0,first,first) &&
          !eligible(1,slow.data(),0xBD10,1,first,first) && !eligible(1,slow.data(),0xBD10,0,cells,first) &&
          !eligible(1,slow.data(),0xBD10,0,first,cells) && !eligible(1,slow.data(),0xBD10,0,first,first,true),
          "null buffer, nonstandard clear bytes, debug, invalid ID and recursion delegate");
    // User/Claude 2026-10-25: outer 1843E0 no longer clears node slots. Only
    // the exported chain may change; all 40 bytes of each exported record match.
    std::vector<Node> published(cells),poison(cells);
    std::memset(poison.data(),0xA5,node_bytes);
    Terrain multi;multi.width=multi.height=8;
    for(unsigned i=0;i<64;++i) {multi.tiles.push_back({first+i,1,false,false});multi.estimates.push_back(0);}
    for(unsigned test=0;test<2000;++test) {
        published=poison;
        const unsigned from=first+random()%64;
        std::vector<unsigned> native_output(cells),fast_output(cells);
        Result expected{native_output.data(),0,0},actual{fast_output.data(),0,0};
        unsigned start=from;
        for(unsigned segment=0;segment<4;++segment) {
            unsigned goal=first+random()%64;
            if(goal==start)goal=first+(goal-first+1)%64; // native skips first node unconditionally after segment 0
            Fake a(multi),b(multi);
            Node *old_node=reference(slow,start,goal,test%128,a);
            Node *new_node=search(fast,start,goal,test%128,b);
            check(a.trace==b.trace,"multi-segment helper order equals independent clean reference");
            const std::vector<Node> untouched=published;
            Node *head=materialize(published.data(),new_node);
            check((old_node!=nullptr)==(head!=nullptr),"published success equals fresh BST reference");
            std::vector<bool> changed(cells,false);
            for(Node *n=new_node,*r=old_node;n;n=n->parent,r=r->parent) {
                changed[n->cell]=true;
                Node value{};value.cell=n->cell;value.state=r->state;value.g=r->g;value.f=r->f;
                value.parent=n->parent?published.data()+n->parent->cell:nullptr;
                check(!std::memcmp(&published[n->cell],&value,sizeof(Node)),"full 40-byte record, native state|4, g/f and in-slot parent");
            }
            for(unsigned i=0;i<cells;++i)if(!changed[i])
                check(!std::memcmp(&published[i],&untouched[i],sizeof(Node)),"outside-chain records remain byte-identical, including stale previous segments");
            // Exact 1844EF / 1844F3 / 184500 consumption, including its cost
            // from the skipped-to node rather than the original head on later segments.
            if(old_node) {
                Node *n=segment?old_node->parent:old_node;
                expected.cost=add(expected.cost,n->g);
                do {expected.data[expected.count++]=n->cell;n=n->parent;}while(n);
            }
            if(head) {
                Node *n=segment?head->parent:head;
                actual.cost=add(actual.cost,n->g);
                do {actual.data[actual.count++]=n->cell;n=n->parent;}while(n);
            }
            check(expected.count==actual.count && expected.cost==actual.cost && native_output==fast_output,
                  "1843E0 reads identical cells and costs across independently clean segments with no outer clear");
            start=goal;
        }
    }
    check(!clean(poison.data()),"verify classifies residual states as multi-waypoint");
    std::memset(slow.data(),0,node_bytes);check(clean(slow.data()),"verify classifies all-zero states as clean");
    slow[cells-1].g=91;check(clean(slow.data()),"clean classification uses state bytes only");
    for(unsigned i:{0u,1209u,1210u,cells-1}) {
        published=poison;published[i].state=0;
        clear_delegate(published.data(),true,true);
        check(!std::memcmp(published.data(),std::vector<Node>(cells).data(),node_bytes),"fallback clears entire known slot, including beyond 1210 nodes");
        Fake a(multi),b(multi);
        Node *old_node=reference(slow,first,first+63,1,a);
        Node *new_node=reference(published,first,first+63,1,b);
        check(a.trace==b.trace && old_node->g==new_node->g,"delegated original receives fresh slot");
    }
    published=poison;clear_delegate(published.data(),true,false);
    check(!std::memcmp(published.data(),poison.data(),node_bytes),"verify mode retains native residual buffer before original");
    clear_delegate(published.data(),false,true);
    check(!std::memcmp(published.data(),poison.data(),node_bytes),"unknown pointer never receives an assumed full-slot clear");
    // 184500 contains no truncation check: exactly 1320 cells, one beyond that
    // result-buffer boundary, and the largest acyclic chain must copy identically.
    for(unsigned length:{0u,1u,1319u,1320u,1321u,cells}) {
        std::vector<Node> chain(length);
        for(unsigned i=0;i<length;++i) {chain[i]={};chain[i].cell=i;chain[i].parent=i+1<length?&chain[i+1]:nullptr;chain[i].g=19;}
        std::vector<unsigned> a(cells+2,0xDEADBEEF),b=a;
        std::memset(a.data(),0,result_bytes);std::memset(b.data(),0,result_bytes);
        Result ra{},rb{};Node *head=length?chain.data():nullptr;
        const bool x=reference_assemble(ra,a.data(),head),y=assemble(rb,b.data(),head);
        check(x==y && ra.count==rb.count && ra.cost==rb.cost && a==b && a[length?length:result_bytes/4]==b[length?length:result_bytes/4],"full/failed path and native result-buffer boundary semantics");
    }
    check(add(INT32_MAX,1)==INT32_MIN && add(INT32_MIN,-1)==INT32_MAX,"native ADD wrapping");
    std::printf("{\"status\":\"path_fast_fixture_verified\",\"checks\":%u,\"random_cases\":%u,\"stateful_native_grid_cases\":4096,\"queue_operations\":20000,\"multi_waypoint_cases\":2000,\"open_improvements\":%u,\"closed_reopens\":%u,\"gameplay_verified\":false}\n",checks,cases,open_improvements,reopens);
    return 0;
}
