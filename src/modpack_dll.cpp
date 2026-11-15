// SAN14_ModPack.dll: other authors' patches ported into the mod (user 2026-11-13). First: 4계절 패치 (wprkfdid12-ux) season rules,
// vendored as C in src/season/season_engine.c (tools/vendor_season_rules.py). SAN14_Convenience.dll loads this module, publishes the
// hooks and calls season_day on the game thread right after the original time step. Every game change goes through the game's own
// functions (user 2026-11-13 "메모리 직접 수정은 안되 ... 원작의 함수를 훅걸어서"):
//   troops    179DE0(kind 27 army, army id, amount, source 255 none, -1): the original damage step (wounded, its morale loss, destruction)
//   morale    271E70(army, delta, 0, 1, 1): the original morale change (trait 0x0D, setter 242FC0; 0 = destroyed, so never below 10 here)
//   endurance 1F5680(building, value): the original set_durability
// food (the base's own vtable setter) and news (936980) are already called the same way inside the vendored rules.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "game_addresses.h"
#include "modpack_dll_api.h"
using Obj=unsigned char *;
static Obj image;
static HMODULE self_module;
static ModPackLog log_sink;
static volatile LONG season_flags_now;
static void mp_log(const char *kind,unsigned a=0,unsigned b=0,unsigned c=0) {if(log_sink)log_sink(kind,a,b,c);}
template<class T> static T fn(unsigned rva) {return reinterpret_cast<T>(image+rva);}
extern "C" {
int sr_start(unsigned char *image,const unsigned short *folder,const unsigned *rva,int nrva,int flags);
void sr_set_flags(int flags);
void sr_day(void);
unsigned sr_params(float *out,int count);
float sr_speed_factor(int cell);
float sr_route_factor(int cell);
int sr_cell_state(int cell);
int sr_cell_paint(int cell);
int sr_flood_terrain(int cell);
int sr_mobility_delta(int cell);
// local140: the rules' day cost by part, every 10 s (slot | calls << 8, total us, max us; slots in season_engine.c pc_*)
void sr_cost_out(int slot,unsigned calls,unsigned total_us,unsigned max_us) {mp_log("season_cost",static_cast<unsigned>(slot)|(calls<<8),total_us,max_us);}
}
namespace season_io {
static bool damage() {return (InterlockedCompareExchange(&season_flags_now,0,0)&kSeasonDamage)!=0;}
static Obj world() {const unsigned r=GA(GA_2780F60);return r?*reinterpret_cast<Obj *>(image+r):nullptr;}
static unsigned army_id(Obj army) {
    Obj w=world();if(!w || !army)return 0;
    for(unsigned i=1;i<=500;++i)if(*reinterpret_cast<Obj *>(w+518200+8*i)==army)return i;   // DB_ARMY 0x7E838
    return 0;
}
static void morale_to(Obj army,int target) {
    const int now=army[26];   // +0x1A
    if(target<10)target=10;
    if(now<=target || now>150)return;
    const unsigned rva=GA(GA_271E70);if(!rva)return;
    fn<uint64_t(__fastcall *)(Obj,int,Obj,int,int)>(rva)(army,target-now,nullptr,1,1);
}
}
extern "C" void sr_cb_hurt(unsigned char *army,int troops_lose,int morale_target) {
    if(!season_io::damage() || !army)return;
    const unsigned id=season_io::army_id(army),rva=GA(GA_179DE0);
    const unsigned troops=*reinterpret_cast<unsigned short *>(army+22);   // +0x16
    if(id && rva && troops_lose>0 && static_cast<unsigned>(troops_lose)<troops)
        fn<uint64_t(__fastcall *)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t)>(rva)(27,id,static_cast<unsigned>(troops_lose),255,4294967295u);
    season_io::morale_to(army,morale_target);
    mp_log("season_hurt",id,static_cast<unsigned>(troops_lose),static_cast<unsigned>(morale_target));
}
extern "C" void sr_cb_morale(unsigned char *army,int delta) {
    if(!season_io::damage() || !army || delta>=0)return;
    season_io::morale_to(army,army[26]+delta);
}
extern "C" void sr_cb_durability(unsigned char *building,int value) {
    if(!season_io::damage() || !building || value<0)return;
    const unsigned rva=GA(GA_1F5680);if(!rva)return;
    fn<void(__fastcall *)(Obj,int)>(rva)(building,value);
    mp_log("season_endurance",value,0,0);
}
// Public order (+0x54, u8 0..100) and population (+0x52, u16 0..60000) of an area: the original writes both fields in place
// (175DE0 attack aftermath, 304D70 quarterly growth) - there is no setter. Same as native_population.inl: written on the game
// thread inside the hooked original time step, with the original's ranges (area vtable checked; order only ever lowered).
namespace season_io {
static bool domestic() {return (InterlockedCompareExchange(&season_flags_now,0,0)&kSeasonDomestic)!=0;}
static bool area_ok(Obj area) {const unsigned vt=GA(GA_13A99C0);return area && vt && *reinterpret_cast<Obj *>(area)==image+vt;}
}
extern "C" void sr_cb_order(unsigned char *area,int value) {
    if(!season_io::domestic() || !season_io::area_ok(area) || area[0x54]>100)return;
    const int v=value<0?0:value>100?100:value;
    if(v>=area[0x54])return;   // the season rules only lower it
    mp_log("season_order",area[0x54],static_cast<unsigned>(v),0);
    area[0x54]=static_cast<unsigned char>(v);
}
extern "C" void sr_cb_population(unsigned char *area,int value) {
    if(!season_io::domestic() || !season_io::area_ok(area))return;
    const int v=value<0?0:value>60000?60000:value;
    unsigned short *p=reinterpret_cast<unsigned short *>(area+0x52);
    mp_log("season_population",*p,static_cast<unsigned>(v),0);
    *p=static_cast<unsigned short>(v);
}
namespace modpack_exports {
static bool fire_setter_ok() {   // the season patch's own check of SetHexFire's first bytes
    static const unsigned char head[12]={72,137,92,36,8,87,72,131,236,32,139,218};
    const unsigned rva=GA(GA_2F0100);return rva && !memcmp(image+rva,head,12);
}
static int engine_flags(unsigned flags) {
    int f=0;if(flags&kSeasonDisasters)f|=1;if(flags&kSeasonNews)f|=2;if((flags&kSeasonFire) && fire_setter_ok())f|=4;if(flags&kSeasonDomestic)f|=32;if(flags&kSeasonFloodTerrain)f|=64;return f;
}
static int __cdecl season_start(Obj base,const wchar_t *folder,unsigned flags,ModPackLog log) {
    image=base;log_sink=log;InterlockedExchange(&season_flags_now,static_cast<LONG>(flags));
    if(ga::build_index()<0 && ga::state().game_host)ga::resolve_runtime(base);   // unknown game build: own signature resolution
    const unsigned rva[15]={GA(GA_2780F60),GA(GA_1345458),GA(GA_13A99C0),GA(GA_936980),GA(GA_16FF70),GA(GA_1A4AB88),GA(GA_13A95F0),
                            GA(GA_13A96D0),GA(GA_22E1D0),GA(GA_269BC0),GA(GA_2F0100),GA(GA_13989C8),GA(GA_1397B4C),GA(GA_1A23578),GA(GA_2144AD0)};
    const int r=sr_start(base,reinterpret_cast<const unsigned short *>(folder),rva,15,engine_flags(flags));
    mp_log("season_rules_start",static_cast<unsigned>(r),flags,static_cast<unsigned>(engine_flags(flags)));
    return r;
}
static void __cdecl season_flags(unsigned flags) {InterlockedExchange(&season_flags_now,static_cast<LONG>(flags));sr_set_flags(engine_flags(flags));}
static void __cdecl season_day() {sr_day();}
static unsigned __cdecl season_params(float *out,int count) {return sr_params(out,count);}
static float __cdecl season_speed(int cell) {return sr_speed_factor(cell);}
static float __cdecl season_route(int cell) {return sr_route_factor(cell);}
static int __cdecl season_cell(int cell) {return sr_cell_state(cell);}
static int __cdecl season_paint(int cell) {return sr_cell_paint(cell);}
static int __cdecl season_flood(int cell) {return sr_flood_terrain(cell);}
static int __cdecl season_mobility(int cell) {return sr_mobility_delta(cell);}
static const ModPackApi api={sizeof(ModPackApi),modpack_dll_abi,season_start,season_flags,season_day,season_params,season_speed,season_route,season_cell,season_paint,season_flood,season_mobility};
}
extern "C" __declspec(dllexport) const ModPackApi *__cdecl SAN14ModPackGetApi(unsigned abi) {return abi==modpack_dll_abi?&modpack_exports::api:nullptr;}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH){self_module=module;DisableThreadLibraryCalls(module);}
    return TRUE;
}
