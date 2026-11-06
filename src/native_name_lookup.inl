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
    return !ga::mcmp_at(image,entry_rva,entry_bytes,sizeof(entry_bytes)) &&
           !ga::mcmp_at(image,compare_rva,compare_bytes,sizeof(compare_bytes));
}
#ifndef NO_RUNTIME_LOG
// local5 diagnostic: which array failed and the first differing byte of the running image against this build's exact bytes.
// name_lookup_guard_detail(array 1 entry / 2 compare, first differing offset or 0xFFFF, rva).
static void name_lookup_guard_detail() {
    using namespace name_lookup;
    const struct {unsigned rva;const unsigned char *bytes;unsigned size;} arrays[]={{entry_rva,entry_bytes,sizeof(entry_bytes)},{compare_rva,compare_bytes,sizeof(compare_bytes)}};
    for(unsigned i=0;i<2;++i) {
        unsigned first=0xFFFF;
        for(unsigned k=0;k<arrays[i].size && first==0xFFFF;++k)if(ga::mcmp_at(image,arrays[i].rva+k,arrays[i].bytes+k,1))first=k;
        if(ga::mcmp_at(image,arrays[i].rva,arrays[i].bytes,arrays[i].size) || first!=0xFFFF)
            log_event("name_lookup_guard_detail",i+1,first,arrays[i].rva);
    }
}
#endif
static bool install_name_lookup() {
    // Switched by [performance] name_lookup (caller); the old [name_lookup] enabled duplicate is no longer read (2026-11-03).
    if(!name_lookup_guards_match()) {
        log_event("name_lookup_guard_rejected");
#ifndef NO_RUNTIME_LOG
        name_lookup_guard_detail();
#endif
        return false;
    }
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
