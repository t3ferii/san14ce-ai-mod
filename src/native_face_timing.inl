// [diagnostics] cost_profile only (2026-10-25, user: 0-day wait after 전략중 in 영웅난무). Times the
// original face setter 1B91E0(face node, person) at each of its 7 direct call sites; results go to the
// function_cost CSV as face_1B91E0_<site> (calls, total, max per 1 s window). No behaviour change.
// Evidence (REA carrier REA_Analysis/turnstart_20261025, (근거: 1a4a447d 이전 기록, docs/HISTORY.md)): 1B91E0 sets five face
// images (sizes 7/0x10/0x18/0x1E/0x20) through 9307B0; CPU samples of the 0-day wait put ~0.9 s of
// 4.7 s in its 9307B0->930840->872170->8A6DE0->8AB113 path. Army flag panels reach it via 1B3523.
// Sites (caller): 1990EF/199203 (198FA0), 199727 (1995E0), 19A0A1/19A0C1 (19A080), 1A95CB (1A957D
// chunk), 1B3523 (1B3450). Args: RCX, RDX only (1B91E0 prologue); callers ignore RAX.
#include "game_addresses.h"
namespace face_timing {
static const unsigned kFace=GA(GA_1BA4E0),kSites[7]={GA(GA_19A3BF),GA(GA_19A4D3),GA(GA_19A9F7),GA(GA_19B371),GA(GA_19B391),GA(GA_1AA8CB),GA(GA_1B4823)};
using Face=void(__fastcall *)(Obj,Obj);
static void __fastcall s0(Obj f,Obj p) {COST_SCOPE("face_1B91E0_1990EF");fn<Face>(kFace)(f,p);}
static void __fastcall s1(Obj f,Obj p) {COST_SCOPE("face_1B91E0_199203");fn<Face>(kFace)(f,p);}
static void __fastcall s2(Obj f,Obj p) {COST_SCOPE("face_1B91E0_199727");fn<Face>(kFace)(f,p);}
static void __fastcall s3(Obj f,Obj p) {COST_SCOPE("face_1B91E0_19A0A1");fn<Face>(kFace)(f,p);}
static void __fastcall s4(Obj f,Obj p) {COST_SCOPE("face_1B91E0_19A0C1");fn<Face>(kFace)(f,p);}
static void __fastcall s5(Obj f,Obj p) {COST_SCOPE("face_1B91E0_1A95CB");fn<Face>(kFace)(f,p);}
static void __fastcall s6(Obj f,Obj p) {COST_SCOPE("face_1B91E0_1B3523");fn<Face>(kFace)(f,p);}
static CallLink links[7];
// local159 (user 2026-11-14 "내정때문이라면 내정쪽 우리 모드에도 호출기달고 원작에도 달아서 원인 알아보자"): the 순-boundary stutter is
// one long original day frame (speed_frame:native 89-122 ms, nearly all outside the mod's scopes). Each case call of the day frame
// 44DEE0 (San14Sim/docs/orig/rules_time.md 2절) and the per-순 / month calls (305390 children, 2593E0, 42FDC0) get a timing wrapper
// at its E8 site, so the function_cost CSV and the long-frame record (native_cost_profile.h) name the original stage. The wrappers
// pass eight integer arguments and RAX through unchanged (no target takes float arguments or more than five arguments; 2BE030,
// with more, is left out). A site that is not the expected E8 (another hook owns it, e.g. 44E50D speed_buttons, 44E7D0 the
// season engine) is skipped and logged. Diagnostic builds with [diagnostics] cost_profile=1 only; no behaviour change.
struct DaySite {unsigned site,target;};
static const DaySite kDay[]={
    {GA(GA_44E332),GA(GA_2035D0)},{GA(GA_44E34D),GA(GA_4048D0)},{GA(GA_44E36A),GA(GA_405A40)},{GA(GA_44E37D),GA(GA_457C90)},
    {GA(GA_44E38F),GA(GA_458520)},{GA(GA_44E399),GA(GA_2E38E0)},{GA(GA_44E3A3),GA(GA_2E3BC0)},{GA(GA_44E3B6),GA(GA_2CCC60)},
    {GA(GA_44E3F7),GA(GA_2B2390)},{GA(GA_44E4B0),GA(GA_2DC550)},{GA(GA_44E4F6),GA(GA_43C9F0)},{GA(GA_44E517),GA(GA_303020)},
    {GA(GA_44E52A),GA(GA_2EA1D0)},{GA(GA_44E545),GA(GA_17B9E0)},{GA(GA_44E560),GA(GA_18F9D0)},{GA(GA_44E56A),GA(GA_28A890)},
    {GA(GA_44E57D),GA(GA_2EABE0)},{GA(GA_44E587),GA(GA_2FF450)},{GA(GA_44E59B),GA(GA_272720)},{GA(GA_44E5C8),GA(GA_2BDFB0)},
    {GA(GA_44E647),GA(GA_2A3CF0)},{GA(GA_44E650),GA(GA_2A3510)},{GA(GA_44E6E2),GA(GA_2E6BD0)},{GA(GA_44E6F4),GA(GA_18FB90)},
    {GA(GA_44E706),GA(GA_43E660)},{GA(GA_44E715),GA(GA_43F4C0)},{GA(GA_44E71C),GA(GA_4512A0)},{GA(GA_44E723),GA(GA_2901A0)},
    {GA(GA_44E728),GA(GA_301AF0)},{GA(GA_44E72F),GA(GA_2902C0)},{GA(GA_44E736),GA(GA_276250)},{GA(GA_44E743),GA(GA_2FEE70)},
    {GA(GA_44E74A),GA(GA_2D4630)},
    {GA(GA_431824),GA(GA_305390)},{GA(GA_30554D),GA(GA_297070)},{GA(GA_305555),GA(GA_27BF90)},{GA(GA_30555F),GA(GA_303240)},
    {GA(GA_3055A9),GA(GA_2E9640)},{GA(GA_44CC1D),GA(GA_2593E0)},{GA(GA_44ECFF),GA(GA_42FDC0)}};
constexpr unsigned kDayCount=sizeof(kDay)/sizeof(kDay[0]);
using Any8=uint64_t(__fastcall *)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
#define DAY_SITE(i,name) static uint64_t __fastcall d##i(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f,uint64_t g,uint64_t h) \
    {COST_SCOPE(name);return reinterpret_cast<Any8>(image+kDay[i].target)(a,b,c,d,e,f,g,h);}
DAY_SITE(0,"day_c00_events_2035D0") DAY_SITE(1,"day_c01_pre_4048D0") DAY_SITE(2,"day_c01_turn_block_405A40") DAY_SITE(3,"day_c02_status_457C90")
DAY_SITE(4,"day_c02_extra_458520") DAY_SITE(5,"day_c03_duel_cmd_2E38E0") DAY_SITE(6,"day_c04_orders_2E3BC0") DAY_SITE(7,"day_c05_duel_2CCC60")
DAY_SITE(8,"day_c07_collect_2B2390") DAY_SITE(9,"day_c08_move_2DC550") DAY_SITE(10,"day_c09_appear_43C9F0") DAY_SITE(11,"day_c0b_paint_303020")
DAY_SITE(12,"day_c0c_take_2EA1D0") DAY_SITE(13,"day_c0d_combat_17B9E0") DAY_SITE(14,"day_c0e_facility_18F9D0") DAY_SITE(15,"day_c0f_build_start_28A890")
DAY_SITE(16,"day_c10_build_2EABE0") DAY_SITE(17,"day_c11_traits_2FF450") DAY_SITE(18,"day_c12_morale_272720") DAY_SITE(19,"day_c13_wounded_2BDFB0")
DAY_SITE(20,"day_c14_fire_2A3CF0") DAY_SITE(21,"day_c14_fire_2A3510") DAY_SITE(22,"day_c15_demolish_2E6BD0") DAY_SITE(23,"day_c16_attack_18FB90")
DAY_SITE(24,"day_c17_fire_damage_43E660") DAY_SITE(25,"day_c18_poison_43F4C0") DAY_SITE(26,"day_c19_capture_4512A0") DAY_SITE(27,"day_c1a_list_2901A0")
DAY_SITE(28,"day_c1a_list_301AF0") DAY_SITE(29,"day_c1b_cleanup_2902C0") DAY_SITE(30,"day_c1c_options_276250") DAY_SITE(31,"day_c1d_traits_2FEE70")
DAY_SITE(32,"day_c1e_misc_2D4630")
DAY_SITE(33,"xun_city_305390") DAY_SITE(34,"xun_district_297070") DAY_SITE(35,"xun_27BF90") DAY_SITE(36,"xun_training_303240")
DAY_SITE(37,"xun_repair_2E9640") DAY_SITE(38,"xun_orders_2593E0") DAY_SITE(39,"month_events_42FDC0")
#undef DAY_SITE
static_assert(kDayCount==40,"one wrapper per day site");
static CallLink day_links[kDayCount];
static void install_day_sites() {
    static const Any8 wrappers[kDayCount]={d0,d1,d2,d3,d4,d5,d6,d7,d8,d9,d10,d11,d12,d13,d14,d15,d16,d17,d18,d19,
        d20,d21,d22,d23,d24,d25,d26,d27,d28,d29,d30,d31,d32,d33,d34,d35,d36,d37,d38,d39};
    bool use[kDayCount]={};unsigned wanted=0;
    for(unsigned i=0;i<kDayCount;++i) {const unsigned site=kDay[i].site,target=kDay[i].target;
        if(!site || !target || image[site]!=0xE8 || site+5+*reinterpret_cast<int *>(image+site+1)!=target) {log_event("day_timing_site_skipped",site,target,i);continue;}
        use[i]=true;++wanted;}
    if(!wanted)return;
    Obj relay=allocate_near(image+kDay[2].site);if(!relay) {log_event("day_timing_relay_rejected");return;}
    DWORD old;bool ok=true;
    for(unsigned i=0;i<kDayCount && ok;++i)if(use[i]) {absolute_jump(relay+32*i,reinterpret_cast<void *>(wrappers[i]));ok=call_prepare(day_links[i],kDay[i].site,relay+32*i);}
    if(!ok || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),relay,kDayCount*32)) {
        VirtualFree(relay,0,MEM_RELEASE);log_event("day_timing_prepare_rejected");return;
    }
    unsigned published=0;
    for(unsigned i=0;i<kDayCount;++i)if(use[i]) {if(call_publish(day_links[i]))++published;else {use[i]=false;log_event("day_timing_publish_rejected",kDay[i].site,i);}}
    log_event("day_timing_installed",published,wanted,kDayCount);
}
static bool install_face();
static bool install() {install_face();install_day_sites();return true;}
static bool install_face() {
    // 1B3523 belongs to face_keep when it is installed (its wrapper keeps the same timing key).
    const unsigned count=face_keep::installed?6:7;
    for(unsigned i=0;i<count;++i) {const unsigned site=kSites[i];
        if(image[site]!=0xE8 || site+5+*reinterpret_cast<int *>(image+site+1)!=kFace) {log_event("face_timing_guard_rejected",site);return false;}}
    Obj relay=allocate_near(image+kFace);if(!relay)return false;
    const Face wrappers[7]={s0,s1,s2,s3,s4,s5,s6};
    DWORD old;bool ok=true;
    for(unsigned i=0;i<count && ok;++i) {absolute_jump(relay+32*i,reinterpret_cast<void *>(wrappers[i]));ok=call_prepare(links[i],kSites[i],relay+32*i);}
    if(!ok || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),relay,count*32)) {
        VirtualFree(relay,0,MEM_RELEASE);log_event("face_timing_prepare_rejected");return false;
    }
    for(unsigned i=0;i<count;++i)if(!call_publish(links[i])) {log_event("face_timing_publish_rejected",i);while(i)call_restore(links[--i]);return false;}
    log_event("face_timing_installed");return true;
}
}
