// Exercises deployed AI callbacks, with native order side effects simulated.
static void recruit_ai_expect(bool ok,unsigned line) {
    ++checks;if(!ok) {fprintf(stderr,"Recruitment AI fixture failed at line %u (check %u)\n",line,checks);exit(1);}
}
#define expect(value) recruit_ai_expect((value),__LINE__)
static unsigned ai_native_calls,ai_sent,ai_player_sent;
static bool ai_reject_family;
static int __fastcall mock_ai_command_valid(PersonnelEmployCommand *command) {
    return !ai_reject_family && mock_command_valid(command);
}
static void __fastcall mock_ai_issue(PersonnelEmployCommand *command,int player) {
    expect(mock_command_valid(command) && batch_district[0x14]>0);
    put16(command->actor,0x15E,word(command->actor,0x15E)|1);--batch_district[0x14];
    if(player)++ai_player_sent;else ++ai_sent;
}
static uint64_t __fastcall mock_ai_native_command(Obj node,Obj context,Obj) {
    ++ai_native_calls;
    Obj actor=personnel_context_object(node,context,0x38),target=personnel_context_object(node,context,0x3C);
    if(!valid(actor) || !valid(target))return 2;
    PersonnelEmployCommand command={target,actor,0,0};
    fn<void(__fastcall *)(PersonnelEmployCommand *,int)>(0x1ED920)(&command,0);
    return 1;
}
static void test_recruit_ai() {
    Obj district=object(5),force=object(2101),ruler=object(2101),other_force=object(2102),other_ruler=object(2102);
    Obj target=object(2103),actor=object(2104),relative=object(2105),allowed=object(2106),node=object(1);
    district[0x10]=6;slot(0xE0D0,5,district);slot(0xDF30,6,force);slot(0xDF30,7,other_force);
    for(Obj p:{ruler,other_ruler,target,actor,relative,allowed})slot(0x158,word(p,0x10),p);
    actor[0xE0]=5;owners[actor]=force;owners[relative]=other_force;owners[target]=other_force;
    put32(node,0x38,1);put32(node,0x3C,2);envoys={actor};family.clear();batch_district=district;
    void *old_command=native_functions[0x2CC50],*old_issue=native_functions[0x1ED920],*old_check=native_functions[0x1EFE60];
    native_functions[0x2CC50]=reinterpret_cast<void *>(&mock_ai_native_command);
    native_functions[0x1ED920]=reinterpret_cast<void *>(&mock_ai_issue);
    native_functions[0x1EFE60]=reinterpret_cast<void *>(&mock_ai_command_valid);
    GameList candidates;list_init(candidates);
    auto filter=[&]() {
        values={{1,district},{2,reinterpret_cast<Obj>(&candidates)}};
        list_add(candidates,target);list_add(candidates,target);list_add(candidates,allowed);
        expect(recruit_ai_targets(node,node,node)==1 && contains(candidates,allowed));
        bool keep=contains(candidates,target);personnel_remove(candidates,target);personnel_remove(candidates,allowed);
        return keep;
    };
    auto dispatch=[&](bool succeeds) {
        values={{1,actor},{2,target}};district[0x14]=5;put16(actor,0x15E,0);
        ai_native_calls=ai_sent=0;
        expect(recruit_ai_command(node,node,node)==(succeeds?1u:2u));
        expect(ai_native_calls==(succeeds?1u:0u) && ai_sent==(succeeds?1u:0u));
        expect(district[0x14]==(succeeds?4u:5u) && (word(actor,0x15E)&1u)==(succeeds?1u:0u));
        put16(actor,0x15E,0);
    };
    expect(filter());dispatch(true);
    // Relations can change after candidate selection: check again immediately
    // before dispatch so a formerly eligible target spends no order.
    expect(filter());put16(target,0xD0,2101);dispatch(false);put16(target,0xD0,0);
    put16(target,0xD0,2101);expect(!filter());dispatch(false);
    // Explicit AI dislike policy remains hard even for a family envoy.
    put16(target,0xBA,2104);
    expect(personnel_recruitment_reason(target,district,actor)==personnel::FamilyEnvoyException);
    expect(!filter());dispatch(false);
    // A ban against another ruler must not be confused with this district.
    put16(target,0xD0,2102);expect(filter());dispatch(true);
    put16(target,0xD0,0);
    for(unsigned loyalty:{99u,100u,101u,255u}) {
        target[0xE8]=static_cast<unsigned char>(loyalty);
        expect(filter()==(loyalty<100));dispatch(loyalty<100);
    }
    target[0xE8]=97;put16(target,0xC0,2102);put16(target,0xBA,0);
    expect(filter());dispatch(true); // current enemy ruler friendship is caution
    for(unsigned timer:{0u,1u,12u,24u,255u}) {
        put16(target,0x162,2101);target[0x164]=static_cast<unsigned char>(timer);put16(target,0xBA,2104);
        expect(!filter());dispatch(false); // family cannot bypass a native ban
    }
    put16(target,0x162,2102);expect(filter());dispatch(true);
    // Ruler replacement changes the relevant ban without touching target data.
    put16(target,0x162,2101);put16(force,0x10,2102);expect(filter());dispatch(true);put16(force,0x10,2101);
    put16(target,0x162,6);expect(filter());dispatch(true); // force slot != ruler
    put16(target,0x162,0);target[0x164]=0;put16(target,0xBA,2105);
    expect(!filter());dispatch(false); // spouse is absent from recruiting force
    put16(target,0xBA,2104);expect(filter());dispatch(true);
    put16(target,0xBC,2110);put16(relative,0xBC,2110);put16(actor,0xBC,2110);family={relative};
    put16(actor,0x15E,1);expect(!filter());put16(actor,0x15E,0);
    owners[actor]=other_force;expect(!filter());owners[actor]=force;
    ai_reject_family=true;expect(!filter());ai_reject_family=false;
    put16(target,0xBA,0);put16(actor,0xBC,0);
    expect(!filter());dispatch(false);
    put16(actor,0xBC,2110);expect(filter());dispatch(true);
    put16(target,0xBC,0);put16(actor,0xBC,0);family.clear();
    // Missing context is rejected before native allocation; turning the mod
    // off returns control to the original AI command and target list.
    slot(0xE0D0,5,nullptr);dispatch(false);slot(0xE0D0,5,district);
    put16(target,0xD0,2101);enabled=0;expect(filter());dispatch(true);enabled=1;
    // The shared player/manual dispatch is never hooked by this feature.
    put16(actor,0x15E,0);district[0x14]=5;ai_player_sent=0;
    PersonnelEmployCommand manual={target,actor,0,0};
    fn<void(__fastcall *)(PersonnelEmployCommand *,int)>(0x1ED920)(&manual,1);
    expect(ai_player_sent==1 && district[0x14]==4);
    // On another computer force, only hatred/ban for that current ruler matters.
    district[0x10]=7;owners[actor]=other_force;put16(actor,0x15E,0);expect(filter());dispatch(true);
    put16(target,0xD0,2102);expect(!filter());dispatch(false);
    put16(target,0xD0,0);put16(target,0x162,2102);expect(!filter());dispatch(false);
    list_free(candidates);envoys.clear();
    native_functions[0x2CC50]=old_command;native_functions[0x1ED920]=old_issue;native_functions[0x1EFE60]=old_check;
    slot(0xE0D0,5,nullptr);slot(0xDF30,6,nullptr);slot(0xDF30,7,nullptr);
    for(Obj p:{ruler,other_ruler,target,actor,relative,allowed}) {slot(0x158,word(p,0x10),nullptr);owners.erase(p);free(p);}
    free(district);free(force);free(other_force);free(node);
}
static void test_recruit_ai_publication() {
    for(const auto &link:recruit_ai_links) {
        putptr(image,link.slot,image+link.original);memcpy(image+link.original,link.entry,16);
    }
    expect(recruit_ai_guards_match());
    for(const auto &link:recruit_ai_links) {
        image[link.original]^=1;expect(!recruit_ai_guards_match());image[link.original]^=1;
        putptr(image,link.slot,image);expect(!recruit_ai_guards_match());putptr(image,link.slot,image+link.original);
    }
    expect(publish_recruit_ai());expect(!recruit_ai_guards_match());
    for(const auto &link:recruit_ai_links)expect(pointer(image,link.slot)==reinterpret_cast<Obj>(link.replacement));
    restore_recruit_ai();expect(recruit_ai_guards_match());
    // A concurrent/foreign second slot rejects publication and rollback
    // restores our first slot without overwriting the foreign pointer.
    putptr(image,recruit_ai_links[1].slot,image);
    expect(!publish_recruit_ai());restore_recruit_ai();
    expect(pointer(image,recruit_ai_links[0].slot)==image+recruit_ai_links[0].original);
    expect(pointer(image,recruit_ai_links[1].slot)==image);
    putptr(image,recruit_ai_links[1].slot,image+recruit_ai_links[1].original);
    expect(recruit_ai_guards_match());
}
#undef expect
