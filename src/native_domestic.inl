/* Automatic district domestic policy only. Manual commands share the native
 * batch executor, so its cost is overridden only in this thread's AI node. */
static __declspec(thread) unsigned in_auto_domestic;
using DomesticTick = int(__fastcall *)(Obj,int,int *);
static DomesticTick original_domestic_tick;

static Obj domestic_direct_district() {
    Obj root=world(),context=root?pointer(root,0x871E8):nullptr;
    return context?fn<Obj(__fastcall *)(Obj)>(0x32E8D0)(context):nullptr;
}
// User rule (v12.10): balanced development applies to delegated player corps
// and computer corps; the ruler's direct 1st corps only with [domestic]
// domestic_direct=1. Free appointment cost never applies to the 1st corps.
// The native automatic-domestic option (3329B0) does not decide either.
static bool domestic_direct_officer(Obj officer) {
    if(!valid(officer))return false;
    Obj controlled=domestic_direct_district();
    return controlled && table(0xE0D0,officer[0xE0],51)==controlled;
}
static unsigned balanced_development(Obj area) {
    unsigned selected=0;int best=5;
    const unsigned getters[3]={0x22E2B0,0x22C760,0x231170};
    for(unsigned i=0;i<3;++i) {
        int level=fn<int(__fastcall *)(Obj)>(getters[i])(area);
        if(level>=1 && level<best) {best=level;selected=i+1;}
    }
    return selected; // zero means all three are MAX; keep the officer.
}
static uint64_t __fastcall domestic_selection_node(Obj node,Obj context,Obj subject) {
    if(!enabled)return fn<Execute>(0x7A110)(node,context,subject);
    Obj store=fn<Obj(__fastcall *)(Obj)>(0xBF7A40)(context),area=nullptr;
    if(!fn<unsigned char(__fastcall *)(Obj,unsigned,Obj *,int)>(0xB26470)(store,dword(node,0x38),&area,0) ||
       !valid(area))return 2;
    if(!mod_features.domestic_direct && domestic_direct_officer(table(0x158,word(area,0x36),6000)))
        return fn<Execute>(0x7A110)(node,context,subject);
    unsigned selection=balanced_development(area);
    if(selection && area[0x72]==selection && valid(table(0x158,word(area,0x36),6000)))selection=0;
    bool written=fn<unsigned char(__fastcall *)(Obj,unsigned,unsigned)>(0xB26990)(store,dword(node,0x3C),selection)!=0;
    return written?1:2;
}
static uint64_t __fastcall domestic_officer_node(Obj node,Obj context,Obj subject) {
    auto original=fn<Execute>(0x36320);
    if(!enabled)return original(node,context,subject);
#if defined(PERSONNEL_CANDIDATE) || defined(INJURY_PROTECTION)
    if(!personnel_healthy_actor(node,context,0x3C))return 2;
    // 1st corps: native cost and budget path, whatever domestic_direct says.
    if(domestic_direct_officer(personnel_context_object(node,context,0x3C)))return original(node,context,subject);
#endif
    uint64_t result;
    ++in_auto_domestic;
    __try { result=original(node,context,subject); }
    __finally { --in_auto_domestic; }
    return result;
}
static unsigned __fastcall domestic_command_cost() {
    if(enabled && in_auto_domestic) {
        log_event("auto_domestic_free_cost",0,dword(image,0x1A483DC),0);
        return 0;
    }
    return dword(image,0x1A483DC);
}
static bool automated_domestic_district(Obj district,bool include_direct=false) {
    if(!valid(district))return false;
    Obj root=world(),context=root?pointer(root,0x871E8):nullptr;
    if(!context)return false;
    // Player-delegated corps and computer corps; the ruler's direct corps only on request.
    return include_direct || district!=domestic_direct_district();
}
static void synchronize_domestic(Obj city) {
    COST_SCOPE("synchronize_domestic");
    if(!valid(city) || word(city,0x4E)>51)return;
    auto district_getter=*reinterpret_cast<Obj(__fastcall **)(Obj)>(pointer(city,0)+0x80);
    Obj district=district_getter(city);
    if(!automated_domestic_district(district,mod_features.domestic_direct))return;
    auto owner_getter=*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(city,0)+0xB0);
    unsigned owner=owner_getter(city);
    if(!owner || owner>=52)return;
    for(unsigned id=1;id<=500;++id) {
        Obj area=table(0x6CAF0,id,500);
        if(!valid(area) || area[0x35]!=word(city,0x4E) ||
           fn<unsigned(__fastcall *)(Obj)>(0x22CC00)(area)!=owner)continue;
        Obj officer=table(0x158,word(area,0x36),6000);
        if(!valid(officer) || table(0xE0D0,officer[0xE0],51)!=district)continue;
        unsigned selection=balanced_development(area);
#if defined(PERSONNEL_CANDIDATE) || defined(INJURY_PROTECTION)
        if(!personnel::may_auto_work(officer[0x161])) {
#ifdef INJURY_PROTECTION
            if(injury_auto_district(district))selection=0;
#else
            selection=0;
#endif
        }
#endif
        if(area[0x72]!=selection) {
            area[0x72]=static_cast<unsigned char>(selection);
            // Native 27A660 also changes only Area+72 for an existing officer.
            log_event("auto_domestic_cycle",id,selection,word(area,0x3A));
        }
    }
}
static int __fastcall domestic_tick(Obj city,int preview,int *output) {
    COST_SCOPE("domestic_tick");
    NativePhase phase("domestic_tick");
#ifdef AWARD_WAREHOUSE
    if(enabled && preview==0 && mod_features.awards) {
        if(mod_features.awards_ai)award_ai_city(city);else award_synchronize(); // warehouse sync only
    }
#endif
#ifdef INJURY_PROTECTION
    if(enabled && preview==0 && (mod_features.injury || mod_features.injury_herbal))injury_synchronize_city(city);
#endif
    // This tick hook is shared by domestic, awards and injury ([features] split).
    const bool domestic=enabled && preview==0 && mod_features.domestic;
    if(domestic)synchronize_domestic(city);
    int result;
    {COST_SCOPE("native_domestic_tick");result=original_domestic_tick(city,preview,output);}
    if(domestic)synchronize_domestic(city);
    return result;
}
