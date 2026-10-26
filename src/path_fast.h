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
struct Storage {
    std::vector<Node> nodes;
    std::vector<std::uint32_t> generations;
    std::vector<unsigned> positions,heap;
    std::vector<std::uint64_t> sequence;
    std::uint32_t generation=0;
    std::uint64_t next_sequence=0;
    explicit Storage(unsigned size=cells):nodes(size),generations(size),positions(size),sequence(size) {
        heap.reserve(size);
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
    bool before(unsigned a,unsigned b) const {
        return nodes[a].f<nodes[b].f || (nodes[a].f==nodes[b].f && sequence[a]<sequence[b]);
    }
    void exchange(unsigned a,unsigned b) {
        const unsigned v=heap[a];heap[a]=heap[b];heap[b]=v;
        positions[heap[a]]=a;positions[heap[b]]=b;
    }
    void up(unsigned i) {
        while(i && before(heap[i],heap[(i-1)/2])) {exchange(i,(i-1)/2);i=(i-1)/2;}
    }
    void down(unsigned i) {
        const unsigned size=static_cast<unsigned>(heap.size());
        for(;;) {
            unsigned child=i*2+1;if(child>=size)return;
            if(child+1<size && before(heap[child+1],heap[child]))++child;
            if(!before(heap[child],heap[i]))return;
            exchange(i,child);i=child;
        }
    }
    void insert(Node *n) {
        const unsigned id=index(n);sequence[id]=next_sequence++;
        positions[id]=static_cast<unsigned>(heap.size());heap.push_back(id);up(positions[id]);
    }
    void erase(Node *n) {
        const unsigned i=positions[index(n)],last=static_cast<unsigned>(heap.size()-1);
        if(i!=last)exchange(i,last);
        heap.pop_back();
        if(i<heap.size()) {
            if(i && before(heap[i],heap[(i-1)/2]))up(i);else down(i);
        }
    }
    Node *pop() {Node *n=&nodes[heap[0]];erase(n);return n;}
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

template<class S> Node *search(Storage &storage,unsigned start,unsigned goal,unsigned flags,S &s) {
    storage.begin();
    Node *seed=&storage.touch(goal);
    // Every segment starts clean in private generation storage. With one goal,
    // zero g/parent/children are preserved; the original setup helper may reject.
    if(!s.prepare(storage.nodes.data(),start,goal,flags))return nullptr;
    storage.insert(seed);
    s.begin();
    Node *result=nullptr;
    while(!storage.heap.empty()) {
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
