#define RUNTIME_COST_PROFILE
#include "../src/native_cost_profile.h"
static unsigned checks;
static void check(bool v,const char *why) {++checks;if(!v) {puts(why);exit(1);}}
int main() {
    costprofile::initialize(GetModuleHandleW(nullptr));
    wchar_t directory[MAX_PATH],temporary[MAX_PATH];
    check(GetTempPathW(MAX_PATH,directory) && GetTempFileNameW(directory,L"cst",0,temporary),"own temp file");
    wcscpy_s(costprofile::file_path,temporary);
    costprofile::Key outer("outer"),inner("inner"),same("inner");
    check(inner.slot==same.slot,"duplicate names use the same counter");
    {costprofile::Scope a(outer);Sleep(3);{costprofile::Scope b(inner);Sleep(3);}Sleep(3);}
    auto &stats=costprofile::state.values;
    check(stats[outer.slot].calls==1 && stats[inner.slot].calls==1,"nested calls counted once each");
    check(stats[outer.slot].total>=stats[inner.slot].total && stats[outer.slot].self<stats[outer.slot].total,"inclusive and self costs separated");
    check(costprofile::state.writes==0 && costprofile::state.depth==0,"per-call destructor does not write a file");
    costprofile::flush();
    check(costprofile::state.writes==1 && !stats[outer.slot].calls,"one batched write resets counters");
    costprofile::flush();check(costprofile::state.writes==1,"empty flush produces no write");
    HANDLE file=CreateFileW(temporary,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
    char contents[2048]={};DWORD read=0;check(file!=INVALID_HANDLE_VALUE && ReadFile(file,contents,sizeof(contents)-1,&read,nullptr),"timing result readable");CloseHandle(file);
    check(strstr(contents,"outer,1,") && strstr(contents,"inner,1,"),"batch records function and call count");
    check(DeleteFileW(temporary)!=0,"remove only own temp fixture");
    printf("{\"status\":\"cost_profile_fixture_passed\",\"checks\":%u}\n",checks);
}
