#include "game_addresses.h"
/* Automatic district domestic policy only. Manual commands share the native
 * batch executor, so its cost is overridden only in this thread's AI node. */
static __declspec(thread) unsigned in_auto_domestic;
using DomesticTick = int(__fastcall *)(Obj,int,int *);
static DomesticTick original_domestic_tick;

static Obj domestic_direct_district() {
    Obj root=world(),context=root?pointer(root,0x871E8):nullptr;
    return context?fn<Obj(__fastcall *)(Obj)>(GA(GA_330B50))(context):nullptr;
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
    const unsigned getters[3]={GA(GA_22FD40),GA(GA_22E1A0),GA(GA_232BD0)};
    for(unsigned i=0;i<3;++i) {
        int level=fn<int(__fastcall *)(Obj)>(getters[i])(area);
        if(level>=1 && level<best) {best=level;selected=i+1;}
    }
    return selected; // zero means all three are MAX; keep the officer.
}
static void *next_domestic_selection,*next_domestic_officer;   // Layout manager chain (null: original)
static uint64_t __fastcall domestic_selection_node(Obj node,Obj context,Obj subject) { COST_SCOPE("domestic_selection_node");
    if(!enabled)return next_or<Execute>(next_domestic_selection,GA(GA_7B0B0))(node,context,subject);
    Obj store=fn<Obj(__fastcall *)(Obj)>(GA(GA_1137FF0))(context),area=nullptr;
    if(!fn<unsigned char(__fastcall *)(Obj,unsigned,Obj *,int)>(GA(GA_B28D20))(store,dword(node,0x38),&area,0) ||
       !valid(area))return 2;
    if(!mod_features.domestic_direct && domestic_direct_officer(table(0x158,word(area,0x36),6000)))
        return next_or<Execute>(next_domestic_selection,GA(GA_7B0B0))(node,context,subject);
    unsigned selection=balanced_development(area);
    if(selection && area[0x72]==selection && valid(table(0x158,word(area,0x36),6000)))selection=0;
    bool written=fn<unsigned char(__fastcall *)(Obj,unsigned,unsigned)>(GA(GA_B29250))(store,dword(node,0x3C),selection)!=0;
    return written?1:2;
}
static uint64_t __fastcall domestic_officer_node(Obj node,Obj context,Obj subject) { COST_CRUMB_HOT("domestic_officer_node",node);
    auto original=next_or<Execute>(next_domestic_officer,GA(GA_36E80));
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
static unsigned __fastcall domestic_command_cost() { COST_SCOPE("domestic_command_cost");
    if(enabled && in_auto_domestic) {
        log_event("auto_domestic_free_cost",0,dword(image,GA(GA_1A4B428)),0);
        return 0;
    }
    return dword(image,GA(GA_1A4B428));
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
           fn<unsigned(__fastcall *)(Obj)>(GA(GA_22E650))(area)!=owner)continue;
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
    // This tick hook is shared by its subscribers (domestic-tick service: awards, injury, domestic, in that order).
    const bool run=enabled && preview==0;
    unsigned ran=0;
    if(run)for(unsigned i=0;i<services::domestic_tick.count;++i)if(services::domestic_tick.live(i)) {
        ran|=1u<<i;
        if(services::domestic_tick.rows[i].fn.pre)services::domestic_tick.rows[i].fn.pre(city);
    }
    int result;
    {COST_SCOPE("native_domestic_tick");result=original_domestic_tick(city,preview,output);}
    for(unsigned i=0;i<services::domestic_tick.count;++i)if((ran&(1u<<i)) && services::domestic_tick.rows[i].fn.post)services::domestic_tick.rows[i].fn.post(city);
    return result;
}
static unsigned make_domestic_cost_bridge(Obj at) {
    // Called mid-function at native 1ED475, where the caller's RSP is aligned.
    // Preserve all volatile GPRs, XMM0..5 and flags. Only EBX is the new cost.
    unsigned n=0;
    const unsigned char push[]={0x9C,0x50,0x51,0x52,0x41,0x50,0x41,0x51,0x41,0x52,0x41,0x53,
                               0x48,0x81,0xEC,0x88,0,0,0};
    memcpy(at+n,push,sizeof(push));n+=sizeof(push);
    for(unsigned xmm=0;xmm<6;++xmm) {
        unsigned char save[]={0xF3,0x0F,0x7F,static_cast<unsigned char>(0x44+xmm*8),0x24,static_cast<unsigned char>(32+xmm*16)};
        memcpy(at+n,save,sizeof(save));n+=sizeof(save);
    }
    at[n++]=0x48;at[n++]=0xB8;
    *reinterpret_cast<void **>(at+n)=reinterpret_cast<void *>(&domestic_command_cost);n+=8;
    at[n++]=0xFF;at[n++]=0xD0;at[n++]=0x89;at[n++]=0xC3; // CALL RAX; MOV EBX,EAX
    for(unsigned xmm=0;xmm<6;++xmm) {
        unsigned char load[]={0xF3,0x0F,0x6F,static_cast<unsigned char>(0x44+xmm*8),0x24,static_cast<unsigned char>(32+xmm*16)};
        memcpy(at+n,load,sizeof(load));n+=sizeof(load);
    }
    const unsigned char pop[]={0x48,0x81,0xC4,0x88,0,0,0,0x41,0x5B,0x41,0x5A,0x41,0x59,0x41,0x58,0x5A,0x59,0x58,0x9D,0xC3};
    memcpy(at+n,pop,sizeof(pop));return n+sizeof(pop);
}
