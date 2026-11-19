#pragma once
// 종군 중립군(무리) 지역 특색 (사용자 2026-11-18 "각 x주마다 무리 특색이 달랐으면", 역사 근거는 docs/NEUTRAL_BANDITS.md 12절).
// 순수 자료: 게임 호출·주소 없음. SAN14_CCZ.dll의 무리 판단(ccz_bands.inl)과 병종 그림(native_ccz_sprites.inl)이 같이 읽는다.
// 이름은 원작 범용 도적 견본(4031 "도적"+"장수", 4025 "남만"+"두목")처럼 성 = 무리 이름, 명 = 직함(무리마다 돌려 씀).
// 원작 이름 칸은 성 +0x12·명 +0x20 각 7 wchar(끝 0 포함, 243400/242F20 → 30FE90(...,7)), 표시는 230990 "%s%s".
namespace ccz_band_kinds {
struct Kind {
    const wchar_t *family;          // 성 칸(6자 이하), nullptr = 원작 이름 그대로(옛 기록)
    unsigned short cls;             // 조조전 병종(ccz_appearance_labels.h class_labels 번호)
    unsigned short troops_lo,troops_hi;   // 병력(+0x16), 0 = 원작 값 그대로
    unsigned char lead,war;         // 지휘 무장 통솔·무력 하한(+0xEC/+0xED), 0 = 원작 값 그대로
    const wchar_t *ranks[3];        // 명 칸(직함, 6자 이하)
};
inline constexpr Kind kinds[]={
    {nullptr,48,0,0,0,0,{nullptr,nullptr,nullptr}},                 // 0 산적(옛 기록·모름)
    {L"청주병",120,3500,5000,55,60,{L"두령",L"거수",L"교위"}},        // 1 연주
    {L"청주황건",84,3000,4500,45,55,{L"거수",L"대방",L"소방"}},       // 2 청주
    {L"해적",52,1500,2500,50,60,{L"두목",L"선장",L"두령"}},           // 3 청주 해안
    {L"호표기",127,1500,2500,70,75,{L"교위",L"도위",L"장수"}},        // 4 예주
    {L"여남황건",87,2500,4000,45,55,{L"거수",L"대방",L"소방"}},       // 5 예주 여남
    {L"백파적",187,3000,4500,50,60,{L"두령",L"거수",L"장수"}},        // 6 사례 하동
    {L"흉노기병",24,1500,2500,55,65,{L"대인",L"골도후",L"추장"}},     // 7 사례·병주
    {L"흑산적",48,3000,5000,50,55,{L"두령",L"두목",L"거수"}},         // 8 병주·기주 태항산
    {L"황건적",181,3000,5000,45,50,{L"거수",L"대방",L"소방"}},        // 9 기주 거록
    {L"백마의종",128,1500,2500,65,70,{L"교위",L"장수",L"도위"}},      // 10 유주
    {L"오환돌기",25,1500,2500,55,70,{L"대인",L"추장",L"두령"}},       // 11 유주·오환
    {L"단양병",122,2500,3500,60,65,{L"교위",L"도위",L"대장"}},        // 12 서주·양주
    {L"함진영",175,1000,1500,70,80,{L"교위",L"도위",L"장수"}},        // 13 서주 하비
    {L"수적",51,1500,2500,50,60,{L"두목",L"두령",L"장수"}},           // 14 회남·형주·양주 물가
    {L"금범적",174,800,1200,60,75,{L"두령",L"두목",L"유협"}},         // 15 형주북부 강하
    {L"무릉만",134,2500,4000,50,60,{L"대수",L"추장",L"두령"}},        // 16 형주남부
    {L"산월",49,3000,4500,50,55,{L"종수",L"대수",L"두령"}},           // 17 양주·회남 산지·산월
    {L"백이병",121,1500,2500,65,70,{L"교위",L"도위",L"장수"}},        // 18 익주
    {L"오두미도",189,2500,4000,50,45,{L"제주",L"치두",L"대제주"}},    // 19 익주 한중
    {L"남만병",134,3000,4500,50,60,{L"동주",L"대왕",L"두목"}},        // 20 남중·남만
    {L"등갑병",135,2000,3000,60,70,{L"동주",L"대왕",L"장수"}},        // 21 남중 오과국
    {L"맹수대",58,1500,2500,55,65,{L"동주",L"대왕",L"두목"}},         // 22 남중
    {L"상아대",133,1500,2500,55,65,{L"동주",L"대왕",L"두령"}},        // 23 교주
    {L"독사대",132,1500,2500,50,60,{L"동주",L"대왕",L"두령"}},        // 24 교주
    {L"서량기병",126,2000,3500,60,70,{L"장수",L"교위",L"도위"}},      // 25 량주·경조
    {L"강족기병",139,2000,3500,55,65,{L"호수",L"추장",L"대인"}},      // 26 량주·경조·강
    {L"낙타기병",129,1500,2000,55,60,{L"대인",L"추장",L"두령"}},      // 27 량주
    {L"선비기병",31,1500,2500,55,65,{L"대인",L"추장",L"두령"}},       // 28 선비
};
constexpr unsigned kKinds=sizeof(kinds)/sizeof(kinds[0]);
inline unsigned class_of(unsigned kind) {return kinds[kind<kKinds?kind:0].cls;}
// 2026-11-20 사용자 재설계 (「정예는 시작사기는 같은데 최대 사기가 130으로 설정되서 2.5배까지」 / 「수군은 그냥 수상부대를 누선으로
// 업그레이드」 / 「산악은 장사진 고정에 +30%버프」). 나머지 종류는 특성 없음(사용자 재설계 대기).
//  정예: 호표기 4, 백마의종 10, 단양병 12, 함진영 13, 백이병 18 — 최대 사기 130(원작 230460 답), 사기 배율 상한 2.5배.
//  수군: 해적 3, 수적 14, 금범적 15 — 수상 진형 누선(13, 원작 2436C0).
//  산악: 흑산적 8, 무릉만 16, 산월 17, 남만병 20 — 육상 진형 장사(6, 원작 242EF0) 고정, 공통 배율 x1.3.
enum Trait : unsigned {kPlain=0,kElite=1,kNavy=2,kMountain=3};
inline unsigned trait_of(unsigned kind) {
    switch(kind) {
    case 4:case 10:case 12:case 13:case 18:return kElite;
    case 3:case 14:case 15:return kNavy;
    case 8:case 16:case 17:case 20:return kMountain;
    default:return kPlain;
    }
}
// 2026-11-20 사용자 「등갑병 등 그런거 추천해준것도 다 구현해」 + 「청주병,서량기병에 한해서 성앞에 대기」: 제안서(5절) 추천 특성.
// 사용자가 다시 정한 정예·수군·산악은 위 trait_of 그대로이고, 아래는 그 밖의 추천을 겹쳐 붙이는 비트다(한 종류가 여럿 가질 수 있음).
//  오합지졸: 청주황건 2, 여남황건 5, 백파적 6, 황건적 9 — 격파·궤멸 사기 변화 1.5배, 대기 성장 최대 병력 130%·기간 70%.
//  정예(추가분): 사기 하락 절반, 최대 병력 70%.
//  기병: 흉노 7, 오환 11, 서량 25, 강족 26, 낙타 27, 선비 28 — 공격 때 평지·가도·모래 x1.2, 숲·밀림·산·산길 x0.8(일반 공격).
//  등갑: 등갑병 21 — 일반 공격 피해 x0.7, 불 피해 x3.
//  위압: 맹수대 22, 상아대 23 — 하루 한 번 바로 옆(1칸) 적 부대 사기 -3.
//  혼란: 황건적 9(요술), 독사대 24(독) — 일반 공격이 맞으면 10% 확률로 대상 혼란 +40.
//  치료: 오두미도 19 — 하루 한 번 2칸 안 아군 부대(자신 포함) 부상병 10%(최소 100)를 병력으로.
//  편입(성 앞 대기): 청주병 1, 서량기병 25 — 주인을 잃으면 성 앞에서 기다렸다가 다음 출진 장수를 따름. 나머지는 원래대로 해체.
enum TraitBit : unsigned {kRabble=1,kCavalry=2,kArmour=4,kAwe=8,kConfuse=16,kHeal=32,kWaits=64};
inline unsigned traits(unsigned kind) {
    switch(kind) {
    case 1:return kWaits;
    case 2:case 5:case 6:return kRabble;
    case 9:return kRabble|kConfuse;
    case 7:case 11:case 26:case 27:case 28:return kCavalry;
    case 25:return kCavalry|kWaits;
    case 21:return kArmour;
    case 22:case 23:return kAwe;
    case 24:return kConfuse;
    case 19:return kHeal;
    default:return 0;
    }
}
inline bool waits_at_base(unsigned kind) {return (traits(kind)&kWaits)!=0;}
// Kill/rout morale shock of one band after the [ccz] band_morale preset: rabble x1.5 both ways, elite falls at half.
inline int shock_for(unsigned kind,int delta) {
    if(traits(kind)&kRabble)return delta*3/2;
    if(delta<0 && trait_of(kind)==kElite)return delta/2;
    return delta;
}
// Idle growth: full troops (percent of the [ccz] band_troops full) and days to full (30, rabble x0.7 = 21). 2026-11-20 user
// "매력으로 빨리 소집되는건 지워줘": the ruler 매력 (인덕) speed-up is gone.
inline unsigned troop_cap_pct(unsigned kind) {return (traits(kind)&kRabble)?130u:trait_of(kind)==kElite?70u:100u;}
inline unsigned growth_days(unsigned kind) {return (traits(kind)&kRabble)?21u:30u;}
// Terrain id = hex +0x10 (ccz_bands.inl dry_land): 1 평지 2 가도 3 모래 / 5 숲 6 밀림 8 저산 9 중산 10 고산 11 산길.
inline float cavalry_terrain(unsigned terrain) {
    if(terrain==1 || terrain==2 || terrain==3)return 1.2f;
    if(terrain==5 || terrain==6 || (terrain>=8 && terrain<=11))return 0.8f;
    return 1.0f;
}
// One direction of the normal exchange: attacker kind (0 = not a band) on its terrain, target kind (0 = not a band).
inline float exchange_factor(unsigned attacker_kind,unsigned attacker_terrain,unsigned target_kind) {
    float f=1.0f;
    if(traits(attacker_kind)&kCavalry)f*=cavalry_terrain(attacker_terrain);
    if(traits(target_kind)&kArmour)f*=0.7f;
    return f;
}
inline int fire_amount(unsigned kind,int amount) {
    if(!(traits(kind)&kArmour) || amount<=0)return amount;
    const long long a=static_cast<long long>(amount)*3;
    return a>20000?20000:static_cast<int>(a);
}
// Wounded -> troops for one army near a healer: 10% of the wounded, at least 100, never more than the wounded or the room left.
inline unsigned heal_amount(unsigned wounded,unsigned troops,unsigned cap) {
    if(!wounded || troops>=cap)return 0;
    unsigned h=wounded/10u;if(h<100u)h=100u;
    if(h>wounded)h=wounded;
    if(h>cap-troops)h=cap-troops;
    return h;
}
// Factor on the original common multiplier 2AAF30 whose morale term is (a + m) / b (m = min(+0x1A, 230460)). 2AAF30 machine code
// 2AB19F..2AB26F (1a4a447d, read 2026-11-20): a/b = 200/300; standard mode (3ED7C0) with 3EC480 == 1: 75/175; expert mode (3ED810):
// a = 200 - 200 x (min(setting 12, 200) - 100) / 100 (3EC920(7AB6A0(options), 12, 0); negative setting = 100 here). Only a matters
// for a ratio. Band morale curve relative to morale 50 (user 2026-11-20 「사기50에 비해서 사기 100은 최대 … 2배까지」, 「사기 50
// 미만은 약해져야지」): 0 -> 0.4, 50 -> 1, 100 -> 2 (cap, non-elite), 130 -> 2.5 (elite cap). At morale 50 a band equals the original.
inline float power_factor(unsigned kind,unsigned morale,float a=200.0f) {
    const bool elite=trait_of(kind)==kElite;
    unsigned m=morale;if(m>(elite?130u:100u))m=elite?130u:100u;
    float rel;
    if(m<=50)rel=0.4f+0.6f*static_cast<float>(m)/50.0f;
    else if(m<=100)rel=1.0f+static_cast<float>(m-50)/50.0f;
    else rel=2.0f+0.5f*static_cast<float>(m-100)/30.0f;
    // the original term relative to morale 50 (caller passes the same capped morale 2AAF30 used)
    const float num=a+static_cast<float>(morale),den=a+50.0f;
    float f=num>0.5f && den>0.5f?rel*den/num:rel;
    if(f>20.0f)f=20.0f;
    if(trait_of(kind)==kMountain)f*=1.3f;
    return f;
}
// 주(州) 번호: 원작 fixdata 표 0 `state`(23칸), 도시 표 `city` +0x34(San14Sim origlib로 읽음, 2026-11-18).
// 1 유주 2 병주 3 기주 4 청주 5 서주 6 회남 7 연주 8 예주 9 사례 10 경조 11 량주 12 양주 13 교주 14 형주북부 15 형주남부
// 16 익주 17 남중 18 오환 19 선비 20 강 21 산월 22 남만.
inline constexpr unsigned char city_state[52]={0,1,1,1,2,3,3,4,4,5,5,5,6,6,7,7,8,8,9,10,10,10,11,11,11,12,12,12,12,12,
    13,13,14,14,14,14,15,15,15,15,16,16,16,16,16,17,17,18,19,20,21,22};
// 관문(시설 번호 52~61 = 원작 관문 표 1~10, 표에 주 칸이 없어 역사 위치로 정함): 호관 병주, 호뢰관 사례, 동관 경조, 함곡관 사례,
// 무관 경조, 양평관·백수관·검각·부수관·면죽관 익주.
inline constexpr unsigned char gate_state[10]={2,9,10,9,10,16,16,16,16,16};
inline unsigned state_of_base(unsigned base) {
    if(base<52)return city_state[base];
    if(base-52<10)return gate_state[base-52];
    return 0;
}
struct Pick {unsigned char kind,weight;};
inline constexpr Pick pools[23][3]={
    {{0,10}},                       // 0 모름
    {{10,6},{11,4}},                // 1 유주: 백마의종, 오환돌기
    {{8,6},{7,4}},                  // 2 병주: 흑산적, 흉노기병
    {{9,6},{8,4}},                  // 3 기주: 황건적, 흑산적
    {{2,6},{3,4}},                  // 4 청주: 청주황건, 해적
    {{12,5},{13,5}},                // 5 서주: 단양병, 함진영
    {{14,6},{17,4}},                // 6 회남: 수적, 산월
    {{1,7},{9,3}},                  // 7 연주: 청주병, 황건적
    {{4,6},{5,4}},                  // 8 예주: 호표기, 여남황건
    {{6,6},{7,4}},                  // 9 사례: 백파적, 흉노기병
    {{26,5},{25,5}},                // 10 경조: 강족기병, 서량기병
    {{25,5},{26,3},{27,2}},         // 11 량주: 서량기병, 강족기병, 낙타기병
    {{12,4},{17,4},{14,2}},         // 12 양주: 단양병, 산월, 수적
    {{23,6},{24,4}},                // 13 교주: 상아대, 독사대
    {{15,5},{14,5}},                // 14 형주북부: 금범적, 수적
    {{16,7},{14,3}},                // 15 형주남부: 무릉만, 수적
    {{18,6},{19,4}},                // 16 익주: 백이병, 오두미도
    {{20,5},{21,3},{22,2}},         // 17 남중: 남만병, 등갑병, 맹수대
    {{11,10}},                      // 18 오환
    {{28,10}},                      // 19 선비
    {{26,10}},                      // 20 강
    {{17,10}},                      // 21 산월
    {{20,7},{21,3}},                // 22 남만
};
// roll = 0~999.
inline unsigned pick_kind(unsigned state,unsigned roll) {
    const Pick *p=pools[state<23?state:0];
    unsigned total=0;for(unsigned i=0;i<3;++i)total+=p[i].weight;
    if(!total)return 0;
    unsigned r=roll%1000*total/1000;
    for(unsigned i=0;i<3;++i){if(r<p[i].weight)return p[i].kind;r-=p[i].weight;}
    return p[0].kind;
}
}
