// Private native objects and executable pages; never attaches to the game.
#define AWARD_WAREHOUSE
#define MERIT_CANDIDATE
#define RECRUITMENT_UI_CANDIDATE
#define main previous_personnel_fixture_main
#include "personnel_native_fixture.cpp"
#undef main
static void award_expect(bool ok,unsigned line) {
    ++checks;if(!ok){fprintf(stderr,"Awards fixture line %u, check %u\n",line,checks);exit(1);}
}
#define expect(x) award_expect((x),__LINE__)
static std::map<Obj,unsigned> forces,busy;
static std::map<Obj,Obj> armies,occupants;
static std::map<Obj,std::array<int,4>> abilities;
static bool transfer_success=true;
static unsigned transfers,post_calls;
static Obj resources,item_base;
static unsigned __fastcall a_force(Obj p){return forces[p];}
static Obj __fastcall a_army(Obj p){return armies[p];}
static int __fastcall a_busy(Obj p,int){return static_cast<int>(busy[p]);}
static int __fastcall a_idle(Obj){return 0;}
static unsigned __fastcall a_post_id(Obj p){return word(p,0x10)-1;}
static Obj __fastcall a_occupant(Obj p){return occupants[p];}
static int __fastcall a_transfer(Obj *p){++transfers;if(!transfer_success)return 0;put16(p[1],0x4A,word(p[0],0x10));return 1;}
static unsigned __fastcall a_list(Obj,GameList *){return 0;}
static void __fastcall a_people(Obj,GameList *){}
static void __fastcall a_batch(Obj){++post_calls;}
static int __fastcall a_select(Obj,Obj,Obj){++post_calls;return 1;}
static Obj picker_p,picker_q;
static bool picker_p_blocked,picker_q_blocked;
static Obj __fastcall a_picker(Obj state) {
    auto &blocked=*reinterpret_cast<GameList *>(state+0x498);
    expect(contains(blocked,picker_p)==picker_p_blocked);
    expect(contains(blocked,picker_q)==picker_q_blocked);
    return picker_p;
}
static int __fastcall a_pred(Obj it,Link *l){return word(l->value,0x4A)==dword(it,24);}
static void a_owned_iterator(Obj person,Obj out) {
    memset(out,0,32);put32(out,24,word(person,0x10));
    Link *l=list_first(*reinterpret_cast<GameList *>(root+0x108));
    while(l && !a_pred(out,l))l=l->next;putptr(out,8,l);
}
static Obj __fastcall a_resource(Obj){return resources;}
static Obj __fastcall a_force_resource(Obj force){expect(force==table(0xDF30,1,51));return resources;}
static Obj __fastcall a_force_object(Obj p){return table(0xDF30,a_force(p),51);}
static int __fastcall a_ability(Obj p,unsigned a,unsigned mask){expect(mask==0xFFD);return abilities[p][a];}
static Obj __fastcall a_control(Obj){return nullptr;}
static void __fastcall a_notice(unsigned,const wchar_t *){}
static Obj ui_award_person,ui_award_item,ui_award_district;
static void __fastcall a_ui(Obj) {
    expect(word(ui_award_item,0x4A)==0 && award_stored(ui_award_item));
    award_synchronize();expect(award_stored(ui_award_item));
    GameList candidates;list_init(candidates);award_recipients(ui_award_district,&candidates);
    expect(contains(candidates,ui_award_person));list_free(candidates);
    Obj pair[]={ui_award_person,ui_award_item};expect(award_transfer(pair));
}
static void __fastcall a_ui_pick_item(Obj state) {
    Obj layout=pointer(state,0x478);expect(word(ui_award_item,0x4A)!=0);
    putptr(layout,0x188,ui_award_item);putptr(layout,0x190,nullptr);
}
static void __fastcall a_ui_confirm(Obj) {
    expect(word(ui_award_item,0x4A)!=0);
    Obj pair[]={ui_award_person,ui_award_item};expect(award_transfer(pair));
}
static void __fastcall a_refresh(Obj layout){expect(pointer(layout,0x190)==ui_award_person);}
static Obj add_item(unsigned id,unsigned owner,unsigned ability,unsigned bonus,unsigned effect=0) {
    Obj p=item_base+id*128;put16(p,0x10,id+1);put16(p,0x4A,owner);
    p[0x45]=static_cast<unsigned char>(ability);p[0x46]=static_cast<unsigned char>(bonus);
    p[0x47]=static_cast<unsigned char>(effect);p[0x48]=effect?1:0;p[0x4F]=1;
    list_add(*reinterpret_cast<GameList *>(root+0x108),p);return p;
}
static Obj add_person(unsigned id,unsigned force,bool ruler=false) {
    Obj p=object(id);slot(0x158,id,p);forces[p]=force;p[0xE6]=ruler?1:3;p[0xE7]=73;return p;
}
static void test_rules() {
    for(unsigned a=1;a<=4;++a) {
        unsigned mask=awards::slots(a,3,0);
        expect(awards::may_award(true,mask,4,3));
        expect(!awards::may_award(true,mask,3,3));
        expect(!awards::may_award(true,mask,2,3));
        expect(awards::may_award(true,mask,1,0));
        expect(awards::may_award(false,mask,2,3));
    }
    expect(!awards::may_award(true,awards::slots(2,0,0),0,0));
    expect(!awards::may_award(true,awards::slots(5,3,0),3,0));
    expect(!awards::may_award(true,awards::slots(0,0,1),1,0));
    expect(!awards::may_award(true,awards::slots(0,0,2),1,0));
    expect(!awards::may_award(true,awards::slots(1,4,1),4,3));
    for(unsigned old:{0u,4000u,6000u,10000u})for(unsigned next:{0u,4000u,6000u,10000u}) {
        expect(awards::may_post(true,true,1,next,old)==(next>=old));
        expect(!awards::may_post(true,true,73,next,old));
        expect(awards::may_post(false,false,1,next,old));
    }
    awards::Inventory inv={},out={};std::array<uint8_t,32> hash={};inv[0]=10;inv[4095]=6000;
    auto bytes=awards::encode(inv,hash);expect(awards::decode(bytes.data(),bytes.size(),hash,out) && out==inv);
    out.fill(77);auto before=out;hash[1]=1;expect(!awards::decode(bytes.data(),bytes.size(),hash,out) && out==before);hash[1]=0;
    expect(!awards::decode(bytes.data(),bytes.size()-1,hash,out));bytes[40]=0xFF;bytes[41]=0xFF;
    expect(!awards::decode(bytes.data(),bytes.size(),hash,out) && out==before);
}
static void test_warehouse(Obj ruler,Obj p,Obj q,Obj district) {
    Obj war3=add_item(0,word(p,0x10),2,3),war2=add_item(1,word(p,0x10),2,2);
    Obj war4=add_item(2,word(ruler,0x10),2,4),intel=add_item(3,word(ruler,0x10),3,1);
    Obj life=add_item(4,word(ruler,0x10),0,0,2),loyal=add_item(5,word(p,0x10),0,0);
    Obj loyal2=add_item(6,word(p,0x10),0,0);
    award_reset();award_synchronize();expect(!award_stored(war3) && award_stored(war2));
    expect(word(war2,0x4A)==word(ruler,0x10) && award_stored(war4) && award_stored(life));
    expect(!award_stored(loyal) && !award_stored(loyal2));
    unsigned syncs=award_full_sync_runs;
    for(unsigned i=0;i<1000;++i)award_synchronize();
    expect(award_full_sync_runs==syncs); // Repeated effect reads do not sort again.
    // Native mutations bypass mod write callbacks and must invalidate at once.
    p[0xE6]=6;award_synchronize();expect(award_full_sync_runs==++syncs);
    p[0xE6]=3;award_synchronize();expect(award_full_sync_runs==++syncs);
    forces[p]=2;award_synchronize();expect(award_full_sync_runs==++syncs);
    forces[p]=1;award_synchronize();expect(award_full_sync_runs==++syncs);
    Obj force=table(0xDF30,1,51);unsigned oldruler=word(force,0x10);
    put16(force,0x10,word(q,0x10));award_synchronize();expect(award_full_sync_runs==++syncs);
    put16(force,0x10,oldruler);award_synchronize();expect(award_full_sync_runs==++syncs);
    // Restore the initial logical storage after this deliberate succession.
    award_reset();award_synchronize();syncs=award_full_sync_runs;
    war3[0x48]=1;award_synchronize();expect(award_full_sync_runs==++syncs);
    war3[0x48]=0;award_synchronize();expect(award_full_sync_runs==++syncs);
    award_selected_item=war2;award_synchronize();expect(award_full_sync_runs==++syncs);
    award_selected_item=nullptr;award_synchronize();expect(award_full_sync_runs==++syncs);
    award_synchronize();expect(award_full_sync_runs==syncs);
    unsigned char scoped[32]={};a_owned_iterator(p,scoped);award_synchronize_person(p,scoped);
    syncs=award_full_sync_runs;
    for(unsigned i=0;i<1000;++i) {a_owned_iterator(p,scoped);award_synchronize_person(p,scoped);}
    expect(award_full_sync_runs==syncs);
    // A native acquisition is detected by the native iterator without scanning
    // unrelated equipment or depending on a mod transfer callback.
    put16(war4,0x4A,word(p,0x10));a_owned_iterator(p,scoped);award_synchronize_person(p,scoped);
    expect(!award_stored(war4) && award_stored(war3) && word(war3,0x4A)==word(ruler,0x10));
    expect(award_full_sync_runs==syncs+1);
    Obj empty=add_person(700,1);a_owned_iterator(empty,scoped);syncs=award_full_sync_runs;
    for(unsigned i=0;i<1000;++i)award_synchronize_person(empty,scoped);
    expect(award_full_sync_runs==syncs);
    put16(war3,0x4A,word(p,0x10));put16(war2,0x4A,word(ruler,0x10));
    put16(war4,0x4A,word(ruler,0x10));
    award_reset();award_synchronize();
    busy[p]=1;expect(!award_allowed(p,war2) && award_allowed(p,war4) && award_allowed(p,intel) && !award_allowed(p,life));
    Obj pair[]={p,war4};transfers=0;transfer_success=false;
    expect(!award_transfer(pair) && word(war3,0x4A)==word(p,0x10) && !award_stored(war3));
    transfer_success=true;expect(award_transfer(pair) && award_stored(war3) && !award_stored(war4));
    pair[1]=war2;expect(!award_transfer(pair));busy[p]=0;
    expect(award_transfer(pair) && award_stored(war4) && !award_stored(war2));
    pair[0]=ruler;expect(award_transfer(pair) && award_stored(war2));
    // Award a stored item to the ruler, rather than interpreting it as recall.
    pair[1]=war4;expect(award_transfer(pair) && !award_stored(war4));
    GameList list;list_init(list);list_add(list,war4);award_warehouse_items(district,&list);
    expect(contains(list,war4) && contains(list,war2) && contains(list,intel));list_free(list);
    // Predicate handles both current item and native subsequent iteration.
    unsigned char it[32]={};put32(it,24,word(ruler,0x10));Link second{war4,nullptr},first{war2,&second};
    putptr(it,8,&first);award_filter_iterator(it,false);expect(pointer(it,8)==reinterpret_cast<Obj>(&second));
    expect(award_equipment_predicate(it,&second) && !award_equipment_predicate(it,&first));
    putptr(it,8,&first);award_filter_iterator(it,true);expect(pointer(it,8)==reinterpret_cast<Obj>(&first));
    expect(award_storage_predicate(it,&first) && !award_storage_predicate(it,&second));
    // Discovery and seizure native owner changes enter the receiving warehouse.
    put16(intel,0x4A,word(q,0x10));award_synchronize();expect(!award_stored(intel));
    put16(intel,0x4A,word(ruler,0x10));award_synchronize();expect(award_stored(intel));
    pair[0]=p;pair[1]=war3;expect(award_transfer(pair));
    busy[p]=1;award_selected_item=war2;list_init(list);award_recipients(district,&list);
    expect(!contains(list,p) && contains(list,q));list_free(list);award_selected_item=nullptr;busy[p]=0;
    put32(p,0x1E8,1);expect(award_active(p));put32(p,0x1E8,0);
    put16(p,0x15E,1);expect(award_active(p) && !award_allowed(p,war2) && award_allowed(p,intel));
    award_selected_item=intel;list_init(list);award_recipients(district,&list);expect(contains(list,p));
    list_free(list);award_selected_item=nullptr;put16(p,0x15E,0);
    // Full UI scope: temporary no-owner storage permits ruler selection,
    // transfer preserves the newly equipped marker after the callback.
    unsigned char state[0x500]={},layout[0x200]={};putptr(state,0x478,layout);putptr(layout,0x188,intel);put32(layout,0x180,1);
    original_award_ui=&a_ui;ui_award_person=ruler;ui_award_item=intel;ui_award_district=district;
    award_ui(state);expect(!award_stored(intel) && word(intel,0x4A)==word(ruler,0x10) && !award_selected_item);
    Obj intel2=add_item(7,word(ruler,0x10),3,2);award_synchronize();expect(award_stored(intel2));
    ui_award_item=intel2;putptr(layout,0x190,ruler);put32(layout,0x180,2);original_award_ui=&a_ui_pick_item;
    award_ui(state);expect(pointer(layout,0x190)==ruler && award_stored(intel2));
    put32(layout,0x180,3);original_award_ui=&a_ui_confirm;award_ui(state);
    expect(!award_stored(intel2) && award_stored(intel));
    // Exact-archive-bound extras survive same-world-address reloads.
    merit::SaveCoordinator::Completed job;job.name="awards_private_fixture.s14";job.binding.fill(0xA3);
    award_snapshot();expect(award_write_sidecar(job));award_put_storage(intel2,1);
    award_restore_sidecar(&job);expect(!award_stored(intel2) && award_stored(war2));
    wchar_t path[MAX_PATH];expect(award_save_path(job,path,false));expect(DeleteFileW(path)!=0);
    award_restore_sidecar(&job);expect(award_stored(intel2));
    intel2[0x4F]=0;award_synchronize();expect(!award_stored(intel2));
    intel2[0x4F]=1;award_synchronize();expect(award_stored(intel2));
}
static void test_posts(Obj p,Obj q) {
    // Execute the captured DB30 decoding instructions on our private page.
    const unsigned char decode[]={0x44,0xf,0xb7,0xd2,0xb9,0x10,0,0,0,0x41,0x2a,0xc8,0x41,0x8b,0xc2,0xd3,0xe8,0x41,0xf,0xb6,0xc8,0x41,0xd3,0xe2,0x41,0xb,0xc2,0x41,0xf,0xb7,0xc9,0x33,0xc1,0xc3};
    memcpy(image+0xDB30,decode,sizeof(decode));FlushInstructionCache(GetCurrentProcess(),image+0xDB30,sizeof(decode));
    void *vt[12]={};vt[11]=image+0xDB30;
    unsigned capacities[]={4000,6000,6000,8000,15000,7000,20000,20000,19000};
    for(unsigned i=0;i<9;++i) {
        Obj post=object(i+1);slot(0x784F8,i,post);putptr(post,0,vt);
        Obj vector=static_cast<Obj>(calloc(1,20));unsigned raw=capacities[i]^i;
        put32(vector,0,((raw>>13)|(raw<<3))&65535);
        putptr(post,0x40,vector);putptr(post,0x48,vector+20);put16(post,0x34,static_cast<uint16_t>(capacities[i]));
        expect(award_post_capacity(i)==capacities[i]);
    }
    Obj fallback=object(10);slot(0x784F8,9,fallback);put16(fallback,0x34,20000);expect(award_post_capacity(9)==20000);
    expect(award_post_capacity(1)==6000);p[0xE7]=1;busy[p]=1;
    expect(!award_post_select(nullptr,p,table(0x784F8,0,255)));
    expect(award_post_select(nullptr,p,table(0x784F8,2,255))==1);
    armies[q]=object(1);q[0xE7]=3;occupants[table(0x784F8,3,255)]=q;
    expect(!award_post_select(nullptr,p,table(0x784F8,3,255)));
    unsigned char map[24]={},head[48]={},node[48]={};putptr(map,8,head);putptr(head,8,node);head[0x19]=1;
    putptr(node,0x20,p);put32(node,0x28,0);award_post_batch(map);expect(dword(node,0x28)==1);
    armies[p]=object(2);put32(node,0x28,73);award_post_batch(map);expect(dword(node,0x28)==1);
    busy[p]=0;armies[p]=nullptr;put32(node,0x28,0);award_post_batch(map);expect(dword(node,0x28)==0);
    put32(node,0x28,3);award_post_batch(map);expect(dword(node,0x28)==1);
    armies[q]=nullptr;occupants.clear();
    Obj state=static_cast<Obj>(calloc(1,0x500)),layout=object(1);
    putptr(state,0x4C0,layout);putptr(state,0x470,table(0xDF30,1,51));
    auto &blocked=*reinterpret_cast<GameList *>(state+0x498);list_init(blocked);list_add(blocked,p);list_add(blocked,q);
    GameList saved=blocked;original_post_picker=&a_picker;picker_p=p;picker_q=q;
    p[0xE6]=q[0xE6]=4;p[0x1BC]=q[0x1BC]=3;armies[p]=object(1);armies[q]=object(1);p[0xE7]=4;q[0xE7]=5;
    auto pick=[&](unsigned id,bool bp,bool bq) {
        putptr(layout,0x1B8,table(0x784F8,id,255));picker_p_blocked=bp;picker_q_blocked=bq;
        expect(award_post_picker(state)==p);
        expect(blocked.handle==saved.handle && contains(blocked,p) && contains(blocked,q));
    };
    pick(6,false,false); // Reported case: deployed 15000 / 7000 -> 20000.
    p[0xE7]=6;q[0xE7]=7;pick(6,false,false); // Equal capacity.
    pick(8,true,true); // 20000 -> 19000 stays disabled.
    q[0xE7]=5;pick(8,true,false); // Each officer is compared independently.
    occupants[table(0x784F8,6,255)]=q;pick(6,true,false);occupants.clear();
    armies[p]=nullptr;busy[p]=0;pick(6,true,false); // Preserve idle native exclusion.
    busy[p]=1;pick(6,false,false); // Domestic/trade activity as well as deployment.
    p[0x1BC]=1;pick(6,true,false);p[0x1BC]=3;
    forces[p]=2;pick(6,true,false);forces[p]=1;
    enabled=0;pick(6,true,true);enabled=1;
    busy[p]=0;armies[p]=armies[q]=nullptr;p[0xE7]=q[0xE7]=73;
    list_free(blocked);free(state);free(layout);
    for(unsigned i=0;i<9;++i)putptr(table(0x784F8,i,255),0,nullptr);
}
static void test_ai(Obj ruler,Obj p,Obj q,Obj district) {
    auto &list=*reinterpret_cast<GameList *>(root+0x108);list_free(list);list_init(list);memset(item_base,0,128*32);
    Obj lead=add_item(0,word(ruler,0x10),1,5),war=add_item(1,word(ruler,0x10),2,5);
    Obj intel=add_item(2,word(ruler,0x10),3,4),pol=add_item(3,word(ruler,0x10),4,3);
    Obj horse=add_item(4,word(ruler,0x10),0,0,1),weak=add_item(5,word(ruler,0x10),2,2);
    abilities[ruler]={50,50,50,50};abilities[p]={95,70,99,60};abilities[q]={70,98,60,99};
    Obj context=object(1);putptr(root,0x871E8,context);Obj city=object(1);batch_district=district;
    void *vt[17]={};vt[16]=reinterpret_cast<void *>(&mock_order_district);putptr(city,0,vt);
    award_reset();transfers=0;award_ai_city(city);
    expect(word(lead,0x4A)==word(p,0x10) && word(war,0x4A)==word(q,0x10));
    expect(word(intel,0x4A)==word(p,0x10) && word(pol,0x4A)==word(q,0x10));
    expect(word(horse,0x4A)==word(p,0x10) && word(weak,0x4A)==word(p,0x10));
    unsigned calls=transfers;award_ai_city(city);expect(transfers==calls);
    unsigned char old_p_district=p[0xE0];p[0xE0]=2;Obj other_district=object(2);slot(0xE0D0,2,other_district);
    other_district[0x14]=0;busy[p]=1;armies[p]=object(1);
    Obj better=add_item(6,word(ruler,0x10),1,8);award_ai_days[1]=-1;award_ai_city(city);
    expect(word(better,0x4A)==word(p,0x10) && word(lead,0x4A)!=word(p,0x10));
    Obj lesser=add_item(7,word(ruler,0x10),1,2);award_ai_days[1]=-1;award_ai_city(city);
    expect(word(better,0x4A)==word(p,0x10) && word(lesser,0x4A)!=word(p,0x10));
    Obj late=add_item(8,word(ruler,0x10),3,8);busy[p]=1;armies[p]=nullptr;
    resources[0x14]=0;award_ai_days[1]=-1;award_ai_city(city);expect(award_stored(late));
    resources[0x14]=200;award_ai_days[1]=-1;award_ai_city(city);expect(word(late,0x4A)==word(p,0x10));
    p[0xE0]=old_p_district;busy[p]=0;armies[p]=nullptr;slot(0xE0D0,2,nullptr);
    free(city);free(context);putptr(root,0x871E8,nullptr);
}
static void test_hooks() {
    for(const auto &entry:award_entries)memcpy(image+entry.rva,entry.bytes,16);
    const unsigned char ai[]={0x48,0x89,0x5C,0x24,0x10,0x57,0x48,0x83,0xEC,0x30,0x33,0xC0,0x48,0x8B,0xF9,0x48};
    memcpy(image+0x535E0,ai,16);putptr(image,0x13491B0,image+0x535E0);putptr(image,0x134C6E8,image+0x333320);
    Obj relay=image+0x2000000;AwardCode code={};expect(prepare_awards(code,relay));
    expect(publish_awards(code));restore_awards(code);expect(award_guards_match());
    for(unsigned i=0;i<sizeof(award_entries)/sizeof(award_entries[0]);++i) {
        expect(prepare_awards(code,relay));image[award_entries[i].rva]^=1;
        expect(!award_guards_match() && !publish_awards(code));restore_awards(code);
        image[award_entries[i].rva]^=1;expect(award_guards_match());
    }
    expect(prepare_awards(code,relay));putptr(image,0x13491B0,nullptr);
    expect(!publish_awards(code));restore_awards(code);putptr(image,0x13491B0,image+0x535E0);expect(award_guards_match());
    // Execute the actual transfer trampoline's captured prologue on a private
    // page, with a balanced synthetic continuation returning the argument.
    Obj at=image+0x1F0300;const unsigned char tail[]={0x48,0x8B,0xC1,0x48,0x83,0xC4,0x20,0x5B,0xC3};
    memcpy(at+6,tail,sizeof(tail));FlushInstructionCache(GetCurrentProcess(),image,0x2800000);
    auto gate=reinterpret_cast<Obj(__fastcall *)(Obj)>(relay+1728+32);
    Obj token=root;expect(gate(token)==token);memcpy(at,award_entries[0].bytes,16);
}
int main() {
    image=static_cast<Obj>(VirtualAlloc(nullptr,0x2800000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    root=static_cast<Obj>(calloc(1,0x90000));putptr(image,0x277DF18,root);putptr(image,0x27D10E8,root);
    putptr(image,0x27D10F0,heads);put32(image,0x27D1120,4096);enabled=1;
    native_functions={{0x32F500,reinterpret_cast<void *>(&mock_valid)},{0x21E2B0,reinterpret_cast<void *>(&mock_list_init)},
        {0x18DF0,reinterpret_cast<void *>(&mock_list_add)},{0x335060,reinterpret_cast<void *>(&mock_remove)},
        {0x424D60,reinterpret_cast<void *>(&mock_list_free)},{0x22CE20,reinterpret_cast<void *>(&a_force)},
        {0x22AA70,reinterpret_cast<void *>(&a_army)},{0x233110,reinterpret_cast<void *>(&a_busy)},
        {0x232240,reinterpret_cast<void *>(&a_idle)},{0x22DF80,reinterpret_cast<void *>(&a_post_id)},
        {0x6DC0A0,reinterpret_cast<void *>(&a_occupant)},{0x333320,reinterpret_cast<void *>(&a_pred)},
        {0x40160,reinterpret_cast<void *>(&a_resource)},{0x22CA00,reinterpret_cast<void *>(&a_force_object)},{0x22E7E0,reinterpret_cast<void *>(&a_force_resource)},{0x6D3670,reinterpret_cast<void *>(&mock_cost)},
        {0x22FCE0,reinterpret_cast<void *>(&a_ability)},{0x32E8D0,reinterpret_cast<void *>(&a_control)},
        {0x1F09E0,reinterpret_cast<void *>(&a_notice)}, {0x32BA50,reinterpret_cast<void *>(&a_idle)},
        {0x79DFB0,reinterpret_cast<void *>(&a_refresh)}};
    original_award_transfer=&a_transfer;original_award_items=&a_list;original_award_recipients=&a_list;
    original_post_people=&a_people;original_post_batch=&a_batch;original_post_select=&a_select;
    Obj force=object(10);slot(0xDF30,1,force);Obj ruler=add_person(10,1,true),p=add_person(20,1),q=add_person(30,1);
    Obj district=object(1);district[0x10]=1;slot(0xE0D0,1,district);resources=object(1);resources[0x14]=200;
    item_base=static_cast<Obj>(calloc(32,128));putptr(root,0x77B18,item_base);list_init(*reinterpret_cast<GameList *>(root+0x108));
    test_rules();test_warehouse(ruler,p,q,district);test_posts(p,q);test_ai(ruler,p,q,district);test_hooks();
    printf("{\"status\":\"awards_native_fixture_passed\",\"checks\":%u,\"gameplay_verified\":false,\"installed\":false}\n",checks);
    return 0;
}
