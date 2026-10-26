#pragma once
#include <cmath>
#include <cstdint>

// 2026-10-26 user: incoming computer diplomacy, over-investment and one last reply.
namespace diplomacy_intel_core {
inline int probability(int score) {return score<0?0:score>100?100:score;}
inline int bounded(double score) {
    // Keep signed arithmetic (including 100-score) defined even with edited resources.
    constexpr int limit=100000000;
    return !std::isfinite(score)?0:score>limit?limit:score<-limit?-limit:static_cast<int>(score);
}
struct Coefficients {double amount_scale,divisor,multiplier,offset,rate_divisor,floor;};
inline bool usable(const Coefficients &c) {
    return c.amount_scale>0 && c.divisor>0 && c.multiplier>0 && c.offset>0 && c.rate_divisor>0 && c.floor>=0;
}
inline int score(int base,int amount,bool defending,const Coefficients &c) {
    if(amount<=0 || !usable(c))return base;
    const int native_base=probability(base);
    // 322B30 assembly: scale*amount/100 * multiplier/divisor /
    // (max(floor,base)/rate_divisor+offset). Native rounds at 4 decimals, then
    // truncates to integer. Use double through the division, avoiding its int32
    // intermediate overflow when the newly exposed investment is very large.
    double delta=c.amount_scale*static_cast<double>(amount)/100.0;
    delta=c.multiplier*delta/c.divisor;
    delta/=((c.floor>native_base?c.floor:native_base)/c.rate_divisor+c.offset);
    const double result=native_base+(defending?-delta:delta);
    return bounded(std::trunc(std::round(result*10000.0)/10000.0)+(base-native_base));
}
inline int preserve_reserve(int raw,int native_after) {
    return bounded(static_cast<double>(raw)-probability(raw)+native_after);
}
struct Round {
    bool extra=false,done=false,deferred=false;
    bool defer(bool feature_on,bool incoming,bool fixed) {
        if(!feature_on || !incoming || fixed || extra || done)return false;
        deferred=true;return true;
    }
    bool reopen() {if(!deferred || extra || done)return false;deferred=false;extra=true;return true;}
    void finish() {deferred=false;extra=false;done=true;}
};
}
