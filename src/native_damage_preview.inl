// Convenience-only. All native calls below execute on the existing target UI
// caller, never the loader worker. No command, casualty or durability writer.
#include "damage_preview.h"
// AI-CONTRACT [read-only UI]: Forecast hooks may compute/display damage but must never issue commands or apply casualties/durability. Preserve target-kind discrimination; verify the UI caller ABI independently of pure arithmetic fixtures.
namespace damage_forecast {
using Refresh=void(__fastcall *)(Obj,Obj,Obj);
using Common=float(__fastcall *)(Obj,Obj,int,int);
using Stat=float(__fastcall *)(float,Obj,Obj,Obj,int,int);
using Damage=void(__fastcall *)(Obj,float,float,Obj,float,float,int *);
static Refresh original_refresh;
static volatile LONG installed;
struct Actor { Obj object=nullptr,army=nullptr,city=nullptr,building=nullptr;unsigned force=0; };
static unsigned kind(Obj p) {
    return valid(p)?(*reinterpret_cast<unsigned(__fastcall **)(Obj)>(pointer(p,0)+0x10))(p):0;
}
static Actor actor(Obj p) {
    Actor a;
    if(!valid(p))return a;
    const unsigned k=kind(p);
    if(k==27) {a.object=a.army=p;a.force=army_force(p);return a;}
    if(k==5 || k==6 || k==2) {
        const unsigned id=word(p,0x4E);
        if(id<1 || id>61)return a;
        a.city=fn<Obj(__fastcall *)(Obj,unsigned)>(0x32C130)(world(),id);
        if(!valid(a.city))return {};
        a.object=a.city;a.building=table(0x6DA98,id,3000);
    } else if(k==11) {
        a.object=a.building=p;
        const unsigned id=fn<unsigned(__fastcall *)(Obj)>(0x22DEA0)(p);
        if(id>=1 && id<=61) {
            a.city=fn<Obj(__fastcall *)(Obj,unsigned)>(0x32C130)(world(),id);
            if(!valid(a.city))return {};
            a.object=a.city;
        }
    }
    if(valid(a.building))a.force=a.building[0x13];
    return a;
}
static Obj formation(const Actor &a) {return a.army?fn<Obj(__fastcall *)(Obj)>(0x22B090)(a.army):nullptr;}
static float stat(const Actor &a,unsigned rva) {
    float c=fn<Common>(0x2A8D40)(a.army,a.city,1,1);
    return fn<Stat>(rva)(c,a.army,a.city,formation(a),1,0);
}
static int soldiers(const Actor &from,const Actor &to,unsigned attack_rva) {
    int out=0;
    const float attack=stat(from,attack_rva),defense=stat(to,0x2A9E50);
    fn<Damage>(0x16A1D0)(from.object,attack,0,to.object,0,defense,&out);
    return (std::max)(0,out);
}
static int durability(const Actor &from,const Actor &to) {
    if(!from.army || !valid(to.building))return 0;
    const float w=fn<float(__fastcall *)(Obj,Obj,int,int)>(0x2AA2E0)(from.army,formation(from),1,0);
    Obj definition=table(0x73958,to.building[0x12],30);
    if(!valid(definition))return 0;
    const int category=definition[0x2C];
    int wall=0;
    if(category==3) {
        int slots[127]={};
        fn<void(__fastcall *)(Obj,int,int *,int,int)>(0x26A2C0)(from.army,103,slots,0,0);
        wall=slots[103];
    }
    Obj force=fn<Obj(__fastcall *)(Obj)>(0x22C850)(from.army);
    int bonus=valid(force)?fn<int(__fastcall *)(Obj,unsigned)>(0x22C0F0)(force,0x45):0;
    const int distance=fn<int(__fastcall *)(unsigned,unsigned)>(0x2AA850)(word(from.army,0x2A),word(to.building,0x10));
    const int finish_range=fn<int(__fastcall *)(Obj)>(0x232650)(to.building)?2:1;
    return damage_preview::breach(damage_preview::count(w),word(to.building,0x14),category,bonus,wall,distance==finish_range);
}
static int facility(const Actor &from,const Actor &to) {
    if(!valid(from.building) || !to.army || !(from.building[0x16]&1))return 0;
    Obj def=table(0x73958,from.building[0x12],30);
    if(!valid(def) || def[0x2C]!=2)return 0; // fort/tower; traps have a different execution path
    int coefficient=0;
    for(int slot=0;slot<2;++slot)
        if(fn<unsigned(__fastcall *)(Obj,int)>(0x18FD20)(def,slot)==1)
            coefficient=fn<int(__fastcall *)(Obj,int,int)>(0x18FDA0)(def,slot,0);
    if(coefficient<=0)return 0; // e.g. fort/music platform: no ordinary attack effect
    const int maximum=fn<int(__fastcall *)(Obj)>(0x18FE30)(def);
    const float attack=damage_preview::facility_attack(coefficient,word(from.building,0x14),maximum);
    if(attack<=0)return 0;
    int out=0;
    fn<Damage>(0x16A1D0)(from.object,attack,0,to.object,0,stat(to,0x2A9E50),&out);
    Obj force=table(0xDF30,from.force,51);
    int policy=valid(force)?fn<int(__fastcall *)(Obj,unsigned)>(0x41C200)(force,0x14):0;
    Obj leader=fn<Obj(__fastcall *)(Obj)>(0x401C0)(to.army);
    int resistance=valid(leader) && fn<int(__fastcall *)(Obj,unsigned)>(0x2C5460)(leader,0x70)?
        fn<int(__fastcall *)(Obj,unsigned)>(0x2B2DA0)(leader,0x70):0;
    int class_bonus=valid(force)?fn<int(__fastcall *)(Obj,unsigned)>(0x22C0F0)(force,0x3D):0;
    return damage_preview::facility_damage(out,policy,class_bonus,resistance);
}
static damage_preview::Result calculate(const Actor &a,const Actor &b) {
    damage_preview::Result r;
    if(b.army) {r.dealt=soldiers(a,b,0x2B21A0);r.received=soldiers(b,a,0x2B21A0);}
    else if(b.city) {
        r.structure=true;r.dealt=soldiers(a,b,0x2B2440);
        r.received=soldiers(b,a,0x2B21A0);r.durability=durability(a,b);
    } else if(b.building) {r.structure=true;r.durability=durability(a,b);r.received=facility(b,a);}
    return r;
}
static Obj hovered() {
    Obj map=fn<Obj(__fastcall *)()>(0x18F10)();
    return map?pointer(map,0x110):nullptr;
}
static void text(Obj table,int part,const wchar_t *value) {
    fn<void(__fastcall *)(Obj,int,const wchar_t *,int)>(0x930D20)(table,part,value,1);
}
static void show(Obj table,int part,bool value) {
    fn<void(__fastcall *)(Obj,int,int)>(0x92F810)(table,part,value?1:0);
}
static void name(const Actor &a,wchar_t (&out)[64]) {
    const wchar_t *value=nullptr;
    if(a.army)value=fn<const wchar_t *(__fastcall *)(Obj)>(0x354950)(a.army);
    else if(a.city)value=reinterpret_cast<const wchar_t *>(a.city+0x10);
    else if(a.building) {
        Obj def=table(0x73958,a.building[0x12],30);
        if(valid(def))value=reinterpret_cast<const wchar_t *>(def+0x10);
    }
    wcsncpy_s(out,value?value:L"대상",_TRUNCATE);
}
static void draw(Obj panel,const Actor &a,const Actor &b,const damage_preview::Result &r,bool identity_changed) {
    Obj parts=panel+0x130;
    if(dword(parts,0x10)<33 || dword(parts,0x10)>256)return;
    wchar_t label[64],number[64];
    if(identity_changed) {
        name(a,label);text(parts,8,label);name(b,label);text(parts,12,label);
        const Actor *sides[]={&a,&b};
        for(unsigned i=0;i<2;++i) {
            Obj person=sides[i]->army?table(0x158,word(sides[i]->army,0x12),6000):
                (sides[i]->city?fn<Obj(__fastcall *)(Obj)>(0x22AB60)(sides[i]->city):nullptr);
            const int part=i?9:5;
            show(parts,part,valid(person));
            if(valid(person))fn<void(__fastcall *)(Obj,int,Obj)>(0x92ED60)(parts,part,person);
        }
    }
    text(parts,3,L"예상 피해(1회)");text(parts,30,L"주는 병사 피해");
    for(int part=17;part<=26;++part)show(parts,part,false);
    show(parts,16,r.structure);
    if(r.structure) {swprintf_s(label,L"내구 %d",r.durability);text(parts,16,label);}
    swprintf_s(number,L"%d명",r.dealt);text(parts,31,number);
    swprintf_s(number,L"%d명",r.received);text(parts,32,number);
    // Same panel visibility slot used at 7FA730..7FA738.
    (*reinterpret_cast<void(__fastcall **)(Obj,int)>(pointer(panel,0)+0xC8))(panel,1);
}
static void __fastcall refresh(Obj panel,Obj unit,Obj target) { COST_SCOPE("damage_preview_refresh");
    original_refresh(panel,unit,target);
    if(!enabled || !mod_features.damage_preview || !panel || !valid(unit) || kind(unit)!=27)return;
    // Cache owns no native objects. Refresh identity immediately, values at 5 Hz.
    static Obj last_panel,last_army,last_target;
    static unsigned last_person,last_force,last_opponent;
    static ULONGLONG next;
    Actor a=actor(unit),under_cursor=actor(hovered());
    Actor b=under_cursor.building && !under_cursor.army?under_cursor:actor(valid(target)?target:hovered());
    if(!a.army || !b.object || !a.force || !b.force || a.force==b.force ||
       !fn<int(__fastcall *)(unsigned,unsigned)>(0x2CA280)(a.force,b.force)) {
        last_target=nullptr;next=0;
        (*reinterpret_cast<void(__fastcall **)(Obj,int)>(pointer(panel,0)+0xC8))(panel,0);return;
    }
    const unsigned person=word(unit,0x12),force=a.force*64+b.force;
    const unsigned opponent=(b.army?word(b.army,0x12):(b.city?word(b.city,0x2E):0))*256+
        (b.building?b.building[0x12]:27);
    const bool changed=last_panel!=panel || last_army!=unit || last_target!=b.object || last_person!=person ||
        last_force!=force || last_opponent!=opponent;
    const ULONGLONG now=GetTickCount64();
    if(!changed && now<next)return;
    draw(panel,a,b,calculate(a,b),changed);
    last_panel=panel;last_army=unit;last_target=b.object;last_person=person;last_force=force;last_opponent=opponent;next=now+200;
}
#include "damage_preview_profile.h"
static bool guards_match() {
    for(const auto &g:guards) {
        unsigned long long h=0xCBF29CE484222325ull;
        for(unsigned i=0;i<g.size;++i)h=(h^image[g.rva+i])*0x100000001B3ull;
        if(h!=g.hash)return false;
    }
    return true;
}
static bool install() {
    if(InterlockedCompareExchange(&installed,0,0) || !guards_match()) {log_event("damage_preview_guard_rejected");return false;}
    Obj relay=allocate_near(image+0x7FA230);if(!relay)return false;
    absolute_jump(relay,reinterpret_cast<void *>(&refresh));
    // The nine displaced bytes are TEST RDX,RDX; JE native RET. Preserve the
    // null-input early return before the original function establishes a frame.
    Obj bridge=relay+32;
    memcpy(bridge,image+0x7FA230,9);
    intptr_t delta=reinterpret_cast<intptr_t>(image+0x7FA762)-reinterpret_cast<intptr_t>(bridge+9);
    if(delta<INT32_MIN || delta>INT32_MAX) {VirtualFree(relay,0,MEM_RELEASE);return false;}
    const int displacement=static_cast<int>(delta);memcpy(bridge+5,&displacement,4);
    absolute_jump(bridge+9,image+0x7FA239);
    CodeLink link;DWORD old;
    if(!prepare_link(link,0x7FA230,0xE9,relay) || !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),relay,64)) {VirtualFree(relay,0,MEM_RELEASE);return false;}
    original_refresh=reinterpret_cast<Refresh>(bridge);
    if(!publish_link(link)) {log_event("damage_preview_publish_rejected");return false;}
    InterlockedExchange(&installed,1);log_event("damage_preview_installed");return true;
}
}
