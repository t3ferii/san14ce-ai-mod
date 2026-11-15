#pragma once
// [combat] gate_intercept (user 2026-11-13 "아군의 관문이 공격당하면 관문소속성이나 관문에서 안행진이 2000명으로 나와서 적이 관문
// 공격하는 반대방향에서 세워놔야해. 적 - 관문 - 안행진", then "관문 1헥스 범위에 딱붙여야돼 ... 딱 붙는 상황이어야 안행진이
// 때리니까"). Pure hex geometry of the stand behind a gate (no game calls).
// Map hexes are y*220+x; odd columns stand half a hex higher (sortie::neighbor). In doubled coordinates
// (column x, row 2y+(x even)) the hex distance is the usual doubled-height formula and the mirror image of a hex about
// another is again a valid hex.
namespace intercept {
constexpr unsigned none=48400,width=220;
inline int distance(unsigned a,unsigned b) {
    const int ax=static_cast<int>(a%width),ay=static_cast<int>(a/width),bx=static_cast<int>(b%width),by=static_cast<int>(b/width);
    const int ar=2*ay+((ax&1)==0),br=2*by+((bx&1)==0);
    int dx=bx-ax,dy=br-ar;if(dx<0)dx=-dx;if(dy<0)dy=-dy;
    return dy>dx?dx+((dy-dx)>>1):dx;
}
// The hex opposite `from` with `center` in the middle (same distance from center), or none off the map.
inline unsigned mirror(unsigned center,unsigned from) {
    if(center>=none || from>=none)return none;
    const int cx=static_cast<int>(center%width),cy=static_cast<int>(center/width),fx=static_cast<int>(from%width),fy=static_cast<int>(from/width);
    const int x=2*cx-fx,row=2*(2*cy+((cx&1)==0))-(2*fy+((fx&1)==0));
    if(x<0 || x>=static_cast<int>(width))return none;
    const int y2=row-((x&1)==0);
    if(y2<0 || (y2&1))return none;
    const int y=y2/2;
    return y<static_cast<int>(width)?static_cast<unsigned>(y*static_cast<int>(width)+x):none;
}
// Behind the gate as seen from the attacker: the gate lies on a shortest way from the attacker to the hex.
inline bool behind(unsigned center,unsigned attacker,unsigned hex) {
    return distance(hex,attacker)==distance(hex,center)+distance(center,attacker);
}
// `open` = the free hexes touching the gate (the caller takes the hexes next to the gate's own tiles that are walkable,
// without building and army). The stand is one of them on the far side from the nearest attacker (farther from it
// than the gate center), never next to any attacker, and as near that attacker as possible so the 안행 reaches it over
// the gate; ties go to the exact mirror of the attacker, then to the lower hex. none = no stand.
inline unsigned pick(unsigned center,const unsigned *attackers,unsigned attacker_count,const unsigned *open,unsigned open_count) {
    if(center>=none || !attackers || !attacker_count || !open)return none;
    unsigned nearest=none;int range=0;
    for(unsigned i=0;i<attacker_count;++i) {
        if(attackers[i]>=none)continue;
        const int d=distance(center,attackers[i]);
        if(nearest==none || d<range || (d==range && attackers[i]<nearest)) {nearest=attackers[i];range=d;}
    }
    if(nearest==none)return none;
    const unsigned opposite=mirror(center,nearest);
    unsigned best=none;int best_reach=0,best_opposite=0;
    for(unsigned k=0;k<open_count;++k) {
        const unsigned h=open[k];if(h>=none || h==center)continue;
        const int reach=distance(h,nearest);
        if(reach<=range)continue;   // the attacker's side of the gate
        bool contact=false;
        for(unsigned i=0;i<attacker_count && !contact;++i)contact=attackers[i]<none && distance(h,attackers[i])<=1;
        if(contact)continue;
        const int to_opposite=opposite<none?distance(h,opposite):0;
        if(best==none || reach<best_reach || (reach==best_reach && (to_opposite<best_opposite || (to_opposite==best_opposite && h<best)))) {
            best=h;best_reach=reach;best_opposite=to_opposite;
        }
    }
    return best;
}
}
