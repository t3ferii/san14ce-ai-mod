// ui-callable service (docs/MODULARIZATION_PLAN.md W4-4): the game's std::function callable in its MSVC layout, shared by every module
// that hands a click callback to a native control (lord_change footer button, the CCZ title button). Moved out of native_lord_change.inl
// unchanged so that ccz_title no longer reaches into lord_change.
// std::function callable in the game's MSVC layout (vtable: copy, move, call, target type, delete, get),
// as 565EB0/65DFE0 implement it: [0] vtable, [8] callback; stored inline in the caller's 0x40 buffer.
namespace ui_callable {
struct Callable {void **vtable;void (*callback)(Obj);};
static void *__fastcall callable_copy(Callable *self,Callable *where) { COST_SCOPE("ui_callable::callable_copy");if(where)*where=*self;return where;}
static void __fastcall callable_call(Callable *self,Obj *arg) { COST_SCOPE("ui_callable::callable_call");self->callback(arg?*arg:nullptr);}
static void *__fastcall callable_type(Callable *) { COST_SCOPE("ui_callable::callable_type");return nullptr;}
static void __fastcall callable_delete(Callable *self,bool heap) { COST_SCOPE("ui_callable::callable_delete");if(heap)fn<void(__fastcall *)(void *)>(GA(GA_3F9170))(self);}
static void *__fastcall callable_get(Callable *self) { COST_SCOPE("ui_callable::callable_get");return &self->callback;}
static void *vtable[6]={reinterpret_cast<void *>(&callable_copy),reinterpret_cast<void *>(&callable_copy),
    reinterpret_cast<void *>(&callable_call),reinterpret_cast<void *>(&callable_type),reinterpret_cast<void *>(&callable_delete),
    reinterpret_cast<void *>(&callable_get)};
}
