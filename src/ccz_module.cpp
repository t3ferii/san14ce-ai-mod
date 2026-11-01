// SAN14_CCZ.dll: 조조전 탭 기능(병종 그림, 막기·회심). SAN14_Convenience.dll이 [ccz] 기능이 켜졌을 때만
// 불러 SAN14CczGetApi로 연결한다(ccz_module_api.h). 로더 작업자는 검사된 연결만 게시하고, 게임 함수는
// 게임 자신의 장면·전투 스레드 콜백 안에서만 부른다. 설계: docs/CCZ_SPRITES.md, docs/CCZ_COMBAT.md.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "ccz_module_api.h"
using Obj=unsigned char *;
static Obj image;
static HMODULE self_module;
static CczLog log_sink;
static void (__cdecl *overlay_hook)();
static void ccz_log(const char *kind,unsigned a=0,unsigned b=0,unsigned c=0) {if(log_sink)log_sink(kind,a,b,c);}
#define log_event(...) ccz_log(__VA_ARGS__)
#define COST_SCOPE(name) do {} while(0)
static void overlay_install() {if(overlay_hook)overlay_hook();}
template<class T> static T fn(unsigned rva) {return reinterpret_cast<T>(image+rva);}
static unsigned short word(Obj p,unsigned off) {return *reinterpret_cast<unsigned short *>(p+off);}
static unsigned dword(Obj p,unsigned off) {return *reinterpret_cast<unsigned *>(p+off);}
static Obj pointer(Obj p,unsigned off) {return *reinterpret_cast<Obj *>(p+off);}
static Obj world() {return pointer(image,0x277DF18);}
static bool valid(Obj obj) {return obj!=nullptr && fn<int(__fastcall *)(Obj)>(0x32F500)(obj)!=0;}
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
static bool patch_pointer(unsigned rva,unsigned expected,void *replacement) {
    auto slot=reinterpret_cast<void **>(image+rva);DWORD old=0;
    if(*slot!=image+expected || !VirtualProtect(slot,8,PAGE_READWRITE,&old))return false;
    void *previous=InterlockedCompareExchangePointer(slot,replacement,image+expected);
    DWORD restored=0;VirtualProtect(slot,8,old,&restored);
    return previous==image+expected;
}
static void restore_pointer(unsigned rva,unsigned original,void *replacement) {
    auto slot=reinterpret_cast<void **>(image+rva);DWORD old=0,restored=0;
    if(!VirtualProtect(slot,8,PAGE_READWRITE,&old))return;
    InterlockedCompareExchangePointer(slot,image+original,replacement);
    VirtualProtect(slot,8,old,&restored);
}
#include "native_ccz_sprites.inl"
namespace ccz_exports {
static void __cdecl initialize(Obj base,CczLog log,void (__cdecl *overlay)()) {image=base;log_sink=log;overlay_hook=overlay;}
static bool __cdecl sprites_guards() {return image && ccz_sprites::guards_match();}
static bool __cdecl install_sprites(bool hide,bool depth) {return image && ccz_sprites::install(hide,depth);}
static bool __cdecl needs_present() {return ccz_sprites::needs_present();}
static void __cdecl on_present(ID3D11Device *d,ID3D11DeviceContext *c,ID3D11Texture2D *back,const D3D11_TEXTURE2D_DESC &desc) {ccz_sprites::on_present(d,c,back,desc);}
static void __cdecl configure_combat(bool block,bool critical) {ccz_combat::configure(block,critical);}
static const CczApi api={sizeof(CczApi),ccz_module_abi,initialize,sprites_guards,install_sprites,needs_present,on_present,configure_combat,ccz_combat::fix};
}
extern "C" __declspec(dllexport) const CczApi *__cdecl SAN14CczGetApi(unsigned abi) {return abi==ccz_module_abi?&ccz_exports::api:nullptr;}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH){self_module=module;DisableThreadLibraryCalls(module);}
    return TRUE;
}
