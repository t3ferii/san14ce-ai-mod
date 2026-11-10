#pragma once
#include <array>
#include <vector>
#include <stdint.h>
#include <string.h>
#include "personnel_policy.h"
namespace awards {
constexpr unsigned MaxItems=4096;
using Inventory=std::array<uint16_t,MaxItems>;
inline unsigned slots(unsigned ability,unsigned bonus,unsigned effect) {
    unsigned result=ability>=1 && ability<=4 && bonus?1u<<(ability-1):0;
    if(effect==1)result|=32;
    if(effect==2)result|=16;
    return result;
}
inline bool may_award(bool active,unsigned mask,int strength,int equipped) {
    return !active || (mask && mask<=8 && !(mask&(mask-1)) && strength>equipped);
}
// local17 awards_v16: post 73 is allowed for a busy officer that is not deployed; off = v15.7.1 rule.
inline bool may_post(bool active,bool deployed,unsigned post,unsigned capacity,unsigned previous,bool release73=false) {
    if(post==73 && deployed)return false;
    if(post==73 && release73)return true;
    return !active || capacity>=previous;
}
// Retreat-certain horses (item ids 1..10) go to a fixed priority list first
// (user 2026-11-09). people is that list in priority order; horses is the
// force's horse list: stored ones are warehouse stock, the others are held by
// a serving officer (busy = award_active, priority = holder is on the list).
// A stored horse wins over a held one, then the lowest item id; a held horse
// is never taken from a priority officer or from a busy one.
struct HorsePerson {uint16_t id;bool has_horse;bool busy;};
struct HorseItem {uint16_t item;uint16_t holder;bool stored;bool busy;bool priority;};
struct HorsePick {bool found;uint16_t recipient;uint16_t item;unsigned source;};
inline HorsePick horse_pick(const std::vector<HorsePerson> &people,const std::vector<HorseItem> &horses) {
    for(const HorsePerson &p:people) {
        if(p.has_horse || p.busy)continue;
        int best=-1;unsigned source=0;
        for(unsigned i=0;i<horses.size();++i)
            if(horses[i].stored && (best<0 || horses[i].item<horses[best].item)) {best=static_cast<int>(i);source=1;}
        if(best<0)for(unsigned i=0;i<horses.size();++i) {
            const HorseItem &h=horses[i];
            if(!h.stored && !h.busy && !h.priority && h.holder!=p.id && (best<0 || h.item<horses[best].item)) {best=static_cast<int>(i);source=2;}
        }
        if(best<0)continue;
        return HorsePick{true,p.id,horses[best].item,source};
    }
    return HorsePick{false,0,0,0};
}
inline std::vector<uint8_t> encode(const Inventory &inventory,const std::array<uint8_t,32> &binding) {
    std::vector<uint8_t> bytes(40+MaxItems*2);
    memcpy(bytes.data(),"WHSE\1\0\0\0",8);memcpy(bytes.data()+8,binding.data(),32);
    for(unsigned i=0;i<MaxItems;++i) {bytes[40+i*2]=static_cast<uint8_t>(inventory[i]);bytes[41+i*2]=static_cast<uint8_t>(inventory[i]>>8);}
    return bytes;
}
inline bool decode(const uint8_t *bytes,size_t size,const std::array<uint8_t,32> &binding,Inventory &out) {
    if(!bytes || size!=40+MaxItems*2 || memcmp(bytes,"WHSE\1\0\0\0",8) || memcmp(bytes+8,binding.data(),32))return false;
    Inventory result={};
    for(unsigned i=0;i<MaxItems;++i) {unsigned owner=bytes[40+i*2]|unsigned(bytes[41+i*2])<<8;if(owner>6000)return false;result[i]=static_cast<uint16_t>(owner);}
    out=result;return true;
}
}
