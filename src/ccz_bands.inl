// 종군 중립군(무리) 0단계 진단 빌드 (사용자 2026-11-17, docs/NEUTRAL_BANDITS.md 10.7~10.8). SAN14_CCZ.dll 쪽 판단.
// SAN14_Convenience.dll(native_ccz_bands.inl)이 원작 함수 9곳의 첫머리를 이 파일의 hook_* 로 잇고, 원래 첫머리(gate)를
// configure로 넘긴다. 모든 hook은 원작 자신의 스레드(하루 처리·화면)에서만 불린다.
//  - 무리 부대 = 원작 세력 51 부대(범용 무장 4033~4100). slots[지휘 무장-4033]에 (부대 주소 | F<<56)을 한 번에 기록하고,
//    22E690은 그 부대에만 F(섬기는 세력)로 답한다. 칠하기 301D00·병참 278290·제거 25A880 안에서는 그 부대에 51로 답한다.
//  - 186E60: 무리 칠하기 범위에서 칸 주인이 F이거나 F와 적대가 아니면 쓰지 않는다. 생성 중에는 칠하지 않는다.
//  - 268070: 무리 부대의 정기 사기 감소 0.  2ECEA0/2ECF00: 모드 종군 명령 밖에서 무리 부대 명령 쓰기를 거절하고 기록한다.
//  - 2DC550(하루 처리 이동 단계) 앞: 종군 명령, 뒤: 주인 점검·해체(같은 날)·대기 무리의 생성 점검·대기 기록 규칙(군주 성의 전선 거점).
// 기록 표는 하루 처리 스레드만 쓰고(잠금), hook의 조회는 slots 한 번 읽기로 끝난다.
#include <intrin.h>
namespace ccz_bands {
constexpr unsigned kFirstGeneric=4033,kSlots=68,kMaxBands=16,kNoHex=48400,kBandit=51;
enum : unsigned {kFlagOn=1,kFlagDev=2,kFlagSetters=4,kFlagPortrait=8};   // kFlagDev: unused since 2026-11-19 (the idle rule is real)
constexpr unsigned kTroopShift=8;   // flags bits 8..11: [ccz] band_troops 1..3
enum : unsigned char {kFree=0,kIdle=1,kLive=2};
struct Band {
    unsigned char state,force,last_kind;
    unsigned short base_before,seen_troops;      // 2026-11-19 commander +0xE2 before the host base, troops last seen (not saved)
    unsigned char cuts;                          // 2026-11-19 idle 치안 cuts already applied (10 each), 사이드카 p[15]
    unsigned char kind,rank;                     // 2026-11-18 지역 특색(ccz_band_kinds.h)·직함 순번, 사이드카 p[3]·p[14]
    unsigned char goal_kind,seen_goal_kind,seen_chase_kind,seen_form;   // 주인 목표(따라붙기)·마지막으로 본 주인 상태(기록 안 함)
    unsigned short commander,host_commander,unit_index,host_index,last_target;
    unsigned short goal_target,dest,seen_goal,seen_chase;
    unsigned short wait_base;                    // 2026-11-20 live band without a host waits beside this base (1..61, not saved)
    unsigned site;
    int created;
    Obj unit,host;
};
// 22E690 ends with movzx eax / xor eax: callers (22E2A0: cmp eax,33h; cdqe) read the whole EAX, so the answer is 32-bit (local218 crash fix).
using ArmyForce=unsigned (__fastcall *)(Obj);
using UnitCall=void (__fastcall *)(Obj);
using Remove=void (__fastcall *)(Obj,Obj,unsigned long long,int,int);
using PaintWrite=unsigned long long (__fastcall *)(Obj,Obj,unsigned,int,unsigned,int,int);
using Morale=int (__fastcall *)(Obj,int);
using DayMove=void (__fastcall *)(Obj,int);
using SetAction=void (__fastcall *)(Obj,int);
using SetTarget=void (__fastcall *)(Obj,unsigned char,unsigned short);
using FaceTexture=unsigned long long (__fastcall *)(Obj,Obj,int,unsigned);   // 8AD530(manager, zp1 bytes, image id, p4): 0 = failed
using MaxMorale=unsigned (__fastcall *)(Obj);   // 230460(army): 26A600 of the army's force (22E690) = 100 + policy terms
using Capture=bool (__fastcall *)(Obj,Obj);     // 2801C0(person, captor person): capture roll (rules_people.md 6.1)
struct Originals {ArmyForce army_force;UnitCall paint,supply;Remove remove;PaintWrite paint_write;Morale morale;DayMove day_move;SetAction set_action;SetTarget set_target;
                  FaceTexture face_texture;UnitCall unit_target;MaxMorale max_morale;Capture capture;};
static Originals original;
static unsigned flags;
static volatile LONG64 (&slots)[kSlots]=ccz_idle::slots;   // shared with the sprite renderer (ccz_module.cpp)
static_assert(kSlots==ccz_idle::kSlots && kFirstGeneric==ccz_idle::kFirstGeneric,"one generic commander table");
static __declspec(thread) Obj face_unit;
static __declspec(thread) unsigned paint_force,paint_refused,paint_allowed;
static __declspec(thread) bool spawning,own_order;
static SRWLOCK lock=SRWLOCK_INIT;
static Band bands[kMaxBands];
static Obj band_world;
static int last_day=-1;
static unsigned day_calls,morale_skips,foreign_today,spawn_skips;

static unsigned hex_distance(unsigned a,unsigned b) {
    if(a>=kNoHex || b>=kNoHex)return ~0u;
    const int ax=static_cast<int>(a%220),bx=static_cast<int>(b%220);
    const int ay=static_cast<int>(a/220)-(ax+(ax&1))/2,by=static_cast<int>(b/220)-(bx+(bx&1))/2;
    auto ab=[](int v){return v<0?-v:v;};
    return static_cast<unsigned>((ab(ax-bx)+ab(ay-by)+ab(ax-bx+ay-by))/2);
}
static Obj unit_at(unsigned index) {return index<=500?table(0x7E838,index,500):nullptr;}
static Obj hex_at(unsigned tile) {return tile<kNoHex?table(0xE270,tile,48399):nullptr;}
static unsigned unit_index(Obj unit) {
    for(unsigned i=0;i<=500;++i)if(unit_at(i)==unit)return i;
    return 0xFFFF;
}
static bool hostile(unsigned owner,unsigned force) {return fn<bool(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(owner,force);}
static unsigned player_force() {   // the idle rule (the ruler's city) and its log
    Obj root=world();Obj context=root?pointer(root,0x871E8):nullptr;if(!context)return 0;
    Obj force=fn<Obj(__fastcall *)(Obj)>(GA(GA_330B90))(context);
    if(!valid(force))return 0;
    auto get_id=*reinterpret_cast<unsigned (__fastcall **)(Obj)>(pointer(force,0)+0x60);
    unsigned id=get_id(force);
    return id>0 && id<52?id:0;
}
// Patron F of a live band unit, 0 for every other unit (one 64-bit read).
static unsigned band_force(Obj unit) {
    if(!unit)return 0;
    unsigned c=static_cast<unsigned>(word(unit,0x12))-kFirstGeneric;if(c>=kSlots)return 0;
    unsigned long long v=static_cast<unsigned long long>(slots[c]);
    return v && reinterpret_cast<Obj>(v&ccz_idle::kUnitMask)==unit?static_cast<unsigned>(v>>56):0;
}
// Patron F (0 = not a live band) and the band's kind (slot bits 48..55), one 64-bit read.
static unsigned band_kind_of(Obj unit,unsigned &kind) {
    kind=0;
    if(!unit)return 0;
    unsigned c=static_cast<unsigned>(word(unit,0x12))-kFirstGeneric;if(c>=kSlots)return 0;
    unsigned long long v=static_cast<unsigned long long>(slots[c]);
    if(!v || reinterpret_cast<Obj>(v&ccz_idle::kUnitMask)!=unit)return 0;
    kind=static_cast<unsigned>((v>>48)&0xFF);
    return static_cast<unsigned>(v>>56);
}
static void slot_set(unsigned commander,Obj unit,unsigned force,unsigned kind=0) {
    unsigned long long v=unit?((reinterpret_cast<unsigned long long>(unit)&ccz_idle::kUnitMask)|(static_cast<unsigned long long>(kind&0xFF)<<48)
        |(static_cast<unsigned long long>(force)<<56)):0;
    if(commander-kFirstGeneric<kSlots)InterlockedExchange64(&slots[commander-kFirstGeneric],static_cast<LONG64>(v));
}
#ifndef NO_RUNTIME_LOG
static unsigned rva_of(unsigned long long a) {
    unsigned long long b=reinterpret_cast<unsigned long long>(image);
    return a>=b && a-b<0x8000000ull?static_cast<unsigned>(a-b):(0xF0000000u|static_cast<unsigned>(a&0x0FFFFFFFu));
}
// First-seen return address -> one log line (with the caller's caller), at most `cap` distinct addresses per table.
template<unsigned N> struct Seen {
    volatile LONG64 keys[N];volatile LONG count;
    bool first(unsigned long long key,unsigned cap) {
        unsigned h=static_cast<unsigned>((key>>2)*2654435761ull)&(N-1);
        for(unsigned probe=0;probe<N;++probe,h=(h+1)&(N-1)) {
            LONG64 v=keys[h];
            if(v==static_cast<LONG64>(key))return false;
            if(v==0) {
                if(InterlockedCompareExchange64(&keys[h],static_cast<LONG64>(key),0)!=0) {if(keys[h]==static_cast<LONG64>(key))return false;continue;}
                return static_cast<unsigned>(InterlockedIncrement(&count))<=cap;
            }
        }
        return false;
    }
};
static Seen<1024> asks;
static Seen<256> foreigners;
static unsigned parent_of(unsigned long long ret) {
    void *frames[8]={};USHORT got=RtlCaptureStackBackTrace(0,8,frames,nullptr);
    for(USHORT i=0;i+1<got;++i)if(reinterpret_cast<unsigned long long>(frames[i])==ret)return rva_of(reinterpret_cast<unsigned long long>(frames[i+1]));
    return 0;
}
static void note_ask(void *ret,unsigned force) {
    unsigned long long r=reinterpret_cast<unsigned long long>(ret);
    if(asks.first(r,300))log_event("band_ask",rva_of(r),parent_of(r),force);
}
static void note_foreign(void *ret,unsigned which,unsigned value,Obj unit) {
    ++foreign_today;
    unsigned long long r=reinterpret_cast<unsigned long long>(ret);
    if(foreigners.first(r^(static_cast<unsigned long long>(which)<<60),120))
        log_event("band_foreign",rva_of(r),(which<<24)|(value&0xFFFFFF),(static_cast<unsigned>(word(unit,0x12))<<16)|(parent_of(r)&0xFFFF));
}
static volatile LONG paint_logs,supply_logs;
#endif

// ---------------------------------------------------------------- hooks (game threads)
static unsigned __fastcall hook_army_force(Obj unit) {
    const unsigned raw=original.army_force(unit)&255u;
    if(raw!=kBandit || unit==face_unit)return raw;
    unsigned f=band_force(unit);if(!f)return raw;
#ifndef NO_RUNTIME_LOG
    note_ask(_ReturnAddress(),f);
#endif
    return f;
}
struct FaceScope {
    Obj saved;unsigned saved_force,saved_refused,saved_allowed;
    FaceScope(Obj unit,unsigned force):saved(face_unit),saved_force(paint_force),saved_refused(paint_refused),saved_allowed(paint_allowed) {
        face_unit=unit;paint_force=force;paint_refused=paint_allowed=0;
    }
    ~FaceScope() {face_unit=saved;paint_force=saved_force;paint_refused=saved_refused;paint_allowed=saved_allowed;}
};
static void __fastcall hook_paint(Obj unit) {
    unsigned f=band_force(unit);
    if(!f) {original.paint(unit);return;}
    FaceScope scope(unit,f);
    original.paint(unit);
#ifndef NO_RUNTIME_LOG
    if((paint_refused || paint_allowed) && InterlockedIncrement(&paint_logs)<=300)
        log_event("band_paint",word(unit,0x12),(f<<24)|paint_refused,paint_allowed);
#endif
}
static void __fastcall hook_supply(Obj unit) {
    if(!band_force(unit)) {original.supply(unit);return;}
    FaceScope scope(unit,0);
#ifndef NO_RUNTIME_LOG
    unsigned before=unit[0x26];
#endif
    original.supply(unit);
#ifndef NO_RUNTIME_LOG
    if(InterlockedIncrement(&supply_logs)<=40)log_event("band_supply",word(unit,0x12),before,unit[0x26]);
#endif
}
// 2026-11-20 user "적부대 격파시 사기상승, 아군부대 퇴각시 사기하강 효과를 크게" / "퇴각의 범위는 궤멸 격파만" / "난 연쇄격파
// 발생하는거 좋아": an army destroyed in battle (25A880 with p4 != 0; arrival in a base is p4 0 / p5 4, as 26E290 and our disband)
// moves every live band's morale through the original 271E70 right after the removal (morale 0 = the original rout, so routs chain):
// the band destroyed an enemy +30, an enemy destroyed within 5 hexes +15, its host destroyed -30, another friendly army destroyed
// within 5 hexes -20 ([ccz] band_morale 1/2/3 = x0.5 / x1 / x1.5). Friendly = the band's patron F or not hostile to F (2CC630).
static unsigned morale_pct=100;   // configure(): flags bits 12..15
constexpr unsigned kShockReach=5,kShockDepth=8;
static __declspec(thread) unsigned shock_depth;
struct Shock {Obj unit;unsigned commander;int delta;unsigned why;};
static int shock_amount(unsigned base) {return static_cast<int>((base*morale_pct+50)/100);}
static unsigned collect_shocks(Obj unit,Obj killer,Shock (&out)[kMaxBands]) {
    unsigned n=0;
    if(!unit || !valid(unit) || unit[0x10]!=1)return 0;
    unsigned dead=band_force(unit);if(!dead)dead=original.army_force(unit)&255u;
    if(!dead || dead>51)return 0;
    if(!TryAcquireSRWLockShared(&lock))return 0;   // our own day pass holds it exclusively (disband, spawn): no shock there
    const unsigned tile=word(unit,0x2A);
    for(const Band &b:bands) {
        if(b.state!=kLive || !b.unit || b.unit==unit || band_force(b.unit)!=b.force || !word(b.unit,0x16))continue;
        const unsigned f=b.force,d=hex_distance(word(b.unit,0x2A),tile);
        int delta=0;unsigned why=0;
        if(dead!=f && hostile(dead,f)) {
            if(killer==b.unit){delta=shock_amount(30);why=1;}
            else if(d<=kShockReach){delta=shock_amount(15);why=2;}
        } else {
            if(unit==b.host){delta=-shock_amount(30);why=3;}
            else if(d<=kShockReach){delta=-shock_amount(20);why=4;}
        }
        delta=ccz_band_kinds::shock_for(b.kind,delta);   // 2026-11-20 rabble x1.5, elite falls at half
        if(delta && n<kMaxBands)out[n++]={b.unit,b.commander,delta,why};
    }
    ReleaseSRWLockShared(&lock);
    return n;
}
static void apply_shocks(const Shock *s,unsigned n) {
    if(!n || shock_depth>=kShockDepth)return;
    ++shock_depth;
    for(unsigned i=0;i<n;++i) {
        Obj u=s[i].unit;
        if(word(u,0x12)!=s[i].commander || !band_force(u) || !word(u,0x16) || !u[0x1A])continue;   // gone in an earlier chained rout
        const unsigned before=u[0x1A];
        fn<void(__fastcall *)(Obj,int,int,int)>(GA(GA_271E70))(u,s[i].delta,0,1);
        // army = commander, value = why (1 killed an enemy, 2 enemy near, 3 host, 4 friend near) << 8 | chain depth, hex = before << 8 | after
        log_event("band_morale_shock",s[i].commander,(s[i].why<<8)|shock_depth,(before<<8)|u[0x1A]);
    }
    --shock_depth;
}
static Obj person_of(unsigned id);
static void __fastcall hook_remove(Obj unit,Obj killer,unsigned long long p3,int p4,int p5) {
    Shock shocks[kMaxBands];
    const unsigned n=p4 && !own_order && (flags&kFlagOn)?collect_shocks(unit,killer,shocks):0u;
    unsigned f=band_force(unit);
    if(!f) {original.remove(unit,killer,p3,p4,p5);apply_shocks(shocks,n);return;}
    const unsigned commander=word(unit,0x12);
    const LONG64 held=slots[commander-kFirstGeneric];
    {FaceScope scope(unit,0);original.remove(unit,killer,p3,p4,p5);}
    // Removal asked: from now on the unit is the original force-51 unit again; the day pass frees the record.
    InterlockedCompareExchange64(&slots[commander-kFirstGeneric],0,held);
    log_event("band_dead",commander,f,(static_cast<unsigned>(p4&0xFF)<<8)|static_cast<unsigned>(p5&0xFF)|(killer?65536u:0u));
#ifndef NO_RUNTIME_LOG
    // 2026-11-20 one-round check of the 2801C0 fix: what 25A880 left of the commander (status +0xE6: 0 = deleted, 5 재야, 6 포로).
    if(Obj p=person_of(commander))log_event("band_dead_person",commander,valid(p)?p[0xE6]:255u,valid(p)?word(p,0xE2):0u);
#endif
    apply_shocks(shocks,n);
}
// 2026-11-20 user "정예는 … 최대 사기가 130으로": 230460 answers 130 for a live elite band (original = its force's cap, 22E690 = F).
static unsigned __fastcall hook_max_morale(Obj unit) {
    const unsigned cap=original.max_morale(unit);
    unsigned kind=0;
    if(unit!=face_unit && band_kind_of(unit,kind) && ccz_band_kinds::trait_of(kind)==ccz_band_kinds::kElite && cap<130)return 130;
    return cap;
}
// 2026-11-20 user "산월종수 이런애들은 등용창에 있으면 안되는데.. 포로나 그런거 되면 야전에서 즉시 사라져야해 아예 플레이어 소속이나
// 재야로 이렇게 튀어나오면 안돼" (local272 log, commit 342966f: band_dead p4 1 with a killer, then auto_search_officer found 4033/4035 as 재야). A band
// commander serves F's corps (22E450 = F, not 51), so 2801C0 let the destruction 25A880 capture him (2E3020) instead of taking the
// generic-deletion path (bVar2 -> 227D30). While his slot still holds the army (25A880 runs before the slot is cleared) the roll
// answers no: 25A880 then deletes him like any bandit generic, the killer keeps every other reward (morale, merit, message).
static bool __fastcall hook_capture(Obj person,Obj captor) {
    if(person && valid(person)) {
        const unsigned id=word(person,0x10),c=id-kFirstGeneric;
        if(c<kSlots && slots[c] && person_of(id)==person) {
            log_event("band_capture_refused",id,captor && valid(captor)?word(captor,0x10):0u,static_cast<unsigned>(static_cast<unsigned long long>(slots[c])>>56));
            return false;
        }
    }
    return original.capture(person,captor);
}
// bands->exchange (CALL 16B86C after 16B180): cavalry attacker by its terrain, rattan-armour target; poison/sorcery may confuse.
// The no-message sentinel 1A4AB88 (as the event commands 3AB0E0 pass it) keeps 2F0E50 from opening a notice.
static void __cdecl exchange(Obj from,Obj to,int *damage) {
    if(!(flags&kFlagOn) || !damage || *damage<=0)return;
    unsigned ak=0,tk=0;
    const unsigned af=band_kind_of(from,ak);band_kind_of(to,tk);
    if(!af)ak=0;
    const unsigned at=af?word(from,0x2A):kNoHex;Obj hex=hex_at(at);
    const float f=ccz_band_kinds::exchange_factor(ak,hex?hex[0x10]:0u,tk);
    if(f!=1.0f) {
        const int before=*damage;
        int after=static_cast<int>(static_cast<float>(before)*f+0.5f);
        if(after<1)after=1;
        if(after>9999)after=9999;
        *damage=after;
#ifndef NO_RUNTIME_LOG
        static volatile LONG lines;   // army = attacker commander, value = before << 16 | after, hex = attacker kind << 8 | target kind
        if(InterlockedIncrement(&lines)<=200)log_event("band_exchange",word(from,0x12),(static_cast<unsigned>(before)<<16)|(after&0xFFFF),(ak<<8)|tk);
#endif
    }
    if(af && (ccz_band_kinds::traits(ak)&ccz_band_kinds::kConfuse) && to && valid(to) && to[0x10]==1 && word(to,0x16) && ccz_combat::roll()<100) {
        const unsigned tf=band_force(to)?band_force(to):original.army_force(to)&255u;
        if(tf && tf!=af && hostile(tf,af)) {
            const unsigned before=to[0x23];
            fn<unsigned long long(__fastcall *)(Obj,Obj,unsigned,int,unsigned long long,Obj,int)>(GA(GA_2F0E50))(to,from,0,40,0,pointer(image,GA(GA_1A4AB88)),0);
            log_event("band_confuse",word(from,0x12),word(to,0x12),(before<<8)|to[0x23]);
        }
    }
}
// bands->fire (CALL 43EB3E -> 179DE0, an army standing in tile fire): rattan armour takes x3.
static int __cdecl fire(Obj army,int amount) {
    unsigned kind=0;
    if(!(flags&kFlagOn) || !band_kind_of(army,kind))return amount;
    const int after=ccz_band_kinds::fire_amount(kind,amount);
    if(after!=amount)log_event("band_fire",word(army,0x12),static_cast<unsigned>(amount),static_cast<unsigned>(after));
    return after;
}
// The `a` of 2AAF30's morale term (a + m) / b, chosen the way 2AAF30 chooses it (ccz_band_kinds.h power_factor).
static float morale_term_a() {
    Obj options=fn<Obj(__fastcall *)()>(GA(GA_3EC1A0))();
    if(fn<int(__fastcall *)(Obj)>(GA(GA_3ED7C0))(options))
        return fn<int(__fastcall *)(Obj)>(GA(GA_3EC480))(fn<Obj(__fastcall *)()>(GA(GA_3EC1A0))())==1?75.0f:200.0f;
    if(!fn<int(__fastcall *)(Obj)>(GA(GA_3ED810))(fn<Obj(__fastcall *)()>(GA(GA_3EC1A0))()))return 200.0f;
    Obj layout=fn<Obj(__fastcall *)(Obj)>(GA(GA_7AB6A0))(fn<Obj(__fastcall *)()>(GA(GA_3EC1A0))());
    int k=static_cast<int>(fn<unsigned(__fastcall *)(Obj,unsigned,unsigned)>(GA(GA_3EC920))(layout,12,0));
    if(k<0)k=100;
    if(k>200)k=200;
    return 200.0f-2.0f*static_cast<float>(k-100);
}
// bands->power: factor on 2AAF30 for a live band (morale curve + mountain x1.3), 1 for every other army.
static float __cdecl power(Obj army) {
    unsigned kind=0;
    if(!(flags&kFlagOn) || !band_kind_of(army,kind))return 1.0f;
    unsigned m=army[0x1A];
    const unsigned cap=hook_max_morale(army);if(m>cap)m=cap;   // the morale 2AAF30 used: min(+0x1A, 230460)
    const float a=morale_term_a(),f=ccz_band_kinds::power_factor(kind,m,a);
#ifndef NO_RUNTIME_LOG
    // army = commander, value = morale << 8 | a, hex = factor x 1000 (when it changes for that commander)
    static unsigned short seen[kSlots];
    const unsigned c=word(army,0x12)-kFirstGeneric,key=(m<<8)^static_cast<unsigned>(a);
    if(c<kSlots && seen[c]!=static_cast<unsigned short>(key+1)) {
        seen[c]=static_cast<unsigned short>(key+1);
        log_event("band_power",word(army,0x12),(m<<8)|(static_cast<unsigned>(a)&0xFF),static_cast<unsigned>(f*1000.0f));
    }
#endif
    return f;
}
static unsigned long long __fastcall hook_paint_write(Obj manager,Obj hex,unsigned owner,int a,unsigned b,int c,int d) {
    if(spawning) {++spawn_skips;return 0;}   // the spawned band keeps F's tile under it
    if(paint_force && hex) {
        unsigned now=hex[0x14];
        if(now==paint_force || (now!=0 && now<52 && !hostile(now,paint_force))) {++paint_refused;return 0;}
        ++paint_allowed;
    }
    return original.paint_write(manager,hex,owner,a,b,c,d);
}
static int __fastcall hook_morale(Obj unit,int linked) {
    if(band_force(unit)) {++morale_skips;return 0;}
    return original.morale(unit,linked);
}
static void __fastcall hook_set_action(Obj unit,int action) {
    if(!own_order && unit!=face_unit && band_force(unit)) {
#ifndef NO_RUNTIME_LOG
        note_foreign(_ReturnAddress(),1,static_cast<unsigned>(action),unit);
#endif
        return;
    }
    original.set_action(unit,action);
}
static void __fastcall hook_set_target(Obj unit,unsigned char kind,unsigned short target) {
    if(!own_order && unit!=face_unit && band_force(unit)) {
#ifndef NO_RUNTIME_LOG
        note_foreign(_ReturnAddress(),2,(static_cast<unsigned>(kind)<<16)|target,unit);
#endif
        return;
    }
    original.set_target(unit,kind,target);
}

// ---------------------------------------------------------------- day pass (day-frame thread, lock held)
// 2026-11-19 user "군량은 종군 소속 (플레이어 OR 컴퓨터세력) 의 군량을 쓰게끔": the 순 food settlement 2DEE10 charges an army to its
// commander's base (22BFE0 = person +0xE2), and the supply path ends there too (2AB620). A live band commander takes the host
// commander's base, so the host's city pays; the old value comes back when the record ends.
static Obj person_of(unsigned id) {return id && id<6000?table(0x158,id,6000):nullptr;}
static void sync_base(Band &b) {
    Obj me=person_of(b.commander),host=b.host?person_of(b.host_commander):nullptr;
    if(!me || !valid(me) || (b.host && (!host || !valid(host))))return;
    // 2026-11-20: a band waiting without a host eats from the base it waits at
    const unsigned want=b.host?word(host,0xE2):b.wait_base,now=word(me,0xE2);
    if(!want || want==now)return;
    if(!b.base_before)b.base_before=static_cast<unsigned short>(now?now:0xFFFF);
    *reinterpret_cast<unsigned short *>(me+0xE2)=static_cast<unsigned short>(want);
    log_event("band_food_base",b.commander,(now<<16)|want,b.host_commander);
}
static void restore_base(Band &b) {
    Obj me=person_of(b.commander);
    if(!b.base_before || !me || !valid(me))return;
    *reinterpret_cast<unsigned short *>(me+0xE2)=static_cast<unsigned short>(b.base_before==0xFFFF?0:b.base_before);
}
#ifndef NO_RUNTIME_LOG
// Diagnostic: why do band troops drop ("군량이 없어서 계속 병사가 탈출해")? army = commander, value = before << 16 | now,
// hex = unit +0x26 | base << 8 | base food / 100 (cap 65535) << 16.
static void watch_troops(Band &b,Obj u) {
    const unsigned now=word(u,0x16);
    if(b.seen_troops && now!=b.seen_troops) {
        static unsigned lines;
        if(lines++<300) {
            Obj me=person_of(b.commander);const unsigned base=me?word(me,0xE2):0;
            Obj city=base && base<=61?table(0xDD38,base,61):nullptr;
            unsigned food=city?*reinterpret_cast<unsigned *>(city+0x38)/100:0;if(food>65535)food=65535;
            log_event("band_troop_change",b.commander,(static_cast<unsigned>(b.seen_troops)<<16)|now,u[0x26]|((base&255)<<8)|(food<<16));
        }
    }
    b.seen_troops=static_cast<unsigned short>(now);
}
#endif
// local260 (crash exe+22B5CF, user: "불러오기 -> 의용병 무리가 플레이어 세력 -> 데모 플레이 ... 여기서 버그가 생기는거라고"):
// 주의 (+0x14C, 1..6) of the generic officers 4033..4100. A band commander serves the player's force, and the generic person
// 2D5F90 makes keeps 주의 0. When that force turns computer (demo play), 22B360 counts its officers (status +0xE6 2..4, force =
// corps +0xE0 -> corps +0x10, as 22E870) into a 6-slot table at 주의-1, and 주의 0 writes outside it. So a commander gets the patron
// ruler's 주의 at spawn, and every generic officer of a force 1..50 with 주의 outside 1..6 gets that force ruler's 주의 at load
// and once a game day (saves from local248~259). Force 51 (bandits) is left as it is.
static unsigned ruler_principle(unsigned force) {
    Obj f=force>=1 && force<=50?table(0xDF30,force,51):nullptr;
    Obj ruler=f && valid(f)?person_of(word(f,0x10)):nullptr;
    const unsigned p=ruler && valid(ruler)?ruler[0x14C]:0u;
    return p>=1 && p<=6?p:1u;
}
static unsigned person_force(Obj p) {
    Obj c=p[0xE0]<0x34?table(0xE0D0,p[0xE0],51):nullptr;
    return c && valid(c)?c[0x10]:0u;
}
static void guard_principles() {
    for(unsigned id=kFirstGeneric;id<kFirstGeneric+kSlots;++id) {
        Obj p=person_of(id);if(!p || !valid(p))continue;
        const unsigned principle=p[0x14C],status=p[0xE6];
        if((principle>=1 && principle<=6) || status<2 || status>4)continue;
        const unsigned force=person_force(p);
        if(force<1 || force>50)continue;
        const unsigned want=ruler_principle(force);
        p[0x14C]=static_cast<unsigned char>(want);
        log_event("band_principle_fix",id,(force<<16)|(status<<8)|principle,(want<<16)|word(p,0xE2));
    }
}
static void free_band(Band &b,unsigned why) {
    if(b.state==kLive)restore_base(b);
    if(b.state==kLive)slot_set(b.commander,nullptr,0,0);
    log_event("band_state",b.commander,why,(static_cast<unsigned>(b.force)<<16)|b.host_commander);
    b={};
}
// 0 = the host still leads F's army in the field; else why the band must go (same-day disband).
static unsigned host_why(const Band &b) {
    Obj h=unit_at(b.host_index);
    if(!h || h!=b.host || !valid(h) || h[0x10]!=1 || !word(h,0x16))return 1;
    if(word(h,0x12)!=b.host_commander)return 2;
    // why 3 (retreat order, act 0xB/0xC) is gone since local221: the user wants the band to walk home with a retreating host and
    // disband only when the host really enters (the unit leaves the table or stands on a city tile, why 1 / 5).
    if(original.army_force(h)!=b.force)return 4;
    unsigned tile=word(h,0x2A);Obj hex=hex_at(tile);
    unsigned facility=hex?word(hex,0x1A):0;
    if(!hex || (facility>=1 && facility<=51))return 5;   // off the map or on a city's seven tiles (own gates are walked through)
    return 0;
}
static bool unit_alive(const Band &b) {
    Obj u=unit_at(b.unit_index);
    return u && u==b.unit && valid(u) && word(u,0x12)==b.commander && word(u,0x16) && band_force(u)==b.force;
}
static void disband(Band &b,unsigned why) {
    Obj u=b.unit;
    if(unit_alive(b)) {
        own_order=true;
        fn<void(__fastcall *)(Obj,Obj,unsigned long long,int,int)>(GA(GA_25A880))(u,nullptr,0,0,4);
        fn<void(__fastcall *)(unsigned long long,unsigned long long,unsigned long long,unsigned long long)>(GA(GA_2901A0))(0,0,0,0);
        fn<void(__fastcall *)()>(GA(GA_276250))();
        own_order=false;
    }
    free_band(b,why);
}
// 2026-11-19 user "강에 생기는 경우가 있는데 이거 좀 이상하다": a band is created only on dry land. Terrain id = hex +0x10 (rows of
// world+0x73860): 1 평지, 2 가도, 3 모래, 5 숲, 8 저산, 9 중산, 11 산길. Not 4 습지, 6 밀림, 7 독늪, 10 고산, 12 여울, 13 강, 14 대하,
// 24 격류, the town/base rows 15~18, the impassable 19~23, nor a season row (ice/flood, 25 and up).
static bool dry_land(Obj hex) {
    if(!hex)return false;
    const unsigned t=hex[0x10];
    return t==1 || t==2 || t==3 || t==5 || t==8 || t==9 || t==11;
}
// A free land tile next to `around` (no facility, no army), the one nearest to `from`; kNoHex if none.
static unsigned near_tile(unsigned around,unsigned from) {
    unsigned cand[8];unsigned n=0;
    const int x=static_cast<int>(around%220),y=static_cast<int>(around/220);
    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
        int nx=x+dx,ny=y+dy;if(nx<0 || ny<0 || nx>=220 || ny>=220 || n>=8)continue;
        unsigned t=static_cast<unsigned>(ny*220+nx);
        if(hex_distance(t,around)!=1)continue;
        Obj hex=hex_at(t);if(!hex || !word(hex,0x12) || word(hex,0x1A))continue;
        cand[n++]=t;
    }
    bool used[8]={};
    for(unsigned i=0;i<=500 && n;++i) {
        Obj u=unit_at(i);if(!u || !valid(u) || (u[0x10]!=1 && u[0x10]!=2))continue;
        unsigned t=word(u,0x2A);
        for(unsigned k=0;k<n;++k)if(cand[k]==t && hex_distance(t,from)!=0)used[k]=true;
    }
    unsigned best=kNoHex,best_d=~0u;
    for(unsigned k=0;k<n;++k)if(!used[k]) {unsigned d=hex_distance(cand[k],from);if(d<best_d) {best_d=d;best=cand[k];}}
    return best;
}
static void order(Band &b,int action,unsigned char kind,unsigned short target,unsigned why) {
    Obj u=b.unit;unsigned pending=u[0x140];
    own_order=true;
    fn<void(__fastcall *)(Obj,int)>(GA(GA_2ECEA0))(u,action);
    fn<void(__fastcall *)(Obj,unsigned char,unsigned short)>(GA(GA_2ECF00))(u,kind,target);
    fn<void(__fastcall *)(Obj)>(GA(GA_26E260))(u);
    own_order=false;
    b.last_kind=kind;b.last_target=target;
    log_event("band_order",b.commander|(pending?65536u:0u),(why<<16)|(static_cast<unsigned>(action)<<8)|kind,target);
    if(u[0x38]!=kind || word(u,0x3A)!=target)log_event("band_order_miss",b.commander,u[0x38],word(u,0x3A));
}
// ---- 2026-11-18 바싹 따라붙기 (user: "종군 부대가 이동 찍으면 종군 무리도 그 경로를 자기 기준에서 똑같이 찍어서", local221 lagged a day:
// orders went out only on the first move step of a day and aimed at the host's current tile). Now every move step (12 a day) looks at the
// host's goal; when it changes the band walks to the matching tile next to the host's goal, through the host's own waypoints.
// Cube coordinates of the offset layout hex_distance uses (x = column, z = row - (x + (x & 1)) / 2).
static void to_cube(unsigned t,int &x,int &z) {x=static_cast<int>(t%220);z=static_cast<int>(t/220)-(x+(x&1))/2;}
static unsigned from_cube(int x,int z) {
    if(x<0 || x>=220)return kNoHex;
    const int row=z+(x+(x&1))/2;
    return row<0 || row>=220?kNoHex:static_cast<unsigned>(row*220+x);
}
struct Occupied {unsigned short tile[512];unsigned n;};   // tiles of every field army except `self`, one pass per decision
static void occupied(Occupied &o,Obj self) {
    o.n=0;
    for(unsigned i=0;i<=500 && o.n<512;++i) {
        Obj u=unit_at(i);if(!u || u==self || !valid(u) || (u[0x10]!=1 && u[0x10]!=2))continue;
        o.tile[o.n++]=word(u,0x2A);
    }
}
static bool free_land(unsigned t,const Occupied &o) {   // a land tile with an area, no facility, no other army
    Obj hex=hex_at(t);if(!hex || !word(hex,0x12) || word(hex,0x1A))return false;
    for(unsigned i=0;i<o.n;++i)if(o.tile[i]==t)return false;
    return true;
}
// Building table tile of the host's goal: city 5 (id 1~51), gate 6 (id 1~10 = base 52~61, as native_adjacency.inl reads target records),
// facility 0xB (building id). kNoHex if none.
static unsigned base_tile(unsigned kind,unsigned id) {
    if(kind==6){if(!id || id>10)return kNoHex;id+=51;}
    else if(kind==5 && (!id || id>51))return kNoHex;
    Obj b=id>=1 && id<3000?table(0x6DA98,id,3000):nullptr;
    return b && word(b,0x10)<kNoHex?word(b,0x10):kNoHex;
}
// The band's tile for a host walking to `goal`: the band's present offset from the host when it is 1~2 hexes, else the free tile around
// the goal nearest to the host (ring 1 for a tile; ring 2~3 for a base, whose seven tiles all carry its id). kNoHex if nothing is free.
static unsigned mirror_tile(unsigned band_tile,unsigned host_tile,unsigned goal,bool base,const Occupied &o) {
    if(!base) {
        const unsigned d=hex_distance(band_tile,host_tile);
        if(d>=1 && d<=2) {
            int ux,uz,hx,hz,gx,gz;to_cube(band_tile,ux,uz);to_cube(host_tile,hx,hz);to_cube(goal,gx,gz);
            const unsigned t=from_cube(gx+ux-hx,gz+uz-hz);
            if(t<kNoHex && t!=goal && free_land(t,o))return t;
        }
    }
    const unsigned r0=base?2u:1u,r1=base?3u:2u;
    unsigned best=kNoHex,best_score=~0u;
    const int gx0=static_cast<int>(goal%220),gy0=static_cast<int>(goal/220);
    for(int dy=-4;dy<=4;++dy)for(int dx=-3;dx<=3;++dx) {
        const int x=gx0+dx,y=gy0+dy;if(x<0 || y<0 || x>=220 || y>=220)continue;
        const unsigned t=static_cast<unsigned>(y*220+x),d=hex_distance(t,goal);
        if(d<r0 || d>r1 || !free_land(t,o))continue;
        const unsigned score=d*1000+hex_distance(t,host_tile);   // nearest ring first, then the host's side
        if(score<best_score){best_score=score;best=t;}
    }
    return best;
}
static void order_move(Band &b,unsigned tile,const unsigned short *relay,unsigned why) {
    Obj u=b.unit;unsigned pending=u[0x140];
    own_order=true;
    fn<void(__fastcall *)(Obj,int)>(GA(GA_2ECEA0))(u,1);
    fn<void(__fastcall *)(Obj,unsigned char,unsigned short)>(GA(GA_2ECF00))(u,9,static_cast<unsigned short>(tile));
    if(relay) {   // the host's own waypoints (2F05A0 writes +0x2C..+0x34; 0xBD10 = none), before the route is computed
        auto set_relay=fn<void(__fastcall *)(Obj,unsigned,int)>(GA(GA_2F05A0));
        for(unsigned i=0;i<5;++i)set_relay(u,i,relay[i]?static_cast<int>(relay[i]):static_cast<int>(kNoHex));
    }
    fn<void(__fastcall *)(Obj)>(GA(GA_26E260))(u);
    own_order=false;
    b.last_kind=9;b.last_target=static_cast<unsigned short>(tile);
    log_event("band_order",b.commander|(pending?65536u:0u),(why<<16)|(1u<<8)|9u,tile);
    if(u[0x38]!=9 || word(u,0x3A)!=tile)log_event("band_order_miss",b.commander,u[0x38],word(u,0x3A));
#ifndef NO_RUNTIME_LOG
    if(relay)log_event("band_relay_after",(static_cast<unsigned>(word(u,0x2C))<<16)|word(u,0x2E),(static_cast<unsigned>(word(u,0x30))<<16)|word(u,0x32),word(u,0x34));
#endif
}
// Band formation = host's land formation: the day's movement score is the formation mobility (rules_movement), so a band in another
// formation drifts behind. 242EF0 itself refuses water formations (record +0x7E == 2).
// 2026-11-19 user "충차? 투석기? 이런 공성병기로 의용병 수거하니까 진형이 안따라가던데": siege 8 정란 / 9 충차 / 10 투석 (rules_combat
// formation table) follow as well; they were left out (1~7 only), so the band kept e.g. 봉시 (log: host 10, band 2).
// 2026-11-20 user "산악은 장사진 고정": mountain kinds keep 장사 (6) whatever the host does (the original already gives 장사 the
// rough-ground 1.5 of 2AAF30); a band waiting without a host keeps its formation.
static void match_formation(Band &b) {
    const bool mountain=ccz_band_kinds::trait_of(b.kind)==ccz_band_kinds::kMountain;
    if(!mountain && !b.host)return;
    const unsigned want=mountain?6u:b.host[0x1B],had=b.unit[0x1B];
    if(want<1 || want>10 || want==had)return;
    fn<void(__fastcall *)(Obj,unsigned)>(GA(GA_242EF0))(b.unit,want);
    log_event("band_formation",b.commander,(had<<8)|want,b.unit[0x1B]);
}
// 2026-11-20 user "수군은 그냥 수상부대를 누선으로 업그레이드": water kinds sail as 누선 (water formation +0x1C = 13, original setter
// 2436C0 as 2D53F0 uses it). Checked at spawn and once a day (a load or the original may put another ship back).
static void keep_ships(Band &b) {
    if(ccz_band_kinds::trait_of(b.kind)!=ccz_band_kinds::kNavy || b.unit[0x1C]==13)return;
    const unsigned had=b.unit[0x1C];
    fn<void(__fastcall *)(Obj,unsigned)>(GA(GA_2436C0))(b.unit,13);
    log_event("band_ships",b.commander,had,b.unit[0x1C]);
}
// 2026-11-20 user "의용병 무리 시작 사기를 60으로": through the original morale change 271E70 (cap and rout stay original).
static void start_morale(Band &b) {
    const int now=b.unit[0x1A];
    if(now!=60)fn<void(__fastcall *)(Obj,int,int,int)>(GA(GA_271E70))(b.unit,60-now,0,1);
    log_event("band_start_morale",b.commander,static_cast<unsigned>(now),b.unit[0x1A]);
}
static void follow(Band &b,bool fresh) {
    Obj u=b.unit,h=b.host;
    unsigned enemy=0;
    if(h[0x38]==0x1B)enemy=word(h,0x3A);else if(h[0x3C]==0x1B)enemy=word(h,0x3E);
    if(enemy) {
        Obj e=unit_at(enemy);
        if(!e || !valid(e) || !word(e,0x16) || band_force(e)==b.force)enemy=0;
        else {unsigned ef=fn<unsigned char(__fastcall *)(Obj)>(GA(GA_22E690))(e);if(ef==b.force || !hostile(ef,b.force))enemy=0;}
    }
    const unsigned kind=u[0x38],target=word(u,0x3A);
    if(enemy) {
        b.dest=static_cast<unsigned short>(kNoHex);
        if(fresh || kind!=0x1B || target!=enemy)order(b,7,0x1B,static_cast<unsigned short>(enemy),fresh?1:2);
        return;
    }
    const unsigned ut=word(u,0x2A),ht=word(h,0x2A),dist=hex_distance(ut,ht);
    // the host walks to a tile or a base: the band walks to the matching tile next to that goal (once per host goal)
    const unsigned gk=h[0x38],gt=word(h,0x3A);
    const bool base=gk==5 || gk==6 || gk==0xB;
    const unsigned goal=gk==9?(gt<kNoHex?gt:kNoHex):base?base_tile(gk,gt):kNoHex;
    if(goal<kNoHex && hex_distance(ht,goal)>(base?1u:0u)) {
        const bool same=b.goal_kind==gk && b.goal_target==gt && b.dest<kNoHex;
        if(!fresh && same && (ut==b.dest || (kind==9 && target==b.dest)))return;
        Occupied o;occupied(o,u);
        const unsigned tile=same && free_land(b.dest,o)?b.dest:mirror_tile(ut,ht,goal,base,o);
        if(tile<kNoHex) {
            unsigned short relay[5];bool any=false;
            for(unsigned i=0;i<5;++i){unsigned r=word(h,0x2C+2*i);relay[i]=static_cast<unsigned short>(r<kNoHex?r:0);any|=relay[i]!=0;}
            b.goal_kind=static_cast<unsigned char>(gk);b.goal_target=static_cast<unsigned short>(gt);b.dest=static_cast<unsigned short>(tile);
            order_move(b,tile,any?relay:nullptr,fresh?3:(same?7:6));
            return;
        }
        log_event("band_order_miss",b.commander,0xFE,goal);
    }
    b.dest=static_cast<unsigned short>(kNoHex);b.goal_kind=0;
    // the host stands (or nothing is free near its goal): stay next to it
    const bool ours=kind==b.last_kind && target==b.last_target;
    if(!fresh && dist<=2 && (ours || kind==0))return;
    unsigned tile=near_tile(ht,ut);
    if(tile>=kNoHex) {log_event("band_order_miss",b.commander,0xFF,ht);return;}
    if(!fresh && kind==9 && target==tile)return;
    order(b,1,9,static_cast<unsigned short>(tile),fresh?3:(dist>2?4:5));
}
// Every move step (12 a day): a host goal, chase or formation change re-plans the band at once (call = move step of the day).
static void track_hosts(unsigned call) {
    (void)call;   // log only (release build has no log_event)
    for(Band &b:bands) {
        if(b.state!=kLive || !unit_alive(b) || host_why(b))continue;   // freed / disbanded by the day checks
        Obj h=b.host;
        const unsigned gk=h[0x38],gt=word(h,0x3A),ck=h[0x3C],ct=word(h,0x3E),form=h[0x1B];
        if(gk==b.seen_goal_kind && gt==b.seen_goal && ck==b.seen_chase_kind && ct==b.seen_chase && form==b.seen_form)continue;
#ifndef NO_RUNTIME_LOG
        static volatile LONG goal_logs;
        if(InterlockedIncrement(&goal_logs)<=600) {
            log_event("band_host_goal",b.commander|(call<<16),(gk<<16)|gt,(static_cast<unsigned>(h[0x140])<<24)|((ck&0xFF)<<16)|ct);
            log_event("band_host_relay",(static_cast<unsigned>(word(h,0x2C))<<16)|word(h,0x2E),(static_cast<unsigned>(word(h,0x30))<<16)|word(h,0x32),
                      word(h,0x34)|(form<<16)|(static_cast<unsigned>(b.unit[0x1B])<<24));
        }
#endif
        b.seen_goal_kind=static_cast<unsigned char>(gk);b.seen_goal=static_cast<unsigned short>(gt);
        b.seen_chase_kind=static_cast<unsigned char>(ck);b.seen_chase=static_cast<unsigned short>(ct);b.seen_form=static_cast<unsigned char>(form);
        match_formation(b);
        follow(b,false);
    }
}
// ---- 2026-11-20 user "편입 … 퇴각해도 성앞에서 대기하는걸로 하자. 또 장수가 출진하면 종군하는거야. 이때 종군상태를 해제하면 안되고
// 종군세력번호를 그대로 유지한채 있어야해. 새 장수가 오면 종군 장수만 갈아끼우면 되게끔": a band that loses its host (the host entered
// a base, was destroyed, changed commander or force) stays a live band of F (slot, 22E690 answer, F's 10-band count) with no host,
// walks next to a base of F and waits there; F's base pays its food. The first army of F (not led by a generic officer, not already
// leading a band) within 5 hexes becomes the new host. Only F gone (no ruler) or F without any base still disbands the band.
static bool force_alive(unsigned f) {
    Obj fo=f>=1 && f<=45?table(0xDF30,f,51):nullptr;
    return fo && valid(fo) && word(fo,0x10)!=0;
}
static unsigned base_hex(unsigned id) {   // building table tile of base 1..61 (cities 1..51, gates 52..61), kNoHex if none
    Obj b=id>=1 && id<=61?table(0x6DA98,id,3000):nullptr;
    return b && word(b,0x10)<kNoHex?word(b,0x10):kNoHex;
}
static bool base_of(unsigned id,unsigned force) {
    Obj hex=hex_at(base_hex(id));
    return hex && hex[0x14]==force;
}
static unsigned nearest_base(unsigned force,unsigned from) {
    unsigned best=0,best_d=~0u;
    for(unsigned id=1;id<=61;++id) {
        if(!base_of(id,force))continue;
        const unsigned d=hex_distance(base_hex(id),from);
        if(d<best_d){best_d=d;best=id;}
    }
    return best;
}
static void wait_move(Band &b) {
    const unsigned t=base_hex(b.wait_base),ut=word(b.unit,0x2A);
    if(t>=kNoHex)return;
    const unsigned d=hex_distance(ut,t);
    if(d>=2 && d<=3 && (b.dest>=kNoHex || ut==b.dest))return;   // already waiting beside the base
    const unsigned kind=b.unit[0x38],target=word(b.unit,0x3A);
    if(b.dest<kNoHex && kind==9 && target==b.dest)return;          // on its way
    Occupied o;occupied(o,b.unit);
    const unsigned tile=mirror_tile(ut,ut,t,true,o);
    if(tile>=kNoHex){log_event("band_order_miss",b.commander,0xFD,t);return;}
    b.dest=static_cast<unsigned short>(tile);
    order_move(b,tile,nullptr,8);
}
// 2026-11-20 user "퇴각눌렀을때 사라져야하는데 성앞에 대기하네? 청주병,서량기병에 한해서 성앞에 대기하기로 한거 아니었어?": only the
// 편입 kinds (ccz_band_kinds.h kWaits: 청주병 1, 서량기병 25) wait; every other kind disbands with the host as before local271.
static void lose_host(Band &b,unsigned why) {
    const unsigned f=b.force;
    if(!ccz_band_kinds::waits_at_base(b.kind) || !force_alive(f)){disband(b,why);return;}
    unsigned base=0;
    if(Obj h=why==5?b.host:nullptr) {   // the host stands on a base's tiles
        Obj hex=hex_at(word(h,0x2A));const unsigned id=hex?word(hex,0x1A):0u;
        if(id>=1 && id<=61 && base_of(id,f))base=id;
    }
    if(!base && (b.seen_goal_kind==5 || b.seen_goal_kind==6)) {   // the host's last goal (a retreat or march into a base)
        const unsigned id=b.seen_goal_kind==6?b.seen_goal+51u:b.seen_goal;
        if(id>=1 && id<=61 && base_of(id,f))base=id;
    }
    if(!base)base=nearest_base(f,word(b.unit,0x2A));
    if(!base){disband(b,10);return;}   // why 10: F holds no base
    log_event("band_wait",b.commander,(why<<8)|base,(f<<16)|b.host_commander);
    b.host=nullptr;b.host_index=0xFFFF;b.host_commander=0;b.wait_base=static_cast<unsigned short>(base);
    b.dest=static_cast<unsigned short>(kNoHex);b.goal_kind=0;
    sync_base(b);
    wait_move(b);
}
static bool hosts_band(Obj u);
// Every move step: a waiting band takes the nearest army of F within 5 hexes as its new host.
static void rehost(Band &b) {
    Obj best=nullptr;unsigned best_d=6;
    const unsigned ut=word(b.unit,0x2A);
    for(unsigned i=0;i<=500;++i) {
        Obj u=unit_at(i);if(!u || !valid(u) || u[0x10]!=1 || !word(u,0x16) || word(u,0x12)>=kFirstGeneric)continue;
        const unsigned d=hex_distance(ut,word(u,0x2A));if(d>=best_d)continue;
        if(original.army_force(u)!=b.force || hosts_band(u))continue;
        Obj hex=hex_at(word(u,0x2A));const unsigned facility=hex?word(hex,0x1A):0;
        if(!hex || (facility>=1 && facility<=51))continue;   // still on a city's tiles: host_why would drop it at once
        best=u;best_d=d;
    }
    if(!best)return;
    Obj h=best;
    b.host=h;b.host_index=static_cast<unsigned short>(unit_index(h));b.host_commander=word(h,0x12);b.wait_base=0;
    b.dest=static_cast<unsigned short>(kNoHex);b.goal_kind=0;
    b.seen_goal_kind=h[0x38];b.seen_goal=word(h,0x3A);b.seen_chase_kind=h[0x3C];b.seen_chase=word(h,0x3E);b.seen_form=h[0x1B];
    log_event("band_rehost",b.commander,(b.force<<16)|best_d,b.host_commander);
    sync_base(b);
    match_formation(b);
    follow(b,true);
}
// Original game lists (same layout as the convenience DLL's GameList in native_core.inl): {vtable, handle}; the nodes live in the
// manager at 27D4138/27D4140, allocation through 18D50 under the 27D4108 lock when 27D4100 is set.
struct GameLink {Obj value;GameLink *next;};
struct GameList {Obj vtable;unsigned *handle;};
static GameLink *list_head(const GameList &list) {
    if(!list.handle || !pointer(image,GA(GA_27D4138)) || *list.handle>=*reinterpret_cast<unsigned *>(image+GA(GA_27D4170)))return nullptr;
    Obj heads=pointer(image,GA(GA_27D4140));
    return heads?*reinterpret_cast<GameLink **>(heads+*list.handle*8):nullptr;
}
static bool list_push(GameList &list,Obj value) {
    if(!list.handle || !value)return false;
    const bool locked=*reinterpret_cast<unsigned *>(image+GA(GA_27D4100))!=0;
    if(locked)EnterCriticalSection(reinterpret_cast<CRITICAL_SECTION *>(image+GA(GA_27D4108)));
    Obj entry=fn<Obj(__fastcall *)(Obj,unsigned,unsigned char)>(GA(GA_18D50))(image+GA(GA_27D4130),*list.handle,1);
    if(entry)*reinterpret_cast<Obj *>(entry)=value;
    if(locked)LeaveCriticalSection(reinterpret_cast<CRITICAL_SECTION *>(image+GA(GA_27D4108)));
    return entry!=nullptr;
}
// 2026-11-19 user "종군하면 클릭해서 명령 못내려야하는데 ... 명령창 자체를 막아야해" / "그냥 플레이어가 동맹부대를 클릭하듯이 그렇게 막으면
// 안되나?": the map click (459D30) and the strategy update (44ED30) both open the command window through 4522B0(state), which turns
// the selection list at state+0x620 (filled by 2AFC80 with the clicked army's linked armies) into CStrategyUnitTargetState. A band
// came in through that list (log: a 최속 진군 order for band 4036 from the window, refused only at the setters). Bands leave the list
// here; a selection of bands only opens nothing, as a click on an army outside the player's direct corps never reaches 4522B0.
static void __fastcall hook_unit_target(Obj state) {
    if(state) {
        GameList &picked=*reinterpret_cast<GameList *>(state+0x620);
        Obj keep[64];unsigned kept=0,dropped=0,over=0;
        for(GameLink *l=list_head(picked);l;l=l->next) {
            if(!l->value)continue;
            if(band_force(l->value)){++dropped;continue;}
            if(kept<64)keep[kept++]=l->value;else ++over;
        }
        if(dropped) {
            log_event("band_command_refused",dropped,kept,over);
            if(!kept)return;
            if(!over && picked.handle) {
                const bool locked=*reinterpret_cast<unsigned *>(image+GA(GA_27D4100))!=0;
                if(locked)EnterCriticalSection(reinterpret_cast<CRITICAL_SECTION *>(image+GA(GA_27D4108)));
                fn<void(__fastcall *)(Obj,unsigned)>(GA(GA_186D0))(image+GA(GA_27D4130),*picked.handle);   // the list's nodes (37DF00)
                if(locked)LeaveCriticalSection(reinterpret_cast<CRITICAL_SECTION *>(image+GA(GA_27D4108)));
                for(unsigned i=0;i<kept;++i)list_push(picked,keep[i]);
            }
        }
    }
    original.unit_target(state);
}
// The dry-land 2AA9E0 candidate (hex object) nearest to `site`, at most `reach` hexes away; kNoHex when none.
static unsigned pick_near(const GameList &list,unsigned site,int reach) {
    Obj seen[512];unsigned n=0;
    for(GameLink *l=list_head(list);l && n<512;l=l->next)if(l->value)seen[n++]=l->value;
    unsigned best=kNoHex,best_d=static_cast<unsigned>(reach)+1;
    const int x=static_cast<int>(site%220),y=static_cast<int>(site/220);
    for(int dy=-reach-1;dy<=reach+1;++dy)for(int dx=-reach;dx<=reach;++dx) {
        int nx=x+dx,ny=y+dy;if(nx<0 || ny<0 || nx>=220 || ny>=220)continue;
        unsigned t=static_cast<unsigned>(ny*220+nx),d=hex_distance(t,site);if(d>=best_d)continue;
        Obj hex=hex_at(t);if(!dry_land(hex))continue;
        for(unsigned k=0;k<n;++k)if(seen[k]==hex) {best=t;best_d=d;break;}
    }
    return best;
}
// The 2AA9E0 candidate of region `area` beside its 거점 center: 1~2 hexes away (3 only when none), then the far side from the nearest
// hostile base (local218: a record toward the enemy was taken by a passing enemy army first; any force can host), then nearer the
// center. kNoHex when none.
static unsigned pick_around(const GameList &list,unsigned center,unsigned enemy_tile,unsigned area) {
    Obj seen[512];unsigned n=0;
    for(GameLink *l=list_head(list);l && n<512;l=l->next)if(l->value)seen[n++]=l->value;
    unsigned best=kNoHex,best_outer=0,best_e=0,best_d=0;   // (no "far": a Windows header macro)
    const int x=static_cast<int>(center%220),y=static_cast<int>(center/220);
    for(int dy=-4;dy<=4;++dy)for(int dx=-3;dx<=3;++dx) {
        int nx=x+dx,ny=y+dy;if(nx<0 || ny<0 || nx>=220 || ny>=220)continue;
        unsigned t=static_cast<unsigned>(ny*220+nx),d=hex_distance(t,center);if(d<1 || d>3)continue;
        Obj hex=hex_at(t);if(!dry_land(hex) || word(hex,0x12)!=area)continue;
        bool listed=false;for(unsigned k=0;k<n && !listed;++k)listed=seen[k]==hex;
        if(!listed)continue;
        const unsigned outer=d>2?1u:0u,e=hex_distance(t,enemy_tile);
        if(best>=kNoHex || outer<best_outer || (outer==best_outer && (e>best_e || (e==best_e && d<best_d)))) {best=t;best_outer=outer;best_e=e;best_d=d;}
    }
    return best;
}
static unsigned spawn_face(unsigned kind);   // flag portrait section below
// 2026-11-19 user "설치기에서 의용병 무리 최대병력을 300~500 부터 3000~5000 선택할 수 있게": [ccz] band_troops 1/2/3 = 300->3000 /
// 400->4000 / 500->5000 (default), handed over in configure() flags bits 8..11. Set once before any band exists.
static unsigned idle_low=500,idle_full=5000;
// 2026-11-20 kind traits (ccz_band_kinds.h): full troops by kind (rabble 130%, elite 70%), days to full by kind (30, rabble 21).
// The 인덕 speed-up by the ruler's 매력 was removed the same day (user "매력으로 빨리 소집되는건 지워줘").
static int idle_age(const Band &b,int day) {return day>=b.created && b.created>=0?day-b.created:0;}
static unsigned idle_top(const Band &b) {
    const unsigned top=(idle_full*ccz_band_kinds::troop_cap_pct(b.kind)/100u+50u)/100u*100u;
    return top>idle_low?top:idle_low+100u;
}
static unsigned idle_troops(const Band &b,int day) {
    const int age=idle_age(b,day);
    const unsigned top=idle_top(b),days=ccz_band_kinds::growth_days(b.kind);
    return idle_low+(static_cast<unsigned>(age)>=days?top-idle_low:static_cast<unsigned>(age)*(top-idle_low)/days/100u*100u);
}
// 2026-11-19 user "종군하면 치안이 병사수에 비례해서 최대 30까지 차감" / "5000명 되고도 회수 안해가면 그때부터 치안이 한달마다 10씩
// 감소되서 도적화" / "치안이 60이하면 도적화": the region (+0x54 security, u8 0..100) of a record pays. Written on the game thread
// (the day pass), the way 175DE0 and the stratagems write it.
static Obj site_area(unsigned site) {Obj hex=hex_at(site);const unsigned a=hex?word(hex,0x12):0;return a?table(0x6CAF0,a,500):nullptr;}
static void cut_security(unsigned site,unsigned amount,unsigned why) {
    Obj area=site_area(site);if(!area || !valid(area) || !amount)return;
    const unsigned before=area[0x54],after=before>amount?before-amount:0;
    area[0x54]=static_cast<unsigned char>(after);
    log_event("band_security",site,(before<<8)|after,why);   // hex: 1 join, 2 a month full and not taken
}
// 2026-11-19 flag portrait (user: "거점옆에 생기는데 부대깃발 초상화가 바뀌질 않네"): the map flag keeps the face its commander wore when
// 2D53F0 made the army (local243 log: band_face_set 1011->6199 right after the spawn, and 8AD530 never saw face 6199 afterwards).
// 2D5F90 copies +0xA6 from template 4031 or 4032 (random) for a force-51 generic, so both templates wear the portrait face for the
// create call only and get theirs back on every exit.
struct TemplateFace {
    Obj person[2]={};unsigned short face[2]={};
    void wear(unsigned short f) {
        for(unsigned i=0;i<2;++i) {
            person[i]=table(0x158,4031+i,6000);if(!person[i])continue;
            face[i]=static_cast<unsigned short>(word(person[i],0xA6));*reinterpret_cast<unsigned short *>(person[i]+0xA6)=f;
        }
    }
    ~TemplateFace() {for(unsigned i=0;i<2;++i)if(person[i])*reinterpret_cast<unsigned short *>(person[i]+0xA6)=face[i];}
};
static Obj spawn_unit(Band &b,unsigned force,unsigned &why,bool keep_tile=true) {
    Obj hex=hex_at(b.site);unsigned area_id=hex?word(hex,0x12):0;
    Obj area=area_id?table(0x6CAF0,area_id,500):nullptr;
    if(!area) {why=1;return nullptr;}
    GameList list={image+GA(GA_1346670),nullptr};
    fn<void(__fastcall *)(GameList *)>(GA(GA_1AC60))(&list);
    unsigned candidates=fn<unsigned(__fastcall *)(Obj,GameList *)>(GA(GA_2AA9E0))(area,&list);
    // 2D53F0 takes the first candidate in its own order, anywhere in the area (local217 test: the band showed up far from the record).
    // Hand it a one-tile list instead: path 0 = the dry candidate nearest to the record site within 3 hexes, 1 = within 8, 3 = the site
    // itself when it is dry land (also after a refused path 0/1 tile; 2D53F0 rolls its officer back when the tile has no 28C8C0 target,
    // so a second call is clean). 2026-11-19: the whole list is no longer handed over (it holds river tiles); no dry tile = no band
    // today, the record waits for the next day.
    // The site is not a current 2AA9E0 candidate: path 3 only when it is dry and free (no facility, no army; a host may stand on it).
    Occupied taken;occupied(taken,nullptr);
    const bool site_free=dry_land(hex) && free_land(b.site,taken);
    unsigned picked=pick_near(list,b.site,3),path=0;
    if(picked>=kNoHex){picked=pick_near(list,b.site,8);path=1;}
    if(picked>=kNoHex && site_free){picked=b.site;path=3;}
    auto create=fn<Obj(__fastcall *)(Obj,int,GameList *,int)>(GA(GA_2D53F0));
    auto try_tile=[&](unsigned t)->Obj {
        GameList one={image+GA(GA_1346670),nullptr};
        fn<void(__fastcall *)(GameList *)>(GA(GA_1AC60))(&one);
        Obj made=nullptr;
        if(list_push(one,hex_at(t))) {
            TemplateFace faces;const unsigned face=spawn_face(b.kind);if(face)faces.wear(static_cast<unsigned short>(face));
            spawning=keep_tile;made=create(area,1,&one,static_cast<int>(kBandit));spawning=false;   // a bandit (keep_tile false) paints its tile
        }
        if(one.handle)fn<void(__fastcall *)(GameList *)>(GA(GA_1BFFF0))(&one);
        return made;
    };
    spawn_skips=0;
    Obj u=picked<kNoHex?try_tile(picked):nullptr;
    if(!u && path<3 && picked<kNoHex && picked!=b.site && site_free){picked=b.site;path=3;u=try_tile(picked);}
    fn<void(__fastcall *)(GameList *)>(GA(GA_1BFFF0))(&list);
    log_event("band_spawn",u?word(u,0x12):0,(force<<24)|(area_id<<8)|(candidates&0xFF),spawn_skips);
    log_event("band_spawn_tile",b.site,(path<<16)|(picked&0xFFFF),u?word(u,0x2A):kNoHex);
    if(!u || !valid(u)) {why=2;return nullptr;}
    // army = commander, value = the face 2D5F90 gave him << 16 | the portrait face the templates wore (0 = none), hex = terrain of his tile
    Obj person=table(0x158,word(u,0x12),6000);Obj at=hex_at(word(u,0x2A));
    log_event("band_spawn_face",word(u,0x12),((person?word(person,0xA6):65535u)<<16)|spawn_face(b.kind),at?at[0x10]:255u);
    return u;
}
// ---------------------------------------------------------------- flag portrait (2026-11-18; both picture modes since 2026-11-19)
// User: "조조전 꺼져있으면 부대깃발 초상화에 조조전 스프라이트 ... 원작 3d모델 쓰면서". A band commander of kind k (1~28) wears face
// ccz_portrait::face_of(k) (6187~6214, the last 28 faces of the base 0005/002.bin, no scenario uses them; data_formats_archives.md
// 1.4). The flag (1BA4E0 -> 932DD0, kind 10) and the other face views load that face through 8ACF20 -> 8AD530(manager, zp1, image
// id, p4); while a live band of kind k wears it, hook_face_texture hands 8AD530 the CCZ class picture (ccz_portrait.h) instead.
// A face another officer already wears stays his: that kind keeps the template face. Without kFlagPortrait a portrait face goes back
// to the template face (the convenience DLL sets the flag in both picture modes since 2026-11-19).
static Obj faces_world;
static bool face_taken[ccz_band_kinds::kKinds];
static Obj ids_manager;
static int face_ids[ccz_band_kinds::kKinds][3];   // image id of face_of(k) for face kinds 9/10/11 (8AA550), 0 = not registered
static int portrait_asset;                        // 0 not tried, 1 CCZ pictures loaded, 2 failed
static void scan_faces() {
    Obj root=world();if(root==faces_world)return;
    faces_world=root;memset(face_taken,0,sizeof(face_taken));
    if(!root)return;
    unsigned mask=0;
    for(unsigned id=0;id<6000;++id) {
        if(id-kFirstGeneric<kSlots)continue;   // band commanders
        Obj p=table(0x158,id,6000);if(!p)continue;
        const unsigned k=ccz_portrait::kind_of_face(word(p,0xA6));
        if(k && k<ccz_band_kinds::kKinds && valid(p)){face_taken[k]=true;mask|=1u<<k;}
    }
    log_event("band_face_scan",mask,flags&kFlagPortrait?1u:0u);
}
// The portrait face a new band commander of `kind` gets (the rule apply_face follows), 0 = none (portrait off, or another officer wears it).
static unsigned spawn_face(unsigned kind) {
    if(!(flags&kFlagPortrait) || !kind || kind>=ccz_band_kinds::kKinds)return 0;
    const unsigned face=ccz_portrait::face_of(kind);if(!face)return 0;
    scan_faces();
    return face_taken[kind]?0u:face;
}
// Kind of the band being created (0 = none): from the create call until its slot is written, a face texture request for that kind's
// portrait face counts as worn (the flag made inside 2D53F0, or right after it, must get the CCZ picture before worn() can see the slot).
static volatile LONG spawn_kind;
struct SpawnKind {
    explicit SpawnKind(unsigned kind) {InterlockedExchange(&spawn_kind,spawn_face(kind)?static_cast<LONG>(kind):0);}
    ~SpawnKind() {InterlockedExchange(&spawn_kind,0);}
};
static void apply_face(unsigned commander,Obj person,unsigned kind) {
    if(!person)return;
    const unsigned now=word(person,0xA6),mine=ccz_portrait::face_of(kind);
    bool portrait=false;
    if((flags&kFlagPortrait) && mine && kind<ccz_band_kinds::kKinds){scan_faces();portrait=!face_taken[kind];}
    unsigned want=now;
    if(portrait)want=mine;
    else if(ccz_portrait::kind_of_face(now)){Obj t=table(0x158,4031,6000);if(t)want=word(t,0xA6);}   // portrait face of the other mode: template (도적장수) face
    if(want!=now)*reinterpret_cast<unsigned short *>(person+0xA6)=static_cast<unsigned short>(want);
    // hex = kind | cached << 8: bit s set = face kind 9+s of this face already has a texture (8AD530 keeps it at
    // [manager+0x178][image id], so a cached one never reaches the hook again); 0xFF = no 8AD530 call seen yet this session.
    unsigned cached=255;
    if(ids_manager && kind<ccz_band_kinds::kKinds) {
        cached=0;Obj loaded=pointer(ids_manager,0x178);
        for(unsigned s=0;s<3 && loaded;++s)if(face_ids[kind][s]>0 && pointer(loaded,static_cast<unsigned>(face_ids[kind][s])*8))cached|=1u<<s;
    }
    log_event("band_face_set",commander,(now<<16)|want,kind|(cached<<8));
}
// A live band of this kind wears its portrait face now (lock-free: slot words and person reads; any game thread).
static bool worn(unsigned kind) {
    const unsigned face=ccz_portrait::face_of(kind);
    for(unsigned c=0;c<kSlots;++c) {
        const unsigned long long v=static_cast<unsigned long long>(slots[c]);
        if(!v || ((v>>48)&0xFF)!=kind)continue;
        Obj p=table(0x158,kFirstGeneric+c,6000);
        if(p && word(p,0xA6)==face)return true;
    }
    return false;
}
static bool portrait_bytes(unsigned kind,unsigned size,Obj &out,size_t &length) {
    if(!portrait_asset)portrait_asset=(!ccz_sprites::entries.empty() || ccz_sprites::load_asset())?1:2;
    if(portrait_asset!=1)return false;
    unsigned cell=ccz_appearance::cell(ccz_band_kinds::class_of(kind));
    std::vector<unsigned char> atlas;
    if(!ccz_sprites::decode(cell,atlas)){cell=ccz_appearance::cell(ccz_band_kinds::class_of(0));if(!ccz_sprites::decode(cell,atlas))return false;}
    // move frame 6 = stand, front (native_ccz_sprites.inl frame choice); palette row 0 = the CCZ colours (green ramp = 우군).
    const ccz_portrait::Picture pic{atlas.data()+6*48*ccz_sprites::kAtlasW,ccz_sprites::kAtlasW,48,48,ccz_sprites::palette[0]};
    const ccz_portrait::Size s=ccz_portrait::size_of(size);
    length=ccz_portrait::zp1_size(s);
    out=static_cast<Obj>(HeapAlloc(GetProcessHeap(),0,length));
    if(!out)return false;
    if(!ccz_portrait::build(pic,s,out,length)){HeapFree(GetProcessHeap(),0,out);out=nullptr;return false;}
    return true;
}
#ifndef NO_RUNTIME_LOG
static volatile LONG face_logs;
#endif
static unsigned long long __fastcall hook_face_texture(Obj manager,Obj bytes,int id,unsigned p4) {
    if(!(flags&kFlagPortrait) || !manager || !bytes || id<=0 || !world())return original.face_texture(manager,bytes,id,p4);
    if(manager!=ids_manager) {   // 8AA550 = the face_keep lookup (manager, face kind, face) -> image id
        ids_manager=manager;
        for(unsigned k=1;k<ccz_band_kinds::kKinds;++k)for(unsigned s=0;s<3;++s)
            face_ids[k][s]=fn<int(__fastcall *)(Obj,int,int)>(GA(GA_8AA550))(manager,static_cast<int>(9+s),static_cast<int>(ccz_portrait::face_of(k)));
        log_event("band_face_ids",face_ids[1][1],face_ids[1][0],face_ids[1][2]);
    }
    unsigned kind=0,size=0;
    for(unsigned k=1;k<ccz_band_kinds::kKinds && !kind;++k)for(unsigned s=0;s<3;++s)if(face_ids[k][s]==id){kind=k;size=9+s;break;}
    if(!kind) {
#ifndef NO_RUNTIME_LOG
        // local253 diagnostic (출진 초상화): every other face texture the original makes, result kept (first 300 per run)
        static volatile LONG other_logs;
        const unsigned long long made=original.face_texture(manager,bytes,id,p4);
        if(InterlockedIncrement(&other_logs)<=300)log_event("face_texture_other",static_cast<unsigned>(id),p4,made?1u:0u);
        return made;
#else
        return original.face_texture(manager,bytes,id,p4);
#endif
    }
    if(!worn(kind) && static_cast<unsigned>(InterlockedCompareExchange(&spawn_kind,0,0))!=kind) {
#ifndef NO_RUNTIME_LOG
        if(InterlockedIncrement(&face_logs)<=200)log_event("band_face_skip",static_cast<unsigned>(id),(kind<<8)|size,0);
#endif
        return original.face_texture(manager,bytes,id,p4);
    }
    Obj mine=nullptr;size_t length=0;
    if(!portrait_bytes(kind,size,mine,length)) {
        log_event("band_face_fail",static_cast<unsigned>(id),(kind<<8)|size,static_cast<unsigned>(portrait_asset));
        return original.face_texture(manager,bytes,id,p4);
    }
    const unsigned long long r=original.face_texture(manager,mine,id,p4);
    HeapFree(GetProcessHeap(),0,mine);
#ifndef NO_RUNTIME_LOG
    // hex = texture made (8AD530 returns the texture object, 0 = refused) | bundle KB << 8
    if(InterlockedIncrement(&face_logs)<=200)log_event("band_face",static_cast<unsigned>(id),(kind<<8)|size,(r?1u:0u)|(static_cast<unsigned>(length>>10)<<8));
#endif
    return r;
}
// 2026-11-18 지역 특색: the band's own generic commander gets the kind's name (as the original 2D5F90 names a generic from its template:
// surname +0x12, given name +0x20, 7 wchar each with the terminator; shown "%s%s" by 230990), its troops (+0x16, as 2D53F0 sets them)
// and a floor on 통솔·무력 (+0xEC/+0xED, as 2D5F90 writes the five abilities). The original rewrites all of it when it reuses the generic.
static void write_name(Obj person,unsigned off,const wchar_t *text) {
    wchar_t buf[7]={};
    for(unsigned i=0;i<6 && text && text[i];++i)buf[i]=text[i];
    memcpy(person+off,buf,sizeof(buf));
}
static void apply_kind(Band &b,Obj u) {
    const ccz_band_kinds::Kind &k=ccz_band_kinds::kinds[b.kind<ccz_band_kinds::kKinds?b.kind:0];
    Obj person=table(0x158,b.commander,6000);
    const unsigned troops_before=word(u,0x16);
    if(k.troops_lo && k.troops_hi>=k.troops_lo) {
        const unsigned span=(k.troops_hi-k.troops_lo)/100+1;
        *reinterpret_cast<unsigned short *>(u+0x16)=static_cast<unsigned short>(k.troops_lo+(ccz_combat::roll()%1000*span/1000)*100);
    }
    if(person) {
        if(k.family){write_name(person,0x12,k.family);write_name(person,0x20,k.ranks[b.rank%3]);}
        if(person[0xEC]<k.lead)person[0xEC]=k.lead;
        if(person[0xED]<k.war)person[0xED]=k.war;
        apply_face(b.commander,person,b.kind);
    }
    log_event("band_kind",b.commander,(static_cast<unsigned>(b.kind)<<24)|(static_cast<unsigned>(b.rank)<<16)|word(u,0x16),
              (troops_before<<16)|(person?(static_cast<unsigned>(person[0xEC])<<8)|person[0xED]:65535u));
}
// 2026-11-19 user "한부대가 이미 회수 했는데도 계속 의용병이 생기네": a 거점 whose band left with a host rests 30 days before a new idle
// record (the log showed one fresh 1000-troop band a day from the same posts). Memory only: a load clears the rests.
namespace post_rest {
static unsigned area[kMaxBands];static int until[kMaxBands];
static void reset() {for(unsigned i=0;i<kMaxBands;++i){area[i]=0;until[i]=0;}}
static bool resting(unsigned a,int day) {for(unsigned i=0;i<kMaxBands;++i)if(area[i]==a && day<until[i])return true;return false;}
static void start(unsigned a,int day) {
    if(!a || day<0)return;
    unsigned pick=0;
    for(unsigned i=0;i<kMaxBands;++i){if(area[i]==a){pick=i;break;}if(until[i]<until[pick])pick=i;}
    area[pick]=a;until[pick]=day+30;
    log_event("band_post_rest",a,static_cast<unsigned>(day+30),0);
}
}
static void spawn(Band &b,Obj host,unsigned force) {
    if(b.kind>=ccz_band_kinds::kKinds)b.kind=0;
    const unsigned troops=idle_troops(b,last_day);   // before the record turns live (idle_low..idle_full by its age)
    SpawnKind wearing(b.kind);
    unsigned why=0;Obj u=spawn_unit(b,force,why);
    if(!u) {log_event("band_spawn_fail",b.site,why,force);return;}
    unsigned commander=word(u,0x12),index=unit_index(u),host_index=unit_index(host);
    if(commander-kFirstGeneric>=kSlots || index>500 || host_index>500) {
        log_event("band_spawn_fail",b.site,3,commander);
        own_order=true;
        fn<void(__fastcall *)(Obj,Obj,unsigned long long,int,int)>(GA(GA_25A880))(u,nullptr,0,0,4);
        fn<void(__fastcall *)(unsigned long long,unsigned long long,unsigned long long,unsigned long long)>(GA(GA_2901A0))(0,0,0,0);
        fn<void(__fastcall *)()>(GA(GA_276250))();
        own_order=false;
        return;
    }
    {Obj site=hex_at(b.site);post_rest::start(site?word(site,0x12):0u,last_day);}   // before the record turns live
    b.state=kLive;b.force=static_cast<unsigned char>(force);b.unit=u;b.host=host;
    b.commander=static_cast<unsigned short>(commander);b.unit_index=static_cast<unsigned short>(index);
    b.host_index=static_cast<unsigned short>(host_index);b.host_commander=word(host,0x12);b.last_kind=0;b.last_target=0;
    if(b.kind>=ccz_band_kinds::kKinds)b.kind=0;
    slot_set(commander,u,force,b.kind);
    apply_kind(b,u);
    b.base_before=0;b.seen_troops=0;sync_base(b);
    if(Obj me=person_of(commander)) {   // local260: a valid 주의 (2D5F90 leaves 0) — see guard_principles
        const unsigned had=me[0x14C];
        if(had<1 || had>6)me[0x14C]=static_cast<unsigned char>(ruler_principle(force));
    }
    *reinterpret_cast<unsigned short *>(u+0x16)=static_cast<unsigned short>(troops);
    log_event("band_troops",commander,troops,static_cast<unsigned>(last_day-b.created));
    cut_security(b.site,(std::min)(30u,(troops*30u+idle_full/2)/idle_full),1);   // start troops -> 3, full -> 30 (500/5000, 300/3000, ...; user cap 30)
    own_order=true;
    auto near_action=fn<void(__fastcall *)(Obj,unsigned,unsigned)>(GA(GA_2F03D0));
    near_action(u,0,2);   // engage when an enemy comes near
    near_action(u,3,2);   // stay after a finished order
    near_action(u,1,0);   // no self retreat
    own_order=false;
    log_event("band_create",b.site,(force<<16)|commander,b.host_commander);
    b.dest=static_cast<unsigned short>(kNoHex);b.goal_kind=0;b.wait_base=0;
    b.seen_goal_kind=host[0x38];b.seen_goal=word(host,0x3A);b.seen_chase_kind=host[0x3C];b.seen_chase=word(host,0x3E);b.seen_form=host[0x1B];
    start_morale(b);
    keep_ships(b);
    match_formation(b);
    follow(b,true);
}
// 2026-11-19 user "부대당 거느릴수있는 의용병은 1부대뿐이야 컴퓨터나 플레이어 세력 전체가 최대 10부대까지 거느린다는거야".
constexpr unsigned kPerForce=10;
static unsigned force_bands(unsigned force) {unsigned n=0;for(const Band &b:bands)n+=b.state==kLive && b.force==force;return n;}
static bool hosts_band(Obj u) {for(const Band &b:bands)if(b.state==kLive && b.host==u)return true;return false;}
// Daily: the nearest regular-force field army (not led by a generic officer) within 3 hexes of an idle site becomes the host, unless it
// already leads a band or its force already has kPerForce bands.
static void watch_idle(Band &b) {
    Obj best=nullptr;unsigned best_d=4,best_force=0;
    for(unsigned i=0;i<=500;++i) {
        Obj u=unit_at(i);if(!u || !valid(u) || u[0x10]!=1 || !word(u,0x16) || word(u,0x12)>=kFirstGeneric)continue;
        unsigned d=hex_distance(b.site,word(u,0x2A));if(d>=best_d)continue;
        unsigned f=original.army_force(u);if(f<1 || f>45)continue;
        if(hosts_band(u) || force_bands(f)>=kPerForce)continue;
        best=u;best_d=d;best_force=f;
    }
    if(!best)return;
    log_event("band_near",b.site,(best_force<<16)|best_d,word(best,0x12));
    spawn(b,best,best_force);
}
static unsigned next_rank;   // rotation over a kind's titles (this session only)
// 2026-11-19 user "생성 시간도 넣고 싶어 ... 1000명부터 3000명까지 한달정도": an idle record grows from 1000 troops on its creation day to
// 3000 after 30 days (whole hundreds); the band spawns with what it has grown. One function for the flag fill and the spawn.
// 2026-11-19 user "생기자마자 1000명 너무 많고 500명부터 하자" / "500명부터 5000명까지로 바꾸자": 500 on the creation day, 5000 after
// 30 days.
// 2026-11-19 user "생성을 1개 제한을 두지마. 또 조건에 플레이어 세력 군주가 있는 성의 거점이 전선일때 생성하는걸로 바꾸자": the real
// rule (no longer a diagnostic trigger). The player's ruler (force +0x10; +0x52 is the 군사, the 2026-11-19 fix) sits in city
// C (person +0xE4 = city id 1..46; in the field, a gate or travelling = no city, no records). Every 거점 of C = region (world+0x6CAF0)
// with +0x35 == C whose center (+0x3A) is a player tile and not a city or gate building, and that is on the front = one of its 8
// neighbour regions (+0x56) has its center held by a regular force hostile to the player (2CC630). Each such 거점 keeps one idle record
// on a 2AA9E0 bandit candidate tile beside its center, on the far side from that hostile neighbour (pick_around); a record whose
// 거점 left the list is dropped (why 8). The one-record-at-a-time gate of the diagnostic trigger is gone; one band per host and 10 per
// force stay (watch_idle), records share the kMaxBands table and the renderer's 16 sites.
// 치안 <= 60 under an idle record: the record leaves as an original bandit army (force 51, no slot, so 22E690 answers 51 and the
// hex writer paints its tile like any 2D53F0 bandit) with the troops it has grown. The 거점 rests 30 days. No dry tile today = try
// again tomorrow.
static void banditize(Band &b,int day) {
    const unsigned troops=idle_troops(b,day),site=b.site;
    const unsigned char kind=b.kind;b.kind=0;   // no CCZ portrait face: an original bandit has no slot to show it
    unsigned why=0;Obj u=spawn_unit(b,kBandit,why,false);
    if(!u) {b.kind=kind;static unsigned fails;if(fails++<30)log_event("band_bandit_fail",site,why,troops);return;}
    *reinterpret_cast<unsigned short *>(u+0x16)=static_cast<unsigned short>(troops);
    log_event("band_bandit",word(u,0x12),troops,site);
    Obj hex=hex_at(site);post_rest::start(hex?word(hex,0x12):0u,day);
    free_band(b,9);
}
static void idle_rule(int day) {
    const unsigned player=player_force();
    Obj force=player?table(0xDF30,player,51):nullptr;
    // 2026-11-19 user "오가 아니라 건업이야": force +0x52 is the 군사 (advisor, structs.md 3.x FORCE), not the ruler. Ruler = force +0x10.
    const unsigned ruler=valid(force)?word(force,0x10):0;
    Obj lord=ruler?table(0x158,ruler,6000):nullptr;
    // 2026-11-19 user "사라졌다가 생겼다가 하네": location +0xE4 left the city every 10th day (log 754, 112) and wiped every idle
    // record. The home 거점 +0xE2 stays put; location is only the fallback when the home is not a city.
    // 2026-11-19 user "유비는 건업에 있는데 오소속에 있는 거점 치안 100이상에서 나오는데": +0xE2 is the officer's own base (오), not where
    // the ruler sits (건업). City = location +0xE4 when it is a city; on the days it is not (754, 112 every 10th day) the last city
    // seen for this ruler; the home base only when none was seen yet.
    const unsigned home=valid(lord)?word(lord,0xE2):0,where=valid(lord)?word(lord,0xE4):0;
    static unsigned seen_ruler,seen_city;
    if(where>=1 && where<=46){seen_ruler=ruler;seen_city=where;}
    const unsigned city=where>=1 && where<=46?where:(seen_ruler==ruler && seen_city?seen_city:(home>=1 && home<=46?home:0));
    // 2026-11-19 user "이제 생성 조건을 치안 100인 거점으로 바꾸자": the front test is gone. A record stays while its 거점 is a player
    // 거점 of the ruler's city (keep), a new one needs security +0x54 == 100 (ready); enemy_tile only steers the site away from a hostile
    // neighbour when there is one.
    struct Post {unsigned area,center,enemy_tile;bool ready;};
    Post post[64];unsigned posts=0,regions=0,front=0;   // front = ready count (log field kept)
    unsigned why=0;   // 0 ok, 1 no player force, 2 ruler not in a city, 3 the city has no player 거점, 4 none at 치안 100, 5 over 10 cities
    unsigned castle[62]={};   // building tiles of cities and gates (any owner): a 거점 center never sits on one
    for(unsigned c=1;c<=61;++c){Obj bld=table(0x6DA98,c,3000);castle[c]=bld?word(bld,0x10):kNoHex;}
    // 2026-11-20 user "의용병무리가 플레이어 세력이가진 성이 10개 이하일때만 켜지도록" / "성 11개 이상일때 종군무리 그대로 둬": cities
    // 1..46 whose building tile the player holds. Over 10: no idle record (the existing ones go, why 8); live bands stay.
    unsigned cities=0;
    for(unsigned c=1;c<=46 && player;++c){Obj hex=hex_at(castle[c]);cities+=hex && hex[0x14]==player?1u:0u;}
    if(!player)why=1;
    else if(cities>10)why=5;
    else if(!city || city>46)why=2;
    else {
        for(unsigned a=1;a<=500;++a) {
            Obj area=table(0x6CAF0,a,500);if(!area || area[0x35]!=city || !valid(area))continue;
            const unsigned center=word(area,0x3A);Obj hex=hex_at(center);if(!hex || hex[0x14]!=player)continue;
            bool on_castle=false;for(unsigned c=1;c<=61 && !on_castle;++c)on_castle=castle[c]==center;
            if(on_castle)continue;
            ++regions;
            unsigned enemy_tile=kNoHex;
            for(unsigned i=0;i<8 && enemy_tile>=kNoHex;++i) {
                const unsigned n=word(area,0x56+i*2);if(!n || n>500)continue;
                Obj other=table(0x6CAF0,n,500);Obj tile=other?hex_at(word(other,0x3A)):nullptr;
                const unsigned owner=tile?tile[0x14]:0;
                if(owner && owner<=45 && owner!=player && hostile(owner,player))enemy_tile=word(other,0x3A);
            }
            const bool ready=area[0x54]>=100;
            front+=ready?1u:0u;
            if(posts<64)post[posts++]={a,center,enemy_tile,ready};
        }
        if(!regions)why=3;else if(!front)why=4;
    }
    // army = ruler city, value = 거점 << 16 | on the front, hex = ruler << 8 | why; logged when it changes
    static unsigned last_rule=~0u;
    const unsigned rule=(city<<24)^(regions<<16)^(front<<8)^why^(ruler<<4)^(where<<12)^(cities<<20);
    if(rule!=last_rule){last_rule=rule;log_event("band_rule",city,(regions<<16)|front,(where<<20)|(ruler<<8)|why);log_event("band_cities",cities,player,why);}   // hex: location<<20
    auto region_of=[&](const Band &b)->unsigned {Obj hex=hex_at(b.site);return hex?word(hex,0x12):0u;};
    for(Band &b:bands) {
        if(b.state!=kIdle)continue;
        const unsigned r=region_of(b);bool kept=false;
        for(unsigned k=0;k<posts && !kept;++k)kept=post[k].area==r;
        if(!kept){free_band(b,8);continue;}
        // full (5000) at 30 days; not taken: -10 security at 60, 90, ... days (b.cuts = cuts done)
        const int age=idle_age(b,day);
        const unsigned due=age>=60?static_cast<unsigned>((age-30)/30):0u;
        while(b.cuts<due && b.cuts<10){cut_security(b.site,10,2);++b.cuts;}
        Obj area=site_area(b.site);
        if(area && area[0x54]<=60)banditize(b,day);
    }
    unsigned idle=0;for(const Band &b:bands)idle+=b.state==kIdle;
    for(unsigned k=0;k<posts && idle<ccz_idle::kMax;++k) {
        const Post &pick=post[k];
        if(!pick.ready)continue;
        bool has=false;for(const Band &b:bands)has=has || (b.state==kIdle && region_of(b)==pick.area);
        if(has || post_rest::resting(pick.area,day))continue;
        Band *slot=nullptr;for(Band &b:bands)if(b.state==kFree){slot=&b;break;}
        if(!slot)break;
        Obj area=table(0x6CAF0,pick.area,500);
        GameList list={image+GA(GA_1346670),nullptr};
        fn<void(__fastcall *)(GameList *)>(GA(GA_1AC60))(&list);
        const unsigned candidates=fn<unsigned(__fastcall *)(Obj,GameList *)>(GA(GA_2AA9E0))(area,&list);
        unsigned site=pick_around(list,pick.center,pick.enemy_tile,pick.area),path=0;
        fn<void(__fastcall *)(GameList *)>(GA(GA_1BFFF0))(&list);
        if(site>=kNoHex) {   // no bandit candidate beside the center: a free dry player tile of the same region 1~2 hexes away
            path=1;unsigned site_e=0;
            const int cx=static_cast<int>(pick.center%220),cy=static_cast<int>(pick.center/220);
            for(int dy=-3;dy<=3;++dy)for(int dx=-2;dx<=2;++dx) {
                int x=cx+dx,y=cy+dy;if(x<0 || y<0 || x>=220 || y>=220)continue;
                unsigned t=static_cast<unsigned>(y*220+x),d=hex_distance(t,pick.center);if(d<1 || d>2)continue;
                Obj hex=hex_at(t);if(!dry_land(hex) || hex[0x14]!=player || word(hex,0x12)!=pick.area || word(hex,0x1A))continue;
                unsigned e=hex_distance(t,pick.enemy_tile);if(site>=kNoHex || e>site_e) {site_e=e;site=t;}
            }
        }
        // 2 = no tile beside this 거점 (value = 2 | region << 8)
        if(site>=kNoHex) {static unsigned fails;if(fails++<50)log_event("band_create_fail",city,2|(pick.area<<8),posts);continue;}
        const unsigned state=ccz_band_kinds::state_of_base(city),kind=ccz_band_kinds::pick_kind(state,ccz_combat::roll());
        Band &b=*slot;
        b={};b.state=kIdle;b.force=static_cast<unsigned char>(player);b.site=site;b.created=day;
        b.kind=static_cast<unsigned char>(kind);b.rank=static_cast<unsigned char>(next_rank++%3);++idle;
        log_event("band_create",site,(city<<16)|pick.area,(state<<16)|(kind<<8)|(posts&0xFF));
        // army = region, value = its center tile, hex = path << 24 | front 거점 << 16 | bandit candidates of the region
        log_event("band_create_post",pick.area,pick.center,(path<<24)|((posts&255u)<<16)|(candidates&65535u));
    }
}
// ---- 2026-11-20 daily kind traits (first move step of a game day, lock held; ccz_band_kinds.h)
// Force of any field army as the game sees it: a live band answers its patron, every other army its raw 22E690.
static unsigned seen_force(Obj u) {const unsigned f=band_force(u);return f?f:original.army_force(u)&255u;}
// 위압 (맹수대·상아대): every hostile army next to the band (1 hex) loses 3 morale through 271E70 (0 = the original rout).
static void awe(Band &b) {
    const unsigned t=word(b.unit,0x2A);unsigned hit=0;
    for(unsigned i=0;i<=500;++i) {
        Obj u=unit_at(i);if(!u || u==b.unit || !valid(u) || u[0x10]!=1 || !word(u,0x16) || !u[0x1A])continue;
        if(hex_distance(t,word(u,0x2A))!=1)continue;
        const unsigned f=seen_force(u);if(!f || f==b.force || !hostile(f,b.force))continue;
        fn<void(__fastcall *)(Obj,int,int,int)>(GA(GA_271E70))(u,-3,0,1);++hit;
    }
    if(hit)log_event("band_awe",b.commander,hit,t);
}
// 치료 (오두미도): armies of F or not hostile to F within 2 hexes (the band too) turn 10% of their wounded (+0x18, at least 100) into
// troops (+0x16), capped by the commander's troop limit 2B4CA0 — the same two writes the heal tactic 178DF0 makes.
static void heal(Band &b) {
    const unsigned t=word(b.unit,0x2A);unsigned armies=0,total=0;
    for(unsigned i=0;i<=500;++i) {
        Obj u=unit_at(i);if(!u || !valid(u) || u[0x10]!=1 || !word(u,0x16) || !word(u,0x18))continue;
        if(hex_distance(t,word(u,0x2A))>2)continue;
        const unsigned f=seen_force(u);if(!f || (f!=b.force && hostile(f,b.force)))continue;
        Obj leader=person_of(word(u,0x12));if(!leader || !valid(leader))continue;
        const int cap=fn<int(__fastcall *)(Obj)>(GA(GA_2B4CA0))(leader);
        const unsigned h=ccz_band_kinds::heal_amount(word(u,0x18),word(u,0x16),cap>0?static_cast<unsigned>(cap):0u);
        if(!h)continue;
        *reinterpret_cast<unsigned short *>(u+0x16)=static_cast<unsigned short>(word(u,0x16)+h);
        *reinterpret_cast<unsigned short *>(u+0x18)=static_cast<unsigned short>(word(u,0x18)-h);
        ++armies;total+=h;
    }
    if(armies)log_event("band_heal",b.commander,armies,total);
}
static void daily_traits() {
    for(Band &b:bands) {
        if(b.state!=kLive || !unit_alive(b))continue;
        const unsigned tr=ccz_band_kinds::traits(b.kind);
        if(tr&ccz_band_kinds::kAwe)awe(b);
        if(tr&ccz_band_kinds::kHeal)heal(b);
    }
}
// 2026-11-20 user "산월종수 이런애들은 등용창에 있으면 안되는데": a former band commander already out as 재야 (5) or 포로 (6) in this
// save (captured before the 2801C0 hook) leaves the game the way 25A880 deletes a bandit generic: 32D4A0 then 227D30. Only generic
// officers 4033..4100 that no band record holds and that still carry a band kind's family name (ccz_band_kinds.h, written by
// apply_kind); an original bandit generic keeps its template name and stays.
static bool band_family(Obj p) {
    const wchar_t *name=reinterpret_cast<const wchar_t *>(p+0x12);
    for(unsigned k=1;k<ccz_band_kinds::kKinds;++k) {
        const wchar_t *f=ccz_band_kinds::kinds[k].family;
        if(f && !wcsncmp(name,f,7))return true;
    }
    return false;
}
static void clean_strays() {
    for(unsigned id=kFirstGeneric;id<kFirstGeneric+kSlots;++id) {
        Obj p=person_of(id);if(!p || !valid(p))continue;
        // 2026-11-20 user "전투중 격파에의한 의용병무리 포로획득을 지워달란거야 … 대도서관에서": a 포로 (6) is held only by his own
        // fields (+0xE2 base, +0xE0 old corps; the per-base captive lists are rebuilt from persons each month, rules_people.md 6.3),
        // and an escape 2E9980 would send him to his corps' force F as status 4 (player-owned when F is the player). The generic
        // lifecycle is person-side too (2D5F90: 321560 register + 242650 corps; 25A880: 32D4A0 + 227D30, rules_tribes.md).
        const unsigned status=p[0xE6];if(status!=5 && status!=6)continue;
        bool held=false;for(const Band &b:bands)held=held || (b.state!=kFree && b.commander==id);
        if(held || slots[id-kFirstGeneric] || !band_family(p))continue;
        const unsigned at=word(p,0xE2);
        fn<unsigned long long(__fastcall *)(Obj,Obj)>(GA(GA_32D4A0))(world(),p);
        fn<void(__fastcall *)(Obj)>(GA(GA_227D30))(p);
        log_event("band_stray_removed",id,status,(at<<8)|p[0xE6]);
    }
}
static bool world_check(int day) {
    Obj w=world();if(!w)return false;
    if(w!=band_world || (last_day>=0 && day>=0 && day<last_day)) {
        for(Band &b:bands)if(b.state!=kFree)free_band(b,7);
        band_world=w;last_day=-1;
    }
    return true;
}
// true on the first 2DC550 call of a game day (orders, spawn checks); later calls of the same day only re-check hosts.
static bool day_before() {
    int day=ccz_sprites::game_day_read();
    if(!world_check(day))return false;
    if(day==last_day) {++day_calls;return false;}
    unsigned live=0,idle=0;
    for(const Band &b:bands) {live+=b.state==kLive;idle+=b.state==kIdle;}
    if(live || idle)log_event("band_day",static_cast<unsigned>(day),(live<<8)|idle,(day_calls<<16)|(foreign_today&0xFFFF));
    if(live || idle)log_event("band_day2",static_cast<unsigned>(day),morale_skips,spawn_skips);
    last_day=day;day_calls=1;foreign_today=0;morale_skips=0;
    for(Band &b:bands) {
        if(b.state!=kLive)continue;
        if(!unit_alive(b)) {free_band(b,6);continue;}
        keep_ships(b);
        if(!b.host) {   // 2026-11-20 waiting beside a base of F
            if(!ccz_band_kinds::waits_at_base(b.kind)){disband(b,12);continue;}   // why 12: only 청주병·서량기병 wait (old saves)
            if(!force_alive(b.force)){disband(b,11);continue;}
            if(!b.wait_base || !base_of(b.wait_base,b.force))b.wait_base=static_cast<unsigned short>(nearest_base(b.force,word(b.unit,0x2A)));
            if(!b.wait_base){disband(b,10);continue;}
            sync_base(b);
            wait_move(b);
            continue;
        }
        if(unsigned why=host_why(b)) {lose_host(b,why);continue;}   // lose_host disbands every kind but 청주병·서량기병
        sync_base(b);
#ifndef NO_RUNTIME_LOG
        watch_troops(b,b.unit);
#endif
        follow(b,false);
    }
    guard_principles();   // local260: once a game day, before the AI day passes that call 22B360
    daily_traits();
    clean_strays();
    return true;
}
static void day_after(bool first) {
    if(!band_world || band_world!=world())return;
    for(Band &b:bands) {
        if(b.state!=kLive)continue;
        if(!unit_alive(b)) {free_band(b,6);continue;}
        if(!b.host){rehost(b);continue;}
        if(unsigned why=host_why(b))lose_host(b,why);
    }
    if(!first)return;
    for(Band &b:bands)if(b.state==kIdle)watch_idle(b);
    idle_rule(last_day);
    bool sites=false;for(const Band &b:bands)sites=sites || b.state==kIdle;
    ccz_sprites::idle_day(sites);
}
// Idle sites for the sprite renderer (ccz_idle in ccz_module.cpp); caller holds the band lock or owns the table.
static void publish_idle() {
    unsigned k=0;
    const int day=last_day>=0?last_day:ccz_sprites::game_day_read();   // right after a load no day pass has run yet
    for(const Band &b:bands)if(b.state==kIdle && b.site<kNoHex && k<ccz_idle::kMax) {
        InterlockedExchange(&ccz_idle::site_kinds[k],static_cast<LONG>(b.kind));   // kind first: a torn pair shows one frame of the old kind
        InterlockedExchange(&ccz_idle::site_fill[k],static_cast<LONG>((idle_troops(b,day)-idle_low)*1000/(idle_top(b)-idle_low)));   // per mille
        InterlockedExchange64(&ccz_idle::sites[k++],static_cast<LONG64>(b.site)+1);
    }
    while(k<ccz_idle::kMax){InterlockedExchange64(&ccz_idle::sites[k],0);InterlockedExchange(&ccz_idle::site_kinds[k],0);InterlockedExchange(&ccz_idle::site_fill[k],0);++k;}
}
static void __fastcall hook_day_move(Obj p,int v) {
    if(!(flags&kFlagOn)) {original.day_move(p,v);return;}
    AcquireSRWLockExclusive(&lock);
    const bool first=day_before();
    if(band_world && band_world==world())track_hosts(day_calls);   // every move step: host goal changes re-plan at once
    ReleaseSRWLockExclusive(&lock);
    original.day_move(p,v);
    AcquireSRWLockExclusive(&lock);
    day_after(first);
    publish_idle();
    ReleaseSRWLockExclusive(&lock);
}

// ---------------------------------------------------------------- module API
static bool __cdecl configure(void *const *gates,unsigned new_flags) {
    Originals o={};
    o.army_force=reinterpret_cast<ArmyForce>(gates[kBandArmyForce]);o.paint=reinterpret_cast<UnitCall>(gates[kBandPaint]);
    o.supply=reinterpret_cast<UnitCall>(gates[kBandSupply]);o.remove=reinterpret_cast<Remove>(gates[kBandRemove]);
    o.paint_write=reinterpret_cast<PaintWrite>(gates[kBandPaintWrite]);o.morale=reinterpret_cast<Morale>(gates[kBandMorale]);
    o.day_move=reinterpret_cast<DayMove>(gates[kBandDayMove]);
    o.set_action=reinterpret_cast<SetAction>(gates[kBandSetAction]);o.set_target=reinterpret_cast<SetTarget>(gates[kBandSetTarget]);
    o.face_texture=reinterpret_cast<FaceTexture>(gates[kBandFaceTexture]);
    o.unit_target=reinterpret_cast<UnitCall>(gates[kBandUnitTarget]);
    o.max_morale=reinterpret_cast<MaxMorale>(gates[kBandMaxMorale]);
    o.capture=reinterpret_cast<Capture>(gates[kBandCapture]);
    if(!o.army_force || !o.paint || !o.supply || !o.remove || !o.paint_write || !o.morale || !o.day_move || !o.unit_target || !o.max_morale ||
       !o.capture)return false;
    const unsigned level=(new_flags>>12)&15u;   // [ccz] band_morale 1..3, 0 (older convenience DLL) = x1
    morale_pct=level==1?50u:level==3?150u:100u;
    if((new_flags&kFlagSetters) && (!o.set_action || !o.set_target))return false;
    if((new_flags&kFlagPortrait) && !o.face_texture)return false;
    const unsigned pick=(new_flags>>kTroopShift)&15u,step=pick>=1 && pick<=3?pick+2u:5u;   // 0 (older convenience DLL) = 500/5000
    idle_low=step*100u;idle_full=step*1000u;
    log_event("band_troop_range",idle_low,idle_full,pick);
    original=o;flags=new_flags;return true;
}
static void __cdecl reset() {
    AcquireSRWLockExclusive(&lock);
    for(auto &s:slots)InterlockedExchange64(&s,0);
    for(Band &b:bands)b={};
    post_rest::reset();
    band_world=nullptr;last_day=-1;day_calls=0;
    faces_world=nullptr;   // the next save may reuse the world object with other officers: scan their faces again
    publish_idle();
    ReleaseSRWLockExclusive(&lock);
}
// Sidecar layout v1: "BNDS", u16 version, u16 count, then 24 bytes per record.
constexpr unsigned kMagic=0x53444E42u,kVersion=1,kRecord=24,kHeader=8;
static void put16(unsigned char *p,unsigned v) {p[0]=static_cast<unsigned char>(v);p[1]=static_cast<unsigned char>(v>>8);}
static void put32(unsigned char *p,unsigned v) {put16(p,v&0xFFFF);put16(p+2,v>>16);}
static unsigned get16(const unsigned char *p) {return p[0]|(static_cast<unsigned>(p[1])<<8);}
static unsigned get32(const unsigned char *p) {return get16(p)|(get16(p+2)<<16);}
static unsigned __cdecl save(unsigned char *out,unsigned cap) {
    AcquireSRWLockShared(&lock);
    unsigned count=0;
    for(const Band &b:bands)count+=b.state!=kFree;
    unsigned need=kHeader+count*kRecord;
    if(!out || cap<need) {ReleaseSRWLockShared(&lock);return 0;}
    put32(out,kMagic);put16(out+4,kVersion);put16(out+6,count);
    unsigned char *p=out+kHeader;
    for(const Band &b:bands) {
        if(b.state==kFree)continue;
        memset(p,0,kRecord);
        p[0]=b.state;p[1]=b.force;p[2]=b.last_kind;p[3]=b.kind;p[14]=b.rank;p[15]=b.cuts;   // p[3]/p[14] were zero padding in local217~221 (= kind 0 산적)
        put16(p+4,b.commander);put16(p+6,b.host_commander);put16(p+8,b.unit_index);put16(p+10,b.host_index);put16(p+12,b.last_target);
        put32(p+16,b.site);put32(p+20,static_cast<unsigned>(b.created));
        p+=kRecord;
    }
    ReleaseSRWLockShared(&lock);
    return need;
}
// Load phase 3 (the world is complete): live records find their units again by index + commander; a record whose unit is gone
// is dropped (the unit then stays an original bandit), a record whose host is gone is disbanded by the next day pass.
static bool __cdecl load(const unsigned char *in,unsigned length) {
    reset();
    if(!in || length<kHeader || get32(in)!=kMagic || get16(in+4)!=kVersion)return false;
    unsigned count=get16(in+6);
    if(count>kMaxBands || length!=kHeader+count*kRecord)return false;
    AcquireSRWLockExclusive(&lock);
    unsigned live=0,idle=0,dropped=0,n=0;
    for(unsigned i=0;i<count;++i) {
        const unsigned char *p=in+kHeader+i*kRecord;
        Band b={};b.state=p[0];b.force=p[1];b.last_kind=p[2];b.cuts=static_cast<unsigned char>(p[15]<=10?p[15]:10);
        b.kind=static_cast<unsigned char>(p[3]<ccz_band_kinds::kKinds?p[3]:0);b.rank=static_cast<unsigned char>(p[14]%3);
        b.dest=static_cast<unsigned short>(kNoHex);
        b.commander=static_cast<unsigned short>(get16(p+4));b.host_commander=static_cast<unsigned short>(get16(p+6));
        b.unit_index=static_cast<unsigned short>(get16(p+8));b.host_index=static_cast<unsigned short>(get16(p+10));
        b.last_target=static_cast<unsigned short>(get16(p+12));b.site=get32(p+16);b.created=static_cast<int>(get32(p+20));
        if(b.state==kLive) {
            b.unit=unit_at(b.unit_index);b.host=unit_at(b.host_index);
            if(!b.unit || !valid(b.unit) || word(b.unit,0x12)!=b.commander || b.commander-kFirstGeneric>=kSlots || !b.force || b.force>45) {++dropped;continue;}
            slot_set(b.commander,b.unit,b.force,b.kind);++live;
            apply_face(b.commander,table(0x158,b.commander,6000),b.kind);   // the save's face follows this session's picture mode
            if(b.host && valid(b.host)){b.seen_goal_kind=b.host[0x38];b.seen_goal=word(b.host,0x3A);b.seen_chase_kind=b.host[0x3C];b.seen_chase=word(b.host,0x3E);b.seen_form=b.host[0x1B];}
        } else if(b.state==kIdle) {
            // 2026-11-19: idle records are real (the ruler's frontline 거점) and keep their creation day = their growth; the next day
            // pass drops one whose 거점 left the rule.
            if(b.site>=kNoHex) {++dropped;continue;}
            ++idle;
        } else {++dropped;continue;}
        bands[n++]=b;
    }
    band_world=world();last_day=-1;
    publish_idle();
    guard_principles();   // local260: before demo play can hand the player's force to the computer (22B360)
    ReleaseSRWLockExclusive(&lock);
    ccz_sprites::idle_day(idle!=0);   // flags-alone mode: heights and the overlay now, not only after the next day pass
    log_event("band_load",live,idle,dropped);
    return true;
}
}
