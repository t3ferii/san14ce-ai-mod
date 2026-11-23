// 조조전 탭 (2026-11-01 사용자: 조조전 기능은 별도 DLL). Sole owner of the Present hook and of the 169F8B
// CALL publication stays SAN14_Convenience; SAN14_CCZ.dll (ccz_module.cpp) draws and decides. It is
// loaded here, on the install worker, only when a [ccz] feature is on. Missing DLL -> those features off.
#include "game_addresses.h"
#ifndef CCZ_GUARD_FIXTURE
#include "ccz_module_api.h"
namespace ccz_module {
static const CczApi *api;
static bool tried;
[[maybe_unused]] static void __cdecl log_sink(const char *kind,unsigned a,unsigned b,unsigned c) {log_event(kind,a,b,c);}
static void __cdecl overlay_install() {unified_overlay::install();}
static bool load() {
    if(tried)return api!=nullptr;
    tried=true;wchar_t path[MAX_PATH];
    DWORD length=GetModuleFileNameW(self_module,path,MAX_PATH);if(!length || length>=MAX_PATH)return false;
    wchar_t *slash=wcsrchr(path,L'\\');if(!slash || wcscpy_s(slash+1,MAX_PATH-static_cast<size_t>(slash+1-path),L"SAN14_CCZ.dll"))return false;
    HMODULE module=LoadLibraryExW(path,nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
#if defined(RUNTIME_COST_PROFILE) && !defined(NO_RUNTIME_LOG)
    if(module)crash_cache_ccz(module);
#endif
    auto get=module?reinterpret_cast<GetCczApi>(GetProcAddress(module,"SAN14CczGetApi")):nullptr;
    const CczApi *found=get?get(ccz_module_abi):nullptr;
    if(!found || found->size!=sizeof(CczApi) || found->abi!=ccz_module_abi || !found->initialize || !found->sprites_guards ||
       !found->install_sprites || !found->needs_present || !found->on_present || !found->configure_combat || !found->combat_fix ||
       !found->bands || !found->bands->configure || !found->bands->save || !found->bands->load || !found->bands->reset || !found->install_idle_flags) {
        log_event("ccz_module_missing",module?1:0,found?1:0);return false;   // module stays loaded; nothing published
    }
#ifndef NO_RUNTIME_LOG
    found->initialize(image,&log_sink,&overlay_install);
#else
    found->initialize(image,nullptr,&overlay_install);
#endif
    api=found;return true;
}
static void reload_appearance() {
    if(!api)return;
#ifndef NO_RUNTIME_LOG
    api->initialize(image,&log_sink,&overlay_install);
#else
    api->initialize(image,nullptr,&overlay_install);
#endif
}
}
namespace ccz_sprites {
static volatile LONG published;
static bool guards_match() {return ccz_module::load() && ccz_module::api->sprites_guards();}
bool install(bool hide,bool depth,bool far_keep,bool fallback) {
    if(!ccz_module::load() || !ccz_module::api->install_sprites(hide,depth,far_keep,fallback))return false;
    InterlockedExchange(&published,1);return true;
}
// 2026-11-19 (user: idle 의용병 무리 had no flag with 조조전 병종 그림 off): the renderer draws only the idle band flags; armies keep
// their native models and the army-display slot stays unhooked. Called by ccz_bands::install when ccz_sprites is off.
bool install_idle_flags(bool depth) {
    if(!ccz_module::load() || !ccz_module::api->install_idle_flags(depth))return false;
    InterlockedExchange(&published,1);return true;
}
bool needs_present() {return InterlockedCompareExchange(&published,0,0) && ccz_module::api->needs_present();}
void on_present(ID3D11Device *d,ID3D11DeviceContext *c,ID3D11Texture2D *back,const D3D11_TEXTURE2D_DESC &desc) {
    if(InterlockedCompareExchange(&published,0,0))ccz_module::api->on_present(d,c,back,desc);
}
}
bool ccz_sprites_guards_match() {return ccz_sprites::guards_match();}
#endif
// 종군 중립군(무리) entry links (published by native_ccz_bands.inl; 22E690 is one of them). A guard that hashes one of these
// entries reads the original bytes where the 16-byte block still holds exactly our patch; any other bytes stay as they are, so a
// foreign patch on the same entry still fails the guard.
namespace ccz_band_links {
static CodeLink links[14];   // = kBandHookCount (ccz_module_api.h; static_assert in native_ccz_bands.inl)
[[maybe_unused]] static unsigned char view(unsigned rva) {
    for(const CodeLink &l:links) {
        if(!l.published || rva<l.block || rva>=l.block+16)continue;
        if(!memcmp(image+l.block,l.modified,16))return l.original[rva-l.block];
    }
    return image[rva];
}
}
// 조조전식 막기·회심. 일반 공격 처리 169C50의 6단계 CALL 16FDD0(manager,pair,output) at 169F8B (유일한 호출자).
namespace ccz_combat {
const unsigned kSite=GA(GA_16AF3B),kFix=GA(GA_170D80);
static CallLink link;
static bool guards_match() {
    // lea r8,[rsp+30]; mov rdx,rbx; mov rcx,r15; call 16FDD0 (169C50 case 6)
    // User 2026-11-02: original army-force and hostile-relation ABI.
    static const struct {unsigned rva;unsigned long long hash;} relations[]={
        {GA(GA_22E690),GH(0x4DEBF7200DE1C67Cull)},{GA(GA_2CC630),GH(0xAE2882DA8EB1870Eull)}};
    for(const auto &g:relations) {
        unsigned long long h=0xCBF29CE484222325ull;
        for(unsigned i=0;i<0x30;++i)h=(h^ccz_band_links::view(g.rva+i))*0x100000001B3ull;
        if(h!=g.hash)return false;
    }
    static const unsigned char before[]={0x4C,0x8D,0x44,0x24,0x30,0x48,0x8B,0xD3,0x49,0x8B,0xCF};
    return image[kSite]==0xE8 && kSite+5+*reinterpret_cast<int *>(image+kSite+1)==kFix && !ga::mcmp_at(image,kSite-11,before,11);
}
#ifndef CCZ_GUARD_FIXTURE
bool install(bool block,bool critical) {
    if(!guards_match()){log_event("ccz_combat_guard_rejected");return false;}
    if(!ccz_module::load())return false;
    ccz_module::api->configure_combat(block,critical);
    Obj relay=allocate_near(image+kSite);if(!relay)return false;
    absolute_jump(relay,reinterpret_cast<void *>(ccz_module::api->combat_fix));DWORD old;
    if(!call_prepare(link,kSite,relay) || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),relay,32)){VirtualFree(relay,0,MEM_RELEASE);log_event("ccz_combat_prepare_rejected");return false;}
    if(!call_publish(link)){log_event("ccz_combat_publish_rejected");return false;}
    return true;
}
#endif
}
