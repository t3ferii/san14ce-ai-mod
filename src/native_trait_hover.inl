// Trait / tactic / editor mouse-over tooltips (docs/TRAIT_HOVER_PLAN.md S4, 2026-11-06). Own source re-implementation of the
// ModPack v1.6 TraitHover blob (research/ghidra_refs/SAN14_TraitHover_8cfad1a6ada78eca/c/0000.c); no blob machine code runs.
// Hooks: owner "trait_hover" rows of layout_registry.h (152 vtable slots + 4 duel 930D20 call sites), registered through
// layout_client::vt/call in one Transaction (all or nothing). All judgement / game calls happen inside those hooks, i.e. on the
// game's own UI thread; activation only registers. Pure decisions come from trait_hover_profile.h (namespace thp, fixture-tested).
// Included by native_modpack.inl (SPLIT_MODPACK), just before native_traits9.inl.
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include "trait_hover_profile.h"
// Internal flags (no settings, no new ini keys). trait_hover_off: a guard/prepare/install failure switches only this feature off.
// trait_hover_yield: a same-function peer (R1 ModPack TraitHover) owns the tooltips (native_split_install.inl split_layout_yield).
static bool trait_hover_off,trait_hover_yield;
static bool trait_hover_wanted() {return mod_features.tooltip_collab && !trait_hover_off && !trait_hover_yield;}
namespace trait_hover {
using ThSlot=uint64_t(__fastcall *)(Obj,uint64_t,uint64_t,uint64_t);
using DuelFn=uint64_t(__fastcall *)(Obj,unsigned,const wchar_t *,uint64_t);
using ValidFn=int(__fastcall *)(Obj);
static const unsigned kVtRows=152,kCallRows=4,kRows=kVtRows+kCallRows,kTableMax=64;

// ------------------------------------------------------------------------------------------------------------------------
// state (UI thread only)
// ------------------------------------------------------------------------------------------------------------------------
static bool installed,runtime_off;
static bool path_off[6];                    // by thp::Path: a fault in a path's resolver switches only that path off
static void *next_row[kRows];               // per owner row: the Layout chain's next function
static void *next_duel[kCallRows];
static thp::SlotEntry slots[kVtRows];       // sorted by slot rva
static unsigned table_vts[kTableMax],body_vts[kTableMax];
static size_t table_count,body_count;
static thp::PathVts path_vts;
static thp::RecordTable records;
static unsigned text_lo,text_hi;
static bool text_range_done;

// diagnostics: every event is bounded (once per key or a small total cap)
static bool seen_tick[6],seen_route[7][7],seen_header[5],seen_balloon[3],seen_fault[0x20],seen_unknown;
static unsigned shown_count,failed_count,capture_count,tactic_logged;
static void note_tick(unsigned path,unsigned vt) {
    if(path<6 && !seen_tick[path]) {seen_tick[path]=true;log_event("trait_hover_tick",path,vt,0);}
}
static void note_route(unsigned path,unsigned cls,uint64_t key) {
    if(path<7 && cls<7 && !seen_route[path][cls]) {seen_route[path][cls]=true;log_event("trait_hover_route",path,cls,static_cast<unsigned>(key));}
}
static void note_fault(unsigned path,unsigned code) {
    if(path<0x20 && !seen_fault[path]) {seen_fault[path]=true;log_event("trait_hover_fault",path,code,0);}
}
static void note_text_failed(unsigned why,uint64_t key,unsigned path) {
    if(failed_count<16) {++failed_count;log_event("trait_hover_text_failed",why,static_cast<unsigned>(key),path);}
}
static void note_shown(unsigned cls,uint64_t key,unsigned path) {
    if(shown_count<32) {++shown_count;log_event("trait_hover_shown",cls,static_cast<unsigned>(key),path);}
}

// ------------------------------------------------------------------------------------------------------------------------
// raw object access (all callers sit inside an SEH helper)
// ------------------------------------------------------------------------------------------------------------------------
static thp::WidgetView view_of(const void *w) {
    const unsigned char *p=static_cast<const unsigned char *>(w);
    thp::WidgetView v;
    v.width=*reinterpret_cast<const int *>(p+0x28);v.height=*reinterpret_cast<const int *>(p+0x2C);
    v.flags=*reinterpret_cast<const unsigned *>(p+0x40);
    v.ox=*reinterpret_cast<const int *>(p+0x20);v.oy=*reinterpret_cast<const int *>(p+0x24);
    v.parent=*reinterpret_cast<void *const *>(p+8);
    return v;
}
static const void *parent_of(const void *w) {return *reinterpret_cast<void *const *>(static_cast<const unsigned char *>(w)+8);}
static bool hit(Obj w,int px,int py,bool xonly) {return thp::contains(w,px,py,xonly,view_of);}
static unsigned vt_rva(Obj o) {return o?static_cast<unsigned>(reinterpret_cast<uintptr_t>(pointer(o,0))-reinterpret_cast<uintptr_t>(image)):0;}
static bool is_edit_vt(Obj row) {
    Obj vt=row?pointer(row,0):nullptr;
    return vt && (vt==image+GA(GA_13A4298) || vt==image+GA(GA_13A42C8));
}
static bool valid(Obj o) {return o && fn<ValidFn>(GA(GA_32F500))(o)!=0;}
static void *vfn(Obj o,unsigned off) {return pointer(pointer(o,0),off);}
static Obj trait_object(Obj root,int id) {return root && thp::trait_id_ok(id)?pointer(root,0x7DD18+id*8):nullptr;}
static Obj tactic_object(Obj root,int i) {return root && i>=0 && i<thp::kTacticCount?pointer(root,0x774D0+i*8):nullptr;}

static void text_range() {
    if(text_range_done)return;
    text_range_done=true;
    __try {
        auto dos=reinterpret_cast<const IMAGE_DOS_HEADER *>(image);
        auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64 *>(image+dos->e_lfanew);
        auto sec=IMAGE_FIRST_SECTION(nt);
        for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
            const bool named=!memcmp(sec[i].Name,".text",6);
            if(named || (!text_hi && (sec[i].Characteristics&IMAGE_SCN_CNT_CODE))) {
                text_lo=sec[i].VirtualAddress;text_hi=text_lo+sec[i].Misc.VirtualSize;
                if(named)break;
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {text_lo=text_hi=0;}
}

// ------------------------------------------------------------------------------------------------------------------------
// tactic feature tab (TRAIT_HOVER_TACTIC_TEXT.md 2.4): the game's own five column formatters plus their caption messages.
// empty_mask bit k: value empty, bit 8+k: caption empty (diagnostic only)
// ------------------------------------------------------------------------------------------------------------------------
static std::wstring tactic_text(Obj t,unsigned *empty_mask=nullptr) {
    const wchar_t *caption[thp::kTacticFields],*value[thp::kTacticFields];
    unsigned mask=0;
    for(unsigned k=0;k<thp::kTacticFields;++k) {
        caption[k]=fn<const wchar_t *(__fastcall *)(unsigned)>(GA(GA_30CF80))(thp::tactic_caption_id(k));
        value[k]=fn<const wchar_t *(__fastcall *)(Obj)>(thp::tactic_func_rva(k))(t);
        if(!value[k] || !*value[k])mask|=1u<<k;
        if(!caption[k] || !*caption[k])mask|=1u<<(8+k);
    }
    if(empty_mask)*empty_mask=mask;
    return thp::compose_tactic_text(caption,value);
}

// ------------------------------------------------------------------------------------------------------------------------
// key resolution (1.4). Runs only through resolve_seh().
// ------------------------------------------------------------------------------------------------------------------------
// name -> key (blob FUN_15F0): edit row, then pointer-identical trait name (valid), then tactic name (game text needed)
static uint64_t name_key(Obj row,const wchar_t *name) {
    Obj root=world();
    const bool edit_row=row && name && static_cast<const void *>(name)==static_cast<const void *>(row+0xC) && is_edit_vt(row) && *name!=0;
    int tid=0,ti=-1;bool tvalid=false,has_text=false;
    if(!edit_row && root && name) {
        tid=thp::find_name_id(reinterpret_cast<uintptr_t>(name),1,thp::kTraitMax,[&](int id)->uintptr_t {
            Obj t=trait_object(root,id);return t?reinterpret_cast<uintptr_t>(t+0x10):0;});
        if(tid)tvalid=valid(trait_object(root,tid));
        if(!(tid && tvalid)) {
            const int k=thp::find_name_id(reinterpret_cast<uintptr_t>(name),1,thp::kTacticCount,[&](int id)->uintptr_t {
                Obj t=tactic_object(root,id-1);return t?reinterpret_cast<uintptr_t>(t+0x10):0;});
            if(k) {
                ti=k-1;
                Obj t=tactic_object(root,ti);
                has_text=valid(t) && !tactic_text(t).empty();
            }
        }
    }
    return thp::name_key_decide(edit_row,reinterpret_cast<uint64_t>(row),tid,tvalid,ti,has_text);
}
// column header handler -> trait id (1.5): decode the 44-byte "has trait ? circle : empty" function
static uint64_t header_key(Obj handler) {
    text_range();
    const thp::HeaderTargets targets={GA(GA_232D00),GA(GA_30CF80),GA(GA_1A47B98)};
    const uintptr_t base=reinterpret_cast<uintptr_t>(image),p=reinterpret_cast<uintptr_t>(handler);
    thp::HeaderReject why=thp::HR_RANGE;int id=0;unsigned rva=0;
    if(handler && p>base && p-base<0xFFFFFFFFull) {
        rva=static_cast<unsigned>(p-base);
        why=thp::decode_header_handler(image+rva,text_hi>rva?text_hi-rva:0,rva,text_lo,text_hi,targets,&id);
    }
    if(why==thp::HR_OK)return static_cast<uint64_t>(id);
    if(why<5 && !seen_header[why]) {seen_header[why]=true;log_event("trait_hover_header_reject",why,rva,0);}
    return 0;
}
// table path (FUN_1000). header_mode: the key came from a column header (route class 4) and not from a cell.
static uint64_t table_key(Obj owner,Obj target,int px,int py,bool &header_mode) {
    header_mode=false;
    if(!owner || !target || !hit(owner,px,py,false))return 0;
    const int tab=*reinterpret_cast<int *>(owner+0x208);
    if(!thp::tab_ok(tab,*reinterpret_cast<int *>(owner+0x20C)))return 0;
    Obj groups=pointer(owner,0x150),columns=pointer(owner,0x158),bodies=pointer(owner,0x130);
    if(!groups || !columns || !bodies || !pointer(groups,tab*0x18))return 0;
    Obj body=pointer(bodies,tab*8),row=nullptr;
    if(body && thp::in_set(body_vts,body_count,vt_rva(body)) && pointer(body,8)==owner &&
       pointer(target,0x48)==body && hit(body,px,py,false) && hit(target,px,py,false)) {
        Obj a=pointer(body,0x138);Obj b=a?pointer(a,0x20):nullptr;Obj res=b?pointer(b,0x20):nullptr;
        if(!res)return 0;
        const int vis=*reinterpret_cast<int *>(target+0x80);
        if(!thp::visible_index_ok(vis,*reinterpret_cast<int *>(res+0x58)))return 0;
        const int idx=fn<int(__fastcall *)(Obj,int)>(GA(GA_AE0370))(res,vis);
        Obj rows=pointer(body,0x198);
        if(!thp::row_index_ok(idx,*reinterpret_cast<int *>(body+0x1A0)) || !rows)return 0;
        row=pointer(rows,idx*8);
        if(!row)return 0;
    }
    Obj headers=pointer(owner,0x210),headers_end=pointer(owner,0x218);
    if(!headers)return 0;
    const unsigned long long used=static_cast<unsigned long long>(headers_end-headers);
    if(!thp::header_array_ok(used))return 0;
    const unsigned n=thp::header_count(used);
    for(unsigned j=1;j<n;++j) {
        Obj h=pointer(headers,j*8);
        if(!h)continue;
        if(row?!hit(h,px,py,true):h!=target)continue;   // cell mode: header column under the cursor (x only); header mode: the target itself
        const unsigned col=*reinterpret_cast<unsigned *>(h+0x80);
        if(!thp::header_col_ok(col,j))return 0;
        const int di=thp::descriptor_index(reinterpret_cast<const int *>(pointer(groups,tab*0x18)),col);
        if(di<0)return 0;
        Obj desc=columns+static_cast<size_t>(di)*0x60;
        if(*reinterpret_cast<int *>(desc+0x5C)==0)return 0;
        Obj handler=pointer(desc,0x18);
        if(!row) {                                   // header mode: the header itself under the cursor
            if(!hit(h,px,py,false))return 0;
            header_mode=true;
            return header_key(handler);
        }
        if(!handler)return 0;
        const wchar_t *name=reinterpret_cast<const wchar_t *(__fastcall *)(Obj)>(handler)(row);
        return name_key(row,name);
    }
    return 0;
}
// summary path (FUN_23C0): the nine trait buttons of CInfoSummaryPersonality
static uint64_t summary_key(Obj self,Obj target,int px,int py) {
    if(!hit(self,px,py,false))return 0;
    Obj army=pointer(self,0x178);
    if(!valid(army))return 0;
    Obj person=fn<Obj(__fastcall *)(Obj)>(GA(GA_401C0))(army);
    if(!valid(person))return 0;
    for(unsigned i=0;i<9;++i) {
        Obj button=pointer(self,0x180+i*8);
        if(!thp::summary_button_hit(button!=nullptr,button && hit(button,px,py,false),button && thp::is_or_descends(target,button,parent_of)))continue;
        const unsigned slot=self[0x1C8+i];
        if(!thp::summary_slot_ok(slot))return 0;
        return thp::summary_key(fn<int(__fastcall *)(Obj,unsigned)>(GA(GA_2305D0))(person,slot));
    }
    return 0;
}
// editor path (FUN_2790): the selected edit row of CEditNewPersonalityLayout
static uint64_t edit_key(Obj self,Obj target,int px,int py) {
    Obj row=pointer(self,0x1A8),w=pointer(self,0x1B0);
    thp::EditProbe p={};
    p.row_nonnull=row!=nullptr;p.w_nonnull=w!=nullptr;p.input_ok=true;p.target_ok=target!=nullptr;
    p.row_vt_edit=row && is_edit_vt(row);
    if(!(p.row_nonnull && p.w_nonnull && p.target_ok && p.row_vt_edit))return 0;
    p.owner_hit=hit(self,px,py,false);
    p.w_hit=hit(w,px,py,false);
    p.target_in_w=thp::is_or_descends(target,w,parent_of);
    p.row_text_first=*reinterpret_cast<unsigned short *>(row+0xC);
    return thp::edit_key(p,reinterpret_cast<uint64_t>(row));
}
struct Resolve {uint64_t key;unsigned route;};
static unsigned route_of(unsigned path,bool header_mode,uint64_t key) {
    if(!key)return 0;
    if(path==thp::PATH_SUMMARY)return 5;
    if(path==thp::PATH_DUEL)return 6;
    if(path==thp::PATH_EDIT)return 3;
    if(header_mode)return 4;
    const thp::KeyClass c=thp::key_class(key);
    return c==thp::KC_TRAIT?1u:c==thp::KC_TACTIC?2u:c==thp::KC_EDIT?3u:0u;
}
static void resolve_impl(unsigned path,Obj self,const thp::Record *rec,Resolve &r) {
    r.key=0;r.route=0;
    Obj app=fn<Obj(__fastcall *)()>(GA(GA_10FC0))();
    Obj input=fn<Obj(__fastcall *)()>(GA(GA_11290))();
    if(!app || !input)return;
    Obj target=fn<Obj(__fastcall *)(Obj)>(GA(GA_86B3B0))(app);
    if(!target)return;
    const int px=*reinterpret_cast<int *>(input+0x34),py=*reinterpret_cast<int *>(input+0x38);
    bool header_mode=false;
    switch(path) {
    case thp::PATH_TABLE:case thp::PATH_BODY:r.key=table_key(self,target,px,py,header_mode);break;
    case thp::PATH_SUMMARY:r.key=summary_key(self,target,px,py);break;
    case thp::PATH_EDIT:r.key=edit_key(self,target,px,py);break;
    case thp::PATH_DUEL:
        if(rec)r.key=static_cast<uint64_t>(thp::duel_tick_key(reinterpret_cast<const uintptr_t *>(rec->labels),rec->ids,thp::kDuelLabels,
            reinterpret_cast<uintptr_t>(target),[&](size_t i)->bool {return hit(reinterpret_cast<Obj>(rec->labels[i]),px,py,false);}));
        break;
    default:break;
    }
    r.key=thp::sanitize_key(r.key);
    r.route=route_of(path,header_mode,r.key);
}
static bool resolve_seh(unsigned path,Obj self,const thp::Record *rec,Resolve &r,unsigned &code) {
    __try {resolve_impl(path,self,rec,r);return true;}
    __except(code=GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {return false;}
}

// ------------------------------------------------------------------------------------------------------------------------
// balloon (1.9): CSimpleBalloonLayout, created in the game's own inline order; failed state is never retried
// ------------------------------------------------------------------------------------------------------------------------
static Obj make_tip(thp::Record &rec) {
    if(rec.failed)return nullptr;
    if(rec.tip)return reinterpret_cast<Obj>(rec.tip);
    unsigned result=2;
    Obj tip=fn<Obj(__fastcall *)(size_t)>(GA(GA_3F6BC0))(0x180);
    if(tip) {
        fn<Obj(__fastcall *)(Obj)>(GA(GA_864C50))(tip);
        fn<void(__fastcall *)(Obj)>(GA(GA_92AB70))(tip+0x130);
        *reinterpret_cast<Obj *>(tip)=image+GA(GA_139C540);
        *reinterpret_cast<Obj *>(tip+0x178)=nullptr;
        rec.tip=reinterpret_cast<uintptr_t>(tip);
        fn<void(__fastcall *)(Obj,int)>(GA(GA_89C320))(tip,0);
        reinterpret_cast<void(__fastcall *)(Obj,int)>(vfn(tip,0xC8))(tip,0);
        result=0;
        if(pointer(tip,0x138)) {
            tip[0x40]|=8;
            fn<void(__fastcall *)(Obj,int)>(GA(GA_8A18C0))(tip,0xFF);
            reinterpret_cast<void(__fastcall *)(Obj,int)>(vfn(tip,0x70))(tip,6);
            result=1;
        }
    }
    if(!seen_balloon[result]) {seen_balloon[result]=true;log_event("trait_hover_balloon",result,vt_rva(tip),0);}
    if(result==1)return tip;
    rec.failed=1;
    return nullptr;
}
static void hide_tip(thp::Record &rec) {
    Obj tip=reinterpret_cast<Obj>(rec.tip);
    if(tip)reinterpret_cast<void(__fastcall *)(Obj,int)>(vfn(tip,0xC8))(tip,0);
    rec.last_key=0;
}
static void set_text(Obj tip,const wchar_t *text) {fn<void(__fastcall *)(Obj,const wchar_t *)>(GA(GA_8A2140))(tip,text);}
// edit rows (1.6): the seven "caption: value" lines of the selected personality row, rebuilt every frame like the blob
static std::wstring edit_text(Obj row) {
    const wchar_t *caption[7],*value[7];
    for(unsigned i=0;i<7;++i) {
        caption[i]=fn<const wchar_t *(__fastcall *)(unsigned)>(GA(GA_30CF80))(thp::edit_title_id(i));
        value[i]=fn<const wchar_t *(__fastcall *)(Obj)>(thp::edit_func_rva(i))(row);
    }
    return thp::compose_edit_text(caption,value,7);   // like the blob: every line kept, never empty
}
// FUN_20E0: show the tooltip for `key` on `self`, or hide it for key 0
static void show_impl(Obj self,uint64_t key,unsigned path,unsigned route) {
    key=thp::sanitize_key(key);
    const uintptr_t owner=reinterpret_cast<uintptr_t>(self);
    thp::Record *rec=records.find(owner);
    if(!rec) {
        if(!key)return;
        if(thp::slot_find_for_vt(slots,kVtRows,vt_rva(self),thp::SK_TICK)<0)return;   // only owners of a registered row
        rec=records.acquire(owner);
        if(!rec) {if(records.take_full_log())log_event("trait_hover_records_full",thp::kRecordCap,0,0);return;}
    }
    if(!key) {if(rec->last_key)hide_tip(*rec);rec->last_key=0;return;}
    Obj root=world();
    const thp::KeyClass kc=thp::key_class(key);
    Obj t=nullptr;
    unsigned fail=0;
    if(kc==thp::KC_TRAIT) {
        t=trait_object(root,static_cast<int>(key));
        if(!t || !valid(t))fail=1;
    } else if(kc==thp::KC_TACTIC) {
        t=tactic_object(root,thp::tactic_index(key));
        if(!t || !valid(t))fail=1;
    } else if(kc==thp::KC_EDIT) {
        if(!is_edit_vt(reinterpret_cast<Obj>(key)))fail=6;
    } else fail=6;
    if(fail) {hide_tip(*rec);if(fail!=6)note_text_failed(fail,key,path);return;}
    Obj tip=make_tip(*rec);
    if(!tip)return;
    const bool changed=key!=rec->last_key;
    std::wstring tactic;
    if(kc==thp::KC_TACTIC && changed) {
        unsigned mask=0;
        tactic=tactic_text(t,&mask);
        if(tactic.empty()) {hide_tip(*rec);note_text_failed(3,key,path);return;}
        if(tactic_logged<8) {++tactic_logged;log_event("trait_hover_tactic_text",static_cast<unsigned>(thp::tactic_index(key)),static_cast<unsigned>(tactic.size()),mask);}
    }
    if(kc==thp::KC_EDIT) {
        const std::wstring text=edit_text(reinterpret_cast<Obj>(key));
        set_text(tip,text.c_str());
    } else if(changed) {
        if(kc==thp::KC_TRAIT) {
            const wchar_t *text=fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_3594B0))(t);
            if(!text || !*text) {hide_tip(*rec);note_text_failed(2,key,path);return;}
            set_text(tip,text);
        } else set_text(tip,tactic.c_str());
    }
    if(changed)note_shown(route,key,path);
    rec->last_key=key;
    if(!rec->registered) {reinterpret_cast<void(__fastcall *)(Obj)>(vfn(tip,0x38))(tip);rec->registered=1;}
    alignas(16) unsigned char m[0x40]={};
    *reinterpret_cast<Obj *>(m+0x10)=tip;
    *reinterpret_cast<int *>(m+0x38)=20;*reinterpret_cast<int *>(m+0x3C)=20;
    fn<void(__fastcall *)(void *)>(GA(GA_8A5D60))(m);
    reinterpret_cast<void(__fastcall *)(Obj,int)>(vfn(tip,0xC8))(tip,1);
}
static bool show_seh(Obj self,uint64_t key,unsigned path,unsigned route,unsigned &code) {
    __try {show_impl(self,key,path,route);return true;}
    __except(code=GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER) {return false;}
}
// FUN_1D60: hide (destroy=false) or destroy the owner's balloon and record
static void release_impl(Obj self,bool destroy,uint64_t arg) {
    const uintptr_t owner=reinterpret_cast<uintptr_t>(self);
    thp::Record *rec=records.find(owner);
    if(!rec)return;
    Obj tip=reinterpret_cast<Obj>(rec->tip);
    if(!tip) {
        rec->registered=0;rec->last_key=0;
        if(destroy)records.release(owner);
        return;
    }
    reinterpret_cast<void(__fastcall *)(Obj,int)>(vfn(tip,0xC8))(tip,0);
    if(rec->registered)reinterpret_cast<void(__fastcall *)(Obj)>(vfn(tip,0x40))(tip);
    if(!destroy) {rec->registered=0;rec->last_key=0;return;}
    reinterpret_cast<void(__fastcall *)(Obj,uint64_t)>(vfn(tip,0x28))(tip,arg);
    reinterpret_cast<void(__fastcall *)(Obj,int)>(vfn(tip,0))(tip,1);
    records.release(owner);
}
static void release(Obj self,bool destroy,uint64_t arg) {
    if(!self)return;
    __try {release_impl(self,destroy,arg);}
    __except(EXCEPTION_EXECUTE_HANDLER) {
        runtime_off=true;records.release(reinterpret_cast<uintptr_t>(self));
        note_fault(destroy?0x1Du:0x1Cu,GetExceptionCode());
    }
}

// ------------------------------------------------------------------------------------------------------------------------
// hook bodies
// ------------------------------------------------------------------------------------------------------------------------
// next function of the row owning `self`'s vtable slot (kind picks +0x08 tick / +0x28 close / +0x40 hide); an unknown vtable
// falls back to the original function of that replacement.
static ThSlot pick_next(Obj self,thp::SlotKind kind,unsigned original) {
    const unsigned vt=vt_rva(self);
    const int row=thp::slot_find_for_vt(slots,kVtRows,vt,kind);
    if(row<0 && !seen_unknown) {seen_unknown=true;log_event("trait_hover_slot_unknown",static_cast<unsigned>(kind),vt,0);}
    return next_or<ThSlot>(row>=0?next_row[row]:nullptr,original);
}
static void bootstrap_seh() {
    __try {SAN14ModpackCollabBootstrap();} __except(EXCEPTION_EXECUTE_HANDLER) {note_fault(0x18,GetExceptionCode());}
}
// collab unlock sweep (3.8): once, before the first judgement, so unlocked traits pass 32F500
static void first_tick() {
    static volatile LONG once;
    if(InterlockedCompareExchange(&once,1,0))return;
    if(mod_features.tooltip_collab && !modpack_yield_collab) {
        bootstrap_seh();
        log_event("trait_hover_collab_bootstrap",1,0,0);
    }
}
// judgement + display for one owner on one path
static void judge(unsigned path,Obj self) {
    if(!self || runtime_off || path>=6 || path_off[path])return;
    note_tick(path,vt_rva(self));
    const thp::Record *rec=nullptr;
    if(path==thp::PATH_DUEL) {
        rec=records.find(reinterpret_cast<uintptr_t>(self));
        if(!rec)return;                                // nothing captured for this duel layout
    }
    Resolve r={};unsigned code=0;
    if(!resolve_seh(path,self,rec,r,code)) {path_off[path]=true;note_fault(path,code);return;}
    if(r.key)note_route(path,r.route,r.key);
    if(!show_seh(self,r.key,path,r.route,code)) {runtime_off=true;note_fault(0x10|path,code);}
}
static uint64_t __fastcall th_tick(Obj self,uint64_t a,uint64_t b,uint64_t c) { COST_SCOPE("trait_hover_tick");
    const uint64_t result=pick_next(self,thp::SK_TICK,GA(GA_92AD0))(self,a,b,c);
    first_tick();
    if(self)judge(thp::tick_path(path_vts,vt_rva(self)),self);
    return result;
}
static uint64_t __fastcall th_body_tick(Obj self,uint64_t a,uint64_t b,uint64_t c) { COST_SCOPE("trait_hover_body_tick");
    const uint64_t result=pick_next(self,thp::SK_TICK,GA(GA_2DC340))(self,a,b,c);
    first_tick();
    if(self && !runtime_off) {
        Obj owner=pointer(self,8);
        if(owner && thp::body_owner_ok(path_vts,vt_rva(owner)))judge(thp::PATH_BODY,owner);
    }
    return result;
}
static unsigned hide_original(unsigned k) {return k==0?GA(GA_7C9080):k==1?GA(GA_9143C0):GA(GA_869820);}
static unsigned close_original(unsigned k) {return k==0?GA(GA_8FBD40):k==1?GA(GA_924B20):GA(GA_8782E0);}
template<unsigned K> static uint64_t __fastcall th_hide(Obj self,uint64_t a,uint64_t b,uint64_t c) { COST_SCOPE("trait_hover_hide");
    ThSlot next=pick_next(self,thp::SK_HIDE,hide_original(K));
    release(self,false,0);
    return next(self,a,b,c);
}
template<unsigned K> static uint64_t __fastcall th_close(Obj self,uint64_t a,uint64_t b,uint64_t c) { COST_SCOPE("trait_hover_close");
    ThSlot next=pick_next(self,thp::SK_CLOSE,close_original(K));
    release(self,true,a);
    return next(self,a,b,c);
}
// duel label capture (FUN_2AA0): after the original label text call, remember the label widget and the trait id of `text`.
// The pointer is the original argument, so another mod rewriting the shown text (advisor_briefing relay) does not matter (3.7).
static void capture_impl(Obj root,unsigned id,const wchar_t *text,unsigned site) {
    Obj owner=root-0x130;
    const unsigned vt=vt_rva(owner);
    const int idx=thp::duel_label_index(vt==path_vts.duel_info,vt==path_vts.duel_gauge,static_cast<int>(id));
    if(idx<0 || idx>=thp::kDuelLabels)return;
    thp::Record *rec=records.acquire(reinterpret_cast<uintptr_t>(owner));
    if(!rec) {if(records.take_full_log())log_event("trait_hover_records_full",thp::kRecordCap,0,0);return;}
    rec->labels[idx]=reinterpret_cast<uintptr_t>(fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_92CEE0))(root,id));
    Obj world_root=world();
    int found=0;bool ok=false;
    if(world_root && text) {
        found=thp::find_name_id(reinterpret_cast<uintptr_t>(text),1,thp::kDuelTraitMax,[&](int n)->uintptr_t {
            Obj t=trait_object(world_root,n);return t?reinterpret_cast<uintptr_t>(t+0x10):0;});
        ok=found && valid(trait_object(world_root,found));
    }
    rec->ids[idx]=thp::duel_capture_id(found,ok);
    rec->last_key=0;
    if(capture_count<8) {++capture_count;log_event("trait_hover_duel_capture",vt==path_vts.duel_info?1u:2u,static_cast<unsigned>(idx),static_cast<unsigned>(rec->ids[idx]));}
    (void)site;
}
static void capture(Obj root,unsigned id,const wchar_t *text,unsigned site) {
    if(runtime_off || path_off[thp::PATH_DUEL] || !root)return;
    __try {capture_impl(root,id,text,site);}
    __except(EXCEPTION_EXECUTE_HANDLER) {path_off[thp::PATH_DUEL]=true;note_fault(0x1A,GetExceptionCode());}
}
template<unsigned I> static uint64_t __fastcall th_duel_text(Obj root,unsigned id,const wchar_t *text,uint64_t flags) { COST_SCOPE("trait_hover_duel_text");
    const uint64_t result=next_or<DuelFn>(next_duel[I],GA(GA_930D20))(root,id,text,flags);
    capture(root,id,text,I);
    return result;
}

// ------------------------------------------------------------------------------------------------------------------------
// guards (3.5) and installation (3.2)
// ------------------------------------------------------------------------------------------------------------------------
static bool owner_row(const LayoutRow &r) {return r.owner && !strcmp(r.owner,"trait_hover");}
static bool guard_fail(unsigned stage,unsigned row,unsigned rva) {
    static bool logged;
    if(!logged) {logged=true;log_event("trait_hover_guard_rejected",stage,row,rva);}
    return false;
}
static bool guards_ok() {
    if(!ga::files_ok({GF_native_trait_hover_inl,GF_trait_hover_profile_h})) {
        log_event("addr_unresolved",74,ga::state().unresolved,ga::state().build<0?0u:static_cast<unsigned>(ga::state().build)+1u);
        return guard_fail(6,0,0);
    }
    const size_t count=sizeof(trait_hover_guards)/sizeof(trait_hover_guards[0]);
    if(!thp::guards_table_ok(trait_hover_guards,count))return guard_fail(5,0,0);
    const unsigned ids[]={trait_hover_empty_tick_rva,trait_hover_slot_getter_rva,trait_hover_duel_call_rvas[0],trait_hover_duel_call_rvas[1],
        trait_hover_duel_call_rvas[2],trait_hover_duel_call_rvas[3],trait_hover_duel_target_rva,trait_hover_balloon_vtable_rva,
        GA(GA_2DC340),GA(GA_7C9080),GA(GA_9143C0),GA(GA_869820),GA(GA_8FBD40),GA(GA_924B20),GA(GA_8782E0),GA(GA_13A4298),GA(GA_13A42C8),
        GA(GA_277DF18),GA(GA_232D00),GA(GA_1A47B98),GA(GA_14B8CE8),GA(GA_145FFE0),GA(GA_14604C0),GA(GA_13EC0D8),GA(GA_139C540),
        trait_hover_tactic_columns_rva,thp::tactic_func_rva(0),thp::tactic_func_rva(1),thp::tactic_func_rva(2),thp::tactic_func_rva(3),thp::tactic_func_rva(4)};
    for(size_t i=0;i<sizeof(ids)/sizeof(ids[0]);++i)if(!ids[i])return guard_fail(5,static_cast<unsigned>(i)+1000,0);
    // registry rows: 152 vtable slots + 4 duel call sites, none unresolved
    unsigned vt=0,call=0;
    for(unsigned i=0;i<layout_row_count;++i) {
        const LayoutRow &r=layout_rows[i];
        if(!owner_row(r))continue;
        if(!r.rva || !r.original)return guard_fail(5,vt+call,r.rva);
        if(r.kind==LR_VT)++vt;else if(r.kind==LR_CALL)++call;
    }
    if(vt!=kVtRows || call!=kCallRows)return guard_fail(5,vt,call);
    for(size_t i=0;i<count;++i) {
        ga::ObserverSet used;
        if(!ga::hash_ok(image,trait_hover_guards[i].rva,trait_hover_guards[i].size,trait_hover_guards[i].hash,&used))
            return guard_fail(1,static_cast<unsigned>(i),trait_hover_guards[i].rva);
        for(unsigned k=0;k<used.count;++k)log_event("observer_hook_tolerated",34,used.rva[k]);
    }
    for(unsigned k=0;k<thp::kTacticFields;++k) {   // tactic list feature-tab column descriptors (docs/TRAIT_HOVER_TACTIC_TEXT.md 3.3)
        const thp::TacticField &f=thp::kTacticFieldTable[k];
        if(!thp::tactic_column_ok(image+trait_hover_tactic_columns_rva+static_cast<size_t>(f.column)*thp::kTacticColStride,f.column,
               reinterpret_cast<uintptr_t>(image)+thp::tactic_func_rva(k),f.caption_id))
            return guard_fail(7,k,trait_hover_tactic_columns_rva);
    }
    const unsigned char ret0[]={0xC2,0x00,0x00};
    if(ga::mcmp_at(image,trait_hover_empty_tick_rva,ret0,sizeof(ret0)))return guard_fail(2,0,trait_hover_empty_tick_rva);
    const unsigned g=trait_hover_slot_getter_rva;
    if(image[g]!=0x83 || image[g+1]!=0xFB || (image[g+2]!=0x05 && image[g+2]!=0x09))return guard_fail(2,1,g);
    // duel label call sites: E8 to 930D20 behind the exact argument setup (1.7 form B for the Info layout, form A for the Gauge)
    static const unsigned char form_b[14]={0x4C,0x8B,0xC0,0x41,0xB9,0x01,0x00,0x00,0x00,0x8B,0xD5,0x48,0x8B,0xCB};
    static const unsigned char form_a[14]={0x4C,0x8B,0xC0,0x45,0x8B,0xCE,0x42,0x8B,0x54,0xAD,0xFF,0x49,0x8B,0xCC};
    for(unsigned i=0;i<kCallRows;++i) {
        const unsigned site=trait_hover_duel_call_rvas[i];
        int rel=0;memcpy(&rel,image+site+1,4);
        const long long dest=static_cast<long long>(site)+5+rel;
        if(image[site]!=0xE8 || dest!=static_cast<long long>(trait_hover_duel_target_rva) ||
           ga::mcmp_at(image,site-14,i<2?form_b:form_a,14))return guard_fail(3,i,site);
    }
    // optional balloon construction context (1A176A mov ecx,180h ... lea rax,[CSimpleBalloonLayout vtable])
    const unsigned b=trait_hover_balloon_ctx_rva;
    const unsigned char size180[]={0xB9,0x80,0x01,0x00,0x00};
    int lea=0;memcpy(&lea,image+b+0x37+3,4);
    if(ga::mcmp_at(image,b,size180,sizeof(size180)) || image[b+0x37]!=0x48 || image[b+0x38]!=0x8D || image[b+0x39]!=0x05 ||
       static_cast<long long>(b)+0x37+7+lea!=static_cast<long long>(trait_hover_balloon_vtable_rva))return guard_fail(4,0,b);
    return true;
}
// which replacement function serves a registry row
static void *replacement_for(unsigned original) {
    if(original==GA(GA_92AD0))return reinterpret_cast<void *>(&th_tick);
    if(original==GA(GA_2DC340))return reinterpret_cast<void *>(&th_body_tick);
    if(original==GA(GA_7C9080))return reinterpret_cast<void *>(&th_hide<0>);
    if(original==GA(GA_9143C0))return reinterpret_cast<void *>(&th_hide<1>);
    if(original==GA(GA_869820))return reinterpret_cast<void *>(&th_hide<2>);
    if(original==GA(GA_8FBD40))return reinterpret_cast<void *>(&th_close<0>);
    if(original==GA(GA_924B20))return reinterpret_cast<void *>(&th_close<1>);
    if(original==GA(GA_8782E0))return reinterpret_cast<void *>(&th_close<2>);
    return nullptr;
}
static void *duel_replacement(unsigned site) {
    if(site==trait_hover_duel_call_rvas[0])return reinterpret_cast<void *>(&th_duel_text<0>);
    if(site==trait_hover_duel_call_rvas[1])return reinterpret_cast<void *>(&th_duel_text<1>);
    if(site==trait_hover_duel_call_rvas[2])return reinterpret_cast<void *>(&th_duel_text<2>);
    if(site==trait_hover_duel_call_rvas[3])return reinterpret_cast<void *>(&th_duel_text<3>);
    return nullptr;
}
// Registers every "trait_hover" row; Layout publishes them together and takes all back when one fails (ALL_OR_NONE).
static bool install() {
    if(installed)return false;
    const unsigned summary=GA(GA_14B8CE8)-8,info=GA(GA_145FFE0)-8,gauge=GA(GA_14604C0)-8,edit=GA(GA_13EC0D8)-8;
    // tables first, so every hook that becomes reachable finds them ready
    table_count=body_count=0;
    unsigned k=0,vt=0,call=0;
    for(unsigned i=0;i<layout_row_count;++i) {
        const LayoutRow &r=layout_rows[i];
        if(!owner_row(r))continue;
        if(r.kind==LR_VT) {
            if(vt>=kVtRows)return false;
            slots[vt]={r.rva,k};++vt;
            const unsigned vtbl=r.rva-0x08;
            if(r.original==GA(GA_92AD0) && vtbl!=summary && vtbl!=info && vtbl!=gauge && vtbl!=edit) {
                if(table_count>=kTableMax)return false;
                table_vts[table_count++]=vtbl;
            }
            if(r.original==GA(GA_2DC340)) {
                if(body_count>=kTableMax)return false;
                body_vts[body_count++]=r.rva-0x08;
            }
        } else if(r.kind==LR_CALL)++call;
        ++k;
    }
    thp::slot_table_sort(slots,kVtRows);
    if(vt!=kVtRows || call!=kCallRows || !thp::slot_table_valid(slots,kVtRows)) {log_event("trait_hover_register_rejected",k,0,1);return false;}
    path_vts={summary,info,gauge,edit,table_vts,table_count};
    records.reset();
    layout_client::Transaction tx;
    k=0;
    for(unsigned i=0;i<layout_row_count;++i) {
        const LayoutRow &r=layout_rows[i];
        if(!owner_row(r))continue;
        void *rep=r.kind==LR_VT?replacement_for(r.original):duel_replacement(r.rva);
        if(!rep) {log_event("trait_hover_register_rejected",k,0,2);return false;}
        void **next=r.kind==LR_VT?&next_row[k]:&next_duel[k-kVtRows];
        const int handle=r.kind==LR_VT?layout_client::vt(r.rva,rep,next):layout_client::call(r.rva,rep,next);
        if(handle<=0) {log_event("trait_hover_register_rejected",k,handle<0?1u:0u,static_cast<unsigned>(handle<0?-handle:0));return false;}
        ++k;
    }
    tx.done=true;installed=true;
    log_event("trait_hover_installed",vt,call,trait_hover_yield?1u:0u);
    return true;
}
}   // namespace trait_hover
static bool trait_hover_guards_ok() {return trait_hover::guards_ok();}
static bool trait_hover_install() {return trait_hover::install();}
