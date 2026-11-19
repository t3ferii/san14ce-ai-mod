#pragma once
// User/Claude 2026-10-25: each waypoint segment starts clean. No game access.
// S supplies the native preparation, six neighbours, costs and cleanup in the
// original call order. 1843E0 joins the independently searched segments.
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <algorithm>
#include <limits>

namespace path_fast_core {
constexpr unsigned cells=0xBD10, result_bytes=0x14A0;
// AI-CONTRACT [layout]: This is the x64 native node ABI, not a freely replaceable container. Keep offset/size assertions and signed-wrap semantics; validate clean waypoint segments, reopen behavior and bounded result assembly in path_fast_fixture.cpp.
struct Node {
    Node *left,*right,*parent;
    unsigned cell;
    unsigned char state,padding[3];
    std::int32_t g,f;
};
static_assert(sizeof(Node)==40 && offsetof(Node,parent)==0x10 &&
    offsetof(Node,state)==0x1C && offsetof(Node,f)==0x24,"native node ABI");
struct Result {unsigned *data;std::int32_t count,cost;};
static_assert(sizeof(Result)==16 && offsetof(Result,cost)==12,"native path ABI");
inline std::int32_t add(std::int32_t a,std::int32_t b) {
    const std::uint32_t bits=static_cast<std::uint32_t>(a)+static_cast<std::uint32_t>(b);
    std::int32_t value;std::memcpy(&value,&bits,4);return value; // x86 ADD wrapping
}
constexpr unsigned node_bytes=cells*sizeof(Node);
static_assert(node_bytes==0x1D8A80,"native slot bytes");
inline bool eligible(int n,const Node *buffer,int clear_bytes,int debug,unsigned start,unsigned goal,bool busy=false) {
    return n==1 && buffer && clear_bytes==0xBD10 && debug==0 && start<cells && goal<cells && !busy;
}
inline bool clean(const Node *buffer) {
    for(unsigned i=0;i<cells;++i)if(buffer[i].state)return false;
    return true;
}
inline void clear_delegate(Node *buffer,bool known_slot,bool skipped_clear) {
    if(known_slot && skipped_clear)std::memset(buffer,0,node_bytes);
}
// Only chain records are published; no heap/BST links or private pointers escape.
inline Node *materialize(Node *buffer,const Node *node) {
    Node *head=node?buffer+node->cell:nullptr;
    for(;node;node=node->parent) {
        Node value{};
        value.parent=node->parent?buffer+node->parent->cell:nullptr;
        value.cell=node->cell;value.state=node->state;
        value.g=node->g;value.f=node->f;
        buffer[node->cell]=value;
    }
    return head;
}
// local25 (user 2026-11-10 lag analysis: heap work was 20% of each path search): each heap slot carries its own
// key {f, insertion sequence} next to the node id, so a comparison no longer reads two 40-byte nodes. The order is
// the same as before (f, then the earlier insertion), so every pop and every result is unchanged.
struct HeapEntry {std::int32_t f;std::uint32_t sequence;unsigned id;};
struct Storage {
    std::vector<Node> nodes;
    std::vector<std::uint32_t> generations;
    std::vector<unsigned> positions;
    std::vector<HeapEntry> heap;
    std::vector<unsigned> queue,probe_queue;      // local268: FIFO order of uniform searches; start-region probe
    std::vector<std::uint32_t> probe_marks;
    std::uint32_t generation=0;
    std::uint32_t next_sequence=0;
    std::uint32_t probe_generation=0;
    explicit Storage(unsigned size=cells):nodes(size),generations(size),positions(size) {
        heap.reserve(size);queue.reserve(size);probe_queue.reserve(2048);
    }
    void begin() {
        heap.clear();next_sequence=0;
        if(++generation==0) {
            // Once per 2^32 calls, reset tags, not the 1.9MiB node buffer.
            std::fill(generations.begin(),generations.end(),0);generation=1;
        }
    }
    Node &touch(unsigned i) {
        if(generations[i]!=generation) {
            nodes[i]={};generations[i]=generation;
        }
        return nodes[i];
    }
    unsigned index(const Node *n) const {return static_cast<unsigned>(n-nodes.data());}
    static bool before(const HeapEntry &a,const HeapEntry &b) {
        return a.f<b.f || (a.f==b.f && a.sequence<b.sequence);
    }
    // Sift with a hole: same comparisons and the same final layout as swapping step by step.
    void up(unsigned i) {
        const HeapEntry v=heap[i];
        while(i) {
            const unsigned parent=(i-1)/2;
            if(!before(v,heap[parent]))break;
            heap[i]=heap[parent];positions[heap[i].id]=i;i=parent;
        }
        heap[i]=v;positions[v.id]=i;
    }
    void down(unsigned i) {
        const unsigned size=static_cast<unsigned>(heap.size());
        const HeapEntry v=heap[i];
        for(;;) {
            unsigned child=i*2+1;if(child>=size)break;
            if(child+1<size && before(heap[child+1],heap[child]))++child;
            if(!before(heap[child],v))break;
            heap[i]=heap[child];positions[heap[i].id]=i;i=child;
        }
        heap[i]=v;positions[v.id]=i;
    }
    void insert(Node *n) {
        const unsigned id=index(n);
        positions[id]=static_cast<unsigned>(heap.size());heap.push_back(HeapEntry{n->f,next_sequence++,id});up(positions[id]);
    }
    void erase(Node *n) {
        const unsigned i=positions[index(n)],last=static_cast<unsigned>(heap.size()-1);
        if(i!=last) {heap[i]=heap[last];positions[heap[i].id]=i;}
        heap.pop_back();
        if(i<heap.size()) {
            if(i && before(heap[i],heap[(i-1)/2]))up(i);else down(i);
        }
    }
    Node *pop() {Node *n=&nodes[heap[0].id];erase(n);return n;}
};

#ifndef NO_RUNTIME_LOG
// User 2026-10-25: the ordinal includes delegated calls and spans all threads.
inline bool verify_call(unsigned interval,std::uint64_t ordinal) {
    return interval && ordinal%interval==0;
}
inline bool elide_clear(unsigned interval) {return interval!=1;}
inline Node *verified_result(unsigned interval,bool mismatch,Node *original,Node *actual,Node *buffer) {
    return interval>1 && !mismatch?materialize(buffer,actual):original;
}
struct Comparison {
    unsigned original_length=0,new_length=0,first=cells,original_cell=cells,new_cell=cells;
    bool mismatch=false;
};
inline Comparison compare(const Node *original,const Node *actual) {
    Comparison out;
    for(unsigned i=0;(original || actual) && i<cells;++i) {
        if(original)++out.original_length;
        if(actual)++out.new_length;
        if((!original || !actual || original->cell!=actual->cell || original->g!=actual->g) && !out.mismatch) {
            out.first=i;out.original_cell=original?original->cell:cells;out.new_cell=actual?actual->cell:cells;
            out.mismatch=true;
        }
        if(original)original=original->parent;
        if(actual)actual=actual->parent;
    }
    // A legal chain cannot exceed the number of cells. Treat cycles as errors.
    if(original || actual)out.mismatch=true;
    return out;
}
inline unsigned packed_node(const Node &n) {
    return (static_cast<unsigned>(n.state)<<24)|(static_cast<unsigned>(n.g)&0xFFFFFF);
}
template<class Emit,class Neighbour> void mismatch_detail(std::uint64_t ordinal,unsigned start,unsigned goal,
        unsigned flags,unsigned ctx10,unsigned ctx28,unsigned ctx2c,int valid,int force,
        const Node *original,const Node *actual,const Node *buffer,const Storage &storage,
        const Comparison &difference,Emit emit,Neighbour neighbour) {
    emit("path_fast_mm",ordinal,start,goal);
    emit("path_fast_mm_flags",flags,ctx10,ctx28);
    emit("path_fast_mm_ctx",ctx2c,valid!=0,force);
    emit("path_fast_mm_cost",original?original->g:0,actual?actual->g:0,
         (difference.original_length<<16)|difference.new_length);
    emit("path_fast_mm_diff",difference.first,difference.original_cell,difference.new_cell);
    unsigned index=0;
    for(const Node *n=original;n && index<64;n=n->parent,++index)emit("path_fast_mm_orig",index,n->cell,n->g);
    index=0;
    for(const Node *n=actual;n && index<64;n=n->parent,++index)emit("path_fast_mm_new",index,n->cell,n->g);
    for(unsigned center:{difference.original_cell,difference.new_cell})if(center<cells)
        for(unsigned direction=1;direction<=6;++direction) {
            const unsigned id=neighbour(center,direction);
            // BD10 denotes an off-map neighbour or a missing chain element.
            const Node zero{};
            const Node &old_node=id<cells?buffer[id]:zero;
            const Node &new_node=id<cells && storage.generations[id]==storage.generation?storage.nodes[id]:zero;
            emit("path_fast_mm_nb",id,packed_node(old_node),packed_node(new_node));
        }
}
#endif

// Optional S::over(g): true drops a candidate whose accumulated cost is above the caller's limit (1861E0 keeps such cells
// out of the open list). Searches without the member are unchanged.
template<class S> auto over_limit(S &s,std::int32_t g,int)->decltype(s.over(g)) {return s.over(g);}
template<class S> bool over_limit(S &,std::int32_t,long) {return false;}
// local268 (Fable advice 2026-11-19), optional S members; searches whose S lacks them are unchanged.
//   uniform()        true: every step costs 10000 and f = g (flags without 1/4: no terrain cost, no heuristic, 187880 only
//                    scales with flags & 1). S::uniform_failed() is told when a step is not 10000; the search then ends
//                    without a result and the caller repeats it the ordinary way.
//   probe_after()    pops after which the start-region probe runs once (0 = never); S::probed(region) gets its answer.
template<class S> auto uniform_of(S &s,int)->decltype(s.uniform()) {return s.uniform();}
template<class S> bool uniform_of(S &,long) {return false;}
template<class S> auto uniform_failed_of(S &s,int)->decltype(s.uniform_failed()) {return s.uniform_failed();}
template<class S> void uniform_failed_of(S &,long) {}
template<class S> auto probe_after_of(S &s,int)->decltype(s.probe_after()) {return s.probe_after();}
template<class S> unsigned probe_after_of(S &,long) {return 0;}
template<class S> auto probed_of(S &s,unsigned region,int)->decltype(s.probed(region)) {return s.probed(region);}
template<class S> void probed_of(S &,unsigned,long) {}
constexpr unsigned probe_cap=256;
// Cells reachable from the start through allowed cells, with the search's own per-cell answers (tile / terrain / allowed;
// the goal is never tested and the start is exempt, as in search()). A route exists exactly when the goal is reached, so
// a region that closes without it proves "no route". Returns the closed region's size, or 0 when the goal was reached or
// the region grew past `cap` (open: say nothing).
template<class S> unsigned closed_region(Storage &storage,unsigned start,unsigned goal,unsigned flags,S &s,unsigned cap) {
    if(start>=cells || start==goal)return 0;
    if(storage.probe_marks.size()!=cells)storage.probe_marks.assign(cells,0);
    if(++storage.probe_generation==0) {std::fill(storage.probe_marks.begin(),storage.probe_marks.end(),0);storage.probe_generation=1;}
    const std::uint32_t mark=storage.probe_generation;
    std::vector<unsigned> &queue=storage.probe_queue;queue.clear();
    storage.probe_marks[start]=mark;queue.push_back(start);
    for(std::size_t head=0;head<queue.size();++head) {
        const unsigned c=queue[head];
        for(unsigned direction=1;direction<7;++direction) {
            const unsigned id=s.neighbour(c,direction);
            if(id>=cells || storage.probe_marks[id]==mark)continue;
            storage.probe_marks[id]=mark;
            if(id==goal)return 0;
            auto tile=s.tile(id);if(!tile)continue;
            std::int32_t cost=(flags&5)?s.terrain(tile):10000;
            if(!s.allowed(id,tile,cost))continue;
            if(queue.size()>=cap)return 0;
            queue.push_back(id);
        }
    }
    return static_cast<unsigned>(queue.size());
}
// Uniform searches (see uniform() above): the heap order {f, insertion sequence} with f = g = 10000 x steps is plain
// insertion order, and a cell once reached is never improved (its g is at most one step above the cell being expanded,
// so `candidate>=node->g` always rejects it). A FIFO queue gives the same pops, parents, g and result, and a reached cell
// is skipped before its callbacks (their answers are not used for it).
template<class S> Node *search_uniform(Storage &storage,unsigned start,unsigned goal,unsigned flags,S &s) {
    std::vector<unsigned> &queue=storage.queue;queue.clear();
    queue.push_back(goal);
    s.begin();
    Node *result=nullptr;
    const unsigned probe_at=probe_after_of(s,0);unsigned pops=0;
    for(std::size_t head=0;head<queue.size();++head) {
        if(probe_at && ++pops==probe_at) {
            const unsigned region=closed_region(storage,start,goal,flags,s,probe_cap);
            probed_of(s,region,0);
            if(region)break;
        }
        Node *current=&storage.nodes[queue[head]];
        if(current->cell==start) {
            result=current;
            for(Node *n=result;n;n=n->parent)n->state|=4;
            s.success();break;
        }
        bool broken=false;
        for(unsigned direction=1;direction<7;++direction) {
            const unsigned id=s.neighbour(current->cell,direction);
            if(id<cells && storage.generations[id]==storage.generation && (storage.nodes[id].state&3))continue;
            auto tile=s.tile(id);
            if(!tile || id>=cells)continue;
            std::int32_t cost=10000;
            if(id!=start && !s.allowed(id,tile,cost))continue;
            Node *node=&storage.touch(id);
            const std::int32_t step=s.adjust(cost,tile);
            if(step!=10000) {broken=true;break;}
            const std::int32_t candidate=add(current->g,step);
            if(over_limit(s,candidate,0))continue;
            node->cell=id;node->parent=current;node->state&=0xFD;
            s.coordinate(current->cell);
            node->g=candidate;node->f=candidate;
            node->state|=1;queue.push_back(id);
        }
        if(broken) {uniform_failed_of(s,0);result=nullptr;break;}
        current->state=static_cast<unsigned char>((current->state&0xFE)|2);
    }
    s.cleanup();return result;
}
template<class S> Node *search(Storage &storage,unsigned start,unsigned goal,unsigned flags,S &s) {
    storage.begin();
    Node *seed=&storage.touch(goal);
    // Every segment starts clean in private generation storage. With one goal,
    // zero g/parent/children are preserved; the original setup helper may reject.
    if(!s.prepare(storage.nodes.data(),start,goal,flags))return nullptr;
    if(!(flags&5) && uniform_of(s,0))return search_uniform(storage,start,goal,flags,s);
    storage.insert(seed);
    s.begin();
    Node *result=nullptr;
    const unsigned probe_at=probe_after_of(s,0);unsigned pops=0;
    while(!storage.heap.empty()) {
        if(probe_at && ++pops==probe_at) {
            const unsigned region=closed_region(storage,start,goal,flags,s,probe_cap);
            probed_of(s,region,0);
            if(region)break;   // closed: the full search would also end without a route
        }
        Node *current=storage.pop();
        if(current->cell==start) {
            result=current;
            for(Node *n=result;n;n=n->parent)n->state|=4;
            s.success();break;
        }
        for(unsigned direction=1;direction<7;++direction) {
            const unsigned id=s.neighbour(current->cell,direction);
            auto tile=s.tile(id); // original fetches cell 0 even for out-of-range IDs
            if(!tile || id>=cells)continue;
            std::int32_t cost=(flags&5)?s.terrain(tile):10000;
            if(id!=start && !s.allowed(id,tile,cost))continue;
            Node *node=&storage.touch(id);
            const std::int32_t candidate=add(current->g,s.adjust(cost,tile));
            if(over_limit(s,candidate,0))continue;
            if((node->state&3) && candidate>=node->g)continue;
            node->cell=id;node->parent=current;node->state&=0xFD;
            s.coordinate(current->cell); // 1841E5, including already-open improvements
            const bool open=(node->state&1)!=0;
            if(open)storage.erase(node); // 18AE60: only private BST links changed
            node->g=candidate;node->f=(flags&5)?add(candidate,s.heuristic(id)):candidate;
            storage.insert(node);
            if(!open)node->state|=1;
        }
        current->state=static_cast<unsigned char>((current->state&0xFE)|2);
    }
    s.cleanup();return result;
}

// Same unbounded DWORD chain copy as 184500. The original has no 1320-cell
// length check. Callers/fixtures must provide the original destination region;
// introducing truncation at 14A0h would change original behaviour.
inline bool assemble(Result &result,unsigned *destination,Node *node) {
    result={destination,0,0};
    if(node) {
        result.cost=add(result.cost,node->g);
        do {destination[result.count++]=node->cell;node=node->parent;} while(node);
    }
    return result.count>0;
}
}
