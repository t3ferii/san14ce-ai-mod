#pragma once
// ai행동교정모드호환성.log (user 2026-11-09): the installer and the game DLLs append to one file in the
// game folder. This writer is compiled in every build, including NO_RUNTIME_LOG releases, and records
// only features that another mod's presence made us refuse or yield. File errors never affect the game.
// Line: "yyyy-MM-dd HH:mm:ss [게임] <calling DLL> 거부|양보 <what> 0x<rva> <why> <other module or ->"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

// Fixtures must not write into the real game folder.
#if defined(SPLIT_MODULE_FIXTURE) || defined(TACTICS_FIXTURE) || defined(LAYOUT_FIXTURE)
#define COMPAT_REPORT_OFF 1
#endif

namespace compat_report {
inline const wchar_t *file_name() {return L"ai행동교정모드호환성.log";}
inline const wchar_t *old_file_name() {return L"ai행동교정모드호환성.old.log";}
inline unsigned file_cap() {return 512u*1024u;}

// 1..13 are layout_api.h LAYOUT_WHY_*.
inline const wchar_t *why_text(unsigned why) {
    switch(why) {
    case 1:return L"다른 모듈 코드";
    case 2:return L"힙 주소";
    case 3:return L"다른 SAN14 DLL";
    case 4:return L"입구 막힘";
    case 5:return L"잘못된 자리";
    case 6:return L"원본 바이트 다름";
    case 7:return L"범위 밖";
    case 8:return L"동시 변경";
    case 9:return L"같은 기능 양보";
    case 10:return L"이미 다른 곳으로 연결";
    case 11:return L"중계 실패";
    case 12:return L"허용 목록 아님";
    case 13:return L"묶음 실패";
    default:return L"알 수 없음";
    }
}
// native_split_install.inl feature_off/guard_mismatch reasons (only used when a feature is actually switched off).
inline const wchar_t *split_why(unsigned reason) {
    switch(reason) {
    case 1:return L"가져온 코드가 참조 빌드에 고정됨";
    case 2:return L"준비 실패";
    case 3:return L"게시 실패";
    case 4:return L"가져온 설치기가 0을 반환";
    case 6:return L"원본 바이트 다름";
    default:return L"기능을 끔";
    }
}

struct State {
    SRWLOCK lock;
    bool header_done;
    bool claimed;
    bool owner;
    HANDLE mutex;
};
inline State &state() {
    static State s={SRWLOCK_INIT,false,false,false,nullptr};
    return s;
}

inline void timestamp(wchar_t *out,size_t count) {
    SYSTEMTIME t;
    GetLocalTime(&t);
    swprintf_s(out,count,L"%04u-%02u-%02u %02u:%02u:%02u",
        static_cast<unsigned>(t.wYear),static_cast<unsigned>(t.wMonth),static_cast<unsigned>(t.wDay),
        static_cast<unsigned>(t.wHour),static_cast<unsigned>(t.wMinute),static_cast<unsigned>(t.wSecond));
}

// Game folder = directory of the running EXE.
inline bool game_directory(wchar_t *out,DWORD count) {
    DWORD length=GetModuleFileNameW(nullptr,out,count);
    if(!length || length>=count)return false;
    wchar_t *slash=wcsrchr(out,L'\\');
    if(!slash)return false;
    *slash=0;
    return true;
}

// PE timestamp (IMAGE_FILE_HEADER.TimeDateStamp) of the running EXE; 0 when unreadable.
inline unsigned exe_stamp() {
    wchar_t path[MAX_PATH];
    DWORD length=GetModuleFileNameW(nullptr,path,MAX_PATH);
    if(!length || length>=MAX_PATH)return 0;
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,
        OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f==INVALID_HANDLE_VALUE)return 0;
    DWORD got=0;
    LONG offset=0;
    unsigned stamp=0;
    if(SetFilePointer(f,0x3C,nullptr,FILE_BEGIN)!=INVALID_SET_FILE_POINTER &&
       ReadFile(f,&offset,4,&got,nullptr) && got==4 && offset>0 &&
       SetFilePointer(f,offset+8,nullptr,FILE_BEGIN)!=INVALID_SET_FILE_POINTER &&
       ReadFile(f,&stamp,4,&got,nullptr) && got==4) {}
    CloseHandle(f);
    return stamp;
}

// Name of the DLL that calls us (Loader/Layout/CombatAI/Convenience/CCZ).
inline const wchar_t *module_name() {
    static wchar_t name[64]=L"?";
    static bool done=false;
    if(!done) {
        static const char anchor=0;
        HMODULE module=nullptr;
        if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(&anchor),&module) && module) {
            wchar_t path[MAX_PATH];
            DWORD length=GetModuleFileNameW(module,path,MAX_PATH);
            if(length && length<MAX_PATH) {
                const wchar_t *base=wcsrchr(path,L'\\');
                base=base?base+1:path;
                wcsncpy_s(name,base,_TRUNCATE);
            }
        }
        done=true;
    }
    return name;
}

inline void write_line(const wchar_t *line) {
#ifdef COMPAT_REPORT_OFF
    (void)line;
#else
    wchar_t dir[MAX_PATH];
    if(!game_directory(dir,MAX_PATH))return;
    wchar_t path[MAX_PATH],old[MAX_PATH];
    if(swprintf_s(path,MAX_PATH,L"%s\\%s",dir,file_name())<0)return;
    WIN32_FILE_ATTRIBUTE_DATA info{};
    if(GetFileAttributesExW(path,GetFileExInfoStandard,&info) &&
       (info.nFileSizeHigh || info.nFileSizeLow>=file_cap())) {
        if(swprintf_s(old,MAX_PATH,L"%s\\%s",dir,old_file_name())>=0) {
            DeleteFileW(old);
            MoveFileExW(path,old,MOVEFILE_REPLACE_EXISTING);
        }
    }
    // Append mode with READ|WRITE sharing (the installer refuses to run while the game runs).
    HANDLE f=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,
        OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f==INVALID_HANDLE_VALUE)return;
    char utf8[2048];
    int length=WideCharToMultiByte(CP_UTF8,0,line,-1,utf8,sizeof(utf8),nullptr,nullptr);
    if(length>1) {
        DWORD written=0;
        const char crlf[2]={'\r','\n'};
        WriteFile(f,utf8,static_cast<DWORD>(length-1),&written,nullptr);
        WriteFile(f,crlf,2,&written,nullptr);
    }
    CloseHandle(f);
#endif
}

// One "=== 게임 실행 ===" header per process; the named mutex coordinates the DLLs loaded in it.
inline bool claim_session() {
#ifdef COMPAT_REPORT_OFF
    return false;
#else
    State &s=state();
    if(s.claimed)return s.owner;
    s.claimed=true;
    wchar_t name[80];
    if(swprintf_s(name,80,L"Local\\SAN14CompatReport_%u",static_cast<unsigned>(GetCurrentProcessId()))<0)return false;
    HANDLE mutex=CreateMutexW(nullptr,FALSE,name);
    if(!mutex) {s.owner=true;return true;}   // cannot coordinate: still leave the header
    if(GetLastError()==ERROR_ALREADY_EXISTS) {CloseHandle(mutex);return false;}
    s.mutex=mutex;   // kept open for the process lifetime so the name stays alive
    s.owner=true;
    return true;
#endif
}

inline void write_header() {
    wchar_t time[32],line[192];
    timestamp(time,32);
    swprintf_s(line,192,L"%s [게임] === 게임 실행 (EXE 0x%08X) ===",time,exe_stamp());
    write_line(line);
}

inline void session() {
    State &s=state();
    AcquireSRWLockExclusive(&s.lock);
    if(!s.header_done) {
        if(claim_session())write_header();
        s.header_done=true;
    }
    ReleaseSRWLockExclusive(&s.lock);
}

// add_text: why is the already resolved text, yield selects 양보 (otherwise 거부).
inline void add_text(const wchar_t *what,unsigned rva,const wchar_t *why,const wchar_t *other_module,bool yield) {
    State &s=state();
    AcquireSRWLockExclusive(&s.lock);
    if(!s.header_done) {
        if(claim_session())write_header();
        s.header_done=true;
    }
    wchar_t time[32],line[768];
    timestamp(time,32);
    swprintf_s(line,768,L"%s [게임] %s %s %s 0x%08X %s %s",time,module_name(),yield?L"양보":L"거부",
        what?what:L"?",rva,why?why:L"?",(other_module && *other_module)?other_module:L"-");
    write_line(line);
    ReleaseSRWLockExclusive(&s.lock);
}

inline void add(const wchar_t *what,unsigned rva,unsigned why,const wchar_t *other_module) {
    add_text(what,rva,why_text(why),other_module,why==9);
}

// A split feature switched itself off (feature_off id / guard_mismatch id).
inline void add_off(const wchar_t *what,unsigned reason) {
    add_text(what,0,split_why(reason),nullptr,false);
}
}
