#pragma once
#include <algorithm>
#include <cmath>
#include <cwchar>

// Display-only values. Native prediction calls supply soldier damage; these
// helpers reproduce the read-only arithmetic surrounding structure damage.
namespace damage_preview {
struct Result { int dealt=0,received=0,durability=0;bool structure=false; };
inline int count(float value) {
    return std::isfinite(value) && value>0 ? static_cast<int>((std::min)(value,9999.0f)) : 0;
}
inline int percent(int value,int rate) { return static_cast<int>(static_cast<long long>(value)*rate/100); }
inline int breach(int base,int durability,int category,int force_bonus,int wall_bonus,bool can_finish) {
    int bonus=0;
    if(category>=2 && category<=4 && force_bonus>0)bonus=(std::max)(1,percent(base,force_bonus));
    int amount=base+bonus;
    if(category==3 && wall_bonus) {
        const int change=percent(base,wall_bonus);
        amount+=wall_bonus>0?(std::max)(1,change):(std::min)(-1,change);
        amount=(std::max)(1,amount);
    }
    return (std::max)(0,(std::min)(amount,durability-(can_finish?0:1)));
}
inline float facility_attack(int coefficient,int durability,int maximum) {
    if(coefficient<=0 || durability<=0 || maximum<=0)return 0;
    return (std::min)(1.0f,static_cast<float>(durability)/maximum)*coefficient*100.0f;
}
inline int facility_damage(int base,int policy,int class_bonus,int resistance) {
    base=(std::max)(1,base); // 191A65..191A7B
    return (std::max)(0,base+percent(base,policy)+percent(base,class_bonus)-percent(base,resistance));
}
inline void format(const Result &r,wchar_t (&dealt)[64],wchar_t (&received)[64]) {
    if(r.structure)std::swprintf(dealt,64,L"%d명 / %d",r.dealt,r.durability);
    else std::swprintf(dealt,64,L"%d명",r.dealt);
    std::swprintf(received,64,L"%d명",r.received);
}
}
