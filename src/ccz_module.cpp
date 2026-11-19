// SAN14_CCZ.dll: 조조전 탭 기능(병종 그림, 막기·회심). SAN14_Convenience.dll이 [ccz] 기능이 켜졌을 때만
// 불러 SAN14CczGetApi로 연결한다(ccz_module_api.h). 로더 작업자는 검사된 연결만 게시하고, 게임 함수는
// 게임 자신의 장면·전투 스레드 콜백 안에서만 부른다. 설계: docs/CCZ_SPRITES.md, docs/CCZ_COMBAT.md.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <initializer_list>
#include "game_addresses.h"
#include <stdint.h>
#include <string.h>
#include "ccz_module_api.h"
#ifdef RUNTIME_COST_PROFILE
#include "native_cost_profile.h"
#endif
using Obj=unsigned char *;
static Obj image;
static HMODULE self_module;
static CczLog log_sink;
static void (__cdecl *overlay_hook)();
static void ccz_log(const char *kind,unsigned a=0,unsigned b=0,unsigned c=0) {if(log_sink)log_sink(kind,a,b,c);}
#define log_event(...) ccz_log(__VA_ARGS__)
#ifndef RUNTIME_COST_PROFILE
#define COST_SCOPE(name) do {} while(0)
#define COST_CRUMB(name,value) do {} while(0)
#endif
static void overlay_install() {if(overlay_hook)overlay_hook();}
#ifdef CCZ_NATIVE_FIXTURE
static void *ccz_fixture_function(unsigned);
static unsigned ccz_fixture_roll();
template<class T> static T fn(unsigned rva) {return reinterpret_cast<T>(ccz_fixture_function(rva));}
#else
template<class T> static T fn(unsigned rva) {
#if defined(RUNTIME_COST_PROFILE) && !defined(NO_RUNTIME_LOG)
    costprofile::crumb("game_function",costprofile::Call,rva);
#endif
    return reinterpret_cast<T>(image+rva);
}
#endif
template<class T> static T next_pick(T next,unsigned rva) {return next?next:fn<T>(rva);}   // Layout manager next (null in fixtures)
static unsigned short word(Obj p,unsigned off) {return *reinterpret_cast<unsigned short *>(p+off);}
static unsigned dword(Obj p,unsigned off) {return *reinterpret_cast<unsigned *>(p+off);}
static Obj pointer(Obj p,unsigned off) {return *reinterpret_cast<Obj *>(p+off);}
static Obj world() {return pointer(image,GA(GA_2780F60));}
static bool valid(Obj obj) {return obj!=nullptr && fn<int(__fastcall *)(Obj)>(GA(GA_331780))(obj)!=0;}
static Obj table(unsigned offset,unsigned id,unsigned maximum) {
    Obj root=world();
    return root && id<=maximum?pointer(root,offset+id*8):nullptr;
}
// <게임>\AI_TacticsMod\<file>; this DLL sits next to the game EXE like the other modules.
static bool mod_ini_path(wchar_t *out,const wchar_t *file) {
    if(!GetModuleFileNameW(self_module,out,MAX_PATH))return false;
    wchar_t *slash=wcsrchr(out,L'\\');if(!slash)return false;*slash=0;
    return !wcscat_s(out,MAX_PATH,L"\\AI_TacticsMod\\") && !wcscat_s(out,MAX_PATH,file);
}
#include "native_layout_client.inl"   // 2026-11-05: the army-display slot is registered with SAN14_Layout.dll (whitelist row, body stays here)
// 2026-11-17 종군 중립군 idle picture: the band day pass publishes its idle record sites here (site+1, 0 = empty), the sprite
// renderer reads them without a lock (a day pass may hold the band lock across game calls; the render thread must not wait).
// 2026-11-18: each site also carries its band kind (ccz_band_kinds.h) so the idle picture and the follower picture are the same class.
// slots[] (moved here from ccz_bands.inl) = live band units by generic commander (4033 + index): unit address (48 bits) | kind << 48
// | patron force << 56, written once by the band day pass; the renderer checks the address against the army it draws.
#include "ccz_band_kinds.h"
#include "ccz_portrait.h"   // 2026-11-18 band flag portrait (ccz_bands.inl; both picture modes since 2026-11-19)
namespace ccz_idle {
constexpr unsigned kMax=16,kSlots=68,kFirstGeneric=4033;
static volatile LONG64 sites[kMax];
static volatile LONG site_kinds[kMax];
// 2026-11-19 user "초상화가 차오르는식으로 ... 1000명부터 3000명까지 한달": per site, the grown share in per mille (0 = 1000 troops,
// 1000 = 3000), written with the site by publish_idle; the flag picture fills from the bottom by this share.
static volatile LONG site_fill[kMax];
static volatile LONG64 slots[kSlots];
constexpr unsigned long long kUnitMask=0x0000FFFFFFFFFFFFull;
static bool band_of(Obj unit,unsigned &kind,unsigned &force) {
    if(!unit)return false;
    const unsigned c=static_cast<unsigned>(*reinterpret_cast<unsigned short *>(unit+0x12))-kFirstGeneric;if(c>=kSlots)return false;
    const unsigned long long v=static_cast<unsigned long long>(slots[c]);
    if(!v || reinterpret_cast<Obj>(v&kUnitMask)!=unit)return false;
    kind=static_cast<unsigned>((v>>48)&0xFF);force=static_cast<unsigned>(v>>56);return true;
}
}
#include "native_ccz_sprites.inl"
#include "ccz_bands.inl"   // 2026-11-17 종군 중립군(무리) stage 0, 2026-11-18 지역 특색·바싹 따라붙기 (docs/NEUTRAL_BANDITS.md)
namespace ccz_exports {
static void __cdecl initialize(Obj base,CczLog log,void (__cdecl *overlay)()) {image=base;log_sink=log;overlay_hook=overlay;
    if(ga::build_index()<0 && ga::state().game_host)ga::resolve_runtime(base);   // unknown game build: own signature resolution (docs/PATCH_RESILIENCE.md)
#ifdef RUNTIME_COST_PROFILE
    costprofile::set_crumbs_enabled(log!=nullptr);
#endif
    COST_SCOPE("ccz_api_initialize");ccz_sprites::reload_appearance();}
static bool __cdecl sprites_guards() { COST_SCOPE("ccz_api_sprites_guards");return image && ccz_sprites::guards_match();}
static bool __cdecl install_sprites(bool hide,bool depth,bool far_keep,bool fallback) { COST_SCOPE("ccz_api_install_sprites");return image && ccz_sprites::install(hide,depth,far_keep,fallback);}
static bool __cdecl install_idle_flags(bool depth) { COST_SCOPE("ccz_api_install_idle_flags");return image && ccz_sprites::install_idle_only(depth);}
static bool __cdecl needs_present() { COST_SCOPE("ccz_api_needs_present");return ccz_sprites::needs_present();}
static void __cdecl on_present(ID3D11Device *d,ID3D11DeviceContext *c,ID3D11Texture2D *back,const D3D11_TEXTURE2D_DESC &desc) { COST_SCOPE("ccz_api_on_present");ccz_sprites::on_present(d,c,back,desc);}
static void __cdecl configure_combat(bool block,bool critical) { COST_SCOPE("ccz_api_configure_combat");ccz_combat::configure(block,critical);}
static const CczBands bands={{reinterpret_cast<void *>(&ccz_bands::hook_army_force),reinterpret_cast<void *>(&ccz_bands::hook_paint),
    reinterpret_cast<void *>(&ccz_bands::hook_supply),reinterpret_cast<void *>(&ccz_bands::hook_remove),
    reinterpret_cast<void *>(&ccz_bands::hook_paint_write),reinterpret_cast<void *>(&ccz_bands::hook_morale),
    reinterpret_cast<void *>(&ccz_bands::hook_day_move),reinterpret_cast<void *>(&ccz_bands::hook_set_action),
    reinterpret_cast<void *>(&ccz_bands::hook_set_target),reinterpret_cast<void *>(&ccz_bands::hook_face_texture),
    reinterpret_cast<void *>(&ccz_bands::hook_unit_target),reinterpret_cast<void *>(&ccz_bands::hook_max_morale),
    reinterpret_cast<void *>(&ccz_bands::hook_capture)},
    ccz_bands::configure,ccz_bands::save,ccz_bands::load,ccz_bands::reset,ccz_bands::power,ccz_bands::exchange,ccz_bands::fire};
static const CczApi api={sizeof(CczApi),ccz_module_abi,initialize,sprites_guards,install_sprites,needs_present,on_present,configure_combat,ccz_combat::fix,&bands,
    install_idle_flags};
}
extern "C" __declspec(dllexport) const CczApi *__cdecl SAN14CczGetApi(unsigned abi) {COST_SCOPE("ccz_api_get");return abi==ccz_module_abi?&ccz_exports::api:nullptr;}
#if defined(RUNTIME_COST_PROFILE) && !defined(NO_RUNTIME_LOG)
extern "C" __declspec(dllexport) void __cdecl SAN14CczDiagCrumbs(const costprofile::CrumbRing **ring,unsigned *size) {
    *ring=costprofile::crumb_rings;*size=64;
}
#endif
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH){self_module=module;DisableThreadLibraryCalls(module);}
    return TRUE;
}
