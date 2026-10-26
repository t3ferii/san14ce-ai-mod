/* Private synthetic native UI objects. Does not attach to the game and
 * cannot establish real rendering/ABI correctness. */
#define RECRUITMENT_UI_CANDIDATE
#define HIDDEN_LOYALTY
#define main personnel_fixture_main
#include "personnel_native_fixture.cpp"
#undef main
static void ui_expect(bool ok,unsigned line) {
    ++checks;if(!ok) {fprintf(stderr,"Recruitment UI fixture failed at line %u (check %u)\n",line,checks);exit(1);}
}
#define expect(value) ui_expect((value),__LINE__)
// User decision 2026-10-25: replace the removed automatic button tests
// with native target multi-selection, close-hook and dispatch tests.
// User decision 2026-10-25: the decision event bypasses the confirm functor;
// test the common close slot with already populated output instead.
static Obj ui_district,ui_layout,ui_dialog,ui_target,ui_actor,ui_body,ui_widget;
static unsigned ui_original_calls,ui_setup_calls,ui_layout_calls,ui_collect_calls,ui_close_calls;
static bool ui_accept=true,ui_send=true,ui_poison_layout=false;
static int ui_cost=1;
static Obj ui_closed_dialog;
static unsigned ui_closed_code,ui_label_node=2;
static std::wstring ui_label,ui_name=L"은관";
static std::vector<Obj> ui_pick_list;
static unsigned char ui_config_copy[0x400];
static std::wstring ui_notice;
static unsigned ui_notices,ui_speaker;
static std::map<unsigned,std::wstring> ui_preview;
static unsigned ui_preview_calls;
static unsigned ui_reset_calls,ui_display_close_calls;
static bool ui_track_dispatch,ui_native_started;
static std::vector<unsigned> ui_dispatch_events;
static void ui_issue_event(PersonnelEmployCommand *command) {
    if(ui_track_dispatch)ui_dispatch_events.push_back(word(command->target,0x10));
}
static int __fastcall ui_order_cost() {return ui_cost;}
static unsigned __fastcall ui_layout_native(Obj layout,Obj district) {
    expect(layout==ui_layout && district==ui_district);++ui_layout_calls;return 7;
}
static uint64_t __fastcall ui_setup_native(Obj,Obj config) {
    ++ui_setup_calls;memcpy(ui_config_copy,config,0x400);
    return 1;
}
static void __fastcall ui_collect(Obj body,GameList *out) {
    expect(body==ui_body);++ui_collect_calls;
    for(Obj person:ui_pick_list)list_add(*out,person);
}
static void __fastcall ui_close(Obj dialog,unsigned result) {
    ui_closed_dialog=dialog;ui_closed_code=result;++ui_close_calls;
    if(dialog==ui_dialog)expect(recruit_pick_dialog==nullptr);
}
static Obj __fastcall ui_label_lookup(Obj graph,unsigned id) {
    expect(graph==ui_widget+0x130 && id==2);return ui_label_node==2?ui_widget:nullptr;
}
static const wchar_t *__fastcall ui_person_name(Obj person) {
    expect(person==recruit_multi.people[0]);return ui_name.c_str();
}
static void __fastcall ui_preview_text(Obj graph,unsigned id,const wchar_t *text,int flag) {
    if(graph==ui_widget+0x130) {
        expect(id==ui_label_node && flag==1);ui_label=text;++ui_preview_calls;return;
    }
    expect(graph==ui_layout+0x130 && (id==0xAB || id==0xAC) && flag==1);
    ui_preview[id]=text;++ui_preview_calls;
}
static void __fastcall ui_result_notice(unsigned speaker,const wchar_t *text) {
    if(ui_track_dispatch) {
        expect(!ui_native_started && pointer(ui_layout,0)==image+0x1440E48);
        ui_dispatch_events.push_back(0x1F09E0);
    }
    ++ui_notices;ui_speaker=speaker;ui_notice=text;
}
static void __fastcall ui_reset_display(Obj layout) {
    expect(layout==ui_layout && recruit_multi.count==0 && !ui_native_started);
    ++ui_reset_calls;ui_dispatch_events.push_back(0x6BD740);
    auto &actors=*reinterpret_cast<GameList *>(layout+0x190);
    list_free(actors);list_init(actors,0x13436B0);
    putptr(layout,0x188,nullptr);put32(layout,0x180,0);
}
static void __fastcall ui_close_display(Obj layout,unsigned code) {
    expect(layout==ui_layout && code==0x7FFFFFFD && !pointer(layout,0x188) &&
           dword(layout,0x180)==0 && !list_first(*reinterpret_cast<GameList *>(layout+0x190)));
    expect(!ui_dispatch_events.empty() && ui_dispatch_events.back()==0x6BD740);
    ++ui_display_close_calls;ui_dispatch_events.push_back(0x190);
    // A read after the direct close must fault, just like the native close.
    DWORD old=0;expect(VirtualProtect(layout,4096,PAGE_NOACCESS,&old)!=0);
}
static void __fastcall ui_state_native(Obj state) {
    ++ui_original_calls;expect(recruit_district==ui_district);
    Obj layout=pointer(state,0x478);unsigned operation=dword(layout,0x180);
    if(ui_track_dispatch) {
        ui_native_started=true;ui_dispatch_events.push_back(0x733BD0);
        expect(recruit_multi.count==0);
    }
    expect(recruit_target_pick==(operation==2));
    expect(recruit_pick_layout==(operation==2?layout:nullptr));
    if(operation==2) {
        GameList out;list_init(out,0x13436B0);
        unsigned char config[0x400]={};
        putptr(config,8,image+0x1A31330);putptr(config,16,image+0x1A2B850);
        put32(config,0x18,1);put32(config,0x1C,1);putptr(config,0x80,&out);
        recruit_selector_init(ui_dialog,config);
        putptr(ui_dialog,0x1A8,ui_config_copy);
        if(ui_accept)ui_collect(ui_body,&out);
        recruit_selector_close(ui_dialog,0x7FFFFFFE); // 8F19C0 ordinary decision path.
        Link *first=list_first(out);if(first)putptr(layout,0x188,first->value);
        list_free(out); // Native 733BD0 releases the selection output.
    }
    if((operation==3 || operation==4) && ui_send) {
        auto &actors=*reinterpret_cast<GameList *>(layout+0x190);
        // The wrapper must already have sent extras and shown their notice.
        for(Link *link=list_first(actors);link;link=link->next) {
            PersonnelEmployCommand command={pointer(layout,0x188),link->value,0,0};
            if(mock_command_valid(&command) && ui_cost>=0 && ui_district[0x14]>=ui_cost)mock_issue(&command,1);
        }
    }
    if(operation==3 && ui_poison_layout) {
        // Any wrapper read after native close faults on this private page.
        DWORD old=0;expect(VirtualProtect(layout,4096,PAGE_NOACCESS,&old)!=0);
        return;
    }
    put32(layout,0x180,0);
}
static void test_ui() {
    ui_district=table(0xE0D0,1,51);Obj force=table(0xDF30,1,51);
    ui_target=object(401);ui_actor=object(402);ui_actor[0xE0]=1;
    for(Obj o:{ui_target,ui_actor})slot(0x158,word(o,0x10),o);
    owners[ui_actor]=force;envoys={ui_actor};family.clear();advice={{{401,402},true}};
    batch_city=object(1);batch_district=ui_district;reject_batch=false;
    void *city_vtable[17]={};city_vtable[16]=reinterpret_cast<void *>(&mock_order_district);
    putptr(batch_city,0,city_vtable);
    ui_layout=static_cast<Obj>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    ui_dialog=static_cast<Obj>(calloc(1,0x300));ui_body=object(1);
    Obj state=static_cast<Obj>(calloc(1,0x600));ui_widget=static_cast<Obj>(calloc(1,0x300));
    putptr(ui_layout,0,image+0x1440E48);putptr(ui_layout,0x1A0,ui_district);
    putptr(state,0x478,ui_layout);putptr(state,0x470,ui_district);
    auto &actors=*reinterpret_cast<GameList *>(ui_layout+0x190);list_init(actors,0x13436B0);list_add(actors,ui_actor);
    native_functions[0x6F1D10]=reinterpret_cast<void *>(&ui_layout_native);
    native_functions[0x2C0820]=reinterpret_cast<void *>(&ui_setup_native);
    native_functions[0x6BE8E0]=reinterpret_cast<void *>(&ui_close);
    native_functions[0x92CEE0]=reinterpret_cast<void *>(&ui_label_lookup);
    native_functions[0x22EEC0]=reinterpret_cast<void *>(&ui_person_name);
    native_functions[0x6DD290]=reinterpret_cast<void *>(&ui_collect);
    native_functions[0x733BD0]=reinterpret_cast<void *>(&ui_state_native);
    native_functions[0x6BD740]=reinterpret_cast<void *>(&ui_reset_display);
    putptr(image+0x1440E48,0x190,reinterpret_cast<void *>(&ui_close_display));
    native_functions[0x1F09E0]=reinterpret_cast<void *>(&ui_result_notice);
    native_functions[0x930D20]=reinterpret_cast<void *>(&ui_preview_text);
    native_functions[0x6D3660]=reinterpret_cast<void *>(&ui_order_cost);
    // Exercise isolated column data: literal heading, native alias entries,
    // terminated tab lists, original caller settings and pointers preserved.
    for(unsigned i=0;i<213;++i)put32(image+0x1A2B850,i*0x60,i);
    put32(image+0x1A2B850,13*0x60,8);put32(image+0x1A2B850,3*0x60+0x20,3);
    static const int ids[3][4]={{0,1,3,-1},{0,61,62,-1},{0,29,28,-1}};
    auto tabs=reinterpret_cast<RecruitTab *>(image+0x1A31330);
    for(unsigned i=0;i<3;++i)tabs[i]={ids[i],nullptr,static_cast<int>(i),0};
    tabs[3]={};expect(recruit_prepare_columns());
    expect(dword(recruit_columns[13],0)==8 && dword(recruit_columns[213],0)==213);
    expect(wcscmp(*reinterpret_cast<const wchar_t **>(recruit_columns[213]+8),L"등용가능성")==0);
    for(unsigned i=0;i<3;++i)expect(recruit_tabs[i].ids[0]==0 && recruit_tabs[i].ids[3]==213 && recruit_tabs[i].ids[4]==-1);
    expect(dword(recruit_columns[213],0x10)==240);
    unsigned char config[0x400]={};putptr(config,8,image+0x1A31330);putptr(config,16,image+0x1A2B850);
    put32(config,0x18,1);put32(config,0x1C,1);
    {RecruitScope scope(ui_district);expect(recruit_selector_init(ui_dialog,config)==1);}
    expect(pointer(ui_config_copy,8)==reinterpret_cast<Obj>(recruit_tabs));
    expect(pointer(ui_config_copy,16)==reinterpret_cast<Obj>(recruit_columns));
    expect(dword(ui_config_copy,0x18)==1 && dword(ui_config_copy,0x1C)==1);
    expect(pointer(config,8)==image+0x1A31330 && pointer(config,16)==image+0x1A2B850);
    recruit_selector_init(ui_dialog,config);expect(pointer(ui_config_copy,16)==image+0x1A2B850);
    expect(wcscmp(recruit_reason_text(ui_target),L"")==0);
    {RecruitScope scope(ui_district);
        expect(wcscmp(recruit_reason_text(ui_target),L"가능성 있음")==0);
        put16(ui_target,0xD0,word(force,0x10));
        expect(wcsstr(recruit_reason_text(ui_target),L"군주 혐오")!=nullptr);
        put16(ui_target,0xBA,word(ui_actor,0x10));
        expect(wcscmp(recruit_reason_text(ui_target),L"가족 권고 시 가능성 있음")==0);
        for(unsigned loyalty:{100u,101u,120u,140u,255u}) {
            ui_target[0xE8]=static_cast<unsigned char>(loyalty);
            expect(personnel_recruitment_reason(ui_target,ui_district,ui_actor)==personnel::HighLoyalty);
            expect(wcscmp(recruit_reason_text(ui_target),L"불가: 충성도 100↑")==0);
            expect(personnel::recruitment_blocked(personnel::HighLoyalty));
        }
        ui_target[0xE8]=99;
        expect(wcscmp(recruit_reason_text(ui_target),L"가족 권고 시 가능성 있음")==0);
        ui_target[0xE8]=0;
        ui_actor[0x161]=1;expect(wcsstr(recruit_reason_text(ui_target),L"군주 혐오")!=nullptr);ui_actor[0x161]=0;
        owners[ui_actor]=object(20);expect(!recruit_family_available(ui_target,ui_district));owners[ui_actor]=force;
    }
    put16(ui_target,0xBA,0);put16(ui_target,0xD0,0);
    Obj enemy_force=object(501),enemy_ruler=object(502),spouse=object(503),brother=object(504);
    slot(0x158,502,enemy_ruler);slot(0x158,503,spouse);slot(0x158,504,brother);put16(enemy_force,0x10,502);
    owners[ui_target]=enemy_force;owners[spouse]=enemy_force;owners[brother]=enemy_force;
    {RecruitScope scope(ui_district);
        put16(ui_target,0xBA,503);
        expect(personnel_recruitment_reason(ui_target,ui_district,nullptr)==personnel::ForeignSpouse);
        expect(wcsstr(recruit_reason_text(ui_target),L"타세력 배우자")!=nullptr);
        owners[spouse]=nullptr;expect(personnel_recruitment_reason(ui_target,ui_district,nullptr)==personnel::ForeignSpouse);
        spouse[0xE6]=9;expect(personnel_recruitment_reason(ui_target,ui_district,nullptr)==personnel::Eligible);
        spouse[0xE6]=0;owners[spouse]=force;expect(personnel_recruitment_reason(ui_target,ui_district,nullptr)==personnel::Eligible);
        put16(ui_target,0xBA,0);put16(ui_target,0xBC,100);put16(brother,0xBC,100);family={brother};
        expect(personnel_recruitment_reason(ui_target,ui_district,nullptr)==personnel::ForeignSwornSibling);
        owners[brother]=force;expect(personnel_recruitment_reason(ui_target,ui_district,nullptr)==personnel::Eligible);
        put16(ui_target,0xBC,0);family.clear();
        put16(enemy_ruler,0xC0,401);expect(personnel_recruitment_reason(ui_target,ui_district,nullptr)==personnel::Eligible);
        put16(ui_target,0xC0,502);
        expect(personnel_recruitment_reason(ui_target,ui_district,nullptr)==personnel::LikesCurrentRuler);
        expect(wcscmp(recruit_reason_text(ui_target),L"주의: 상대군주 친애")==0);
        expect(!personnel::recruitment_blocked(personnel::LikesCurrentRuler));
        for(unsigned loyalty:{0u,1u,97u,99u,100u,101u,255u}) {
            ui_target[0xE8]=static_cast<unsigned char>(loyalty);
            unsigned reason=personnel_recruitment_reason(ui_target,ui_district,nullptr);
            expect((reason & personnel::LikesCurrentRuler)!=0);
            expect(personnel::recruitment_blocked(reason)==(loyalty>=100));
            expect(wcscmp(recruit_reason_text(ui_target),loyalty>=100?
                L"불가: 상대군주 친애(100↑)":L"주의: 상대군주 친애")==0);
        }
        ui_target[0xE8]=97;
        // A caution must never hide a separate actual relationship blocker.
        put16(ui_target,0xD0,word(force,0x10));
        expect(personnel::recruitment_blocked(personnel_recruitment_reason(ui_target,ui_district,nullptr)));
        expect(wcsstr(recruit_reason_text(ui_target),L"군주 혐오")!=nullptr);
        put16(ui_target,0xD0,0);
        put16(ui_target,0xBA,402);
        expect(personnel_recruitment_reason(ui_target,ui_district,ui_actor)==
               (personnel::FamilyEnvoyException|personnel::LikesCurrentRuler));
        expect(wcscmp(recruit_reason_text(ui_target),L"주의: 상대군주 친애")==0);
        ui_target[0xE8]=100;
        expect(personnel_recruitment_reason(ui_target,ui_district,ui_actor)==
               (personnel::HighLoyalty|personnel::LikesCurrentRuler));
        // Native ban comparison precedes the family exception and loyalty.
        put16(ui_target,0x162,word(force,0x10));
        expect(wcscmp(recruit_reason_text(ui_target),L"불가: 현 세력 사관금지")==0);
        ui_target[0xE8]=0;
        for(unsigned timer:{0u,1u,12u,24u,255u}) {
            ui_target[0x164]=static_cast<unsigned char>(timer);ui_target[0x165]=0xA5;
            expect(personnel_recruitment_reason(ui_target,ui_district,ui_actor)==
                   (personnel::ServiceBan|personnel::LikesCurrentRuler));
            expect(!recruit_family_available(ui_target,ui_district) ||
                   wcscmp(recruit_reason_text(ui_target),L"불가: 현 세력 사관금지")==0);
            expect(wcscmp(recruit_reason_text(ui_target),L"불가: 현 세력 사관금지")==0);
            expect(personnel::recruitment_hard_blocked(personnel::ServiceBan));
        }
        // An unrelated ruler, or a force slot that happens to match the ban,
        // does not forbid service to this recruiting ruler.
        put16(ui_target,0x162,502);
        expect(!personnel::recruitment_blocked(personnel_recruitment_reason(ui_target,ui_district,ui_actor)));
        put16(ui_target,0x162,ui_district[0x10]);
        expect(!personnel::recruitment_blocked(personnel_recruitment_reason(ui_target,ui_district,ui_actor)));
        // A ruler change must immediately change whose ban is relevant.
        put16(ui_target,0x162,word(force,0x10));
        unsigned previous_ruler=word(force,0x10);put16(force,0x10,502);
        expect(!(personnel_recruitment_reason(ui_target,ui_district,ui_actor)&personnel::ServiceBan));
        put16(force,0x10,previous_ruler);
        put16(ui_target,0x162,0);ui_target[0x164]=24;
        expect(!(personnel_recruitment_reason(ui_target,ui_district,ui_actor)&personnel::ServiceBan));
        ui_target[0x164]=0;ui_target[0x165]=0;
        ui_target[0xE8]=0;put16(ui_target,0xBA,0);put16(ui_target,0xC0,0);
    }
    // Later paint/sort callbacks retain the selector context outside TLS.
    {RecruitScope scope(ui_district);recruit_selector_init(ui_dialog,config);}
    expect(recruit_district==nullptr && recruit_display_district()==ui_district);
    expect(wcscmp(recruit_reason_text(ui_target),L"가능성 있음")==0);
    Obj prior_world=world();putptr(image,0x277DF18,image);
    expect(recruit_display_district()==nullptr);putptr(image,0x277DF18,prior_world);
    Obj prior_district=table(0xE0D0,1,51);slot(0xE0D0,1,nullptr);
    expect(recruit_display_district()==nullptr);slot(0xE0D0,1,prior_district);
    recruit_selector_init(ui_dialog,config);expect(recruit_display_district()==nullptr);
    // The removed automatic button allocates/registers no widget or callback.
    enabled=0;expect(recruit_layout_init(ui_layout,ui_district)==7);enabled=1;
    expect(recruit_layout_init(ui_layout,ui_district)==7 && ui_layout_calls==2);
    expect(pointer(ui_layout+0x130,8)==nullptr);

    Obj second=object(403),last=object(404),other_actor=object(405),third_actor=object(406);
    for(Obj o:{second,last,other_actor,third_actor})slot(0x158,word(o,0x10),o);
    for(Obj o:{other_actor,third_actor}) {owners[o]=force;o[0xE0]=1;}
    envoys={ui_actor,other_actor,third_actor};
    advice={{{401,402},true},{{403,402},true},{{401,405},true},{{403,406},true},{{404,402},true}};
    ui_district[0x14]=12;
    // Eligibility exclusions and exact caller restoration. Existing functor
    // storage remains untouched; close capture no longer depends on it.
    unsigned char original_config[0x400];
    for(unsigned scenario=0;scenario<10;++scenario) {
        memset(config,0,sizeof(config));putptr(config,8,image+0x1A31330);putptr(config,16,image+0x1A2B850);
        put32(config,0x18,1);put32(config,0x1C,1);
        memset(config+0x2C8,0xA5,56); // preserve nonzero unused inline bytes
        if(scenario==1)put32(config,0x18,2);
        if(scenario==2)put32(config,0x1C,2);
        if(scenario==3)putptr(config,0x300,ui_body);
        if(scenario==4)putptr(config,8,image+0x1A31390);
        if(scenario==5)putptr(config,16,image+0x1A2B8B0);
        ui_district[0x14]=scenario==6?1:scenario==7?0:12;
        ui_cost=scenario==8?0:scenario==9?-1:1;
        memcpy(original_config,config,sizeof(config));
        recruit_pick_dialog=nullptr;
        {
            RecruitScope scope(ui_district);RecruitTargetScope target_scope(ui_layout,scenario!=0);
            expect(recruit_selector_init(ui_dialog,config)==1);
        }
        expect(memcmp(config,original_config,sizeof(config))==0);
        // 2026-10-25: one-order picks (6,7) record dialog without changing mode.
        bool changed=scenario==3 || scenario>=8,recorded=changed || scenario==6 || scenario==7;
        expect(dword(ui_config_copy,0x18)==(changed?2:dword(config,0x18)));
        expect(dword(ui_config_copy,0x1C)==(changed?12:dword(config,0x1C)));
        expect(memcmp(ui_config_copy+0x2C8,config+0x2C8,64)==0);
        expect(recruit_pick_dialog==(recorded?ui_dialog:nullptr));
    }
    ui_cost=2;ui_district[0x14]=11;
    putptr(config,0x300,nullptr);put32(config,0x18,1);put32(config,0x1C,1);
    memcpy(original_config,config,sizeof(config));
    {RecruitScope scope(ui_district);RecruitTargetScope target_scope(ui_layout,true);
        expect(recruit_selector_init(ui_dialog,config)==1);}
    expect(dword(ui_config_copy,0x1C)==5 && memcmp(config,original_config,sizeof(config))==0);
    GameList selected;list_init(selected,0x13436B0);putptr(ui_config_copy,0x80,&selected);
    putptr(ui_dialog,0x1A8,ui_config_copy);
    unsigned closes_before=ui_close_calls,revision_before=recruit_multi_revision;
    // A different dialog must pass through exactly once without dereferencing
    // its config or consuming the recorded selection dialog.
    recruit_selector_close(ui_body,0x7FFFFFFD);
    expect(ui_close_calls==++closes_before && ui_closed_dialog==ui_body && ui_closed_code==0x7FFFFFFD &&
           recruit_pick_dialog==ui_dialog && recruit_multi_revision==revision_before);
    for(unsigned code:{0x7FFFFFFDu,0x7FFFFFFEu}) {
        for(unsigned count:{3u,0u,1u}) {
            list_free(selected);list_init(selected,0x13436B0);
            ui_pick_list=count==3?std::vector<Obj>{ui_target,second,last}:
                         count==1?std::vector<Obj>{ui_target}:std::vector<Obj>{};
            ui_collect(ui_body,&selected);
            {RecruitScope scope(ui_district);RecruitTargetScope target_scope(ui_layout,true);
                recruit_selector_init(ui_dialog,config);}
            putptr(ui_config_copy,0x80,&selected);putptr(ui_dialog,0x1A8,ui_config_copy);
            unsigned collected=ui_collect_calls,revision=recruit_multi_revision;
            // Snapshot context must come from initialization, outside its TLS scope.
            recruit_bind_context(nullptr);recruit_selector_close(ui_dialog,code);
            expect(ui_close_calls==++closes_before && ui_closed_dialog==ui_dialog && ui_closed_code==code);
            expect(!recruit_pick_dialog && ui_collect_calls==collected);
            expect(recruit_multi_revision==revision+(count?1u:0u));
            if(count==3) {
                expect(list_first(selected)->value==last && list_first(selected)->next->value==second &&
                       list_first(selected)->next->next->value==ui_target);
                expect(recruit_multi.count==3 && recruit_multi.people[0]==last && recruit_multi.people[1]==second &&
                       recruit_multi.people[2]==ui_target && recruit_multi.layout==ui_layout &&
                       recruit_multi.root==world() && recruit_multi.district==ui_district && recruit_multi.district_id==1);
            } else expect(recruit_multi.count==(count?0u:3u));
        }
    }
    // Other close codes release the record without inspecting output.
    {RecruitScope scope(ui_district);RecruitTargetScope target_scope(ui_layout,true);
        recruit_selector_init(ui_dialog,config);}
    putptr(ui_dialog,0x1A8,nullptr);revision_before=recruit_multi_revision;
    recruit_selector_close(ui_dialog,42);
    expect(ui_close_calls==++closes_before && ui_closed_code==42 && !recruit_pick_dialog &&
           recruit_multi_revision==revision_before);
    list_free(selected);
    // Native state selection releases its output; the copied snapshot lives on.
    ui_pick_list={ui_target,second,last};putptr(ui_layout,0x1A8,ui_widget);
    ui_cost=1;ui_district[0x14]=12;put32(ui_layout,0x180,2);recruit_state_update(state);
    expect(pointer(ui_layout,0x188)==last && recruit_multi.count==3 && recruit_district==nullptr && !recruit_target_pick);
    expect(ui_preview[0xAB]==L"9" && ui_preview[0xAC]==L"(^03-3^00)");
    expect(ui_label==L"은관 외 2명");
    unsigned previews_before=ui_preview_calls;recruit_state_update(state);
    expect(ui_preview_calls==previews_before); // no per-frame text writes
    ui_label_node=7;put32(ui_layout,0x180,1);recruit_state_update(state);
    expect(ui_label==L"은관 외 2명");
    ui_name.assign(100,L'가');put32(ui_layout,0x180,1);recruit_state_update(state);
    expect(ui_label.size()==63 && ui_label==ui_name.substr(0,63));ui_name=L"은관";
    putptr(ui_layout,0x1A8,nullptr);previews_before=ui_preview_calls;
    put32(ui_layout,0x180,1);recruit_state_update(state);
    expect(ui_preview_calls==previews_before+2);putptr(ui_layout,0x1A8,ui_widget);
    // Actor changes refresh only on native actions; use the district fallback
    // when layout+1A0 points elsewhere.
    Obj foreign_district=object(50);foreign_district[0x14]=99;putptr(ui_layout,0x1A0,foreign_district);
    list_add(actors,other_actor);put32(ui_layout,0x180,1);recruit_state_update(state);
    expect(ui_preview[0xAB]==L"8" && ui_preview[0xAC]==L"(^03-4^00)");
    list_free(actors);list_init(actors,0x13436B0);list_add(actors,ui_actor);
    putptr(ui_layout,0x1A0,ui_district);free(foreign_district);
    // Empty cancellation output retains the previous confirmed target.
    ui_accept=false;put32(ui_layout,0x180,2);recruit_state_update(state);ui_accept=true;
    expect(recruit_multi.count==3 && pointer(ui_layout,0x188)==last);
    // A mismatched target, world, district or layout must never auto-dispatch.
    ui_send=false;batch_orders.clear();
    RecruitMultiSnapshot saved_batch=recruit_multi;
    for(unsigned operation:{3u,4u}) {
        for(unsigned mismatch=0;mismatch<5;++mismatch) {
            recruit_multi=saved_batch;putptr(ui_layout,0x188,last);
            if(mismatch==0)putptr(ui_layout,0x188,second);
            if(mismatch==1)recruit_multi.root=image;
            if(mismatch==2)recruit_multi.layout=ui_dialog;
            if(mismatch==3)recruit_multi.district=ui_body;
            if(mismatch==4)slot(0xE0D0,1,nullptr);
            expect(!recruit_multi_matches(ui_layout,ui_district));
            unsigned originals=ui_original_calls,notices=ui_notices,previews=ui_preview_calls;
            put32(ui_layout,0x180,operation);recruit_state_update(state);
            expect(ui_original_calls==originals+1 && batch_orders.empty() && recruit_multi.count==3 &&
                   ui_notices==notices && ui_preview_calls==previews);
            slot(0xE0D0,1,ui_district);
        }
    }
    recruit_multi=saved_batch;putptr(ui_layout,0x188,last);
    batch_issue_observer=&ui_issue_event;
    // User decision 2026-10-25: both 3/4 send extras and show the notice
    // before original entry, even if the native screen dispatch later fails.
    // Case 3 makes layout unreadable at native close: no post-call UI access.
    for(unsigned operation:{3u,4u})for(bool native_send:{false,true}) {
        for(Obj actor:envoys)put16(actor,0x15E,0);
        ui_district[0x14]=12;put32(ui_layout,0x180,2);recruit_state_update(state);batch_orders.clear();
        unsigned notices=ui_notices,originals=ui_original_calls,previews=ui_preview_calls;
        ui_send=native_send;ui_track_dispatch=true;ui_native_started=false;ui_dispatch_events.clear();
        ui_poison_layout=operation==3;
        put32(ui_layout,0x180,operation);recruit_state_update(state);
        std::vector<unsigned> expected={403,401,0x1F09E0,0x733BD0};
        if(native_send)expected.push_back(404);
        expect(ui_dispatch_events==expected && ui_original_calls==originals+1 && ui_notices==notices+1);
        expect(batch_orders.size()==(native_send?3u:2u) && batch_orders[0]==std::make_pair(403u,406u) &&
               batch_orders[1]==std::make_pair(401u,405u));
        expect(recruit_multi.count==0 && ui_district[0x14]==(native_send?9:10) && ui_preview_calls==previews);
        expect(ui_notice.find(L"명령 2건")!=std::wstring::npos);
        if(native_send)expect(batch_orders[2]==std::make_pair(404u,402u));
        else expect(!(word(ui_actor,0x15E)&1));
        if(ui_poison_layout) {
            DWORD old=0;expect(VirtualProtect(ui_layout,4096,PAGE_READWRITE,&old)!=0);
        }
        ui_track_dispatch=false;ui_poison_layout=false;
    }
    // All screen actors are excluded before native code has marked them busy.
    advice[{403,405}]=true;
    advice[{404,402}]=false;advice[{404,405}]=true; // A later actor's recommendation preserves BOTH native actors.
    for(Obj actor:envoys)put16(actor,0x15E,0);
    ui_district[0x14]=12;put32(ui_layout,0x180,2);recruit_state_update(state);
    list_add(actors,other_actor);batch_orders.clear();ui_send=true;
    ui_track_dispatch=true;ui_native_started=false;ui_dispatch_events.clear();
    put32(ui_layout,0x180,4);recruit_state_update(state);ui_track_dispatch=false;
    expect(ui_dispatch_events==std::vector<unsigned>({403,0x1F09E0,0x733BD0,404,404}));
    expect(batch_orders.size()==3 && batch_orders[0]==std::make_pair(403u,406u) &&
           batch_orders[1]==std::make_pair(404u,402u) && batch_orders[2]==std::make_pair(404u,405u) &&
           ui_district[0x14]==9);
    // Cost x two screen actors is reserved; an extra recommended actor must
    // not spend the last four orders before the original screen dispatch.
    Obj fourth_actor=object(407);slot(0x158,407,fourth_actor);owners[fourth_actor]=force;fourth_actor[0xE0]=1;
    envoys.push_back(fourth_actor);advice[{401,407}]=true;
    for(Obj actor:envoys)put16(actor,0x15E,0);
    ui_cost=batch_cost=2;ui_district[0x14]=6;put32(ui_layout,0x180,2);recruit_state_update(state);
    batch_orders.clear();put32(ui_layout,0x180,4);recruit_state_update(state);
    expect(batch_orders.size()==3 && batch_orders[0]==std::make_pair(403u,406u) &&
           batch_orders[1]==std::make_pair(404u,402u) && batch_orders[2]==std::make_pair(404u,405u));
    expect(ui_district[0x14]==0 && !(word(fourth_actor,0x15E)&1) && ui_notice.find(L"제외했습니다")!=std::wstring::npos);
    ui_cost=batch_cost=1;envoys.pop_back();owners.erase(fourth_actor);slot(0x158,407,nullptr);free(fourth_actor);
    // Skipped-only results still show their notice before original entry.
    for(Obj actor:envoys)put16(actor,0x15E,0);
    ui_district[0x14]=2;put32(ui_layout,0x180,2);recruit_state_update(state);batch_orders.clear();
    ui_track_dispatch=true;ui_native_started=false;ui_dispatch_events.clear();
    put32(ui_layout,0x180,4);recruit_state_update(state);ui_track_dispatch=false;
    expect(ui_dispatch_events==std::vector<unsigned>({0x1F09E0,0x733BD0,404,404}));
    expect(batch_orders.size()==2 && ui_district[0x14]==0 && ui_notice.find(L"내리지 못했습니다")!=std::wstring::npos);
    batch_issue_observer=nullptr;
    // User decision 2026-10-25: no recommended screen actor (including an
    // empty actor list) skips native dispatch, reports before reset, and
    // closes only case 3. All targets advised against still get one notice.
    auto saved_advice=advice;
    for(unsigned operation:{3u,4u})for(unsigned screen_actors:{0u,1u,2u})for(bool extras:{false,true}) {
        advice.clear();if(extras)advice[{403,402}]=true;
        for(Obj actor:envoys)put16(actor,0x15E,0);
        list_free(actors);list_init(actors,0x13436B0);
        if(screen_actors)list_add(actors,ui_actor);
        if(screen_actors==2)list_add(actors,other_actor);
        ui_district[0x14]=1;ui_pick_list={ui_target,second,last};
        // Use the confirmed snapshot already validated by the selector tests.
        recruit_multi=saved_batch;putptr(ui_layout,0x188,last);
        batch_orders.clear();ui_track_dispatch=true;ui_native_started=false;ui_dispatch_events.clear();
        batch_issue_observer=&ui_issue_event;
        unsigned originals=ui_original_calls,notices=ui_notices,resets=ui_reset_calls;
        unsigned closes=ui_display_close_calls,previews=ui_preview_calls;
        put32(ui_layout,0x180,operation);recruit_state_update(state);
        std::vector<unsigned> expected;
        if(extras)expected.push_back(403);
        expected.push_back(0x1F09E0);expected.push_back(0x6BD740);
        if(operation==3)expected.push_back(0x190);
        expect(ui_dispatch_events==expected && ui_original_calls==originals && ui_notices==notices+1 &&
               ui_reset_calls==resets+1 && ui_display_close_calls==closes+(operation==3?1u:0u));
        expect(recruit_multi.count==0 && ui_preview_calls==previews);
        expect(batch_orders.size()==(extras?1u:0u) && ui_district[0x14]==(extras?0:1));
        if(extras)expect(batch_orders[0]==std::make_pair(403u,402u)); // Released actor AND last order.
        expect(ui_notice.find(extras?L"군사가 반대한 대상 1명은 보내지 않았습니다.":
                                    L"군사가 반대한 대상 3명은 보내지 않았습니다.")!=std::wstring::npos);
        if(operation==3) {
            DWORD old=0;expect(VirtualProtect(ui_layout,4096,PAGE_READWRITE,&old)!=0);
        }
        expect(!pointer(ui_layout,0x188) && dword(ui_layout,0x180)==0 && !list_first(actors));
        ui_track_dispatch=false;batch_issue_observer=nullptr;
    }
    advice=saved_advice;
    list_free(actors);list_init(actors,0x13436B0);list_add(actors,ui_actor);
    // One-person confirmation clears a prior multi snapshot; its dispatch is
    // wholly native, with no batch notice or preview replacement.
    for(Obj actor:envoys)put16(actor,0x15E,0);
    ui_district[0x14]=12;put32(ui_layout,0x180,2);recruit_state_update(state);
    expect(recruit_multi.count==3);ui_pick_list={ui_target};
    ui_district[0x14]=1;previews_before=ui_preview_calls;put32(ui_layout,0x180,2);recruit_state_update(state);
    expect(recruit_multi.count==0 && pointer(ui_layout,0x188)==ui_target && ui_preview_calls==previews_before);
    expect(dword(ui_config_copy,0x18)==1 && dword(ui_config_copy,0x1C)==1 && !recruit_pick_dialog);
    advice[{401,402}]=false; // One selected target keeps the original decision even without advice.
    unsigned notices_before=ui_notices,originals_before=ui_original_calls,resets_before=ui_reset_calls;
    batch_orders.clear();put32(ui_layout,0x180,4);recruit_state_update(state);
    expect(batch_orders.size()==1 && batch_orders[0]==std::make_pair(401u,402u) && ui_notices==notices_before &&
           ui_original_calls==originals_before+1 && ui_reset_calls==resets_before);
    advice=saved_advice;
    // Filtering: reject non-Person/invalid/duplicate entries; cap at 256.
    ui_pick_list={ui_target,nullptr,ui_target,ui_body,second,last};
    put32(ui_layout,0x180,2);recruit_state_update(state);
    expect(recruit_multi.count==3 && recruit_multi.people[0]==last);
    std::vector<Obj> many;
    for(unsigned id=3000;id<3260;++id) {Obj person=object(id);slot(0x158,id,person);many.push_back(person);}
    ui_pick_list=many;put32(ui_layout,0x180,2);recruit_state_update(state);
    expect(recruit_multi.count==256 && recruit_multi.people[0]==many[255] && recruit_multi.people[255]==many[0]);
    ui_pick_list.clear();revision_before=recruit_multi_revision;
    put32(ui_layout,0x180,2);recruit_state_update(state);
    expect(recruit_multi.count==256 && recruit_multi_revision==revision_before);recruit_multi={};
    for(Obj person:many) {slot(0x158,word(person,0x10),nullptr);free(person);}
    // Prefer own-force advisor, preserve the existing result notice wording.
    put16(force,0x52,402);
    PersonnelBatchResult partial;partial.sent=2;partial.no_orders=1;
    recruit_report_automatic(ui_district,partial);
    expect(ui_speaker==402 && ui_notice.find(L"명령 2건")!=std::wstring::npos);
    expect(ui_notice.find(L"군사가 등용을 권하는 실행무장만 배정했습니다")!=std::wstring::npos);
    expect(ui_notice.find(L"제외했습니다")!=std::wstring::npos);
    owners[spouse]=enemy_force;put16(force,0x52,503);recruit_report_automatic(ui_district,partial);
    expect(ui_speaker==word(force,0x10));put16(force,0x52,0);
    partial.no_orders=0;partial.advised_against=1;recruit_report_automatic(ui_district,partial);
    expect(ui_notice.find(L"제외했습니다")==std::wstring::npos);
    expect(ui_notice.find(L"군사가 반대한 대상 1명은 보내지 않았습니다.")!=std::wstring::npos);
    partial.sent=0;recruit_report_automatic(ui_district,partial);
    expect(ui_notice.find(L"내리지 못했습니다")!=std::wstring::npos && ui_notice.find(L"군사 반대 여부")!=std::wstring::npos);
    partial.sent=6000;partial.advised_against=6000;recruit_report_automatic(ui_district,partial);
    expect(ui_notice.find(L"군사가 반대한 대상 6000명은 보내지 않았습니다.")!=std::wstring::npos);
    expect(ui_notice.size()+1<=0x192/sizeof(wchar_t));
    bool close_registered=false;
    for(const auto &link:recruit_ui_links) {
        putptr(image,link.slot,image+link.original);
        if(link.slot==0x13AC708)close_registered=link.original==0x6BE8E0 &&
            link.replacement==reinterpret_cast<void *>(&recruit_selector_close);
    }
    expect(close_registered);
    expect(recruit_ui_guards_match());expect(publish_recruit_ui());expect(!recruit_ui_guards_match());
    restore_recruit_ui();expect(recruit_ui_guards_match());
    const unsigned char loyalty_number[]={0x48,0x0F,0x4E,0xC2},loyalty_string[]={0x0F,0x4F,0xD8};
    memcpy(image+0x34A8D6,loyalty_number,4);memcpy(image+0x35CA35,loyalty_string,3);
    expect(loyalty_guards_match());image[0x34A8D6]^=1;expect(!loyalty_guards_match());image[0x34A8D6]^=1;
    image[0x35CA35]^=1;expect(!loyalty_guards_match());image[0x35CA35]^=1;
    list_free(actors);free(state);VirtualFree(ui_layout,0,MEM_RELEASE);free(ui_dialog);free(ui_widget);free(ui_body);free(batch_city);
}
static Obj alert_player,alert_hidden;
static int alert_native_result=7;
static unsigned alert_native_calls;
static Obj __fastcall alert_get_player(Obj) {return alert_player;}
static int __fastcall alert_native_candidate(Obj,Obj) {++alert_native_calls;return alert_native_result;}
static void __fastcall alert_native_location(Obj,int *kind,int *id) {*kind=5;*id=19;}
static Obj __fastcall alert_native_search(GameList *areas,int *people,int *items,int *scenery,int *selected) {
    expect(areas && selected && recruit_alert_force==alert_player);
    int kind,id;recruit_alert_location(alert_hidden,&kind,&id);
    *people=kind==5?1:0;*items=1;*scenery=1;*selected=19;
    return alert_hidden;
}
static void test_alert() {
    Obj district=table(0xE0D0,1,51),force=table(0xDF30,1,51);
    Obj target=object(2100),other_ruler=object(2101),context=object(1);
    slot(0x158,2101,other_ruler);putptr(root,0x871E8,context);alert_player=force;
    native_functions[0x32E910]=reinterpret_cast<void *>(&alert_get_player);
    native_functions[0x1EF680]=reinterpret_cast<void *>(&alert_native_candidate);
    native_functions[0x231C60]=reinterpret_cast<void *>(&alert_native_location);
    native_functions[0x2B2F10]=reinterpret_cast<void *>(&alert_native_search);
    target[0xE6]=5;unsigned ruler=word(force,0x10);
    expect(recruit_alert_candidate(target,district)==7);
    for(unsigned i=0;i<8;++i) {
        put16(target,0xD0+i*2,ruler);
        unsigned char before[0x400];memcpy(before,target,sizeof(before));
        expect(recruit_alert_candidate(target,district)==0);
        expect(memcmp(target,before,sizeof(before))==0);
        put16(target,0xD0+i*2,0);
    }
    put16(target,0xD0,ruler);enabled=0;expect(recruit_alert_candidate(target,district)==7);enabled=1;
    alert_player=nullptr;expect(recruit_alert_candidate(target,district)==7);alert_player=force;
    unsigned own=district[0x10];district[0x10]=0;expect(recruit_alert_candidate(target,district)==7);district[0x10]=static_cast<unsigned char>(own);
    put16(force,0x10,2101);expect(recruit_alert_candidate(target,district)==7);put16(force,0x10,ruler);
    alert_native_result=0;expect(recruit_alert_candidate(target,district)==0);alert_native_result=7;
    expect(alert_native_calls==14);
    // Native location remains available outside the menu alert scope.
    int kind,id,people,items,scenery,selected;alert_hidden=target;target[0xE6]=8;
    recruit_alert_location(target,&kind,&id);expect(kind==5 && id==19);
    GameList areas={};recruit_alert_force=other_ruler;
    expect(recruit_alert_search(&areas,&people,&items,&scenery,&selected)==target);
    expect(!people && items==1 && scenery==1 && selected==19 && recruit_alert_force==other_ruler);
    recruit_alert_force=nullptr;put16(target,0xD0,0);
    recruit_alert_search(&areas,&people,&items,&scenery,&selected);expect(people==1 && items==1 && scenery==1);
    put16(target,0xD0,ruler);target[0xE6]=5;
    recruit_alert_search(&areas,&people,&items,&scenery,&selected);expect(people==1);
    // Actual emitted JMP stubs preserve RCX/RDX/R8/R9 and argument five.
    Obj relay=allocate_near(image+0x2DF204);
    expect(relay!=nullptr);RecruitAlertCode code;
    for(const auto &entry:recruit_alert_entries)memcpy(image+(entry.rva&~15u),entry.block,16);
    expect(recruit_alert_guards_match());
    for(const auto &entry:recruit_alert_entries) {
        image[entry.rva]^=1;expect(!recruit_alert_guards_match());image[entry.rva]^=1;
    }
    expect(prepare_recruit_alert(code,relay));DWORD old=0;
    expect(VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old));FlushInstructionCache(GetCurrentProcess(),relay,4096);
    expect(reinterpret_cast<int(__fastcall *)(Obj,Obj)>(relay+2944)(target,district)==0);
    target[0xE6]=8;
    expect(reinterpret_cast<Obj(__fastcall *)(GameList *,int *,int *,int *,int *)>(relay+2976)
        (&areas,&people,&items,&scenery,&selected)==target && !people && selected==19);
    expect(publish_recruit_alert(code));expect(!recruit_alert_guards_match());
    for(unsigned i=0;i<3;++i) {
        const auto &link=code.links[i];unsigned offset=link.rva-link.block;
        expect(image+link.rva+5+*reinterpret_cast<int *>(image+link.rva+1)==relay+2944+i*32);
        for(unsigned j=0;j<16;++j)if(j<offset || j>=offset+5)expect(image[link.block+j]==link.original[j]);
    }
    restore_recruit_alert(code);expect(recruit_alert_guards_match());
    expect(VirtualProtect(relay,4096,PAGE_READWRITE,&old));
    expect(prepare_recruit_alert(code,relay));image[code.links[1].block]^=1;
    expect(!publish_recruit_alert(code));restore_recruit_alert(code);
    expect(memcmp(image+code.links[0].block,code.links[0].original,16)==0);
    expect(image[code.links[1].block]==(code.links[1].original[0]^1));
    image[code.links[1].block]^=1;expect(recruit_alert_guards_match());
    VirtualFree(relay,0,MEM_RELEASE);putptr(root,0x871E8,nullptr);slot(0x158,2101,nullptr);
    free(context);free(target);free(other_ruler);
}
int main() {
    if(personnel_fixture_main()!=0)return 1;
    test_ui();
    test_alert();
    printf("{\"status\":\"recruitment_ui_fixture_passed\",\"checks\":%u,\"rendering_verified\":false,\"gameplay_verified\":false,\"installed\":false}\n",checks);
    return 0;
}
