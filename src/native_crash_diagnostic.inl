// First-chance observation only. Never suppress, retry, or recover exceptions.
// Avoid the normal log lock, CRT formatting, heap allocation and game calls:
// an exception can arrive while any of those facilities is already locked.
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
static void crash_hex(char *line,unsigned &used,uintptr_t value) {
    static const char digits[]="0123456789ABCDEF";
    crash_text(line,used,"0x");
    for(int shift=static_cast<int>(sizeof(value)*8)-4;shift>=0;shift-=4)
        line[used++]=digits[(value>>shift)&15];
}
#ifndef NO_RUNTIME_LOG
// Walk a local copy of the fault context using Windows unwind metadata.
// No writes to game memory, symbols, game calls, retry or exception suppression.
static void crash_stack(HANDLE file,const CONTEXT &fault) {
    using Lookup=PRUNTIME_FUNCTION(WINAPI *)(DWORD64,PDWORD64,PUNWIND_HISTORY_TABLE);
    using Unwind=PVOID(WINAPI *)(DWORD,DWORD64,DWORD64,PRUNTIME_FUNCTION,PCONTEXT,PVOID *,PDWORD64,PKNONVOLATILE_CONTEXT_POINTERS);
    HMODULE ntdll=GetModuleHandleW(L"ntdll.dll");
    auto lookup=reinterpret_cast<Lookup>(GetProcAddress(ntdll,"RtlLookupFunctionEntry"));
    auto unwind=reinterpret_cast<Unwind>(GetProcAddress(ntdll,"RtlVirtualUnwind"));
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
        crash_text(line,used," r12=");crash_hex(line,used,info->ContextRecord->R12);
        crash_text(line,used," r15=");crash_hex(line,used,info->ContextRecord->R15);
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
        CloseHandle(file);}
    return EXCEPTION_CONTINUE_SEARCH;
#endif
}
