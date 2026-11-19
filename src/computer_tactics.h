// local150 computer tactic order (user 2026-11-14 "컴퓨터군단의 전법 우선순위도 정해주자 1. 고유전법 2. 교역전법 (아수라진,
// 구갑대열, 안식사법) 3. 연노, 매성, 도발, 혼란, 급습, 분전, 저지, 돌격, 대갈, 제사, 경파 ~ 나머지 아무렇게나 (무력형)
// 4. 매성, 혼란, 업화, 화시, 도발, 저지, 대갈 ~ 나머지 아무렇게나"). Pure table shared by the computer learning
// (native_corps_domestic.inl ai_learn) and the computer departure tactic choice (native_sortie.inl ai_tactic_order).
// Tactic ids (San14Sim/data/tables/tactic.json): 1..26 common tactics, 31..130 and 140..183 officer-unique tactics,
// 131..136 tribe tactics, 137 아수라진, 138 안식사법, 139 구갑대열 (trade). Martial = 무력 >= 지력 (tactic_priority rule).
#pragma once
namespace ctac {
// The named head of each list, then the rest in an order picked here (the user left it free).
static constexpr unsigned kMartial[]={6,17,16,15,3,1,14,2,11,4,9,   5,13,10,12,8,7,18,20,19,25,26,24,21,22,23};
static constexpr unsigned kIntellect[]={17,15,13,5,16,14,11,   12,6,10,7,18,20,19,8,3,4,9,1,2,25,26,24,21,22,23};
static constexpr unsigned kNamedMartial=11,kNamedIntellect=7;
static constexpr unsigned kUnknown=200;
inline bool unique(unsigned id) {return (id>=31 && id<=130) || (id>=140 && id<=183);}
inline bool trade(unsigned id) {return id==137 || id==138 || id==139;}
inline bool tribe(unsigned id) {return id>=131 && id<=136;}
// Lower is better: 0 unique, 1 trade, 2 other tribe tactics, 10 + list position, kUnknown otherwise.
inline unsigned rank(unsigned id,bool martial) {
    if(unique(id))return 0;
    if(trade(id))return 1;
    if(tribe(id))return 2;
    const unsigned *list=martial?kMartial:kIntellect;
    const unsigned n=martial?sizeof(kMartial)/sizeof(unsigned):sizeof(kIntellect)/sizeof(unsigned);
    for(unsigned i=0;i<n;++i)if(list[i]==id)return 10+i;
    return kUnknown;
}
// The "나머지 아무렇게나" part: such a tactic may be dropped to learn another one (and relearned later) for the experience.
inline bool spare(unsigned id,bool martial) {return rank(id,martial)>=10+(martial?kNamedMartial:kNamedIntellect);}
// When an officer with 10 tactics may forget `worst` to learn `best`.
// local171 (user 2026-11-15 "시람, 강습 같은 수상 전용 전법은 한번 배워두고 지우면 안돼"): the water tactics (family 8: 24 구축,
// 25 강습, 26 시람) are never forgotten once learned.
inline bool naval(unsigned id) {return id>=24 && id<=26;}
inline bool replace(unsigned worst,unsigned best,bool martial) {
    if(unique(worst) || naval(worst))return false;
    return rank(worst,martial)>rank(best,martial) || (spare(worst,martial) && spare(best,martial));
}
// Up to three of `have` (count n) by rank, best first; out gets 0 for empty slots. Returns how many were chosen.
inline unsigned pick3(const unsigned *have,unsigned n,bool martial,unsigned out[3]) {
    out[0]=out[1]=out[2]=0;unsigned k=0;
    for(unsigned r=0;r<3;++r) {
        unsigned best=0,best_rank=kUnknown+1;
        for(unsigned i=0;i<n;++i) {
            const unsigned t=have[i];if(!t)continue;
            bool taken=false;for(unsigned j=0;j<k;++j)taken=taken || out[j]==t;
            if(taken)continue;
            const unsigned v=rank(t,martial);
            if(v<best_rank) {best_rank=v;best=t;}
        }
        if(!best)break;
        out[k++]=best;
    }
    return k;
}
// local226 (user 2026-11-18 "강력전법 1개 + 교역전법 2개로 3개 세트 ... (아수라진,안식사법,구갑대열) ... 교역전법이 없으면 강력전법만"):
// the strong tactic, then up to two of the officer's usable trade tactics in the order 137, 138, 139 (not the strong one again).
inline void strong_set(unsigned strong,const unsigned *have,unsigned n,unsigned out[3]) {
    out[0]=strong;out[1]=out[2]=0;unsigned k=1;
    for(unsigned id=137;id<=139 && k<3;++id) {
        if(id==strong)continue;
        for(unsigned i=0;i<n;++i)if(have[i]==id) {out[k++]=id;break;}
    }
}
}
