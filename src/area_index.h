#pragma once
#include <cstdint>
#include <vector>

// Pure cache policy: no game addresses, clock, date, locking or native calls.
namespace area_index_core {
constexpr unsigned kCells=48400,kAreas=500,kAreaStride=0x98;
struct Identity {
    uintptr_t world,first,last,area0;
    bool operator==(const Identity &b) const {
        return world==b.world && first==b.first && last==b.last && area0==b.area0;
    }
};
inline unsigned region(uintptr_t area,uintptr_t first) {
    if(!first || area<first)return 0;
    const uintptr_t delta=area-first;
    if(delta%kAreaStride || delta/kAreaStride<1 || delta/kAreaStride>kAreas)return 0;
    return static_cast<unsigned>(delta/kAreaStride);
}
struct Result {bool usable;int value;};
template<class Cell> class Cache {
    Identity identity_{};
    bool ready_=false;
    std::vector<Cell> lists_[kAreas+1];
public:
    bool matches(const Identity &id) const {return ready_ && identity_==id;}
    void invalidate() {ready_=false;}
    template<class ReadCell,class ReadRegion>
    void rebuild(const Identity &id,ReadCell cell,ReadRegion area) {
        ready_=false;
        for(auto &list:lists_)list.clear();
        // User decision 2026-10-25: cell region IDs are fixed map data unless
        // the map file changes. Rebuild on identity change, never on a new day.
        for(unsigned i=0;i<kCells;++i) {
            Cell c=cell(i);const unsigned n=area(c);
            if(n>=1 && n<=kAreas)lists_[n].push_back(c);
        }
        identity_=id;ready_=true;
    }
    template<class Native> Result evaluate(unsigned area,unsigned limit,Native &native) const {
        if(!ready_ || area<1 || area>kAreas)return {false,0};
        unsigned count=0;
        for(Cell c:lists_[area]) {
            if(native.region(c)!=area)return {false,0};
            auto building=native.building(c); // 22FB10, then 32F500, unchanged order
            if(!native.valid(building))continue;
            auto type=native.type(building); // live table, never a cached facility count
            if(!native.valid(type))continue;
            const unsigned kind=native.kind(type);
            if(kind==2 || kind-3u<2u)++count;
        }
        return {true,limit<=count?1:0};
    }
};
}
