#pragma once
#ifndef NO_RUNTIME_LOG
#include <cstdint>
#include <cstddef>
#include <cstring>
namespace path_count_core {
using Hash=std::uint64_t;
constexpr Hash basis=0xcbf29ce484222325ull,prime=0x100000001b3ull;
constexpr unsigned capacity=4096,slots=8192,max_nodes=2000;
inline Hash bytes(Hash h,const void *data,std::size_t n) {
    const auto p=static_cast<const unsigned char *>(data);
    for(std::size_t i=0;i<n;++i)h=(h^p[i])*prime;
    return h;
}
inline Hash input(const void *ctx,std::uint32_t start,const int *goals,int count,std::uint32_t flags) {
    Hash h=bytes(basis,ctx,0x30);h=bytes(h,&start,sizeof(start));
    // Out-of-range counts are represented, never dereferenced or silently clamped.
    if(count>=0 && count<=64)h=bytes(h,goals,static_cast<unsigned>(count)*sizeof(int));
    h=bytes(h,&count,sizeof(count));return bytes(h,&flags,sizeof(flags));
}
struct Result {Hash hash;unsigned nodes;bool cycle,truncated,unreadable;};
// Reader supplies scalar parent/cell values. No game pointer is dereferenced here.
template<class Reader> Result result(std::uintptr_t node,Reader read) {
    if(!node)return {};
    std::uintptr_t seen[4096]={};Result r={basis,0,false,false,false};
    while(node && r.nodes<max_nodes) {
        unsigned slot=static_cast<unsigned>((node>>3)*prime)&4095u;
        while(seen[slot] && seen[slot]!=node)slot=(slot+1)&4095u;
        if(seen[slot]) {r.cycle=true;break;}
        seen[slot]=node;std::uintptr_t parent=0;std::uint32_t cell=0;
        if(!read(node,parent,cell)) {r.unreadable=true;break;}
        r.hash=bytes(r.hash,&cell,sizeof(cell));++r.nodes;node=parent;
    }
    r.truncated=node && r.nodes==max_nodes;
    // Distinguish partial/cyclic diagnostic observations from complete paths.
    if(r.cycle || r.truncated || r.unreadable) {
        const unsigned marker=r.cycle?1u:r.truncated?2u:3u;
        r.hash=bytes(r.hash,&marker,sizeof(marker));
    }
    return r;
}
struct Totals {std::uint64_t calls=0,repeats=0,repeat_us=0,same=0,different=0,total_us=0,overflow=0;};
struct Entry {Hash key=0,result=0;bool used=false;};
struct Force {
    Totals totals;unsigned used=0;Entry table[slots]={};
    void add(Hash key,Hash output,std::uint64_t us,bool readable=true) {
        ++totals.calls;totals.total_us+=us;
        if(!readable) {++totals.overflow;return;}
        unsigned slot=static_cast<unsigned>(key)&(slots-1);
        while(table[slot].used && table[slot].key!=key)slot=(slot+1)&(slots-1);
        if(table[slot].used) {
            ++totals.repeats;totals.repeat_us+=us;
            if(table[slot].result==output)++totals.same;else ++totals.different;
        } else if(used<capacity) {table[slot]={key,output,true};++used;}
        else ++totals.overflow;
    }
    Totals flush() {
        const Totals out=totals;totals={};used=0;
        for(auto &e:table)e.used=false;
        return out;
    }
};
struct Day {
    int day=0;bool valid=false;Totals totals;
    template<class Emit> void advance(int next,Emit emit) {
        if(valid && day!=next) {emit(day,totals);totals={};}
        day=next;valid=true;
    }
    void add(std::uint64_t us,bool repeat) {++totals.calls;totals.total_us+=us;if(repeat)++totals.repeats;}
};
}
#endif
