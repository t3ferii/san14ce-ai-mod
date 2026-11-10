// First-chance observation only. Never suppress, retry, or recover exceptions.
// Avoid the normal log lock, CRT formatting, heap allocation and game calls:
// an exception can arrive while any of those facilities is already locked.
#include "game_addresses.h"
static __declspec(thread) const char *native_phase="outside_mod_callback";
static __declspec(thread) const char *native_search_caller="none";
static __declspec(thread) unsigned native_search_from,native_search_flags,native_search_buffer;
struct NativePhase {
    const char *previous;
    explicit NativePhase(const char *name):previous(native_phase) {native_phase=name;}
    ~NativePhase() {native_phase=previous;}
};
static unsigned crash_image_size,crash_module_size;
static volatile LONG crash_records;
static void crash_text(char *line,unsigned &used,const char *text) {
    for(unsigned i=0;text[i] && used<700;++i)line[used++]=text[i];
}
[[maybe_unused]] static void crash_hex(char *line,unsigned &used,uintptr_t value) {
    static const char digits[]="0123456789ABCDEF";
    crash_text(line,used,"0x");
    for(int shift=static_cast<int>(sizeof(value)*8)-4;shift>=0;shift-=4)
        line[used++]=digits[(value>>shift)&15];
}
#if defined(RUNTIME_COST_PROFILE) && !defined(NO_RUNTIME_LOG)
static void crash_decimal(char *line,unsigned &used,unsigned value) {
    char digits[10];unsigned n=0;do {digits[n++]=static_cast<char>('0'+value%10);value/=10;} while(value);
    while(n && used<700)line[used++]=digits[--n];
}
#endif
#ifndef NO_RUNTIME_LOG
// Walk a local copy of the fault context using Windows unwind metadata.
// No writes to game memory, symbols, game calls, retry or exception suppression.
using CrashLookup=PRUNTIME_FUNCTION(WINAPI *)(DWORD64,PDWORD64,PUNWIND_HISTORY_TABLE);
using CrashUnwind=PVOID(WINAPI *)(DWORD,DWORD64,DWORD64,PRUNTIME_FUNCTION,PCONTEXT,PVOID *,PDWORD64,PKNONVOLATILE_CONTEXT_POINTERS);
static CrashLookup crash_lookup;static CrashUnwind crash_unwind;
static void crash_prepare_unwind() {
    HMODULE ntdll=GetModuleHandleW(L"ntdll.dll");
    crash_lookup=reinterpret_cast<CrashLookup>(GetProcAddress(ntdll,"RtlLookupFunctionEntry"));
    crash_unwind=reinterpret_cast<CrashUnwind>(GetProcAddress(ntdll,"RtlVirtualUnwind"));
}
static void crash_stack(HANDLE file,const CONTEXT &fault) {
    auto lookup=crash_lookup;auto unwind=crash_unwind;
    if(!lookup || !unwind)return;
    CONTEXT current=fault;
    for(unsigned frame=0;frame<32 && current.Rip && current.Rsp;++frame) {
        DWORD64 base=0;PRUNTIME_FUNCTION entry=lookup(current.Rip,&base,nullptr);
        char line[768];unsigned used=0;
        crash_text(line,used,"native_exception_stack frame=");crash_hex(line,used,frame);
        crash_text(line,used," ip=");crash_hex(line,used,current.Rip);
        crash_text(line,used," image=");crash_hex(line,used,base);
        crash_text(line,used," rva=");crash_hex(line,used,base?current.Rip-base:0);
        crash_text(line,used," rsp=");crash_hex(line,used,current.Rsp);crash_text(line,used,"\r\n");
        DWORD written=0;WriteFile(file,line,used,&written,nullptr);
        DWORD64 prior_ip=current.Rip,prior_sp=current.Rsp;
        if(entry) {PVOID data=nullptr;DWORD64 establisher=0;
            unwind(0,base,current.Rip,entry,&current,&data,&establisher,nullptr);
        } else {SIZE_T bytes=0;DWORD64 caller=0;
            if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void *>(current.Rsp),&caller,sizeof(caller),&bytes) || bytes!=sizeof(caller))break;
            current.Rip=caller;current.Rsp+=8;
        }
        if(current.Rip==prior_ip || current.Rsp<=prior_sp || current.Rsp-prior_sp>1048576)break;
    }
}
static bool crash_read(const void *address,void *value,SIZE_T size) {
    SIZE_T read=0;return address && ReadProcessMemory(GetCurrentProcess(),address,value,size,&read) && read==size;
}
static uintptr_t crash_vtable_rva(uintptr_t object) {
    uintptr_t table=0,base=reinterpret_cast<uintptr_t>(image);
    if(!crash_read(reinterpret_cast<const void *>(object),&table,sizeof(table)) || table<base || table-base>=crash_image_size)return 0;
    return table-base;
}
#ifdef RUNTIME_COST_PROFILE
struct CrashCczRing {const costprofile::CrumbRing *rings;unsigned size;};
static CrashCczRing cached_ccz_ring;
[[maybe_unused]] static void crash_cache_ccz(HMODULE module) {
    using GetRing=void (__cdecl *)(const costprofile::CrumbRing **,unsigned *);
    auto get=module?reinterpret_cast<GetRing>(GetProcAddress(module,"SAN14CczDiagCrumbs")):nullptr;
    CrashCczRing r={};if(get)get(&r.rings,&r.size);
    if(r.rings && r.size==64)cached_ccz_ring=r;
}
static CrashCczRing crash_ccz_ring() {return cached_ccz_ring;}
static bool crash_rings(HANDLE file,const costprofile::CrumbRing *rings,uintptr_t match,const char *prefix,const char *reg=nullptr) {
    if(!rings)return false;bool found=false;DWORD current=GetCurrentThreadId();
    for(unsigned i=0;i<64;++i) {
        LONG owner=0,end=0;
        if(!crash_read(const_cast<const LONG *>(&rings[i].thread),&owner,sizeof(owner)) || !crash_read(const_cast<const LONG *>(&rings[i].index),&end,sizeof(end)) || end<=0)continue;
        unsigned count=(static_cast<DWORD>(owner)==current || i==63)?256:32;
        LONG first=end>static_cast<LONG>(count)?end-static_cast<LONG>(count):0;
        if(match || reg)first=end>256?end-256:0;
        for(LONG seq=first+1;seq<=end;++seq) {
            costprofile::Crumb entry={};
            if(!crash_read(&rings[i].entries[static_cast<unsigned>(seq)&255],&entry,sizeof(entry)) || !entry.name)continue;
            if((match || reg) && entry.value!=match)continue;
            if(match)found=true;
            if(file==INVALID_HANDLE_VALUE)continue;
            char name[128]={};for(unsigned j=0;j<sizeof(name)-1;++j)if(!crash_read(entry.name+j,&name[j],1) || !name[j])break;
            char line[768];unsigned used=0;
            if(reg) {crash_text(line,used,"reg_match=");crash_text(line,used,reg);crash_text(line,used,":");crash_text(line,used,name);}
            else {
                crash_text(line,used,prefix);crash_text(line,used," tick=");crash_hex(line,used,entry.tick);
                crash_text(line,used," thread=");crash_hex(line,used,entry.thread);
                crash_text(line,used,entry.enter==costprofile::Call?" call name=":entry.enter==costprofile::Enter?" enter name=":" leave name=");
                crash_text(line,used,name);crash_text(line,used," value=");crash_hex(line,used,entry.value);
                crash_text(line,used," repeat=");crash_decimal(line,used,static_cast<unsigned>(entry.repeat));
            }
            crash_text(line,used,"\r\n");DWORD written=0;WriteFile(file,line,used,&written,nullptr);
        }
    }
    return found;
}
static bool crash_ccz_crumbs(HANDLE file,const CrashCczRing &r,uintptr_t match) {
    return crash_rings(file,r.rings,match,"native_crumb_ccz");
}
#endif
static void crash_crumbs(HANDLE file) {
#ifdef RUNTIME_COST_PROFILE
    crash_rings(file,costprofile::crumb_rings,0,"native_crumb");
#else
    (void)file;
#endif
}

#endif
static LONG CALLBACK observe_native_exception(EXCEPTION_POINTERS *info) {
#ifdef NO_RUNTIME_LOG
    (void)info;return EXCEPTION_CONTINUE_SEARCH;
#else
    if(!diagnostic_logging)return EXCEPTION_CONTINUE_SEARCH;
    if(!info || !info->ExceptionRecord)return EXCEPTION_CONTINUE_SEARCH;
    DWORD code=info->ExceptionRecord->ExceptionCode;
    // Debugger/C++ exceptions are routinely handled. Stack overflow cannot
    // safely use this writer without reserving stack on every game thread.
    if(code!=EXCEPTION_ACCESS_VIOLATION && code!=EXCEPTION_IN_PAGE_ERROR &&
       code!=EXCEPTION_ILLEGAL_INSTRUCTION && code!=EXCEPTION_INT_DIVIDE_BY_ZERO)
        return EXCEPTION_CONTINUE_SEARCH;
    if(InterlockedIncrement(&crash_records)>3)return EXCEPTION_CONTINUE_SEARCH;
    char line[768];unsigned used=0;
    crash_text(line,used,"native_exception_first_chance code=");crash_hex(line,used,code);
    uintptr_t ip=reinterpret_cast<uintptr_t>(info->ExceptionRecord->ExceptionAddress);
    crash_text(line,used," ip=");crash_hex(line,used,ip);
    uintptr_t base=reinterpret_cast<uintptr_t>(image),module=reinterpret_cast<uintptr_t>(self_module);
    if(ip>=base && ip-base<crash_image_size) {
        crash_text(line,used," game_rva=");crash_hex(line,used,ip-base);
    }
    if(ip>=module && ip-module<crash_module_size) {
        crash_text(line,used," mod_rva=");crash_hex(line,used,ip-module);
    }
    crash_text(line,used," thread=");crash_hex(line,used,GetCurrentThreadId());
    crash_text(line,used," phase=");crash_text(line,used,native_phase);
    if(native_search_depth) {
        crash_text(line,used," path_caller=");crash_text(line,used,native_search_caller);
        crash_text(line,used," from=");crash_hex(line,used,native_search_from);
        crash_text(line,used," flags=");crash_hex(line,used,native_search_flags);
        crash_text(line,used," buffer=");crash_hex(line,used,native_search_buffer);
    }
    if(info->ContextRecord) {
        const CONTEXT &c=*info->ContextRecord;
        crash_text(line,used," rax=");crash_hex(line,used,c.Rax);
        crash_text(line,used," rbx=");crash_hex(line,used,c.Rbx);
        crash_text(line,used," rcx=");crash_hex(line,used,c.Rcx);
        crash_text(line,used," rdx=");crash_hex(line,used,c.Rdx);
        crash_text(line,used," rsi=");crash_hex(line,used,c.Rsi);
        crash_text(line,used," rdi=");crash_hex(line,used,c.Rdi);
        crash_text(line,used," rbp=");crash_hex(line,used,c.Rbp);
        crash_text(line,used," rsp=");crash_hex(line,used,c.Rsp);
        crash_text(line,used," r8=");crash_hex(line,used,c.R8);
        crash_text(line,used," r9=");crash_hex(line,used,c.R9);
        crash_text(line,used," r10=");crash_hex(line,used,c.R10);
        crash_text(line,used," r11=");crash_hex(line,used,c.R11);
        crash_text(line,used," r12=");crash_hex(line,used,c.R12);
        crash_text(line,used," r13=");crash_hex(line,used,c.R13);
        crash_text(line,used," r14=");crash_hex(line,used,c.R14);
        crash_text(line,used," r15=");crash_hex(line,used,c.R15);
        crash_text(line,used," rip=");crash_hex(line,used,c.Rip);
    }
    if(info->ExceptionRecord->NumberParameters>=2 &&
       (code==EXCEPTION_ACCESS_VIOLATION || code==EXCEPTION_IN_PAGE_ERROR)) {
        crash_text(line,used," access=");crash_hex(line,used,info->ExceptionRecord->ExceptionInformation[0]);
        crash_text(line,used," address=");crash_hex(line,used,info->ExceptionRecord->ExceptionInformation[1]);
    }
    crash_text(line,used,"\r\n");
    HANDLE file=CreateFileW(log_path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,
                            nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file!=INVALID_HANDLE_VALUE) {DWORD written=0;WriteFile(file,line,used,&written,nullptr);
        if(info->ContextRecord)crash_stack(file,*info->ContextRecord);
#ifdef RUNTIME_COST_PROFILE
        CrashCczRing ccz=crash_ccz_ring();
#endif
        if(code==EXCEPTION_ACCESS_VIOLATION && info->ContextRecord) {
            const CONTEXT &c=*info->ContextRecord;
            struct RegisterValue {const char *name;uintptr_t value;};
            const RegisterValue regs[]={{"rax",c.Rax},{"rbx",c.Rbx},{"rcx",c.Rcx},{"rdx",c.Rdx},
                {"rsi",c.Rsi},{"rdi",c.Rdi},{"rbp",c.Rbp},{"rsp",c.Rsp},
                {"r8",c.R8},{"r9",c.R9},{"r10",c.R10},{"r11",c.R11},
                {"r12",c.R12},{"r13",c.R13},{"r14",c.R14},{"r15",c.R15}};
            const unsigned object_regs[]={1,4,5,2};
            for(unsigned index:object_regs) {
                uintptr_t value=0;bool ok=crash_read(reinterpret_cast<void *>(regs[index].value),&value,sizeof(value));
                char details[768];unsigned n=0;crash_text(details,n,"native_exception_object reg=");crash_text(details,n,regs[index].name);
                crash_text(details,n," readable=");crash_hex(details,n,ok?1:0);
                if(ok) {crash_text(details,n," qword=");crash_hex(details,n,value);
                    if(value>=base && value-base<crash_image_size) {crash_text(details,n," image_rva=");crash_hex(details,n,value-base);}}
                crash_text(details,n,"\r\n");WriteFile(file,details,n,&written,nullptr);
            }
#ifdef RUNTIME_COST_PROFILE
            for(const auto &reg:regs) {
                crash_rings(file,costprofile::crumb_rings,reg.value,"native_crumb",reg.name);
                crash_rings(file,ccz.rings,reg.value,"native_crumb_ccz",reg.name);
            }
#endif
        }
        uintptr_t game_rva=ip>=base?ip-base:0;
        if(code==EXCEPTION_ACCESS_VIOLATION && game_rva==GA(GA_1A00EE) && info->ContextRecord) {
            const CONTEXT &c=*info->ContextRecord;uintptr_t target=0;unsigned short unit_id=0;uintptr_t display=0;unsigned char readable=0;bool display_readable=false;
            crash_read(reinterpret_cast<const void *>(c.Rdi+0x1F0),&target,sizeof(target));
            crash_read(reinterpret_cast<const void *>(target+0x12),&unit_id,sizeof(unit_id));
            crash_read(reinterpret_cast<const void *>(target+0x148),&display,sizeof(display));
            if(display)display_readable=crash_read(reinterpret_cast<const void *>(display),&readable,1);
            char details[768];unsigned n=0;
            crash_text(details,n,"native_exception_widget widget_vtable_rva=");crash_hex(details,n,crash_vtable_rva(c.Rdi));
            crash_text(details,n," unit=");crash_hex(details,n,target);
            crash_text(details,n," unit_vtable_rva=");crash_hex(details,n,crash_vtable_rva(target));
            crash_text(details,n," unit_id=");crash_hex(details,n,unit_id);
            crash_text(details,n," display=");crash_hex(details,n,display);
            crash_text(details,n," display_readable=");crash_hex(details,n,display_readable?1:0);
#ifdef RUNTIME_COST_PROFILE
            crash_text(details,n," ccz_match=");crash_hex(details,n,crash_ccz_crumbs(INVALID_HANDLE_VALUE,ccz,display)?1:0);
#endif
            crash_text(details,n,"\r\n");
            (void)readable;WriteFile(file,details,n,&written,nullptr);
        }
        crash_crumbs(file);
#ifdef RUNTIME_COST_PROFILE
        crash_ccz_crumbs(file,ccz,0);
#endif
        CloseHandle(file);}
    return EXCEPTION_CONTINUE_SEARCH;
#endif
}
