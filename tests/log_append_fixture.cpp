#define WIN32_LEAN_AND_MEAN
#include "../src/log_append.h"
#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>
static unsigned checks;
static void check(bool value) {if(!value){fprintf(stderr,"Append check failed: %u\n",checks+1);exit(1);}++checks;}
static std::string read(const wchar_t *path) {
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);check(file!=INVALID_HANDLE_VALUE);
    DWORD length=GetFileSize(file,nullptr),done=0;std::string result(length,0);
    check(ReadFile(file,result.data(),length,&done,nullptr)!=0 && done==length);CloseHandle(file);return result;
}
static double measure(const wchar_t *path,bool reuse) {
    diagnostics::AppendFile writer;LARGE_INTEGER begin,end,freq;QueryPerformanceFrequency(&freq);QueryPerformanceCounter(&begin);
    for(unsigned i=0;i<3000;++i)if(!writer.write(path,"benchmark event army=1 value=2 hex=3\r\n",38,reuse))exit(2);
    writer.close();QueryPerformanceCounter(&end);return static_cast<double>(end.QuadPart-begin.QuadPart)*1000.0/static_cast<double>(freq.QuadPart);
}
static SRWLOCK fixture_lock=SRWLOCK_INIT;
static diagnostics::AppendFile concurrent;
static const wchar_t *concurrent_path;
static unsigned sequence;
static DWORD WINAPI worker(LPVOID) {
    for(unsigned i=0;i<1000;++i) {
        AcquireSRWLockExclusive(&fixture_lock);char line[64];int n=sprintf_s(line,"sequence=%u\r\n",sequence++);
        bool ok=concurrent.write(concurrent_path,line,static_cast<DWORD>(n),true);
        ReleaseSRWLockExclusive(&fixture_lock);if(!ok)return 1;
    }
    return 0;
}
int wmain(int argc,wchar_t **argv) {
    check(argc==2);std::wstring folder=argv[1];
    std::wstring path=folder+L"\\append.log",old=folder+L"\\legacy.log",bad=folder+L"\\absent\\bad.log";
    diagnostics::AppendFile writer;
    check(!writer.write(bad.c_str(),"one\r\n",5,true));check(writer.cached==INVALID_HANDLE_VALUE);
    check(writer.write(path.c_str(),"one\r\n",5,true));HANDLE saved=writer.cached;check(saved!=INVALID_HANDLE_VALUE);
    check(writer.write(path.c_str(),"two\r\n",5,true));check(writer.cached==saved);check(read(path.c_str())=="one\r\ntwo\r\n");
    writer.close();check(writer.cached==INVALID_HANDLE_VALUE);check(writer.write(path.c_str(),"three\r\n",7,true));writer.close();
    check(read(path.c_str())=="one\r\ntwo\r\nthree\r\n");
    check(writer.write(old.c_str(),"legacy\r\n",8,false));check(writer.cached==INVALID_HANDLE_VALUE);check(read(old.c_str())=="legacy\r\n");
    std::wstring parallel=folder+L"\\parallel.log";concurrent_path=parallel.c_str();HANDLE threads[8];
    for(unsigned i=0;i<8;++i){threads[i]=CreateThread(nullptr,0,worker,nullptr,0,nullptr);check(threads[i]!=nullptr);}
    check(WaitForMultipleObjects(8,threads,TRUE,30000)==WAIT_OBJECT_0);
    for(HANDLE thread:threads){DWORD status=1;check(GetExitCodeThread(thread,&status)!=0 && !status);CloseHandle(thread);}
    concurrent.close();std::string expected;
    for(unsigned i=0;i<8000;++i)expected+="sequence="+std::to_string(i)+"\r\n";
    check(sequence==8000 && read(parallel.c_str())==expected);
    std::wstring before=folder+L"\\before.log",after=folder+L"\\after.log";std::vector<double> times_old,times_new;
    for(unsigned i=0;i<5;++i){times_old.push_back(measure(before.c_str(),false));times_new.push_back(measure(after.c_str(),true));}
    check(read(before.c_str())==read(after.c_str()));std::sort(times_old.begin(),times_old.end());std::sort(times_new.begin(),times_new.end());
    printf("{\"status\":\"log_append_fixture_passed\",\"checks\":%u,\"concurrent_lines\":8000,\"legacy_ms\":%.3f,\"reuse_ms\":%.3f,\"ratio\":%.3f,\"gameplay_verified\":false}\n",checks,times_old[2],times_new[2],times_old[2]/times_new[2]);return 0;
}
