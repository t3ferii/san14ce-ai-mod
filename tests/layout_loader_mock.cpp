// Loader order mocks (user exception 2026-11-05): harmless stand-ins for SAN14_Layout.dll and
// SAN14_Convenience.dll that append their call order to layout_order.log next to the DLL.
// MOCK_LAYOUT: SAN14LayoutInitialize / SAN14LayoutPublish. MOCK_LAYOUT_FAIL: Initialize returns false.
// default (convenience): SAN14ConvenienceInitialize + SAN14TestCalls (the shared loader fixture asks for it).
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HMODULE self_module;
static void record(const char *line) {
    wchar_t path[MAX_PATH];
    if(!GetModuleFileNameW(self_module,path,MAX_PATH))return;
    wchar_t *slash=wcsrchr(path,L'\\');if(!slash)return;
    if(wcscpy_s(slash+1,MAX_PATH-static_cast<size_t>(slash+1-path),L"layout_order.log"))return;
    HANDLE file=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return;
    DWORD written=0;WriteFile(file,line,static_cast<DWORD>(lstrlenA(line)),&written,nullptr);WriteFile(file,"\r\n",2,&written,nullptr);
    CloseHandle(file);
}
#if defined(MOCK_LAYOUT) || defined(MOCK_LAYOUT_FAIL)
extern "C" __declspec(dllexport) bool __cdecl SAN14LayoutInitialize(unsigned version) {
    record(version==1?"layout_init":"layout_init_bad_version");
#ifdef MOCK_LAYOUT_FAIL
    return false;
#else
    return version==1;
#endif
}
extern "C" __declspec(dllexport) void __cdecl SAN14LayoutPublish() {record("layout_publish");}
#else
static volatile LONG calls;
extern "C" __declspec(dllexport) bool __cdecl SAN14ConvenienceInitialize(unsigned,const void *) {
    InterlockedIncrement(&calls);record("convenience");return true;
}
extern "C" __declspec(dllexport) unsigned __cdecl SAN14TestCalls() {return static_cast<unsigned>(calls);}
#endif
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH)self_module=module;
    return TRUE;
}
