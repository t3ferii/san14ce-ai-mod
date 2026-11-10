#pragma once
// SAN14_Layout.dll C ABI, version 1 (design: docs/LAYOUT_DLL_COMPAT_DESIGN.md 4.5).
// User exception 2026-11-05: the only extra DLL besides loader/combat/convenience/ccz.
// Shared by the loader, SAN14_Convenience.dll and the Layout fixtures; no game code here.
#include <stdint.h>
static const uint32_t san14_layout_abi=1;
// hook flags
enum : uint32_t {
    LAYOUT_YIELD_IF_PEER=1,      // owner has a known same-function peer mod that is present and enabled: do not install
    LAYOUT_CHAIN_OK=2,           // a slot/site already taken by another module image may be chained (next=that value)
    LAYOUT_LATE_INSTALL=4,       // install after every normal registration of the same publish
    LAYOUT_REFUSE_IF_FOREIGN=8,  // never chain; wins over CHAIN_OK
    LAYOUT_ALL_OR_NONE=32,       // G4: hooks of one owner carrying this flag stand or fall together (a refused/yielded one rolls back the rest)
    LAYOUT_NEED_WHITELIST=16     // vtable replacement must live in SAN14_Convenience/CombatAI/CCZ (design C-1, fog/yuga contested slots)
};
// hook_state values
enum : int {
    LAYOUT_STATE_PENDING=0,LAYOUT_STATE_INSTALLED=1,LAYOUT_STATE_CHAINED=2,LAYOUT_STATE_YIELDED=3,
    LAYOUT_STATE_REFUSED=4,LAYOUT_STATE_BYPASSED=5,LAYOUT_STATE_REMOVED=6,LAYOUT_STATE_STUCK=7
};
// refusal / yield reasons (second log value)
enum : uint32_t {
    LAYOUT_WHY_NONE=0,LAYOUT_WHY_FOREIGN=1,LAYOUT_WHY_HEAP=2,LAYOUT_WHY_OTHER_OWN=3,LAYOUT_WHY_BLOCKED_ENTRY=4,
    LAYOUT_WHY_BAD_SITE=5,LAYOUT_WHY_BYTES=6,LAYOUT_WHY_RANGE=7,LAYOUT_WHY_CAS=8,LAYOUT_WHY_PEER=9,
    LAYOUT_WHY_REDIRECTED=10,LAYOUT_WHY_RELAY=11,LAYOUT_WHY_NOT_WHITELISTED=12,LAYOUT_WHY_GROUP=13
};
// unhook results
enum : int {LAYOUT_UNHOOK_REMOVED=0,LAYOUT_UNHOOK_BYPASSED=1,LAYOUT_UNHOOK_STUCK=2,LAYOUT_UNHOOK_INVALID=-1,LAYOUT_UNHOOK_INACTIVE=-2};
// overlay_subscribe: draw(swapchain,ctx) runs before the next Present in the chain on the render
// thread; DXGI_PRESENT_TEST presents are skipped. wndproc_subscribe: return nonzero to consume
// the message (the subclass then returns 0 without calling later subscribers or the previous proc).
typedef void (*San14LayoutLogSink)(const char *kind,unsigned a,unsigned b,unsigned c);
struct San14LayoutApiV1 {
    uint32_t size,version;
    // Hook manager. Returns a handle > 0 (outcome via hook_state), -1 invalid argument,
    // -2 forbidden entry (hook_block blocklist), -3 table full.
    int (*hook_vtable)(const char *owner,uint32_t slot_rva,uint32_t original_rva,void *replacement,void **next_out,uint32_t flags);
    int (*hook_call)(const char *owner,uint32_t site_rva,uint32_t original_target_rva,void *replacement,void **next_out,uint32_t flags);
    int (*hook_block)(const char *owner,uint32_t rva,const uint8_t *expect,uint32_t len,void *relay,uint32_t flags);
    int (*unhook)(int handle);
    int (*hook_state)(int handle);
    // Layout lifetime + runtime UI (stage 3 / P4, docs/LAYOUT_P4_DIAG.md). on_layout_built: ktid 0 = any KTID, name null = any name,
    // both given = both must match; returns a handle > 0, -1 invalid argument, -3 table full. Callbacks run on the game thread that
    // called the original layout build (86E320) / destruction (865560), never nested (events raised while a callback runs are dropped).
    // The runtime UI entries only work inside such a callback (the game UI thread); outside one they return -1 / nullptr.
    int (*on_layout_built)(uint32_t ktid,const char *name_or_null,void (*cb)(void *layout,void *ctx),void *ctx);
    int (*on_layout_destroyed)(uint32_t ktid,void (*cb)(void *layout,void *ctx),void *ctx);
    void *(*load_sublayout)(void *parent,const char *existing_name,float x,float y);
    int (*attach)(void *parent,int existing_node,void *child);
    void *(*clone_control)(void *layout,int node,int style,int kind);
    void *(*create_label)(void *parent,int existing_node);
    int (*set_text)(void *control,const wchar_t *text);
    int (*set_visible)(void *control,int on);
    int (*bind_click)(void *control,void (*cb)(void *control,void *ctx),void *ctx);
    int (*destroy)(void *created);
    // Overlay / input.
    int (*overlay_subscribe)(void (*draw)(void *swapchain,void *ctx),void *ctx);
    int (*wndproc_subscribe)(int (*proc)(void *hwnd,unsigned msg,uintptr_t w,intptr_t l,void *ctx),void *ctx);
    // Compatibility.
    int (*peer_present)(const wchar_t *module_or_r1_ini);
    int (*slot_owner)(uint32_t rva,wchar_t *out,int cap);
    void (*log)(const char *owner,const char *event,uint32_t a,uint32_t b);
    // Appended by stage 1 (size-gated; V1 consumers check size before use).
    int (*wndproc_bind)(void *hwnd);                  // subclass the game window once; 1 bound, 0 refused
    void (*set_log_sink)(San14LayoutLogSink sink);    // common log_event writer of the host module
    // Appended by stage 2 (P3; size-gated). hook_jump = E9 rel32 tail-jump site (same rules as hook_call).
    // peer_for_owner: 1 when a same-function peer rule of `owner` (docs/LAYOUT_DLL_COMPAT_DESIGN.md 8.2) is satisfied.
    // hook_owner_state: worst state over the owner's hooks (REFUSED/YIELDED win), 0 when the owner has none.
    int (*hook_jump)(const char *owner,uint32_t site_rva,uint32_t original_target_rva,void *replacement,void **next_out,uint32_t flags);
    int (*peer_for_owner)(const char *owner);
    int (*hook_owner_state)(const char *owner);
    // hook_span: E9 relay + NOP fill for 5..32 bytes that may cross 16-byte blocks (block-by-block CMPXCHG16B).
    // hook_bytes: raw byte patch (1..32 bytes, original bytes must match). Both: owner/blocklist/peer rules as hook_block.
    int (*hook_span)(const char *owner,uint32_t rva,const uint8_t *expect,uint32_t len,void *relay,uint32_t flags);
    int (*hook_bytes)(const char *owner,uint32_t rva,const uint8_t *expect,const uint8_t *patch,uint32_t len,uint32_t flags);
    // Appended by stage 3 (P4; size-gated). note_layout_load: called by the owner of the 86C570 call sites BEFORE the original
    // load, records layout->name for the next built event (name is copied). layout_unsubscribe: handle of on_layout_built /
    // on_layout_destroyed. set_position/get_position: integer control position (virtual +0x78 / fields +0x20,+0x24).
    void (*note_layout_load)(void *layout,const char *name,int async);
    int (*layout_unsubscribe)(int subscription);
    int (*set_position)(void *control,int x,int y);
    int (*get_position)(void *control,int *x,int *y);
    // Appended by the address-table work (user 2026-11-05 patch resilience, docs/PATCH_RESILIENCE.md; size-gated).
    // ga_resolve: RVA of address table id (src/game_addresses.h GaId) in the running build, 0 = unresolved.
    // ga_table: the Layout-resolved table (GA_COUNT values; *count receives GA_COUNT) for modules that start before/without own resolution.
    uint32_t (*ga_resolve)(uint32_t id);
    const uint32_t *(*ga_table)(uint32_t *count);
    // Appended by the UI-ownership work (user 2026-11-07, docs/UI_RESOURCE_LIFECYCLE.md 10; size-gated). A feature module brackets its UI
    // calls with ui_begin(owner)/ui_end() on the game UI thread it was called on; ui_begin returns 0 unless the owner's registry hooks are
    // installed/chained (feature published). Inside a scope the runtime UI calls above (clone_control, set_text, bind_click, set_visible)
    // work outside layout events and tag their objects with the owner.
    //  ui_create_window: descriptor window (870410, same assembly as the native sub-panes); ui_activate: vt+0x38 (register with the UI manager);
    //  ui_node/ui_set_node_text/ui_set_node_visible: 92CEE0/930D20/931DA0 on any layout table; ui_select: radio setter (radios made by clone_control kind 1);
    //  ui_sync_visibility: own follows source visibility/alpha; ui_release(owner,next): original destroy sequence of every pane of the owner
    //  (vt+0x40, vt+0x28(obj,next), vt[0](obj,1)), `next` = second argument of the screen's leave. Returns panes released, -1 outside a scope.
    int (*ui_begin)(const char *owner);
    void (*ui_end)();
    void *(*ui_create_window)(void *parent,int x,int y,int w,int h,const void *desc,int count);
    int (*ui_activate)(void *pane);
    void *(*ui_node)(void *layout,int node);
    int (*ui_set_node_text)(void *layout,int node,const wchar_t *text);
    int (*ui_set_node_visible)(void *layout,int node,int on);
    int (*ui_select)(void *radio,int on);
    int (*ui_sync_visibility)(void *own,void *source);
    int (*ui_release)(const char *owner,void *next);
    // Appended by the tactic priority fixes (user 2026-11-07; size-gated, V1 consumers check the size). Both need an open ui scope.
    //  ui_enable(control,on): a control made inside a scope is disabled (input flag +0x40 bit 4 cleared: the original CCmnButton update 860830 then shows its disabled state and presses are ignored) or restored;
    //  ui_tooltip(text): the owner's mouse-over balloon (the CSimpleBalloonLayout trait_hover builds, same assembly) shows `text` next to the
    //  mouse, nullptr hides it; ui_release destroys it with the owner's panes.
    int (*ui_enable)(void *control,int on);
    int (*ui_tooltip)(const wchar_t *text);
};
// destroy() results: 0 destroyed now, 1 hidden and left to its owner layout (freed when the owner dies), -1 rejected (not created by us / outside an event).
enum : int {LAYOUT_DESTROY_DONE=0,LAYOUT_DESTROY_DEFERRED=1,LAYOUT_DESTROY_REJECTED=-1};
typedef const San14LayoutApiV1 *(__cdecl *GetLayoutApi)(uint32_t);
typedef bool (__cdecl *InitializeLayout)(uint32_t);
typedef void (__cdecl *PublishLayout)();
