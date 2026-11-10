#pragma once
// Per-save memory of each officer's last sortie formation + tactics and the 「출진 기억」 flag (docs/TACTIC_PRIORITY_V2_PLAN.md 5). Pure.
// v2: "TPRM\2\0\0\0" + binding(32) + MaxPeople * {formation, tactic0, tactic1, tactic2, flags} = 30040 bytes. 0xFF = unset, flags bit0 = remember.
// v1 (read only): "TPRM\1\0\0\0" + binding(32) + MaxPeople * {formation, tactic0..2} = 24040 bytes, flags read as 0. Writing is always v2.
#include <array>
#include <vector>
#include <stdint.h>
#include <string.h>
namespace tactic_memory {
constexpr unsigned MaxPeople=6000;
constexpr unsigned EncodedSizeV1=40+MaxPeople*4;
constexpr unsigned EncodedSize=40+MaxPeople*5;
constexpr unsigned MaxFormation=20;
constexpr unsigned MaxTactic=200;
constexpr uint8_t kFlagRemember=1;
struct Record {
    uint8_t formation=0xFF;
    uint8_t tactics[3]={0xFF,0xFF,0xFF};
    uint8_t flags=0;
};
using Table=std::array<Record,MaxPeople>;
using Binding=std::array<uint8_t,32>;
inline bool empty(const Record &r) {return r.formation==0xFF && r.tactics[0]==0xFF && r.tactics[1]==0xFF && r.tactics[2]==0xFF && r.flags==0;}
// A new sortie record replaces formation and tactics, never the flags.
inline Record merge_record(const Record &old,uint8_t formation,const uint8_t *tactics) {
    Record r;r.formation=formation;for(unsigned k=0;k<3;++k)r.tactics[k]=tactics[k];r.flags=old.flags;return r;
}
inline std::vector<uint8_t> encode(const Table &table,const Binding &binding) {
    std::vector<uint8_t> bytes(EncodedSize);
    memcpy(bytes.data(),"TPRM\2\0\0\0",8);memcpy(bytes.data()+8,binding.data(),32);
    for(unsigned i=0;i<MaxPeople;++i) {
        uint8_t *at=bytes.data()+40+i*5;at[0]=table[i].formation;
        for(unsigned k=0;k<3;++k)at[1+k]=table[i].tactics[k];
        at[4]=table[i].flags;
    }
    return bytes;
}
// Accepts v1 (24040 bytes) and v2 (30040 bytes); magic version and size must agree. *version = 1 or 2 on success. out untouched on failure.
inline bool decode(const uint8_t *bytes,size_t size,const Binding &binding,Table &out,unsigned *version=nullptr) {
    if(!bytes || (size!=EncodedSize && size!=EncodedSizeV1))return false;
    const unsigned ver=size==EncodedSize?2:1,stride=ver==2?5:4;
    if(memcmp(bytes,"TPRM",4) || bytes[4]!=ver || bytes[5] || bytes[6] || bytes[7] || memcmp(bytes+8,binding.data(),32))return false;
    Table result;
    for(unsigned i=0;i<MaxPeople;++i) {
        const uint8_t *at=bytes+40+i*stride;
        if(at[0]!=0xFF && at[0]>MaxFormation)return false;
        result[i].formation=at[0];
        for(unsigned k=0;k<3;++k) {
            if(at[1+k]!=0xFF && (at[1+k]<1 || at[1+k]>MaxTactic))return false;
            result[i].tactics[k]=at[1+k];
        }
        if(ver==2) {if(at[4]&~kFlagRemember)return false;result[i].flags=at[4];}
    }
    out=result;if(version)*version=ver;return true;
}
}
