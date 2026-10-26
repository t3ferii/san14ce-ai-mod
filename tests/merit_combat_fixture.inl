// Execute captured native scoring/grouping bytes in THIS fixture process.
// Game world, neighbor enumeration and other relationships remain mocks;
// this proves common-path parity, not live damage or a real tactic animation.
#include <cmath>
#include "merit_combat_capture.h"
static std::vector<Obj> combat_neighbors;
static uint64_t __fastcall combat_zero(Obj,Obj) {return 0;}
static int __fastcall combat_one(Obj,Obj) {return 1;}
static int __fastcall combat_affinity(Obj,Obj) {return 100;}
static float __fastcall combat_floor(float value) {return std::floor(value);}
static void __fastcall combat_log(unsigned,const char *,...) {}
static const wchar_t *__fastcall combat_name(Obj) {return L"fixture";}
static unsigned *__fastcall combat_allocate(Obj,unsigned,unsigned) {
    expect(next_list<256);return new unsigned(next_list++);
}
static void __fastcall combat_clear(Obj,unsigned handle) {
    Link *it=heads[handle];while(it) {Link *next=it->next;delete it;it=next;}
    heads[handle]=nullptr;list_counts[handle]=0;
}
static void __fastcall combat_handle_free(Obj,unsigned *handle) {delete handle;}
static void __fastcall combat_hate(Obj,GameList *,int) {}
static void __fastcall combat_clear_list(GameList *out) {combat_clear(nullptr,*out->handle);}
static void __fastcall combat_copy_add(GameList *out,Obj *value) {list_add(*out,*value);}
static void __fastcall combat_nearby(Obj,unsigned,GameList *out) {
    for(Obj army:combat_neighbors)list_add(*out,army);
}
static int __fastcall combat_force(Obj army) {return army[0x40];}
static bool contains_unit(GameList &list,Obj army) {
    for(Link *it=list_first(list);it;it=it->next)if(it->value==army)return true;return false;
}
static void test_captured_combat() {
    Obj saved=image;
    Obj memory=static_cast<Obj>(VirtualAlloc(nullptr,0x2800000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    expect(memory!=nullptr);image=memory;putptr(image,0x277DF18,root);
    putptr(image,0x27D10E8,root);putptr(image,0x27D10F0,heads);putptr(image,0x27D1108,list_counts);
    put32(image,0x27D1120,256);putptr(image,0x27D1160,root);
    memcpy(image+0x2A6AA0,captured_2A6AA0,sizeof(captured_2A6AA0));
    memcpy(image+0x2A6B60,captured_2A6B60,sizeof(captured_2A6B60));
    memcpy(image+0x2C8180,captured_2C8180,sizeof(captured_2C8180));
    memcpy(image+0x2AF7E0,captured_2AF7E0,sizeof(captured_2AF7E0));
    memcpy(image+0x21B2E0,captured_21B2E0,sizeof(captured_21B2E0));
    // Controlled parameter table, used identically in native and merit cases.
    put32(image,0x1A4A268,30);*reinterpret_cast<float *>(image+0x13AE8FC)=100;
    put32(image,0x27D2694,10);put32(image,0x1A4A240,40);put32(image,0x1A4A24C,60);put32(image,0x1A4A254,80);
    put32(image,0x1A4A22C,3);
    for(unsigned rva:{0x2C9BD0u,0x2C8AC0u,0x2C5780u,0x2C6EF0u,0x2C3B50u,0x2C5460u,0x2B2DA0u,0x232A00u,
                      0x2C9310u,0x233440u})absolute_jump(image+rva,reinterpret_cast<void *>(&combat_zero));
    absolute_jump(image+0x32F500,reinterpret_cast<void *>(&mock_valid));
    absolute_jump(image+0x22E310,reinterpret_cast<void *>(&merit_list));
    absolute_jump(image+0x2AFAC0,reinterpret_cast<void *>(&combat_one));
    absolute_jump(image+0x18B60,reinterpret_cast<void *>(&combat_allocate));
    absolute_jump(image+0x18770,reinterpret_cast<void *>(&combat_clear));
    absolute_jump(image+0x18830,reinterpret_cast<void *>(&combat_handle_free));
    absolute_jump(image+0x18DF0,reinterpret_cast<void *>(&mock_add));
    absolute_jump(image+0x22B840,reinterpret_cast<void *>(&combat_hate));
    absolute_jump(image+0x2A5E20,reinterpret_cast<void *>(&combat_affinity));
    absolute_jump(image+0x101D4E8,reinterpret_cast<void *>(&combat_floor));
    absolute_jump(image+0x22CA00,reinterpret_cast<void *>(&mock_force));
    absolute_jump(image+0x22EEC0,reinterpret_cast<void *>(&combat_name));
    absolute_jump(image+0x366B40,reinterpret_cast<void *>(&combat_log));
    absolute_jump(image+0x56EAA0,reinterpret_cast<void *>(&combat_clear_list));
    absolute_jump(image+0x2A7490,reinterpret_cast<void *>(&combat_nearby));
    absolute_jump(image+0x22BAE0,reinterpret_cast<void *>(&combat_force));
    absolute_jump(image+0x58820,reinterpret_cast<void *>(&combat_copy_add));
    FlushInstructionCache(GetCurrentProcess(),memory,0x2800000);
    auto score=reinterpret_cast<int(__fastcall *)(Obj,Obj,int)>(image+0x2A6B60);
    auto level=reinterpret_cast<unsigned char(__fastcall *)(Obj,Obj,int)>(image+0x2A6AA0);
    auto nearby=reinterpret_cast<unsigned(__fastcall *)(Obj,GameList *)>(image+0x2AF7E0);
    original_merit_has=&mock_has;original_merit_list=&mock_list;
    original_merit_normalize=reinterpret_cast<MeritNormalize>(image+0x21B2E0);
    Obj person=object(450),ruler=object(10);person[0xE0]=ruler[0xE0]=1;person[0x1C2]=3;
    slot(0x158,450,person);slot(0x158,10,ruler);put16(force1,0x10,10);force1[0x178]=0;
    Obj army=object(90),peer=object(91);put16(army,0x12,450);put16(peer,0x12,10);
    army[0x40]=peer[0x40]=1;combat_neighbors={peer};merit_bonds.clear();merit_world=root;
    int none=score(person,ruler,1);expect(none==0 && level(person,ruler,1)==1);
    put16(person,0xC0,10);put16(ruler,0xC0,450);
    int native_forward=score(person,ruler,1),native_reverse=score(ruler,person,1);
    unsigned native_level=level(person,ruler,1);
    GameList out;list_init(out);expect(nearby(army,&out)==1 && contains_unit(out,peer));list_free(out);
    put16(person,0xC0,0);put16(ruler,0xC0,0);expect(merit_bonds.award(450,2,merit_context,nullptr));
    expect(score(person,ruler,1)==native_forward && score(ruler,person,1)==native_reverse);
    expect(native_forward==60 && native_reverse==60 && level(person,ruler,1)==native_level && native_level==4);
    list_init(out);expect(nearby(army,&out)==1 && contains_unit(out,peer));list_free(out);
    // Native status/same-force conditions still reject a relation candidate.
    peer[0x23]=1;list_init(out);expect(nearby(army,&out)==0);list_free(out);peer[0x23]=0;
    peer[0x40]=2;list_init(out);expect(nearby(army,&out)==0);list_free(out);peer[0x40]=1;
    put16(person,0x15E,0x4000);list_init(out);expect(nearby(army,&out)==0);list_free(out);put16(person,0x15E,0);
    // Existing native + supplemental is counted once in each direction.
    put16(person,0xC0,10);put16(ruler,0xC0,450);expect(score(person,ruler,1)==native_forward);
    put16(person,0xC0,0);put16(ruler,0xC0,0);
    unsigned char fields[32];memcpy(fields,person+0xC0,32);
    merit_normalize(person);expect(!memcmp(fields,person+0xC0,32) && in_merit_normalize==0);
    expect(merit_has(person,ruler) && merit_has(ruler,person));
    // Native cleanup must never turn a temporary merit relationship permanent.
    put16(force1,0x10,11);expect(score(person,ruler,1)==none && !merit_has(person,ruler));
    put16(force1,0x10,10);expect(!merit_has(person,ruler));
    list_init(out);expect(nearby(army,&out)==0);list_free(out);
    slot(0x158,450,nullptr);slot(0x158,10,nullptr);free(person);free(ruler);free(army);free(peer);
    combat_neighbors.clear();image=saved;VirtualFree(memory,0,MEM_RELEASE);
}
