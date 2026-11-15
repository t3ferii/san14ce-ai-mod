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
// local179 (user 2026-11-15 "어떤 관문들은 관문헥스하고 인접한 칸이 1개가 아니라 여러개일 수 있어. 그걸 감지해서 그 헥스타일에
// 안행진을 더 배치", then "관문칸에 안행진으로 공격가능한 헥스는 동관은 저렇게 3칸이고 양평관, 검각은 1칸"): every stand pick()
// accepts, in pick()'s order but with the attack stands first (out may be null to count only); *front = the attack stands,
// one 안행 unit each. An attack stand lies across the gate from every attacker - the gate and impassable ground split
// the free hexes around it into sides (hexes linked through touching free hexes), an attacker's side being the one of the
// free hex(es) nearest to it - and within kRanged of an attacker. Checked on the original map (MapExCe.bin, terrain 17
// = gate tiles): 동관 3 (one attacker; 4 hexes touch its west wall), 양평관 1, 검각 1.
constexpr int kRanged=2;   // 안행 reach over the gate (assumed from the gate geometry above, not read from the game)
inline unsigned ranked(unsigned center,const unsigned *attackers,unsigned attacker_count,const unsigned *open,unsigned open_count,
                       unsigned *out,unsigned limit,unsigned *front=nullptr) {
    if(front)*front=0;
    if(center>=none || !attackers || !attacker_count || !open)return 0;
    unsigned nearest=none;int range=0;
    for(unsigned i=0;i<attacker_count;++i) {
        if(attackers[i]>=none)continue;
        const int d=distance(center,attackers[i]);
        if(nearest==none || d<range || (d==range && attackers[i]<nearest)) {nearest=attackers[i];range=d;}
    }
    if(nearest==none)return 0;
    const unsigned opposite=mirror(center,nearest);
    constexpr unsigned kMax=64;unsigned hex[kMax];int reach[kMax],to_opposite[kMax];unsigned count=0;
    for(unsigned k=0;k<open_count && count<kMax;++k) {
        const unsigned h=open[k];if(h>=none || h==center)continue;
        const int r=distance(h,nearest);
        if(r<=range)continue;   // the attacker's side of the gate
        bool contact=false,seen=false;
        for(unsigned i=0;i<attacker_count && !contact;++i)contact=attackers[i]<none && distance(h,attackers[i])<=1;
        for(unsigned j=0;j<count && !seen;++j)seen=hex[j]==h;
        if(contact || seen)continue;
        const int o=opposite<none?distance(h,opposite):0;
        unsigned at=count;   // insertion by (reach, distance to the mirror, hex)
        while(at>0 && (r<reach[at-1] || (r==reach[at-1] && (o<to_opposite[at-1] || (o==to_opposite[at-1] && h<hex[at-1]))))) {
            hex[at]=hex[at-1];reach[at]=reach[at-1];to_opposite[at]=to_opposite[at-1];--at;
        }
        hex[at]=h;reach[at]=r;to_opposite[at]=o;++count;
    }
    // Sides of all free hexes (not only the accepted ones): labels by touching free hexes.
    unsigned side[kMax];unsigned all[kMax];unsigned all_count=0;
    for(unsigned k=0;k<open_count && all_count<kMax;++k) {
        const unsigned h=open[k];if(h>=none || h==center)continue;
        bool seen=false;for(unsigned j=0;j<all_count && !seen;++j)seen=all[j]==h;
        if(!seen) {side[all_count]=all_count;all[all_count++]=h;}
    }
    for(bool changed=true;changed;) {
        changed=false;
        for(unsigned i=0;i<all_count;++i)for(unsigned j=0;j<all_count;++j)
            if(side[j]<side[i] && distance(all[i],all[j])==1) {side[i]=side[j];changed=true;}
    }
    auto side_of=[&](unsigned h) {for(unsigned j=0;j<all_count;++j)if(all[j]==h)return side[j];return kMax;};
    bool attacker_side[kMax]={};
    for(unsigned i=0;i<attacker_count;++i) {
        if(attackers[i]>=none)continue;
        int nearest_open=-1;
        for(unsigned j=0;j<all_count;++j) {const int d=distance(all[j],attackers[i]);if(nearest_open<0 || d<nearest_open)nearest_open=d;}
        for(unsigned j=0;j<all_count;++j)if(distance(all[j],attackers[i])==nearest_open)attacker_side[side[j]]=true;
    }
    unsigned attack[kMax],rest[kMax],attack_count=0,rest_count=0;
    for(unsigned j=0;j<count;++j) {
        const unsigned s=side_of(hex[j]);
        bool in_reach=false;
        for(unsigned i=0;i<attacker_count && !in_reach;++i)in_reach=attackers[i]<none && distance(hex[j],attackers[i])<=kRanged;
        if(s<kMax && !attacker_side[s] && in_reach)attack[attack_count++]=hex[j];else rest[rest_count++]=hex[j];
    }
    for(unsigned j=0;j<attack_count;++j)hex[j]=attack[j];
    for(unsigned j=0;j<rest_count;++j)hex[attack_count+j]=rest[j];
    if(front)*front=attack_count;
    for(unsigned j=0;out && j<count && j<limit;++j)out[j]=hex[j];
    return count<limit || !out?count:limit;
}
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
