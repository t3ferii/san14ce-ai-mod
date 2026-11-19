// local273 (user 2026-11-20 "알림함이 수집하는 항목을 on off 하는것중에서 대사로만 수집하는게 있는지 보고 방금 포로탈출처럼 함수 기준으로 좀 더
// 정확하게 나누고 세분화"): the original function that sent a notice decides its topic. The four message entries (1F1E90, 1F1DA0, 1F2190,
// 844860) are entered by a jump from their prologue, so the return address the hook sees lies in the calling function. size = that
// function's length in the reference build (tools/ghidra_full.py func). Rows are the callers that run during progress (the UI callers
// run while the player acts and are never collected). This file is not an advisor_briefing gate file: an id the running build cannot
// resolve reads 0, its row never matches, and that notice falls back to its wording as before local273.
// flags: kSourceRefine = the wording picks within the function's group (등용 성공/실패/인재 발견, 사절 여정/사자 방문, 인사/혼인·의형제);
// kSourceNative = every line also plays as the original window (local251/252 sound-6 notices, the captive escape 2A1530 whose 10 of 52
// lines missed the wording keys); kSourceNativeAlarm = only the lines the local252 wording names (2D6460 keeps 24247-24249 in the box);
// kSourceNoCopy = the original window only, nothing stored (user 2026-11-20 "군량부족은 수집안하고 원작처럼 떠야해").
enum : unsigned char {kSourceRefine=1,kSourceNative=2,kSourceNativeAlarm=4,kSourceNoCopy=8};
struct BriefingSource {unsigned id;unsigned short size;Topic topic;unsigned char flags;};
static const BriefingSource briefing_sources[]={
    {GA_1DBD80,3300,Topic::Employ,kSourceRefine},{GA_1DCA70,2224,Topic::Employ,kSourceRefine},{GA_1DD4B0,5410,Topic::Employ,kSourceRefine},
    {GA_1E62F0,1086,Topic::MissionStop,0},{GA_1E7EF0,1287,Topic::MissionStop,0},{GA_1E6730,995,Topic::MissionStop,0},
    {GA_1DEBE0,1557,Topic::MissionStop,0},{GA_1E70A0,2235,Topic::MissionStop,0},{GA_1E7960,667,Topic::MissionStop,0},
    {GA_1E8400,1163,Topic::MissionStop,0},{GA_1E8890,977,Topic::MissionStop,0},{GA_1E6B20,733,Topic::Intel,0},{GA_1E6E00,659,Topic::Intel,0},
    {GA_1F9670,2703,Topic::Intel,0},{GA_1FA260,714,Topic::Intel,0},{GA_1FA640,2864,Topic::Plot,0},{GA_1FB170,5035,Topic::Plot,0},
    {GA_1FC530,5120,Topic::Plot,0},{GA_1FD930,5669,Topic::Plot,0},{GA_1FEF60,2115,Topic::Plot,0},{GA_1FF7B0,2364,Topic::Plot,0},
    {GA_2000F0,3049,Topic::Plot,0},{GA_200CE0,3062,Topic::Plot,0},{GA_1D9DC0,6386,Topic::Journey,kSourceRefine},
    {GA_1DB6C0,571,Topic::Journey,kSourceRefine},{GA_201FA0,896,Topic::Journey,0},{GA_202320,842,Topic::Journey,0},{GA_202670,848,Topic::Journey,0},
    {GA_2029C0,822,Topic::Journey,0},{GA_202D00,2085,Topic::Journey,0},{GA_81B8C0,4813,Topic::Journey,kSourceRefine},{GA_1DB900,885,Topic::Envoy,0},
    {GA_2762B0,578,Topic::Coalition,0},{GA_276500,771,Topic::Coalition,0},{GA_278A70,604,Topic::Coalition,0},{GA_278CD0,791,Topic::Coalition,0},
    {GA_3A1990,1046,Topic::Coalition,0},{GA_2A5140,3119,Topic::Coalition,kSourceNative},{GA_2A62B0,552,Topic::Coalition,0},
    {GA_301200,1415,Topic::Coalition,0},{GA_258840,1022,Topic::Coalition,0},{GA_2956A0,5114,Topic::Rise,0},{GA_2A6730,5317,Topic::Rise,0},
    {GA_2E8100,5426,Topic::Rise,kSourceNativeAlarm},{GA_28DF20,6636,Topic::Rise,0},{GA_285DB0,3396,Topic::Rise,0},
    {GA_2C3BD0,4450,Topic::Rise,kSourceNative},{GA_2D6460,7746,Topic::Rise,kSourceNativeAlarm},{GA_2F8960,7775,Topic::Emperor,0},
    {GA_429290,2492,Topic::Emperor,0},{GA_29FD90,2277,Topic::Diplomacy,0},{GA_2FEB10,850,Topic::Diplomacy,0},{GA_81FA70,11420,Topic::Diplomacy,0},
    {GA_27C5D0,3032,Topic::Diplomacy,0},{GA_2719C0,898,Topic::Diplomacy,0},{GA_2B94B0,7565,Topic::AttackRequest,kSourceNative},
    {GA_2EB9D0,2968,Topic::AttackRequest,0},{GA_2979B0,5841,Topic::Death,0},{GA_2DD8B0,1217,Topic::Death,0},{GA_277030,2102,Topic::Officer,0},
    {GA_29E8A0,918,Topic::Officer,kSourceRefine},{GA_3062E0,1860,Topic::Officer,kSourceRefine},{GA_2CF1A0,1664,Topic::Officer,0},
    {GA_26E5E0,416,Topic::Officer,0},{GA_279570,788,Topic::Officer,0},{GA_279890,778,Topic::Officer,0},{GA_278FF0,1398,Topic::Officer,0},
    {GA_822710,665,Topic::Officer,0},{GA_741A20,1291,Topic::Loyalty,kSourceNativeAlarm},{GA_8229B0,3281,Topic::Loyalty,0},
    {GA_2DF350,7755,Topic::Shortage,kSourceNative},{GA_2CEBB0,657,Topic::Shortage,kSourceNative|kSourceNoCopy},
    {GA_2CEE50,839,Topic::Shortage,kSourceNative|kSourceNoCopy},{GA_2BC5A0,2545,Topic::City,0},{GA_33EB40,1420,Topic::City,0},
    {GA_36F3F0,1174,Topic::City,0},{GA_2906E0,1912,Topic::Omen,0},{GA_291710,3786,Topic::Omen,0},{GA_2925E0,6549,Topic::Omen,0},
    {GA_290E60,2211,Topic::Omen,0},{GA_286B00,13903,Topic::Land,0},{GA_83CE10,5225,Topic::Land,0},{GA_25E7D0,2154,Topic::Bandit,0},
    {GA_2D4630,1597,Topic::Growth,0},{GA_7422F0,493,Topic::Growth,0},{GA_29A8B0,5069,Topic::Disaster,0},{GA_29C3C0,3269,Topic::Disaster,0},
    {GA_37A170,1097,Topic::Duel,0},{GA_2E3020,2228,Topic::Captive,kSourceNative},{GA_2A1530,3787,Topic::Captive,kSourceNative},
    {GA_2FA7C0,2049,Topic::Captive,0},{GA_2FC080,1891,Topic::Captive,0},{GA_2FC9D0,846,Topic::Captive,0},{GA_40ACF0,941,Topic::Proposal,0},
    {GA_40C060,1472,Topic::Proposal,0},{GA_40CC90,1551,Topic::Proposal,0},{GA_40D890,679,Topic::Proposal,0},{GA_40E650,1548,Topic::Proposal,0},
    {GA_40F310,618,Topic::Proposal,0},{GA_40FCE0,799,Topic::Proposal,0},{GA_4106C0,649,Topic::Proposal,0},{GA_411100,716,Topic::Proposal,0},
    {GA_411BD0,716,Topic::Proposal,0},{GA_4126C0,1056,Topic::Proposal,0},{GA_413250,1226,Topic::Proposal,0},{GA_413DD0,583,Topic::Proposal,0},
    {GA_4147F0,894,Topic::Proposal,0},{GA_415220,758,Topic::Proposal,0},{GA_415D70,808,Topic::Proposal,0},{GA_416AF0,807,Topic::Proposal,0},
    {GA_417FD0,806,Topic::Proposal,0},{GA_418F50,692,Topic::Proposal,0},{GA_41E990,705,Topic::Proposal,0},
};
static Topic source_refine(Topic group,Topic worded) {
    switch(group) {
    case Topic::Employ:return worded==Topic::EmployFail || worded==Topic::Talent?worded:Topic::Employ;
    case Topic::Journey:return worded==Topic::Envoy?worded:Topic::Journey;
    case Topic::Officer:return worded==Topic::Bond?worded:Topic::Officer;
    default:return group;
    }
}
struct SourceHit {Topic topic=Topic::Count;unsigned char flags=0;unsigned rva=0;};
// The row whose function holds the return address; topic Count when none does (rva = the return address, 0 outside the image).
static SourceHit briefing_source(const void *ret) { COST_SCOPE("advisor_briefing:briefing_source");
    SourceHit hit;
    const auto at=reinterpret_cast<std::uintptr_t>(ret),base=reinterpret_cast<std::uintptr_t>(image);
    if(!ret || !image || at<=base || at-base>=0x10000000u)return hit;
    hit.rva=static_cast<unsigned>(at-base);
    for(const BriefingSource &s:briefing_sources) {
        const unsigned start=::ga::rva(s.id);
        if(start && hit.rva>start && hit.rva<=start+s.size){hit.topic=s.topic;hit.flags=s.flags;break;}
    }
    return hit;
}
// What a notice does: its topic, whether the original window plays too, whether nothing is stored. The table only adds windows:
// the local251/252 wording (capture, escape, alarm-sound lines) still plays natively for every caller, and the 군량 wording
// (24125/24126) is never stored whichever function sent it (user 2026-11-20 "군량부족은 수집안하고 원작처럼 떠야해").
struct SourceDecision {Topic topic=Topic::Other;bool native=false,no_copy=false;};
static SourceDecision source_decision(const SourceHit &source,const std::wstring &text,bool duel) {
    SourceDecision d;const bool known=source.topic!=Topic::Count;
    const Topic worded=topic_of_text(text);
    d.topic=duel?Topic::Duel:!known?worded:(source.flags&kSourceRefine)?source_refine(source.topic,worded):source.topic;
    d.native=(known && (source.flags&kSourceNative)) || native_scene_line(text);   // kSourceNativeAlarm = the wording itself
    d.no_copy=(known && (source.flags&kSourceNoCopy)) || food_shortage_line(text);
    return d;
}
