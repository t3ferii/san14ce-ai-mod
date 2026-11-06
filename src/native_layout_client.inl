// Client side of SAN14_Layout.dll's hook manager for SAN14_Convenience.dll (user exception 2026-11-05, design 8, P3).
// UI hooks (vtable slots, E8 call sites, the 898957 E9 tail jump, blocks/spans) are registered here by RVA; the
// static parameters (owner, kind, original target, manager flags) come from layout_registry.h, the replacement
// bodies stay in this module (design C-1). Layout installs them all in one publish, chains over or yields to other
// mods, and rolls an owner back together (ALL_OR_NONE). Without Layout the UI features stay off: there is no
// direct-hook fallback. Replacement functions must call `next` (the manager writes it before the hook is
// reachable); calling the original RVA directly would cut other mods out of the chain (design 8.0 G1).
#include "layout_api.h"
#include "layout_registry.h"
// Production convenience builds route their UI hooks through the manager. Split fixtures and the legacy integrated
// build keep the direct path against their synthetic images (no Layout DLL there); the Layout registry fixture
// covers the routed path.
#if defined(SPLIT_BASE) && !defined(SPLIT_MODULE_FIXTURE) && !defined(TACTICS_FIXTURE)
#define LAYOUT_ROUTED 1
#endif
namespace layout_client {
static const San14LayoutApiV1 *found;
static INIT_ONCE once=INIT_ONCE_STATIC_INIT;
static BOOL CALLBACK find_api(PINIT_ONCE,PVOID,PVOID *) {
    HMODULE module=GetModuleHandleW(L"SAN14_Layout.dll");
    auto get_api=module?reinterpret_cast<GetLayoutApi>(GetProcAddress(module,"SAN14LayoutGetApi")):nullptr;
    const San14LayoutApiV1 *api=get_api?get_api(san14_layout_abi):nullptr;
    if(api && api->version==san14_layout_abi && api->size>=sizeof(San14LayoutApiV1) && api->overlay_subscribe &&
       api->wndproc_subscribe && api->wndproc_bind && api->set_log_sink && api->hook_vtable && api->hook_call &&
       api->hook_jump && api->hook_block && api->hook_span && api->hook_bytes && api->peer_for_owner) {
        found=api;
        if(api->ga_table) {unsigned count=0;const uint32_t *table=api->ga_table(&count);ga::adopt(table,count);}   // unknown build: the Layout-resolved address table
    }
    return TRUE;
}
static const San14LayoutApiV1 *get() {InitOnceExecuteOnce(&once,find_api,nullptr,nullptr);return found;}
#ifndef NO_RUNTIME_LOG
// Layout decisions (installed/chained/yielded/refused/bypassed) go to this module's common log_event writer.
static void __cdecl sink(const char *kind,unsigned a,unsigned b,unsigned c) {log_event(kind,a,b,c);}
static void connect_log() {if(const San14LayoutApiV1 *api=get())api->set_log_sink(&sink);}
#endif
static const LayoutRow *row_for(unsigned rva) {
    for(unsigned i=0;i<layout_row_count;++i)if(layout_rows[i].rva==rva)return &layout_rows[i];
    return nullptr;
}
static const unsigned kMaxHandles=384;
static int handles[kMaxHandles];
static unsigned handle_count;
static int remember(int handle) {
    if(handle>0 && handle_count<kMaxHandles)handles[handle_count++]=handle;
    return handle;
}
// Registers one registry row. `next` (optional) is preset to the original target so a direct call before the
// manager installs the hook still reaches the game. `drop` removes manager flags for this owner instance.
static int add(unsigned rva,unsigned char kind,void *replacement,void **next,uint32_t drop=0) {
    const San14LayoutApiV1 *api=get();
    const LayoutRow *row=row_for(rva);
    if(!api || !row || row->kind!=kind || !replacement || !image) {log_event("layout_register_rejected",rva,api?1u:0u,row?1u:0u);return 0;}
    if(next)*next=image+row->original;
    const uint32_t flags=row->flags&~drop;
    int handle=0;
    switch(kind) {
    case LR_VT:handle=api->hook_vtable(row->owner,row->rva,row->original,replacement,next,flags);break;
    case LR_CALL:handle=api->hook_call(row->owner,row->rva,row->original,replacement,next,flags);break;
    default:handle=api->hook_jump(row->owner,row->rva,row->original,replacement,next,flags);break;
    }
    if(handle<=0)log_event("layout_register_failed",rva,static_cast<unsigned>(-handle),0);
    return remember(handle);
}
static int vt(unsigned slot,void *replacement,void **next,uint32_t drop=0) {return add(slot,LR_VT,replacement,next,drop);}
static int call(unsigned site,void *replacement,void **next,uint32_t drop=0) {return add(site,LR_CALL,replacement,next,drop);}
static int jump(unsigned site,void *replacement,void **next,uint32_t drop=0) {return add(site,LR_JUMP,replacement,next,drop);}
// Block, span and raw-byte registrations (owner and flags given by the caller; the blocklist applies).
static int block(const char *owner,unsigned rva,const unsigned char *expect,unsigned len,void *relay,uint32_t flags) {
    const San14LayoutApiV1 *api=get();
    return api?remember(api->hook_block(owner,rva,expect,len,relay,flags)):0;
}
static int span(const char *owner,unsigned rva,const unsigned char *expect,unsigned len,void *relay,uint32_t flags) {
    const San14LayoutApiV1 *api=get();
    return api?remember(api->hook_span(owner,rva,expect,len,relay,flags)):0;
}
static int bytes(const char *owner,unsigned rva,const unsigned char *expect,const unsigned char *patch,unsigned len,uint32_t flags) {
    const San14LayoutApiV1 *api=get();
    return api?remember(api->hook_bytes(owner,rva,expect,patch,len,flags)):0;
}
// Same-function peer decision for owners that apply their patches themselves (imported code): true = yield.
// The decision is logged like a manager yield so every owner shows up as installed/yielded in the common log.
static bool yield_to_peer(const char *owner,unsigned rva) {
    const San14LayoutApiV1 *api=get();
    if(!api || !api->peer_for_owner(owner))return false;
    api->log(owner,"layout_yielded",rva,LAYOUT_WHY_PEER);
    return true;
}
// P4: the owner of the 86C570 call sites reports (layout, name) BEFORE each original load so Layout can name the next built
// event. Layout needs the names in diagnostic builds (KTID offset probe) and whenever a module subscribed to layout events.
static void note_load(void *layout,const char *name,int async) {
    const San14LayoutApiV1 *api=get();
    if(api && api->note_layout_load)api->note_layout_load(layout,name,async);
}
static bool names_wanted() {
#ifndef NO_RUNTIME_LOG
    return get()!=nullptr;
#else
    return false;   // production: no subscriber exists yet (P5 adds the subscribing features)
#endif
}
// A module that registers several hooks takes all of them back when one registration fails: otherwise Layout would
// publish a partial owner that never learned its missing hooks (design G4). `done=true` keeps the registrations.
// 2026-11-05 local3 (user: a failed optional feature switches only itself off): `current_mark()` before a feature's registrations,
// `rollback_to(mark)` takes exactly that feature's still unpublished registrations back; every other owner stays registered.
static unsigned current_mark() {return handle_count;}
static void rollback_to(unsigned mark) {
    const San14LayoutApiV1 *api=get();
    if(api)for(unsigned i=handle_count;i>mark;--i)api->unhook(handles[i-1]);
    if(handle_count>mark)handle_count=mark;
}
struct Transaction {
    unsigned mark;bool done;
    Transaction():mark(handle_count),done(false) {}
    ~Transaction() {if(!done)rollback_to(mark);}
};
// Rolls back registrations that were not published yet (split_rollback before the Layout publish).
static void cancel_all() {
    const San14LayoutApiV1 *api=get();
    if(api)for(unsigned i=handle_count;i>0;--i)api->unhook(handles[i-1]);
    handle_count=0;
}
}
// Routing helpers for modules that also compile into split fixtures (synthetic images, no Layout DLL).
#ifdef LAYOUT_ROUTED
#define ROUTE_VT(slot,original,replacement,next) (layout_client::vt((slot),reinterpret_cast<void *>(replacement),reinterpret_cast<void **>(next))>0)
#else
#define ROUTE_VT(slot,original,replacement,next) (patch_pointer((slot),(original),reinterpret_cast<void *>(replacement)) && ((*reinterpret_cast<void **>(next)=image+(original)),true))
#endif
