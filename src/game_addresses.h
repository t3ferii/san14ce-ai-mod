#pragma once
// Central original-game address table + runtime resolver (user 2026-11-05 patch resilience, docs/PATCH_RESILIENCE.md).
// Feature code never writes an original RVA: it writes GA(GA_<oldRVA>) and gets the RVA of the running build.
//   - known build (EXE TimeDateStamp in ga_builds): the precomputed per-build table (src/game_addresses_data.inl, generated).
//   - unknown build of the game EXE: signature / RTTI resolution (ga::resolve_image), unresolved ids read as 0.
//   - any other host (test fixtures, tools): the reference build table (ga_builds[0] = 1fbe6be7), or ga::select_build().
// An id that is unresolved (or deliberately denied for a changed function) reads 0; guards then fail and
// ga::file_ok()/split_address_gates() switch the dependent feature off. Resolution methods: see tools/gen_signatures.py.
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include <initializer_list>
#include <string>
#include <vector>
#include <unordered_map>
#include "game_addresses_data.inl"

namespace ga {
static const uint64_t kPreferredBase=0x140000000ull;
typedef void (*LogFn)(const char *kind,unsigned a,unsigned b,unsigned c);

struct State {
    unsigned vals[GA_COUNT];
    int build;                       // index into ga_builds, -1 unknown
    bool game_host,resolved_runtime; // game EXE host / signature resolver has filled vals
    unsigned resolved,unresolved;    // ids with a value / without (after the last fill)
    unsigned long long denied[GA_FILE_WORDS];   // source files that use an unresolved id
    unsigned resolve_ms10;           // last runtime resolution time in 0.1 ms
    unsigned generation;             // bumped whenever vals change (index caches)
    LogFn log;
};

inline void recount(State &s) {
    s.resolved=s.unresolved=0;++s.generation;memset(s.denied,0,sizeof(s.denied));
    for(unsigned i=0;i<GA_COUNT;++i) {
        if(s.vals[i]) {++s.resolved;continue;}
        ++s.unresolved;
        for(unsigned w=0;w<GA_FILE_WORDS;++w)s.denied[w]|=ga_users[i][w];
    }
}
inline void load_build(State &s,int build) {
    s.build=build;
    if(build>=0)memcpy(s.vals,ga_builds[build].vals,sizeof(s.vals));else memset(s.vals,0,sizeof(s.vals));
    recount(s);
}
inline int find_build(unsigned timestamp) {
    for(int i=0;i<2;++i)if(ga_builds[i].timestamp==timestamp)return i;
    return -1;
}
inline State &state() {
    static State s=[]{
        State t={};t.build=-1;
        unsigned ts=0;
        __try {
            auto base=reinterpret_cast<const unsigned char *>(GetModuleHandleW(nullptr));
            auto dos=reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
            if(dos->e_magic==IMAGE_DOS_SIGNATURE)ts=reinterpret_cast<const IMAGE_NT_HEADERS64 *>(base+dos->e_lfanew)->FileHeader.TimeDateStamp;
        } __except(EXCEPTION_EXECUTE_HANDLER) {ts=0;}
        wchar_t path[MAX_PATH]={};GetModuleFileNameW(nullptr,path,MAX_PATH);
        const wchar_t *name=wcsrchr(path,L'\\');name=name?name+1:path;
        t.game_host=_wcsnicmp(name,L"SAN14",5)==0;
        int b=find_build(ts);
        if(b>=0)load_build(t,b);
        else if(t.game_host)load_build(t,-1);      // unknown game build: unresolved until ga::resolve_image
        else {                                      // fixtures/tools: reference build, or SAN14_GA_BUILD=<index> (address fixtures)
            char env[8]={};
            int pick=GetEnvironmentVariableA("SAN14_GA_BUILD",env,sizeof(env))?atoi(env):0;
            load_build(t,pick>=0 && pick<2?pick:0);
        }
        return t;
    }();
    return s;
}
inline unsigned rva(unsigned id) {return state().vals[id];}
inline bool known(unsigned id) {return state().vals[id]!=0;}
inline void select_build(int build) {load_build(state(),build);}    // fixtures: choose ga_builds[build]
inline int build_index() {return state().build;}
inline const char *build_name() {int b=state().build;return b>=0?ga_builds[b].name:"unknown";}
inline const unsigned *table(unsigned *count) {if(count)*count=GA_COUNT;return state().vals;}
inline void set_log(LogFn f) {state().log=f;}
// Adopt the table another module (Layout) resolved. Only values for unresolved ids or an unknown build are taken.
inline void adopt(const unsigned *vals,unsigned count) {
    State &s=state();
    if(!vals||count!=GA_COUNT||s.build>=0||s.resolved_runtime)return;
    memcpy(s.vals,vals,sizeof(s.vals));s.resolved_runtime=true;recount(s);
}
// Guard hash of an original code range (FNV-1a 64 over the bytes): the expected value for the running build. The reference build
// (and fixtures, unknown builds) keep the source literal; the other known build looks the pair up (0 = range unresolved/changed).
inline unsigned long long hash_expect(unsigned long long reference) {
    int b=state().build;
    if(b<=0)return reference;
    unsigned lo=0,hi=ga_hash_count;
    while(lo<hi) {
        unsigned mid=(lo+hi)/2;
        if(ga_hash_pairs[mid][0]<reference)lo=mid+1;else hi=mid;
    }
    return lo<ga_hash_count && ga_hash_pairs[lo][0]==reference?ga_hash_pairs[lo][1]:reference;
}
// memcmp of original code at table id + byte offset against the bytes expected from the reference build. On the reference
// build (and fixtures) this is memcmp. On the other KNOWN build the exact bytes of that build (ga_expb: bytes -16..+32, or ..+160 for long guards, around the id,
// generated from its unpacked EXE) replace the expected bytes: nothing is ignored, so another mod's edit of any byte is seen. Only an
// UNKNOWN build ignores the relocatable fields (rel32 / RIP displacement, from the reference decode). 0 = unresolved id: never equal.
// 2026-11-05 local5 (docs/INSTALL_LOCAL2_TRIAGE.md section 13): on a known build with exact bytes (build index 1) a guard range that no
// exact-byte window covers silently fell back to the reference bytes with the reference wildcard map (name_lookup: a 206-byte entry array
// with a CALL rel32 at +143 against a 32-byte window). Every such fallback is recorded here so that the guard fixture can fail on it
// (tools/check_addresses.py (c2)); extend the window in tools/guard_windows.json.
inline std::vector<std::pair<unsigned,unsigned>> &uncovered() {static std::vector<std::pair<unsigned,unsigned>> v;return v;}
inline void note_uncovered(unsigned rv,size_t n) {
    for(const auto &u:uncovered())if(u.first==rv && u.second==n)return;
    uncovered().push_back({rv,static_cast<unsigned>(n)});
}
inline int mcmp(const unsigned char *code,unsigned id,unsigned off,const void *expected,size_t n) {
    unsigned rv=rva(id);
    if(!rv)return 1;
    const unsigned char *a=code+rv+off,*e=static_cast<const unsigned char *>(expected);
    int b=state().build;
    if(b==0 || (b<0 && !state().game_host))return memcmp(a,e,n);
    if(b==1 && off+n<=ga_expb_post_1a4a447d[id])return memcmp(a,ga_expb_pool_1a4a447d+ga_expb_off_1a4a447d[id]+16+off,n);
    if(b==1)note_uncovered(rv+off,n);
    unsigned wild=ga_wild[id];
    for(size_t k=0;k<n;++k) {
        unsigned bit=off+static_cast<unsigned>(k);
        if(bit<32 && ((wild>>bit)&1))continue;
        if(a[k]!=e[k])return a[k]<e[k]?-1:1;
    }
    return 0;
}
// Same as mcmp for an address computed by the caller (rows of guard tables, entry bytes at a table-derived RVA, 16-byte code blocks):
// on the second known build the exact bytes of that build (ga_expb, window -16..+32 around the nearest table id that covers the whole
// range) replace the expected bytes; on an unknown build the wildcard map of the nearest id at or below the address applies; no such
// id = plain memcmp.
inline int mcmp_at(const unsigned char *code,unsigned rv,const void *expected,size_t n) {
    if(!rv)return 1;
    const unsigned char *a=code+rv,*e=static_cast<const unsigned char *>(expected);
    State &s=state();
    if(s.build==0 || (s.build<0 && !s.game_host))return memcmp(a,e,n);
    static std::vector<std::pair<unsigned,unsigned>> index;static unsigned index_gen=~0u;
    if(index_gen!=s.generation) {
        index.clear();
        for(unsigned i=0;i<GA_COUNT;++i)if(s.vals[i] && ga_entries[i].kind<=1)index.push_back({s.vals[i],i});
        std::sort(index.begin(),index.end());index_gen=s.generation;
    }
    if(s.build==1) {
        // ids whose window [v-16,v+post) covers [rv,rv+n): value v in [rv+n-post, rv+16]; the nearest one wins
        const unsigned char *best=nullptr;unsigned best_d=~0u;
        auto lo=std::lower_bound(index.begin(),index.end(),std::make_pair(rv>=160?rv-160:0u,0u));
        for(auto it=lo;it!=index.end() && it->first<=rv+16;++it) {
            unsigned id=it->second,v=it->first,post=ga_expb_post_1a4a447d[id];
            if(rv+16<v || rv+n>v+post)continue;
            unsigned d=v>rv?v-rv:rv-v;
            if(d<best_d) {best_d=d;best=ga_expb_pool_1a4a447d+ga_expb_off_1a4a447d[id]+(rv+16-v);}
        }
        if(best)return memcmp(a,best,n);
        note_uncovered(rv,n);
    }
    unsigned wild=0,off=0;
    auto it=std::upper_bound(index.begin(),index.end(),std::make_pair(rv,~0u));
    if(it!=index.begin()) {--it;if(rv-it->first<32) {wild=ga_wild[it->second];off=rv-it->first;}}
    for(size_t k=0;k<n;++k) {
        unsigned bit=off+static_cast<unsigned>(k);
        if(bit<32 && ((wild>>bit)&1))continue;
        if(a[k]!=e[k])return a[k]<e[k]?-1:1;
    }
    return 0;
}
inline bool file_ok(unsigned file) {return (state().denied[file>>6]&(1ull<<(file&63)))==0;}
// every listed source file free of unresolved ids
inline bool files_ok(std::initializer_list<unsigned> files) {for(unsigned f:files)if(!file_ok(f))return false;return true;}

// ---------------------------------------------------------------- read-only observers (local11, docs/INSTALL_LOCAL2_TRIAGE.md section 18)
// San14Sim's version.dll replaces the first 5..11 bytes of 23 original functions plus the daily-progress entry by `E9 rel32` (+ NOP padding)
// whose target lies outside the game image. ga_observers (generated from tools/observer_hooks.json and both unpacked EXEs) lists those
// entries per known build with the ORIGINAL prefix bytes (identical in both builds, no relocation inside).
// An entry counts as OBSERVED only when all of these hold: image[rva]==E9, the jump target is outside [image,image+SizeOfImage), and the
// padding bytes up to the prefix length are NOP or INT3. Everything behind the prefix stays under the caller's own guard, which is computed with
// the prefix replaced by the original bytes (hash_ok / fnv_observed / the .text CRC of native_adapter.cpp), so a patch that lands inside the game,
// a prefix of another length and any changed byte behind the prefix are still mismatches.
// The tolerance is only for guards that IDENTIFY an original function (hash rows, the whole-text CRC). Code that PATCHES inside a prefix never
// uses it: observer_overlaps() tells it to leave the entry alone (conflict: that feature stays off, native_adapter.cpp prepare_link/prepare_bytes).
// ga::mcmp / ga::mcmp_at are deliberately NOT tolerant: modpack_write() uses mcmp_at as "the original is still there" before it writes.
struct ObserverSet {
    unsigned count;
    unsigned rva[GA_OBSERVER_COUNT];
    unsigned char len[GA_OBSERVER_COUNT];
    const unsigned char *bytes[GA_OBSERVER_COUNT];
};
inline unsigned image_size_of(const unsigned char *img) {
    unsigned n=0;
    __try {
        auto dos=reinterpret_cast<const IMAGE_DOS_HEADER *>(img);
        if(dos->e_magic==IMAGE_DOS_SIGNATURE) {
            auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64 *>(img+dos->e_lfanew);
            if(nt->Signature==IMAGE_NT_SIGNATURE)n=nt->OptionalHeader.SizeOfImage;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {n=0;}
    return n;
}
// E9 at the entry, target outside the image, NOP/INT3 padding to `len`.
inline bool observer_shape(const unsigned char *img,unsigned image_size,unsigned rva,unsigned len) {
    if(!rva || len<5 || rva>image_size || image_size-rva<len)return false;
    const unsigned char *p=img+rva;
    if(p[0]!=0xE9)return false;
    int rel;memcpy(&rel,p+1,4);
    const long long target=static_cast<long long>(rva)+5+rel;      // offset from the image base
    if(target>=0 && target<static_cast<long long>(image_size))return false;   // inside the game: another patch, not an observer
    for(unsigned k=5;k<len;++k)if(p[k]!=0x90 && p[k]!=0xCC)return false;
    return true;
}
// The observed entries of the running (known) build; empty for an unknown build, a missing header or an image without observers.
inline void observers(const unsigned char *img,ObserverSet &set) {
    set.count=0;
    const int b=state().build;
    if(b<0 || b>1 || !img)return;
    const unsigned size=image_size_of(img);
    if(!size)return;
    __try {
        for(unsigned i=0;i<GA_OBSERVER_COUNT;++i) {
            const GaObserver &o=ga_observers[i];
            if(!observer_shape(img,size,o.rva[b],o.len))continue;
            set.rva[set.count]=o.rva[b];set.len[set.count]=o.len;set.bytes[set.count]=o.bytes;++set.count;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {set.count=0;}
}
// FNV-1a 64 of image[rva,rva+n) with the displaced prefix of every observed entry replaced by its original bytes. `used` (optional)
// receives the observed entries that overlap the range.
inline unsigned long long fnv_observed(const unsigned char *img,unsigned rva,unsigned n,ObserverSet *used=nullptr) {
    ObserverSet set;observers(img,set);
    if(used)used->count=0;
    for(unsigned k=0;k<set.count && used;++k)
        if(set.rva[k]<rva+n && rva<set.rva[k]+set.len[k]) {
            used->rva[used->count]=set.rva[k];used->len[used->count]=set.len[k];used->bytes[used->count]=set.bytes[k];++used->count;
        }
    unsigned long long h=0xcbf29ce484222325ull;
    for(unsigned i=0;i<n;++i) {
        unsigned char c=img[rva+i];
        for(unsigned k=0;k<set.count;++k) {
            const unsigned at=rva+i-set.rva[k];
            if(rva+i>=set.rva[k] && at<set.len[k]) {c=set.bytes[k][at];break;}
        }
        h=(h^c)*0x100000001b3ull;
    }
    return h;
}
// Guard hash row: the plain FNV hash of the range equals `expected`, or does so once the observed prefixes are replaced by the original
// bytes (then `used` lists the tolerated entries; empty when the plain hash matched). rva 0 (unresolved id) never matches.
inline bool hash_ok(const unsigned char *img,unsigned rva,unsigned n,unsigned long long expected,ObserverSet *used=nullptr) {
    if(used)used->count=0;
    if(!rva)return false;
    unsigned long long h=0xcbf29ce484222325ull;
    for(unsigned i=0;i<n;++i)h=(h^img[rva+i])*0x100000001b3ull;
    if(h==expected)return true;
    ObserverSet local;
    ObserverSet &u=used?*used:local;
    return fnv_observed(img,rva,n,&u)==expected && u.count>0;
}
// true when a patch of [rva,rva+n) would touch the displaced prefix of an observed entry (a real conflict); `entry` = that entry
inline bool observer_overlaps(const unsigned char *img,unsigned rva,unsigned n,unsigned *entry=nullptr) {
    ObserverSet set;observers(img,set);
    for(unsigned k=0;k<set.count;++k)
        if(set.rva[k]<rva+n && rva<set.rva[k]+set.len[k]) {if(entry)*entry=set.rva[k];return true;}
    return false;
}

// ---------------------------------------------------------------- runtime resolver
struct Img {
    const unsigned char *base;uint64_t ptr_base;unsigned size;
    unsigned text_va,text_size,pdata_va,pdata_size;
    struct Sec {unsigned va,vsize,rsize;char name[9];};
    std::vector<Sec> secs;
    bool ok(unsigned rva,unsigned n) const {return rva<size && n<=size-rva;}
    unsigned u32(unsigned rva) const {unsigned v;memcpy(&v,base+rva,4);return v;}
    uint64_t u64(unsigned rva) const {uint64_t v;memcpy(&v,base+rva,8);return v;}
};
inline bool open_image(const unsigned char *base,uint64_t ptr_base,Img &im) {
    if(!base)return false;
    auto dos=reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE)return false;
    auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64 *>(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE)return false;
    im.base=base;im.ptr_base=ptr_base;im.size=nt->OptionalHeader.SizeOfImage;
    im.text_va=im.text_size=im.pdata_va=im.pdata_size=0;
    auto sec=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        Img::Sec s={sec[i].VirtualAddress,sec[i].Misc.VirtualSize,sec[i].SizeOfRawData,{}};
        memcpy(s.name,sec[i].Name,8);s.name[8]=0;
        im.secs.push_back(s);
        if(!strcmp(s.name,".text")) {im.text_va=s.va;im.text_size=s.rsize;}
        if(!strcmp(s.name,".pdata")) {im.pdata_va=s.va;im.pdata_size=s.vsize<s.rsize?s.vsize:s.rsize;}
    }
    return im.text_va && im.text_size && im.ok(im.text_va,im.text_size);
}
// function-start candidates: .pdata begin addresses + 16-aligned starts after 0xCC padding (same rule as tools/addrlib.py)
inline void candidates(const Img &im,std::vector<unsigned> &out) {
    out.clear();
    const unsigned char *t=im.base+im.text_va;
    if(im.pdata_va && im.ok(im.pdata_va,im.pdata_size))
        for(unsigned i=0;i+12<=im.pdata_size;i+=12) {
            unsigned b=im.u32(im.pdata_va+i);
            if(b && b>=im.text_va && b<im.text_va+im.text_size)out.push_back(b);
        }
    unsigned n=im.text_size;
    for(unsigned i=0;i<n;) {
        const unsigned char *p=static_cast<const unsigned char *>(memchr(t+i,0xCC,n-i));
        if(!p)break;
        unsigned s=static_cast<unsigned>(p-t),j=s;
        while(j<n && t[j]==0xCC)++j;
        if(j<n && ((im.text_va+j)&15)==0)out.push_back(im.text_va+j);
        i=j+1;
    }
    std::sort(out.begin(),out.end());out.erase(std::unique(out.begin(),out.end()),out.end());
}
struct Rtti {
    std::unordered_map<std::string,std::vector<unsigned>> td;    // needed class -> TD rvas
    std::unordered_map<unsigned,std::string> td_name;
    std::unordered_map<unsigned,std::pair<unsigned,unsigned>> cols;   // col rva -> (td rva, offset)
    std::unordered_map<unsigned,std::vector<unsigned>> vt_of_col;
    std::unordered_map<unsigned,unsigned> extent;                // vtable rva -> end rva
    bool built=false;
};
inline const Img::Sec *section(const Img &im,const char *name) {
    for(const auto &s:im.secs)if(!strcmp(s.name,name))return &s;
    return nullptr;
}
inline void build_rtti(const Img &im,Rtti &r,const std::vector<bool> &need) {
    r.built=true;
    for(unsigned i=0;i<GA_COUNT;++i)if(need[i])r.td[std::string(ga_names+ga_entries[i].a)];
    for(const char *sn:{".data",".rdata"}) {
        const Img::Sec *s=section(im,sn);
        if(!s || !im.ok(s->va,s->rsize))continue;
        const unsigned char *b=im.base+s->va;unsigned n=s->rsize;
        for(unsigned i=0;i+4<n;) {
            const unsigned char *p=static_cast<const unsigned char *>(memchr(b+i,'.',n-i-4));
            if(!p)break;
            unsigned k=static_cast<unsigned>(p-b);
            if(p[1]=='?' && p[2]=='A' && (p[3]=='V' || p[3]=='U')) {
                unsigned e=k;while(e<n && b[e])++e;
                if(e<n && e-k>6 && b[e-1]=='@' && b[e-2]=='@' && s->va+k>=16) {
                    std::string name(reinterpret_cast<const char *>(b+k),e-k);
                    auto it=r.td.find(name);
                    if(it!=r.td.end()) {it->second.push_back(s->va+k-16);r.td_name[s->va+k-16]=name;}
                }
                i=e;
            } else i=k+1;
        }
    }
    const Img::Sec *rd=section(im,".rdata");
    if(!rd || !im.ok(rd->va,rd->rsize))return;
    const unsigned char *b=im.base+rd->va;unsigned n=rd->rsize&~3u;
    unsigned td_lo=0xFFFFFFFFu,td_hi=0;
    for(const auto &t:r.td_name) {if(t.first<td_lo)td_lo=t.first;if(t.first>td_hi)td_hi=t.first;}
    for(unsigned i=3;i*4+4<=n;++i) {
        unsigned v;memcpy(&v,b+i*4,4);
        if(v<td_lo || v>td_hi || !r.td_name.count(v))continue;
        unsigned col=rd->va+(i-3)*4,o=col-rd->va;
        if(o+24>n)continue;
        unsigned f[6];memcpy(f,b+o,24);
        if(f[0]==1 && f[3]==v && f[5]==col)r.cols[col]={v,f[1]};
    }
    std::unordered_map<uint64_t,unsigned> colva;
    for(const auto &c:r.cols)colva[im.ptr_base+c.first]=c.first;
    unsigned nq=rd->rsize&~7u;
    uint64_t col_lo=~0ull,col_hi=0;
    for(const auto &c:colva) {if(c.first<col_lo)col_lo=c.first;if(c.first>col_hi)col_hi=c.first;}
    for(unsigned i=0;i*8+8<=nq;++i) {
        uint64_t v;memcpy(&v,b+i*8,8);
        if(v<col_lo || v>col_hi)continue;
        auto it=colva.find(v);
        if(it!=colva.end())r.vt_of_col[it->second].push_back(rd->va+i*8+8);
    }
    uint64_t lo=im.ptr_base+im.text_va,hi=lo+im.text_size;
    for(const auto &v:r.vt_of_col)for(unsigned vt:v.second) {
        unsigned k=0;
        while(vt-rd->va+8*(k+1)<=nq) {
            uint64_t q;memcpy(&q,b+(vt-rd->va)+8*k,8);
            if(q<lo || q>=hi)break;
            ++k;
        }
        r.extent[vt]=vt+8*k;
    }
}
inline unsigned rtti_slot(const Rtti &r,const char *name,unsigned off,unsigned slot) {
    auto it=r.td.find(name);
    if(it==r.td.end() || it->second.size()!=1)return 0;
    unsigned tdr=it->second[0],col=0,count=0;
    for(const auto &c:r.cols)if(c.second.first==tdr && c.second.second==off) {col=c.first;++count;}
    if(count!=1)return 0;
    auto v=r.vt_of_col.find(col);
    if(v==r.vt_of_col.end() || v->second.size()!=1)return 0;
    unsigned vt=v->second[0];
    auto e=r.extent.find(vt);
    if(e==r.extent.end() || vt+8*slot>=e->second)return 0;
    return vt+8*slot;
}
// Fill out[GA_COUNT] from the image. Returns the number of resolved ids. Never writes game memory.
#ifdef GA_PROFILE
static double g_phase[6];
static double now_ms() {LARGE_INTEGER f,c;QueryPerformanceFrequency(&f);QueryPerformanceCounter(&c);return c.QuadPart*1000.0/f.QuadPart;}
#define GA_PH(i) g_phase[i]=now_ms()
#else
#define GA_PH(i)
#endif
inline unsigned resolve_image(const unsigned char *base,uint64_t ptr_base,unsigned *out) {
    GA_PH(0);
    memset(out,0,sizeof(unsigned)*GA_COUNT);
    Img im;
    if(!open_image(base,ptr_base,im))return 0;
    std::vector<unsigned> cands;candidates(im,cands);
    GA_PH(1);
    const unsigned char *t=im.base+im.text_va;
    std::vector<unsigned char> done(GA_COUNT,0);
    // signatures: one pass over all candidates, bucketed by the first two literal bytes
    std::vector<std::vector<unsigned>> bucket(65536);
    std::vector<unsigned> hits(GA_COUNT,0),first(GA_COUNT,0);
    std::vector<bool> need_rtti(GA_COUNT,false);
    bool any_rtti=false;
    std::vector<uint64_t> p8(GA_COUNT,0),m8(GA_COUNT,0);   // first eight pattern bytes as one masked word (cheap pre-test)
    for(unsigned i=0;i<GA_COUNT;++i) {
        const GaEntry &e=ga_entries[i];
        if(e.method==1) {
            bucket[(ga_pool[e.a]<<8)|ga_pool[e.a+1]].push_back(i);
            for(unsigned k=0;k<8 && k<e.len;++k)if(!ga_pool[e.a+e.len+k]) {
                p8[i]|=static_cast<uint64_t>(ga_pool[e.a+k])<<(8*k);m8[i]|=0xFFull<<(8*k);
            }
        }
        else if(e.method==3) {need_rtti[i]=true;any_rtti=true;}
    }
    for(unsigned c:cands) {
        unsigned o=c-im.text_va;
        unsigned key=(t[o]<<8)|(o+1<im.text_size?t[o+1]:0);
        const std::vector<unsigned> &list=bucket[key];
        if(list.empty())continue;
        uint64_t word=0;
        bool fast=o+8<=im.text_size;
        if(fast)memcpy(&word,t+o,8);
        for(unsigned i:list) {
            if(fast && (word&m8[i])!=p8[i])continue;
            const GaEntry &e=ga_entries[i];
            const unsigned char *pat=ga_pool+e.a,*mk=pat+e.len;
            bool ok=true;
            for(unsigned k=0;k<e.len;++k) {
                if(mk[k])continue;
                unsigned char byte=o+k<im.text_size?t[o+k]:0;
                if(byte!=pat[k]) {ok=false;break;}
            }
            if(ok) {if(++hits[i]==1)first[i]=c;}
        }
    }
    for(unsigned i=0;i<GA_COUNT;++i)if(ga_entries[i].method==1) {done[i]=1;if(hits[i]==1)out[i]=first[i];}
    GA_PH(2);
    Rtti rt;
    if(any_rtti) {
        build_rtti(im,rt,need_rtti);
        for(unsigned i=0;i<GA_COUNT;++i)if(ga_entries[i].method==3) {
            done[i]=1;out[i]=rtti_slot(rt,ga_names+ga_entries[i].a,ga_entries[i].c,ga_entries[i].b);
        }
    }
    GA_PH(3);
    uint64_t lo=im.ptr_base+im.text_va,hi=lo+im.text_size;
    for(unsigned pass=0;pass<8;++pass) {
        bool progress=false;
        for(unsigned i=0;i<GA_COUNT;++i) {
            const GaEntry &e=ga_entries[i];
            if(done[i] || e.method==0)continue;
            if(e.method==4) {                                   // slotfn
                if(!done[e.a])continue;
                done[i]=1;progress=true;
                unsigned s=out[e.a];
                if(s && im.ok(s,8)) {uint64_t q=im.u64(s);if(q>=lo && q<hi)out[i]=static_cast<unsigned>(q-im.ptr_base);}
            } else if(e.method==2) {                            // mid
                if(!done[e.a])continue;
                done[i]=1;progress=true;
                unsigned c=out[e.a];
                if(!c || !im.ok(c,1))continue;
                unsigned o=c-im.text_va,wlen=im.text_size-o;if(wlen>e.c)wlen=e.c;
                const unsigned char *pat=ga_pool+e.b,*mk=pat+e.len;
                unsigned cnt=0,pos=0;
                for(unsigned p=0;p+e.len<=wlen;++p) {
                    bool ok=true;
                    for(unsigned k=0;k<e.len;++k)if(!mk[k] && t[o+p+k]!=pat[k]) {ok=false;break;}
                    if(ok) {if(++cnt==1)pos=p;else break;}
                }
                if(cnt==1)out[i]=c+pos+e.d;
            } else if(e.method==5) {                            // dataref
                if(!done[e.a])continue;
                done[i]=1;progress=true;
                unsigned c=out[e.a];
                if(!c || !im.ok(c+e.b,4))continue;
                int d;memcpy(&d,im.base+c+e.b,4);
                unsigned v=c+e.c+static_cast<unsigned>(d);
                if(v<im.size)out[i]=v;
            }
        }
        if(!progress)break;
    }
    GA_PH(4);
    unsigned n=0;for(unsigned i=0;i<GA_COUNT;++i)if(out[i])++n;
    return n;
}
// Runtime fill for an unknown game build (called once the image is readable). Returns resolved count; logs the elapsed time.
inline unsigned resolve_runtime(const unsigned char *base) {
    State &s=state();
    if(s.build>=0 || s.resolved_runtime)return s.resolved;
    LARGE_INTEGER f,a,b;QueryPerformanceFrequency(&f);QueryPerformanceCounter(&a);
    std::vector<unsigned> v(GA_COUNT);
    resolve_image(base,reinterpret_cast<uint64_t>(base),v.data());
    QueryPerformanceCounter(&b);
    s.resolve_ms10=static_cast<unsigned>((b.QuadPart-a.QuadPart)*10000/f.QuadPart);
    // ids marked table-only stay 0 (no signature): only entries with a method can be filled for an unknown build
    memcpy(s.vals,v.data(),sizeof(s.vals));s.resolved_runtime=true;recount(s);
    if(s.log)s.log("addr_resolve",s.resolve_ms10,s.resolved,s.unresolved);
    return s.resolved;
}
}   // namespace ga
#define GA(id) ::ga::rva(static_cast<unsigned>(id))
#define GH(hash) ::ga::hash_expect(hash)
