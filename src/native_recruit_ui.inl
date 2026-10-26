/* Recruitment screen candidate. Invoked only by native UI callbacks.
 * No process injection, polling dispatcher, synthetic click, or background
 * native call. The automatic action starts after a user-confirmed selection.
 */
#include <wchar.h>
// Only the native player menu's alert calculation uses these wrappers.
// The hidden-person scan is also used by exploration; scope its filter to
// 2DEE70 -> 2B2F10 so actual search lists and discoveries stay native.
static __declspec(thread) Obj recruit_alert_force;
static Obj recruit_alert_player() {
    Obj root=world(),context=root?pointer(root,0x871E8):nullptr;
    return context?fn<Obj(__fastcall *)(Obj)>(0x32E910)(context):nullptr;
}
static bool recruit_alert_hated(Obj target,Obj force) {
    if(!valid(target) || !valid(force))return false;
    Obj ruler=table(0x158,word(force,0x10),6000);
    return valid(ruler) && fn<int(__fastcall *)(Obj,Obj)>(0x232A00)(target,ruler)!=0;
}
static int __fastcall recruit_alert_candidate(Obj target,Obj district) { COST_SCOPE("recruit_ui_recruit_alert_candidate");
    int result=fn<int(__fastcall *)(Obj,Obj)>(0x1EF680)(target,district);
    if(!enabled || !result || !valid(district))return result;
    Obj force=table(0xDF30,district[0x10],51);
    return force==recruit_alert_player() && recruit_alert_hated(target,force)?0:result;
}
static void __fastcall recruit_alert_location(Obj target,int *kind,int *id) { COST_SCOPE("recruit_ui_recruit_alert_location");
    fn<void(__fastcall *)(Obj,int *,int *)>(0x231C60)(target,kind,id);
    if(enabled && recruit_alert_force && valid(target) && target[0xE6]==8 &&
       recruit_alert_hated(target,recruit_alert_force)) {
        *kind=0xFF;*id=0; // Native invalid location: skip this alert candidate.
    }
}
static Obj __fastcall recruit_alert_search(GameList *areas,int *people,int *items,int *scenery,int *selected) { COST_SCOPE("recruit_ui_recruit_alert_search");
    Obj previous=recruit_alert_force;
    recruit_alert_force=enabled?recruit_alert_player():nullptr;
    Obj result=fn<Obj(__fastcall *)(GameList *,int *,int *,int *,int *)>(0x2B2F10)
        (areas,people,items,scenery,selected);
    recruit_alert_force=previous;return result;
}
static __declspec(thread) Obj recruit_district;
static __declspec(thread) bool recruit_target_pick;
static __declspec(thread) Obj recruit_pick_layout;
struct RecruitTargetScope {
    bool previous_pick;Obj previous_layout;
    RecruitTargetScope(Obj layout,bool pick):previous_pick(recruit_target_pick),previous_layout(recruit_pick_layout) {
        recruit_target_pick=pick;recruit_pick_layout=pick?layout:nullptr;
    }
    ~RecruitTargetScope() {recruit_target_pick=previous_pick;recruit_pick_layout=previous_layout;}
};
static unsigned recruit_district_id(Obj district) {
    if(world() && valid(district))for(unsigned i=1;i<=51;++i)
        if(table(0xE0D0,i,51)==district)return i;
    return 0;
}
static SRWLOCK recruit_context_lock=SRWLOCK_INIT;
static Obj recruit_bound_world,recruit_bound_district;
static unsigned recruit_bound_district_id;
static void recruit_bind_context(Obj district) {
    Obj root=world();unsigned id=recruit_district_id(district);
    AcquireSRWLockExclusive(&recruit_context_lock);
    recruit_bound_world=id?root:nullptr;recruit_bound_district=id?district:nullptr;recruit_bound_district_id=id;
    ReleaseSRWLockExclusive(&recruit_context_lock);
}
static Obj recruit_display_district() {
    if(valid(recruit_district))return recruit_district;
    AcquireSRWLockShared(&recruit_context_lock);
    Obj root=recruit_bound_world,district=recruit_bound_district;unsigned id=recruit_bound_district_id;
    ReleaseSRWLockShared(&recruit_context_lock);
    // Drawing/sorting may occur after the initiating UI update has returned.
    // Keep the selector's context, but reject a new world or replaced district.
    return root && root==world() && id && table(0xE0D0,id,51)==district && valid(district)?district:nullptr;
}
struct RecruitScope {
    Obj previous;
    explicit RecruitScope(Obj district):previous(recruit_district) {recruit_district=district;}
    ~RecruitScope() {recruit_district=previous;}
};
static const wchar_t *recruit_reason_label(unsigned reason,bool family_available=false) {
    if(reason & personnel::MissingContext)return L"";
    if(reason & personnel::ServiceBan)return L"불가: 현 세력 사관금지";
    if(reason & personnel::HighLoyalty)return (reason & personnel::LikesCurrentRuler)?
        L"불가: 상대군주 친애(100↑)":L"불가: 충성도 100↑";
    if((reason & personnel::FamilyEnvoyException) || family_available)return (reason & personnel::LikesCurrentRuler)?
        L"주의: 상대군주 친애":L"가족 권고 시 가능성 있음";
    static const wchar_t *labels[]={
        L"가능성 있음",L"불가: 군주 혐오",
        L"불가: 타세력 배우자",L"불가: 혐오·배우자",
        L"불가: 타세력 의형제",L"불가: 혐오·의형제",
        L"불가: 타세력 가족",L"불가: 혐오·가족"
    };
    // Keep genuine remaining blockers visible; friendship alone is caution.
    if(reason&7)return labels[reason&7];
    return (reason & personnel::LikesCurrentRuler)?L"주의: 상대군주 친애":labels[0];
}
static bool recruit_family_available(Obj target,Obj district) {
    Obj own_force=valid(district)?table(0xDF30,district[0x10],51):nullptr;
    if(!valid(target) || !valid(own_force))return false;
    GameList candidates;list_init(candidates,0x13436B0);
    fn<unsigned(__fastcall *)(Obj,GameList *)>(0x6DA8A0)(district,&candidates);
    bool found=false;unsigned count=0;
    for(Link *link=list_first(candidates);link && count<6000;link=link->next,++count) {
        Obj actor=link->value;
        if(!valid(actor) || personnel_force(actor)!=own_force || actor[0x161] || (word(actor,0x15E)&1) ||
           !personnel_family_envoy(target,actor))continue;
        PersonnelEmployCommand command={target,actor,0,0};
        if(fn<int(__fastcall *)(PersonnelEmployCommand *)>(0x1EFE60)(&command)) {found=true;break;}
    }
    list_free(candidates);return found;
}
static const wchar_t *__fastcall recruit_reason_text(Obj target) { COST_SCOPE("recruit_ui_recruit_reason_text");
    Obj district=enabled?recruit_display_district():nullptr;
    if(!district)return recruit_reason_label(personnel::MissingContext);
    unsigned reason=personnel_recruitment_reason(target,district,nullptr);
    bool family=!personnel::recruitment_hard_blocked(reason) &&
                personnel::recruitment_blocked(reason) && recruit_family_available(target,district);
    return recruit_reason_label(reason,family);
}
static int __fastcall recruit_reason_compare(Obj left,Obj right) { COST_SCOPE("recruit_ui_recruit_reason_compare");
    return wcscmp(recruit_reason_text(left),recruit_reason_text(right));
}
struct RecruitTab {const int *ids;const wchar_t *name;int message_id;int padding;};
static_assert(sizeof(RecruitTab)==24,"native tab metadata ABI");
static unsigned char recruit_columns[214][0x60];
static int recruit_tab_ids[3][65];
static RecruitTab recruit_tabs[4];
static bool recruit_columns_ready;
static bool recruit_prepare_columns() {
    if(recruit_columns_ready)return true;
    Obj original=image+0x1A2B850;
    // The captured table has 213 entries. Preserve aliases at IDs 13..17.
    if(dword(original,0)!=0 || dword(original,0x60)!=1 || dword(original,212*0x60)!=212)return false;
    const auto *tabs=reinterpret_cast<const RecruitTab *>(image+0x1A31330);
    if(tabs[3].ids)return false;
    for(unsigned tab=0;tab<3;++tab) {
        if(!tabs[tab].ids)return false;
        unsigned out=0;
        // Append after the final native column (dispatching officer on tab 0).
        for(unsigned i=0;i<63;++i) {
            int id=tabs[tab].ids[i];
            if(id==-1) {recruit_tab_ids[tab][out++]=213;recruit_tab_ids[tab][out]=-1;break;}
            if(id<0 || id>=213)return false;
            recruit_tab_ids[tab][out++]=id;
            if(i==62)return false;
        }
        recruit_tabs[tab]=tabs[tab];recruit_tabs[tab].ids=recruit_tab_ids[tab];
    }
    memcpy(recruit_columns,original,213*0x60);
    Obj column=recruit_columns[213];memcpy(column,recruit_columns[3],0x60);
    *reinterpret_cast<unsigned *>(column)=213;
    *reinterpret_cast<const wchar_t **>(column+8)=L"등용가능성";
    *reinterpret_cast<unsigned *>(column+0x10)=240;
    *reinterpret_cast<void **>(column+0x18)=reinterpret_cast<void *>(&recruit_reason_text);
    *reinterpret_cast<void **>(column+0x28)=nullptr;
    *reinterpret_cast<void **>(column+0x30)=reinterpret_cast<void *>(&recruit_reason_compare);
    *reinterpret_cast<void **>(column+0x38)=nullptr;
    *reinterpret_cast<void **>(column+0x40)=nullptr;
    *reinterpret_cast<int *>(column+0x48)=-1; // 2D6Fxx uses the literal at +8.
    recruit_columns_ready=true;return true;
}
struct RecruitMultiSnapshot {
    Obj root,district,layout;
    unsigned district_id,count;
    Obj people[256];
};
// Process-wide, not TLS (2026-10-25 live read): the selector closes on the
// dialog thread while 733BD0 waits in 571EF0, and the recruitment state
// update later reads the snapshot on its own thread.
static RecruitMultiSnapshot recruit_multi;
static volatile unsigned recruit_multi_revision;
static bool recruit_person(Obj person) {
    return valid(person) && word(person,0x10)<=6000 && table(0x158,word(person,0x10),6000)==person;
}
static bool recruit_multi_matches(Obj layout,Obj district) {
    return recruit_multi.count>=2 && recruit_multi.root==world() && recruit_multi.layout==layout &&
        recruit_multi.district==district && recruit_multi.district_id &&
        table(0xE0D0,recruit_multi.district_id,51)==district &&
        pointer(layout,0x188)==recruit_multi.people[0];
}
// 2E2FB0/2E22B0 and 8F19C0 fill the output before closing via vt+190.
// The ordinary decision event in 8F19C0 bypasses the config functor.
static Obj recruit_pick_dialog,recruit_pick_dialog_layout,recruit_pick_dialog_district,recruit_pick_dialog_world;
static unsigned recruit_pick_dialog_district_id;
static void __fastcall recruit_selector_close(Obj dialog,unsigned code) { COST_SCOPE("recruit_ui_recruit_selector_close");
    if(dialog==recruit_pick_dialog) {
        recruit_pick_dialog=nullptr;
        if(code==0x7FFFFFFD || code==0x7FFFFFFE) {
            auto out=reinterpret_cast<GameList *>(pointer(pointer(dialog,0x1A8),0x80));
            RecruitMultiSnapshot next={};
            Link *first=nullptr,*last=nullptr;unsigned scanned=0;
            if(out)for(Link *link=list_first(*out);link && scanned<6000 && next.count<256;
                       link=link->next,++scanned) {
                Obj person=link->value;if(!recruit_person(person))continue;
                bool duplicate=false;
                for(unsigned i=0;i<next.count;++i)if(next.people[i]==person) {duplicate=true;break;}
                if(duplicate)continue;
                if(!first)first=link;last=link;next.people[next.count++]=person;
            }
            if(next.count>=2) {
                Obj person=first->value;first->value=last->value;last->value=person;
                person=next.people[0];next.people[0]=next.people[next.count-1];next.people[next.count-1]=person;
                next.root=recruit_pick_dialog_world;next.district=recruit_pick_dialog_district;next.layout=recruit_pick_dialog_layout;
                next.district_id=recruit_pick_dialog_district_id;
            } else if(next.count==1)next={};
            // Empty output is cancellation: retain the previous confirmed selection.
            if(first) {recruit_multi=next;++recruit_multi_revision;}
        }
    }
    fn<void(__fastcall *)(Obj,unsigned)>(0x6BE8E0)(dialog,code);
}
static uint64_t __fastcall recruit_selector_init(Obj dialog,Obj config) { COST_SCOPE("recruit_ui_recruit_selector_init");
    auto original=fn<uint64_t(__fastcall *)(Obj,Obj)>(0x2C0820);
    if(!enabled || !config || !valid(recruit_district) || pointer(config,8)!=image+0x1A31330 ||
       pointer(config,16)!=image+0x1A2B850 || !recruit_prepare_columns()) {
        recruit_bind_context(nullptr);return original(dialog,config);
    }
    recruit_bind_context(recruit_district);
    // 2C0820 copies the configuration. Restore caller settings after the copy.
    Obj old_tabs=pointer(config,8),old_columns=pointer(config,16);
    unsigned old_mode=dword(config,0x18),old_limit=dword(config,0x1C);
    bool multi=false;
    if(recruit_target_pick && old_mode==1 && old_limit==1) {
        int cost=fn<int(__fastcall *)()>(0x6D3660)();unsigned orders=recruit_district[0x14];
        unsigned limit=cost>0?orders/static_cast<unsigned>(cost):orders;
        if(!limit)limit=1;
        // Record even a one-order pick: its confirmation clears old snapshots.
        recruit_pick_dialog=dialog;recruit_pick_dialog_layout=recruit_pick_layout;
        recruit_pick_dialog_district=recruit_district;recruit_pick_dialog_world=world();
        recruit_pick_dialog_district_id=recruit_district_id(recruit_district);
        if(limit>1) {
            multi=true;
            *reinterpret_cast<unsigned *>(config+0x18)=2;*reinterpret_cast<unsigned *>(config+0x1C)=limit;
        }
    }
    *reinterpret_cast<Obj *>(config+8)=reinterpret_cast<Obj>(recruit_tabs);
    *reinterpret_cast<Obj *>(config+16)=reinterpret_cast<Obj>(recruit_columns);
    uint64_t result=original(dialog,config);
    *reinterpret_cast<Obj *>(config+8)=old_tabs;*reinterpret_cast<Obj *>(config+16)=old_columns;
    if(multi) {
        *reinterpret_cast<unsigned *>(config+0x18)=old_mode;*reinterpret_cast<unsigned *>(config+0x1C)=old_limit;
    }
    return result;
}
// Keep the published hook, but leave the original layout entirely native.
static unsigned __fastcall recruit_layout_init(Obj layout,Obj district) { COST_SCOPE("recruit_ui_recruit_layout_init");
    return fn<unsigned(__fastcall *)(Obj,Obj)>(0x6F1D10)(layout,district);
}
static void recruit_report_automatic(Obj district,const PersonnelBatchResult &result) {
    unsigned skipped=result.blocked+result.no_actor+result.no_orders+result.failed+result.advised_against;
    wchar_t text[256]={},advice_text[64]={};
    if(result.advised_against)
        _snwprintf_s(advice_text,sizeof(advice_text)/sizeof(advice_text[0]),_TRUNCATE,
            L"\n군사가 반대한 대상 %u명은 보내지 않았습니다.",result.advised_against);
    if(result.sent) {
        _snwprintf_s(text,sizeof(text)/sizeof(text[0]),_TRUNCATE,
            L"군사가 등용을 권하는 실행무장만 배정했습니다.\n자동등용 명령 %u건을 내렸습니다.%s%s",
            result.sent,skipped>result.advised_against?L"\n등용 불가·실행무장 또는 명령서 부족 등의 대상은 제외했습니다.":L"",advice_text);
    } else {
        _snwprintf_s(text,sizeof(text)/sizeof(text[0]),_TRUNCATE,
            L"자동등용 명령을 내리지 못했습니다.\n등용가능성, 군사 반대 여부, 실행무장과 명령서를 확인해 주세요.%s",advice_text);
    }
    Obj force=valid(district)?table(0xDF30,district[0x10],51):nullptr;
    if(!valid(force))return;
    unsigned speaker=word(force,0x52);Obj actor=table(0x158,speaker,6000);
    if(!valid(actor) || personnel_force(actor)!=force) {
        speaker=word(force,0x10);actor=table(0x158,speaker,6000);
    }
    if(!valid(actor))return;
    // Same native UTF-16 officer notification used by 6CD040 after manual
    // recruitment. 1F09E0 copies text into its ring before queuing the dialog.
    native_ui::officer_notice(speaker,text);
    log_event("auto_employ_result_notice",result.sent,skipped,result.advised_against);
}
static unsigned recruit_actor_count(Obj layout) {
    auto &actors=*reinterpret_cast<GameList *>(layout+0x190);
    unsigned count=0;
    for(Link *link=list_first(actors);link && count<6000;link=link->next)++count;
    return count;
}
static void recruit_multi_preview(Obj layout,Obj district) {
    int cost=fn<int(__fastcall *)()>(0x6D3660)();
    int total=cost*static_cast<int>(recruit_actor_count(layout)+recruit_multi.count-1);
    Obj source=pointer(layout,0x1A0);
    unsigned orders=(source==district?source:district)[0x14];
    wchar_t remaining[32]={},deduction[48]={};
    _snwprintf_s(remaining,32,_TRUNCATE,L"%d",static_cast<int>(orders)-total);
    _snwprintf_s(deduction,48,_TRUNCATE,L"(^03-%d^00)",total);
    auto text=fn<void(__fastcall *)(Obj,unsigned,const wchar_t *,int)>(0x930D20);
    text(layout+0x130,0xAB,remaining,1);text(layout+0x130,0xAC,deduction,1);
    Obj widget=pointer(layout,0x1A8);
    if(widget) {
        unsigned node=fn<Obj(__fastcall *)(Obj,unsigned)>(0x92CEE0)(widget+0x130,2)?2:7;
        const wchar_t *name=fn<const wchar_t *(__fastcall *)(Obj)>(0x22EEC0)(recruit_multi.people[0]);
        wchar_t label[64]={};
        _snwprintf_s(label,64,_TRUNCATE,L"%s 외 %u명",name,recruit_multi.count-1);
        text(widget+0x130,node,label,1);
    }
}
static void __fastcall recruit_state_update(Obj state) { COST_SCOPE("recruit_ui_recruit_state_update");
    Obj layout=pointer(state,0x478),district=pointer(state,0x470);
    auto original=fn<void(__fastcall *)(Obj)>(0x733BD0);
    if(!enabled || !layout || pointer(layout,0)!=image+0x1440E48 || !valid(district)) {
        original(state);return;
    }
    RecruitScope scope(district);
    unsigned operation=dword(layout,0x180),revision=recruit_multi_revision;
    bool dispatch=(operation==3 || operation==4) && recruit_multi_matches(layout,district);
    Obj actors[256];unsigned actor_count=0,scanned=0;
    // User decision 2026-10-25: send extras and show their modal notice while
    // the screen is open, before 733BD0 can dispatch and close it (case 3).
    // Reserve the actors and orders that native 6CD040 will use afterwards.
    RecruitMultiSnapshot batch={};
    if(dispatch) {
        batch=recruit_multi;
        auto &list=*reinterpret_cast<GameList *>(layout+0x190);
        for(Link *link=list_first(list);link && scanned<6000 && actor_count<256;link=link->next,++scanned)
            if(recruit_person(link->value))actors[actor_count++]=link->value;
        Obj target=pointer(layout,0x188),force=table(0xDF30,district[0x10],51);
        bool recommended=false;
        if(valid(force))for(unsigned i=0;i<actor_count;++i)
            if(personnel_recommended(force,target,actors[i])) {recommended=true;break;}
        // User decision 2026-10-25: an unadvised display target is skipped
        // only for a matching multi-selection. Release its batch reservations.
        unsigned reserved_count=recommended?actor_count:0;
        int cost=fn<int(__fastcall *)()>(0x6D3660)();
        unsigned reserved_orders=cost>0?static_cast<unsigned>(cost)*reserved_count:0;
        GameList remaining;list_init(remaining,0x13436B0);
        for(unsigned i=1;i<batch.count;++i)list_add(remaining,batch.people[i]);
        PersonnelBatchResult result=personnel_auto_employ(district,remaining,actors,reserved_count,reserved_orders,district);
        list_free(remaining);
        if(!recommended)++result.advised_against;
        if(result.sent || result.blocked || result.no_actor || result.no_orders || result.failed ||
           result.duplicates || result.advised_against)recruit_report_automatic(district,result);
        recruit_multi={};
        log_event("recruit_multi_batch",batch.count,result.sent,result.advised_against);
        if(!recommended) {
            log_event("recruit_multi_display_advised_against",word(target,0x10),actor_count);
            // Mirror 733BD0's successful cases 3/4 on this UI caller thread.
            fn<void(__fastcall *)(Obj)>(0x6BD740)(layout);
            if(operation==3) {
                auto close=*reinterpret_cast<void(__fastcall **)(Obj,unsigned)>(pointer(layout,0)+0x190);
                close(layout,0x7FFFFFFD);
            }
            return; // Case 3 may have destroyed layout; case 4 keeps it reset.
        }
    }
    {
        RecruitTargetScope target_scope(layout,operation==2);
        original(state);
    }
    if(operation==3)return; // Never read a potentially closed layout.
    if((operation!=0 || recruit_multi_revision!=revision) && recruit_multi_matches(layout,district))
        recruit_multi_preview(layout,district);
}
