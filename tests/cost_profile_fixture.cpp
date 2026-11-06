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
    costprofile::set_crumbs_enabled(true);auto *ring=costprofile::get_crumb_ring();LONG crumb_start=ring->index;
    {costprofile::Key crumb_key("crumb_fixture");costprofile::Scope crumb_scope(crumb_key);}
    {costprofile::Key crumb_key("crumb_fixture");costprofile::Scope crumb_scope(crumb_key);}
    costprofile::crumb("game_function",costprofile::Call,0x1234);
    costprofile::set_crumbs_enabled(false);
    const auto &crumb_enter=ring->entries[static_cast<unsigned>(crumb_start+1)&255];
    const auto &crumb_leave=ring->entries[static_cast<unsigned>(crumb_start+2)&255];
    const auto &crumb_call=ring->entries[static_cast<unsigned>(crumb_start+3)&255];
    check(ring->index==crumb_start+3 && crumb_enter.enter==1 && crumb_leave.enter==0 &&
          crumb_enter.repeat==2 && crumb_leave.repeat==2 && crumb_call.enter==costprofile::Call && crumb_call.value==0x1234,
          "thread ring compresses repeated pairs and records calls");
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
