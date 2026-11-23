#pragma once
// [combat] wall_age_pick / wall_age_siege (user 2026-11-21, docs/NEUTRAL_WALL_AGE.md 2절 A-2): one table shared by the two DLLs.
// SAN14_CombatAI.dll owns and writes it (daily reconcile and the target counts, game AI thread, under `lock`) and exports it:
// SAN14CombatWallAge(kWallAgeVersion) (nullptr = other version or both switches off). SAN14_Convenience.dll only saves and
// restores it through its sidecar plugin (wallage, order 90). Without the convenience DLL the table lives in memory only.
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <vector>
static const unsigned kWallAgeVersion=1,kWallAgeIds=3001;
struct WallAgeRecord {
    uint16_t hex;      // building hex when recorded
    uint8_t kind;      // 9 토성 / 10 석벽 / 16 철쇄; 0 = empty
    uint8_t pad;
    uint16_t count;    // destroy orders the mod wrote for it
    uint16_t pad2;
    int32_t born;      // game_day() of the first day seen
};
struct WallAgeTable {
    unsigned size,version;
    SRWLOCK lock;
    volatile LONG epoch;     // +1 at each load (the convenience begin_load)
    LONG last_day;           // game_day() of the last reconcile, -1 = never
    WallAgeRecord rec[kWallAgeIds];
};
using GetWallAgeTable=WallAgeTable *(__cdecl *)(unsigned version);

// Sidecar <save>.<sha256>.wallage: "WAGE" + version 1 + 3 pad + 32-byte binding (the ambush sidecar head), last_day i32,
// count u32, then count entries of 12 bytes: id u16, hex u16, kind u8, pad u8, count u16, born i32. Pure, no game access.
namespace wall_age_codec {
constexpr size_t kHead=48,kEntry=12;
inline void put16(uint8_t *at,unsigned v) {at[0]=static_cast<uint8_t>(v);at[1]=static_cast<uint8_t>(v>>8);}
inline void put32(uint8_t *at,int32_t v) {memcpy(at,&v,4);}
inline unsigned get16(const uint8_t *at) {return static_cast<unsigned>(at[0])|(static_cast<unsigned>(at[1])<<8);}
inline int32_t get32(const uint8_t *at) {int32_t v;memcpy(&v,at,4);return v;}
inline std::vector<uint8_t> encode(const uint8_t *binding,int32_t last_day,const WallAgeRecord *rec,unsigned ids) {
    unsigned n=0;
    for(unsigned id=1;id<ids;++id)if(rec[id].kind)++n;
    std::vector<uint8_t> out(kHead+static_cast<size_t>(n)*kEntry,0);
    memcpy(out.data(),"WAGE",4);out[4]=1;memcpy(out.data()+8,binding,32);
    put32(out.data()+40,last_day);put32(out.data()+44,static_cast<int32_t>(n));
    uint8_t *e=out.data()+kHead;
    for(unsigned id=1;id<ids;++id) {
        const WallAgeRecord &r=rec[id];if(!r.kind)continue;
        put16(e,id);put16(e+2,r.hex);e[4]=r.kind;e[5]=0;put16(e+6,r.count);put32(e+8,r.born);e+=kEntry;
    }
    return out;
}
// Fills rec (cleared first) and last_day only when the head, the binding and the size match; false leaves rec cleared.
inline bool decode(const uint8_t *bytes,size_t size,const uint8_t *binding,int32_t &last_day,WallAgeRecord *rec,unsigned ids) {
    for(unsigned id=0;id<ids;++id)rec[id]=WallAgeRecord{};
    if(!bytes || size<kHead || memcmp(bytes,"WAGE",4) || bytes[4]!=1 || memcmp(bytes+8,binding,32))return false;
    const int32_t n=get32(bytes+44);
    if(n<0 || static_cast<size_t>(n)>ids || size!=kHead+static_cast<size_t>(n)*kEntry)return false;
    const uint8_t *e=bytes+kHead;
    for(int32_t i=0;i<n;++i,e+=kEntry) {
        const unsigned id=get16(e),hex=get16(e+2),kind=e[4];
        if(!id || id>=ids || hex>=48400 || (kind!=9 && kind!=10 && kind!=16))continue;
        WallAgeRecord &r=rec[id];r.hex=static_cast<uint16_t>(hex);r.kind=static_cast<uint8_t>(kind);r.count=static_cast<uint16_t>(get16(e+6));r.born=get32(e+8);
    }
    last_day=get32(bytes+40);
    return true;
}
}
