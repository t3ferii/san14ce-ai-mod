#pragma once
#include <stdint.h>

namespace expert_combat {
struct Settings { unsigned base=100,region=4,tenth=0; };
constexpr uint32_t tag=0x53430000u;
constexpr uint32_t decimal_tag=0x53600000u;
inline bool decimal(uint32_t value) {return (value&0xffe00000u)==decimal_tag;}
inline bool tagged(uint32_t value) {return (value&0xffff0000u)==tag || decimal(value);}
inline bool encoded(uint32_t value) {
    if(decimal(value))return ((value>>11)&255)>=50 && ((value>>11)&255)<=200 &&
        (value&2047)<=2000 && (value&0x180000u)==0;
    return (value&0xffff0000u)==tag && ((value>>8)&255)>=50 &&
        ((value>>8)&255)<=200 && (value&255)<=200;
}
inline Settings decode(uint32_t value) {
    if(decimal(value) && encoded(value))return {(value>>11)&255,(value&2047)/10,(value&2047)%10};
    if(encoded(value))return {(value>>8)&255,value&255};
    // Old saves store a multiplier of the native 4% regional rate.
    if(value>=50 && value<=200)return {100,(value*4+50)/100};
    return {};
}
inline uint32_t encode(Settings value) {
    if(value.tenth)return decimal_tag | (value.base<<11) | (value.region*10+value.tenth);
    return tag | (value.base<<8) | value.region;
}
inline float multiplier(Settings value,int occupation_score) {
    return static_cast<float>(value.base)*0.01f+
        static_cast<float>(occupation_score)*(static_cast<float>(value.region)+value.tenth*0.1f)*0.0001f;
}
// User decision 2026-10-23: beginner/intermediate share advanced's presets.
inline unsigned preset_tenths(unsigned key,unsigned difficulty,unsigned choice) {
    if((key!=4 && key!=5) || difficulty>4 || choice>2)return 0;
    constexpr unsigned rates[3][3]={{40,250,460},{160,392,625},{240,490,740}};
    return rates[key==4 || difficulty<3?0:difficulty-2][choice];
}
}
