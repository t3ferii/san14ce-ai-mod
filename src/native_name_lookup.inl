// Optional, independently guarded replacement of the original name scan
// E09910 (see name_lookup.h). Runs on whichever game thread calls E09910;
// no worker or remote call. The entry is a full replacement: its first
// instruction (MOV [RSP+10],RBP, five bytes) becomes JMP rel32 -> FF25 relay.
// The relay changes no stack, so the original .pdata still unwinds offset 0.
#include "name_lookup.h"
static unsigned char *__fastcall name_lookup_entry(unsigned char *table,const char *key) { COST_SCOPE("perf_name_lookup_entry");
    return name_lookup::find(table,key);
}
static bool name_lookup_guards_match() {
    using namespace name_lookup;
    return !memcmp(image+entry_rva,entry_bytes,sizeof(entry_bytes)) &&
           !memcmp(image+compare_rva,compare_bytes,sizeof(compare_bytes));
}
static bool install_name_lookup() {
    // A/B switch, read once at game start: 0 leaves E09910 untouched (기능설정.ini, then progress_native.ini).
    if(!mod_setting(L"name_lookup",L"enabled",1,L"progress_native.ini")) {log_event("name_lookup_disabled");return true;}
    if(!name_lookup_guards_match()) {log_event("name_lookup_guard_rejected");return false;}
    Obj relay=allocate_near(image+name_lookup::entry_rva);if(!relay)return false;
    absolute_jump(relay,reinterpret_cast<void *>(&name_lookup_entry));
    CodeLink link;DWORD old;
    if(!prepare_link(link,name_lookup::entry_rva,0xE9,relay) || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),relay,16)) {VirtualFree(relay,0,MEM_RELEASE);return false;}
    // A failed 16-byte swap leaves the original block; the relay is kept in
    // case another thread observed a published block during the attempt.
    if(!publish_link(link)) {log_event("name_lookup_publish_rejected");return false;}
    log_event("name_lookup_hook_installed");return true;
}
