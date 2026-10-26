static unsigned injury_applies,injury_clears;
static int injury_capacity=3;
static Obj injury_controlled,injury_district,injury_selected_advisor;
static Obj injury_player;
static Obj __fastcall mock_injury_player(Obj) {return injury_player;}
static int __fastcall mock_injury_capacity(Obj,unsigned category) {expect(category==4);return injury_capacity;}
static Obj __fastcall mock_injury_controlled(Obj) {return injury_controlled;}
static Obj __fastcall mock_injury_city_district(Obj) {return injury_district;}
static int __fastcall mock_injury_clear(Obj p,int reset,int retain) {
    expect(reset==1 && retain==0);put32(p,0x1E8,0);++injury_clears;return 1;
}
static void __fastcall mock_injury_apply(Obj force,unsigned category,GameList *list,int notifications) {
    expect(category==4 && notifications==0);++injury_applies;
    unsigned i=0;for(Link *l=list_first(*list);l;l=l->next,++i)put16(force,0xCC+i*2,word(l->value,0x10));
    for(;i<7;++i)put16(force,0xCC+i*2,0);
}
static unsigned __fastcall mock_injury_rumor(Obj,GameList *list,int) {
    for(Obj p:envoys)list_add(*list,p);return static_cast<unsigned>(envoys.size());
}
static Obj __fastcall mock_injury_selection(Obj,Obj,GameList *) {return injury_selected_advisor;}
static void test_injury() {
    native_functions[0x22D2A0]=reinterpret_cast<void *>(&mock_injury_capacity);
    native_functions[0x32E8D0]=reinterpret_cast<void *>(&mock_injury_controlled);
    native_functions[0x29BF00]=reinterpret_cast<void *>(&mock_injury_clear);
    native_functions[0x2ED6B0]=reinterpret_cast<void *>(&mock_injury_apply);
    native_functions[0x6DAF20]=reinterpret_cast<void *>(&mock_injury_rumor);
    native_functions[0x261B70]=reinterpret_cast<void *>(&mock_injury_selection);
    native_functions[0x32E910]=reinterpret_cast<void *>(&mock_injury_player);
    Obj force=object(2201),district=object(10),wounded=object(2202),herb=object(2203);
    Obj naval=object(2204),special=object(2205),ordinary=object(2206),city=object(11),node=object(1);
    district[0x10]=8;slot(0xDF30,8,force);slot(0xE0D0,10,district);
    Obj definition=object(41);definition[0xA0]=4;slot(0x78D50,41,definition);
    for(Obj p:{wounded,herb,naval,special,ordinary}) {
        p[0xE0]=10;p[0xE6]=4;slot(0x158,word(p,0x10),p);owners[p]=force;
    }
    herb[0x14D]=41;naval[0x14D]=39;special[0x14D]=50;ordinary[0x14D]=40;
    GameList actors;list_init(actors);
    put32(node,0x38,1);put32(node,0x3C,2);
    for(unsigned state:{3u,2u,1u,0u}) {
        wounded[0x161]=static_cast<unsigned char>(state);
        for(unsigned rva:{0x52600u,0x52780u,0x534A0u,0x53F40u})native_functions[rva]=reinterpret_cast<void *>(&mock_node);
        values={{1,ordinary},{2,wounded}};native_orders=0;
        for(Execute command:{&injury_dual_destruction,&injury_estrange,&injury_hidden_poison,&injury_spy_away}) {
            expect(command(node,node,node)==(state?2u:1u));
        }
        expect(native_orders==(state?0u:4u));
        envoys={wounded,herb,wounded};
        expect(injury_rumor_actors(district,&actors,0)==(state?1u:3u));
        expect(contains(actors,wounded)==(state==0));
        personnel_remove(actors,wounded);personnel_remove(actors,herb);
    }
    // Injury acquired after candidate selection is checked before native issue.
    wounded[0x161]=1;values={{1,wounded},{2,ordinary}};native_orders=0;
    expect(recruit_ai_command(node,node,node)==2 && native_orders==0);
    enabled=0;values={{1,ordinary},{2,wounded}};
    expect(injury_hidden_poison(node,node,node)==1 && native_orders==1);enabled=1;
    // Every nonzero state blocks creation before native mutates army/troop state.
    Obj army=object(1);put16(army,0x12,2202);list_add(actors,army);
    values={{1,reinterpret_cast<Obj>(&actors)}};native_functions[0x35A40]=reinterpret_cast<void *>(&mock_node);
    for(unsigned state:{1u,2u,3u,255u,0u}) {
        wounded[0x161]=static_cast<unsigned char>(state);native_orders=0;
        expect(injury_departure_create(node,node,node)==(state?2u:1u));
        expect(native_orders==(state?0u:1u));
    }
    personnel_remove(actors,army);free(army);
    wounded[0x161]=1;injury_capacity=3;
    put16(force,0xCC,2204);put16(force,0xCE,2205);put16(force,0xD0,2206);
    list_add(actors,naval);list_add(actors,special);list_add(actors,ordinary);
    expect(injury_plan_herb(force,actors,nullptr));
    expect(contains(actors,naval) && contains(actors,special) && contains(actors,herb) && !contains(actors,ordinary));
    expect(injury_list_size(actors)==3);
    personnel_remove(actors,herb);list_add(actors,ordinary);
    herb[0x161]=1;expect(!injury_plan_herb(force,actors,nullptr));
    expect(contains(actors,ordinary) && contains(actors,naval) && contains(actors,special));herb[0x161]=0;
    expect(!injury_plan_herb(force,actors,herb)); // never evict the chosen advisor
    injury_capacity=2;personnel_remove(actors,ordinary);
    expect(!injury_plan_herb(force,actors,nullptr));
    expect(contains(actors,naval) && contains(actors,special));injury_capacity=3;
    personnel_remove(actors,naval);personnel_remove(actors,special);list_add(actors,herb);list_add(actors,ordinary);
    injury_capacity=2;expect(!injury_plan_herb(force,actors,nullptr));
    expect(contains(actors,naval) && contains(actors,special) && !contains(actors,herb));
    injury_capacity=3;
    wounded[0x161]=0;expect(!injury_plan_herb(force,actors,nullptr));
    wounded[0x161]=2;
    // A native government replan cannot discard our protected old providers.
    personnel_remove(actors,naval);personnel_remove(actors,special);list_add(actors,ordinary);
    expect(injury_plan_herb(force,actors,nullptr));
    expect(contains(actors,naval) && contains(actors,special) && contains(actors,herb));
    for(Obj p:{naval,special,herb})personnel_remove(actors,p);
    GameList plans[5];for(auto &plan:plans)list_init(plan);injury_selected_advisor=special;
    expect(injury_government_selection(force,node,plans)==nullptr);
    expect(contains(plans[4],special) && contains(plans[4],naval) && contains(plans[4],herb));
    for(auto &plan:plans)list_free(plan);injury_selected_advisor=nullptr;
    injury_controlled=district;injury_district=district;
    Obj context=object(1);putptr(root,0x871E8,context);
    void *vtable[17]={};vtable[16]=reinterpret_cast<void *>(&mock_injury_city_district);putptr(city,0,vtable);
    put16(city,0xCA,2202);put16(city,0xCC,2202);put32(wounded,0x1E8,1);
    put16(force,0x196,2202);force[0x19C]=8;injury_applies=injury_clears=0;
    injury_synchronize_city(city);
    expect(word(city,0xCA)==2202 && word(city,0xCC)==2202 && dword(wounded,0x1E8)==1 && injury_applies==0);
    injury_controlled=nullptr;injury_player=force;injury_synchronize_city(city);
    // Player force: wounded assignments are still released, government untouched.
    expect(word(city,0xCA)==0 && word(city,0xCC)==0 && injury_clears==1 && word(force,0x196)==0);
    expect(injury_applies==0 && word(force,0xD0)==2206);
    expect(!injury_plan_herb(force,actors,nullptr));
    injury_player=nullptr;put16(city,0xCA,2202);put16(city,0xCC,2202);put32(wounded,0x1E8,1);
    put16(force,0x196,2202);force[0x19C]=8;injury_clears=0;
    injury_synchronize_city(city);
    expect(word(city,0xCA)==0 && word(city,0xCC)==0 && injury_clears==1 && word(force,0x196)==0);
    expect(word(force,0xCC)==2204 && word(force,0xCE)==2205 && word(force,0xD0)==2203 && injury_applies==1);
    injury_synchronize_city(city);expect(injury_applies==1 && injury_clears==1); // idempotent
    wounded[0x161]=0;injury_synchronize_city(city);expect(injury_applies==1); // native may replan once healed
    envoys.clear();list_free(actors);putptr(root,0x871E8,nullptr);
    for(Obj p:{wounded,herb,naval,special,ordinary}) {slot(0x158,word(p,0x10),nullptr);owners.erase(p);free(p);}
    slot(0xDF30,8,nullptr);slot(0xE0D0,10,nullptr);slot(0x78D50,41,nullptr);
    free(force);free(district);free(city);free(node);free(context);free(definition);
}
static Obj injury_bridge_actor;
static GameList *injury_bridge_list;
static unsigned __fastcall mock_injury_bridge(Obj actor,GameList *list,int sort) {
    expect(actor==injury_bridge_actor && list==injury_bridge_list && sort==7);return 123;
}
static void test_injury_bridge() {
    // Execute the actual replacement MOV/CALL/JMP sequence on a private page.
    Obj page=static_cast<Obj>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));expect(page!=nullptr);
    const unsigned char prologue[]={0x55,0x48,0x83,0xEC,0x70,0x48,0x8D,0x6C,0x24,0x60,0x48,0x89,0x0C,0x24};
    memcpy(page+0xC0,prologue,sizeof(prologue));memset(page+0xCE,0x90,0x1B);
    memcpy(page+0xE9,injury_rumor_call,9);
    const unsigned char epilogue[]={0x48,0x83,0xC4,0x70,0x5D,0xC3};memcpy(page+0xF2,epilogue,sizeof(epilogue));
    memcpy(page+512,injury_rumor_call,4);absolute_jump(page+516,reinterpret_cast<void *>(&mock_injury_bridge));
    Obj saved_image=image;image=page-0x2EC00;CodeLink link;
    expect(prepare_link(link,0x2ECE9,0xE8,page+512));link.modified[14]=0xEB;link.modified[15]=2;
    expect(publish_link(link));image=saved_image;
    DWORD old=0;expect(VirtualProtect(page,4096,PAGE_EXECUTE_READ,&old)!=0);FlushInstructionCache(GetCurrentProcess(),page,1024);
    injury_bridge_actor=reinterpret_cast<Obj>(0x12345678);injury_bridge_list=reinterpret_cast<GameList *>(0x34567890);
    expect(reinterpret_cast<unsigned(__fastcall *)(Obj,GameList *,int)>(page+0xC0)(injury_bridge_actor,injury_bridge_list,7)==123);
    image=page-0x2EC00;restore_link(link);image=saved_image;
    expect(!memcmp(page+0xE9,injury_rumor_call,9));VirtualFree(page,0,MEM_RELEASE);
}
static void test_injury_publication() {
    test_injury_bridge();
    for(const auto &l:injury_links) {putptr(image,l.slot,image+l.original);memcpy(image+l.original,l.bytes,16);}
    memcpy(image+0x53042,injury_government_call,5);memcpy(image+0x2ECE9,injury_rumor_call,9);
    memcpy(image+0x2ED6B0,injury_policy_apply_entry,16);memcpy(image+0x22D2A0,injury_policy_capacity_entry,16);
    memcpy(image+0x29BF00,injury_assignment_clear_entry,16);memcpy(image+0x261B70,injury_policy_selection_entry,16);
    expect(injury_guards_match());
    for(const auto &l:injury_links) {
        image[l.original]^=1;expect(!injury_guards_match());image[l.original]^=1;
        putptr(image,l.slot,image);expect(!injury_guards_match());putptr(image,l.slot,image+l.original);
    }
    Obj relay=allocate_near(image+0x53042);expect(relay!=nullptr);InjuryCode code;
    expect(prepare_injury(code,relay));expect(publish_injury(code,true,true));
    for(const auto &l:injury_links)expect(pointer(image,l.slot)==reinterpret_cast<Obj>(l.replacement));
    expect(image[0x53042]==0xE8 && image[0x2ECE9]==0xE8 && image[0x2ECEE]==0xEB);
    restore_injury(code);expect(injury_guards_match());
    expect(prepare_injury(code,relay));putptr(image,injury_links[2].slot,image);
    expect(!publish_injury(code,true,true));restore_injury(code);
    expect(pointer(image,injury_links[0].slot)==image+injury_links[0].original && pointer(image,injury_links[2].slot)==image);
    putptr(image,injury_links[2].slot,image+injury_links[2].original);
    expect(prepare_injury(code,relay));image[code.rumor.block]^=1;
    expect(!publish_injury(code,true,true));restore_injury(code);
    expect(!memcmp(image+0x53042,injury_government_call,5));image[code.rumor.block]^=1;
    expect(injury_guards_match());VirtualFree(relay,0,MEM_RELEASE);
}
