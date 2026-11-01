// 조조전 병종 그림 (2026-10-31 사용자: 2D 모드에 의존하지 않고 우리 모드가 직접 그린다).
// 설계·근거: docs/CCZ_SPRITES.md. 자산: assets/ccz/ccz_units.bin (CCZU v2, tools/ccz_extract_units.py).
// 게임 스레드: 부대 표시 객체 vtable 5번(부대 모형 제출 1C6EC0)에서 위치·방향·상태·진영과 카메라를
// 읽기만 하고 복사한다. 대상 부대는 원래 제출을 건너뛰어 3D 모형을 숨긴다(깃발 슬롯은 그대로).
// 렌더 스레드: Present 직전 복사본과 우리 GPU 자원만으로 빌보드를 그린다. 게임 함수 호출 없음.
#include <algorithm>
#include <cmath>
#include <iterator>
#include <unordered_map>
#include <vector>
#include "ccz_sprites_vs.h"
#include "ccz_sprites_ps.h"
namespace ccz_sprites {
constexpr unsigned kDisplayVtable=0x13A03A8,kSubmitSlot=0x13A03D0,kSubmit=0x1C6EC0,kSubmitSize=0x60;
constexpr unsigned long long kSubmitHash=0x0BC927F835180669ull;   // FNV-1a of 1C6EC0..+60 (exe = running .text)
constexpr unsigned kArmyVtable=0x1342458;                          // Army record, Army+148 = display
constexpr unsigned kCameraRoot=0x27D1208,kCameraVtable=0x16B32C0;
// CCZU v2 entry: move 11x48x48 | attack 12x64x64 | special 5x48x48, 8-bit palette index, 0 = clear.
constexpr unsigned kRawSize=48*48*11+64*64*12+48*48*5;
constexpr unsigned kAtlasW=64,kAtlasH=11*48+12*64+5*48,kAttackRow=11*48,kSpecialRow=kAttackRow+12*64;
constexpr unsigned kSlots=256,kTextures=64,kQuads=kSlots*3;   // unit + up to two state labels
// 2026-11-01: 24 -> 64 cached cells, named generals add personal classes (~86 KB per 8-bit atlas).
// 2026-10-31 user: 막기·회심 were hard to notice -> "막기!" / "회심!" labels over armies in that state.
// Rendered once with GDI (Malgun Gothic) into an R8_UINT mask: row 0 막기, row 1 회심, 1 = glyph.
constexpr unsigned kLabelW=64,kLabelH=20,kLabelCell=0xFFFFFFFEu;
// Palette rows: 0 neutral (original green marks), 1+k = force colour k (Force+49, 62 colours), 63 spare.
// 2026-10-31 user: force colours, plus a contrasting outline so units do not sink into the ground.
constexpr unsigned kRows=64,kForceColours=0x1A26590,kColourCount=62;   // 2ACF00 mode 0 -> 32C220 table (RRGGBBAA)
constexpr ULONGLONG kBlockMs=700,kHurtMs=450,kCritMs=700;
// 2026-11-01 user: attack / hurt motions changed too often -> slower attack frames, one hurt motion per 1.5 s.
constexpr ULONGLONG kAttackFrameMs=260,kHurtCooldownMs=1500;
// 2026-10-31 user: the 5-day state showed on armies far from any enemy -> count-based instead: a triggered
// 막기 blocks 5 normal attacks, a triggered 회심 covers 5 normal attacks; the triggering hit is the first.
// 2026-11-01 user: still shown with no enemy around -> the state also ends once the army goes a whole game day
// without a normal attack exchange (last exchange day + 1 is the last day shown).
constexpr int kCharges=5;
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
static unsigned general_cell(unsigned id) {
    const General *p=std::lower_bound(std::begin(kGenerals),std::end(kGenerals),id,[](const General &g,unsigned v){return g.id<v;});
    return p!=std::end(kGenerals) && p->id==id?p->cell:0u;
}
// 2026-11-01 user: the army flag (commander portrait) hid the big CCZ sprites -> smaller (9 -> 6.5). The ground
// offset toward the camera (local9, 300 units) moved armies off their hex -> removed, and a bit smaller (5.8).
constexpr float kPixel=5.8f,kTowardCamera=250.0f;                  // world units per CCZ pixel; depth margin
constexpr float kMinScreen=1.0f/12,kMaxScale=3.0f;                // 48px frame >= 1/12 screen height when zoomed out
using Submit=void(__fastcall *)(Obj,Obj,Obj);
struct Unit {
    Obj army=nullptr;ULONGLONG seen=0,moved=0;
    float x=0,y=0,z=0,yaw=0,fx=0,fz=0;bool facing=false;
    unsigned state=0,team=0,cell=0,morale=100,troops=0;
    ULONGLONG hurt_at=0,block_at=0,crit_at=0;   // 막기·피격·회심 연출 (ccz_combat marks, troop drops)
    bool guard=false,fury=false;                // 막기·회심 상태 유지 중 (ccz_combat, 5일)
};
struct Vertex {float x,y,z,w,u,v;unsigned team;};
static SRWLOCK lock=SRWLOCK_INIT;
static Unit units[kSlots];
static float camera[80];static ULONGLONG camera_seen;
static volatile LONG installed,hide_model=1,depth_test=1;
static std::vector<unsigned char> asset;
static std::unordered_map<unsigned,std::pair<unsigned,unsigned>> entries; // cell -> (offset, packed size)
static unsigned palette[kRows][256];                                   // index 0 = outline colour of the row
#ifndef NO_RUNTIME_LOG
static volatile LONG captures,replaced,yaw_logged;
#endif
// ---- game thread: read only ----
static bool read_camera(float out[80]) {
    Obj root=pointer(image,kCameraRoot);Obj p=root?pointer(root,0x78):nullptr;Obj cam=p?pointer(p,0x20):nullptr;
    // 2026-10-31 live read: cam+428 holds floats, not C5B1A0; the vtable plus the projection
    // sanity test below are the guard.
    if(!cam || pointer(cam,0)!=image+kCameraVtable)return false;
    float again[80];memcpy(out,cam+0x1340,sizeof(again));memcpy(again,cam+0x1340,sizeof(again));
    if(memcmp(out,again,sizeof(again)))return false;   // torn copy while the camera moves
    return out[44]>0 && out[49]>0 && std::fabs(std::fabs(out[55])-1)<1e-3f && out[59]==0;
}
struct Identity {unsigned team=0,cls=kNative,cell=0;};
static volatile LONG force_colours;   // force colour table matched; otherwise rows 1 player blue / 2 others red
static Identity identify(Obj army,unsigned formation) {   // pure reads, no game calls
    Identity r;if(formation>=21)return r;
    Obj root=world();unsigned leader=word(army,0x12);
    Obj person=root && leader<0x1771?pointer(root,0x158+leader*8):nullptr;
    unsigned tier=0;
    if(person){unsigned lead=person[0xEC];tier=lead>=90?2:lead>=70?1:0;}   // base 통솔 (22FCE0 index 0)
    r.cls=kFormationClass[formation][tier];r.cell=r.cls*3u;
    if(r.cls!=kNative && formation<=7){unsigned g=general_cell(leader);if(g)r.cell=g;}   // soldiers only, not siege/carts
    if(!root || !person)return r;
    Obj context=pointer(root,0x871E8);unsigned player=context?context[0x3A]:0;
    unsigned corps_id=person[0xE0];Obj corps=pointer(root,0xE0D0+(corps_id<0x34?corps_id:0)*8);
    unsigned force=corps?corps[0x10]:0;   // 22CC40/22CA00 read the same fields
    Obj owner=force && force<0x34?pointer(root,0xDF30+force*8):nullptr;   // 22CA00 force table
    if(InterlockedCompareExchange(&force_colours,0,0))r.team=owner && owner[0x49]<kColourCount?1u+owner[0x49]:0u;   // 2AD010 reads Force+49
    else r.team=player && force==player?1u:2u;
    if(owner && formation>=1 && formation<=7) {
        if(leader==word(owner,0x10))r.cls=kRulerClass[tier];
        else if(leader==word(owner,0x52))r.cls=kStrategistClass[tier];   // injury.inl: current strategist
        if(r.cell==kFormationClass[formation][tier]*3u)r.cell=r.cls*3u;   // a general's picture wins
    }
    return r;
}
static bool capture(Obj display,Obj scene) {
    if(dword(display,0x740)!=0 || dword(display,0x748)!=0)return false;   // field mode only, no extra part
    // 2026-10-31 live read: *(image+1B9FB70) held a stack-like value, never main+210==scene, so
    // every army was rejected. The Army back-reference below already identifies field armies.
    (void)scene;
    Obj army=pointer(display,0x40);
    if(!army || pointer(army,0)!=image+kArmyVtable || pointer(army,0x148)!=display)return false;
    unsigned formation=army[0x1B];
    Identity who=identify(army,formation);
    if(who.cls==kNative)return false;   // 충차 / water / 사이 / ships keep native models
    float position[3];memcpy(position,display+0x90,sizeof(position));
    if(!std::isfinite(position[0]) || !std::isfinite(position[1]) || !std::isfinite(position[2]))return false;
    float cam[80];bool fresh=read_camera(cam);
    unsigned team=who.team;ULONGLONG now=GetTickCount64();
    AcquireSRWLockExclusive(&lock);
    if(fresh){memcpy(camera,cam,sizeof(camera));camera_seen=now;}
    Unit *slot=nullptr,*stale=nullptr;
    for(auto &u:units) {
        if(u.army==army){slot=&u;break;}
        if(!stale || u.seen<stale->seen)stale=&u;
    }
    bool fresh_slot=!slot;
    if(!slot){slot=stale;*slot=Unit{};slot->army=army;}
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
    u.x=position[0];u.y=position[1];u.z=position[2];memcpy(&u.yaw,display+0x744,4);
    unsigned troops=word(army,0x16);if(!fresh_slot && troops<u.troops && now-u.hurt_at>=kHurtCooldownMs)u.hurt_at=now;u.troops=troops;   // any damage -> 피격
    buff_read(army,game_day_read(),u.guard,u.fury);
    u.state=dword(display,0x60);u.team=team;u.morale=army[0x1A];u.cell=who.cell;u.seen=now;
    ReleaseSRWLockExclusive(&lock);
#ifndef NO_RUNTIME_LOG
    if(InterlockedIncrement(&captures)==1)log_event("ccz_sprites_first_capture",team,fresh?1:0,army[0x1B]);
#endif
    return InterlockedCompareExchange(&hide_model,0,0)!=0;
}
static void __fastcall submit(Obj display,Obj scene,Obj list) { COST_SCOPE("ccz_sprites_submit");
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
    fn<Submit>(kSubmit)(display,scene,list);
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
    // Force colours: read the game's static table once (pure read); guard on its fixed tail.
    unsigned colours[kColourCount];memcpy(colours,image+kForceColours,sizeof(colours));
    bool table_ok=colours[0]==0x2828E8FFu && colours[60]==0xFFFFFFFFu && colours[61]==0;
    InterlockedExchange(&force_colours,table_ok?1:0);
    for(unsigned row=0;row<kRows;++row) {
        unsigned rgb=row>=1 && row<=kColourCount?colours[row-1]>>8:0;
        float fr=((rgb>>16)&255)/255.0f,fg=((rgb>>8)&255)/255.0f,fb=(rgb&255)/255.0f;
        bool tint=table_ok && row>=1 && row<=kColourCount;
        if(tint) {   // 2026-11-01 user (option C): cool force colours -> warm contrast (blue -> red); warm ones stay (원소 gold)
            float h,s,v;rgb_hsv(fr,fg,fb,h,s,v);
            // 2026-11-01 user: 조조 blue came out orange -> blue/purple map to red, green/cyan to orange.
            if(h>0.2f && h<0.92f){h=h<=0.62f?0.12f*(0.62f-h)/0.42f:1-0.06f*(h-0.62f)/0.30f;s=s<0.8f?0.8f:s;v=v<0.8f?0.8f:v;hsv_rgb(h,s,v,fr,fg,fb);}
        }
        for(unsigned i=0;i<256;++i) {
            const unsigned char *c=asset.data()+16+i*3;unsigned rgba=0xFF000000u|c[0]|c[1]<<8|c[2]<<16;
            // 2026-11-01 user: horses and faces were force-coloured too. The CCZ palette draws nearly everything in
            // greens; only the saturated pure-green ramp is the team colour (clothes). The other green/teal marks
            // become natural materials: same brightness, brown hue, less saturation (brown horses, skin, leather).
            float ch_,cs,cv;rgb_hsv(c[0]/255.0f,c[1]/255.0f,c[2]/255.0f,ch_,cs,cv);
            if(asset[784+i] && !(cs>0.85f && ch_>=0.30f && ch_<=0.43f)) {
                float r,g,b;hsv_rgb(0.07f,cs*0.55f>1?1:cs*0.55f,cv,r,g,b);
                rgba=0xFF000000u|static_cast<unsigned>(r*255+.5f)|static_cast<unsigned>(g*255+.5f)<<8|static_cast<unsigned>(b*255+.5f)<<16;
            }
            else if(asset[784+i]) {   // team colour (saturated green ramp)
                if(tint) {
                    unsigned top=c[0]>c[1]?c[0]:c[1];top=top>c[2]?top:c[2];float v=top/255.0f/0.8f;v=v<0.25f?0.25f:v>1.25f?1.25f:v;
                    auto ch=[&](float x){x*=v;return static_cast<unsigned>((x>1?1:x)*255+.5f);};
                    rgba=0xFF000000u|ch(fr)|ch(fg)<<8|ch(fb)<<16;
                }
                else if(!table_ok && row==1)hsv_shift(rgba,0.27f);   // fallback: player blue
                else if(!table_ok && row==2)hsv_shift(rgba,-0.33f);  // fallback: others red
            }
            palette[row][i]=rgba;
        }
        // Outline contrasts with the row's colour (dark outline on light colours, light on dark).
        float lum=tint?0.299f*fr+0.587f*fg+0.114f*fb:0.5f;
        palette[row][0]=lum>0.45f?0xFF141414u:0xFFF0F0F0u;
    }
    auto has=[](unsigned char c){return c==kNative || entries.count(c*3u)!=0;};
    for(auto &row:kFormationClass)for(unsigned char c:row)if(!has(c))return false;
    for(unsigned t=0;t<3;++t)if(!has(kRulerClass[t]) || !has(kStrategistClass[t]))return false;
    for(const auto &g:kGenerals)if(!entries.count(g.cell))return false;
    return true;
}
static bool decode(unsigned cell,std::vector<unsigned char> &atlas) {
    auto it=entries.find(cell);if(it==entries.end())return false;
    std::vector<unsigned char> raw;raw.reserve(kRawSize);
    const unsigned char *p=asset.data()+it->second.first,*end=p+it->second.second;
    while(p<end && raw.size()<=kRawSize) {
        unsigned b=*p++;
        if(b<0x80){if(static_cast<size_t>(end-p)<b+1)return false;raw.insert(raw.end(),p,p+b+1);p+=b+1;}
        else raw.insert(raw.end(),b-0x7F,0);
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
static ID3D11Buffer *vertices;static ID3D11ShaderResourceView *palette_view,*label_view;
static ID3D11DepthStencilState *depth_on[2],*depth_off;static ID3D11RasterizerState *raster;
struct CellTexture {unsigned cell=~0u;ID3D11ShaderResourceView *view=nullptr;ULONGLONG used=0;};
static CellTexture textures[kTextures];
static bool resources_failed;
static SRWLOCK dsv_lock=SRWLOCK_INIT;
static ID3D11DepthStencilView *scene_dsv;static ID3D11DepthStencilView *rejected[8];static unsigned rejected_next;
static volatile LONG want_width,want_height,drawing;
using SetTargets=void(STDMETHODCALLTYPE *)(ID3D11DeviceContext *,UINT,ID3D11RenderTargetView *const *,ID3D11DepthStencilView *);
constexpr unsigned kSetTargetsSlot=33;   // ID3D11DeviceContext::OMSetRenderTargets
static SetTargets original_set;static void **set_table;
template<class T> static void drop(T *&p){if(p){p->Release();p=nullptr;}}
static void release_resources() {
    drop(vs);drop(ps);drop(layout);drop(vertices);drop(palette_view);drop(label_view);drop(depth_on[0]);drop(depth_on[1]);drop(depth_off);drop(raster);
    for(auto &t:textures){drop(t.view);t=CellTexture{};}
    AcquireSRWLockExclusive(&dsv_lock);drop(scene_dsv);memset(rejected,0,sizeof(rejected));ReleaseSRWLockExclusive(&dsv_lock);
    if(device){device->Release();device=nullptr;}
}
struct Stats {   // read-only diagnostics, inspected from outside the process (frame-drop question)
    LONGLONG qpc_per_second,last_present,frames,frame_qpc,frame_max,cost_qpc,cost_max;
    LONG set_calls,slow_notes,scene_draws,present_draws,pass_ends,units_drawn,target_format,seq_count,seq_armed;
    struct Bind {unsigned width,height,format,count,dsv;} seq[128];
};
static Stats stats;
// The scene depth buffer stays intact until the next frame's clear; remember the last
// back-buffer-sized depth view the game bound with colour targets.
static void note_dsv(ID3D11DepthStencilView *dsv) {
    if(dsv==scene_dsv)return;
    AcquireSRWLockExclusive(&dsv_lock);
    bool known=false;for(auto r:rejected)if(r==dsv)known=true;
    if(!known) {
        ID3D11Resource *resource=nullptr;dsv->GetResource(&resource);ID3D11Texture2D *texture=nullptr;
        if(resource){resource->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void **>(&texture));resource->Release();}
        D3D11_TEXTURE2D_DESC d{};if(texture){texture->GetDesc(&d);texture->Release();}
        if(texture && d.Width==static_cast<UINT>(want_width) && d.Height==static_cast<UINT>(want_height) && d.SampleDesc.Count==1) {
            dsv->AddRef();drop(scene_dsv);scene_dsv=dsv;
#ifndef NO_RUNTIME_LOG
            log_event("ccz_sprites_scene_depth",d.Width,d.Height,d.Format);
#endif
        } else {rejected[rejected_next++%8]=dsv;++stats.slow_notes;}
    }
    ReleaseSRWLockExclusive(&dsv_lock);
}
// 2026-10-31 live: drawing at Present covered the HUD portraits and stayed above full-screen windows.
// local7 read (stats.seq): the game draws the 3D scene straight into the sRGB back buffer with the scene
// depth bound until Present, then post-processing on half-size targets (640x360 at 1280x720) that rewrites
// the back buffer (local8 drew 28 units there and nothing survived), then the UI on its own layer
// (1920x1080 targets with their own depth), composited last. Draw into the back buffer just before the
// first colour bind with a non-scene depth view; Present remains the fallback when none is seen.
static ID3D11RenderTargetView *main_rtv;static bool drawn_this_frame,post_seen;
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
static void draw_scene(ID3D11DeviceContext *c,ID3D11RenderTargetView *rtv,ID3D11DepthStencilView *dsv);
static void sample_bind(UINT n,ID3D11RenderTargetView *const *rtv,ID3D11DepthStencilView *dsv) {
    if(!stats.seq_armed || stats.seq_count>=128)return;
    Stats::Bind &b=stats.seq[stats.seq_count++];b={0,0,0,n,dsv?(dsv==scene_dsv?1u:2u):0u};
    if(n && rtv && rtv[0]) {
        ID3D11Resource *r=nullptr;rtv[0]->GetResource(&r);ID3D11Texture2D *t=nullptr;
        if(r){r->QueryInterface(__uuidof(ID3D11Texture2D),reinterpret_cast<void **>(&t));r->Release();}
        if(t){D3D11_TEXTURE2D_DESC d{};t->GetDesc(&d);b.width=d.Width;b.height=d.Height;b.format=d.Format|(t==back_texture?0x10000u:0u);t->Release();}
    }
}
static void STDMETHODCALLTYPE set_targets(ID3D11DeviceContext *c,UINT n,ID3D11RenderTargetView *const *rtv,ID3D11DepthStencilView *dsv) {
    if(InterlockedCompareExchange(&drawing,0,0)){original_set(c,n,rtv,dsv);return;}
    LARGE_INTEGER t0;QueryPerformanceCounter(&t0);++stats.set_calls;
    if(dsv && n && n<=D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT && want_width)note_dsv(dsv);
    sample_bind(n,rtv,dsv);
    if(!drawn_this_frame && n && rtv && rtv[0] && want_width) {
        unsigned w=0,h=0;rtv_size(rtv[0],w,h);
        if(!main_rtv && dsv && dsv==scene_dsv && w==static_cast<unsigned>(want_width) && h==static_cast<unsigned>(want_height)){main_rtv=rtv[0];main_rtv->AddRef();}
        else if(main_rtv && w*2==static_cast<unsigned>(want_width) && h*2==static_cast<unsigned>(want_height))post_seen=true;
        // 2026-10-31 live: battle effects bind their own depth in the middle of the scene; drawing there was
        // covered by the rest of the scene. Only the first other-depth bind after post-processing is the UI layer.
        else if(main_rtv && post_seen && dsv && dsv!=scene_dsv) {
            ++stats.pass_ends;draw_scene(c,main_rtv,scene_dsv);drawn_this_frame=true;
        }
    }
    LARGE_INTEGER t1;QueryPerformanceCounter(&t1);stats.cost_qpc+=t1.QuadPart-t0.QuadPart;
    original_set(c,n,rtv,dsv);
}
static void hook_context(ID3D11DeviceContext *c) {
    if(set_table || !InterlockedCompareExchange(&depth_test,0,0))return;
    void **table=*reinterpret_cast<void ***>(c);DWORD prior=0;
    if(!VirtualProtect(table+kSetTargetsSlot,sizeof(void *),PAGE_READWRITE,&prior))return;
    original_set=reinterpret_cast<SetTargets>(table[kSetTargetsSlot]);   // chains over gpu_profile if it came first
    InterlockedExchangePointer(table+kSetTargetsSlot,reinterpret_cast<void *>(&set_targets));
    DWORD ignored=0;VirtualProtect(table+kSetTargetsSlot,sizeof(void *),prior,&ignored);set_table=table;
#ifndef NO_RUNTIME_LOG
    log_event("ccz_sprites_context_hook",1,0,0);
#endif
}
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
static bool create_resources(ID3D11Device *d) {
    if(device==d)return !resources_failed;
    release_resources();device=d;device->AddRef();resources_failed=true;
    D3D11_INPUT_ELEMENT_DESC elements[]={{"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",1,DXGI_FORMAT_R32_UINT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0}};
    if(FAILED(d->CreateVertexShader(ccz_sprites_vs,sizeof(ccz_sprites_vs),nullptr,&vs)) || FAILED(d->CreatePixelShader(ccz_sprites_ps,sizeof(ccz_sprites_ps),nullptr,&ps)) ||
       FAILED(d->CreateInputLayout(elements,3,ccz_sprites_vs,sizeof(ccz_sprites_vs),&layout)))return false;
    D3D11_BUFFER_DESC b{};b.ByteWidth=sizeof(Vertex)*6*kQuads;b.Usage=D3D11_USAGE_DYNAMIC;b.BindFlags=D3D11_BIND_VERTEX_BUFFER;b.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    if(FAILED(d->CreateBuffer(&b,nullptr,&vertices)))return false;
    D3D11_TEXTURE2D_DESC t{};t.Width=256;t.Height=kRows;t.MipLevels=t.ArraySize=1;t.Format=DXGI_FORMAT_R8G8B8A8_UNORM;t.SampleDesc.Count=1;t.Usage=D3D11_USAGE_IMMUTABLE;t.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init{palette,256*4,0};ID3D11Texture2D *texture=nullptr;
    if(FAILED(d->CreateTexture2D(&t,&init,&texture)))return false;
    HRESULT made=d->CreateShaderResourceView(texture,nullptr,&palette_view);texture->Release();if(FAILED(made))return false;
    if(!create_labels(d))log_event("ccz_sprites_label_failed");   // labels optional
    D3D11_DEPTH_STENCIL_DESC ds{};ds.DepthEnable=TRUE;ds.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;
    ds.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;if(FAILED(d->CreateDepthStencilState(&ds,&depth_on[0])))return false;
    ds.DepthFunc=D3D11_COMPARISON_GREATER_EQUAL;if(FAILED(d->CreateDepthStencilState(&ds,&depth_on[1])))return false;   // reversed Z
    ds.DepthEnable=FALSE;if(FAILED(d->CreateDepthStencilState(&ds,&depth_off)))return false;
    D3D11_RASTERIZER_DESC rs{};rs.FillMode=D3D11_FILL_SOLID;rs.CullMode=D3D11_CULL_NONE;rs.DepthClipEnable=TRUE;
    if(FAILED(d->CreateRasterizerState(&rs,&raster)))return false;
    resources_failed=false;return true;
}
static ID3D11ShaderResourceView *cell_view(unsigned cell) {
    CellTexture *slot=nullptr;ULONGLONG now=GetTickCount64();
    for(auto &t:textures){if(t.cell==cell && t.view){t.used=now;return t.view;}if(!slot || t.used<slot->used)slot=&t;}
    std::vector<unsigned char> atlas;if(!decode(cell,atlas))return nullptr;
    D3D11_TEXTURE2D_DESC t{};t.Width=kAtlasW;t.Height=kAtlasH;t.MipLevels=t.ArraySize=1;t.Format=DXGI_FORMAT_R8_UINT;t.SampleDesc.Count=1;t.Usage=D3D11_USAGE_IMMUTABLE;t.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init{atlas.data(),kAtlasW,0};ID3D11Texture2D *texture=nullptr;
    if(FAILED(device->CreateTexture2D(&t,&init,&texture)))return nullptr;
    drop(slot->view);slot->cell=~0u;
    if(SUCCEEDED(device->CreateShaderResourceView(texture,nullptr,&slot->view))){slot->cell=cell;slot->used=now;}
    texture->Release();return slot->view;
}
static void clip(const float *m,float x,float y,float z,Vertex &v) {   // row vector * ViewProj
    v.x=x*m[0]+y*m[4]+z*m[8]+m[12];v.y=x*m[1]+y*m[5]+z*m[9]+m[13];v.z=x*m[2]+y*m[6]+z*m[10]+m[14];v.w=x*m[3]+y*m[7]+z*m[11]+m[15];
}
// Frame choice. CCZ: move 0-1 front walk, 2-3 back, 4-5 side (faces left), 6-8 stand, 9-10 weak;
// attack 0-3 front, 4-7 back, 8-11 side; special 0-2 block front/back/side, 3 hurt, 4 victory.
// 막기 > 피격 > 회심(공격 + 섬광) > 공격 > 이동 > 약함 > 대기.
struct Frame {unsigned row,size;bool mirror,flash;};
// 막기 상태(5일): 맞을 때·대기 중 방어 자세. 회심 상태(5일): 공격 중 섬광, 그 밖에도 주기적으로 번쩍인다.
static Frame frame_for(const Unit &u,int view,bool mirror,ULONGLONG now) {
    Frame guard_pose{kSpecialRow+static_cast<unsigned>(view)*48,48,mirror,false};
    if(u.block_at && now-u.block_at<kBlockMs)return guard_pose;
    // 2026-10-31 user: while guarding, no hurt or attack motion -- the guard pose holds (walking still shows).
    if(u.guard && now-u.moved>=400)return guard_pose;
    if(u.hurt_at && now-u.hurt_at<kHurtMs)return {kSpecialRow+3*48,48,mirror,false};
    if(u.crit_at && now-u.crit_at<kCritMs)
        return {kAttackRow+static_cast<unsigned>(view*4+((now-u.crit_at)/kAttackFrameMs)%4)*64,64,mirror,((now-u.crit_at)/220)%2==0};
    bool blink=u.fury && (now/350)%4==0;   // 2026-10-31 user: slower 회심 blinking
    if(u.state==2)return {kAttackRow+static_cast<unsigned>(view*4+(now/kAttackFrameMs)%4)*64,64,mirror,u.fury && (now/300)%2==0};
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
static bool needs_present() {
    if(!InterlockedCompareExchange(&installed,0,0))return false;
    AcquireSRWLockShared(&lock);ULONGLONG seen=camera_seen;ReleaseSRWLockShared(&lock);
    return seen && GetTickCount64()-seen<500;
}
static unsigned build(ID3D11DeviceContext *c,float height,bool linear,unsigned cells[kQuads],float cam[80]) {
    Unit list[kSlots];unsigned count=0;ULONGLONG now=GetTickCount64();
    AcquireSRWLockShared(&lock);memcpy(cam,camera,sizeof(camera));   // cam is a pointer parameter here
    for(const auto &u:units)if(u.army && now-u.seen<300)list[count++]=u;
    ReleaseSRWLockShared(&lock);
    if(!count)return 0;
    std::sort(list,list+count,[](const Unit &a,const Unit &b){return a.cell<b.cell;});
    const float *vp=cam+60;
    // Camera axes from eye/target; screen direction checked by projection (no handedness assumption).
    float fx=cam[4]-cam[0],fy=cam[5]-cam[1],fz=cam[6]-cam[2],fl=std::sqrt(fx*fx+fy*fy+fz*fz);if(fl<1e-3f)return 0;
    fx/=fl;fy/=fl;fz/=fl;
    float rx=-fz,rz=fx,rl=std::sqrt(rx*rx+rz*rz);if(rl<1e-3f)return 0;rx/=rl;rz/=rl;   // horizontal right
    float ux=-rz*fy,uy=rz*fx-rx*fz,uz=rx*fy;   // up = right x forward, right = (rx,0,rz)
    {Vertex a,b,e;clip(vp,cam[4],cam[5],cam[6],a);clip(vp,cam[4]+rx*100,cam[5],cam[6]+rz*100,b);clip(vp,cam[4]+ux*100,cam[5]+uy*100,cam[6]+uz*100,e);
     if(a.w<=0)return 0;if(b.x/b.w<a.x/a.w){rx=-rx;rz=-rz;}if(e.y/e.w<a.y/a.w){ux=-ux;uy=-uy;uz=-uz;}}
    float hx=fx,hz=fz,hl=std::sqrt(hx*hx+hz*hz);if(hl>1e-3f){hx/=hl;hz/=hl;}
    D3D11_MAPPED_SUBRESOURCE mapped{};if(FAILED(c->Map(vertices,0,D3D11_MAP_WRITE_DISCARD,0,&mapped)))return 0;
    auto out=static_cast<Vertex *>(mapped.pData);unsigned written=0;
    Vertex labels[kSlots*2][4];unsigned label_count=0;
    for(unsigned i=0;i<count;++i) {
        const Unit &u=list[i];
        float ffx=u.facing?u.fx:std::sin(u.yaw),ffz=u.facing?u.fz:std::cos(u.yaw);
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
        Vertex *o=out+written*6;o[0]=q[0];o[1]=q[1];o[2]=q[2];o[3]=q[0];o[4]=q[2];o[5]=q[3];cells[written++]=u.cell;
        // State labels stacked over the head (48px body top), 1 texel = 1 CCZ pixel; a short hop when triggered.
        float lift=(48+3)*kPixel*scale;
        for(unsigned kind=0;kind<2;++kind) {
            if(!(kind?u.fury:u.guard) || !label_view)continue;
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
        const Vertex *l=labels[i];Vertex *o=out+written*6;o[0]=l[0];o[1]=l[1];o[2]=l[2];o[3]=l[0];o[4]=l[2];o[5]=l[3];cells[written++]=kLabelCell;
    }
    c->Unmap(vertices,0);return written;
}
static bool linear_target(ID3D11RenderTargetView *rtv) {   // float / sRGB targets take linear colour
    D3D11_RENDER_TARGET_VIEW_DESC d{};rtv->GetDesc(&d);
    switch(d.Format) {
        case DXGI_FORMAT_R16G16B16A16_FLOAT:case DXGI_FORMAT_R11G11B10_FLOAT:case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:return true;
        default:return false;
    }
}
static void draw(ID3D11DeviceContext *c,ID3D11RenderTargetView *rtv,ID3D11DepthStencilView *dsv,float width,float height) {
    LARGE_INTEGER t0;QueryPerformanceCounter(&t0);
    unsigned cells[kQuads];float cam[80];
    unsigned written=build(c,height,linear_target(rtv),cells,cam);
    if(written) {
        // Reversed Z when the near plane maps above the far plane.
        const float *P=cam+44;float n=cam[76]>0?cam[76]:1,fr=cam[77]>n?cam[77]:n*1000;
        bool reversed=((-n*P[10]+P[14])/n)>((-fr*P[10]+P[14])/fr);
        InterlockedExchange(&drawing,1);
        {
            SavedState saved(c);
            D3D11_VIEWPORT viewport{0,0,width,height,0,1};
            c->OMSetRenderTargets(1,&rtv,dsv);c->OMSetBlendState(nullptr,nullptr,0xFFFFFFFFu);c->OMSetDepthStencilState(dsv?depth_on[reversed?1:0]:depth_off,0);
            c->RSSetState(raster);c->RSSetViewports(1,&viewport);c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);c->IASetInputLayout(layout);
            UINT stride=sizeof(Vertex),offset=0;c->IASetVertexBuffers(0,1,&vertices,&stride,&offset);
            c->VSSetShader(vs,nullptr,0);c->PSSetShader(ps,nullptr,0);c->GSSetShader(nullptr,nullptr,0);c->HSSetShader(nullptr,nullptr,0);c->DSSetShader(nullptr,nullptr,0);
            for(unsigned i=0;i<written;) {
                unsigned j=i;while(j<written && cells[j]==cells[i])++j;
                ID3D11ShaderResourceView *views[2]={cells[i]==kLabelCell?label_view:cell_view(cells[i]),palette_view};
                if(views[0]){c->PSSetShaderResources(0,2,views);c->Draw((j-i)*6,i*6);}
                i=j;
            }
        }
        InterlockedExchange(&drawing,0);
    }
    stats.units_drawn=static_cast<LONG>(written);
    LARGE_INTEGER t1;QueryPerformanceCounter(&t1);LONGLONG spent=t1.QuadPart-t0.QuadPart;
    stats.cost_qpc+=spent;if(spent>stats.cost_max)stats.cost_max=spent;
}
static void draw_scene(ID3D11DeviceContext *c,ID3D11RenderTargetView *rtv,ID3D11DepthStencilView *dsv) {
    if(!needs_present() || !device || resources_failed)return;
    ++stats.scene_draws;
    {D3D11_RENDER_TARGET_VIEW_DESC d{};rtv->GetDesc(&d);stats.target_format=d.Format;}
    draw(c,rtv,dsv,static_cast<float>(want_width),static_cast<float>(want_height));
}
static void on_present(ID3D11Device *d,ID3D11DeviceContext *c,ID3D11Texture2D *back,const D3D11_TEXTURE2D_DESC &desc) { COST_SCOPE("ccz_sprites_present");
    LARGE_INTEGER now_qpc;QueryPerformanceCounter(&now_qpc);
    if(!stats.qpc_per_second){LARGE_INTEGER f;QueryPerformanceFrequency(&f);stats.qpc_per_second=f.QuadPart;}
    if(stats.last_present){LONGLONG gap=now_qpc.QuadPart-stats.last_present;stats.frame_qpc+=gap;if(gap>stats.frame_max)stats.frame_max=gap;}
    stats.last_present=now_qpc.QuadPart;++stats.frames;
    stats.seq_armed=0;if(stats.frames%600==1){stats.seq_count=0;stats.seq_armed=1;}   // sample one frame's target sequence
    back_texture=back;
    InterlockedExchange(&want_width,static_cast<LONG>(desc.Width));InterlockedExchange(&want_height,static_cast<LONG>(desc.Height));
    bool scene_drawn=drawn_this_frame;drawn_this_frame=false;post_seen=false;drop(main_rtv);
    if(stats.frames%600==0)memset(sizes,0,sizeof(sizes));   // view pointers can be reused after release
    if(!needs_present() || !create_resources(d))return;
    hook_context(c);
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
    draw(c,rtv,dsv,static_cast<float>(desc.Width),static_cast<float>(desc.Height));
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
    if(pointer(image,kSubmitSlot)!=image+kSubmit)return false;
    unsigned long long hash=0xCBF29CE484222325ull;
    for(unsigned i=0;i<kSubmitSize;++i)hash=(hash^image[kSubmit+i])*0x100000001B3ull;
    return hash==kSubmitHash;
}
static bool install(bool hide,bool depth) {
    if(InterlockedCompareExchange(&installed,0,0) || !guards_match())return false;
    InterlockedExchange(&hide_model,hide?1:0);InterlockedExchange(&depth_test,depth?1:0);
    if(!load_asset()){log_event("ccz_sprites_asset_missing");return false;}
    if(!patch_pointer(kSubmitSlot,kSubmit,reinterpret_cast<void *>(&submit)))return false;
    InterlockedExchange(&installed,1);log_event("ccz_sprites_installed",static_cast<unsigned>(entries.size()),hide?1:0,depth?1:0);return true;
}
static void restore() {
    if(InterlockedExchange(&installed,0))restore_pointer(kSubmitSlot,kSubmit,reinterpret_cast<void *>(&submit));
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
constexpr unsigned kFix=0x16FDD0,kStat=0x22FCE0;
static volatile LONG block_on,critical_on;
static unsigned roll() {   // per mille; own state, the game's random state is untouched
    thread_local unsigned s=0;
    if(!s)s=(static_cast<unsigned>(GetTickCount64()*2654435761ull)^(GetCurrentThreadId()<<16))|1u;
    s^=s<<13;s^=s>>17;s^=s<<5;return s%1000;
}
static Obj field_army(Obj record) {return record && dword(record,4)==27?table(0x7E838,word(record,0),500):nullptr;}
static int war(Obj army) {   // effective 무력 (22FCE0 index 1), -1 without a commander
    Obj root=world();unsigned id=word(army,0x12);
    Obj person=root && id<0x1771?pointer(root,0x158+id*8):nullptr;
    return valid(person)?fn<int(__fastcall *)(Obj,unsigned,unsigned)>(kStat)(person,1,0xFFF):-1;
}
static int clamp(int v,int lo,int hi) {return v<lo?lo:v>hi?hi:v;}
static int __fastcall fix(Obj manager,Obj pair,Obj output) { COST_SCOPE("ccz_combat_fix");
    int next=fn<int(__fastcall *)(Obj,Obj,Obj)>(kFix)(manager,pair,output);
    bool block=InterlockedCompareExchange(&block_on,0,0)!=0,critical=InterlockedCompareExchange(&critical_on,0,0)!=0;
    if(next!=7 || (!block && !critical) || !pair || !output)return next;
    Obj attacker=field_army(pointer(pair,0)),victim=field_army(pair+8);
    if(!valid(attacker) || !valid(victim) || attacker==victim)return next;
    int wa=war(attacker),wv=war(victim);if(wa<0 || wv<0)return next;
    auto *damage=reinterpret_cast<unsigned short *>(output+0x20);
    unsigned before=damage[0]+damage[1];if(!before)return next;
    // 2026-10-31 user: once triggered, the next normal attacks on a guarding army are blocked and those by a
    // furious army are critical, 5 times each (ccz_sprites::kCharges).
    // 2026-11-01 user: a guarding army attacks normally again (the five blocks end the state anyway).
    int day=ccz_sprites::game_day_read();bool guard=false,fury=false,unused=false;
    ccz_sprites::buff_read(victim,day,guard,unused);ccz_sprites::buff_read(attacker,day,unused,fury);
    // 막기% = 5 + 0.4 x (피격 무력 - 공격 무력), 1~10% (2026-11-01 user: 15 -> 10). 성공하면 피해 0 (원작이 1명으로 처리).
    if(block && (guard || static_cast<int>(roll())<clamp(50+4*(wv-wa),10,100))) {
        damage[0]=damage[1]=0;ccz_sprites::mark(victim,attacker,ccz_sprites::kMarkBlock);
        ccz_sprites::buff_use(victim,day,true,!guard);if(!guard)log_event("ccz_block",word(attacker,0x12),word(victim,0x12),before);
        return next;
    }
    // 회심% = 5 + 0.3 x 무력 차 + 0.2 x 사기 차, 1~10% (2026-11-01 user: 15 -> 10). 피해 1.5배.
    if(critical && (fury || static_cast<int>(roll())<clamp(50+3*(wa-wv)+2*(static_cast<int>(attacker[0x1A])-static_cast<int>(victim[0x1A])),10,100))) {
        for(unsigned i=0;i<2;++i){unsigned v=damage[i]*3u/2u;damage[i]=static_cast<unsigned short>(v>0xFFFF?0xFFFF:v);}
        ccz_sprites::mark(victim,attacker,ccz_sprites::kMarkCrit);
        ccz_sprites::buff_use(attacker,day,false,!fury);if(!fury)log_event("ccz_crit",word(attacker,0x12),word(victim,0x12),damage[0]+damage[1]);
    }
    return next;
}
// 2026-11-01: SAN14_CCZ.dll. The CALL at 169F8B is guarded and published by SAN14_Convenience
// (native_ccz_bridge.inl), whose relay jumps to fix; this module only keeps the switches.
static void configure(bool block,bool critical) {
    InterlockedExchange(&block_on,block?1:0);InterlockedExchange(&critical_on,critical?1:0);
    log_event("ccz_combat_installed",block?1:0,critical?1:0);
}
}
