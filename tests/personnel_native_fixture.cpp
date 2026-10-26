/* Uses the real candidate adapter wrappers with simulated native objects.
 * Does not attach to the game or establish native ABI/gameplay correctness. */
#define TACTICS_FIXTURE
#define PERSONNEL_CANDIDATE
#define INJURY_PROTECTION
#include <map>
#include <vector>
#include <stdlib.h>
#include "../src/native_adapter.cpp"
static std::map<unsigned,void *> native_functions;
static void *fixture_function(unsigned rva) {
    auto at=native_functions.find(rva);
    if(at==native_functions.end()) {fprintf(stderr,"Missing native fixture %X\n",rva);abort();}
    return at->second;
}
static Obj root;
static Link *heads[4096];
static unsigned next_handle=1,native_orders,checks;
static std::map<unsigned,Obj> values;
static std::map<Obj,Obj> owners;
static std::vector<Obj> family,envoys;
static std::vector<std::pair<unsigned,unsigned>> batch_orders;
static std::map<std::pair<unsigned,unsigned>,bool> advice;
static Obj batch_city,batch_district;
static bool reject_batch;
static int batch_cost=1;
static void (*batch_issue_observer)(PersonnelEmployCommand *);
static void put16(Obj at,unsigned off,unsigned value) {*reinterpret_cast<unsigned short *>(at+off)=static_cast<unsigned short>(value);}
static void put32(Obj at,unsigned off,unsigned value) {*reinterpret_cast<unsigned *>(at+off)=value;}
static void putptr(Obj at,unsigned off,void *value) {*reinterpret_cast<void **>(at+off)=value;}
static Obj object(unsigned id) {Obj at=static_cast<Obj>(calloc(1,0x400));put16(at,0x10,id);return at;}
static void slot(unsigned off,unsigned id,Obj value) {putptr(root,off+id*8,value);}
static int __fastcall mock_valid(Obj obj) {return obj && word(obj,0x10);}
static Obj __fastcall mock_owner(Obj person) {return owners[person];}
static Obj __fastcall mock_store(Obj obj) {return obj;}
static unsigned char __fastcall mock_object(Obj,unsigned key,Obj *out,int) {
    auto at=values.find(key);if(at==values.end())return 0;*out=at->second;return 1;
}
static void __fastcall mock_list_init(GameList *list) {list->handle=new unsigned(next_handle++);}
static Obj __fastcall mock_list_add(Obj,unsigned handle,unsigned char) {
    auto *entry=new Link{nullptr,nullptr};Link **tail=&heads[handle];
    while(*tail)tail=&(*tail)->next;*tail=entry;return reinterpret_cast<Obj>(entry);
}
static void __fastcall mock_remove(GameList *list,Obj *person) {
    Link **current=&heads[*list->handle];
    while(*current) {
        Link *link=*current;
        if(link->value==*person) {*current=link->next;delete link;}
        else current=&link->next;
    }
}
static void __fastcall mock_list_free(GameList *list) {
    Link *link=heads[*list->handle];
    while(link) {Link *next=link->next;delete link;link=next;}
    heads[*list->handle]=nullptr;delete list->handle;list->handle=nullptr;
}
static unsigned __fastcall mock_family(Obj target,GameList *list) {
    unsigned count=0;
    for(Obj person:family)if(person!=target && word(target,0xBC)!=0 &&
        word(target,0xBC)==word(person,0xBC)) {list_add(*list,person);++count;}
    return count;
}
static unsigned __fastcall mock_envoys(Obj,GameList *list) {
    for(Obj person:envoys)list_add(*list,person);return static_cast<unsigned>(envoys.size());
}
static int __fastcall mock_dislikes(Obj target,Obj ruler) {
    for(unsigned i=0;i<8;++i)if(word(target,0xD0+i*2)==word(ruler,0x10))return 1;return 0;
}
static int __fastcall mock_likes(Obj target,Obj ruler) {
    for(unsigned i=0;i<8;++i)if(word(target,0xC0+i*2)==word(ruler,0x10))return 1;return 0;
}
static uint64_t __fastcall mock_node(Obj,Obj,Obj) {++native_orders;return 1;}
static Obj __fastcall mock_city(Obj,unsigned) {return batch_city;}
static Obj __fastcall mock_order_district(Obj) {return batch_district;}
static int __fastcall mock_cost() {return batch_cost;}
static int __fastcall mock_player(Obj) {return 1;}
static int __fastcall mock_command_valid(PersonnelEmployCommand *c) {
    return valid(c->target) && valid(c->actor) && !(word(c->actor,0x15E)&1) && c->mode==0;
}
static Obj __fastcall mock_advice(Obj,int type,unsigned *recommended,int *score,unsigned *,
                                int actor,int target,int,Obj,int) {
    if(type!=2)abort();*recommended=advice[{static_cast<unsigned>(target),static_cast<unsigned>(actor)}]?1:0;
    *score=-1;return nullptr;
}
static void __fastcall mock_issue(PersonnelEmployCommand *c,int player) {
    if(player!=1 || !mock_command_valid(c) || batch_cost<0 || batch_district[0x14]<batch_cost)abort();
    if(reject_batch)return;
    put16(c->actor,0x15E,word(c->actor,0x15E)|1);
    batch_district[0x14]=static_cast<unsigned char>(batch_district[0x14]-batch_cost);
    batch_orders.emplace_back(word(c->target,0x10),word(c->actor,0x10));
    if(batch_issue_observer)batch_issue_observer(c);
}
static void expect(bool ok) {++checks;if(!ok) {fprintf(stderr,"Personnel native fixture failed at %u\n",checks);exit(1);}}
static bool contains(GameList &list,Obj person) {
    for(Link *link=list_first(list);link;link=link->next)if(link->value==person)return true;return false;
}
static void test_auto_employ(Obj district,Obj force) {
    batch_city=object(1);batch_district=district;
    void *city_vtable[17]={};city_vtable[16]=reinterpret_cast<void *>(&mock_order_district);
    putptr(batch_city,0,city_vtable);
    native_functions[0x32C130]=reinterpret_cast<void *>(&mock_city);
    native_functions[0x6D3660]=reinterpret_cast<void *>(&mock_cost);
    native_functions[0x233960]=reinterpret_cast<void *>(&mock_player);
    native_functions[0x1EFE60]=reinterpret_cast<void *>(&mock_command_valid);
    native_functions[0x2F36B0]=reinterpret_cast<void *>(&mock_advice);
    native_functions[0x1ED920]=reinterpret_cast<void *>(&mock_issue);
    Obj a=object(201),b=object(202),c=object(203),d=object(204);
    Obj first=object(301),second=object(302),third=object(303),fourth=object(304);
    for(Obj person:{a,b,c,d,first,second,third,fourth})slot(0x158,word(person,0x10),person);
    for(Obj person:{a,b,c,d}) {owners[person]=force;person[0xE0]=1;}
    envoys={a,b,c,d};advice={{{301,202},true},{{302,202},true},{{302,203},true},{{303,204},true}};
    GameList selected;list_init(selected);for(Obj target:{first,second,third,fourth})list_add(selected,target);
    district[0x14]=4;batch_orders.clear();reject_batch=false;
    auto result=personnel_auto_employ(district,selected);
    // User decision 2026-10-25: fourth has no recommendation; no fallback
    // dispatch or extra order deduction is allowed.
    expect(result.sent==3 && result.advised_against==1 && result.failed==0 && batch_orders.size()==3);
    expect(batch_orders[0]==std::make_pair(301u,202u));
    expect(batch_orders[1]==std::make_pair(302u,203u));
    expect(batch_orders[2]==std::make_pair(303u,204u));
    expect(district[0x14]==1 && !(word(a,0x15E)&1));
    // User decision 2026-10-25: reserve the native screen actors and their
    // cost before sending extras. The last otherwise eligible target must
    // report no_orders, without consuming the native screen's order budget.
    for(Obj actor:envoys)put16(actor,0x15E,0);
    Obj excluded[]={b};district[0x14]=3;batch_orders.clear();
    result=personnel_auto_employ(district,selected,excluded,1,1,district);
    expect(result.sent==2 && result.no_orders==1 && result.advised_against==1 && batch_orders.size()==2);
    expect(batch_orders[0]==std::make_pair(302u,203u) && batch_orders[1]==std::make_pair(303u,204u));
    expect(!(word(b,0x15E)&1) && district[0x14]==1);
    // Reserving more than available cannot underflow; another district's
    // reservation must not restrict an actor's actual issuing district.
    for(Obj actor:envoys)put16(actor,0x15E,0);
    batch_orders.clear();district[0x14]=1;
    result=personnel_auto_employ(district,selected,nullptr,0,2,district);
    expect(result.sent==0 && result.no_orders==4 && batch_orders.empty() && district[0x14]==1);
    Obj other_district=object(50);other_district[0x14]=4;
    result=personnel_auto_employ(district,selected,nullptr,0,4,other_district);
    expect(result.sent==1 && result.no_orders==3 && district[0x14]==0 && other_district[0x14]==4);
    free(other_district);
    // Non-unit cost leaves cost x two native actors' orders intact.
    for(Obj actor:envoys)put16(actor,0x15E,0);
    batch_cost=2;district[0x14]=6;batch_orders.clear();
    result=personnel_auto_employ(district,selected,nullptr,0,4,district);
    expect(result.sent==1 && result.no_orders==3 && district[0x14]==4);
    batch_cost=1;
    for(Obj actor:envoys)put16(actor,0x15E,0);
    district[0x14]=2;batch_orders.clear();result=personnel_auto_employ(district,selected);
    expect(result.sent==2 && result.no_orders==2 && result.advised_against==0 && batch_orders.size()==2 && district[0x14]==0);
    for(Obj actor:envoys)put16(actor,0x15E,0);
    // User decision 2026-10-25: no recommended actor means no dispatch.
    GameList single;list_init(single);list_add(single,first);
    advice.clear();envoys={a,b};district[0x14]=4;batch_orders.clear();
    result=personnel_auto_employ(district,single);
    expect(result.sent==0 && result.advised_against==1 && batch_orders.empty() && district[0x14]==4);
    expect(result.blocked==0 && result.no_actor==0 && result.no_orders==0);
    // One recommendation must select that actor, past a non-recommended one.
    advice={{{301,202},true}};result=personnel_auto_employ(district,single);
    expect(result.sent==1 && result.advised_against==0 && batch_orders.size()==1);
    expect(batch_orders[0]==std::make_pair(301u,202u) && district[0x14]==3);
    put16(b,0x15E,0);
    // Of two targets only one has advice: exactly one order is consumed.
    GameList pair;list_init(pair);list_add(pair,first);list_add(pair,second);
    district[0x14]=4;batch_orders.clear();result=personnel_auto_employ(district,pair);
    expect(result.sent==1 && result.advised_against==1 && batch_orders.size()==1);
    expect(batch_orders[0]==std::make_pair(301u,202u) && district[0x14]==3);
    put16(b,0x15E,0);list_free(single);list_free(pair);
    // Duplicate target and duplicate actor entries issue only one order.
    put16(a,0x15E,0);envoys={a,a};advice={{{301,201},true}};GameList duplicated;list_init(duplicated);
    list_add(duplicated,first);list_add(duplicated,first);batch_orders.clear();district[0x14]=4;
    result=personnel_auto_employ(district,duplicated);
    expect(result.sent==1 && result.duplicates==1 && batch_orders.size()==1);
    // Native rejection is counted as failure, with no invented success.
    put16(a,0x15E,0);reject_batch=true;batch_orders.clear();district[0x14]=4;
    result=personnel_auto_employ(district,selected);
    expect(result.sent==0 && result.failed==1 && result.advised_against==0 && result.no_actor==3 && batch_orders.empty());
    reject_batch=false;
    a[0x161]=1;result=personnel_auto_employ(district,duplicated);
    expect(result.sent==0 && result.no_actor==1 && result.advised_against==0);
    a[0x161]=0;put16(first,0xD0,word(force,0x10));
    result=personnel_auto_employ(district,duplicated);
    expect(result.blocked==1 && result.sent==0 && result.advised_against==0);
    put16(first,0xBA,201);result=personnel_auto_employ(district,duplicated);
    expect(result.sent==1 && result.blocked==0);
    // Below-100 ruler friendship is caution, so the player batch can still
    // dispatch an available actor and leaves the final chance to native code.
    Obj enemy=object(305),enemy_ruler=object(306);slot(0x158,306,enemy_ruler);
    owners[first]=enemy;put16(enemy,0x10,306);put16(first,0xC0,306);
    put16(first,0xBA,0);put16(first,0xD0,0);first[0xE8]=97;
    put16(a,0x15E,0);district[0x14]=4;batch_orders.clear();
    result=personnel_auto_employ(district,duplicated);
    expect(result.sent==1 && result.blocked==0 && batch_orders.size()==1);
    first[0xE8]=100;put16(a,0x15E,0);batch_orders.clear();
    result=personnel_auto_employ(district,duplicated);
    expect(result.sent==0 && result.blocked==1 && batch_orders.empty());
    first[0xE8]=97;put16(first,0xBA,201);put16(first,0x162,word(force,0x10));
    for(unsigned timer:{0u,1u,12u,24u,255u}) {
        first[0x164]=static_cast<unsigned char>(timer);unsigned budget=district[0x14];
        result=personnel_auto_employ(district,duplicated);
        expect(result.sent==0 && result.blocked==1 && batch_orders.empty() && district[0x14]==budget);
    }
    put16(first,0x162,306);result=personnel_auto_employ(district,duplicated);
    expect(result.sent==1 && result.blocked==0);
    put16(first,0x162,0);first[0x164]=0;owners[first]=nullptr;
    put16(first,0xC0,0);put16(first,0xBA,0);free(enemy);free(enemy_ruler);slot(0x158,306,nullptr);
    // Mod off and missing district issue no native order.
    batch_orders.clear();enabled=0;result=personnel_auto_employ(district,selected);
    expect(result.failed==1 && batch_orders.empty());enabled=1;
    result=personnel_auto_employ(nullptr,selected);expect(result.failed==1 && batch_orders.empty());
    list_free(selected);list_free(duplicated);free(batch_city);
}
static int __fastcall mock_friend_has(Obj person,Obj other) {
    if(!valid(other))return 0;
    for(unsigned i=0;i<8;++i)if(word(person,0xC0+i*2)==word(other,0x10))return 1;
    return 0;
}
static void __fastcall mock_friend_add(Obj person,Obj other) {
    if(mock_friend_has(person,other))return;
    for(unsigned i=0;i<8;++i)if(word(person,0xC0+i*2)==0) {
        put16(person,0xC0+i*2,word(other,0x10));return;
    }
}
static void __fastcall mock_friend_remove(Obj person,Obj other) {
    unsigned n=0;
    for(unsigned i=0;i<8;++i) {
        unsigned id=word(person,0xC0+i*2);
        if(id!=word(other,0x10))put16(person,0xC0+n++*2,id);
    }
    for(;n<8;++n)put16(person,0xC0+n*2,0);
}
static void test_friendship_bindings() {
    original_friend_add=&mock_friend_add;original_friend_has=&mock_friend_has;
    original_friend_remove=&mock_friend_remove;
    Obj person=object(1000);std::vector<Obj> friends;
    unsigned char protected_bytes[0x130];
    memset(person+0xD0,0xA5,sizeof(protected_bytes));memcpy(protected_bytes,person+0xD0,sizeof(protected_bytes));
    expanded_friends.clear();friendship_world=root;
    for(unsigned id=1001;id<=1500;++id) {
        Obj other=object(id);friends.push_back(other);friendship_add(person,other);
        expect(friendship_has(person,other)==1);
    }
    expect(expanded_friends.list(1000,friendship_native(person)).size()==500);
    expect(mock_friend_has(person,friends[7]) && !mock_friend_has(person,friends[8]));
    expect(memcmp(person+0xD0,protected_bytes,sizeof(protected_bytes))==0);
    Obj excess=object(1501);friendship_add(person,excess);
    expect(!friendship_has(person,excess));expect(!friendship_mutual(person,excess));
    expect(!friendship_has(excess,person));
    friendship_remove(person,friends.back());expect(!friendship_has(person,friends.back()));
    expect(friendship_mutual(person,excess));
    expect(friendship_has(person,excess) && friendship_has(excess,person));
    expect(expanded_friends.list(1000,friendship_native(person)).size()==500);
    enabled=0;expect(!friendship_has(person,excess));expect(friendship_has(person,friends.front()));enabled=1;
    // World pointer changes cannot retain relationships from another game.
    Obj replacement=static_cast<Obj>(calloc(1,0x90000));putptr(image,0x277DF18,replacement);
    expect(!friendship_has(person,excess));putptr(image,0x277DF18,root);
    expect(friendship_has(person,friends.front()));
    free(replacement);free(person);free(excess);for(Obj other:friends)free(other);
}
#include "friendship_bindings_fixture.inl"
static void test_loyalty_instructions() {
    Obj saved_image=image;
    Obj page=static_cast<Obj>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    expect(page!=nullptr);
    // Execute the native string formatter's actual MOV/CMP/CMOV sequence on
    // a private fixture page. RBX is preserved according to the Windows ABI.
    const unsigned char text[]={0x53,0x0F,0xB6,0x19,0xB8,0x64,0,0,0,0x39,0xC3,
                                0x0F,0x4F,0xD8,0x89,0xD8,0x5B,0xC3};
    memcpy(page+0x2A,text,sizeof(text));image=page-0x35CA00;
    auto formatter=reinterpret_cast<unsigned(__fastcall *)(unsigned char *)>(page+0x2A);
    for(unsigned value:{0u,99u,100u,101u,120u,140u,255u}) {
        unsigned char raw=static_cast<unsigned char>(value);expect(formatter(&raw)==(value>100?100:value));
    }
    const unsigned char nops[]={0x90,0x90,0x90};CodeLink text_link;
    expect(prepare_bytes(text_link,0x35CA35,nops,3));expect(publish_link(text_link));
    for(unsigned value:{0u,99u,100u,101u,120u,140u,255u}) {
        unsigned char raw=static_cast<unsigned char>(value);expect(formatter(&raw)==value && raw==value);
    }
    restore_link(text_link);expect(memcmp(page+0x2A,text,sizeof(text))==0);
    // Numeric value constructor picks either the constant-100 pointer or
    // the raw-value pointer. The replacement picks the raw pointer for all.
    const unsigned char number[]={0x48,0x83,0xEC,0x18,0xC7,0x04,0x24,0x64,0,0,0,
        0x89,0x4C,0x24,0x08,0x48,0x8D,0x04,0x24,0x48,0x8D,0x54,0x24,0x08,
        0x83,0xF9,0x64,0x48,0x0F,0x4E,0xC2,0x8B,0x00,0x48,0x83,0xC4,0x18,0xC3};
    memcpy(page+0x2B,number,sizeof(number));image=page-0x34A890;
    auto numeric=reinterpret_cast<unsigned(__fastcall *)(unsigned)>(page+0x2B);
    for(unsigned value:{0u,99u,100u,101u,120u,140u,255u})expect(numeric(value)==(value>100?100:value));
    const unsigned char raw_pointer[]={0x48,0x89,0xD0,0x90};CodeLink numeric_link;
    expect(prepare_bytes(numeric_link,0x34A8D6,raw_pointer,4));expect(publish_link(numeric_link));
    for(unsigned value:{0u,99u,100u,101u,120u,140u,255u})expect(numeric(value)==value);
    restore_link(numeric_link);expect(memcmp(page+0x2B,number,sizeof(number))==0);
    // A competing byte change rejects the whole 16-byte publication. No
    // partially published display clamp is left behind after that failure.
    expect(prepare_bytes(numeric_link,0x34A8D6,raw_pointer,4));page[0x40]^=1;
    expect(!publish_link(numeric_link));expect(memcmp(page+0x46,number+27,4)==0);
    image=saved_image;VirtualFree(page,0,MEM_RELEASE);
}
#include "recruit_ai_fixture.inl"
#include "injury_fixture.inl"
int main() {
    image=static_cast<Obj>(calloc(1,0x2800000));root=static_cast<Obj>(calloc(1,0x90000));
    putptr(image,0x277DF18,root);putptr(image,0x27D10E8,root);putptr(image,0x27D10F0,heads);
    put32(image,0x27D1120,4096);enabled=1;
    native_functions={{0x32F500,reinterpret_cast<void *>(&mock_valid)},
        {0x22CA00,reinterpret_cast<void *>(&mock_owner)},
        {0xBF7A40,reinterpret_cast<void *>(&mock_store)},
        {0xB26470,reinterpret_cast<void *>(&mock_object)},
        {0x21E2B0,reinterpret_cast<void *>(&mock_list_init)},
        {0x18DF0,reinterpret_cast<void *>(&mock_list_add)},
        {0x335060,reinterpret_cast<void *>(&mock_remove)},
        {0x424D60,reinterpret_cast<void *>(&mock_list_free)},
        {0x2B5620,reinterpret_cast<void *>(&mock_family)},
        {0x6DA8A0,reinterpret_cast<void *>(&mock_envoys)},
        {0x232A00,reinterpret_cast<void *>(&mock_dislikes)},
        {0x232EE0,reinterpret_cast<void *>(&mock_likes)}};
    for(unsigned rva:{0x7B360u,0x39490u,0x3D170u,0x2CC50u,0x53E50u,0x7E550u,0x34F10u,
                     0x35770u,0x3DBF0u,0x3E490u,0x3E100u,0x36320u})
        native_functions[rva]=reinterpret_cast<void *>(&mock_node);
    Obj district=object(1),own_force=object(10),foreign_force=object(20),ruler=object(10);
    district[0x10]=1;slot(0xDF30,1,own_force);slot(0x158,10,ruler);slot(0xE0D0,1,district);
    Obj target=object(100),spouse=object(101),sibling=object(102),envoy=object(103),node=object(1);
    for(Obj person:{target,spouse,sibling,envoy})slot(0x158,word(person,0x10),person);
    owners[envoy]=own_force;envoy[0xE0]=1;owners[spouse]=foreign_force;owners[sibling]=foreign_force;
    family={target,sibling,envoy};envoys={envoy};put32(node,0x38,1);put32(node,0x3C,2);
    expect(personnel_recruitment_reason(target,district,nullptr)==personnel::Eligible);
    put16(target,0xD0,10);expect(personnel_recruitment_reason(target,district,nullptr)==personnel::DislikedRuler);
    put16(target,0xBA,101);expect(personnel_recruitment_reason(target,district,nullptr)==3);
    put16(target,0xBC,100);put16(sibling,0xBC,100);
    expect(personnel_recruitment_reason(target,district,nullptr)==7);
    put16(envoy,0xBC,100);
    expect(personnel_recruitment_reason(target,district,envoy)==personnel::FamilyEnvoyException);
    expect(personnel_recruitment_reason(target,nullptr,envoy)==personnel::MissingContext);
    values={{1,envoy},{2,target}};native_orders=0;
    expect(personnel_employ_command(node,node,node)==1 && native_orders==1);
    // The already investigated AI wrapper uses the same hard ban policy;
    // family envoys cannot bypass it or consume an order.
    put16(target,0x162,10);target[0x164]=12;
    expect(personnel_employ_command(node,node,node)==2 && native_orders==1);
    put16(target,0x162,20);
    expect(personnel_employ_command(node,node,node)==1 && native_orders==2);
    put16(target,0x162,0);target[0x164]=0;native_orders=1;
    put16(envoy,0xBC,0);expect(personnel_employ_command(node,node,node)==2 && native_orders==1);
    put16(envoy,0xBC,100);envoy[0x161]=1;
    expect(personnel_employ_command(node,node,node)==2 && native_orders==1);
    enabled=0;expect(personnel_employ_command(node,node,node)==1 && native_orders==2);enabled=1;
    GameList targets;list_init(targets);list_add(targets,target);list_add(targets,target);
    values={{1,district},{2,reinterpret_cast<Obj>(&targets)}};
    expect(personnel_recruitment_targets(node,node,node)==1 && !contains(targets,target));
    envoy[0x161]=0;list_add(targets,target);
    expect(personnel_recruitment_targets(node,node,node)==1 && contains(targets,target));
    put16(target,0x162,10);target[0x164]=0;
    expect(personnel_recruitment_targets(node,node,node)==1 && !contains(targets,target));
    put16(target,0x162,0);target[0xE8]=100;list_add(targets,target);
    expect(personnel_recruitment_targets(node,node,node)==1 && !contains(targets,target));
    target[0xE8]=0;list_add(targets,target);
    put16(envoy,0xBC,0);expect(personnel_recruitment_targets(node,node,node)==1 && !contains(targets,target));
    put16(target,0xD0,0);put16(target,0xBA,0);put16(target,0xBC,0);list_add(targets,target);
    expect(personnel_recruitment_targets(node,node,node)==1 && contains(targets,target));
    GameList actors;list_init(actors);list_add(actors,envoy);list_add(actors,spouse);list_add(actors,spouse);
    spouse[0x161]=1;values={{1,reinterpret_cast<Obj>(&actors)}};
    expect(personnel_employ_actors(node,node,node)==1 && contains(actors,envoy) && !contains(actors,spouse));
    spouse[0x161]=3;list_add(actors,spouse);
    expect(personnel_search_actors(node,node,node)==1 && contains(actors,envoy) && !contains(actors,spouse));
    values={{2,envoy}};native_orders=0;expect(personnel_search_command(node,node,node)==1 && native_orders==1);
    envoy[0x161]=1;expect(personnel_search_command(node,node,node)==2 && native_orders==1);
    for(Execute handler:{&personnel_departure,&personnel_transport,&personnel_recruit_officer,
                         &personnel_training_officer,&personnel_spy_officer,&domestic_officer_node}) {
        values={{1,envoy},{2,envoy}};native_orders=0;envoy[0x161]=0;
        expect(handler(node,node,node)==1 && native_orders==1);
        for(unsigned injury:{1u,2u,3u}) {
            envoy[0x161]=static_cast<unsigned char>(injury);
            expect(handler(node,node,node)==2 && native_orders==1);
        }
        enabled=0;expect(handler(node,node,node)==1 && native_orders==2);enabled=1;
    }
    expect(in_auto_domestic==0);
    envoy[0x161]=0;values={{1,district},{2,reinterpret_cast<Obj>(&actors)}};
    list_add(actors,envoy);list_add(actors,spouse);
    expect(personnel_officer_candidates(node,node,node)==1 && contains(actors,envoy) && !contains(actors,spouse));
    test_loyalty_instructions();
    test_auto_employ(district,own_force);
    test_recruit_ai();
    test_recruit_ai_publication();
    test_injury();
    test_injury_publication();
    test_friendship_bindings();
    test_friendship_persistence();
    test_friendship_archive_bridges();
    test_friendship_publication();
    test_friendship_lifecycle();
    list_free(actors);list_free(targets);
    printf("{\"status\":\"personnel_native_fixture_passed\",\"checks\":%u,\"gameplay_verified\":false,\"installed\":false}\n",checks);
    return 0;
}
