// Synchronous layout load 86C570 (name hash -> load job -> Sleep(0) spin until ready).
// [features] layout_keep (default on): a first load costs ~25 ms, a load while the
//   resource is still referenced ~2 ms, and the HUD/report layouts are released and
//   reloaded on every phase change. After a slow first synchronous load, the same
//   name is loaded once more, asynchronously, into a hidden base CUILayout that is
//   never destroyed, so the resource stays resident (at most kKeepMax names).
// [diagnostics] layout_wait=1: append waits >= 2 ms to data\diagnostics\layout_wait.csv.
// Same arguments, same caller thread; the original always runs. Evidence:
// REA_Analysis/stall_20261018/README.md. Call sites: 676767, 86CFA5, 86DEC9 (E8
// rel32) and 86E0C0 (whole thin wrapper replaced: its call crosses a 16-byte block).
#include <string>
namespace layout_wait {
// 86E0C0 forwards its 7th argument as an 8th, so 86C570 takes eight.
using Load=void(__fastcall *)(Obj,unsigned,unsigned,Obj,const char *,unsigned long long,int,unsigned long long);
static const unsigned kLoad=0x86C570,kWrapper=0x86E0C0,kSites[3]={0x676767,0x86CFA5,0x86DEC9};
static const unsigned char kWrapperBytes[0x33]={0x48,0x83,0xEC,0x48,0x48,0x8B,0x84,0x24,0x80,0x00,0x00,0x00,0x48,0x89,0x44,0x24,
    0x38,0x8B,0x44,0x24,0x78,0x89,0x44,0x24,0x30,0x48,0x8B,0x44,0x24,0x70,0x48,0xC7,0x44,0x24,0x28,0x00,0x00,0x00,0x00,0x48,
    0x89,0x44,0x24,0x20,0xE8,0x7F,0xE4,0xFF,0xFF,0xB8,0x01};
static LARGE_INTEGER frequency,origin;
static HANDLE file=INVALID_HANDLE_VALUE;
static SRWLOCK lock=SRWLOCK_INIT;
static const size_t kKeepMax=512;
static bool keep_enabled;
static SRWLOCK keep_lock=SRWLOCK_INIT;
static std::vector<std::string> kept;          // names already held
static Obj make_holder() {
    // Same construction as the plain CUILayout container in 882540.
    Obj holder=fn<Obj(__fastcall *)(size_t)>(0x3F6BC0)(0x178);
    if(!holder)return nullptr;
    fn<void(__fastcall *)(Obj)>(0x864C50)(holder);
    *reinterpret_cast<Obj *>(holder)=image+0x1342530;
    fn<void(__fastcall *)(Obj)>(0x92AB70)(holder+0x130);
    return holder;
}
static void keep(const char *name,unsigned a,unsigned b,unsigned long long extra,unsigned long long tail) {
    std::string key(name);
    AcquireSRWLockExclusive(&keep_lock);
    bool fresh=kept.size()<kKeepMax && std::find(kept.begin(),kept.end(),key)==kept.end();
    if(fresh)kept.push_back(key);
    ReleaseSRWLockExclusive(&keep_lock);
    if(!fresh)return;
    if(Obj holder=make_holder())fn<Load>(kLoad)(holder,a,b,nullptr,name,extra,1,tail);   // async: no parent, never shown
}
static void __fastcall timed(Obj self,unsigned a,unsigned b,Obj parent,const char *name,unsigned long long extra,int async,unsigned long long tail) { COST_SCOPE("layout_timed");
    LARGE_INTEGER begin,end;QueryPerformanceCounter(&begin);
    fn<Load>(kLoad)(self,a,b,parent,name,extra,async,tail);
    QueryPerformanceCounter(&end);
    const double ms=static_cast<double>(end.QuadPart-begin.QuadPart)*1000.0/static_cast<double>(frequency.QuadPart);
    if(keep_enabled && !async && name && ms>=5.0)keep(name,a,b,extra,tail);
    if(ms<2.0 || file==INVALID_HANDLE_VALUE)return;
    char line[256];
    const double at=static_cast<double>(begin.QuadPart-origin.QuadPart)/static_cast<double>(frequency.QuadPart);
    int n=_snprintf_s(line,sizeof(line),_TRUNCATE,"%.3f,%lu,%.1f,%d,%.120s\r\n",at,GetCurrentThreadId(),ms,async,name?name:"");
    if(n<=0)return;
    AcquireSRWLockExclusive(&lock);DWORD written;WriteFile(file,line,static_cast<DWORD>(n),&written,nullptr);ReleaseSRWLockExclusive(&lock);
}
static unsigned long long __fastcall wrapper(Obj self,unsigned a,unsigned b,Obj parent,const char *name,int async,unsigned long long tail) {
    timed(self,a,b,parent,name,0,async,tail);return 1;   // original 86E0C0: 86C570(...,name,0,async,tail); return 1
}
static CodeLink links[4];
static bool install() {
    wchar_t path[MAX_PATH];
    if(!GetModuleFileNameW(self_module,path,MAX_PATH))return false;
    wchar_t *slash=wcsrchr(path,L'\\');if(!slash)return false;*slash=0;
    wchar_t config[MAX_PATH];wcscpy_s(config,path);
    if(wcscat_s(config,L"\\AI_TacticsMod\\기능설정.ini"))return false;
    keep_enabled=mod_features.layout_keep;
    const bool diagnostics=dev_group() && GetPrivateProfileIntW(L"diagnostics",L"layout_wait",0,config)!=0;
    if(!keep_enabled && !diagnostics)return true;
    for(unsigned site:kSites)if(image[site]!=0xE8 || site+5+*reinterpret_cast<int *>(image+site+1)!=kLoad)return false;
    if(memcmp(image+kWrapper,kWrapperBytes,sizeof(kWrapperBytes)))return false;
    QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&origin);
    if(diagnostics && !wcscat_s(path,L"\\AI_TacticsMod\\data\\diagnostics")) {
        CreateDirectoryW(path,nullptr);
        if(!wcscat_s(path,L"\\layout_wait.csv"))
            file=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file!=INVALID_HANDLE_VALUE) {
            const char head[]="session_seconds,thread,wait_ms,async,layout\r\n";DWORD written;WriteFile(file,head,sizeof(head)-1,&written,nullptr);
        }
    }
    Obj relay=allocate_near(image+kLoad);if(!relay)return false;
    absolute_jump(relay,reinterpret_cast<void *>(&timed));absolute_jump(relay+32,reinterpret_cast<void *>(&wrapper));
    DWORD old;bool ok=true;
    for(unsigned i=0;i<3;++i)ok=ok && prepare_link(links[i],kSites[i],0xE8,relay);
    ok=ok && prepare_link(links[3],kWrapper,0xE9,relay+32);
    if(!ok || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),relay,64)) {
        VirtualFree(relay,0,MEM_RELEASE);return false;
    }
    for(auto &l:links)if(!publish_link(l)) {for(auto &r:links)restore_link(r);return false;}
    log_event("layout_wait_diagnostic_installed");return true;
}
}
