// SAN14_ModPack.dll bridge (user 2026-11-13: other authors' patches in their own DLL). First user: 4계절 패치 (wprkfdid12-ux) season
// rules, vendored in the module (src/season/season_engine.c). This side loads the module on the install worker when a season rule is
// on, and owns the one hook: the original time-step CALL 44E7D0 -> 33EB20 (where the season patch itself ran its day). The wrapper
// runs the original step, then the rules' day on the same game thread; the rules change the game only through original functions
// (SAN14_ModPack.dll: 179DE0 damage, 271E70 morale, 1F5680 endurance, the base's food setter, 936980 news); area public order
// and population have no setter in the original (it writes them in place), so they are written the same way, inside this hook. Guard: the CALL must still
// target 33EB20 (the 180 modpack's own SeasonRules DLL takes the same site - then nothing here is published and the rules stay off).
// The rules' IniParams list feeds the season picture (terrain_render::season_bake::weather_params). Off = no CALL, no module call.
#include "modpack_dll_api.h"
#include "season_mobility_shared.h"
namespace season_rules {
static const ModPackApi *api;
static bool tried;
static volatile LONG running;
static CallLink link;
[[maybe_unused]] static void __cdecl log_sink(const char *kind,unsigned a,unsigned b,unsigned c) {log_event(kind,a,b,c);}
static bool load() {
    if(tried)return api!=nullptr;
    tried=true;wchar_t path[MAX_PATH];
    DWORD length=GetModuleFileNameW(self_module,path,MAX_PATH);if(!length || length>=MAX_PATH)return false;
    wchar_t *slash=wcsrchr(path,L'\\');if(!slash || wcscpy_s(slash+1,MAX_PATH-static_cast<size_t>(slash+1-path),L"SAN14_ModPack.dll"))return false;
    HMODULE module=LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    auto get=module?reinterpret_cast<GetModPackApi>(GetProcAddress(module,"SAN14ModPackGetApi")):nullptr;
    const ModPackApi *found=get?get(modpack_dll_abi):nullptr;
    if(!found || found->size!=sizeof(ModPackApi) || found->abi!=modpack_dll_abi || !found->season_start || !found->season_day ||
       !found->season_params || !found->season_flags) {
        log_event("modpack_dll_missing",module?1u:0u,found?1u:0u);return false;
    }
    api=found;return true;
}
static unsigned flags() {
    unsigned f=0;
    if(mod_features.season_disaster)f|=kSeasonDisasters;
    if(mod_features.season_news)f|=kSeasonNews;
    if(mod_features.season_damage)f|=kSeasonDamage;
    if(mod_features.season_fire)f|=kSeasonFire;
    if(mod_features.season_domestic)f|=kSeasonDomestic;
    if(mod_features.season_tiles)f|=kSeasonFloodTerrain;   // local133: flood water and river ice as terrain
    return f;
}
// R1d disaster tiles: the painter compares and queues only the cells whose disaster changed (one pass over the cell table).
static bool painted;
static void tiles() { COST_SCOPE("season_rules:tiles");
    const bool on=mod_features.season_tiles && api->season_paint && terrain_render::active();
    if(!on && !painted)return;
    const unsigned changed=terrain_render::season_sync(on?api->season_paint:nullptr);painted=on;
#ifndef NO_RUNTIME_LOG
    if(changed)log_event("season_tiles",changed,on?1u:0u,0);
#else
    (void)changed;
#endif
}
// local132 (user 2026-11-14 "홍수 일어나면 육지타일을 강으로 바꾸고 싶어 자연스럽게"): flooded flat land becomes 여울 / 강 while the water
// stands (terrain_edit::flood_sync: drawn by terrain_render, never saved, restored when the water leaves or the world changes).
static void flood() { COST_SCOPE("season_rules:flood");
    const bool on=mod_features.season_tiles && api->season_flood && terrain_render::active();
    const unsigned changed=terrain_edit::flood_sync(on?api->season_flood:nullptr);
#ifndef NO_RUNTIME_LOG
    if(changed)log_event("season_flood_terrain",changed,terrain_edit::flood_count,on?1u:0u);
#else
    (void)changed;
#endif
}
#ifndef NO_RUNTIME_LOG
// local148 probe (user 2026-11-14 "만총이 운두를 먹으러갈때 이상하게 먹으러가 ... 적도 없는데 왜저러지?"): once a game day, every
// player army whose order changed since the day before: a = army | order kind +0x37 << 16 | order arg +0x38 << 24,
// b = position +0x2A | order target +0x3A << 16, c = next stored hex +0x48 | commander +0x12 << 16. Diagnostic build only.
static void probe_player_armies() { COST_SCOPE("season_rules:probe_player_armies");
    static unsigned long long last[501];static Obj last_root;static unsigned lines;
    if(last_root!=world()) {last_root=world();memset(last,0,sizeof(last));}
    const unsigned me=player_force();if(!me || lines>=6000)return;
    for(unsigned i=1;i<=500;++i) {
        Obj a=table(0x7E838,i,500);if(!valid(a) || army_force(a)!=me) {last[i]=0;continue;}
        const unsigned va=i|(static_cast<unsigned>(a[0x37])<<16)|(static_cast<unsigned>(a[0x38])<<24);
        const unsigned vc=word(a,0x48)|(static_cast<unsigned>(word(a,0x12))<<16);
        const unsigned long long sig=(static_cast<unsigned long long>(va)<<32)|(static_cast<unsigned long long>(word(a,0x3A))<<16)|word(a,0x48);
        if(sig==last[i])continue;
        last[i]=sig;++lines;
        log_event("probe_player_army",va,word(a,0x2A)|(static_cast<unsigned>(word(a,0x3A))<<16),vc);
    }
}
#endif
using TimeStep=uint64_t(__fastcall *)(uint64_t,uint64_t,uint64_t,uint64_t);
static uint64_t __fastcall time_step(uint64_t a,uint64_t b,uint64_t c,uint64_t d) { COST_SCOPE("season_rules::time_step");
    const uint64_t r=fn<TimeStep>(GA(GA_33EB20))(a,b,c,d);
    if(InterlockedCompareExchange(&running,0,0) && api) {
#ifndef NO_RUNTIME_LOG
        LARGE_INTEGER q0,q1,qf;QueryPerformanceCounter(&q0);
        {COST_SCOPE("season_rules:season_day");api->season_day();}
        QueryPerformanceCounter(&q1);QueryPerformanceFrequency(&qf);
        const unsigned us=static_cast<unsigned>((q1.QuadPart-q0.QuadPart)*1000000/qf.QuadPart);
        static ULONGLONG slow_at;   // a day of the rules over 20 ms (a frame or more), at most every 2 s
        if(us>20000 && GetTickCount64()-slow_at>2000){slow_at=GetTickCount64();log_event("season_day_slow",us,0,0);}
#else
        api->season_day();
#endif
        // local141: the disaster tiles and the flood terrain only change with the rules' day; they scanned all 48,400 cells and
        // 3,000 buildings on every time step (about 19 a second). Now once per game day, world or switch change.
        static int synced_day=-1;static Obj synced_world;static bool synced_on;
        const int day=game_day();Obj w=world();const bool on_now=mod_features.season_tiles && terrain_render::active();
        if(day!=synced_day || w!=synced_world || on_now!=synced_on) {synced_day=day;synced_world=w;synced_on=on_now;tiles();flood();
#ifndef NO_RUNTIME_LOG
            probe_player_armies();
#endif
        }
    }
    return r;
}
static unsigned __cdecl params(float *out,int count) {return InterlockedCompareExchange(&running,0,0) && api?api->season_params(out,count):0u;}
// interop 2026-11-16: the same conditions install_rules() needs before it can start - the time-step CALL still targets 33EB20 and
// SAN14_ModPack.dll sits next to this module - checked at the first feature guard pass, before any terrain hook installs.
bool can_run() {
    const unsigned site=GA(GA_44E7D0),target=GA(GA_33EB20);
    if(!site || !target || image[site]!=0xE8 || site+5+*reinterpret_cast<int *>(image+site+1)!=target)return false;
    wchar_t path[MAX_PATH];DWORD length=GetModuleFileNameW(self_module,path,MAX_PATH);if(!length || length>=MAX_PATH)return false;
    wchar_t *slash=wcsrchr(path,L'\\');if(!slash || wcscpy_s(slash+1,MAX_PATH-static_cast<size_t>(slash+1-path),L"SAN14_ModPack.dll"))return false;
    return GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES;
}
static bool install_rules() { COST_SCOPE("season_rules:install");
    if(!mod_features.season_weather)return false;
    const unsigned site=GA(GA_44E7D0),target=GA(GA_33EB20);
    if(!site || !target || image[site]!=0xE8 || site+5+*reinterpret_cast<int *>(image+site+1)!=target) {log_event("season_rules_guard_rejected");return false;}
    if(!load())return false;
    wchar_t folder[MAX_PATH]={};
    DWORD n=GetModuleFileNameW(nullptr,folder,MAX_PATH);wchar_t *slash=n && n<MAX_PATH?wcsrchr(folder,L'\\'):nullptr;
    if(!slash)return false;
    *slash=0;if(wcscat_s(folder,L"\\AI_TacticsMod\\data\\season\\"))return false;
    const int started=api->season_start(image,folder,flags(),
#ifndef NO_RUNTIME_LOG
        &log_sink
#else
        nullptr
#endif
    );
    if(!started) {log_event("season_rules_start_failed");return false;}
    Obj relay=allocate_near(image+site);if(!relay)return false;
    absolute_jump(relay,reinterpret_cast<void *>(&time_step));
    DWORD old;
    if(!call_prepare(link,site,relay) || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),relay,32)) {
        VirtualFree(relay,0,MEM_RELEASE);log_event("season_rules_prepare_rejected");return false;
    }
    InterlockedExchange(&running,1);
    if(!call_publish(link)) {InterlockedExchange(&running,0);log_event("season_rules_publish_rejected");return false;}
    terrain_render::season_bake::weather_params=&params;
    log_event("season_rules_installed",static_cast<unsigned>(started),flags(),0);
    // local135: snow -1 / ice -3 mobility lives in the combat module's 2B2550 hook; hand it the rules' per-cell answer.
    HMODULE combat=GetModuleHandleW(L"SAN14_CombatAI.dll");
    auto give=combat?reinterpret_cast<GiveSeasonMobility>(GetProcAddress(combat,"SAN14CombatSeasonMobility")):nullptr;
    const bool linked=give && api->season_mobility && give(kSeasonMobilityVersion,api->season_mobility);
    log_event("season_mobility_link",linked?1u:0u,combat?1u:0u,give?1u:0u);
    return true;
}
// interop 2026-11-16: a rules install that does not complete takes the season tiles out of the terrain engine gate
// (terrain_core_on): the engine pieces already installed then stay idle, exactly as with 부동산 개발 off.
void install() {if(!install_rules())season_tiles_runnable=false;}
}
