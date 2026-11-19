// [game_flow] ambush (매복기동, user 2026-11-11, docs/AMBUSH.md). Convenience DLL: state, tickets, sidecar and UI.
// Effects (AI blindness through condition 29, half mobility) live in SAN14_CombatAI.dll (native_ambush_combat.inl);
// this module writes the shared table that DLL exports (SAN14CombatAmbush). Without that export the feature is off.
// Flow:
//  1. Once per month, at the first 10-day turn (상순), a chance of 5 % + 10 % per player region lost since the last
//     monthly roll (lost = drops of the player region count between consecutive turns). A hit = one pending offer.
//  2. The advisor head shows a blue count (pending offers + ready tickets) at its top-left. The first head click of a
//     turn with a pending offer opens the proposal instead of the report: 수락 = 1000 gold from the ruler's base,
//     one ready ticket; 거절 = the offer is gone. No answer in that turn = declined (local82). At most two ready tickets.
//  3. Sortie screen (CStrategyUnitMakeState): while ready tickets exist at screen entry, a bottom-right
//     [군사제안수행] toggle marks the current officer (at most as many officers as tickets). An officer who really
//     departs (CALL 7EE1C4 -> 1EDC30, tactic_priority's hook via sortie_screen_bridge.h) spends one ticket: his army
//     is hidden for 60 days.
// Player only: tickets, offers and marks belong to the player force; computer forces never get any.
#include "ambush_shared.h"
#include "advisor_ambush_bridge.h"
#include "sortie_screen_bridge.h"
namespace ambush {
// local82 (user 2026-11-12 "6개까지 쌓이더라 ... 제안한 턴에 수락/거절 안누르면 거절로 처리 ... 티켓 최대값도 두개"): at most
// kMaxTickets ready tickets; a proposal lives for the turn it was made in and counts as declined when the turn changes.
static const unsigned kMaxTickets=2;
static const int kCost=1000,kDays=kAmbushMaxDays;   // 180 days at most, 60 from the first ZOC day (ambush_shared.h)
static AmbushTable *shared;               // combat module table (nullptr = feature unusable)
static unsigned tickets,offers,lost;       // ready tickets, pending offers, regions lost since the last roll
static int prev_count=-1,last_roll_month=-1,last_turn=-1,offer_turn=-1;
// [game_flow] destruction_ticket (user 2026-11-13 "플레이어 1차 인접선에는 계략 건물파괴를 무조건 성공하는 티켓을 군사제안에 넣자"):
// a second proposal kind. Once a month at 상순 (local105, together with the ambush roll, one proposal at most), when hostile facilities stand in regions next to
// the player's land, a chance of 20 % + 5 % per such facility (at most 60 %) makes the proposal; 수락 = 1000 gold, one
// destruction ticket (at most kMaxTickets). A player officer's building destruction against a region next to the
// player's land that the original roll fails then succeeds and spends one ticket (building_destruction::ticket_roll);
// a roll that succeeds on its own spends nothing.
static unsigned destroy_tickets,offer_kind;   // offer_kind of the pending proposal: 0 ambush, 1 destruction
static unsigned long long rng_state;
#ifndef NO_RUNTIME_LOG
static unsigned log_lines;
static void log_capped(const char *kind,unsigned a=0,unsigned b=0,unsigned c=0) {if(log_lines<400){++log_lines;log_event(kind,a,b,c);}}
#else
static void log_capped(const char *,unsigned=0,unsigned=0,unsigned=0) {}
#endif
static bool usable() {return mod_features.ambush && shared;}
// local64 (user 2026-11-12): the blue head count is pending proposals only; ready tickets get their own ticket mark.
static unsigned badge() {return usable()?offers:0;}
static bool destroy_usable() {return usable() && mod_features.destruction_ticket;}
static unsigned ticket_count() {return usable()?tickets+(destroy_usable()?destroy_tickets:0u):0;}
static unsigned current_offer_kind() {return usable() && offers?offer_kind:0u;}
// Officers who departed hidden in the current 10-day turn. A sortie the player cancels before 진행 removes the army:
// the ticket comes back (user 2026-11-12 "출진취소하면 명령서처럼 되돌려 받았으면"). The list is dropped once the turn moves.
static std::vector<unsigned> fresh;
static void table_set(unsigned leader,int until);
static void refund_cancelled() { COST_SCOPE("ambush:refund_cancelled");
    static ULONGLONG checked;
    if(fresh.empty())return;
    const ULONGLONG now=GetTickCount64();if(now-checked<300)return;
    checked=now;
    for(size_t n=fresh.size();n--;) {
        const unsigned person=fresh[n];bool alive=false;
        for(unsigned i=1;i<=500 && !alive;++i) {Obj army=table(0x7E838,i,500);alive=valid(army) && word(army,0x12)==person;}
        if(alive)continue;
        fresh.erase(fresh.begin()+static_cast<ptrdiff_t>(n));
        table_set(person,0);if(tickets+destroy_tickets<kMaxTickets)++tickets;
        log_capped("ambush_refund",person,tickets,0);
        briefing_overlay::toast(L"출진을 거두어 매복 준비를 되돌려 받았습니다.");
    }
}
// Calendar of the running world (same fields as 32BA50: year short, month, day at context+0x34).
static bool calendar(int &month,int &xun) {
    Obj root=world();Obj context=root?pointer(root,0x871E8):nullptr;if(!context)return false;
    Obj d=context+0x34;const int day=static_cast<signed char>(d[3]);
    month=static_cast<int>(*reinterpret_cast<short *>(d))*12+static_cast<signed char>(d[2]);
    xun=day<=10?0:day<=20?1:2;
    return day>=1 && day<=31;
}
// Regions (world+0x6CAF0, 1..500) whose center tile (+0x3A, tile table +0xE270, owner +0x14) belongs to the player.
static int player_regions(unsigned player) { COST_SCOPE("ambush:player_regions");
    int count=0;
    for(unsigned id=1;id<=500;++id) {
        Obj area=table(0x6CAF0,id,500);if(!valid(area))continue;
        Obj tile=table(0xE270,word(area,0x3A),48399);
        if(valid(tile) && tile[0x14]==player)++count;
    }
    return count;
}
static unsigned next_random() {
    if(!rng_state)rng_state=GetTickCount64()^(reinterpret_cast<uintptr_t>(world())<<7)^0x9E3779B97F4A7C15ull;
    rng_state^=rng_state<<13;rng_state^=rng_state>>7;rng_state^=rng_state<<17;
    return static_cast<unsigned>(rng_state>>11);
}
static void table_clear() {
    if(!shared)return;
    for(unsigned i=0;i<kAmbushPeople;++i){InterlockedExchange(&shared->expiry[i],0);InterlockedExchange(&shared->contact[i],0);}
    InterlockedExchange(&shared->count,0);
}
static void table_set(unsigned leader,int until) {
    if(!shared || !leader || leader>=kAmbushPeople)return;
    const LONG before=InterlockedExchange(&shared->expiry[leader],until);
    if(!until)InterlockedExchange(&shared->contact[leader],0);
    if(!before && until)InterlockedIncrement(&shared->count);
    else if(before && !until)InterlockedDecrement(&shared->count);
}
static void expire(int today) {
    if(!shared || !shared->count)return;
    for(unsigned i=1;i<kAmbushPeople;++i) {
        const LONG until=shared->expiry[i];
        if(until && today>=until) {table_set(i,0);log_capped("ambush_expired",i,static_cast<unsigned>(today),0);}
    }
}
// The player's first line: a region not the player's (center tile owner, KB structs.md 3.6) with a neighbour region that
// is (neighbour slots +0x56, counted ones in +0x66, as the retake rings).
static unsigned region_owner(Obj area) {
    Obj tile=valid(area)?table(0xE270,word(area,0x3A),48399):nullptr;
    return valid(tile)?tile[0x14]:255u;
}
// user 2026-11-13 "강하랑 연결된건 안륙이랑 경릉 하구항인데 운두는 경릉이랑 인접": the first line is measured from the player's
// bases, not from any held region. A home region = the region of the center hex (building +0x10) of a castle (building ids
// 1..51) the player owns (building +0x13); a first-line region is a region not the player's that neighbours a home region.
static void home_regions(unsigned player,bool home[501]) {
    for(unsigned i=0;i<501;++i)home[i]=false;
    for(unsigned id=1;id<=51;++id) {   // castles only, as the computer retake rings (campaign_area_id of the castle)
        Obj b=table(0x6DA98,id,3000);if(!valid(b) || b[0x13]!=player)continue;
        const unsigned hex=word(b,0x10);Obj tile=hex<48400?table(0xE270,hex,48399):nullptr;
        const unsigned region=valid(tile)?word(tile,0x12):0;if(region && region<=500)home[region]=true;
    }
}
static bool frontline_region(unsigned player,unsigned id,const bool *homes=nullptr) {
    Obj area=id && id<=500?table(0x6CAF0,id,500):nullptr;
    if(!valid(area) || !player || region_owner(area)==player)return false;
    bool own[501];if(!homes){home_regions(player,own);homes=own;}
    for(unsigned i=0;i<8;++i) {
        const unsigned n=word(area,0x56+i*2);if(!(area[0x66]&(1u<<i)) || !n || n>500)continue;
        if(homes[n] && region_owner(table(0x6CAF0,n,500))==player)return true;
    }
    return false;
}
// Hostile facilities (definition class 2..4, as building_destruction::hostile: no cities or gates) standing in the first
// line; the proposal is only made when there is something to destroy.
static unsigned frontline_facilities(unsigned player) { COST_SCOPE("ambush:frontline_facilities");
    if(!player)return 0;
    bool line[501]={},seen[501]={};unsigned count=0;
    bool homes[501];home_regions(player,homes);
    for(unsigned id=62;id<=3000;++id) {
        Obj b=table(0x6DA98,id,3000);
        if(!valid(b) || !word(b,0x14) || !b[0x13] || b[0x13]>51 || b[0x13]==player || b[0x12]>30)continue;
        Obj definition=table(0x73958,b[0x12],30);
        if(!valid(definition) || definition[0x2C]<2 || definition[0x2C]>4)continue;
        if(!fn<int(__fastcall *)(unsigned,unsigned)>(GA(GA_2CC630))(player,b[0x13]))continue;
        const unsigned hex=word(b,0x10);Obj tile=hex<48400?table(0xE270,hex,48399):nullptr;
        const unsigned region=valid(tile)?word(tile,0x12):0;if(!region || region>500)continue;
        if(!seen[region]) {seen[region]=true;line[region]=frontline_region(player,region,homes);}
        if(line[region])++count;
    }
    return count;
}
// building_destruction::ticket_roll asks after an original failure (UI/AI thread of the stratagem's execution).
// destroy_check: the same conditions without spending (local128: building_destruction::advice, the original advisor line).
static unsigned destroy_region(Obj person,Obj region) {
    if(!destroy_usable() || !destroy_tickets || !valid(person) || !valid(region))return 0;
    const unsigned player=player_force();
    Obj district=table(0xE0D0,person[0xE0],51);
    if(!player || !valid(district) || district[0x10]!=player)return 0;
    unsigned id=0;for(unsigned k=1;k<=500 && !id;++k)if(table(0x6CAF0,k,500)==region)id=k;
    return id;
}
static bool destroy_check(Obj person,Obj region) {const unsigned id=destroy_region(person,region);return id && frontline_region(player_force(),id);}
static bool destroy_use(Obj person,Obj region) { COST_SCOPE("ambush:destroy_use");
    const unsigned id=destroy_region(person,region);if(!id)return false;
    if(!frontline_region(player_force(),id)) {log_capped("destruction_ticket_not_frontline",word(person,0x10),id,destroy_tickets);return false;}
    --destroy_tickets;
    log_capped("destruction_ticket_used",word(person,0x10),id,destroy_tickets);
    briefing_overlay::toast(L"파괴공작으로 계략 [건물파괴]를 성공시켰습니다.");
    return true;
}
// Player frame (advisor hud, outside the progress phase): one pass per 10-day turn.
static void tick() { COST_SCOPE("ambush:tick");
    if(!usable())return;
    int month=0,xun=0;if(!calendar(month,xun))return;
    const int key=month*3+xun;if(key==last_turn){refund_cancelled();return;}
    last_turn=key;fresh.clear();
    if(offers) {log_capped("ambush_offer_lapsed",offers,tickets,0);offers=0;}   // unanswered last turn = declined
    const unsigned player=player_force();if(!player)return;
    const int count=player_regions(player);
    if(prev_count>=0 && count<prev_count)lost=(std::min)(255u,lost+static_cast<unsigned>(prev_count-count));
    prev_count=count;
    expire(game_day());
    // local64 (user 2026-11-12 "다음순에 풀리게 될때 말풍선으로 ... 미리 한턴 ... 15초"): one turn before the end.
    if(shared && shared->count) {
        unsigned ending=0;const int today=game_day();
        for(unsigned i=1;i<kAmbushPeople;++i) {const LONG until=shared->expiry[i];if(until>today && until-today<=10)++ending;}
        if(ending) {
            wchar_t text[160];
            swprintf_s(text,L"매복기동: %u개 부대의 매복이 이번 순을 끝으로 풀립니다. 다음 순부터는 적의 눈에 다시 띕니다.",ending);
            briefing_overlay::toast_for(text,15000);log_capped("ambush_ending",ending,static_cast<unsigned>(today),0);
        }
    }
    // local105 (user 2026-11-13 "건물파괴 티켓은 중순이 아니라 상순에 주자. 또 매복기동이랑 독립적으로 도는게 아니라 둘중 하나의
    // 티켓만 얻을 수 있게"): both kinds roll at 상순 in the same monthly pass and at most one proposal is made; when both
    // rolls hit, one of them is taken 50:50.
    if(xun!=0 || month==last_roll_month)return;
    last_roll_month=month;
    const unsigned chance=(std::min)(100u,5u+10u*lost),roll=next_random()%100u;
    log_capped("ambush_roll",chance,roll,static_cast<unsigned>(count));
    lost=0;
    // user 2026-11-13 "전체 티켓은 2장으로 제한": ambush and destruction tickets share one cap of kMaxTickets.
    const bool room=tickets+destroy_tickets<kMaxTickets;
    const bool ambush_hit=roll<chance && room;
    bool destroy_hit=false;
    if(destroy_usable() && room) {
        const unsigned found=frontline_facilities(player);
        const unsigned dchance=found?(std::min)(60u,20u+5u*found):0u,droll=next_random()%100u;
        log_capped("destruction_ticket_roll",dchance,droll,found);
        destroy_hit=droll<dchance;
    }
    if(!ambush_hit && !destroy_hit)return;
    offers=1;   // local64: shown as a speech balloon at the advisor head (수락/거절), no report panel
    offer_kind=destroy_hit && (!ambush_hit || (next_random()&1u))?1u:0u;
    log_capped("ticket_offer_kind",offer_kind,ambush_hit?1u:0u,destroy_hit?1u:0u);
}
// First head click of a turn with a pending offer opens the proposal (advisor_briefing::hud).
static bool offer_pending() {return usable() && offers && offer_turn!=last_turn;}
static void offer_shown() {offer_turn=last_turn;}
static Obj ruler_base() {
    Obj root=world();const unsigned force=player_force();if(!root || !force)return nullptr;
    Obj force_object=pointer(root,0xDF30+(force<0x34?force:0u)*8);
    const unsigned ruler_id=force_object?word(force_object,0x10):0;   // ruler +0x10 (structs.md FORCE); +0x52 = 군사, read until local257
    Obj ruler=ruler_id && ruler_id<=6000?table(0x158,ruler_id,6000):nullptr;
    const unsigned base_id=valid(ruler)?word(ruler,0xE2):0;
    Obj base=base_id?fn<Obj(__fastcall *)(Obj,unsigned)>(GA(GA_32E3B0))(root,base_id):nullptr;
    return valid(base)?base:nullptr;
}
// 수락/거절 (game UI thread, advisor hud). Gold: the ruler's base (KB structs.md 3.7, CCityData +0x34 gold); the base's
// own getter (vtable+0x90, auto_commands) must report the same value, otherwise nothing is paid.
static void answer(bool accept) { COST_SCOPE("ambush:answer");
    if(!usable() || !offers)return;
    const bool destroy=offer_kind==1;
    if(!accept) {--offers;log_capped(destroy?"destruction_ticket_offer":"ambush_offer",0,offers,destroy?destroy_tickets:tickets);briefing_overlay::toast(L"알겠습니다. 다음 기회를 엿보겠습니다.");return;}
    unsigned &held=destroy?destroy_tickets:tickets;
    if(tickets+destroy_tickets>=kMaxTickets) {offers=0;log_capped("ambush_offer_skipped",4,tickets+destroy_tickets);briefing_overlay::toast(L"매복·파괴공작 준비는 합쳐서 두 개까지만 갖출 수 있습니다.");return;}
    Obj base=ruler_base();
    if(!base) {log_capped("ambush_offer_skipped",1);briefing_overlay::toast(L"군주께서 머무실 거점을 찾지 못했습니다.");return;}
    Obj vtable=*reinterpret_cast<Obj *>(base);
    const int gold=static_cast<int>(reinterpret_cast<int(__fastcall *)(Obj)>(*reinterpret_cast<void **>(vtable+0x90))(base));
    int &field=*reinterpret_cast<int *>(base+0x34);
    if(gold!=field) {log_capped("ambush_offer_skipped",2,static_cast<unsigned>(gold),static_cast<unsigned>(field));briefing_overlay::toast(L"금을 셈하지 못해 이번에는 미루겠습니다.");return;}
    if(gold<kCost) {log_capped("ambush_offer_skipped",3,static_cast<unsigned>(gold));briefing_overlay::toast(L"군주 거점의 금이 1000에 못 미칩니다. 금이 모이면 다시 아뢰겠습니다.");return;}
    field=gold-kCost;
    --offers;++held;
    log_capped(destroy?"destruction_ticket_offer":"ambush_offer",1,offers,held);
    briefing_overlay::toast(destroy?L"파괴공작 준비를 마쳤습니다. 우리 성이 있는 지역과 바로 맞닿은 지역에 계략 [건물파괴]를 쓰면 실패할 계략도 성공합니다.":
                                    L"매복 준비를 마쳤습니다. 출진 화면에서 [군사제안수행]를 켜고 출진하십시오.");
}

// ---- sortie screen: the [군사제안수행] toggle (same technique as tactic_priority's 출진 기억 toggle) ----
// tactic_priority owns the screen's update/leave/departure hooks (sortie_screen_bridge.h); it registers them whenever
// tactic_priority or ambush is on, and this module's panes belong to its UI owner.
static const char kUiOwner[]="tactic_priority";
struct Desc {int type,x,y,w,h,r14,id,r1c;const wchar_t *text;int minus1,r2c;};
static_assert(sizeof(Desc)==0x30,"descriptor entry is 0x30 bytes");
struct Toggle {Obj pane=nullptr,control=nullptr,tab=nullptr;bool built=false,skipped=false;unsigned tries=0,person=0;int shown=-1,enabled=-1;};
static Toggle tog;
static Obj screen;
static std::vector<unsigned> marked;       // officers marked on this screen (cleared at leave)
static unsigned departed;                  // ambush armies sent from this screen
static volatile LONG toggle_pending;
static const unsigned kTabNodeB=0x2B;      // original mode tab of the sortie page (7D67E0), visibility source
static const int kToggleW=320,kToggleH=64,kImageId=0x2BA;
static bool is_marked(unsigned person) {return std::find(marked.begin(),marked.end(),person)!=marked.end();}
static Obj current_record(Obj state) {   // list[state+0x4E4] of the records state+0x4C0..0x4C8 (tactic_priority current_record)
    uintptr_t first=reinterpret_cast<uintptr_t>(pointer(state,0x4C0)),last=reinterpret_cast<uintptr_t>(pointer(state,0x4C8));
    if(!first || last<first || (last-first)%8 || (last-first)/8>64 || first==last)return nullptr;
    const unsigned count=static_cast<unsigned>((last-first)/8),index=dword(state,0x4E4);
    return index<count?*reinterpret_cast<Obj *>(first+index*8ull):nullptr;
}
static unsigned record_person(Obj record) {return record?dword(record,0):0;}
static void __cdecl toggle_clicked(void *,void *) {InterlockedExchange(&toggle_pending,1);}
static void follow(Obj own,Obj source) {
    if(!own || !source)return;
    if(((dword(own,0x40)^dword(source,0x40))&0xBu)==0 && dword(own,0x30)==dword(source,0x30))return;
    layout_client::ui_follow(own,source);
}
static void build_toggle(Obj state) { COST_SCOPE("ambush:build_toggle");
    Obj main=pointer(state,0x470);
    if(!main || ++tog.tries>600)return;
    Obj tab=static_cast<Obj>(layout_client::ui_node(main,static_cast<int>(kTabNodeB)));
    if(!tab)return;
    // Bottom-right of the 1920x1080 layout space, above the footer (user 2026-11-11). Diagnostics log the parent.
    // local64 (user 2026-11-12 "조금 더 내려가고 글씨가 좁아지지 않게 넓혀줘"): 220 -> 320 wide, 58 lower.
    const int x=1920-kToggleW-40,y=1080-kToggleH-62;
    layout_client::UiScope scope(kUiOwner);
    if(!scope.ok) {tog.tries=601;log_capped("ambush_toggle_built",0,3,0);return;}
    Desc desc{};desc.type=2;desc.w=kToggleW;desc.h=kToggleH;desc.id=kImageId;desc.minus1=-1;
    Obj pane=static_cast<Obj>(layout_client::ui_window(main,x,y,kToggleW,kToggleH,&desc,1));
    if(!pane) {tog.tries=601;log_capped("ambush_toggle_built",0,1,0);return;}
    Obj control=static_cast<Obj>(layout_client::ui_button(pane,0,10,1));
    if(!control) {tog.pane=pane;tog.tries=601;log_capped("ambush_toggle_built",0,2,0);return;}
    layout_client::ui_click(control,&toggle_clicked,nullptr);
    const int activated=layout_client::ui_activate(pane);
    follow(pane,tab);
    tog.pane=pane;tog.control=control;tog.tab=tab;tog.built=true;tog.shown=-1;tog.enabled=-1;
    log_capped("ambush_toggle_built",1,static_cast<unsigned>(x),static_cast<unsigned>(y));
    log_capped("ambush_toggle_parent",dword(main,0x20),dword(main,0x24),static_cast<unsigned>(activated&0xFF));
}
static void apply_toggle(Obj state) {
    follow(tog.pane,tog.tab);
    const unsigned person=record_person(current_record(state));
    const bool on=person && is_marked(person);
    const int want=person?(on?1:0):2;
    if(want!=tog.shown) {
        tog.shown=want;tog.person=person;
        wchar_t text[48];
        const unsigned left=tickets>marked.size()?tickets-static_cast<unsigned>(marked.size()):0;
        swprintf_s(text,L"%ls (%u)",on?L"군사제안수행 켬":L"군사제안수행 끔",left);
        layout_client::ui_node_text(tog.pane,0,want==2?L"군사제안수행":text);
        layout_client::ui_select(tog.control,on?1:0);
    }
    const int can=person && (on || marked.size()<tickets)?1:0;
    if(can!=tog.enabled) {tog.enabled=can;layout_client::ui_enable(tog.control,can);}
    else if(person && ((dword(tog.control,0x198)!=0)!=on))layout_client::ui_select(tog.control,on?1:0);
}
static void run_toggle(Obj state) {
    const unsigned person=record_person(current_record(state));
    if(!person || person>=kAmbushPeople)return;
    if(is_marked(person))marked.erase(std::find(marked.begin(),marked.end(),person));
    else if(marked.size()<tickets)marked.push_back(person);
    else {briefing_overlay::toast(L"준비된 매복은 여기까지입니다.");return;}
    log_capped("ambush_toggle",person,is_marked(person)?1u:0u,static_cast<unsigned>(marked.size()));
    tog.shown=-1;
}
static void reset_screen() {tog=Toggle();marked.clear();departed=0;toggle_pending=0;screen=nullptr;}
static void on_update(Obj state) { COST_SCOPE("ambush:on_update");
    if(!usable())return;
    if(screen!=state) {reset_screen();screen=state;tog.skipped=!tickets;   // no ticket at entry = no toggle at all
        log_capped("ambush_screen",tickets,offers,tog.skipped?1u:0u);}
    if(tog.skipped)return;
    if(!tog.built && tog.tries<=600)build_toggle(state);
    if(!tog.built)return;
    bool work=toggle_pending!=0;
    const unsigned person=record_person(current_record(state));
    const int want=person?(is_marked(person)?1:0):2;
    if(want!=tog.shown || (((dword(tog.pane,0x40)^dword(tog.tab,0x40))&0xBu) || dword(tog.pane,0x30)!=dword(tog.tab,0x30)))work=true;
    if(!work)return;
    layout_client::UiScope scope(kUiOwner);
    if(!scope.ok) {toggle_pending=0;return;}
    if(InterlockedExchange(&toggle_pending,0))run_toggle(state);
    apply_toggle(state);
}
static void on_leave(Obj state,void *next,bool released) { COST_SCOPE("ambush:on_leave");
    if(screen!=state)return;
    if(!released && (tog.built || tog.pane)) {   // tactic_priority itself is off: release this owner's panes here
        layout_client::UiScope scope(kUiOwner);
        const int count=scope.ok?layout_client::ui_release(kUiOwner,next):-1;
        log_capped("ambush_toggle_released",static_cast<unsigned>(count),departed,0);
    }
    if(departed) {
        wchar_t text[160];
        swprintf_s(text,L"매복기동: %u개 부대가 떠났습니다. 이제는 장수들이 잘 해내리라 믿을 뿐입니다.",departed);
        briefing_overlay::toast(text);
    }
    reset_screen();
}
// Departure (tactic_priority's CALL 7EE1C4 -> 1EDC30 hook): the officer really left with this army.
static void on_depart(unsigned person,Obj army) { COST_SCOPE("ambush:on_depart");
    if(!army || !usable() || !person || person>=kAmbushPeople || !is_marked(person) || !tickets)return;
    --tickets;++departed;
    marked.erase(std::find(marked.begin(),marked.end(),person));
    const int until=game_day()+kDays;
    table_set(person,until);fresh.push_back(person);
    log_capped("ambush_departed",person,static_cast<unsigned>(until),tickets);
}
static unsigned offer_count() {return usable()?offers:0;}
// local67 (user 2026-11-12 "군사보고에 군사제안 칸 ... 매복기동 작전 수행부대 : , 남은 매복준비 x개").
static void status(std::wstring &out) { COST_SCOPE("ambush:status");
    out.clear();if(!usable())return;
    std::wstring names;unsigned shown=0,total=0;
    if(shared->count)for(unsigned i=1;i<kAmbushPeople;++i)if(shared->expiry[i]) {
        ++total;if(shown>=4)continue;
        Obj person=table(0x158,i,6000);
        const wchar_t *name=valid(person)?fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_230950))(person):nullptr;
        if(!name || !*name)continue;
        if(shown)names+=L", ";
        names.append(name,wcsnlen(name,24));++shown;
    }
    (void)names;(void)shown;(void)total;
    // local68 (user 2026-11-12): body of the report's [군사 제안] page. local130 (user 2026-11-13 "티켓이 없는건 지웠으면 좋겠어. 종류를 앞으로
    // 많이 추가할거거든" / "조언마다 테이블로 감싸서 배경으로 구분"): one card per kind (\x1E = the overlay's card mark), only kinds with a
    // prepared ticket (ambush also while an army is still hidden). Nothing held = empty text: the page says there is nothing to report.
    std::wstring army;
    const int today=game_day();
    if(shared->count)for(unsigned i=1;i<kAmbushPeople;++i)if(const LONG until=shared->expiry[i]) {
        Obj person=table(0x158,i,6000);
        const wchar_t *name=valid(person)?fn<const wchar_t *(__fastcall *)(Obj)>(GA(GA_230950))(person):nullptr;
        std::wstring who=name && *name?std::wstring(name,wcsnlen(name,24)):L"무장 "+std::to_wstring(i);
        const int left=(std::max)(0,static_cast<int>(until)-today);
        army+=L"· "+who+L"  —  남은 기간 "+std::to_wstring(left)+L"일,  "+
            (shared->contact[i]?L"ZOC에 걸림 (기동 회복, 60일 제한 시작)":L"아직 ZOC 없음 (기동 절반)")+L"\n";
    }
    if(tickets || !army.empty()) {
        out+=L"\x1E매복기동  ·  준비 "+std::to_wstring(tickets)+L"개\n";
        out+=army.empty()?std::wstring(L"지금 매복 중인 부대가 없습니다.\n"):L"작전 수행 부대\n"+army;
        out+=L"출진부터 길어도 180일, 처음 ZOC에 걸린 날부터는 60일까지 유지됩니다.\n";
    }
    if(destroy_usable() && destroy_tickets)
        out+=L"\x1E파괴공작  ·  준비 "+std::to_wstring(destroy_tickets)+L"개\n우리 성이 있는 지역과 바로 맞닿은 지역에 계략 [건물파괴]를 쓰다가 원래 판정이 실패하면 준비 하나를 써서 성공으로 바꿉니다. "
             L"스스로 성공한 계략에는 쓰지 않습니다.\n";
}
static const advisor_bridge::AmbushHooks bridge_hooks={&tick,&badge,&offer_pending,&offer_shown,&answer,&offer_count,&ticket_count,&status,&current_offer_kind};
static const sortie_bridge::Hooks sortie_hooks={&on_update,&on_leave,&on_depart};
// Called after tactic_priority::install (registry order); the screen hooks are tactic_priority's.
static bool install() {
    HMODULE combat=GetModuleHandleW(L"SAN14_CombatAI.dll");
    auto get=combat?reinterpret_cast<GetAmbushTable>(GetProcAddress(combat,"SAN14CombatAmbush")):nullptr;
    AmbushTable *table=get?get(kAmbushVersion):nullptr;
    if(!table || table->size!=sizeof(AmbushTable) || table->version!=kAmbushVersion) {log_event("ambush_combat_missing",combat?1u:0u,get?1u:0u,0);return false;}
    if(!tactic_priority::screen_hooks_ready()) {log_event("ambush_screen_missing");return false;}
    shared=table;advisor_bridge::ambush=&bridge_hooks;sortie_bridge::extra=&sortie_hooks;
    building_destruction::ticket_use=&destroy_use;building_destruction::ticket_ready=&destroy_check;   // the CALL link is building_destruction's (destruction_ticket)
    log_event("ambush_ready",0,0,0);return true;
}

// ---- sidecar <save>.<sha256>.ambush: "AMBH" + version 1 + 3 pad + 32-byte binding, then state and up to 64 hidden leaders ----
static const size_t kSideHead=56,kSideEntries=64,kSideSize=kSideHead+kSideEntries*8;
static std::vector<uint8_t> saved;
static SidecarCache side_cache;
static void side_reset() {
    tickets=offers=lost=0;destroy_tickets=offer_kind=0;fresh.clear();prev_count=-1;last_roll_month=-1;last_turn=-1;offer_turn=-1;
    table_clear();reset_screen();
}
static void put32(uint8_t *at,int v) {memcpy(at,&v,4);}
static int get32(const uint8_t *at) {int v;memcpy(&v,at,4);return v;}
static void side_snapshot() { COST_SCOPE("ambush:side_snapshot");
    saved.assign(kSideSize-40,0);   // without magic/version/binding
    uint8_t *b=saved.data();
    // destruction_ticket: the kind of the pending proposal in b[1] bit 4, destruction tickets in the former pad b[3]
    // (both 0 in older sidecars = ambush proposal, no destruction ticket).
    b[0]=static_cast<uint8_t>((std::min)(tickets,255u));b[1]=static_cast<uint8_t>((offers?1u:0u)|((offer_kind&1u)<<4));b[2]=static_cast<uint8_t>(lost);
    b[3]=static_cast<uint8_t>((std::min)(destroy_tickets,kMaxTickets));
    put32(b+4,prev_count);put32(b+8,last_roll_month);put32(b+12,last_turn);
    unsigned n=0;
    if(shared)for(unsigned i=1;i<kAmbushPeople && n<kSideEntries;++i)if(const LONG until=shared->expiry[i]) {
        uint8_t *e=b+16+n*8;e[0]=static_cast<uint8_t>(i);e[1]=static_cast<uint8_t>(i>>8);e[2]=shared->contact[i]?1:0;put32(e+4,until);++n;
    }
}
static bool side_write(const merit::SaveCoordinator::Completed &job,save_host::Written &out) { COST_SCOPE("ambush:side_write");
    std::vector<uint8_t> bytes(kSideSize,0);
    memcpy(bytes.data(),"AMBH",4);bytes[4]=1;memcpy(bytes.data()+8,job.binding.data(),32);
    if(saved.size()==kSideSize-40)memcpy(bytes.data()+40,saved.data(),saved.size());
    return save_host::store(self_module,save_host::Job{job.name,job.binding},L"ambush",bytes,side_cache,out);
}
static bool side_restore(const merit::SaveCoordinator::Completed *job) { COST_SCOPE("ambush:side_restore");
    side_reset();bool restored=false;
    if(job && usable()) {
        wchar_t path[MAX_PATH];
        if(save_host::path(self_module,save_host::Job{job->name,job->binding},L"ambush",path,MAX_PATH,false)) {
            HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
            if(file!=INVALID_HANDLE_VALUE) {
                std::vector<uint8_t> bytes(kSideSize,0);DWORD got=0;LARGE_INTEGER length={};
                if(GetFileSizeEx(file,&length) && length.QuadPart==static_cast<LONGLONG>(kSideSize) &&
                   ReadFile(file,bytes.data(),static_cast<DWORD>(kSideSize),&got,nullptr) && got==kSideSize &&
                   !memcmp(bytes.data(),"AMBH",4) && bytes[4]==1 && !memcmp(bytes.data()+8,job->binding.data(),32)) {
                    const uint8_t *b=bytes.data()+40;
                    tickets=(std::min)(static_cast<unsigned>(b[0]),kMaxTickets);offers=(b[1]&0x0F)?1u:0u;offer_kind=(b[1]>>4)&1u;lost=b[2];
                    destroy_tickets=(std::min)(static_cast<unsigned>(b[3]),kMaxTickets);prev_count=get32(b+4);last_roll_month=get32(b+8);last_turn=-1;
                    for(unsigned n=0;n<kSideEntries;++n) {
                        const uint8_t *e=b+16+n*8;const unsigned leader=e[0]|(e[1]<<8);const int until=get32(e+4);
                        if(leader && leader<kAmbushPeople && until>0){table_set(leader,until);InterlockedExchange(&shared->contact[leader],e[2]?1:0);}
                    }
                    restored=true;
                }
                CloseHandle(file);
            }
        }
    }
    log_event("ambush_load",tickets,offers,(shared?static_cast<unsigned>(shared->count):0u)|(restored?(1u<<16):0u));
    return restored;
}
static const save_host::SidecarPlugin sidecar_plugin={"ambush",L"ambush",1,"ambush_save_written","ambush_save_write_failed",
    []() {return mod_features.ambush;},
    []() {side_snapshot();},
    [](const save_host::Job &job,save_host::Written &out) {return side_write(*static_cast<const merit::SaveCoordinator::Completed *>(job.source),out);},
    []() {side_cache.clear();side_reset();},
    [](const save_host::Job *job) {return side_restore(job?static_cast<const merit::SaveCoordinator::Completed *>(job->source):nullptr);}};
}   // namespace ambush
