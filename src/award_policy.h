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
inline bool may_post(bool active,bool deployed,unsigned post,unsigned capacity,unsigned previous) {
    if(post==73 && deployed)return false;
    return !active || capacity>=previous;
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
