#pragma once
#include <stdint.h>
// Stats200 growth past 100 (native_modpack.inl modpack_extended_growth). The experience table has rows up to 100 only, so the
// need for level L = 101..200 is t100 + (L-100)*(t100-t99), with t99/t100 = that ability's need in rows 99/100.
// Formula and safety rules are those of the old 모음팩 v1.9 SAN14_Stats200.dll (the integrated patch's Stats200New R43 reads it the
// same way). Safety (2026-11-18): no growth past 100 when t99==0, t100<=t99, t100>=0xF0000000, the step exceeds 10,000,000
// (e.g. a x1000 "suppressed" table) or the need no longer fits 32 bits.
namespace statgrowth {
inline uint64_t threshold(unsigned next,unsigned at99,unsigned at100) {
    const uint64_t step=at100>at99?at100-at99:1u;
    return static_cast<uint64_t>(at100)+step*(next>100?next-100:0);
}
inline bool usable(unsigned at99,unsigned at100) {
    return at99!=0 && at100>at99 && at100<0xF0000000u && at100-at99<=10000000u;
}
inline unsigned advance(unsigned current,unsigned experience,unsigned at99,unsigned at100) {
    if(!usable(at99,at100))return current;
    while(current<200) {
        const uint64_t need=threshold(current+1,at99,at100);
        if(need>UINT32_MAX || need>experience)break;
        ++current;
    }
    return current;
}
}
