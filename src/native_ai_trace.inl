// Observation-only native AI trace (AI_TRACE_ONLY analysis build; no mod feature is published).
// Every native behaviour-tree node's Execute slot (vtable index 16, RTTI-verified list in
// ai_trace_nodes.h) is wrapped: the original runs unchanged with the same arguments and its
// result is returned unchanged. Around each top-level node call the wrapper compares a raw
// snapshot of the army (world+7E838) and troops (world+7F8D8) tables, so every change of
// action/target/relay/purpose is attributed either to that node ("node") or to code that ran
// between two node calls ("between": daily processing, combat, player commands). No game data
// is written, no thread is created, and the only native calls are the date getter 32BA50 and
// the army force getter 22CC40, both on the calling AI thread. The original call holds no lock;
// overlapping calls from other threads are marked conc=1 so analysis can discard them.
#include "game_addresses.h"
#include "ai_trace_nodes.h"
#include <string>
#include <utility>
namespace ai_trace {
constexpr unsigned kNodes=sizeof(trace_nodes)/sizeof(trace_nodes[0]);
constexpr unsigned kArmyTable=0x7E838,kTroopsTable=0x7F8D8,kMax=500;
constexpr unsigned kArmyLo=0x10,kArmyHi=0x70,kTroopsLo=0x10,kTroopsHi=0x90;
struct Snapshot {
    Obj army_ptr[kMax+1],troops_ptr[kMax+1];
    unsigned char army[kMax+1][kArmyHi-kArmyLo];
    unsigned char troops[kMax+1][kTroopsHi-kTroopsLo];
    unsigned char army_ai[kMax+1][8];   // +1F0 행동 비트(u32), +1F4 병참 단절 목표 부대(u32) (2026-11-10)
};
constexpr unsigned kArmyAi=0x1F0;
static Snapshot *last_state,*scratch;
static bool have_last;
static SRWLOCK lock=SRWLOCK_INIT;
static volatile LONG installed,active;
static thread_local unsigned depth;
static thread_local unsigned cur_node=~0u;   // innermost traced node on this thread (fn rows)
static unsigned last_node=~0u;
static unsigned long long sequence,written;
static int current_day=-1;
static volatile LONG calls[kNodes],changes[kNodes]; // calls also counted for unwatched nodes
static HANDLE file=INVALID_HANDLE_VALUE;
static std::string *buffer;
static wchar_t output_path[MAX_PATH];
// No size cap (user 2026-11-10 "로그 용량제한은 걱정하지말고 일단 다 수집"); writing stops only when the
// drive holding the log has under 5 GiB free, so saves and the game itself never run out of disk.
static const unsigned long long kMinFree=5ull<<30;
static bool disk_full;
using Exec=uint64_t(__fastcall *)(Obj,Obj,Obj);
static Exec originals[kNodes];
// Optional SAN14_CombatAI.dll export: the mod decision steps taken during the traced node call on this
// thread (e.g. "army_node:mod;ap:build"). Written in the `next` column of node/call rows.
using StepFn=unsigned(__cdecl *)(char *,unsigned);
static StepFn mod_steps;
static char step_label[160];

// Known army fields (research/sortie, NATIVE_FUNCTIONS). Volatile fields are context only.
struct Field {unsigned offset,size;const char *name;bool volatile_field;};
static const Field army_fields[]={
    {0x12,2,"commander",false},{0x14,2,"move_points",true},{0x16,2,"soldiers",true},{0x1A,1,"morale",true},
    {0x26,2,"flags",false},{0x2A,2,"hex",true},{0x2C,2,"relay1",false},{0x2E,2,"relay2",false},{0x30,2,"relay3",false},
    {0x32,2,"relay4",false},{0x34,2,"relay5",false},{0x37,1,"action",false},{0x38,1,"target_kind",false},
    {0x3A,2,"target_id",false},{0x3C,1,"sub_target_kind",false},{0x3E,2,"sub_target_id",false},
    {0x40,1,"on_enemy_approach",false},{0x41,1,"after_order",false},{0x42,2,"target_hex",false},{0x46,1,"facility",false},
    {0x48,2,"step_hex",true},{0x58,2,"troops",false}};
// v12.13 (live read-only check 2026-10-20, 장비 부대): +3C/+3E = 잠정목표 종류/번호 (0xFF = 없음);
// Unit-detail options, confirmed one by one on the user's own army (동도나, read-only snapshots
// 2026-10-20, tools/army_option_probe.py): +40 적 접근 시 1 무시 / 2 공격 (stored when the dialog
// closes); +26 bit 0x04 자주퇴각, bit 0x08 추격; +41 사후명령 1 퇴각 / 2 대기 (choosing 퇴각 also
// clears 자주퇴각).
static const Field troops_fields[]={{0x14,2,"mode",false},{0x16,2,"goal",false}};

static int trace_day() {
    Obj root=world();Obj context=root?pointer(root,0x871E8):nullptr;
    return context?fn<int(__fastcall *)(Obj)>(GA(GA_32DD00))(context+0x34):-1;
}
static void flush() {
    if(!buffer || buffer->empty())return;
    if(file!=INVALID_HANDLE_VALUE && !disk_full) {
        ULARGE_INTEGER free_bytes{};
        if(GetDiskFreeSpaceExW(output_path,&free_bytes,nullptr,nullptr) && free_bytes.QuadPart<kMinFree) {
            disk_full=true;const char stop[]="0,-1,0,meta,start,,,0,meta,0,0,0,0,0,disk_low_stop,0,0,\n";
            DWORD done=0;WriteFile(file,stop,sizeof(stop)-1,&done,nullptr);buffer->clear();return;
        }
        DWORD done=0;WriteFile(file,buffer->data(),static_cast<DWORD>(buffer->size()),&done,nullptr);written+=done;
    }
    buffer->clear();
}
static void text(const char *s){buffer->append(s);}
static void number(unsigned long long v){char b[24];int n=0;do{b[n++]=static_cast<char>('0'+v%10);v/=10;}while(v);while(n)buffer->push_back(b[--n]);}
static void signed_number(int v){if(v<0){buffer->push_back('-');number(static_cast<unsigned long long>(-static_cast<long long>(v)));}else number(static_cast<unsigned>(v));}
static void hex(unsigned v){char b[12];int n=0;do{b[n++]="0123456789ABCDEF"[v&15];v>>=4;}while(v);text("0x");while(n)buffer->push_back(b[--n]);}
static unsigned read_field(const unsigned char *bytes,unsigned lo,unsigned offset,unsigned size) {
    unsigned v=0;memcpy(&v,bytes+offset-lo,size);return v;
}
// ---- Game terms (v12.13). Plain memory reads of tables the game already keeps; no native call.
// Verified read-only on the running game 2026-10-20: person +34 "성명+자" with 자 at +2E,
// force +10 = ruler person, area +10 name, tile(world+E270) +12 area / +14 owner,
// building(world+6DA98, id = city 1..51 / gate 52..61) +10 centre hex, +13 owner.
static void utf8(const wchar_t *w,unsigned n) {
    char out[96];int k=WideCharToMultiByte(CP_UTF8,0,w,static_cast<int>(n),out,sizeof(out),nullptr,nullptr);
    for(int i=0;i<k;++i)if(out[i]!=',' && out[i]!='\n' && out[i]!='\r')buffer->push_back(out[i]);
}
static unsigned wide_length(const wchar_t *w,unsigned max){unsigned n=0;while(n<max && w[n])++n;return n;}
static void person_name(unsigned id) {
    Obj p=table(0x158,id,6000);
    if(!id || !p){text("-");return;}
    const wchar_t *full=reinterpret_cast<const wchar_t *>(p+0x34),*style=reinterpret_cast<const wchar_t *>(p+0x2E);
    unsigned n=wide_length(full,20),s=wide_length(style,3);
    if(s && s<n && !memcmp(full+n-s,style,s*2))n-=s;   // drop the courtesy name
    if(n)utf8(full,n);else number(id);
}
static void force_name(unsigned id) {
    Obj f=table(0xDF30,id,51);
    if(!id || !f){text("세력없음");return;}
    person_name(*reinterpret_cast<unsigned short *>(f+0x10));text("군");
}
static unsigned hex_area(unsigned hex) {
    Obj tile=hex<48400?table(0xE270,hex,48399):nullptr;
    return tile?*reinterpret_cast<unsigned short *>(tile+0x12):0;
}
static void area_name(unsigned area) {
    Obj a=table(0x6CAF0,area,500);
    if(!a){text("?");return;}
    const wchar_t *w=reinterpret_cast<const wchar_t *>(a+0x10);utf8(w,wide_length(w,10));
}
static void base_name(unsigned id) {   // city 1..51, gate 52..61
    Obj b=table(0x6DA98,id,3000);
    if(!b){text("거점");number(id);return;}
    area_name(hex_area(*reinterpret_cast<unsigned short *>(b+0x10)));
}
static const char *action_name(unsigned a) {
    switch(a){case 0:return "없음";case 1:return "진군";case 2:return "아군도시이동";case 3:return "아군관문이동";
    case 4:return "도시공격";case 5:return "관문공격";case 6:return "시설공격";case 7:return "부대공격";case 8:case 9:return "건설";
    case 11:case 12:return "귀환";case 13:return "대기";case 14:return "이동";default:return nullptr;}
}
static void target_text(unsigned kind,unsigned id) {
    switch(kind) {
    case 5:text("도시 ");base_name(id);break;
    case 6:text("관문 ");base_name(id+51);break;
    case 9:area_name(hex_area(id));text("(칸");number(id);text(")");break;
    case 27:{Obj e=table(0x7E838,id,500);text("부대");number(id);
             if(e){text("(");person_name(*reinterpret_cast<unsigned short *>(e+0x12));text(")");}break;}
    case 0:case 255:text("-");break;
    default:text("종류");number(kind);text(" ");number(id);
    }
}
static const char *mission_name(unsigned mode) {
    switch(mode){case 0:return "임무없음(귀환)";case 1:return "요격";case 5:return "건설";case 7:return "부대행동";
    case 8:case 9:case 10:case 22:return "성공략";case 11:return "성공략(직접)";case 12:return "아군거점이동";
    case 13:case 14:case 15:case 3:return "지역점령";case 17:case 18:return "목표유지";default:return nullptr;}
}
// ",context" for an army line: 무장|세력|병력|사기|위치|명령 목표|잠정목표.
static void army_context(const unsigned char *b,unsigned force) {
    unsigned action=b[0x37-kArmyLo];
    text(",");person_name(read_field(b,kArmyLo,0x12,2));text("|");force_name(force);
    text("|병");number(read_field(b,kArmyLo,0x16,2));text("|사기");number(b[0x1A-kArmyLo]);
    text("|위치 ");area_name(hex_area(read_field(b,kArmyLo,0x2A,2)));text("|");
    if(const char *a=action_name(action))text(a);else{text("행동");number(action);}
    text(" ");target_text(b[0x38-kArmyLo],read_field(b,kArmyLo,0x3A,2));
    if(b[0x3C-kArmyLo]!=0xFF){text("|잠정 ");target_text(b[0x3C-kArmyLo],read_field(b,kArmyLo,0x3E,2));}
    unsigned approach=b[0x40-kArmyLo];
    text("|적접근시 ");if(approach==1)text("무시");else if(approach==2)text("공격");else number(approach);
    unsigned flags=b[0x26-kArmyLo],after=b[0x41-kArmyLo];
    text("|자주퇴각");text(flags&0x04?"○":"×");text("|추격");text(flags&0x08?"○":"×");
    text("|사후명령 ");if(after==1)text("퇴각");else if(after==2)text("대기");else number(after);
}
static void troops_context(const unsigned char *b) {
    unsigned mode=read_field(b,kTroopsLo,0x14,2),goal=read_field(b,kTroopsLo,0x16,2);
    text(",");if(const char *m=mission_name(mode))text(m);else{text("임무");number(mode);}
    text(" 목표");number(goal);
    if(goal && goal<=61){text("(");base_name(goal);text(")");}
    else if(goal && goal<48400){text("(");area_name(hex_area(goal));text(")");}
}
static void capture(Snapshot &s) {
    for(unsigned id=1;id<=kMax;++id) {
        Obj a=table(kArmyTable,id,kMax),t=table(kTroopsTable,id,kMax);
        s.army_ptr[id]=a;s.troops_ptr[id]=t;
        if(a)memcpy(s.army[id],a+kArmyLo,sizeof(s.army[id]));else memset(s.army[id],0,sizeof(s.army[id]));
        if(a)memcpy(s.army_ai[id],a+kArmyAi,sizeof(s.army_ai[id]));else memset(s.army_ai[id],0,sizeof(s.army_ai[id]));
        if(t)memcpy(s.troops[id],t+kTroopsLo,sizeof(s.troops[id]));else memset(s.troops[id],0,sizeof(s.troops[id]));
    }
}
// seq,day,thread,cause,node,next,result,conc,kind,id,commander,force,hex,troops,field,old,new
static void header(const char *cause,unsigned node,unsigned next,long long result,const char *kind,unsigned id,
                   unsigned commander,unsigned force,unsigned position,unsigned troops) {
    number(++sequence);text(",");signed_number(current_day);text(",");number(GetCurrentThreadId());text(",");
    text(cause);text(",");text(node<kNodes?trace_nodes[node].name:"start");text(",");
    text(next<kNodes?trace_nodes[next].name:(step_label[0]?step_label:""));text(",");
    if(result<0)text("");else number(static_cast<unsigned long long>(result));text(",");
    text(active>1?"1":"0");text(",");text(kind);text(",");number(id);text(",");number(commander);text(",");
    number(force);text(",");number(position);text(",");number(troops);text(",");
}
static bool named_byte(unsigned off) {
    for(const auto &f:army_fields)if(off>=f.offset && off<f.offset+f.size)return true;
    return false;
}
static unsigned army_owner(Obj army) {return army?fn<unsigned(__fastcall *)(Obj)>(GA(GA_22E690))(army):0;}
static unsigned emit_army(const char *cause,unsigned node,unsigned next,long long result,unsigned id,
                          const Snapshot &old_state,const Snapshot &now) {
    const unsigned char *o=old_state.army[id],*n=now.army[id];
    bool appeared=!old_state.army_ptr[id] && now.army_ptr[id],gone=old_state.army_ptr[id] && !now.army_ptr[id];
    bool replaced=old_state.army_ptr[id]!=now.army_ptr[id] ||
        read_field(o,kArmyLo,0x12,2)!=read_field(n,kArmyLo,0x12,2);
    bool any=false;
    for(const auto &f:army_fields)if(!f.volatile_field && read_field(o,kArmyLo,f.offset,f.size)!=read_field(n,kArmyLo,f.offset,f.size))any=true;
    // Unknown (unnamed) bytes are compared one by one and reported raw.
    unsigned raw_changed=0;
    for(unsigned off=kArmyLo;off<kArmyHi;++off)if(!named_byte(off) && o[off-kArmyLo]!=n[off-kArmyLo])++raw_changed;
    const unsigned ai_old=read_field(old_state.army_ai[id],0,0,4),ai_new=read_field(now.army_ai[id],0,0,4);
    const unsigned ai_target_old=read_field(old_state.army_ai[id],0,4,4),ai_target_new=read_field(now.army_ai[id],0,4,4);
    const bool ai_changed=ai_old!=ai_new || ai_target_old!=ai_target_new;
    if(!any && !raw_changed && !replaced && !ai_changed)return 0;
    // A removed army (pointer gone, or its slot cleared to commander 0) keeps its last state in the log.
    Obj army=now.army_ptr[id];const unsigned char *ctx=(gone || !read_field(n,kArmyLo,0x12,2))?o:n;
    unsigned commander=read_field(ctx,kArmyLo,0x12,2),position=read_field(ctx,kArmyLo,0x2A,2),troops=read_field(ctx,kArmyLo,0x58,2);
    unsigned force=army && !gone?army_owner(army):0,lines=0;
    auto line=[&](const char *field,unsigned a,unsigned b){
        header(cause,node,next,result,"army",id,commander,force,position,troops);text(field);text(",");number(a);text(",");number(b);army_context(ctx,force);text("\n");++lines;
    };
    if(appeared || (replaced && !gone)) {
        // A new or reused army: record its whole decision state once.
        line(appeared?"spawn":"reuse",0,0);
        for(const auto &f:army_fields) {
            header(cause,node,next,result,"army",id,commander,force,position,troops);
            text("init.");text(f.name);text(",");number(read_field(o,kArmyLo,f.offset,f.size));text(",");
            number(read_field(n,kArmyLo,f.offset,f.size));text(",\n");++lines;
        }
        return lines;
    }
    if(gone){line("gone",0,0);return lines;}
    for(const auto &f:army_fields) {
        unsigned a=read_field(o,kArmyLo,f.offset,f.size),b=read_field(n,kArmyLo,f.offset,f.size);
        if(a!=b && !f.volatile_field)line(f.name,a,b);
    }
    // 부대 판단표 행동 비트(1 독회피 2 인접거점 4 병참회복 8 병참단절 10 유리위치 80 인접명소) / 병참 단절 목표 부대.
    if(ai_old!=ai_new)line("ai_bits",ai_old,ai_new);
    if(ai_target_old!=ai_target_new)line("ai_target_army",ai_target_old,ai_target_new);
    for(unsigned off=kArmyLo;off<kArmyHi;++off) {
        unsigned a=o[off-kArmyLo],b=n[off-kArmyLo];
        if(named_byte(off) || a==b)continue;
        header(cause,node,next,result,"army",id,commander,force,position,troops);
        text("u8@");hex(off);text(",");number(a);text(",");number(b);text(",\n");++lines;
    }
    return lines;
}
static unsigned emit_troops(const char *cause,unsigned node,unsigned next,long long result,unsigned id,
                            const Snapshot &old_state,const Snapshot &now) {
    const unsigned char *o=old_state.troops[id],*n=now.troops[id];
    if(old_state.troops_ptr[id]==now.troops_ptr[id] && !memcmp(o,n,sizeof(now.troops[id])))return 0;
    unsigned lines=0;
    auto line=[&](const char *field,unsigned a,unsigned b){
        header(cause,node,next,result,"troops",id,0,0,0,id);text(field);text(",");number(a);text(",");number(b);
        troops_context(now.troops_ptr[id]?n:o);text("\n");++lines;
    };
    if(old_state.troops_ptr[id]!=now.troops_ptr[id])line(now.troops_ptr[id]?"spawn":"gone",0,0);
    for(const auto &f:troops_fields) {
        unsigned a=read_field(o,kTroopsLo,f.offset,f.size),b=read_field(n,kTroopsLo,f.offset,f.size);
        if(a!=b)line(f.name,a,b);
    }
    for(unsigned off=kTroopsLo;off<kTroopsHi;off+=2) {
        if(off==0x14 || off==0x16)continue;
        unsigned a=read_field(o,kTroopsLo,off,2),b=read_field(n,kTroopsLo,off,2);
        if(a==b)continue;
        header(cause,node,next,result,"troops",id,0,0,0,id);
        text("u16@");hex(off);text(",");number(a);text(",");number(b);text(",\n");++lines;
    }
    // 작업 목록 +18 u16[10] (경유 지역 중심 칸, 0xBD10 빈칸) as one readable line: filled counts, old → new.
    if(old_state.troops_ptr[id]==now.troops_ptr[id] && memcmp(o+0x18-kTroopsLo,n+0x18-kTroopsLo,20)) {
        auto filled=[](const unsigned char *b){unsigned c=0;for(unsigned i=0;i<10;++i)if(read_field(b,kTroopsLo,0x18+i*2,2)<48400)++c;return c;};
        auto list=[](const unsigned char *b){bool first=true;
            for(unsigned i=0;i<10;++i){unsigned h=read_field(b,kTroopsLo,0x18+i*2,2);if(h>=48400)continue;
                if(!first)text(">");first=false;area_name(hex_area(h));text("(");number(h);text(")");}
            if(first)text("-");};
        header(cause,node,next,result,"troops",id,0,0,0,id);text("work_list,");number(filled(o));text(",");number(filled(n));
        text(",목록 ");list(o);text(" → ");list(n);text("\n");++lines;
    }
    return lines;
}
static unsigned compare(const char *cause,unsigned node,unsigned next,long long result,const Snapshot &old_state,const Snapshot &now) {
    unsigned lines=0;
    for(unsigned id=1;id<=kMax;++id) {
        if(old_state.army_ptr[id]!=now.army_ptr[id] || memcmp(old_state.army[id],now.army[id],sizeof(now.army[id])) ||
           memcmp(old_state.army_ai[id],now.army_ai[id],sizeof(now.army_ai[id])))
            lines+=emit_army(cause,node,next,result,id,old_state,now);
        lines+=emit_troops(cause,node,next,result,id,old_state,now);
    }
    return lines;
}
// Once per game day: cities/gates whose owner changed or whose garrison moved by >=1000 or >=10%.
// Owner = building +13; garrison = City +3C via the game's own city lookup 32C130 (AI thread).
static unsigned city_owner[62],city_garrison[62];
static bool cities_seen;
static void cities() {
    Obj root=world();if(!root)return;
    for(unsigned id=1;id<=61;++id) {
        Obj b=table(0x6DA98,id,3000);if(!b)continue;
        unsigned owner=b[0x13],garrison=0;
        Obj city=fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_32E3B0))(root,id);
        if(city && *reinterpret_cast<unsigned short *>(city+0x4E)==id)garrison=*reinterpret_cast<unsigned *>(city+0x3C);
        unsigned old_owner=city_owner[id],old_garrison=city_garrison[id];
        unsigned delta=garrison>old_garrison?garrison-old_garrison:old_garrison-garrison;
        bool report=!cities_seen || owner!=old_owner || delta>=1000 || (old_garrison && delta*10>=old_garrison);
        city_owner[id]=owner;city_garrison[id]=garrison;
        if(!report)continue;
        header("city",~0u,~0u,-1,"city",id,0,owner,*reinterpret_cast<unsigned short *>(b+0x10),0);
        text(owner!=old_owner && cities_seen?"owner":"garrison");text(",");
        number(owner!=old_owner && cities_seen?old_owner:old_garrison);text(",");number(owner!=old_owner && cities_seen?owner:garrison);
        text(",");base_name(id);text("|소유 ");force_name(owner);text("|수비병");number(garrison);text("\n");
    }
    cities_seen=true;
}
// Once per game day (user 2026-11-10 "야전에 출진중인 부대들 결과를 기록"): every army on the map, one row:
// field status, old = position yesterday, new = position now, context = 무장|세력|병|사기|위치|명령…
// |이동 N칸(어제→오늘 칸 거리) |병력변화 |경유 |목표칸 |행동비트 |군세 임무·목표 |목록. Taken before the day's
// first BT node, so it is the outcome of the previous day's orders and movement.
static unsigned field_pos[kMax+1],field_men[kMax+1],field_commander[kMax+1];
static unsigned hex_distance(unsigned a,unsigned b) {   // same formula as combat_v2/hex.h
    if(a>=48400 || b>=48400)return ~0u;
    const int ax=static_cast<int>(a%220),bx=static_cast<int>(b%220);
    const int ay=static_cast<int>(a/220)-(ax+(ax&1))/2,by=static_cast<int>(b/220)-(bx+(bx&1))/2;
    auto ab=[](int v){return v<0?-v:v;};
    return static_cast<unsigned>((ab(ax-bx)+ab(ay-by)+ab(ax-bx+ay-by))/2);
}
static void field_armies() {
    for(unsigned id=1;id<=kMax;++id) {
        Obj a=table(kArmyTable,id,kMax);
        unsigned commander=a?*reinterpret_cast<unsigned short *>(a+0x12):0;
        if(!a || !commander){field_pos[id]=field_men[id]=field_commander[id]=0;continue;}
        const bool same=field_commander[id]==commander;
        const unsigned pos=*reinterpret_cast<unsigned short *>(a+0x2A),men=*reinterpret_cast<unsigned short *>(a+0x16);
        const unsigned tid=*reinterpret_cast<unsigned short *>(a+0x58);const unsigned force=army_owner(a);
        header("field",~0u,~0u,-1,"army",id,commander,force,pos,tid);
        text("status,");number(same?field_pos[id]:0);text(",");number(pos);
        unsigned char b[kArmyHi-kArmyLo];memcpy(b,a+kArmyLo,sizeof(b));army_context(b,force);
        text("|이동 ");if(same){unsigned d=hex_distance(field_pos[id],pos);if(d==~0u)text("-");else number(d);}else text("신규");
        text("|병력변화 ");if(same)signed_number(static_cast<int>(men)-static_cast<int>(field_men[id]));else text("-");
        text("|경유");for(unsigned i=0;i<5;++i){unsigned h=*reinterpret_cast<unsigned short *>(a+0x2C+i*2);if(h>=48400)break;
            text(" ");area_name(hex_area(h));text("(");number(h);text(")");}
        text("|목표칸 ");number(*reinterpret_cast<unsigned short *>(a+0x42));
        text("|행동비트 ");hex(*reinterpret_cast<unsigned *>(a+kArmyAi));text(" 대상부대 ");number(*reinterpret_cast<unsigned *>(a+kArmyAi+4));
        Obj t=tid && tid<=kMax?table(kTroopsTable,tid,kMax):nullptr;
        if(t) {
            text("|군세");number(tid);troops_context(t+kTroopsLo);
            text("|목록");bool any=false;
            for(unsigned i=0;i<10;++i){unsigned h=*reinterpret_cast<unsigned short *>(t+0x18+i*2);if(h>=48400)continue;
                text(" ");area_name(hex_area(h));text("(");number(h);text(")");any=true;}
            if(!any)text(" -");
        }
        text("\n");
        field_pos[id]=pos;field_men[id]=men;field_commander[id]=commander;
    }
}
static void day_check() {
    int day=trace_day();
    if(day==current_day)return;
    if(current_day>=0) {
        // Per-day call/change counts for every node that ran.
        for(unsigned i=0;i<kNodes;++i)if(calls[i]) {
            header("count",i,~0u,InterlockedExchange(&calls[i],0),"node",0,0,0,0,0);
            text("changes,0,");number(static_cast<unsigned>(InterlockedExchange(&changes[i],0)));text(",\n");
        }
    }
    current_day=day;
    cities();
    field_armies();
    flush();
}
// Departure target of CCommandFootholdAttackDepatureNode: node+60 final city/gate (+4E id).
static unsigned departure_target(unsigned index,Obj node) {
    if(trace_nodes[index].execute!=GA(GA_2FE10) || !node)return 0;
    Obj target=pointer(node,0x60);return target?*reinterpret_cast<unsigned short *>(target+0x4E):0;
}
static void before(unsigned index) {
    if(mod_steps){char stale[4];mod_steps(stale,sizeof(stale));}
    AcquireSRWLockExclusive(&lock);
    day_check();
    capture(*scratch);
    if(have_last)compare("between",last_node,index,-1,*last_state,*scratch);
    std::swap(last_state,scratch);have_last=true;
    ReleaseSRWLockExclusive(&lock);
}
static void after(unsigned index,Obj node,uint64_t result) {
    char steps[sizeof(step_label)]={};if(mod_steps)mod_steps(steps,sizeof(steps));
    AcquireSRWLockExclusive(&lock);
    memcpy(step_label,steps,sizeof(step_label));
    capture(*scratch);
    unsigned lines=compare("node",index,~0u,static_cast<long long>(result),*last_state,*scratch);
    std::swap(last_state,scratch);last_node=index;
    InterlockedIncrement(&calls[index]);if(lines)InterlockedIncrement(&changes[index]);
    if(trace_nodes[index].key) {
        header("call",index,~0u,static_cast<long long>(result),"node",0,0,0,0,0);
        text("departure_target,0,");unsigned target=departure_target(index,node);number(target);
        text(",");if(target){text("목표 ");base_name(target);}text("\n");
    }
    step_label[0]=0;
    if(buffer->size()>(1u<<20))flush();
    ReleaseSRWLockExclusive(&lock);
}
static uint64_t call_original(unsigned index,Obj node,Obj context,Obj subject) {
    uint64_t result=0;
    __try {result=originals[index](node,context,subject);}
    __finally {
        if(AbnormalTermination() && --depth==0)InterlockedDecrement(&active);
    }
    return result;
}
#ifdef RUNTIME_COST_PROFILE
// Timing build only (2026-10-23): per native node class, calls / inclusive / self / max time of its
// Execute, cumulative, rewritten every ~5 s to data/performance/node_cost_<pid>.csv. No snapshot,
// no trace CSV; the original runs unchanged with the same arguments.
static volatile LONG cost_mode;
static volatile LONG64 node_calls[kNodes],node_total[kNodes],node_self[kNodes],node_max[kNodes];
static thread_local LONGLONG cost_children[256];
static thread_local unsigned cost_depth;
static volatile LONG64 cost_written;
static void cost_write() {
    LONGLONG now=static_cast<LONGLONG>(GetTickCount64()),last=cost_written;
    if(now-last<5000 || InterlockedCompareExchange64(&cost_written,now,last)!=last)return;
    wchar_t path[MAX_PATH];if(!GetModuleFileNameW(self_module,path,MAX_PATH))return;
    wchar_t *slash=wcsrchr(path,L'\\');if(!slash)return;
    swprintf_s(slash+1,MAX_PATH-static_cast<size_t>(slash+1-path),L"AI_TacticsMod\\data\\performance\\node_cost_%lu.csv",GetCurrentProcessId());
    LARGE_INTEGER f;QueryPerformanceFrequency(&f);double ms=1000.0/static_cast<double>(f.QuadPart);
    std::string out="index,node,calls,total_ms,self_ms,max_ms\n";char line[256];
    for(unsigned i=0;i<kNodes;++i) {if(!node_calls[i])continue;
        _snprintf_s(line,sizeof(line),_TRUNCATE,"%u,%s,%lld,%.3f,%.3f,%.3f\n",i,trace_nodes[i].name,
            node_calls[i],node_total[i]*ms,node_self[i]*ms,node_max[i]*ms);out+=line;}
    HANDLE h=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)return;DWORD done=0;WriteFile(h,out.data(),static_cast<DWORD>(out.size()),&done,nullptr);CloseHandle(h);
}
static uint64_t cost_run(unsigned index,Obj node,Obj context,Obj subject) {
    unsigned level=cost_depth;if(level>=256)return originals[index](node,context,subject);
    cost_children[level]=0;++cost_depth;
    LARGE_INTEGER a,b;QueryPerformanceCounter(&a);
    uint64_t result=0;
    __try {result=originals[index](node,context,subject);}
    __finally {
        QueryPerformanceCounter(&b);LONGLONG spent=b.QuadPart-a.QuadPart;--cost_depth;
        InterlockedIncrement64(&node_calls[index]);InterlockedAdd64(&node_total[index],spent);
        InterlockedAdd64(&node_self[index],spent>cost_children[level]?spent-cost_children[level]:0);
        if(spent>node_max[index])node_max[index]=spent;
        if(level)cost_children[level-1]+=spent;
    }
    if(!level)cost_write();
    return result;
}
#endif
static uint64_t run(unsigned index,Obj node,Obj context,Obj subject) {
#ifdef RUNTIME_COST_PROFILE
    if(cost_mode)return cost_run(index,node,context,subject);
#endif
    if(!InterlockedCompareExchange(&installed,0,0))return originals[index](node,context,subject);
    if(!trace_nodes[index].watch) {   // domestic/diplomacy etc.: count only, no state comparison
        InterlockedIncrement(&calls[index]);return originals[index](node,context,subject);
    }
    bool top=depth++==0;
    if(top){InterlockedIncrement(&active);before(index);}
    const unsigned saved_node=cur_node;cur_node=index;   // fn rows name the innermost running node
    uint64_t result=call_original(index,node,context,subject);
    cur_node=saved_node;
    if(--depth==0){after(index,node,result);InterlockedDecrement(&active);}
    return result;
}
template<unsigned I> static uint64_t __fastcall wrap(Obj node,Obj context,Obj subject){ COST_SCOPE(trace_nodes[I].name);return run(I,node,context,subject);}
template<unsigned... I> static constexpr std::array<Exec,sizeof...(I)> wrappers(std::integer_sequence<unsigned,I...>) {return {&wrap<I>...};}
static const auto replacement=wrappers(std::make_integer_sequence<unsigned,kNodes>{});
// A slot may hold the original Execute or a hook published earlier by one of the mod's own
// DLLs (SAN14_CombatAI.dll / SAN14_Convenience.dll); the trace then wraps that hook, so mod
// effects are observed too. Anything else (another program's hook) rejects the trace.
static bool mod_module(const void *p) {
    HMODULE module=nullptr;wchar_t path[MAX_PATH];
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           static_cast<LPCWSTR>(p),&module) || !module)return false;
    if(module==self_module)return true;
    if(!GetModuleFileNameW(module,path,MAX_PATH))return false;
    const wchar_t *name=wcsrchr(path,L'\\');name=name?name+1:path;
    return !_wcsicmp(name,L"SAN14_CombatAI.dll") || !_wcsicmp(name,L"SAN14_Convenience.dll");
}
static bool (*accept_hook)(const void *)=&mod_module;
static Obj current[kNodes];
static bool chained[kNodes];
static bool guards_match() {
    for(unsigned i=0;i<kNodes;++i) {
        Obj now=pointer(image,trace_nodes[i].slot);
        if(now!=image+trace_nodes[i].execute && !accept_hook(now))return false;
    }
    return true;
}
static bool swap_slot(unsigned rva,Obj expected,Obj replacement_ptr) {
    auto slot=reinterpret_cast<void **>(image+rva);DWORD old=0,restored=0;
    if(!VirtualProtect(slot,8,PAGE_READWRITE,&old))return false;
    void *previous=InterlockedCompareExchangePointer(slot,replacement_ptr,expected);
    VirtualProtect(slot,8,old,&restored);
    return previous==expected;
}
#ifdef AI_TRACE_FIXTURE
static unsigned fail_after;
#endif
static bool open_output(const wchar_t *mode) {
    if(!output_path[0]) {
        wchar_t dir[MAX_PATH];
        if(!GetModuleFileNameW(self_module,dir,MAX_PATH))return false;
        wchar_t *slash=wcsrchr(dir,L'\\');if(!slash)return false;*slash=0;
        if(wcscat_s(dir,L"\\AI_TacticsMod\\data\\ai_trace"))return false;
        CreateDirectoryW(dir,nullptr);
        SYSTEMTIME t;GetLocalTime(&t);
        if(swprintf_s(output_path,L"%s\\trace_%s_%04u%02u%02u_%02u%02u%02u.csv",dir,mode,t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond)<0)return false;
    }
    file=CreateFileW(output_path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    const char head[]="seq,day,thread,cause,node,next,result,conc,kind,id,commander,force,hex,troops,field,old,new,context\n";
    DWORD done=0;return WriteFile(file,head,sizeof(head)-1,&done,nullptr)!=0;
}
#if !defined(NO_RUNTIME_LOG) && !defined(AI_TRACE_FIXTURE)
// ---- fn_trace (user 2026-11-10 "출진부터 야전지휘까지 관여하는 모든 원작함수에 호출기"): diagnostic builds,
// ai_trace_original=1 only (no mod hook anywhere). Each listed original function gets a 5-byte JMP at its
// entry to a wrapper that passes the same eight integer arguments to a trampoline (the displaced prologue
// instructions + JMP back) and returns its RAX unchanged; nothing in the game is written. The prologue bytes
// were decoded from the current plain EXE (tools/pelib.py): whole instructions, no RIP-relative or branch
// operand. A target whose entry bytes differ (another build or hook) is skipped and logged. Rows:
//   cause fn_in / fn, node = innermost traced BT node on the thread, next = function, field args,
//   old = caller RVA (return address), new = nesting depth, context = decoded arguments | after-state.
namespace fn {
using Fn8=uint64_t(__fastcall *)(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
// args: a army*, t troops*, A area* (+3A centre hex), h hex, i int, p raw pointer, o out army**, r int record (13 ints)
struct Target {unsigned rva;unsigned char len;unsigned char bytes[9];const char *name;const char *args;bool enter;};
static const Target targets[]={
    {GA(GA_2ECEA0),5,{0x48,0x89,0x5C,0x24,0x08},"2ECEA0_set_action","ai",false},
    {GA(GA_2ECF00),5,{0x48,0x89,0x5C,0x24,0x08},"2ECF00_set_target","aii",false},
    {GA(GA_2F05A0),5,{0x48,0x89,0x5C,0x24,0x08},"2F05A0_set_relay","aih",false},
    {GA(GA_CF570),6,{0x40,0x57,0x41,0x56,0x41,0x57},"CF570_prepend_relay","ah",false},
    {GA(GA_A3370),5,{0x48,0x89,0x5C,0x24,0x08},"A3370_clear_relays","a",false},
    {GA(GA_2EC570),5,{0x48,0x89,0x5C,0x24,0x18},"2EC570_retreat","aiii",false},
    {GA(GA_A33B0),7,{0x33,0xC0,0xBA,0x10,0xBD,0x00,0x00},"A33B0_clear_work_list","t",false},
    {GA(GA_A33E0),9,{0x4C,0x8B,0xC1,0x41,0xB9,0x10,0xBD,0x00,0x00},"A33E0_remove_work_item","ti",false},
    {GA(GA_CC160),7,{0x48,0x8B,0xC4,0x4C,0x89,0x48,0x20},"CC160_route_members","tpAAp",true},
    {GA(GA_B7DC0),5,{0x4C,0x89,0x4C,0x24,0x20},"B7DC0_route_to_area","aAppi",true},
    {GA(GA_D1760),5,{0x4C,0x89,0x44,0x24,0x18},"D1760_plan_cut_logistics","apo",true},
    {GA(GA_C5AC0),6,{0x40,0x55,0x56,0x57,0x41,0x54},"C5AC0_plan_advantage_move","pa",true},
    {GA(GA_CD5F0),6,{0x48,0x8B,0xC4,0x57,0x41,0x54},"CD5F0_plan_occupy_neighbour","pappii",true},
    {GA(GA_CED10),6,{0x40,0x57,0x48,0x83,0xEC,0x20},"CED10_plan_recover_logistics","pa",true},
    {GA(GA_C0390),5,{0x48,0x89,0x5C,0x24,0x08},"C0390_path_to_list","pip",true},
    {GA(GA_27D490),7,{0x48,0x8B,0xC4,0x48,0x89,0x48,0x08},"27D490_facility_handoff","ta",true},
    {GA(GA_43A50),5,{0x48,0x89,0x5C,0x24,0x08},"43A50_make_sortie_order","pppri",true},
    {GA(GA_1EDC30),6,{0x40,0x55,0x56,0x57,0x41,0x54},"1EDC30_run_sortie_order","ri",true},
    // Purpose not settled by static analysis (user 2026-11-10 "원인을 모르던 함수들까지 추적해서 기능을 확정"):
    // 61F10 stage functions (node, troops, ctx, node state), two unnamed army-tree nodes (bits 0x20/0x40),
    // the 65A60 helpers and the daily 2E6E00 that reads the cut-logistics bit.
    {GA(GA_720E0),5,{0x4C,0x89,0x4C,0x24,0x20},"720E0_siege_stage","ptps",true},
    {GA(GA_73E60),7,{0x48,0x8B,0xC4,0x4C,0x89,0x48,0x20},"73E60_siege_stage","ptps",true},
    {GA(GA_738A0),7,{0x48,0x8B,0xC4,0x4C,0x89,0x48,0x20},"738A0_siege_stage","ptps",true},
    {GA(GA_72D30),7,{0x48,0x8B,0xC4,0x4C,0x89,0x48,0x20},"72D30_siege_stage","ptps",true},
    {GA(GA_704A0),6,{0x48,0x8B,0xC4,0x55,0x41,0x54},"704A0_siege_stage","ptps",true},
    {GA(GA_72960),6,{0x48,0x8B,0xC4,0x55,0x41,0x54},"72960_siege_stage","ptps",true},
    {GA(GA_1F760),6,{0x40,0x53,0x48,0x83,0xEC,0x20},"1F760_army_node_bit20","npp",true},
    {GA(GA_1FEE0),6,{0x40,0x53,0x48,0x83,0xEC,0x20},"1FEE0_army_node_bit40","npp",true},
    {GA(GA_CCDF0),7,{0x48,0x8B,0xC4,0x4C,0x89,0x40,0x18},"CCDF0_area_attack_helper","xxxx",true},
    {GA(GA_CAA60),7,{0x48,0x8B,0xC4,0x4C,0x89,0x40,0x18},"CAA60_area_attack_helper","xxxx",true},
    {GA(GA_2E6E00),6,{0x48,0x8B,0xC4,0x55,0x41,0x54},"2E6E00_daily_cut_report","xx",true},
};
constexpr unsigned kTargets=sizeof(targets)/sizeof(targets[0]);
static Fn8 tramp[kTargets];
static bool hooked[kTargets];
static thread_local unsigned fdepth;
static volatile LONG rows;
static unsigned army_slot(Obj p) {   // 22DA10: (army - Army[1]) / 512 + 1, checked against the table
    Obj first=table(kArmyTable,1,kMax);
    if(!p || !first || p<first)return 0;
    const uintptr_t d=static_cast<uintptr_t>(p-first);
    const unsigned id=(d%512)?0:static_cast<unsigned>(d/512)+1;
    return id && id<=kMax && table(kArmyTable,id,kMax)==p?id:0;
}
static unsigned troops_slot(Obj p) {
    if(!p)return 0;
    for(unsigned id=1;id<=kMax;++id)if(table(kTroopsTable,id,kMax)==p)return id;
    return 0;
}
static void relays(Obj a) {
    text("|경유");for(unsigned i=0;i<5;++i){unsigned h=*reinterpret_cast<unsigned short *>(a+0x2C+i*2);if(h>=48400)break;text(" ");number(h);}
    text("|목표칸 ");number(*reinterpret_cast<unsigned short *>(a+0x42));
    text("|행동비트 ");hex(*reinterpret_cast<unsigned *>(a+kArmyAi));
}
static void work_list(Obj t) {
    text("|목록");bool any=false;
    for(unsigned i=0;i<10;++i){unsigned h=*reinterpret_cast<unsigned short *>(t+0x18+i*2);if(h>=48400)continue;
        text(" ");area_name(hex_area(h));text("(");number(h);text(")");any=true;}
    if(!any)text(" -");
}
static void emit(unsigned i,const uint64_t *a,uintptr_t caller,bool enter,uint64_t result) {
    if(!buffer || !InterlockedCompareExchange(&installed,0,0) || disk_full)return;
    const Target &t=targets[i];
    AcquireSRWLockExclusive(&lock);
    Obj army=nullptr,troops=nullptr;
    for(unsigned k=0;t.args[k] && k<8;++k) {
        if(t.args[k]=='a' && !army)army=reinterpret_cast<Obj>(a[k]);
        if(t.args[k]=='t' && !troops)troops=reinterpret_cast<Obj>(a[k]);
        if(t.args[k]=='x') {   // unknown argument: an army or troops pointer is recognised by its table slot
            Obj p=reinterpret_cast<Obj>(a[k]);
            if(!army && army_slot(p))army=p;else if(!troops && troops_slot(p))troops=p;
        }
    }
    const unsigned aid=army_slot(army),tid=troops?troops_slot(troops):(aid?*reinterpret_cast<unsigned short *>(army+0x58):0);
    number(++sequence);text(",");signed_number(current_day);text(",");number(GetCurrentThreadId());text(",");
    text(enter?"fn_in":"fn");text(",");text(cur_node<kNodes?trace_nodes[cur_node].name:"-");text(",");text(t.name);text(",");
    if(enter)text("");else number(result);text(",");text(active>1?"1":"0");text(",");
    if(aid) {
        text("army,");number(aid);text(",");number(*reinterpret_cast<unsigned short *>(army+0x12));text(",");
        number(army_owner(army));text(",");number(*reinterpret_cast<unsigned short *>(army+0x2A));text(",");number(tid);text(",");
    } else {text(troops?"troops,":"call,");number(tid);text(",0,0,0,");number(tid);text(",");}
    text("args,");number(static_cast<unsigned long long>(caller-reinterpret_cast<uintptr_t>(image)));text(",");number(fdepth);text(",");
    for(unsigned k=0;t.args[k] && k<8;++k) {
        if(k)text(" ");
        const uint64_t v=a[k];
        switch(t.args[k]) {
        case 'a':{unsigned id=army_slot(reinterpret_cast<Obj>(v));text("부대");number(id);break;}
        case 't':text("군세");number(troops_slot(reinterpret_cast<Obj>(v)));break;
        case 'A':if(v){Obj ar=reinterpret_cast<Obj>(v);unsigned h=*reinterpret_cast<unsigned short *>(ar+0x3A);
                     text("지역 ");area_name(hex_area(h));text("(");number(h);text(")");}else text("지역 -");break;
        case 'h':{unsigned h=static_cast<unsigned>(v)&0xFFFF;text("칸");number(h);if(h<48400){text("(");area_name(hex_area(h));text(")");}break;}
        case 'i':signed_number(static_cast<int>(v));break;
        case 'o':if(!enter && v){Obj target=*reinterpret_cast<Obj *>(v);text("결과부대");number(army_slot(target));}else text("out");break;
        case 'r':if(v){const int *r=reinterpret_cast<const int *>(v);text("rec[");
                     for(unsigned j=0;j<13;++j){if(j)text(" ");signed_number(r[j]);}text("]");}else text("rec -");break;
        case 's':if(v){Obj s=reinterpret_cast<Obj>(v);   // 61F10 node state (B27D70): +08 current / +10 next relay area, stage bytes, +30 hex
                     auto centre=[&](unsigned off){Obj ar=*reinterpret_cast<Obj *>(s+off);
                         if(ar){unsigned hh=*reinterpret_cast<unsigned short *>(ar+0x3A);area_name(hex_area(hh));text("(");number(hh);text(")");}else text("-");};
                     text("상태[현재 ");centre(0x08);text(" 다음 ");centre(0x10);
                     const unsigned offs[]={0x18,0x1C,0x1D,0x1E,0x1F,0x24,0x28,0x2C};
                     for(unsigned off:offs){text(" +");hex(off);text("=");number(s[off]);}
                     text(" +30=");number(*reinterpret_cast<unsigned *>(s+0x30));text("]");}else text("상태 -");break;
        case 'n':if(v){__try {   // node object: RTTI class name from the vtable's complete-object locator
                     Obj vt=*reinterpret_cast<Obj *>(v);Obj col=*reinterpret_cast<Obj *>(vt-8);
                     const char *name=reinterpret_cast<const char *>(image+*reinterpret_cast<unsigned *>(col+12)+0x10);
                     char buf[96];unsigned n=0;while(n<95 && name[n] && name[n]!=','){buf[n]=name[n];++n;}buf[n]=0;text(buf);
                 } __except(EXCEPTION_EXECUTE_HANDLER){text("node?");}}else text("node -");break;
        case 'x':{Obj p=reinterpret_cast<Obj>(v);unsigned id=army_slot(p);
                     if(id){text("부대");number(id);break;}
                     id=troops_slot(p);if(id){text("군세");number(id);break;}
                     hex(static_cast<unsigned>(v));break;}
        default:hex(static_cast<unsigned>(v));break;
        }
    }
    if(!enter && aid){unsigned char b[kArmyHi-kArmyLo];memcpy(b,army+kArmyLo,sizeof(b));army_context(b,army_owner(army));relays(army);}
    if(troops){troops_context(troops+kTroopsLo);work_list(troops);}
    text("\n");
    InterlockedIncrement(&rows);
    if(buffer->size()>(1u<<20))flush();
    ReleaseSRWLockExclusive(&lock);
}
template<unsigned I> static uint64_t __fastcall wrap(uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t e,uint64_t f,uint64_t g,uint64_t h) {
    const uint64_t args[8]={a,b,c,d,e,f,g,h};
    const uintptr_t caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    if(targets[I].enter)emit(I,args,caller,true,0);
    ++fdepth;const uint64_t result=tramp[I](a,b,c,d,e,f,g,h);--fdepth;
    emit(I,args,caller,false,result);
    return result;
}
template<unsigned... I> static constexpr std::array<void *,sizeof...(I)> wrap_table(std::integer_sequence<unsigned,I...>) {
    return {reinterpret_cast<void *>(&wrap<I>)...};
}
static const auto wrappers=wrap_table(std::make_integer_sequence<unsigned,kTargets>{});
static void absolute(Obj at,const void *to) {   // FF 25 00000000 <abs64>
    at[0]=0xFF;at[1]=0x25;memset(at+2,0,4);memcpy(at+6,&to,8);
}
static Obj near_page(unsigned bytes) {
    const uintptr_t base=reinterpret_cast<uintptr_t>(image);
    for(uintptr_t step=0x10000;step<0x70000000;step+=0x10000) {
        for(int dir=-1;dir<=1;dir+=2) {
            const uintptr_t at=dir<0?((base>step)?base-step:0):base+step;
            if(!at)continue;
            if(void *m=VirtualAlloc(reinterpret_cast<void *>(at&~uintptr_t(0xFFFF)),bytes,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE))return static_cast<Obj>(m);
        }
    }
    return nullptr;
}
static void install() {
    Obj page=near_page(0x1000);
    if(!page){header("meta",~0u,~0u,-1,"fn_trace",0,0,0,0,0);text("no_page,0,0,\n");return;}
    for(unsigned i=0;i<kTargets;++i) {
        const Target &t=targets[i];Obj entry=t.rva?image+t.rva:nullptr;
        Obj stub=page+i*64,trampoline=stub+16;   // stub: JMP wrapper (14); trampoline: prologue + JMP back
        const bool match=entry && !memcmp(entry,t.bytes,t.len);
        header("meta",~0u,~0u,-1,"fn_trace",i,0,0,0,0);text(t.name);text(",");number(t.rva);text(",");text(match?"1":"0");text(",\n");
        if(!match)continue;
        absolute(stub,wrappers[i]);
        memcpy(trampoline,entry,t.len);absolute(trampoline+t.len,entry+t.len);
        tramp[i]=reinterpret_cast<Fn8>(trampoline);
        const intptr_t rel=reinterpret_cast<intptr_t>(stub)-reinterpret_cast<intptr_t>(entry+5);
        if(rel<INT32_MIN || rel>INT32_MAX)continue;
        unsigned char patch[9];patch[0]=0xE9;const int32_t r32=static_cast<int32_t>(rel);memcpy(patch+1,&r32,4);
        for(unsigned k=5;k<t.len;++k)patch[k]=0x90;
        DWORD old=0,restored=0;
        if(!VirtualProtect(entry,t.len,PAGE_EXECUTE_READWRITE,&old))continue;
        memcpy(entry,patch,t.len);
        VirtualProtect(entry,t.len,old,&restored);
        FlushInstructionCache(GetCurrentProcess(),entry,t.len);
        hooked[i]=true;
    }
    DWORD old=0;VirtualProtect(page,0x1000,PAGE_EXECUTE_READ,&old);FlushInstructionCache(GetCurrentProcess(),page,0x1000);
    unsigned n=0;for(unsigned i=0;i<kTargets;++i)if(hooked[i])++n;
    header("meta",~0u,~0u,-1,"fn_trace",n,0,0,0,0);text("hooked,");number(n);text(",");number(kTargets);text(",\n");
    flush();
}
}
#endif
// original=true: no mod hook is published (ai_trace_original). Meta lines record the mode and
// every node whose slot already held a mod hook (chained).
static bool install(bool original,bool cost=false) {
    if(InterlockedCompareExchange(&installed,0,0) || !guards_match())return false;
#ifdef RUNTIME_COST_PROFILE
    if(cost) {
        for(unsigned i=0;i<kNodes;++i) {current[i]=pointer(image,trace_nodes[i].slot);originals[i]=reinterpret_cast<Exec>(current[i]);}
        InterlockedExchange(&cost_mode,1);unsigned done=0;
        for(;done<kNodes;++done)if(!swap_slot(trace_nodes[done].slot,current[done],reinterpret_cast<Obj>(replacement[done])))break;
        if(done!=kNodes) {while(done){--done;swap_slot(trace_nodes[done].slot,reinterpret_cast<Obj>(replacement[done]),current[done]);}return false;}
        return true;
    }
#endif
    (void)cost;
    last_state=new Snapshot();scratch=new Snapshot();buffer=new std::string();buffer->reserve(2u<<20);
    if(!open_output(original?L"original":L"mod"))return false;
    if(!original)if(HMODULE combat=GetModuleHandleW(L"SAN14_CombatAI.dll")) {
        mod_steps=reinterpret_cast<StepFn>(GetProcAddress(combat,"SAN14CombatTraceStep"));
        if(mod_steps)mod_steps(nullptr,0);
    }
    for(unsigned i=0;i<kNodes;++i) {
        current[i]=pointer(image,trace_nodes[i].slot);chained[i]=current[i]!=image+trace_nodes[i].execute;
        originals[i]=reinterpret_cast<Exec>(current[i]);
    }
    unsigned published=0;
    for(;published<kNodes;++published) {
#ifdef AI_TRACE_FIXTURE
        if(fail_after && published==fail_after)break;
#endif
        if(!swap_slot(trace_nodes[published].slot,current[published],reinterpret_cast<Obj>(replacement[published])))break;
    }
    if(published!=kNodes) {
        while(published){--published;swap_slot(trace_nodes[published].slot,reinterpret_cast<Obj>(replacement[published]),current[published]);}
        CloseHandle(file);file=INVALID_HANDLE_VALUE;return false;
    }
    header("meta",~0u,~0u,-1,"mode",0,0,0,0,0);text(original?"original":"mod");text(",0,0,\n");
    for(unsigned i=0;i<kNodes;++i)if(chained[i]){header("meta",i,~0u,-1,"node",0,0,0,0,0);text("chained_mod_hook,0,1,\n");}
    flush();
    InterlockedExchange(&installed,1);
#if !defined(NO_RUNTIME_LOG) && !defined(AI_TRACE_FIXTURE)
    if(original)fn::install();   // pure original AI only: function-level call rows
#endif
    return true;
}
}
