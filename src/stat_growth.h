#pragma once
#include <stdint.h>
namespace statgrowth {
inline uint64_t threshold(unsigned next,unsigned at99,unsigned at100) {
    const uint64_t step=at100>at99?at100-at99:1u;
    return static_cast<uint64_t>(at100)+step*(next>100?next-100:0);
}
inline unsigned advance(unsigned current,unsigned experience,unsigned at99,unsigned at100) {
    while(current<200 && threshold(current+1,at99,at100)<=experience)++current;
    return current;
}
}
