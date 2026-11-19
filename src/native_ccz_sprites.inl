// 조조전 병종 그림 (2026-10-31 사용자: 2D 모드에 의존하지 않고 우리 모드가 직접 그린다).
// 설계·근거: docs/CCZ_SPRITES.md. 자산: assets/ccz/ccz_units.bin (CCZU v2, tools/ccz_extract_units.py).
// 게임 스레드: 부대 표시 객체 vtable 5번(부대 모형 제출 1C6EC0)에서 위치·방향·상태·진영과 카메라를
// 읽기만 하고 복사한다. 대상 부대는 원래 제출을 건너뛰어 3D 모형을 숨긴다(깃발 슬롯은 그대로).
// 렌더 스레드: Present 직전 복사본과 우리 GPU 자원만으로 빌보드를 그린다. 게임 함수 호출 없음.
#include "game_addresses.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "ccz_appearance.h"
#include "ccz_catalog.h"
#include "ccz_sprites_vs.h"
#include "ccz_sprites_ps.h"
namespace ccz_sprites {
const unsigned kDisplayVtable=GA(GA_13A33E8),kSubmitSlot=GA(GA_13A3410),kSubmit=GA(GA_1C8240),kSubmitSize=0x60;
const unsigned long long kSubmitHash=GH(0x0BC927F835180669ull);   // FNV-1a of 1C6EC0..+60 (exe = running .text)
const unsigned kArmyVtable=GA(GA_1345458);                          // Army record, Army+148 = display
const unsigned kCameraRoot=GA(GA_27D4258),kCameraVtable=GA(GA_16B62E0);
// CCZU v2 entry: move 11x48x48 | attack 12x64x64 | special 5x48x48, 8-bit palette index, 0 = clear.
constexpr unsigned kRawSize=48*48*11+64*64*12+48*48*5;
constexpr unsigned kAtlasW=64,kAtlasH=11*48+12*64+5*48,kAttackRow=11*48,kSpecialRow=kAttackRow+12*64;
constexpr unsigned kSlots=512,kTextures=512,kQuads=kSlots*3;   // unit + up to two state labels
// 2026-11-04 user: 256 -> 512 cached cells. Lazy GPU allocation: 96 KiB per atlas, at most 48 MiB.
// 2026-10-31 user: 막기·회심 were hard to notice -> "막기!" / "회심!" labels over armies in that state.
// Rendered once with GDI (Malgun Gothic) into an R8_UINT mask: row 0 막기, row 1 회심, 1 = glyph.
constexpr unsigned kLabelW=64,kLabelH=20,kLabelCell=0xFFFFFFFEu;
// Palette rows: 0 neutral (original green marks), 1+k = force colour k (Force+49, 62 colours), 63 Red Hare.
// 2026-10-31 user: force colours, plus a contrasting outline so units do not sink into the ground.
const unsigned kRows=64,kForceColours=GA(GA_1A29590),kColourCount=62,kRedHare=63;   // 2ACF00 mode 0 -> 32C220 table (RRGGBBAA)
constexpr ULONGLONG kBlockMs=700,kHurtMs=450,kCritMs=700;
// 2026-11-01 user: attack / hurt motions changed too often -> slower attack frames, one hurt motion per 1.5 s.
constexpr ULONGLONG kAttackFrameMs=260,kHurtCooldownMs=1500;
// 2026-10-31 user: the 5-day state showed on armies far from any enemy -> count-based instead: a triggered
// 막기 blocks 5 normal attacks, a triggered 회심 covers 5 normal attacks; the triggering hit is the first.
// 2026-11-01 user: still shown with no enemy around -> the state also ends once the army goes a whole game day
// without a normal attack exchange (last exchange day + 1 is the last day shown).
constexpr int kCharges=8;
// Absolute game day, same arithmetic as 32BA50(context+34): (year*12+month)*30+day-31 (pure read).
static int game_day_read() {
    Obj root=world();Obj context=root?pointer(root,0x871E8):nullptr;if(!context)return -1;
    Obj d=context+0x34;return (static_cast<int>(*reinterpret_cast<short *>(d))*12+static_cast<signed char>(d[2]))*30+static_cast<signed char>(d[3])-31;
}
struct Buff {Obj army;unsigned leader;int guard,fury,last;};   // charges left, last exchange day; leader pins slot reuse
static Buff buffs[128];static SRWLOCK buff_lock=SRWLOCK_INIT;
static bool buff_fresh(const Buff &b,int day) {return day<0 || b.last<0 || day<=b.last+1;}
static void buff_read(Obj army,int day,bool &guard,bool &fury) {
    guard=fury=false;
    AcquireSRWLockShared(&buff_lock);
    for(const auto &b:buffs)if(b.army==army && b.leader==word(army,0x12)){bool on=buff_fresh(b,day);guard=on && b.guard>0;fury=on && b.fury>0;break;}
    ReleaseSRWLockShared(&buff_lock);
}
// Game logic thread (ccz_combat): trigger -> 4 left, otherwise spend one; any exchange refreshes the day.
static void buff_use(Obj army,int day,bool guard,bool trigger) {
    unsigned leader=word(army,0x12);
    AcquireSRWLockExclusive(&buff_lock);
    Buff *slot=nullptr;
    for(auto &b:buffs) {
        if(b.army==army && b.leader==leader){slot=&b;break;}
        if(!slot && trigger && (!b.army || (b.guard<=0 && b.fury<=0) || !buff_fresh(b,day)))slot=&b;   // empty, used up or stale
    }
    if(slot && (trigger || (slot->army==army && slot->leader==leader))) {
        if(slot->army!=army || slot->leader!=leader)*slot=Buff{army,leader,0,0,-1};
        if(!buff_fresh(*slot,day))slot->guard=slot->fury=0;   // a stale state never carries into a new fight
        int &left=guard?slot->guard:slot->fury;left=trigger?kCharges-1:(left>0?left-1:0);slot->last=day;
    }
    ReleaseSRWLockExclusive(&buff_lock);
}
// 2026-10-31 user: class = formation (Army+1B, 22B090) x leader 통솔 tier (<70, 70-89, 90+); the force
// ruler (Force+10) and strategist (Force+52) override field formations, land transport (19) uses carts.
// Values are 신조조전 class ids (research/cao_cao_den/tables/classes_sin.csv, Ekd5.exe 0x14E970 names);
// cell = id*3 (colour variant 0). 장사 적병 -> 산적 (this version's name). 0xFF = keep the native model.
constexpr unsigned char kNative=0xFF;
constexpr unsigned char kFormationClass[21][3]={
    {3,4,5},{9,40,41},{19,34,35},{13,14,29},{3,4,5},{75,76,77},{48,49,50},{31,31,31},{141,141,141},
    {kNative,kNative,kNative},{42,43,44},                                 // 0..10: -,어린,봉시,안행,방원,학익,장사,추행,정란,충차,투석
    {kNative,kNative,kNative},{kNative,kNative,kNative},{kNative,kNative,kNative},{kNative,kNative,kNative},
    {kNative,kNative,kNative},{kNative,kNative,kNative},{kNative,kNative,kNative},{kNative,kNative,kNative},
    {81,82,83},{kNative,kNative,kNative}};                                // 11..20: water, 사이, ships; 19 수송
// 2026-11-01 user: the force strategist rides as a mounted strategist (책사교위/참모중랑/군사장군, was 부관 line).
constexpr unsigned char kRulerClass[3]={0,1,2},kStrategistClass[3]={75,76,77};   // 군웅/영웅/제왕, 책사교위/참모중랑/군사장군
// 2026-11-01 user: named generals use their 신조조전 personal class (160~180: 위무제=조조, 무신=여포 ...), ahead of
// the ruler/strategist classes and fixed whatever the 통솔. Cells 720+ are tiered generic classes, not generals
// (local11/12 mistake). Other generals use their 신조조전 personal picture: Ekd5.exe 0x501000 holds R형상 per person
// and 0x58CC88 maps it (R<33: 720+(R-1)*3+tier, tier fixed at 2 here; R>=33: R+783).
// {person id (Person+10), cell}, sorted; data/ccz_generals.tsv.
struct General {unsigned short id,cell;};
constexpr General kGenerals[]={{16,752},{29,900},{32,922},{34,901},{35,809},{37,899},{92,776},{95,773},{102,740},{105,889},{114,734},{124,725},{144,920},{147,495},{161,843},{162,856},{176,881},{179,522},{183,837},{185,834},{186,894},{189,828},{203,859},{208,865},{215,516},{219,923},{231,839},{247,878},{254,525},{271,510},{281,893},{344,819},{371,838},{373,873},{379,531},{383,862},{393,767},{402,779},{405,845},{412,874},{432,868},{440,534},{442,749},{449,871},{464,892},{511,746},{515,816},{518,731},{521,480},{526,817},{535,891},{551,886},{553,866},{557,486},{586,882},{596,501},{607,540},{617,884},{618,737},{631,875},{640,528},{656,498},{658,844},{663,728},{671,912},{689,770},{703,885},{709,519},{736,913},{765,841},{766,848},{769,504},{787,846},{812,895},{822,888},{827,829},{828,877},{830,818},{842,782},{845,861},{902,803},{904,850},{913,743},{952,483},{958,864},{959,836},{962,764},{968,835},{974,872},{987,492},{988,869},{997,867}};
[[maybe_unused]] static unsigned general_cell(unsigned id) {
    const General *p=std::lower_bound(std::begin(kGenerals),std::end(kGenerals),id,[](const General &g,unsigned v){return g.id<v;});
    return p!=std::end(kGenerals) && p->id==id?p->cell:0u;
}
static SRWLOCK appearance_lock=SRWLOCK_INIT;
static ccz_appearance::Settings appearance;
static void reset_appearance_captures();
static void reload_appearance() {
    ccz_appearance::Settings next;wchar_t path[MAX_PATH],text[2048]={};
    if(mod_ini_path(path,L"기능설정.ini"))GetPrivateProfileStringW(ccz_appearance::section,ccz_appearance::key,L"",text,2048,path);
    bool parsed=ccz_appearance::decode(text,next);
#ifndef NO_RUNTIME_LOG
    if(*text && !parsed)log_event("ccz_appearance_invalid",1,0,0);
#else
    (void)parsed;
#endif
    AcquireSRWLockExclusive(&appearance_lock);appearance=next;ReleaseSRWLockExclusive(&appearance_lock);
    reset_appearance_captures(); // opted-in far keep must not retain a previous profile's cached cell
#ifndef NO_RUNTIME_LOG
    log_event("ccz_appearance_loaded",next.low,next.high,parsed?1u:0u);
#endif
}
// 2026-11-01 user: the army flag (commander portrait) hid the big CCZ sprites -> smaller (9 -> 6.5). The ground
// offset toward the camera (local9, 300 units) moved armies off their hex -> removed, and a bit smaller (5.8).
// 2026-11-01 user: back to the big size (9, 1/9); the flag parts move away instead (unit_flag layout patch).
constexpr float kPixel=9.0f,kTowardCamera=250.0f;                  // world units per CCZ pixel; depth margin
constexpr float kMinScreen=1.0f/10,kMaxScale=3.0f;                // 48px frame >= 1/10 screen height when zoomed out (user: 1/9 too big)
using Submit=void(__fastcall *)(Obj,Obj,Obj);
static Submit next_submit;   // Layout manager: next of 13A03D0 (original 1C6EC0 unless another mod chained first)
struct Unit {
    Obj army=nullptr;ULONGLONG seen=0,moved=0;
    float x=0,y=0,z=0,yaw=0,fx=0,fz=0;bool facing=false;
    unsigned state=0,team=0,cell=0,morale=100,troops=0;
    ULONGLONG hurt_at=0,block_at=0,crit_at=0;   // 막기·피격·회심 연출 (ccz_combat marks, troop drops)
    bool guard=false,fury=false;                // 막기·회심 상태 유지 중 (ccz_combat, 5일)
    // 2026-11-16 user (CCZ units vanish and the 3D model is gone too, never back until CCZ is turned off): a unit is hidden
    // only while its own picture went out in a recent batch. first = slot start (short trial before the first draw),
    // drawn_at = last batch that carried this unit with a loaded picture (render thread writes it back), slot = its index.
    ULONGLONG first=0,drawn_at=0;unsigned slot=0;
    ULONGLONG sampled=0;   // last idle-band grid sample taken from this unit (ghost::sample)
    // 2026-11-19 idle band flag (ghost::collect): 0 = not a flag, 1 = the grown part (texel rows lo..hi, bottom), 2 = the part still
    // to grow (rows lo..hi, top, drawn pale with the 0x200 bit).
    unsigned badge=0,lo=0,hi=0;
};
// Idle band flag cells: kBadgeTag | the source picture cell (a disc built from its standing frame, ghost::badge_atlas).
constexpr unsigned kBadgeTag=0xB0000000u,kBadgeMask=0xFFFF0000u,kBadgeSize=64;
static bool badge_cell(unsigned cell) {return (cell&kBadgeMask)==kBadgeTag;}
static unsigned source_cell(unsigned cell) {return badge_cell(cell)?cell&0xFFFFu:cell;}
static volatile LONG idle_only;   // 2026-11-19: renderer installed for the idle band flags alone (조조전 병종 그림 off)
constexpr ULONGLONG kProofMs=1000,kTrialMs=700;
constexpr unsigned kNoOwner=~0u;   // label quads belong to no unit
struct Vertex {float x,y,z,w,u,v;unsigned team;};
static bool native_facing(Unit &u,float yaw) {
    if(!std::isfinite(yaw))return false;
    u.yaw=yaw;u.fx=std::sin(yaw);u.fz=std::cos(yaw);u.facing=true;return true;
}
static SRWLOCK lock=SRWLOCK_INIT;
static Unit units[kSlots];
// Pictures we could not decode/upload; guarded by lock. 2026-11-16: retried with back-off (2 s .. 60 s) instead of staying
// failed until a new device (a short GPU memory shortage used to leave the unit hidden and undrawn for the whole session).
struct CellFail {ULONGLONG retry;unsigned tries;};
static std::unordered_map<unsigned,CellFail> failed_cells;
static float camera[80];static ULONGLONG camera_seen;
static void reset_appearance_captures() {
    AcquireSRWLockExclusive(&lock);for(auto &u:units)u=Unit{};camera_seen=0;ReleaseSRWLockExclusive(&lock);
    // Combat charges, GPU assets and the local14 render hooks are intentionally independent.
}
static volatile LONG installed,hide_model=1,depth_test=1;
// 2026-11-09 user: the native model must show unless our sprite is actually being drawn (PW6).
// last_draw_ok = last draw() that really submitted vertices; hide_since = first capture tick of the current hide episode.
static volatile LONGLONG last_draw_ok,hide_since;
// 2026-11-16: last_submit = last draw() that submitted a batch (no GPU confirmation). The per-unit proof below is always on;
// sprite_fallback (strict) additionally needs the batch's occlusion query to have passed pixels (last_draw_ok, local61).
static volatile LONGLONG last_submit;
static volatile LONG submit_count;   // batches submitted (render-thread draw-gap watchdog)
// 2026-11-16: 30 batch queries in a row passed no pixel (every sprite failing depth or covered, the local7 failure class).
// Without strict mode the submits alone would keep the model hidden; this clears with the next batch that passes a pixel.
static volatile LONG covered;
static bool fallback_enabled;   // local17 [ccz] sprite_fallback (startup-only); 2026-11-16: strict GPU check on top of the per-unit proof
#ifndef NO_RUNTIME_LOG
// 2026-11-16 diagnostic builds only: [diagnostics] ccz_sprite_fault forces a failure to check that the native model returns.
// 1 = never draw, 2 = every sprite fails the depth test (drawn but invisible), 4 = every picture fails to load.
static unsigned sprite_fault;
// 2026-11-16 measurement build: [diagnostics] ccz_draw_probe (probe namespace below). probe_show = this 4 s phase lets every
// native model through; probe_hidden counts the hide decisions of the other phase.
static bool probe_mode;static volatile LONG probe_show,probe_hidden;
// bound at this thread's last bind: 0 no colour target, 1 scene target before post, 2 other before post, 3 after post.
// local196 log: the scene binds come from another thread (16708) than Present (38276), so the pass is kept per thread.
static thread_local unsigned probe_pass;
#endif
static std::vector<unsigned char> asset;
static std::unordered_map<unsigned,std::pair<unsigned,unsigned>> entries; // cell -> (offset, packed size)
static unsigned original_palette[kRows][256];
static unsigned palette[kRows][256];                                   // index 0 = outline colour of the row
#ifndef NO_RUNTIME_LOG
static volatile LONG captures,replaced,yaw_logged;
static bool zoom_recovery_pending,has_submitted_sprites;
static unsigned zoom_recovery_logs;
static ULONGLONG last_sprite_submission;
// PW6 health line: one per 10 s while sprites are installed (at most 2000 lines).
// fallback = captures that would have hidden but showed the native model; low 16 bits total,
// bits 16-23 global proof missing (strict: no passing query), bits 24-31 this unit not drawn (failed cell, full slots, no batch).
static volatile LONG health_hidden,health_drawn,health_fallback1,health_fallback23,health_lines;
static ULONGLONG health_next;
static void health_tick() {
    ULONGLONG now=GetTickCount64();
    if(!health_next){health_next=now+10000;return;}
    if(now<health_next)return;
    health_next=now+10000;
    LONG lines=InterlockedIncrement(&health_lines);
    LONG hidden=InterlockedExchange(&health_hidden,0),drawn=InterlockedExchange(&health_drawn,0);
    LONG one=InterlockedExchange(&health_fallback1,0),many=InterlockedExchange(&health_fallback23,0);
    if(lines>2000)return;
    unsigned a=one>0?static_cast<unsigned>(one):0u,b=many>0?static_cast<unsigned>(many):0u,total=a+b;
    if(total>65535u)total=65535u;if(a>255u)a=255u;if(b>255u)b=255u;
    log_event("ccz_sprites_health",static_cast<unsigned>(hidden>0?hidden:0),static_cast<unsigned>(drawn>0?drawn:0),total|(a<<16)|(b<<24));
}
#endif
// ---- game thread: read only ----
static bool read_camera(float out[80]) {
    Obj root=pointer(image,kCameraRoot);Obj p=root?pointer(root,0x78):nullptr;Obj cam=p?pointer(p,0x20):nullptr;
    // 2026-10-31 live read: cam+428 holds floats, not C5B1A0; the vtable plus the projection
    // sanity test below are the guard.
    // 2026-11-01 user: after the farthest zoom and back, sprites stayed gone. Log each change of the
    // rejection reason (first 32): 1 no camera, 2 other vtable (value = RVA), 3 projection test.
    unsigned reason=0,value=0;
    if(!cam)reason=1;
    else if(pointer(cam,0)!=image+kCameraVtable){reason=2;value=static_cast<unsigned>(pointer(cam,0)-image);}
    float again[80];
    if(!reason){memcpy(out,cam+0x1340,sizeof(again));memcpy(again,cam+0x1340,sizeof(again));
        if(memcmp(out,again,sizeof(again)))return false;   // torn copy while the camera moves
        if(!(out[44]>0 && out[49]>0 && std::fabs(std::fabs(out[55])-1)<1e-3f && out[59]==0)){reason=3;value=static_cast<unsigned>(out[55]*1000+10000);}}
#ifndef NO_RUNTIME_LOG
    static volatile LONG last_reason=-1,changes;
    if(InterlockedExchange(&last_reason,static_cast<LONG>(reason))!=static_cast<LONG>(reason) && InterlockedIncrement(&changes)<=32)
        log_event("ccz_sprites_camera",reason,value,reason==3?static_cast<unsigned>(out[59]*1000+10000):0);
#endif
    return !reason;
}
static ccz_catalog::Reader catalog;
struct Identity {unsigned team=0,cls=kNative,cell=0;};
static volatile LONG force_colours;   // force colour table matched; otherwise rows 1 player blue / 2 others red
// 2026-11-18 종군 무리 (user: "생성되는 스프라이트랑 실제 종군 스프라이트가 다름"): a live band unit draws its band kind, the same class as
// its idle picture, in its patron's colour. Land formations only (water and siege keep the native models like every army).
static bool band_identity(Obj army,unsigned formation,Identity &r) {
    unsigned kind=0,patron=0;
    if(formation>7 || !ccz_idle::band_of(army,kind,patron))return false;
    r.cls=ccz_band_kinds::class_of(kind);r.cell=ccz_appearance::cell(r.cls);
    if(!catalog.find(r.cell)){r.cls=ccz_band_kinds::class_of(0);r.cell=ccz_appearance::cell(r.cls);if(!catalog.find(r.cell))return false;}
    Obj root=world();Obj owner=root && patron && patron<0x34?pointer(root,0xDF30+patron*8):nullptr;
    Obj context=root?pointer(root,0x871E8):nullptr;unsigned player=context?context[0x3A]:0;
    if(InterlockedCompareExchange(&force_colours,0,0))r.team=owner && owner[0x49]<kColourCount?1u+owner[0x49]:0u;
    else r.team=player && patron==player?1u:2u;
    return true;
}
static Identity identify(Obj army,unsigned formation) {   // pure reads, no game calls
    Identity r;if(band_identity(army,formation,r))return r;
    if(formation>=21)return r;
    Obj root=world();unsigned leader=word(army,0x12);
    Obj person=root && leader<0x1771?pointer(root,0x158+leader*8):nullptr;
    ccz_appearance::Settings config;
    AcquireSRWLockShared(&appearance_lock);config=appearance;ReleaseSRWLockShared(&appearance_lock);
    unsigned tier=ccz_appearance::tier(config,person?person[0xEC]:0);   // same base 통솔; editable boundaries
    r.cls=config.mapping[formation][tier];r.cell=ccz_appearance::cell(r.cls);
    if(formation<=7){unsigned g=ccz_appearance::personal(config,leader,formation);if(g){r.cell=g;if(r.cls==kNative)r.cls=g/3u;}}
    if(!root || !person)return r;
    Obj context=pointer(root,0x871E8);unsigned player=context?context[0x3A]:0;
    unsigned corps_id=person[0xE0];Obj corps=pointer(root,0xE0D0+(corps_id<0x34?corps_id:0)*8);
    unsigned force=corps?corps[0x10]:0;   // 22CC40/22CA00 read the same fields
    Obj owner=force && force<0x34?pointer(root,0xDF30+force*8):nullptr;   // 22CA00 force table
    if(InterlockedCompareExchange(&force_colours,0,0))r.team=owner && owner[0x49]<kColourCount?1u+owner[0x49]:0u;   // 2AD010 reads Force+49
    else r.team=player && force==player?1u:2u;
    if(leader==987 || leader==147)r.team=kRedHare;   // 2026-11-01 user: 여포/관우 always ride the red 적토마 (horse shares the team ramp)
    if(owner && formation>=1 && formation<=7) {
        bool own=ccz_appearance::personal(config,leader,formation)!=0 && r.cls!=kNative;
        if(leader==word(owner,0x10))r.cls=config.mapping[21][tier];
        else if(leader==word(owner,0x52))r.cls=config.mapping[22][tier];
        if(!own)r.cell=ccz_appearance::cell(r.cls);
        else if(r.cls==kNative)r.cls=r.cell/3u;   // a checked personal picture still wins over native role art
    }
    if(r.cls>=0x1000 && r.cls!=kNative && !catalog.find(r.cell))r.cls=kNative;
    return r;
}
// 2026-11-17 종군 중립군 idle bands (docs/NEUTRAL_BANDITS.md 11, user "대기 상태에 그림이 없어 어디 생기는지 몰랐음"): an idle
// record has no original unit, so the renderer draws a neutral 산적 picture on its site tile. Hex -> world: the terrain module's
// hex centres (native_terrain_render.inl center(): 702 per column, sqrt(3)*468 per row, even columns half a row lower), then an
// affine fit learnt from standing armies (their tile +0x2A against their display position +0x90), identity until the fit is
// trusted. Ground height: the last army that stood on that tile or the nearest one within 6 hexes.
namespace ghost {
constexpr unsigned kMax=ccz_idle::kMax,kHexes=48400,kClass=48;   // 48 = 산적 (ccz_appearance_labels.h class_labels)
constexpr double kCol=702.0,kRow=810.5997779422346,kX0=37594.0,kZ0=13335.3,kRef=100000.0;
constexpr ULONGLONG kSampleMs=1000,kStillMs=1500;
static double su,sv,suu,suv,svv,sn,sxu,sxv,sx,szu,szv,sz,sy;   // fit sums (u,v = centre - kRef), guarded by ccz_sprites::lock
static double coef[6]={1,0,kRef,0,1,kRef};static bool fitted;   // x = c0*u+c1*v+c2, z = c3*u+c4*v+c5
static float tile_y[kHexes];static bool tile_has[kHexes];
#ifndef NO_RUNTIME_LOG
static double err_identity,err_fit;static unsigned err_n;static ULONGLONG next_log;
#endif
static void centre(unsigned hex,double &x,double &z) {
    const unsigned row=hex/220,col=hex%220;
    x=kX0+col*kCol;z=kZ0+row*kRow+((col%2==0)?kRow/2:0);
}
static bool refit() {
    if(sn<12)return false;
    const double mu=su/sn,mv=sv/sn,vu=suu/sn-mu*mu,vv=svv/sn-mv*mv;
    if(vu<1e6 || vv<1e6)return false;   // samples must spread over a few hexes both ways
    const double m[3][3]={{suu,suv,su},{suv,svv,sv},{su,sv,sn}};
    auto det3=[](const double a[3][3]) {return a[0][0]*(a[1][1]*a[2][2]-a[1][2]*a[2][1])-a[0][1]*(a[1][0]*a[2][2]-a[1][2]*a[2][0])+a[0][2]*(a[1][0]*a[2][1]-a[1][1]*a[2][0]);};
    const double d=det3(m);if(!(d>1e-6 || d<-1e-6))return false;
    const double rx[3]={sxu,sxv,sx},rz[3]={szu,szv,sz};
    double out[6];
    for(unsigned axis=0;axis<2;++axis)for(unsigned k=0;k<3;++k) {
        double t[3][3];memcpy(t,m,sizeof(t));
        for(unsigned r=0;r<3;++r)t[r][k]=axis?rz[r]:rx[r];
        out[axis*3+k]=det3(t)/d;
    }
    for(double v:out)if(!std::isfinite(v))return false;
    memcpy(coef,out,sizeof(coef));return true;
}
static void apply(double cx,double cz,double &x,double &z) {
    const double u=cx-kRef,v=cz-kRef;
    if(fitted){x=coef[0]*u+coef[1]*v+coef[2];z=coef[3]*u+coef[4]*v+coef[5];}else{x=cx;z=cz;}
}
// Game thread, ccz_sprites::lock held exclusive: one sample a second from an army that has stood still for 1.5 s.
static void sample(Unit &u,Obj army,const float position[3],ULONGLONG now,bool fresh_slot) {
    if(fresh_slot || now-u.moved<kStillMs || now-u.sampled<kSampleMs || u.state==2)return;
    const unsigned tile=word(army,0x2A);if(tile>=kHexes)return;
    u.sampled=now;
    double cx,cz;centre(tile,cx,cz);
    const double x=position[0],z=position[2];
    double px,pz;apply(cx,cz,px,pz);
    const double miss=std::sqrt((px-x)*(px-x)+(pz-z)*(pz-z));
    if(fitted && miss>1500)return;   // an army caught between hexes or a stray record
    tile_y[tile]=position[1];tile_has[tile]=true;
#ifndef NO_RUNTIME_LOG
    err_identity+=std::sqrt((cx-x)*(cx-x)+(cz-z)*(cz-z));err_fit+=miss;++err_n;
#endif
    if(sn>=4096){su*=0.5;sv*=0.5;suu*=0.5;suv*=0.5;svv*=0.5;sn*=0.5;sxu*=0.5;sxv*=0.5;sx*=0.5;szu*=0.5;szv*=0.5;sz*=0.5;sy*=0.5;}
    const double a=cx-kRef,b=cz-kRef;
    su+=a;sv+=b;suu+=a*a;suv+=a*b;svv+=b*b;sn+=1;sxu+=x*a;sxv+=x*b;sx+=x;szu+=z*a;szv+=z*b;sz+=z;sy+=position[1];
    fitted=refit()||fitted;
#ifndef NO_RUNTIME_LOG
    if(now>=next_log && err_n) {
        next_log=now+30000;
        auto pack=[](double c) {double v=c*1000+32768;return static_cast<unsigned>(v<0?0:v>65535?65535:v);};
        log_event("ccz_ghost_fit",static_cast<unsigned>(sn),static_cast<unsigned>(err_identity/err_n),static_cast<unsigned>(err_fit/err_n)|(fitted?0x80000000u:0u));
        log_event("ccz_ghost_axes",(pack(coef[0])<<16)|pack(coef[1]),(pack(coef[3])<<16)|pack(coef[4]),static_cast<unsigned>(coef[2]>0?coef[2]:0));
        err_identity=err_fit=0;err_n=0;
    }
#endif
}
static bool height_at(unsigned site,float &y) {   // caller holds ccz_sprites::lock (shared is enough)
    if(tile_has[site]){y=tile_y[site];return true;}
    const int x0=static_cast<int>(site%220),y0=static_cast<int>(site/220);
    for(int r=1;r<=6;++r)for(int dy=-r;dy<=r;++dy)for(int dx=-r;dx<=r;++dx) {
        if((dx>-r && dx<r) && (dy>-r && dy<r))continue;   // ring only
        const int x=x0+dx,yy=y0+dy;if(x<0 || yy<0 || x>=220 || yy>=220)continue;
        const unsigned t=static_cast<unsigned>(yy*220+x);if(tile_has[t]){y=tile_y[t];return true;}
    }
    if(sn>=1){y=static_cast<float>(sy/sn);return false;}
    y=800.0f;return false;
}
// Render thread: neutral idle pictures, appended after the live armies (owner kNoOwner, no army). 2026-11-19 user "부대 깃발만
// 픽셀처럼 ... 초상화가 차오르는식으로": every site also gets a round flag (the standing frame in a disc) in both picture modes, split at
// the fill line into the grown part (bottom) and the pale part still to grow (top); the standing picture only with 조조전 병종 그림 on.
static unsigned collect(Unit *out,unsigned room,ULONGLONG now) {
    if(!room)return 0;
    unsigned n=0;
    const bool pictures=!InterlockedCompareExchange(&idle_only,0,0);
    AcquireSRWLockShared(&lock);
    for(unsigned k=0;k<kMax && n+3<=room;++k) {
        const LONG64 v=InterlockedCompareExchange64(&ccz_idle::sites[k],0,0);
        if(v<=0 || v>static_cast<LONG64>(kHexes))continue;
        const unsigned site=static_cast<unsigned>(v-1);
        // 2026-11-18: the band kind decided with the idle record (same class the follower unit will draw), else 산적.
        unsigned cell=ccz_appearance::cell(ccz_band_kinds::class_of(static_cast<unsigned>(InterlockedCompareExchange(&ccz_idle::site_kinds[k],0,0))));
        if(!catalog.find(cell)){cell=ccz_appearance::cell(kClass);if(!catalog.find(cell))continue;}
        double cx,cz,x,z;centre(site,cx,cz);apply(cx,cz,x,z);
        float y;const bool known=height_at(site,y);(void)known;   // logged in diagnostic builds
        Unit g{};g.x=static_cast<float>(x);g.y=y;g.z=static_cast<float>(z);g.cell=cell;g.team=0;g.morale=100;g.seen=now;g.slot=kNoOwner;
        if(pictures)out[n++]=g;
        LONG fill=InterlockedCompareExchange(&ccz_idle::site_fill[k],0,0);fill=fill<0?0:fill>1000?1000:fill;
        const unsigned line=kBadgeSize-static_cast<unsigned>((fill*static_cast<LONG>(kBadgeSize)+500)/1000);   // first grown texel row
        g.cell=kBadgeTag|cell;
        if(line<kBadgeSize){Unit b=g;b.badge=1;b.lo=line;b.hi=kBadgeSize;out[n++]=b;}
        if(line>0){Unit b=g;b.badge=2;b.lo=0;b.hi=line;out[n++]=b;}
#ifndef NO_RUNTIME_LOG
        static ULONGLONG drawn_next;
        if(now>=drawn_next){drawn_next=now+30000;log_event("ccz_ghost_drawn",site,(fitted?1u:0u)|(known?2u:0u)|(pictures?4u:0u),static_cast<unsigned>(y>0?y:0)|(static_cast<unsigned>(fill)<<16));}
#endif
    }
    ReleaseSRWLockShared(&lock);
    return n;
}
// Game thread (band day pass) with 조조전 병종 그림 off: no army capture feeds the ground height, so every standing field army's display
// position (+0x148 display, +0x90; the same guarded reads as the far-zoom path) gives its tile's height. Old local23x logs: the hex
// centre already matches the display position (ccz_ghost_fit identity error 0), so only the height is taken.
static void note_height(unsigned tile,const float position[3]) {
    if(tile>=kHexes || !std::isfinite(position[1]))return;
    double cx,cz;centre(tile,cx,cz);
    const double dx=position[0]-cx,dz=position[2]-cz;if(dx*dx+dz*dz>300.0*300.0)return;   // between two hexes
    AcquireSRWLockExclusive(&lock);tile_y[tile]=position[1];tile_has[tile]=true;ReleaseSRWLockExclusive(&lock);
}
static bool wanted() {
    for(unsigned k=0;k<kMax;++k)if(InterlockedCompareExchange64(&ccz_idle::sites[k],0,0)>0)return true;
    return false;
}
}
static bool capture(Obj display,Obj scene) { COST_SCOPE("ccz_sprites_capture"); COST_CRUMB("ccz_capture_display",display);
#ifndef NO_RUNTIME_LOG
    health_tick();   // game thread only, same as every other capture counter
#endif
    if(dword(display,0x740)!=0 || dword(display,0x748)!=0)return false;   // field mode only, no extra part
    // 2026-10-31 live read: *(image+1B9FB70) held a stack-like value, never main+210==scene, so
    // every army was rejected. The Army back-reference below already identifies field armies.
    (void)scene;
    Obj army=pointer(display,0x40); COST_CRUMB("ccz_capture_army",army);
    if(!army || pointer(army,0)!=image+kArmyVtable || pointer(army,0x148)!=display)return false;
    unsigned formation=army[0x1B];
    Identity who=identify(army,formation);
    if(who.cls==kNative)return false;   // 충차 / water / 사이 / ships keep native models
    // PW6 rule 2 (2026-11-16: always): a picture we could not load keeps the native model. The unit stays tracked so the
    // renderer retries the picture after its back-off; the per-unit proof below hides it again once it is drawn.
    float position[3];memcpy(position,display+0x90,sizeof(position));
    if(!std::isfinite(position[0]) || !std::isfinite(position[1]) || !std::isfinite(position[2]))return false;
    float cam[80];bool fresh=read_camera(cam);
    unsigned team=who.team;ULONGLONG now=GetTickCount64();
    AcquireSRWLockExclusive(&lock);
    if(fresh){memcpy(camera,cam,sizeof(camera));camera_seen=now;}
    bool known_failed=failed_cells.find(who.cell)!=failed_cells.end();
    Unit *slot=nullptr,*stale=nullptr;
    for(auto &u:units) {
        if(u.army==army){slot=&u;break;}
        if(!stale || u.seen<stale->seen)stale=&u;
    }
    bool fresh_slot=!slot;
    if(!slot) {
        // PW6 rule 3 (2026-11-16: always): the slot table is full of live armies -> do not hide the newcomer.
        if(stale && stale->army && now<stale->seen+300) {
            ReleaseSRWLockExclusive(&lock);
#ifndef NO_RUNTIME_LOG
            InterlockedIncrement(&health_fallback23);
            static ULONGLONG full_next;static LONG full_count;
            LONG full=InterlockedIncrement(&full_count);
            if(now>=full_next) {
                full_next=now+10000;InterlockedExchange(&full_count,0);
                log_event("ccz_sprites_slots_full",static_cast<unsigned>(full>0?full:0),kSlots,0);
            }
#endif
            return false;
        }
        slot=stale;*slot=Unit{};slot->army=army;slot->first=now;slot->slot=static_cast<unsigned>(slot-units);
    }
    Unit &u=*slot;
    float dx=position[0]-u.x,dz=position[2]-u.z;
    if(!fresh_slot && dx*dx+dz*dz>1.0f) {
        float length=std::sqrt(dx*dx+dz*dz);u.fx=dx/length;u.fz=dz/length;u.facing=true;u.moved=now;
#ifndef NO_RUNTIME_LOG
        if(InterlockedCompareExchange(&yaw_logged,1,0)==0) {   // evidence for the yaw convention
            float yaw;memcpy(&yaw,display+0x744,4);
            log_event("ccz_sprites_yaw",static_cast<unsigned>(std::atan2(u.fx,u.fz)*1000+10000),static_cast<unsigned>(yaw*1000+10000),0);
        }
#endif
    }
    u.x=position[0];u.y=position[1];u.z=position[2];float yaw;memcpy(&yaw,display+0x744,4);
    // User 2026-11-02: native yaw also turns in place for attack/defence.
    // Movement is only a fallback when the current native angle is invalid.
    bool turned=native_facing(u,yaw);
#ifndef NO_RUNTIME_LOG
    static unsigned facing_notes;
    if(turned && !fresh_slot && dx*dx+dz*dz<=1.0f && (dword(display,0x60)==2 || now-u.hurt_at<kHurtMs) && facing_notes++<16)
        log_event("ccz_sprites_combat_facing",word(army,0x12),static_cast<unsigned>(yaw*1000+10000),dword(display,0x60));
#endif
    (void)turned;
    unsigned troops=word(army,0x16);if(!fresh_slot && troops<u.troops && now-u.hurt_at>=kHurtCooldownMs)u.hurt_at=now;u.troops=troops;   // any damage -> 피격
    buff_read(army,game_day_read(),u.guard,u.fury);
    if(u.cell!=who.cell){u.drawn_at=0;u.first=now;}   // a new picture (commander or appearance change) needs its own proof
    else if(!fresh_slot && now>u.seen+300)u.first=now;   // back after a capture gap (menu, far zoom): a new short trial, no model flash
    u.state=dword(display,0x60);u.team=team;u.morale=army[0x1A];u.cell=who.cell;u.seen=now;
    ghost::sample(u,army,position,now,fresh_slot);
    const ULONGLONG first=u.first,drawn_at=u.drawn_at;
    ReleaseSRWLockExclusive(&lock);
#ifndef NO_RUNTIME_LOG
    if(InterlockedIncrement(&captures)==1)log_event("ccz_sprites_first_capture",team,fresh?1:0,army[0x1B]);
#endif
    // Hide the native model only while the sprite can be drawn (camera read recently); otherwise the
    // army would vanish completely (2026-11-01: nothing shown after the farthest zoom).
    // PW6 (2026-11-09 user): also only once our sprite was drawn within 1 s, or during the first 1.5 s
    // of a hide episode (sprites not drawn yet). After that the native model shows until a draw succeeds.
    bool drawable=fresh || now-camera_seen<500;
    bool want=drawable && InterlockedCompareExchange(&hide_model,0,0)!=0;
    LONGLONG since=InterlockedCompareExchange64(&hide_since,0,0);
    if(want) {
        if(!since){InterlockedCompareExchange64(&hide_since,static_cast<LONGLONG>(now),0);since=InterlockedCompareExchange64(&hide_since,0,0);}
    } else if(since && now-static_cast<ULONGLONG>(since)>2000)InterlockedCompareExchange64(&hide_since,0,since);
    // 2026-11-16 (user: sprites vanish with the 3D model, never back until CCZ is turned off): with sprite_fallback off (the
    // default) the model used to hide whenever the camera read, whether or not anything was drawn, so any lasting draw failure
    // left the army invisible. The proof is now per unit and always on: hide only while this unit's picture went out in a
    // batch within 1 s, during its first 0.7 s (trial before the first draw) or the episode grace; never while its picture
    // failed to load. Strict mode (sprite_fallback) also needs a passing occlusion query (local61) instead of any batch.
    LONGLONG proof=fallback_enabled?InterlockedCompareExchange64(&last_draw_ok,0,0):InterlockedCompareExchange64(&last_submit,0,0);
    bool recent=proof && static_cast<ULONGLONG>(proof)+kProofMs>now &&   // the render tick may run a tick ahead of this one
        (fallback_enabled || !InterlockedCompareExchange(&covered,0,0));
    bool unit_drawn=drawn_at && drawn_at+kProofMs>now;
    bool grace=since && now-static_cast<ULONGLONG>(since)<1500;
    bool trial=first+kTrialMs>now;
    bool hide=want && !known_failed && ((unit_drawn && recent) || grace || trial);
#ifndef NO_RUNTIME_LOG
    if(hide)InterlockedIncrement(&health_hidden);
    else if(want)InterlockedIncrement(known_failed || !unit_drawn?&health_fallback23:&health_fallback1);
    if(hide && probe_mode) {   // ccz_draw_probe: the show phase draws the native model as well (measurement only)
        if(InterlockedCompareExchange(&probe_show,0,0))hide=false;else InterlockedIncrement(&probe_hidden);
    }
#endif
    return hide;
}
static void __fastcall submit(Obj display,Obj scene,Obj list) { COST_SCOPE("ccz_sprites_submit"); COST_CRUMB("ccz_sprites_display",display); COST_CRUMB("ccz_submit_display",display);
#if defined(RUNTIME_COST_PROFILE) && !defined(NO_RUNTIME_LOG)
    Obj submit_army=nullptr; SIZE_T submit_read=0;
    if(display && ReadProcessMemory(GetCurrentProcess(),display+0x40,&submit_army,sizeof(submit_army),&submit_read) && submit_read==sizeof(submit_army))
        COST_CRUMB("ccz_submit_army",submit_army);
#endif
    if(InterlockedCompareExchange(&installed,0,0) && capture(display,scene)) {
        // 2026-10-31 live crash: installing the Present hook from split_activate (before the
        // game swap chain) let the Steam overlay re-hook over it -> Present recursion.
        // Install lazily on the game's own scene thread like the other overlay users.
        overlay_install();
#ifndef NO_RUNTIME_LOG
        InterlockedIncrement(&replaced);
#endif
        return;
    }
    next_pick(next_submit,kSubmit)(display,scene,list);
}
// ---- asset ----
static void hsv_shift(unsigned &rgba,float hue) {
    float r=(rgba&255)/255.0f,g=((rgba>>8)&255)/255.0f,b=((rgba>>16)&255)/255.0f;
    float mx=std::fmax(r,std::fmax(g,b)),mn=std::fmin(r,std::fmin(g,b)),d=mx-mn,h=0;
    if(d>0)h=mx==r?std::fmod((g-b)/d+6,6.0f):mx==g?(b-r)/d+2:(r-g)/d+4;
    h=std::fmod(h/6+hue+1,1.0f)*6;float s=mx>0?d/mx:0,v=mx,f=h-std::floor(h),p=v*(1-s),q=v*(1-s*f),t=v*(1-s*(1-f));
    float o[3];switch(static_cast<int>(h)%6){case 0:o[0]=v;o[1]=t;o[2]=p;break;case 1:o[0]=q;o[1]=v;o[2]=p;break;
        case 2:o[0]=p;o[1]=v;o[2]=t;break;case 3:o[0]=p;o[1]=q;o[2]=v;break;case 4:o[0]=t;o[1]=p;o[2]=v;break;default:o[0]=v;o[1]=p;o[2]=q;}
    rgba=0xFF000000u|static_cast<unsigned>(o[0]*255+.5f)|static_cast<unsigned>(o[1]*255+.5f)<<8|static_cast<unsigned>(o[2]*255+.5f)<<16;
}
static void rgb_hsv(float r,float g,float b,float &h,float &s,float &v) {
    float mx=std::fmax(r,std::fmax(g,b)),mn=std::fmin(r,std::fmin(g,b)),d=mx-mn;h=0;
    if(d>0)h=(mx==r?std::fmod((g-b)/d+6,6.0f):mx==g?(b-r)/d+2:(r-g)/d+4)/6;
    s=mx>0?d/mx:0;v=mx;
}
static void hsv_rgb(float h,float s,float v,float &r,float &g,float &b) {
    h=std::fmod(h+1,1.0f)*6;float f=h-std::floor(h),p=v*(1-s),q=v*(1-s*f),t=v*(1-s*(1-f));
    switch(static_cast<int>(h)%6){case 0:r=v;g=t;b=p;break;case 1:r=q;g=v;b=p;break;case 2:r=p;g=v;b=t;break;
        case 3:r=p;g=q;b=v;break;case 4:r=t;g=p;b=v;break;default:r=v;g=p;b=q;}
}
static void build_palette(const unsigned char *colours_rgb,const unsigned char *mask,unsigned (&output)[kRows][256]) {
    // Force colours: read the game's static table once (pure read); guard on its fixed tail.
    unsigned colours[kColourCount];memcpy(colours,image+kForceColours,sizeof(colours));
    bool table_ok=colours[0]==0x2828E8FFu && colours[60]==0xFFFFFFFFu && colours[61]==0;
    InterlockedExchange(&force_colours,table_ok?1:0);
    for(unsigned row=0;row<kRows;++row) {
        unsigned rgb=row>=1 && row<=kColourCount?colours[row-1]>>8:0;
        float fr=((rgb>>16)&255)/255.0f,fg=((rgb>>8)&255)/255.0f,fb=(rgb&255)/255.0f;
        bool tint=(table_ok && row>=1 && row<=kColourCount) || row==kRedHare;
        if(row==kRedHare){fr=0.86f;fg=0.12f;fb=0.08f;}
        else if(tint) {   // 2026-11-01 user (option C): cool force colours -> warm contrast (blue -> red); warm ones stay (원소 gold)
            float h,s,v;rgb_hsv(fr,fg,fb,h,s,v);
            // 2026-11-01 user: 조조 blue came out orange -> blue/purple map to red, green/cyan to orange.
            if(h>0.2f && h<0.92f){h=h<=0.62f?0.12f*(0.62f-h)/0.42f:1-0.06f*(h-0.62f)/0.30f;s=s<0.8f?0.8f:s;v=v<0.8f?0.8f:v;hsv_rgb(h,s,v,fr,fg,fb);}
        }
        for(unsigned i=0;i<256;++i) {
            const unsigned char *c=colours_rgb+i*3;unsigned rgba=0xFF000000u|c[0]|c[1]<<8|c[2]<<16;
            // 2026-11-01 user: horses and faces were force-coloured too. The CCZ palette draws nearly everything in
            // greens; only the saturated pure-green ramp is the team colour (clothes). The other green/teal marks
            // become natural materials: same brightness, brown hue, less saturation (brown horses, skin, leather).
            float ch_,cs,cv;rgb_hsv(c[0]/255.0f,c[1]/255.0f,c[2]/255.0f,ch_,cs,cv);
            if(mask[i] && !(cs>0.85f && ch_>=0.30f && ch_<=0.43f)) {
                float r,g,b;hsv_rgb(0.07f,cs*0.55f>1?1:cs*0.55f,cv,r,g,b);
                rgba=0xFF000000u|static_cast<unsigned>(r*255+.5f)|static_cast<unsigned>(g*255+.5f)<<8|static_cast<unsigned>(b*255+.5f)<<16;
            }
            else if(mask[i]) {   // team colour (saturated green ramp)
                if(tint) {
                    unsigned top=c[0]>c[1]?c[0]:c[1];top=top>c[2]?top:c[2];float v=top/255.0f/0.8f;v=v<0.25f?0.25f:v>1.25f?1.25f:v;
                    auto ch=[&](float x){x*=v;return static_cast<unsigned>((x>1?1:x)*255+.5f);};
                    rgba=0xFF000000u|ch(fr)|ch(fg)<<8|ch(fb)<<16;
                }
                else if(!table_ok && row==1)hsv_shift(rgba,0.27f);   // fallback: player blue
                else if(!table_ok && row==2)hsv_shift(rgba,-0.33f);  // fallback: others red
            }
            output[row][i]=rgba;
        }
        // Outline contrasts with the row's colour (dark outline on light colours, light on dark).
        float lum=tint?0.299f*fr+0.587f*fg+0.114f*fb:0.5f;
        output[row][0]=lum>0.45f?0xFF141414u:0xFFF0F0F0u;
    }
}
static bool load_asset() {
    wchar_t path[MAX_PATH];if(!mod_ini_path(path,L"assets\\ccz\\ccz_units.bin"))return false;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    LARGE_INTEGER size{};bool ok=GetFileSizeEx(file,&size) && size.QuadPart>1040 && size.QuadPart<(64ll<<20);
    if(ok){asset.resize(static_cast<size_t>(size.QuadPart));DWORD got=0;ok=ReadFile(file,asset.data(),static_cast<DWORD>(asset.size()),&got,nullptr) && got==asset.size();}
    CloseHandle(file);
    if(!ok || memcmp(asset.data(),"CCZU",4) || dword(asset.data(),4)!=2)return false;
    unsigned count=dword(asset.data(),8),table=dword(asset.data(),12);
    if(static_cast<unsigned long long>(table)+16ull*count>asset.size())return false;
    for(unsigned k=0;k<count;++k) {
        Obj e=asset.data()+table+16*k;unsigned offset=dword(e,4),packed=dword(e,8);
        if(dword(e,12)==kRawSize && static_cast<unsigned long long>(offset)+packed<=asset.size())entries[word(e,0)]={offset,packed};
    }
    build_palette(asset.data()+16,asset.data()+784,palette);
    wchar_t catalog_path[MAX_PATH];
    if(mod_ini_path(catalog_path,L"assets\\ccz\\ccz_catalog.bin") && catalog.open(catalog_path))
        build_palette(catalog.palettes[1].data(),catalog.palettes[1].data()+768,original_palette);
    else memcpy(original_palette,palette,sizeof(palette));
    auto has=[](unsigned char c){return c==kNative || entries.count(c*3u)!=0;};
    for(auto &row:kFormationClass)for(unsigned char c:row)if(!has(c))return false;
    for(unsigned t=0;t<3;++t)if(!has(kRulerClass[t]) || !has(kStrategistClass[t]))return false;
    for(const auto &g:kGenerals)if(!entries.count(g.cell))return false;
    return true;
}
static bool decode(unsigned cell,std::vector<unsigned char> &atlas) {
    std::vector<unsigned char> raw;raw.reserve(kRawSize);
    auto it=entries.find(cell);
    if(it==entries.end()) {if(!catalog.decode(cell,raw))return false;}
    else {
        const unsigned char *p=asset.data()+it->second.first,*end=p+it->second.second;
        while(p<end && raw.size()<=kRawSize) {
            unsigned b=*p++;
            if(b<0x80){if(static_cast<size_t>(end-p)<b+1)return false;raw.insert(raw.end(),p,p+b+1);p+=b+1;}
            else raw.insert(raw.end(),b-0x7F,0);
        }
    }
    if(raw.size()!=kRawSize)return false;
    atlas.assign(kAtlasW*kAtlasH,0);const unsigned char *s=raw.data();
    auto copy=[&](unsigned frames,unsigned size,unsigned row) {
        for(unsigned f=0;f<frames;++f)for(unsigned y=0;y<size;++y,s+=size)memcpy(&atlas[(row+f*size+y)*kAtlasW],s,size);
    };
    copy(11,48,0);copy(12,64,kAttackRow);copy(5,48,kSpecialRow);return true;
}
// ---- render thread ----
static ID3D11Device *device;
static ID3D11VertexShader *vs;static ID3D11PixelShader *ps;static ID3D11InputLayout *layout;
static ID3D11Buffer *vertices;static ID3D11ShaderResourceView *palette_view,*original_palette_view,*label_view;
static ID3D11DepthStencilState *depth_on[2],*depth_off;static ID3D11RasterizerState *raster;
#ifndef NO_RUNTIME_LOG
static ID3D11DepthStencilState *depth_never;   // ccz_sprite_fault 2
#endif
struct CellTexture {unsigned cell=~0u;ID3D11ShaderResourceView *view=nullptr;ULONGLONG used=0;};
static CellTexture textures[kTextures];
static bool resources_failed;
static SRWLOCK dsv_lock=SRWLOCK_INIT;
static ID3D11DepthStencilView *scene_dsv;static ID3D11DepthStencilView *rejected[8];static unsigned rejected_next;
// 2026-11-12 (user: sprites flicker during 진행, then units vanish): rejected[] used to hold raw addresses, so a depth view
// the game recreated at a reused address stayed "not the scene depth" until a new device. Entries now hold a reference
// (the address cannot be reused while we keep it; depth textures only, never a swap-chain buffer) and are cleared after
// a Present stall or a back-buffer size change.
// Visibility: a Draw call alone no longer counts as "drawn". Each sprite batch runs inside an occlusion query; only a
// result with passing pixels refreshes last_draw_ok, so a sprite covered or failing depth lets the native model return.
// Query creation failure keeps the previous rule (a submitted Draw counts).
constexpr unsigned kVisibilityQueries=8;
static ID3D11Query *visibility[kVisibilityQueries];static ULONGLONG visibility_at[kVisibilityQueries];
static bool visibility_busy[kVisibilityQueries];static unsigned visibility_next;
#ifndef NO_RUNTIME_LOG
static LONG visibility_seen,visibility_hidden,visibility_streak,visibility_logs;
// 2026-11-16: the bind dump used to spend its "good" samples at startup and filed drawn-but-covered frames as good. A forced
// dump (kind 2) follows a draw gap, an invisible streak or a drawn -> not drawn path change (at most 8 per session).
static bool force_dump;
#endif
static volatile LONG want_width,want_height,drawing;
using SetTargets=void(STDMETHODCALLTYPE *)(ID3D11DeviceContext *,UINT,ID3D11RenderTargetView *const *,ID3D11DepthStencilView *);
constexpr unsigned kSetTargetsSlot=33;   // ID3D11DeviceContext::OMSetRenderTargets
static SetTargets original_set;static void **set_table;
template<class T> static void drop(T *&p){if(p){p->Release();p=nullptr;}}
static void release_resources() {
    drop(vs);drop(ps);drop(layout);drop(vertices);drop(palette_view);drop(original_palette_view);drop(label_view);drop(depth_on[0]);drop(depth_on[1]);drop(depth_off);drop(raster);
#ifndef NO_RUNTIME_LOG
    drop(depth_never);
#endif
    for(auto &t:textures){drop(t.view);t=CellTexture{};}
    for(unsigned i=0;i<kVisibilityQueries;++i){drop(visibility[i]);visibility_busy[i]=false;}
    AcquireSRWLockExclusive(&dsv_lock);drop(scene_dsv);for(auto &r:rejected)drop(r);rejected_next=0;ReleaseSRWLockExclusive(&dsv_lock);
    if(device){device->Release();device=nullptr;}
}
struct Stats {   // read-only diagnostics, inspected from outside the process (frame-drop question)
    LONGLONG qpc_per_second,last_present,frames,frame_qpc,frame_max,cost_qpc,cost_max;
    LONG set_calls,slow_notes,scene_draws,present_draws,pass_ends,units_drawn,target_format,seq_count,seq_armed;
    struct Bind {unsigned width,height,format,count,dsv,depth_width,depth_height;} seq[4096];   // local13: whole frames
};
static Stats stats;
// The scene depth buffer stays intact until the next frame's clear; remember the last
// back-buffer-sized depth view the game bound with colour targets.
// 2026-11-16: every recorded bind order is from 1280x720, where the UI layer's depth (1920x1080) differs from the back buffer.
// With a 1920x1080 back buffer that UI depth would be taken for the scene depth at the UI bind itself, so the "first other
// depth after post-processing" draw point could never fire. A back-buffer-sized depth first seen after post-processing
// (late) is therefore never promoted; this frame's late views are remembered by address only to skip repeat queries.
static bool late_depth;static ID3D11DepthStencilView *late_views[4];static unsigned late_next;
// 2026-11-16 user (local196/197: CCZ never visible): the scene alternates two back-buffer-sized depth views, and with the late
// rule above the one bound with the back buffer DURING post-processing (bind 50 of the local197 dump) no longer counted as scene
// depth, so the UI-layer test (dsv != scene_dsv) fired there; the remaining post passes rewrote the back buffer over the sprites
// while the per-unit proof hid the models -> armies invisible. Every depth view bound before post-processing this frame is a
// scene depth for that test (addresses only, compared, never dereferenced).
static ID3D11DepthStencilView *pre_views[16];static unsigned pre_count;
static bool pre_post_depth(ID3D11DepthStencilView *dsv) {for(unsigned i=0;i<pre_count;++i)if(pre_views[i]==dsv)return true;return false;}
static void note_dsv(ID3D11DepthStencilView *dsv,bool late) {
    if(dsv==scene_dsv)return;
    if(late)for(auto v:late_views)if(v==dsv)return;
    AcquireSRWLockExclusive(&dsv_lock);
    bool known=false;for(auto r:rejected)if(r==dsv)known=true;
    if(!known) {
        ID3D11Resource *resource=nullptr;dsv->GetResource(&resource);ID3D11Texture2D *texture=nullptr;
        if(resource){resource->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void **>(&texture));resource->Release();}
        D3D11_TEXTURE2D_DESC d{};if(texture){texture->GetDesc(&d);texture->Release();}
        if(texture && d.Width==static_cast<UINT>(want_width) && d.Height==static_cast<UINT>(want_height) && d.SampleDesc.Count==1 && late) {
            late_depth=true;late_views[late_next++%4]=dsv;
#ifndef NO_RUNTIME_LOG
            static volatile LONG late_logs;if(InterlockedIncrement(&late_logs)<=4)log_event("ccz_sprites_late_depth",d.Width,d.Height,d.Format);
#endif
        } else if(texture && d.Width==static_cast<UINT>(want_width) && d.Height==static_cast<UINT>(want_height) && d.SampleDesc.Count==1) {
            dsv->AddRef();drop(scene_dsv);scene_dsv=dsv;
#ifndef NO_RUNTIME_LOG
            // local7 log: two back-buffer-sized depth views alternate ~15 times a frame -> log only the first few.
            static volatile LONG depth_logs;if(InterlockedIncrement(&depth_logs)<=4)log_event("ccz_sprites_scene_depth",d.Width,d.Height,d.Format);
#endif
        } else {ID3D11DepthStencilView *&slot=rejected[rejected_next++%8];drop(slot);dsv->AddRef();slot=dsv;++stats.slow_notes;}
    }
    ReleaseSRWLockExclusive(&dsv_lock);
}
static void reset_views(bool resized) {   // render thread: Present stall or back-buffer size change
    AcquireSRWLockExclusive(&dsv_lock);
    for(auto &r:rejected)drop(r);rejected_next=0;
    if(resized)drop(scene_dsv);
    ReleaseSRWLockExclusive(&dsv_lock);
}
// 2026-10-31 live: drawing at Present covered the HUD portraits and stayed above full-screen windows.
// local7 read (stats.seq): the game draws the 3D scene straight into the sRGB back buffer with the scene
// depth bound until Present, then post-processing on half-size targets (640x360 at 1280x720) that rewrites
// the back buffer (local8 drew 28 units there and nothing survived), then the UI on its own layer
// (1920x1080 targets with their own depth), composited last. Draw into the back buffer just before the
// first colour bind with a non-scene depth view; Present remains the fallback when none is seen.
static ID3D11RenderTargetView *main_rtv;static bool drawn_this_frame,post_seen;
static bool drew_ui;   // this frame drew at the UI-layer bind (path signature, 2026-11-16)
static bool drew_after_post;   // this frame drew at the scene-depth back-buffer bind after post-processing (local199)
#ifndef NO_RUNTIME_LOG
static bool rebound_after_draw;   // this frame bound the back buffer again after our draw (signature, not an error by itself)
#endif
// local14: the zoomed-out path skips the 1920x1080 UI depth pass. Try the first
// late 3/8-resolution pass instead (480x270 at 1280x720), after post-processing.
// Normal UI-depth injection stays unchanged; the rescue runs at most once/frame.
static bool fallback_attempted;
static unsigned fallback_draws,fallback_misses;
// 2026-11-01 user (local7): after the farthest zoom the sprites never came back while armies were still
// captured. scene_dsv is just the last back-buffer-sized depth bound, and two of them alternate within a
// frame; after the strategy view the last one before the UI was the other buffer, so every sprite failed
// the depth test. Depth-test against the view last bound together with the scene colour target instead.
static ID3D11DepthStencilView *frame_dsv;
struct SizeCache {ID3D11RenderTargetView *rtv;unsigned width,height;};
static SizeCache sizes[16];static unsigned sizes_next;
static void rtv_size(ID3D11RenderTargetView *rtv,unsigned &width,unsigned &height) {   // render thread only
    for(auto &e:sizes)if(e.rtv==rtv){width=e.width;height=e.height;return;}
    width=height=0;ID3D11Resource *r=nullptr;rtv->GetResource(&r);ID3D11Texture2D *t=nullptr;
    if(r){r->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void **>(&t));r->Release();}
    if(t){D3D11_TEXTURE2D_DESC d{};t->GetDesc(&d);width=d.Width;height=d.Height;t->Release();}
    sizes[sizes_next++%16]={rtv,width,height};
}
static ID3D11Texture2D *back_texture;   // this frame's back buffer, compared only (no reference held)
static bool draw_scene(ID3D11DeviceContext *c,ID3D11RenderTargetView *rtv,ID3D11DepthStencilView *dsv);
static void sample_bind(UINT n,ID3D11RenderTargetView *const *rtv,ID3D11DepthStencilView *dsv) {
    if(!stats.seq_armed || stats.seq_count>=4096)return;
    // dsv flags: 1 depth bound, 2 = scene_dsv, 4 = frame_dsv, 8 colour = main_rtv, 16 post seen, 32 drawn already
    unsigned flags=(dsv?1u:0u)|(dsv && dsv==scene_dsv?2u:0u)|(dsv && dsv==frame_dsv?4u:0u)|(main_rtv && n && rtv && rtv[0]==main_rtv?8u:0u)|(post_seen?16u:0u)|(drawn_this_frame?32u:0u);
    Stats::Bind &b=stats.seq[stats.seq_count++];b={0,0,0,n,flags,0,0};
    if(dsv) {
        ID3D11Resource *r=nullptr;dsv->GetResource(&r);ID3D11Texture2D *t=nullptr;
        if(r){r->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void **>(&t));r->Release();}
        if(t){D3D11_TEXTURE2D_DESC d{};t->GetDesc(&d);b.depth_width=d.Width;b.depth_height=d.Height;t->Release();}
    }
    if(n && rtv && rtv[0]) {
        ID3D11Resource *r=nullptr;rtv[0]->GetResource(&r);ID3D11Texture2D *t=nullptr;
        if(r){r->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void **>(&t));r->Release();}
        if(t){D3D11_TEXTURE2D_DESC d{};t->GetDesc(&d);b.width=d.Width;b.height=d.Height;b.format=d.Format|(t==back_texture?0x10000u:0u);t->Release();}
    }
}
static void STDMETHODCALLTYPE set_targets(ID3D11DeviceContext *c,UINT n,ID3D11RenderTargetView *const *rtv,ID3D11DepthStencilView *dsv) { COST_SCOPE("ccz_set_targets");
    if(InterlockedCompareExchange(&drawing,0,0)){original_set(c,n,rtv,dsv);return;}
    LARGE_INTEGER t0;QueryPerformanceCounter(&t0);++stats.set_calls;
    if(dsv && n && n<=D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT && want_width)note_dsv(dsv,main_rtv && post_seen);
    sample_bind(n,rtv,dsv);
    if(dsv && !post_seen && pre_count<16 && !pre_post_depth(dsv))pre_views[pre_count++]=dsv;
#ifndef NO_RUNTIME_LOG
    if(drawn_this_frame && main_rtv && n && rtv && rtv[0]==main_rtv)rebound_after_draw=true;   // path signature only (2026-11-16)
#endif
    if(!drawn_this_frame && n && rtv && rtv[0] && want_width) {
        unsigned w=0,h=0;rtv_size(rtv[0],w,h);
        if(!main_rtv && dsv && dsv==scene_dsv && w==static_cast<unsigned>(want_width) && h==static_cast<unsigned>(want_height)){main_rtv=rtv[0];main_rtv->AddRef();}
        else if(main_rtv && w*2==static_cast<unsigned>(want_width) && h*2==static_cast<unsigned>(want_height))post_seen=true;
        // 2026-11-16 user (local198: gone after the far view, shown only briefly in event close-ups, and then above the event
        // picture): every recorded frame (13 dumps, near and far) binds the back buffer with the scene depth again right after
        // post-processing (tone mapping done), before the unit gauges, the 1920x1080 pass (present only in some frames, e.g.
        // events) and the 3/8 passes. Draw there; the later rules stay as fallbacks.
        else if(main_rtv && post_seen && rtv[0]==main_rtv && dsv && dsv==scene_dsv) {
            ++stats.pass_ends;
            if(draw_scene(c,main_rtv,frame_dsv?frame_dsv:scene_dsv))drawn_this_frame=true;
            drew_after_post=drawn_this_frame;
        }
        // 2026-10-31 live: battle effects bind their own depth in the middle of the scene; drawing there was
        // covered by the rest of the scene. Only the first other-depth bind after post-processing is the UI layer.
        else if(main_rtv && post_seen && dsv && dsv!=scene_dsv && !pre_post_depth(dsv)) {
            ++stats.pass_ends;
            if(draw_scene(c,main_rtv,frame_dsv?frame_dsv:scene_dsv))drawn_this_frame=true;
            drew_ui=drawn_this_frame;
        }
        // The failed zoom path does not bind a separate-depth UI target at all.
        // Its last 3/8-resolution pass comes AFTER half-resolution post-processing.
        // Inject immediately before it; never use Present (which overlays HUDs).
        else if(main_rtv && post_seen && !fallback_attempted &&
                static_cast<unsigned long long>(w)*8u==static_cast<unsigned long long>(want_width)*3u &&
                static_cast<unsigned long long>(h)*8u==static_cast<unsigned long long>(want_height)*3u) {
            fallback_attempted=true;
            bool success=draw_scene(c,main_rtv,frame_dsv?frame_dsv:scene_dsv);
            if(success){drawn_this_frame=true;++fallback_draws;}
            else ++fallback_misses;
#ifndef NO_RUNTIME_LOG
            static unsigned fallback_logs;
            if(++fallback_logs<=12 || fallback_logs%300==0)
                log_event("ccz_sprites_zoom_fallback",success?1u:0u,
                          static_cast<unsigned>(stats.units_drawn),
                          (static_cast<unsigned>(w)<<16)|static_cast<unsigned>(h));
#endif
        }
        if(main_rtv && rtv[0]==main_rtv && dsv && dsv!=frame_dsv && !post_seen){dsv->AddRef();drop(frame_dsv);frame_dsv=dsv;}
    }
#ifndef NO_RUNTIME_LOG
    if(probe_mode)probe_pass=!n || !rtv || !rtv[0]?0u:post_seen?3u:rtv[0]==main_rtv?1u:2u;
#endif
    LARGE_INTEGER t1;QueryPerformanceCounter(&t1);stats.cost_qpc+=t1.QuadPart-t0.QuadPart;
    original_set(c,n,rtv,dsv);
}
static void hook_context(ID3D11DeviceContext *c) {
    if(set_table || !InterlockedCompareExchange(&depth_test,0,0))return;
    void **table=*reinterpret_cast<void ***>(c);DWORD prior=0;
    if(!VirtualProtect(table+kSetTargetsSlot,sizeof(void *),PAGE_READWRITE,&prior))return;
    // Review F-05: swap only if the slot still holds the value we chain to (another mod may hook it between the read and the write);
    // a slot that already holds our hook is never chained to itself. Bounded retry, then give up (the next call tries again).
    bool published=false,self=false;
    for(int attempt=0;attempt<3 && !published;++attempt) {
        void *current=table[kSetTargetsSlot];   // chains over gpu_profile if it came first
        if(current==reinterpret_cast<void *>(&set_targets)) {self=true;break;}
        original_set=reinterpret_cast<SetTargets>(current);
        published=InterlockedCompareExchangePointer(table+kSetTargetsSlot,reinterpret_cast<void *>(&set_targets),current)==current;
    }
    DWORD ignored=0;VirtualProtect(table+kSetTargetsSlot,sizeof(void *),prior,&ignored);
    if(self){set_table=table;return;}   // 2026-11-16 re-hook after a silent stretch: already first in this table, keep it
    if(!published) {
#ifndef NO_RUNTIME_LOG
        static unsigned failures;if(++failures<=4)log_event("ccz_sprites_context_hook_failed",self?1u:0u,0,0);
#endif
        return;
    }
    set_table=table;
#ifndef NO_RUNTIME_LOG
    log_event("ccz_sprites_context_hook",1,0,0);
#endif
}
#ifndef NO_RUNTIME_LOG
// 2026-11-16 measurement build (user: hide and draw at the same stage like the 2D mod, measure first; frame cost must not grow
// blindly). [diagnostics] ccz_draw_probe=1, diagnostic builds only, no effect on release builds:
// - the immediate context's draw calls (DrawIndexed 12, Draw 13, DrawIndexedInstanced 20, DrawInstanced 21,
//   DrawIndexedInstancedIndirect 39) are chained like the bind hook (CAS over whatever is there, never over ourselves);
// - every frame only counts draws; every 10th frame reads each draw's state (shaders, index / vertex buffer, counts) into two
//   signature tables (with buffers / shape only);
// - the model hide alternates every 4 s (show phase: the native models are drawn too), so the signatures drawn only while the
//   models show are the unit-model draws. Every 16 s: ccz_probe_cost / ccz_probe_phase / ccz_probe_sig / ccz_probe_shape lines.
namespace probe {
using DrawIndexedFn=void(STDMETHODCALLTYPE *)(ID3D11DeviceContext *,UINT,UINT,INT);
using DrawFn=void(STDMETHODCALLTYPE *)(ID3D11DeviceContext *,UINT,UINT);
using DrawIIFn=void(STDMETHODCALLTYPE *)(ID3D11DeviceContext *,UINT,UINT,UINT,INT,UINT);
using DrawIFn=void(STDMETHODCALLTYPE *)(ID3D11DeviceContext *,UINT,UINT,UINT,UINT);
using DrawIndirectFn=void(STDMETHODCALLTYPE *)(ID3D11DeviceContext *,ID3D11Buffer *,UINT);
constexpr unsigned kSlotCount=5,kSlots[kSlotCount]={12,13,20,21,39};
static void *orig[kSlotCount];static void **table;static unsigned published;static ID3D11DeviceContext *ctx;
// local196 log: 0 draws counted while the scene was drawn -> the draws do not come through the Present context (another
// thread / deferred context). Every context of the hooked class is counted now (Present context vs others) and the sampled
// notes take a lock; up to 8 drawing contexts are listed with their type.
static volatile bool sampling;static volatile unsigned sample_phase;static unsigned max_draws,sample_frames,reports,halves;
static volatile LONG imm_draws,other_draws;static unsigned long long all_imm,all_other;static SRWLOCK note_lock=SRWLOCK_INIT;
struct Context {ID3D11DeviceContext *c;unsigned type,thread,draws;};static Context contexts[8];static unsigned context_count;
static unsigned long long all_frames;static LONGLONG sample_qpc;static ULONGLONG phase_at;
static unsigned phase_samples[2],phase_frames[2];
struct Sig {unsigned long long key;unsigned n,meta,shader,count[2],insts[2];};
constexpr unsigned kSigs=1024;
static Sig full[kSigs],shape[kSigs];static unsigned used[2],overflow;
static void add(Sig *t,unsigned which,unsigned long long key,unsigned n,unsigned meta,unsigned shader_id,UINT inst) {
    if(!key)key=1;
    unsigned i=static_cast<unsigned>(key^(key>>32))&(kSigs-1);
    for(unsigned step=0;step<16;++step,i=(i+1)&(kSigs-1)) {
        Sig &s=t[i];
        if(!s.key) {
            if(used[which]>=kSigs*3/4)break;
            s.key=key;s.n=n;s.meta=meta;s.shader=shader_id;++used[which];
        }
        if(s.key==key){++s.count[sample_phase];s.insts[sample_phase]+=inst;return;}
    }
    ++overflow;
}
static void note(ID3D11DeviceContext *c,unsigned kind,UINT n,UINT inst,UINT start,INT base) {   // any drawing thread, sampled frames
    LARGE_INTEGER t0;QueryPerformanceCounter(&t0);
    AcquireSRWLockExclusive(&note_lock);
    {
        unsigned k=0;while(k<context_count && contexts[k].c!=c)++k;
        if(k==context_count && k<8){contexts[k]={c,static_cast<unsigned>(c->GetType()),GetCurrentThreadId(),0};++context_count;}
        if(k<context_count)++contexts[k].draws;
    }
    ID3D11Buffer *ib=nullptr,*vb=nullptr;DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;UINT ib_offset=0,stride=0,vb_offset=0;
    ID3D11VertexShader *vsh=nullptr;ID3D11PixelShader *psh=nullptr;
    if(kind==0 || kind==2 || kind==4)c->IAGetIndexBuffer(&ib,&format,&ib_offset);
    c->IAGetVertexBuffers(0,1,&vb,&stride,&vb_offset);c->VSGetShader(&vsh,nullptr,nullptr);c->PSGetShader(&psh,nullptr,nullptr);
    unsigned long long h=0xCBF29CE484222325ull;
    auto mix=[&h](unsigned long long v){h=(h^v)*0x100000001B3ull;};
    mix(reinterpret_cast<UINT_PTR>(vsh));mix(reinterpret_cast<UINT_PTR>(psh));mix(n);mix(kind|probe_pass<<4);mix(stride);
    const unsigned long long shape_key=h;
    mix(reinterpret_cast<UINT_PTR>(ib));mix(reinterpret_cast<UINT_PTR>(vb));mix(start);mix(static_cast<unsigned>(base));
    const unsigned meta=probe_pass<<28|kind<<24|(stride&0xFFu)<<16|(start&0xFFFFu);
    const unsigned vs_low=static_cast<unsigned>(reinterpret_cast<UINT_PTR>(vsh));
    add(full,0,h,n,meta,vs_low,inst);add(shape,1,shape_key,n,meta&0xFFFF0000u,vs_low,inst);
    if(ib)ib->Release();if(vb)vb->Release();if(vsh)vsh->Release();if(psh)psh->Release();
    LARGE_INTEGER t1;QueryPerformanceCounter(&t1);sample_qpc+=t1.QuadPart-t0.QuadPart;
    ReleaseSRWLockExclusive(&note_lock);
}
static inline void seen(ID3D11DeviceContext *c,unsigned kind,UINT n,UINT inst,UINT start,INT base) {
    if(!ctx || InterlockedCompareExchange(&drawing,0,0))return;   // not hooked yet / our own sprite draws
    InterlockedIncrement(c==ctx?&imm_draws:&other_draws);
    if(sampling)note(c,kind,n,inst,start,base);
}
static void STDMETHODCALLTYPE draw_indexed(ID3D11DeviceContext *c,UINT n,UINT start,INT base) {
    seen(c,0,n,1,start,base);reinterpret_cast<DrawIndexedFn>(orig[0])(c,n,start,base);
}
static void STDMETHODCALLTYPE draw_plain(ID3D11DeviceContext *c,UINT n,UINT start) {
    seen(c,1,n,1,start,0);reinterpret_cast<DrawFn>(orig[1])(c,n,start);
}
static void STDMETHODCALLTYPE draw_indexed_instanced(ID3D11DeviceContext *c,UINT n,UINT inst,UINT start,INT base,UINT si) {
    seen(c,2,n,inst,start,base);reinterpret_cast<DrawIIFn>(orig[2])(c,n,inst,start,base,si);
}
static void STDMETHODCALLTYPE draw_instanced(ID3D11DeviceContext *c,UINT n,UINT inst,UINT start,UINT si) {
    seen(c,3,n,inst,start,0);reinterpret_cast<DrawIFn>(orig[3])(c,n,inst,start,si);
}
static void STDMETHODCALLTYPE draw_indexed_indirect(ID3D11DeviceContext *c,ID3D11Buffer *args,UINT offset) {
    seen(c,4,0,1,offset,0);reinterpret_cast<DrawIndirectFn>(orig[4])(c,args,offset);
}
static void *const hooks[kSlotCount]={reinterpret_cast<void *>(&draw_indexed),reinterpret_cast<void *>(&draw_plain),
    reinterpret_cast<void *>(&draw_indexed_instanced),reinterpret_cast<void *>(&draw_instanced),reinterpret_cast<void *>(&draw_indexed_indirect)};
static void hook(ID3D11DeviceContext *c) {   // render thread, once, after the bind hook is in (the pass id comes from it)
    if(table || !set_table || c->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return;
    void **t=*reinterpret_cast<void ***>(c);
    for(unsigned i=0;i<kSlotCount;++i) {
        DWORD prior=0;if(!VirtualProtect(t+kSlots[i],sizeof(void *),PAGE_READWRITE,&prior))continue;
        void *current=t[kSlots[i]];
        if(current!=hooks[i]) {
            orig[i]=current;
            if(InterlockedCompareExchangePointer(t+kSlots[i],hooks[i],current)==current)published|=1u<<i;
        }
        DWORD ignored=0;VirtualProtect(t+kSlots[i],sizeof(void *),prior,&ignored);
    }
    ctx=c;table=t;
    log_event("ccz_probe_hook",published,kSlotCount,0);
}
static void unhook() {
    if(!table)return;
    ctx=nullptr;
    for(unsigned i=0;i<kSlotCount;++i) if(published&(1u<<i)) {   // a later hook over ours keeps it in the chain (it only forwards)
        DWORD prior=0;if(!VirtualProtect(table+kSlots[i],sizeof(void *),PAGE_READWRITE,&prior))continue;
        if(InterlockedCompareExchangePointer(table+kSlots[i],orig[i],hooks[i])==hooks[i])published&=~(1u<<i);
        DWORD ignored=0;VirtualProtect(table+kSlots[i],sizeof(void *),prior,&ignored);
    }
}
static unsigned per_sample10(unsigned count,unsigned samples) {
    const unsigned long long v=samples?static_cast<unsigned long long>(count)*10/samples:0;return v>65535?65535u:static_cast<unsigned>(v);
}
static void report_table(const char *kind,Sig *t,unsigned s0,unsigned s1) {   // up to 8 signatures drawn more while models show
    bool taken[kSigs]{};
    for(unsigned k=0;k<8;++k) {
        int best=-1;unsigned best_diff=4;   // at least 0.5 draws per sampled frame more
        for(unsigned i=0;i<kSigs;++i) {
            if(!t[i].key || taken[i])continue;
            const unsigned show=per_sample10(t[i].count[1],s1),hide=per_sample10(t[i].count[0],s0);
            if(show>hide && show-hide>best_diff){best_diff=show-hide;best=static_cast<int>(i);}
        }
        if(best<0)break;
        taken[best]=true;const Sig &s=t[best];
        log_event(kind,s.n,per_sample10(s.count[1],s1)<<16|per_sample10(s.count[0],s0),s.meta);
        log_event("ccz_probe_key",static_cast<unsigned>(s.key),per_sample10(s.insts[1],s1),s.shader);
    }
}
static void report() {
    LARGE_INTEGER f;QueryPerformanceFrequency(&f);
    const unsigned s0=phase_samples[0],s1=phase_samples[1];
    const unsigned hidden=static_cast<unsigned>(InterlockedExchange(&probe_hidden,0));
    AcquireSRWLockExclusive(&note_lock);
    if(reports++<40) {
        log_event("ccz_probe_cost",all_frames?static_cast<unsigned>((all_imm+all_other)/all_frames):0u,max_draws,
            sample_frames?static_cast<unsigned>(sample_qpc*1000000/f.QuadPart/sample_frames):0u);   // draws/frame, max, us per sampled frame
        log_event("ccz_probe_ctx",all_frames?static_cast<unsigned>(all_imm/all_frames):0u,all_frames?static_cast<unsigned>(all_other/all_frames):0u,context_count);
        for(unsigned k=0;k<context_count;++k)   // context type (0 immediate, 1 deferred) | Present context, thread, draws per sampled frame x10
            log_event("ccz_probe_context",contexts[k].type|(contexts[k].c==ctx?0x100u:0u),contexts[k].thread,per_sample10(contexts[k].draws,s0+s1));
        log_event("ccz_probe_phase",s1<<16|s0,phase_frames[0]?hidden*10/phase_frames[0]:0u,used[0]|used[1]<<12|(overflow>4095?4095u:overflow)<<20);
        if(s0 && s1){report_table("ccz_probe_sig",full,s0,s1);report_table("ccz_probe_shape",shape,s0,s1);}
    }
    memset(full,0,sizeof(full));memset(shape,0,sizeof(shape));used[0]=used[1]=overflow=0;
    for(unsigned k=0;k<context_count;++k)contexts[k].draws=0;   // contexts stay listed (pointers compared only)
    phase_samples[0]=phase_samples[1]=phase_frames[0]=phase_frames[1]=0;
    all_imm=all_other=all_frames=0;max_draws=sample_frames=0;sample_qpc=0;
    ReleaseSRWLockExclusive(&note_lock);
}
static void frame() {   // render thread, at Present
    if(!table)return;
    if(sampling){++phase_samples[sample_phase];++sample_frames;}
    const unsigned ph=InterlockedCompareExchange(&probe_show,0,0)?1u:0u;
    const unsigned imm=static_cast<unsigned>(InterlockedExchange(&imm_draws,0)),other=static_cast<unsigned>(InterlockedExchange(&other_draws,0));
    ++phase_frames[ph];++all_frames;all_imm+=imm;all_other+=other;if(imm+other>max_draws)max_draws=imm+other;
    const ULONGLONG now=GetTickCount64();if(!phase_at)phase_at=now;
    if(now-phase_at>=4000) {
        InterlockedExchange(&probe_show,ph?0:1);phase_at=now;
        if(++halves%4==0)report();   // two show / hide cycles
    }
    sample_phase=InterlockedCompareExchange(&probe_show,0,0)?1u:0u;
    sampling=all_frames%10==0 && now-phase_at>=500;   // models settle for 0.5 s after a phase change
}
}
#endif
static bool create_labels(ID3D11Device *d) {   // render thread, once per device
    BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=kLabelW;bi.bmiHeader.biHeight=-static_cast<LONG>(kLabelH*2);
    bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    HDC dc=CreateCompatibleDC(nullptr);if(!dc)return false;
    void *bits=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
    HFONT font=CreateFontW(-17,0,0,0,FW_HEAVY,0,0,0,HANGUL_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,DEFAULT_PITCH,L"Malgun Gothic");
    bool ok=bitmap && bits && font;std::vector<unsigned char> mask(kLabelW*kLabelH*2,0);
    if(ok) {
        HGDIOBJ old_bitmap=SelectObject(dc,bitmap),old_font=SelectObject(dc,font);
        memset(bits,0,kLabelW*kLabelH*2*4);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
        const wchar_t *text[2]={L"\uB9C9\uAE30!",L"\uD68C\uC2EC!"};   // 막기! / 회심!
        for(int r=0;r<2;++r){RECT rc{0,r*static_cast<LONG>(kLabelH),kLabelW,(r+1)*static_cast<LONG>(kLabelH)};DrawTextW(dc,text[r],-1,&rc,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);}
        GdiFlush();
        for(unsigned i=0;i<kLabelW*kLabelH*2;++i)mask[i]=(static_cast<const unsigned *>(bits)[i]&0xFF)>127?1:0;
        SelectObject(dc,old_font);SelectObject(dc,old_bitmap);
    }
    if(font)DeleteObject(font);if(bitmap)DeleteObject(bitmap);DeleteDC(dc);
    if(!ok)return false;
    D3D11_TEXTURE2D_DESC t{};t.Width=kLabelW;t.Height=kLabelH*2;t.MipLevels=t.ArraySize=1;t.Format=DXGI_FORMAT_R8_UINT;t.SampleDesc.Count=1;t.Usage=D3D11_USAGE_IMMUTABLE;t.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init{mask.data(),kLabelW,0};ID3D11Texture2D *texture=nullptr;
    if(FAILED(d->CreateTexture2D(&t,&init,&texture)))return false;
    HRESULT made=d->CreateShaderResourceView(texture,nullptr,&label_view);texture->Release();return SUCCEEDED(made);
}
static unsigned create_all(ID3D11Device *d);
static ULONGLONG resources_retry;
static bool create_resources(ID3D11Device *d) {
    if(device==d) {
        if(!resources_failed)return true;
        if(GetTickCount64()<resources_retry)return false;   // 2026-11-16: a failed creation is retried every 10 s, not never
    }
    const bool new_device=device!=d;
    release_resources();device=d;device->AddRef();resources_failed=true;resources_retry=GetTickCount64()+10000;
    if(new_device){AcquireSRWLockExclusive(&lock);failed_cells.clear();ReleaseSRWLockExclusive(&lock);}   // new device: retry every failed cell
    const unsigned step=create_all(d);
#ifndef NO_RUNTIME_LOG
    if(step){static unsigned failure_logs;if(failure_logs++<8)log_event("ccz_sprites_resources_failed",step,new_device?1u:0u,0);}
#endif
    resources_failed=step!=0;return !resources_failed;
}
static unsigned create_all(ID3D11Device *d) {   // 0 = ready, otherwise the step that failed
    D3D11_INPUT_ELEMENT_DESC elements[]={{"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",1,DXGI_FORMAT_R32_UINT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0}};
    if(FAILED(d->CreateVertexShader(ccz_sprites_vs,sizeof(ccz_sprites_vs),nullptr,&vs)) || FAILED(d->CreatePixelShader(ccz_sprites_ps,sizeof(ccz_sprites_ps),nullptr,&ps)) ||
       FAILED(d->CreateInputLayout(elements,3,ccz_sprites_vs,sizeof(ccz_sprites_vs),&layout)))return 1;
    D3D11_BUFFER_DESC b{};b.ByteWidth=sizeof(Vertex)*6*kQuads;b.Usage=D3D11_USAGE_DYNAMIC;b.BindFlags=D3D11_BIND_VERTEX_BUFFER;b.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    if(FAILED(d->CreateBuffer(&b,nullptr,&vertices)))return 2;
    D3D11_TEXTURE2D_DESC t{};t.Width=256;t.Height=kRows;t.MipLevels=t.ArraySize=1;t.Format=DXGI_FORMAT_R8G8B8A8_UNORM;t.SampleDesc.Count=1;t.Usage=D3D11_USAGE_IMMUTABLE;t.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init{palette,256*4,0};ID3D11Texture2D *texture=nullptr;
    if(FAILED(d->CreateTexture2D(&t,&init,&texture)))return 3;
    HRESULT made=d->CreateShaderResourceView(texture,nullptr,&palette_view);texture->Release();if(FAILED(made))return 3;
    init.pSysMem=original_palette;texture=nullptr;if(FAILED(d->CreateTexture2D(&t,&init,&texture)))return 4;
    made=d->CreateShaderResourceView(texture,nullptr,&original_palette_view);texture->Release();if(FAILED(made))return 4;
    if(!create_labels(d))log_event("ccz_sprites_label_failed");   // labels optional
    D3D11_DEPTH_STENCIL_DESC ds{};ds.DepthEnable=TRUE;ds.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;
    ds.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;if(FAILED(d->CreateDepthStencilState(&ds,&depth_on[0])))return 5;
    ds.DepthFunc=D3D11_COMPARISON_GREATER_EQUAL;if(FAILED(d->CreateDepthStencilState(&ds,&depth_on[1])))return 5;   // reversed Z
#ifndef NO_RUNTIME_LOG
    ds.DepthFunc=D3D11_COMPARISON_NEVER;if(FAILED(d->CreateDepthStencilState(&ds,&depth_never)))depth_never=nullptr;   // ccz_sprite_fault 2 only
#endif
    ds.DepthEnable=FALSE;if(FAILED(d->CreateDepthStencilState(&ds,&depth_off)))return 5;
    D3D11_RASTERIZER_DESC rs{};rs.FillMode=D3D11_FILL_SOLID;rs.CullMode=D3D11_CULL_NONE;rs.DepthClipEnable=TRUE;
    if(FAILED(d->CreateRasterizerState(&rs,&raster)))return 6;
    D3D11_QUERY_DESC qd{D3D11_QUERY_OCCLUSION,0};
    for(unsigned i=0;i<kVisibilityQueries;++i)if(FAILED(d->CreateQuery(&qd,&visibility[i]))){for(unsigned k=0;k<kVisibilityQueries;++k)drop(visibility[k]);break;}
    return 0;
}
// PW6 rule 2: remember a cell we could not load; reason 1 decode, 2 texture, 3 view. The set is functional
// (capture keeps the native model); the log line is diagnostic-only.
static void note_cell_failed(unsigned cell,unsigned reason,HRESULT hr) {
    bool first=false;unsigned tries=0;ULONGLONG now=GetTickCount64();
    AcquireSRWLockExclusive(&lock);
    {auto made=failed_cells.try_emplace(cell,CellFail{0,0});first=made.second;CellFail &f=made.first->second;
     tries=++f.tries;f.retry=now+(std::min)(60000ull,2000ull<<(std::min)(tries-1,5u));}   // 2, 4, 8, 16, 32, 60 s
    ReleaseSRWLockExclusive(&lock);
#ifndef NO_RUNTIME_LOG
    if(!first){static unsigned retry_logs;if(retry_logs++<32)log_event("ccz_sprites_cell_retry",cell,static_cast<unsigned>(hr)&65535u,(tries<<8)|reason);return;}
    log_event("ccz_sprites_cell_failed",cell,static_cast<unsigned>(hr)&65535u,reason);
    if(device && device->GetDeviceRemovedReason()!=S_OK) {
        static volatile LONG removed_logged;
        if(InterlockedExchange(&removed_logged,1)==0)log_event("ccz_sprites_device_removed",static_cast<unsigned>(device->GetDeviceRemovedReason()),0,0);
    }
#else
    (void)first;(void)reason;(void)hr;(void)tries;
#endif
}
// Idle band flag disc (2026-11-19), palette indices like the atlas: a bright ring and a dark ground (the darkest / brightest entries
// of row 0 of the source cell's palette; index 0 stays the clear outside, the shader draws the row's outline around it) with the
// standing front frame (move frame 6) inside. 64 x 64, drawn 1 texel per CCZ pixel like the bodies.
static bool badge_atlas(unsigned source,std::vector<unsigned char> &out) {
    std::vector<unsigned char> atlas;if(!decode(source,atlas))return false;
    const unsigned *row=source>=4096 && source<5596?original_palette[0]:palette[0];
    unsigned dark=1,bright=1;float dark_l=2,bright_l=-1;
    for(unsigned i=1;i<256;++i) {
        const unsigned c=row[i];const float l=0.299f*(c&255)+0.587f*((c>>8)&255)+0.114f*((c>>16)&255);
        if(l<dark_l){dark_l=l;dark=i;}
        if(l>bright_l){bright_l=l;bright=i;}
    }
    out.assign(kBadgeSize*kBadgeSize,0);
    const float r=kBadgeSize*0.5f;
    auto distance=[&](unsigned x,unsigned y){const float dx=x+0.5f-r,dy=y+0.5f-r;return std::sqrt(dx*dx+dy*dy);};
    for(unsigned y=0;y<kBadgeSize;++y)for(unsigned x=0;x<kBadgeSize;++x) {
        const float d=distance(x,y);if(d>r-1)continue;
        out[y*kBadgeSize+x]=static_cast<unsigned char>(d>r-4?bright:dark);
    }
    const unsigned ox=(kBadgeSize-48)/2,oy=(kBadgeSize-48)/2+3;   // feet a little low: the head sits near the centre
    for(unsigned sy=0;sy<48;++sy)for(unsigned sx=0;sx<48;++sx) {
        const unsigned char v=atlas[(6*48+sy)*kAtlasW+sx];if(!v)continue;
        const unsigned x=ox+sx,y=oy+sy;if(distance(x,y)>r-4)continue;
        out[y*kBadgeSize+x]=v;
    }
    return true;
}
static ID3D11ShaderResourceView *cell_view(unsigned cell) {
    CellTexture *slot=nullptr;ULONGLONG now=GetTickCount64();
#ifndef NO_RUNTIME_LOG
    if(sprite_fault&4){note_cell_failed(cell,4,0);return nullptr;}   // diagnostic fault: every picture fails
#endif
    for(auto &t:textures){if(t.cell==cell && t.view){t.used=now;return t.view;}if(!slot || t.used<slot->used)slot=&t;}
    unsigned tries=0;
    {   // a failed picture waits for its back-off; the unit keeps the native model meanwhile (capture: known_failed)
        AcquireSRWLockShared(&lock);auto f=failed_cells.find(cell);bool wait=f!=failed_cells.end() && now<f->second.retry;
        if(f!=failed_cells.end())tries=f->second.tries;ReleaseSRWLockShared(&lock);
        if(wait)return nullptr;
    }
    std::vector<unsigned char> atlas;
    const bool badge=badge_cell(cell);
    if(badge?!badge_atlas(source_cell(cell),atlas):!decode(cell,atlas)) {
        note_cell_failed(cell,1,0);
        return nullptr;
    }
    const unsigned width=badge?kBadgeSize:kAtlasW,height=badge?kBadgeSize:kAtlasH;
    D3D11_TEXTURE2D_DESC t{};t.Width=width;t.Height=height;t.MipLevels=t.ArraySize=1;t.Format=DXGI_FORMAT_R8_UINT;t.SampleDesc.Count=1;t.Usage=D3D11_USAGE_IMMUTABLE;t.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init{atlas.data(),width,0};ID3D11Texture2D *texture=nullptr;
    HRESULT made=device->CreateTexture2D(&t,&init,&texture);
    if(FAILED(made)) {
        note_cell_failed(cell,2,made);
        return nullptr;
    }
    drop(slot->view);slot->cell=~0u;
    made=device->CreateShaderResourceView(texture,nullptr,&slot->view);
    if(SUCCEEDED(made)) {
        slot->cell=cell;slot->used=now;
        if(tries) {   // recovered after a failure
            AcquireSRWLockExclusive(&lock);failed_cells.erase(cell);ReleaseSRWLockExclusive(&lock);
#ifndef NO_RUNTIME_LOG
            static unsigned recovered_logs;if(recovered_logs++<32)log_event("ccz_sprites_cell_recovered",cell,tries,0);
#endif
        }
    }
    else note_cell_failed(cell,3,made);
    texture->Release();return slot->view;
}
static void clip(const float *m,float x,float y,float z,Vertex &v) {   // row vector * ViewProj
    v.x=x*m[0]+y*m[4]+z*m[8]+m[12];v.y=x*m[1]+y*m[5]+z*m[9]+m[13];v.z=x*m[2]+y*m[6]+z*m[10]+m[14];v.w=x*m[3]+y*m[7]+z*m[11]+m[15];
}
// Frame choice. CCZ: move 0-1 front walk, 2-3 back, 4-5 side (faces left), 6-8 stand, 9-10 weak;
// attack 0-3 front, 4-7 back, 8-11 side; special 0-2 block front/back/side, 3 hurt, 4 victory.
// 막기 > 피격 > 회심(공격 + 섬광) > 공격 > 이동 > 약함 > 대기.
struct Frame {unsigned row,size;bool mirror,flash;};
static bool effect_visible(const Unit &u,bool critical,ULONGLONG now) {
    ULONGLONG at=critical?u.crit_at:u.block_at;
    return at && now>=at && now-at<(critical?kCritMs:kBlockMs);
}
// Remaining charges affect combat only; presentation follows an actual hit.
static Frame frame_for(const Unit &u,int view,bool mirror,ULONGLONG now) {
    Frame guard_pose{kSpecialRow+static_cast<unsigned>(view)*48,48,mirror,false};
    if(u.block_at && now-u.block_at<kBlockMs)return guard_pose;
    if(u.hurt_at && now-u.hurt_at<kHurtMs)return {kSpecialRow+3*48,48,mirror,false};
    if(u.crit_at && now-u.crit_at<kCritMs)
        return {kAttackRow+static_cast<unsigned>(view*4+((now-u.crit_at)/kAttackFrameMs)%4)*64,64,mirror,((now-u.crit_at)/220)%2==0};
    bool blink=false;
    if(u.state==2)return {kAttackRow+static_cast<unsigned>(view*4+(now/kAttackFrameMs)%4)*64,64,mirror,false};
    if(now-u.moved<400)return {static_cast<unsigned>(view*2+(now/250)%2)*48,48,mirror,blink};
    if(u.morale<30)return {static_cast<unsigned>(9+(now/400)%2)*48,48,mirror,blink};
    return {static_cast<unsigned>(6+view)*48,48,mirror,blink};
}
struct SavedState {   // only the bindings this draw touches; Get calls AddRef
    ID3D11DeviceContext *c;ID3D11RenderTargetView *rtv[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};ID3D11DepthStencilView *dsv=nullptr;
    ID3D11BlendState *blend=nullptr;FLOAT factor[4]{};UINT mask=0;ID3D11DepthStencilState *depth=nullptr;UINT stencil=0;ID3D11RasterizerState *rs=nullptr;
    UINT viewport_count=D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;D3D11_VIEWPORT viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
    D3D11_PRIMITIVE_TOPOLOGY topology{};ID3D11InputLayout *il=nullptr;ID3D11Buffer *vb=nullptr;UINT stride=0,offset=0;
    ID3D11VertexShader *v=nullptr;ID3D11PixelShader *p=nullptr;ID3D11GeometryShader *g=nullptr;ID3D11HullShader *h=nullptr;ID3D11DomainShader *d=nullptr;
    ID3D11ShaderResourceView *srv[2]{};
    explicit SavedState(ID3D11DeviceContext *context):c(context) {
        c->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,rtv,&dsv);c->OMGetBlendState(&blend,factor,&mask);c->OMGetDepthStencilState(&depth,&stencil);
        c->RSGetState(&rs);c->RSGetViewports(&viewport_count,viewports);c->IAGetPrimitiveTopology(&topology);c->IAGetInputLayout(&il);c->IAGetVertexBuffers(0,1,&vb,&stride,&offset);
        c->VSGetShader(&v,nullptr,nullptr);c->PSGetShader(&p,nullptr,nullptr);c->GSGetShader(&g,nullptr,nullptr);c->HSGetShader(&h,nullptr,nullptr);c->DSGetShader(&d,nullptr,nullptr);
        c->PSGetShaderResources(0,2,srv);
    }
    ~SavedState() {
        c->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT,rtv,dsv);c->OMSetBlendState(blend,factor,mask);c->OMSetDepthStencilState(depth,stencil);
        c->RSSetState(rs);c->RSSetViewports(viewport_count,viewports);c->IASetPrimitiveTopology(topology);c->IASetInputLayout(il);c->IASetVertexBuffers(0,1,&vb,&stride,&offset);
        c->VSSetShader(v,nullptr,0);c->PSSetShader(p,nullptr,0);c->GSSetShader(g,nullptr,0);c->HSSetShader(h,nullptr,0);c->DSSetShader(d,nullptr,0);
        c->PSSetShaderResources(0,2,srv);
        for(auto &x:rtv)drop(x);drop(dsv);drop(blend);drop(depth);drop(rs);drop(il);drop(vb);drop(v);drop(p);drop(g);drop(h);drop(d);drop(srv[0]);drop(srv[1]);
    }
};
// 2026-11-01 user + local5 log: past some zoom distance the game stops submitting army displays (its own
// LOD; mode +740 is unrelated: 1 ship, 2/3 siege states), so captures stop and the sprites vanished.
// Far-zoom fallback: armies captured within kFarKeepMs keep being drawn from their own display position
// (guarded pure reads, scene pass only) until the camera is kFarFactor x the distance where submits
// stopped, so the farthest strategy view stays without sprites.
constexpr ULONGLONG kFarKeepMs=600000;constexpr float kFarFactor=2.0f;
static float stop_distance;   // camera eye-target distance when submits last stopped (0 = live)
static bool far_keep_enabled; // startup-only [ccz] ccz_far_keep=0; independent of local14 render rescue
static bool safe_read(void *out,const void *from,size_t size) {
    __try{memcpy(out,from,size);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
static bool far_position(Obj army,float out[3],float &yaw) { COST_SCOPE("ccz_sprites_far_position"); COST_CRUMB("ccz_far_army",army);   // display position of a known army, every pointer checked
    Obj vt=nullptr,display=nullptr,back=nullptr,dvt=nullptr;unsigned mode[3]={};
    if(!safe_read(&vt,army,8) || vt!=image+kArmyVtable || !safe_read(&display,army+0x148,8) || !display)return false;
    if(!safe_read(&dvt,display,8) || dvt!=image+kDisplayVtable || !safe_read(&back,display+0x40,8) || back!=army)return false;
    if(!safe_read(mode,display+0x740,12) || mode[0] || mode[2] || !safe_read(out,display+0x90,12))return false;   // +740 / +748
    memcpy(&yaw,mode+1,sizeof(yaw)); // +744, from the same guarded snapshot
    return std::isfinite(out[0]) && std::isfinite(out[1]) && std::isfinite(out[2]);
}
static bool needs_present() {
    if(!InterlockedCompareExchange(&installed,0,0))return false;
    AcquireSRWLockShared(&lock);ULONGLONG seen=camera_seen;float armed=stop_distance;ReleaseSRWLockShared(&lock);
    ULONGLONG now=GetTickCount64();
    if(seen && now-seen<500)return true;
    if(ghost::wanted()) {   // idle band pictures need the camera even with no army on screen (pure read, as the far-zoom path)
        float cam[80];if(!read_camera(cam))return false;
        AcquireSRWLockExclusive(&lock);memcpy(camera,cam,sizeof(camera));ReleaseSRWLockExclusive(&lock);
        return true;
    }
    if(!far_keep_enabled || armed<=0 || !seen || now-seen>kFarKeepMs)return false;
    float cam[80];if(!read_camera(cam))return false;   // pure read; the game camera keeps updating while zoomed out
    AcquireSRWLockExclusive(&lock);memcpy(camera,cam,sizeof(camera));camera_seen=now;ReleaseSRWLockExclusive(&lock);
    return true;
}
static volatile LONG zero_reason;   // why the last build drew nothing: 1 no army, 2/3 camera axes, 4 target behind, 5 map, 6 all culled
static bool built_this_frame;   // render thread: build() ran since the last Present, so zero_reason is this frame's
// owners/armies: per quad, the units[] index and army it was built from (labels: kNoOwner) so draw() can write the proof back.
static unsigned build(ID3D11DeviceContext *c,float height,bool linear,bool scene,unsigned cells[kQuads],unsigned owners[kQuads],Obj armies[kQuads],float cam[80]) {
    Unit list[kSlots+ghost::kMax];unsigned count=0;ULONGLONG now=GetTickCount64();built_this_frame=true;
    AcquireSRWLockShared(&lock);memcpy(cam,camera,sizeof(camera));   // cam is a pointer parameter here
    unsigned recent=0;float armed=stop_distance;
    for(unsigned k=0;k<kSlots;++k) {   // seen may be a tick ahead of now
        const Unit &u=units[k];
        if(u.army && u.seen+300>now){list[count]=u;list[count++].slot=k;}else if(u.army && u.seen+5000>now)++recent;
    }
    ReleaseSRWLockShared(&lock);
    float distance=std::sqrt((cam[4]-cam[0])*(cam[4]-cam[0])+(cam[5]-cam[1])*(cam[5]-cam[1])+(cam[6]-cam[2])*(cam[6]-cam[2]));
    if(count && armed>0){AcquireSRWLockExclusive(&lock);stop_distance=0;ReleaseSRWLockExclusive(&lock);armed=0;}   // live again
    else if(far_keep_enabled && !count && recent && armed<=0 && distance>1) {
        AcquireSRWLockExclusive(&lock);stop_distance=distance;ReleaseSRWLockExclusive(&lock);armed=distance;
#ifndef NO_RUNTIME_LOG
        log_event("ccz_sprites_far_armed",recent,static_cast<unsigned>(distance),0);
#endif
    }
    if(far_keep_enabled && !count && scene && armed>0 && distance<=armed*kFarFactor) {
        AcquireSRWLockExclusive(&lock);
        for(unsigned k=0;k<kSlots;++k) {
            Unit &u=units[k];
            if(!u.army || now-u.seen>kFarKeepMs)continue;
            float pos[3],yaw;if(!far_position(u.army,pos,yaw))continue;
            float dx=pos[0]-u.x,dz=pos[2]-u.z;
            if(dx*dx+dz*dz>1.0f){float length=std::sqrt(dx*dx+dz*dz);u.fx=dx/length;u.fz=dz/length;u.facing=true;u.moved=now;}
            native_facing(u,yaw);
            u.x=pos[0];u.y=pos[1];u.z=pos[2];list[count]=u;list[count++].slot=k;
        }
        ReleaseSRWLockExclusive(&lock);
    }
#ifndef NO_RUNTIME_LOG
    // 2026-11-01 user: sprites vanish at the farthest zoom. Once each: the game stopped submitting armies
    // (native LOD) vs. every captured army culled here.
    static volatile LONG logged_stop,logged_cull;
    if(!count && recent && InterlockedExchange(&logged_stop,1)==0)log_event("ccz_sprites_submit_stopped",recent,static_cast<unsigned>(height),0);
#endif
    // Idle band pictures: scene passes only, never past the far-zoom cut-off of the armies themselves; the unit table plus the
    // pictures stays within kSlots so the label quads still fit kQuads.
    if(scene && (armed<=0 || distance<=armed*kFarFactor)) {
        const unsigned room=count<kSlots?(kSlots-count<ghost::kMax*3?kSlots-count:ghost::kMax*3):0;   // picture + two flag parts per site
        count+=ghost::collect(list+count,room,now);
    }
    if(!count){zero_reason=1;return 0;}
    std::sort(list,list+count,[](const Unit &a,const Unit &b){return a.cell<b.cell;});
    const float *vp=cam+60;
    // Camera axes from eye/target; screen direction checked by projection (no handedness assumption).
    float fx=cam[4]-cam[0],fy=cam[5]-cam[1],fz=cam[6]-cam[2],fl=std::sqrt(fx*fx+fy*fy+fz*fz);if(fl<1e-3f){zero_reason=2;return 0;}
    fx/=fl;fy/=fl;fz/=fl;
    float rx=-fz,rz=fx,rl=std::sqrt(rx*rx+rz*rz);   // horizontal right
    // 2026-11-01 local10 log (reason 3): the strategy zoom step looks straight down, so forward has no
    // horizontal part. Take the right axis from the View matrix (row vectors: right = column 0) instead,
    // then the last good one; the projection test below still fixes its sign.
    static float last_rx=1,last_rz=0;
    if(rl<0.05f){rx=cam[12];rz=cam[20];rl=std::sqrt(rx*rx+rz*rz);if(rl<1e-3f){rx=last_rx;rz=last_rz;rl=1;}}
    if(rl<1e-3f){zero_reason=3;return 0;}
    rx/=rl;rz/=rl;last_rx=rx;last_rz=rz;
    float ux=-rz*fy,uy=rz*fx-rx*fz,uz=rx*fy;   // up = right x forward, right = (rx,0,rz)
    {Vertex a,b,e;clip(vp,cam[4],cam[5],cam[6],a);clip(vp,cam[4]+rx*100,cam[5],cam[6]+rz*100,b);clip(vp,cam[4]+ux*100,cam[5]+uy*100,cam[6]+uz*100,e);
     if(a.w<=0){zero_reason=4;return 0;}if(b.x/b.w<a.x/a.w){rx=-rx;rz=-rz;}if(e.y/e.w<a.y/a.w){ux=-ux;uy=-uy;uz=-uz;}}
    float hx=fx,hz=fz,hl=std::sqrt(hx*hx+hz*hz);if(hl>1e-3f){hx/=hl;hz/=hl;}else{hx=ux;hz=uz;}   // top-down: screen up = away
    D3D11_MAPPED_SUBRESOURCE mapped{};if(FAILED(c->Map(vertices,0,D3D11_MAP_WRITE_DISCARD,0,&mapped))){zero_reason=5;return 0;}
    auto out=static_cast<Vertex *>(mapped.pData);unsigned written=0;
    static thread_local Vertex labels[kSlots*2][4];unsigned label_count=0;
    const bool flags_alone=InterlockedCompareExchange(&idle_only,0,0)!=0;
    for(unsigned i=0;i<count;++i) {
        const Unit &u=list[i];
        if(u.badge) {   // idle band flag part: texel rows lo..hi of the 64-texel disc, over the picture (or low over the ground alone)
            float scale=1;
            {Vertex a,b;clip(vp,u.x,u.y,u.z,a);clip(vp,u.x+ux*48*kPixel,u.y+uy*48*kPixel,u.z+uz*48*kPixel,b);
             if(a.w>1 && b.w>1){float h=std::fabs(b.y/b.w-a.y/a.w)*0.5f*height,want=kMinScreen*height;if(h>1 && h<want)scale=want/h<kMaxScale?want/h:kMaxScale;}}
            const float size=34*kPixel*scale,hw=size*0.5f,lift=(flags_alone?16:50)*kPixel*scale;
            const float y0=lift+(kBadgeSize-u.hi)*size/kBadgeSize,y1=lift+(kBadgeSize-u.lo)*size/kBadgeSize;
            const float bx=u.x-fx*kTowardCamera*2,by=u.y-fy*kTowardCamera*2,bz=u.z-fz*kTowardCamera*2;
            Vertex q[4];   // 0 bottom-left, 1 top-left, 2 top-right, 3 bottom-right
            clip(vp,bx-rx*hw+ux*y0,by+uy*y0,bz-rz*hw+uz*y0,q[0]);clip(vp,bx-rx*hw+ux*y1,by+uy*y1,bz-rz*hw+uz*y1,q[1]);
            clip(vp,bx+rx*hw+ux*y1,by+uy*y1,bz+rz*hw+uz*y1,q[2]);clip(vp,bx+rx*hw+ux*y0,by+uy*y0,bz+rz*hw+uz*y0,q[3]);
            if(q[0].w<=1 || q[1].w<=1 || q[2].w<=1 || q[3].w<=1)continue;
            const float v0=static_cast<float>(u.lo),v1=static_cast<float>(u.hi),u1=static_cast<float>(kBadgeSize);
            q[0].u=0;q[0].v=v1;q[1].u=0;q[1].v=v0;q[2].u=u1;q[2].v=v0;q[3].u=u1;q[3].v=v1;
            for(auto &x:q)x.team=u.team|(linear?0x100u:0u)|(u.badge==2?0x200u:0u);
            Vertex *o=out+written*6;o[0]=q[0];o[1]=q[1];o[2]=q[2];o[3]=q[0];o[4]=q[2];o[5]=q[3];owners[written]=kNoOwner;armies[written]=nullptr;cells[written++]=u.cell;
            continue;
        }
        float ffx=u.facing?u.fx:std::sin(u.yaw),ffz=u.facing?u.fz:std::cos(u.yaw);
        if(!u.army){ffx=-hx;ffz=-hz;}   // idle band picture faces the camera
        float side=ffx*rx+ffz*rz,depth=ffx*hx+ffz*hz;
        int view=std::fabs(side)>std::fabs(depth)?2:depth>0?1:0;   // 0 front, 1 back, 2 side
        Frame f=frame_for(u,view,view==2 && side>0,now);
        float scale=1;   // keep a readable screen size when zoomed out
        {Vertex a,b;clip(vp,u.x,u.y,u.z,a);clip(vp,u.x+ux*48*kPixel,u.y+uy*48*kPixel,u.z+uz*48*kPixel,b);
         if(a.w>1 && b.w>1) {
             float h=std::fabs(b.y/b.w-a.y/a.w)*0.5f*height,want=kMinScreen*height;
             if(h>1 && h<want)scale=want/h<kMaxScale?want/h:kMaxScale;
         }}
        float size=f.size*kPixel*scale,drop_px=(f.size-48)*0.5f*kPixel*scale;
        float gx=u.x,gz=u.z;
        float bx=gx-fx*kTowardCamera-ux*drop_px,by=u.y-fy*kTowardCamera-uy*drop_px,bz=gz-fz*kTowardCamera-uz*drop_px;
        float u0=f.mirror?static_cast<float>(f.size):0,u1=f.mirror?0:static_cast<float>(f.size);
        float v0=static_cast<float>(f.row),v1=static_cast<float>(f.row+f.size);
        Vertex q[4];   // 0 bottom-left, 1 top-left, 2 top-right, 3 bottom-right
        float hw=size*0.5f;
        clip(vp,bx-rx*hw,by,bz-rz*hw,q[0]);clip(vp,bx-rx*hw+ux*size,by+uy*size,bz-rz*hw+uz*size,q[1]);
        clip(vp,bx+rx*hw+ux*size,by+uy*size,bz+rz*hw+uz*size,q[2]);clip(vp,bx+rx*hw,by,bz+rz*hw,q[3]);
        if(q[0].w<=1 || q[1].w<=1 || q[2].w<=1 || q[3].w<=1)continue;
        q[0].u=u0;q[0].v=v1;q[1].u=u0;q[1].v=v0;q[2].u=u1;q[2].v=v0;q[3].u=u1;q[3].v=v1;
        for(auto &x:q)x.team=u.team|(linear?0x100u:0u)|(f.flash?0x200u:0u);
        Vertex *o=out+written*6;o[0]=q[0];o[1]=q[1];o[2]=q[2];o[3]=q[0];o[4]=q[2];o[5]=q[3];owners[written]=u.army?u.slot:kNoOwner;armies[written]=u.army;cells[written++]=u.cell;
        // State labels stacked over the head (48px body top), 1 texel = 1 CCZ pixel; a short hop when triggered.
        float lift=(48+3)*kPixel*scale;
        for(unsigned kind=0;kind<2;++kind) {
            if(!effect_visible(u,kind!=0,now) || !label_view)continue;
            ULONGLONG at=kind?u.crit_at:u.block_at;float hop=at && now-at<300?(300-static_cast<float>(now-at))/300*6*kPixel*scale:0;
            float lw=kLabelW*0.5f*kPixel*scale,lh=kLabelH*kPixel*scale,ly=lift+hop;lift+=lh;
            float cx=gx-fx*kTowardCamera*2,cy=u.y-fy*kTowardCamera*2,cz=gz-fz*kTowardCamera*2;
            Vertex *l=labels[label_count++];
            clip(vp,cx-rx*lw+ux*ly,cy+uy*ly,cz-rz*lw+uz*ly,l[0]);clip(vp,cx-rx*lw+ux*(ly+lh),cy+uy*(ly+lh),cz-rz*lw+uz*(ly+lh),l[1]);
            clip(vp,cx+rx*lw+ux*(ly+lh),cy+uy*(ly+lh),cz+rz*lw+uz*(ly+lh),l[2]);clip(vp,cx+rx*lw+ux*ly,cy+uy*ly,cz+rz*lw+uz*ly,l[3]);
            float t0=static_cast<float>(kind*kLabelH),t1=t0+kLabelH;
            l[0].u=0;l[0].v=t1;l[1].u=0;l[1].v=t0;l[2].u=kLabelW;l[2].v=t0;l[3].u=kLabelW;l[3].v=t1;
            for(unsigned k=0;k<4;++k)l[k].team=u.team|(linear?0x100u:0u)|0x400u|(kind<<11);
            if(l[0].w<=1 || l[1].w<=1 || l[2].w<=1 || l[3].w<=1)--label_count;
        }
    }
    for(unsigned i=0;i<label_count;++i) {   // after every unit: one batch, drawn over the sprites
        const Vertex *l=labels[i];Vertex *o=out+written*6;o[0]=l[0];o[1]=l[1];o[2]=l[2];o[3]=l[0];o[4]=l[2];o[5]=l[3];owners[written]=kNoOwner;armies[written]=nullptr;cells[written++]=kLabelCell;
    }
    c->Unmap(vertices,0);zero_reason=written?0:6;
#ifndef NO_RUNTIME_LOG
    if(!written && InterlockedExchange(&logged_cull,1)==0)log_event("ccz_sprites_all_culled",count,static_cast<unsigned>(cam[1]>0?cam[1]:0),0);
#endif
    return written;
}
static bool linear_target(ID3D11RenderTargetView *rtv) {   // float / sRGB targets take linear colour
    D3D11_RENDER_TARGET_VIEW_DESC d{};rtv->GetDesc(&d);
    switch(d.Format) {
        case DXGI_FORMAT_R16G16B16A16_FLOAT:case DXGI_FORMAT_R11G11B10_FLOAT:case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:return true;
        default:return false;
    }
}
static void poll_visibility(ID3D11DeviceContext *c) {   // render thread; never flushes
    static unsigned zero_run;   // batches in a row whose query passed no pixel (feeds covered)
    for(unsigned i=0;i<kVisibilityQueries;++i) {
        if(!visibility_busy[i] || !visibility[i])continue;
        UINT64 pixels=0;HRESULT hr=c->GetData(visibility[i],&pixels,sizeof(pixels),D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if(hr==S_FALSE)continue;
        visibility_busy[i]=false;if(hr!=S_OK)continue;
        if(pixels){zero_run=0;if(covered)InterlockedExchange(&covered,0);}
        else if(++zero_run>=30 && !covered) {
            InterlockedExchange(&covered,1);
#ifndef NO_RUNTIME_LOG
            static unsigned covered_logs;if(covered_logs++<16)log_event("ccz_sprites_covered",static_cast<unsigned>(visibility_seen),static_cast<unsigned>(visibility_hidden),fallback_enabled?1u:0u);
#endif
        }
        if(pixels) {
            LONGLONG at=static_cast<LONGLONG>(visibility_at[i]);
            if(at>InterlockedCompareExchange64(&last_draw_ok,0,0))InterlockedExchange64(&last_draw_ok,at);
#ifndef NO_RUNTIME_LOG
            ++visibility_seen;visibility_streak=0;
#endif
        }
#ifndef NO_RUNTIME_LOG
        else {
            ++visibility_hidden;
            if(++visibility_streak==30 && visibility_logs++<32) {   // ~30 drawn-but-invisible frames in a row
                log_event("ccz_sprites_invisible",static_cast<unsigned>(visibility_seen),static_cast<unsigned>(visibility_hidden),0);
                force_dump=true;   // 2026-11-16: dump the bind order of a drawn-but-covered frame too
            }
        }
#endif
    }
}
static bool draw(ID3D11DeviceContext *c,ID3D11RenderTargetView *rtv,ID3D11DepthStencilView *dsv,float width,float height,bool scene) { COST_SCOPE("ccz_draw");
    LARGE_INTEGER t0;QueryPerformanceCounter(&t0);
    poll_visibility(c);
#ifndef NO_RUNTIME_LOG
    if(sprite_fault&1){stats.units_drawn=0;return false;}   // diagnostic fault: the draw point is reached but nothing is drawn
#endif
    ID3D11Query *query=nullptr;unsigned query_slot=0;
    // Render thread only and never re-entered (drawing passes our own binds through): per-quad tables stay off the stack.
    static unsigned cells[kQuads],owners[kQuads];static Obj armies[kQuads];static bool emitted[kQuads];   // emitted = went out with its picture
    float cam[80];
    unsigned written=build(c,height,linear_target(rtv),scene,cells,owners,armies,cam);
    bool submitted=false,bodies=false;
    if(written) {
        // Reversed Z when the near plane maps above the far plane.
        const float *P=cam+44;float n=cam[76]>0?cam[76]:1,fr=cam[77]>n?cam[77]:n*1000;
        bool reversed=((-n*P[10]+P[14])/n)>((-fr*P[10]+P[14])/fr);
        ID3D11DepthStencilState *depth_state=dsv?depth_on[reversed?1:0]:depth_off;
#ifndef NO_RUNTIME_LOG
        if((sprite_fault&2) && depth_never)depth_state=depth_never;   // diagnostic fault: drawn but every pixel fails depth
#endif
        InterlockedExchange(&drawing,1);
        {
            SavedState saved(c);
            D3D11_VIEWPORT viewport{0,0,width,height,0,1};
            c->OMSetRenderTargets(1,&rtv,dsv);c->OMSetBlendState(nullptr,nullptr,0xFFFFFFFFu);c->OMSetDepthStencilState(depth_state,0);
            c->RSSetState(raster);c->RSSetViewports(1,&viewport);c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);c->IASetInputLayout(layout);
            UINT stride=sizeof(Vertex),offset=0;c->IASetVertexBuffers(0,1,&vertices,&stride,&offset);
            c->VSSetShader(vs,nullptr,0);c->PSSetShader(ps,nullptr,0);c->GSSetShader(nullptr,nullptr,0);c->HSSetShader(nullptr,nullptr,0);c->DSSetShader(nullptr,nullptr,0);
            for(unsigned k=0;k<kVisibilityQueries && !query;++k) {
                unsigned s=(visibility_next+k)%kVisibilityQueries;
                if(visibility[s] && !visibility_busy[s]){query=visibility[s];query_slot=s;}
            }
            bool query_open=false;
            if(query){c->Begin(query);query_open=true;}
            for(unsigned i=0;i<written;) {
                unsigned j=i;while(j<written && cells[j]==cells[i])++j;
                // labels and idle band flags (sorted after the bodies) do not prove a body is visible
                if(query_open && (cells[i]==kLabelCell || badge_cell(cells[i]))){c->End(query);query_open=false;}
                const unsigned source=source_cell(cells[i]);   // a flag disc keeps the palette of the picture it was built from
                ID3D11ShaderResourceView *views[2]={cells[i]==kLabelCell?label_view:cell_view(cells[i]),source>=4096 && source<5596?original_palette_view:palette_view};
                if(views[0]){c->PSSetShaderResources(0,2,views);c->Draw((j-i)*6,i*6);submitted=true;if(cells[i]!=kLabelCell && !badge_cell(cells[i]))bodies=true;}
                for(unsigned k=i;k<j;++k)emitted[k]=views[0]!=nullptr;
                i=j;
            }
            if(query_open)c->End(query);
            if(query){visibility_busy[query_slot]=true;visibility_at[query_slot]=GetTickCount64();visibility_next=query_slot+1;}
        }
        InterlockedExchange(&drawing,0);
    }
    if(bodies) {   // 2026-11-16 per-unit proof: units whose picture went out in this batch (capture() reads drawn_at)
        const ULONGLONG at=GetTickCount64();
        AcquireSRWLockExclusive(&lock);
        for(unsigned k=0;k<written;++k)if(emitted[k] && owners[k]<kSlots && units[owners[k]].army==armies[k])units[owners[k]].drawn_at=at;
        ReleaseSRWLockExclusive(&lock);
        InterlockedExchange64(&last_submit,static_cast<LONGLONG>(at));InterlockedIncrement(&submit_count);
    }
    stats.units_drawn=submitted?static_cast<LONG>(written):0;
    // PW6: draws are really happening. With a visibility query the result (poll_visibility) decides instead.
    if(submitted && !query)InterlockedExchange64(&last_draw_ok,static_cast<LONGLONG>(GetTickCount64()));
#ifndef NO_RUNTIME_LOG
    if(submitted)InterlockedExchangeAdd(&health_drawn,static_cast<LONG>(written));
    if(submitted) {
        ULONGLONG submitted_at=GetTickCount64();
        // The shared overlay can skip on_present altogether while no module needs it.
        // A gap also detects recovery across that path, without a per-frame log.
        if((zoom_recovery_pending || (last_sprite_submission && submitted_at-last_sprite_submission>=500)) && zoom_recovery_logs++<32)
            log_event("ccz_sprites_zoom_recovered",written,far_keep_enabled?1u:0u,scene?1u:0u);
        last_sprite_submission=submitted_at;
        zoom_recovery_pending=false;has_submitted_sprites=true;
    } else if(has_submitted_sprites)zoom_recovery_pending=true;
#endif
    LARGE_INTEGER t1;QueryPerformanceCounter(&t1);LONGLONG spent=t1.QuadPart-t0.QuadPart;
    stats.cost_qpc+=spent;if(spent>stats.cost_max)stats.cost_max=spent;
    return submitted;
}
static bool draw_scene(ID3D11DeviceContext *c,ID3D11RenderTargetView *rtv,ID3D11DepthStencilView *dsv) { COST_SCOPE("ccz_draw_scene");
    if(!needs_present() || !device || resources_failed)return false;
    ++stats.scene_draws;
    {D3D11_RENDER_TARGET_VIEW_DESC d{};rtv->GetDesc(&d);stats.target_format=d.Format;}
    return draw(c,rtv,dsv,static_cast<float>(want_width),static_cast<float>(want_height),true);
}
static void on_present(ID3D11Device *d,ID3D11DeviceContext *c,ID3D11Texture2D *back,const D3D11_TEXTURE2D_DESC &desc) { COST_SCOPE("ccz_sprites_present");
    // 2026-11-16: between frames no draw is running on this thread; a flag left set would pass every bind through for good.
    const LONG stuck=InterlockedExchange(&drawing,0);
    LARGE_INTEGER now_qpc;QueryPerformanceCounter(&now_qpc);
    if(!stats.qpc_per_second){LARGE_INTEGER f;QueryPerformanceFrequency(&f);stats.qpc_per_second=f.QuadPart;}
    if(stats.last_present){LONGLONG gap=now_qpc.QuadPart-stats.last_present;stats.frame_qpc+=gap;if(gap>stats.frame_max)stats.frame_max=gap;}
bool zoom_stall=stats.last_present && stats.qpc_per_second &&
        now_qpc.QuadPart-stats.last_present>stats.qpc_per_second/10;
#ifndef NO_RUNTIME_LOG
    // Distinguish the observed ~500ms frame stall from time spent inside our
    // OMSetRenderTargets hook and sprite drawing; avoid logging every bind.
    static LONGLONG previous_cost;
    static LONG previous_sets;
    static unsigned stall_logs;
    if(stats.last_present && stats.qpc_per_second &&
       now_qpc.QuadPart-stats.last_present>stats.qpc_per_second/10 && stall_logs++<32) {
        LONGLONG gap=now_qpc.QuadPart-stats.last_present;
        LONGLONG own=stats.cost_qpc-previous_cost;
        log_event("ccz_sprites_frame_stall",
            static_cast<unsigned>(gap*1000/stats.qpc_per_second),
            static_cast<unsigned>((own>0?own:0)*1000/stats.qpc_per_second),
            static_cast<unsigned>(stats.set_calls-previous_sets));
    }
    previous_cost=stats.cost_qpc;previous_sets=stats.set_calls;
#endif
    stats.last_present=now_qpc.QuadPart;++stats.frames;
#ifndef NO_RUNTIME_LOG
    // 2026-11-01 user (local11): after the stall before the farthest zoom the UI-bind draw point never came
    // (path shape 23). Dump one sampled frame's binds that missed the draw while armies were captured, and one
    // that drew, to compare the order (80 lines each at most).
    // local13 (user: record everything): whole frames, up to 3 missed frames 5 s apart and 2 drawn ones.
    static LONG dumped_bad,dumped_good,forced_dumps;static LONGLONG next_bad;static bool force_armed;
    if(stats.seq_armed && stats.seq_count) {
        bool live=camera_seen && GetTickCount64()-camera_seen<500;
        bool bad=live && !drawn_this_frame && main_rtv,good=drawn_this_frame,forced=force_armed && forced_dumps<8;
        if(forced || (bad && dumped_bad<3 && stats.frames>=next_bad) || (good && dumped_good<2)) {
            if(forced)++forced_dumps;else{++(bad?dumped_bad:dumped_good);if(bad)next_bad=stats.frames+300;}
            log_event("ccz_sprites_bind_frame",forced?2u:bad?1u:0u,static_cast<unsigned>(stats.seq_count),static_cast<unsigned>(want_width<<16|want_height));
            for(LONG i=0;i<stats.seq_count;++i) {
                const Stats::Bind &b=stats.seq[i];
                log_event("ccz_sprites_bind",b.width<<16|b.height,b.format|b.count<<24,b.dsv|b.depth_width<<6|b.depth_height<<18);
            }
        }
    }
    bool want_sample=(dumped_bad<3 || dumped_good<2) && stats.frames%30==0;
    force_armed=false;
#else
    bool want_sample=false;
#endif
    stats.seq_armed=0;if(stats.frames%600==1 || want_sample){stats.seq_count=0;stats.seq_armed=1;}   // sample one frame's target sequence
#ifndef NO_RUNTIME_LOG
    if(force_dump && forced_dumps<8){stats.seq_count=0;stats.seq_armed=1;force_armed=true;}
    force_dump=false;
    {   // back-buffer size with the path logs (every recorded bind order so far is 1280x720)
        static unsigned last_size,size_logs;const unsigned size=desc.Width<<16|desc.Height;
        if(size!=last_size && size_logs++<16){last_size=size;log_event("ccz_sprites_backbuffer",desc.Width,desc.Height,desc.Format);}
    }
#endif
    back_texture=back;
    const bool resized=(want_width && static_cast<UINT>(want_width)!=desc.Width) || (want_height && static_cast<UINT>(want_height)!=desc.Height);
    InterlockedExchange(&want_width,static_cast<LONG>(desc.Width));InterlockedExchange(&want_height,static_cast<LONG>(desc.Height));
#ifndef NO_RUNTIME_LOG
    // 2026-11-01 user (local8): from the strategy zoom step (a short stall) the sprites stop. Every 60 frames
    // compare the draw path; log only when its shape changes (first 200): value = flags (1 captures,
    // 2 scene colour target found, 4 post-processing seen, 8 drawn at the UI bind, 16 units drawn),
    // army = captures in the window, hex = present-path draws in the window.
    // 2026-11-16 more bits: 32 a late back-buffer-sized depth was seen, 64 the bind hook was not called at all, 128 the back
    // buffer was bound again after our draw, 0x1000 drawn at the UI-layer bind, 0x2000 drawn at the 3/8 pass (bits 8-11 =
    // zero reason). A drawn -> not drawn change arms one forced bind dump. local199: 0x4000 drawn at the scene-depth back-buffer bind
    // after post-processing (the main draw point now).
    {
        static LONG window_frames,window_main,window_post,window_drawn,window_present,last_captures,last_present,last_shape=-1,shape_logs;
        static LONG window_late,window_ui,window_post_draw,window_rebound,window_sets;static unsigned window_fallback;
        ++window_frames;if(main_rtv)++window_main;if(post_seen)++window_post;if(drawn_this_frame)++window_drawn;
        if(late_depth)++window_late;if(drew_ui)++window_ui;if(drew_after_post)++window_post_draw;if(rebound_after_draw)++window_rebound;
        if(window_frames>=60) {
            LONG caught=InterlockedCompareExchange(&captures,0,0)-last_captures;last_captures+=caught;
            LONG at_present=stats.present_draws-last_present;last_present=stats.present_draws;
            const bool silent=stats.set_calls==window_sets,rescued=fallback_draws!=window_fallback;
            window_sets=stats.set_calls;window_fallback=fallback_draws;
            LONG shape=(caught?1:0)|(window_main?2:0)|(window_post?4:0)|(window_drawn?8:0)|(stats.units_drawn?16:0);
            shape|=(window_late?32:0)|(silent?64:0)|(window_rebound?128:0)|(window_ui?0x1000:0)|(rescued?0x2000:0)|(window_post_draw?0x4000:0);
            shape|=static_cast<LONG>(zero_reason)<<8;   // hex = present draws; value bits 8+ = zero reason
            if(shape!=last_shape && ++shape_logs<=200)log_event("ccz_sprites_path",static_cast<unsigned>(caught),static_cast<unsigned>(shape),static_cast<unsigned>(at_present));
            if(last_shape>=0 && (last_shape&8) && !(shape&8) && caught)force_dump=true;
            last_shape=shape;window_frames=window_main=window_post=window_drawn=window_late=window_ui=window_post_draw=window_rebound=0;
        }
    }
#endif
    {   // 2026-11-16 draw-gap watchdog: armies captured and the module wants to draw, but no batch went out for 60 frames.
        // The per-unit proof already brings the models back; this drops the cached views (a stale depth or RTV address is the
        // known way to miss every draw point) and, in diagnostic builds, says which part of the path is missing.
        static LONG gap_frames,gap_submits;
        const LONG submits=InterlockedCompareExchange(&submit_count,0,0);
        // A build this frame that found no army (1) or culled them all (6) had nothing to draw. Without a build this frame the
        // reason is stale (a draw point never reached keeps the last value, often 1 after a far zoom), so that still counts.
        const LONG why=zero_reason;const bool nothing=built_this_frame && (why==1 || why==6);
        const bool live=camera_seen && GetTickCount64()-camera_seen<500 && !nothing && needs_present() && device==d;
        if(!live || submits!=gap_submits) {
#ifndef NO_RUNTIME_LOG
            static unsigned resumed_logs;
            if(live && gap_frames>=60 && resumed_logs++<32)log_event("ccz_sprites_draw_resumed",static_cast<unsigned>(gap_frames),static_cast<unsigned>(submits-gap_submits),drew_after_post?3u:drew_ui?1u:drawn_this_frame?2u:0u);
#endif
            gap_frames=0;gap_submits=submits;
        } else if(++gap_frames==60 || (gap_frames>60 && (gap_frames-60)%600==0)) {
            memset(sizes,0,sizeof(sizes));sizes_next=0;reset_views(true);
#ifndef NO_RUNTIME_LOG
            static LONG gap_sets;static unsigned gap_logs;
            const unsigned flags=(stats.set_calls==gap_sets?1u:0u)|(main_rtv?2u:0u)|(post_seen?4u:0u)|(late_depth?8u:0u)|
                (resources_failed?16u:0u)|(fallback_attempted?32u:0u)|(!set_table?64u:0u)|(stuck?128u:0u)|static_cast<unsigned>(zero_reason)<<8;
            if(gap_logs++<32)log_event("ccz_sprites_draw_gap",static_cast<unsigned>(gap_frames),flags,desc.Width<<16|desc.Height);
            gap_sets=stats.set_calls;force_dump=true;
#endif
        }
    }
    // 2026-11-16 hook check: our slot holding exactly the function we chain to means someone put the pre-hook value back (a
    // restore that removed us), so the bind hook can be installed again safely. Any other value is left alone: a later hook
    // may chain to us, and taking it as our original would loop.
    if(set_table && original_set && set_table[kSetTargetsSlot]==reinterpret_cast<void *>(original_set)) {
        set_table=nullptr;
#ifndef NO_RUNTIME_LOG
        static unsigned lost_logs;if(lost_logs++<8)log_event("ccz_sprites_hook_lost",static_cast<unsigned>(stats.set_calls),0,0);
#endif
    }
#ifdef NO_RUNTIME_LOG
    (void)stuck;
#endif
    bool scene_drawn=drawn_this_frame;drawn_this_frame=false;post_seen=false;
    fallback_attempted=false;drop(main_rtv);drop(frame_dsv);
    late_depth=false;memset(late_views,0,sizeof(late_views));late_next=0;drew_ui=false;built_this_frame=false;   // per-frame path signature (2026-11-16)
    pre_count=0;drew_after_post=false;
#ifndef NO_RUNTIME_LOG
    rebound_after_draw=false;
#endif
    // RTV size cache holds raw pointers. A zoom transition can recreate render
    // targets at reused addresses, so invalidate after a >100ms Present gap.
    // Keep the original periodic eviction as a fallback.
    if(zoom_stall || resized || stats.frames%600==0){memset(sizes,0,sizeof(sizes));sizes_next=0;}
    if(zoom_stall || resized) {
        reset_views(resized);
#ifndef NO_RUNTIME_LOG
        static unsigned reset_logs;
        if(resized && reset_logs++<16)log_event("ccz_sprites_view_reset",desc.Width,desc.Height,zoom_stall?1u:0u);
#endif
    }
    if(device==d)poll_visibility(c);   // queries belong to the current device only
#ifndef NO_RUNTIME_LOG
    if(probe_mode)probe::frame();
#endif

    if(!needs_present()) {
#ifndef NO_RUNTIME_LOG
        if(has_submitted_sprites)zoom_recovery_pending=true;
#endif
        return;
    }
    if(!create_resources(d))return;
    hook_context(c);
#ifndef NO_RUNTIME_LOG
    if(probe_mode)probe::hook(c);
#endif
    // 2026-10-31 live (local2): frames without post-processing (strategy phase, report screens) fell back to
    // drawing here, over the finished UI, right before advisor_briefing copies the back buffer for its portrait
    // and clean-scene reuse -> background portrait, replayed lord faces. With the scene hook active, a frame
    // that never reached the UI layer after post has no world view to decorate: draw nothing.
    if(scene_drawn || set_table)return;
    ID3D11RenderTargetView *rtv=nullptr;if(FAILED(d->CreateRenderTargetView(back,nullptr,&rtv)))return;
    ID3D11DepthStencilView *dsv=nullptr;
    if(InterlockedCompareExchange(&depth_test,0,0)) {
        AcquireSRWLockShared(&dsv_lock);dsv=scene_dsv;if(dsv)dsv->AddRef();ReleaseSRWLockShared(&dsv_lock);
    }
    ++stats.present_draws;
    draw(c,rtv,dsv,static_cast<float>(desc.Width),static_cast<float>(desc.Height),false);
    rtv->Release();if(dsv)dsv->Release();
}
enum Mark {kMarkBlock,kMarkCrit};
static void mark(Obj victim,Obj attacker,Mark kind) {   // game thread; touches only our copies
    ULONGLONG now=GetTickCount64();
    AcquireSRWLockExclusive(&lock);
    for(auto &u:units) {
        if(u.army==victim){if(kind==kMarkBlock)u.block_at=now;else u.hurt_at=now;}
        else if(kind==kMarkCrit && u.army==attacker)u.crit_at=now;
    }
    ReleaseSRWLockExclusive(&lock);
}
static bool guards_match() {
    // Slot ownership (13A03D0) is judged by the Layout hook manager (whitelist row: the fog mod checks it).
    unsigned long long hash=0xCBF29CE484222325ull;
    for(unsigned i=0;i<kSubmitSize;++i)hash=(hash^image[kSubmit+i])*0x100000001B3ull;
    return hash==kSubmitHash;
}
static bool install(bool hide,bool depth,bool far_keep,bool fallback) { COST_SCOPE("ccz_install");
    if(InterlockedCompareExchange(&installed,0,0) || !guards_match())return false;
    InterlockedExchange(&hide_model,hide?1:0);InterlockedExchange(&depth_test,depth?1:0);
    far_keep_enabled=far_keep;fallback_enabled=fallback;stop_distance=0;
#ifndef NO_RUNTIME_LOG
    log_event("ccz_sprites_far_keep",far_keep?1u:0u,static_cast<unsigned>(kFarKeepMs),0);
    {   // 2026-11-16 fault switch (diagnostic builds, development group only): reproduce a failure to see the native model return
        wchar_t config[MAX_PATH]{};sprite_fault=0;
        if(mod_ini_path(config,L"기능설정.ini") && GetPrivateProfileIntW(L"groups",L"diagnostics",1,config))
            sprite_fault=static_cast<unsigned>(GetPrivateProfileIntW(L"diagnostics",L"ccz_sprite_fault",0,config))&7u;
        if(sprite_fault)log_event("ccz_sprites_fault",sprite_fault,fallback?1u:0u,0);
        probe_mode=sprite_fault==0 && config[0] && GetPrivateProfileIntW(L"groups",L"diagnostics",1,config) &&
            GetPrivateProfileIntW(L"diagnostics",L"ccz_draw_probe",0,config)!=0;   // measurement build (not mixed with a fault)
        if(probe_mode)log_event("ccz_probe_on",hide?1u:0u,depth?1u:0u,0);
    }
#endif
    if(!load_asset()){log_event("ccz_sprites_asset_missing");return false;}
    if(layout_client::vt(kSubmitSlot,reinterpret_cast<void *>(&submit),reinterpret_cast<void **>(&next_submit))<=0)return false;
    InterlockedExchange(&installed,1);log_event("ccz_sprites_installed",static_cast<unsigned>(entries.size()),hide?1:0,depth?1:0);return true;
}
// 2026-11-19 의용병 무리 with 조조전 병종 그림 off: only the idle band flags. No army-display hook (armies keep their native models and
// nothing is captured), the picture data is loaded here on the install worker before anything reads it, and the shared overlay is
// installed later from the band day pass (game thread) once a site exists.
static bool install_idle_only(bool depth) { COST_SCOPE("ccz_install_idle");
    if(InterlockedCompareExchange(&installed,0,0))return false;
    if(entries.empty() && !load_asset()){log_event("ccz_sprites_asset_missing");return false;}
    InterlockedExchange(&hide_model,0);InterlockedExchange(&depth_test,depth?1:0);
    far_keep_enabled=false;fallback_enabled=false;stop_distance=0;
    InterlockedExchange(&idle_only,1);InterlockedExchange(&installed,1);
    log_event("ccz_sprites_idle_installed",static_cast<unsigned>(entries.size()),depth?1u:0u,0);return true;
}
static bool idle_flags_alone() {return InterlockedCompareExchange(&idle_only,0,0) && InterlockedCompareExchange(&installed,0,0);}
// Band day pass (game thread), flags-alone mode: ground heights for the flags from standing field armies, then the shared overlay
// (idempotent) so on_present runs.
static void idle_day(bool sites) {
    if(!idle_flags_alone() || !sites)return;
    for(unsigned i=0;i<=500;++i) {
        Obj army=table(0x7E838,i,500);if(!army)continue;
        float position[3],yaw;if(!far_position(army,position,yaw))continue;
        ghost::note_height(word(army,0x2A),position);
    }
    static bool overlay_done;
    if(!overlay_done){overlay_done=true;overlay_install();}
}
static void restore() { COST_SCOPE("ccz_restore");
    if(InterlockedExchange(&installed,0))layout_client::cancel_all();   // unpublished registrations only; installed hooks go through Layout unhook
#ifndef NO_RUNTIME_LOG
    probe::unhook();
#endif
    if(set_table) {
        DWORD prior=0;
        if(VirtualProtect(set_table+kSetTargetsSlot,sizeof(void *),PAGE_READWRITE,&prior)) {
            InterlockedCompareExchangePointer(set_table+kSetTargetsSlot,reinterpret_cast<void *>(original_set),reinterpret_cast<void *>(&set_targets));
            DWORD ignored=0;VirtualProtect(set_table+kSetTargetsSlot,sizeof(void *),prior,&ignored);
        }
    }
}
struct Lifetime {~Lifetime(){restore();}};
static Lifetime lifetime;
}
// 조조전식 막기·회심 (2026-10-31 사용자: 막기는 완전 무효). 설계: docs/CCZ_COMBAT.md.
// 일반 공격 단계 처리 169C50: 6단계 CALL 16FDD0(manager,pair,output) at 169F8B (유일한 호출자), 반환 7이면
// 7단계 1749A0이 output+20/+22(u16) 합을 병력에서 뺀다(합 0 -> 원작이 1로 처리). 6단계 직후 output만 고친다.
// 1749A0 진입 훅(전투 DLL)과 겹치지 않는다. pair+0 = 공격 기록 포인터, pair+8 = 피격 기록, 기록+4 27 = 야전 부대.
namespace ccz_combat {
const unsigned kFix=GA(GA_170D80),kStat=GA(GA_231770);
static volatile LONG block_on,critical_on;
static unsigned roll() {   // per mille; own state, the game's random state is untouched
#ifdef CCZ_NATIVE_FIXTURE
    return ccz_fixture_roll();
#else
    thread_local unsigned s=0;
    if(!s)s=(static_cast<unsigned>(GetTickCount64()*2654435761ull)^(GetCurrentThreadId()<<16))|1u;
    s^=s<<13;s^=s>>17;s^=s<<5;return s%1000;
#endif
}
static Obj field_army(Obj record) {
    if(!record || dword(record,4)!=27 || !word(record,0) || word(record,0)>500)return nullptr;
    Obj army=table(0x7E838,word(record,0),500);
    return army && pointer(army,0)==image+ccz_sprites::kArmyVtable?army:nullptr;
}
static int war(Obj army) {   // effective 무력 (22FCE0 index 1), -1 without a commander
    Obj root=world();unsigned id=word(army,0x12);
    Obj person=root && id<0x1771?pointer(root,0x158+id*8):nullptr;
    return valid(person)?fn<int(__fastcall *)(Obj,unsigned,unsigned)>(kStat)(person,1,0xFFF):-1;
}
static int intel(Obj army) {   // effective 지력 (22FCE0 index 2), -1 without a commander
    Obj root=world();unsigned id=word(army,0x12);
    Obj person=root && id<0x1771?pointer(root,0x158+id*8):nullptr;
    return valid(person)?fn<int(__fastcall *)(Obj,unsigned,unsigned)>(kStat)(person,2,0xFFF):-1;
}
static int clamp(int v,int lo,int hi) {return v<lo?lo:v>hi?hi:v;}
static int __fastcall fix(Obj manager,Obj pair,Obj output) { COST_SCOPE("ccz_combat_fix"); COST_CRUMB("ccz_sprites_manager",manager);
    int next=fn<int(__fastcall *)(Obj,Obj,Obj)>(kFix)(manager,pair,output);
    bool block=InterlockedCompareExchange(&block_on,0,0)!=0,critical=InterlockedCompareExchange(&critical_on,0,0)!=0;
    if(next!=7 || (!block && !critical) || !pair || !output)return next;
    Obj attacker=field_army(pointer(pair,0)),victim=field_army(pair+8);
    COST_CRUMB("ccz_combat_attacker",attacker); COST_CRUMB("ccz_combat_victim",victim);
    if(!valid(attacker) || !valid(victim) || attacker==victim)return next;
    if(!word(attacker,0x16) || !word(victim,0x16))return next;
    auto *damage=reinterpret_cast<unsigned short *>(output+0x20);
    unsigned before=damage[0]+damage[1];if(!before)return next;
    unsigned af=fn<unsigned char(__fastcall *)(Obj)>(GA(GA_22E690))(attacker);
    unsigned vf=fn<unsigned char(__fastcall *)(Obj)>(GA(GA_22E690))(victim);
    if(af>=52 || vf>=52 || af==vf || !fn<bool(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(af,vf)) {
#ifndef NO_RUNTIME_LOG
        static volatile LONG rejected;
        if(InterlockedIncrement(&rejected)<=16)log_event("ccz_combat_not_hostile",af,vf,word(victim,0x12));
#endif
        return next;
    }
    int wa=war(attacker),wv=war(victim);if(wa<0 || wv<0)return next;
    int ia=intel(attacker),iv=intel(victim);if(ia<0 || iv<0)return next;
    // 2026-11-08 user: both states last 8 normal attacks; base probability targets
    // ~28% special attacks in a 10-day two-army fight (measured 36.7% at 5%/5).
    // 2026-11-01 user: a guarding army attacks normally again (the blocks end the state).
    int day=ccz_sprites::game_day_read();bool guard=false,fury=false,unused=false;
    ccz_sprites::buff_read(victim,day,guard,unused);ccz_sprites::buff_read(attacker,day,unused,fury);
#ifndef NO_RUNTIME_LOG
    // 2026-11-08 user: frequency must use the real number of attack/hit events.
    // Diagnostic only: one row per eligible hostile normal attack; flags bit0=guard state, bit1=fury state.
    log_event("ccz_attack",word(attacker,0x12),word(victim,0x12),(guard?1u:0u)|(fury?2u:0u));
#endif
    // 막기% = 2.3 + 0.1 x (피격 지력 - 공격 지력) + 0.05 x (피격 사기 - 공격 사기), 0.3~3%.
    if(block && (guard || static_cast<int>(roll())<clamp(23+(iv-ia)+(static_cast<int>(victim[0x1A])-static_cast<int>(attacker[0x1A]))/2,3,30))) {
        damage[0]=damage[1]=0;ccz_sprites::mark(victim,attacker,ccz_sprites::kMarkBlock);
        ccz_sprites::buff_use(victim,day,true,!guard);if(!guard)log_event("ccz_block",word(attacker,0x12),word(victim,0x12),before);
#ifndef NO_RUNTIME_LOG
        if(!guard)log_event("ccz_block_forces",af,vf,before);
#endif
        return next;
    }
    // 회심% = 2.3 + 0.1 x (공격 무력 - 피격 무력) + 0.05 x (공격 사기 - 피격 사기), 0.3~3%. 피해 1.5배.
    if(critical && (fury || static_cast<int>(roll())<clamp(23+(wa-wv)+(static_cast<int>(attacker[0x1A])-static_cast<int>(victim[0x1A]))/2,3,30))) {
        for(unsigned i=0;i<2;++i){unsigned v=damage[i]*3u/2u;damage[i]=static_cast<unsigned short>(v>0xFFFF?0xFFFF:v);}
        ccz_sprites::mark(victim,attacker,ccz_sprites::kMarkCrit);
        ccz_sprites::buff_use(attacker,day,false,!fury);if(!fury)log_event("ccz_crit",word(attacker,0x12),word(victim,0x12),damage[0]+damage[1]);
#ifndef NO_RUNTIME_LOG
        if(!fury)log_event("ccz_crit_forces",af,vf,damage[0]+damage[1]);
#endif
    }
    return next;
}
// 2026-11-01: SAN14_CCZ.dll. The CALL at 169F8B is guarded and published by SAN14_Convenience
// (native_ccz_bridge.inl), whose relay jumps to fix; this module only keeps the switches.
static void configure(bool block,bool critical) { COST_SCOPE("ccz_combat_configure");
    InterlockedExchange(&block_on,block?1:0);InterlockedExchange(&critical_on,critical?1:0);
    log_event("ccz_combat_installed",block?1:0,critical?1:0);
}
}
