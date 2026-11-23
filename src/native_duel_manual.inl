// [extensions] duel_relay (local298-308 [diagnostics] duel_manual_test): the San11-style relay duel (user 2026-11-22 「의용병무리는 일기토에서 아예빼버려 … 버튼이 좌하단에 떠야하고
// 버튼식이어야해 다중일기토는 결국없앨거야 삼11식 릴레이 일기토를 구현할거야 … 일기토합류는 2헥스로 하자 … 플레이어조작에따라 달라져야지」,
// 「한번에 다하고 테세1-4에따라서 유불리가 다르다던데?」; design docs/MANUAL_DUEL.md sections 10-11). Every build since v16.7 (a gameplay-tab test feature; local298-308 diagnostic builds only), inside the duel_multi
// module (installed when this key is on; its hooks dispatch here; the multi-duel feature itself was removed 2026-11-22). A player duel scene whose original block
// has no 일격 is decided by the San11 rules engine (duel_s11_rules.h), one round (합) at a time (local305; local295-304 five at a time):
//   opening: the original intro, charge and clash, once. From the scene decision on the block +0x60 is a neutral one (start HP in
//     every HP column, round codes 0, W/L 2/2), so the clash and the round-1 setup show the start HP.
//   hold (before every 합, from the first frame of state 0xB round 1): the timer +0x4B0 goes back to 0 after every tick (no swing, no HP
//     step, no round end). local295 (user 「대기장면에서 … 지형이 멈춰있어서 정적인 느낌이 되게 별로임」, 「사진2의 두번째 샷이 대기장면으로 더
//     보기 좋았음」): both duelists keep action 0x15 (the gallop of that shot, 37D930 does not move it) and the mod moves them as one
//     pair (hold_ride). local298 (user 「그5 합만 자동」 plan, rehearsal artifact): on a circle (kCircleR 1400, the centre on the camera
//     side, the original riding speed) from the first hold frame, the duelists kSeatGap apart with the enemy easing to kLead ahead,
//     the player's duelist nearest the camera (seated on the far side: both turn around where they stand at that cut; local307: the
//     original seats kept, no lead, a gap straight on, the camera on the player's side instead — see kRideSpeed). The camera
//     is ours (vt+0x68 eye, vt+0x70 rotation): kCamAzim off the travel towards the player's side, kCamPitch down, the riders aimed
//     at the middle of the free screen rectangle, height from the ground (BAAF50) every frame, the eye never under its ground +
//     kEyeClear. local300 (user 「원작의 1.8배만 멀어지게 해줘」, 「원형으로 돌게하면서 실제로 움직이게 바꿔준다며」): the rotation is
//     written absolute (Rot(q) z = forward, Rot(q) y = up: local299 read four writes back exactly so; the original camera's getter
//     rotation is not its view, so nothing is built on it), the distance is 1.8 x the original camera's eye-to-pair distance at the
//     scene's first hold, and the ride never waits on our camera: the hold's first write is read back in the same tick, and a
//     rotation read back off (no rotation writes for the run), an eye not taken, no ground or a fault give the follow camera (the
//     original rotation, the eye 1.8 x back along it from the riding pair); a follow camera that loses the pair sends it home to
//     gallop in place (duel_manual_dist / side / cam / camf / camg / ride).
//   flow (local305, user 2026-11-23 「대기화면은 일기토 처음 시작하고나서 나오고 난입할때나오고 교체버튼 누를때나오고 필살버튼 누를때
//     나오는거야. 5합씩 멈추는게 아니라 … 1합을 2초정도 진행하면서 천천히 진행되는데 플레이어가 실시간으로 태세를 바꾸는거지 필살버튼도
//     실시간으로 누르고 … 선택 안되면 그전 입력이 들어가고 … 교체는 1회로 제한하자」, confirmed 「1A 2C 3A」): every hold plays one 합
//     at its end. A gap hold (camera kGapDist) lasts the installer's gap (local308 duel_relay_gap, kGapMs without it) and needs no click; a full hold (camera kHoldDist, 「확정」) comes at the
//     duel's start, for a 필살 or 교대 pressed (at once in a gap, else after the 합 on stage) and right before a 합 with a join: the
//     next 합 is looked ahead on a copy of the engine (exact for a stance or a 교대, duel_manual_script flow_peeks; 자동 and 필살 are not
//     looked ahead), and a join seats the joiner with its 「난입!」 sticker first. That 합 keeps the command class looked ahead with (a
//     stance changed in the join hold applies; a 필살 or 교대 picked there waits for the 합 after). A full hold with no click for kAutoMs
//     plays the stance (the pick dropped), kHoldCapMs plays the pick. Both sides switch once a duel (Config::swap_limit; joins do not
//     count, a 교대 dropped for a join spends nothing); non-special damage is San11's (dmg 1/1).
//   retreat and criticals (local306, user 「크리티컬이나 자동발동견수 퇴각(교대 횟수 다쓰면 퇴각으로 변하기)는 삼11 코드로 확인해서
//     넣으면안돼?」, confirmed 「1B 2A 3A 4A」): San11's retreat (action 3, 투지 100) is open to both sides (Config::retreat; the
//     computer's kind 15 and a side on 자동 too). The 교대 place reads 퇴각 while no 교대 can be made; pressed it opens the 퇴각 hold
//     (퇴각 picked, 「확정」). The retreat 합 ends the duel, the retreating side loses; a retreating fighter with at least the other's HP
//     gives up the difference + 1 as the other's parting hit (engine retreat_end: San14 settles on end HP; never below 1, so no death
//     or capture). The stances' criticals already in the engine (0x509480: 연속 공격, 자동 견수 = 보호, 투지 +100) and a retreat show as
//     stickers on their side's half; a status line under the strip shows the charge and both guard timers.
//   input (local305 strip): a bottom-left strip up from the first hold to the scene end (owner duel_multi, no frame; drawn by
//     native_duel_digits.inl over this button window as its click catcher; the plain buttons when the overlay draws nothing): the four
//     stances (kept for every 합 until changed), 필살 (on while the engine allows one now), 교대 (on while the side's switch is left and a
//     member who fought waits; 「교대 사용함」 after it), 자동 (the engine plays the side 합 after 합, until turned off or a stance / 필살 /
//     교대 is pressed). A full hold adds its list over the strip (필살기 / 급소 / 무쌍 / 기합 / 견수, or the 교대 members) and 「확정」.
//     Input during a gap applies to the 합 at its end, during a 합 to the 합 after. A click reaches us through the native button callback
//     or by polling the mouse; hidden and disabled buttons take no click. The 투지 is a yellow bar under each HP bar (the 필살 picked or
//     pending in red) and the round number over the medallion is ours (1-30), both drawn on Present too.
//   release: the pair goes back to its hold-start heading and seat offsets about its middle on that frame (the round's own camera takes
//     over), then 374E00(kind of round 1) plays the 합 there; the round states re-centre on the seats (377350 / 377E60 / 377930 all
//     recompute +0x5D0 every frame).
//   specials (local295, 「3. 4 둘다 A」): 급소 plays as 반격 (code 3, state 0xE), 견수 as 막음 on its side, 무쌍 as two or three 회심 scenes in
//     a row (the round end replays state 0xD with the next HP step; the last step is the engine's HP), 기합 shows by its sticker and
//     caption only (local295-310 tried the unused action 0x14 on its duelist; local311 dropped it: the shouter stopped
//     mid-charge). Every special and every join shows a caption (Layout text, 「장비 난입!」 / 「관우 무쌍!」).
//   round end hook (the 37D1C0 calls): a join or swap of the engine is shown as its 합 starts (duel_multi swap_in: model, look, person;
//     the HUD name and face; seat HP and the block's HP before the 합 = the incoming fighter's; a joiner the join hold seated keeps its
//     seat and leaves its sticker out). After the 합 either the duel ended (the original end runs) or the next hold block goes in with
//     pages 3 -> 4 and state 9 (the original round-1 setup: stand, re-seat behind the state-10 camera cut at the same place every time,
//     so the gap rides never drift; state 0xB, the next hold).
//   end: the last block carries the original result of the final pair. 3747A0 is called once with the final pair in the arg rows,
//     both from their duel-start +0xF1, its damage columns forced to the engine's final HP and no 일격 roll (duel_multi's damage and
//     일격 hooks), so the original writes W/L, outcome [0], fate [3] and both +0xF1. A member who fought and was swapped out gets
//     the engine HP in +0xF1 through a forced draw. 37AA90 and the 378890 records go to the final pair (duel_multi apply_hook /
//     record_hook). A skip (state 0x11 before the engine finished): the computer plays the rest at once, the final pair is seated and
//     state 0x11 starts again so 378DD0 reads the final block. A failed settlement check: the original block and every +0xF1 come back.
//   extras: duel_multi's extras of a relay scene are picked within 2 hexes (0-2 per side, band armies excluded), stay under the
//     ground until the engine brings them in, then ride in from behind beside their duelist with the waiting bars.
#include "duel_manual_script.h"
#include "duel_s11_rules.h"
namespace duel_manual {
namespace dm=duel_multi;
namespace sc=duel_manual_script;
namespace s11=duel_s11;
static const int kBlockRounds=5,kMembers=3;   // kBlockRounds: the block's round columns (local305 plays one 합 a block)
static const unsigned kAutoMs=60000,kHoldCapMs=300000,kStateLogs=400,kClickGapMs=250,kRebuilds=6,kCaptionMs=2500,kCaptionTries=3,kEndCaptionMs=4000;
static const unsigned kProbe[6]={1,30,60,120,300,600};
// hold ride (local298, the rehearsal's numbers): the original riding speed (37D930: 20 * 60 / fps a frame); a full hold on a circle of
// kCircleR, the seat headings easing to the pair's over kLeadEase seconds. local307 (user 「아군 무장이랑 적무장 달리는게 살짝
// 어긋나있어서 … 처음부터 일기토 하는 무장들은 같은 양옆에서 달리도록 못하나? 원으로 돌아도 각속도를 똑같이하면 같은 라인에서 달릴텐데」,
// confirmed A): the duelists keep the original seats (local298-306 eased the enemy kLead 250 ahead and turned the pair around), a gap
// rides straight on (the 합's own line: the original rides the round side by side at the same speed, local286), our side camera sits
// on the player's side (cam_side) instead of turning the pair.
static const float kRideSpeed=1200.0f,kCircleR=1400.0f,kLeadEase=1.0f,kSameHeading=0.5f,kPi=3.14159265f;
static const unsigned kSeatLogs=40;
// local307 (user 「A 뒤에서 출발」): a gap starts behind the original seats by the ride left (at most kRideSpeed * kGapMs, placed at the
// camera decision: gap_start) and rides straight into them; a miss up to kArriveSnap at the release is closed there
static const float kArriveSnap=400.0f;
// side camera: kCamAzim off the travel towards the camera side, kCamPitch down, the eye never closer than kEyeClear over its ground,
// the riders' boxes aimed at the middle of the free screen rectangle (gauge, waiting bars, command window and skip buttons left out);
// the distance is duel_manual_script.h hold_distance (local303: fixed kHoldDist 2500, nothing else changes it). kChest: the
// height over a seat the distances are measured to. The follow camera stops the ride after kFollowOff frames with the pair's middle
// outside kFollowEdge of the screen.
static const float kCamAzim=74.0f*kPi/180.0f,kCamPitch=13.0f*kPi/180.0f,kEyeClear=200.0f,kChest=150.0f,kFollowEdge=0.92f;
static const unsigned kFollowOff=20;
enum HoldCam:unsigned {kModeNone=0,kModeSide=1,kModeFollow=2,kModeStill=3};
static const float kRectLeft=500.0f,kRectRight=1860.0f,kRectTop=215.0f,kRectTopBars=300.0f,kRectBottom=1045.0f,kRectSlack=160.0f;
// checks: a written rotation reads back right when forward / up agree this well; the block is the getter's camera within kEyeSame and
// took our eye within kEyeApplied; kQuatTries hold frames to find a block of the original camera
static const float kQuatFwd=0.995f,kQuatUp=0.98f,kEyeSame=5.0f,kEyeApplied=20.0f;
static const unsigned kQuatTries=10;
static const int kActStill=0x15;
// the plain button window (the overlay off): two text rows, the list (three rows of two), the strip (four rows of two)
static const int kColW=200,kGap=6,kTextH=30,kBtnH=44,kPaneW=2*kColW+kGap;
static const int kListTop=2*kTextH+kGap,kStripTop=kListTop+3*(kBtnH+kGap),kPaneH=kStripTop+4*(kBtnH+kGap)-kGap,kPaneX=16,kPaneY=1080-16-kPaneH,kImageId=0x2BA;
static const int kCapW=640,kCapH=40,kCapX=(1920-kCapW)/2,kCapY=236;
// own round number: the gauge's round markers 0x1B-0x1F (827D70 shows the current round's) and its digit nodes 0x36 / 0x37 (827D70
// sets 07_num_(n-1) / 07_num_n on them through 931AB0; local299, the local298 screen showed the original digit), the medallion's
// nodes logged once a scene, the node whose centre places the number, the box it must fall in (1920 x 1080), the fixed place otherwise
static const int kDigitCount=7,kDigitNodes[kDigitCount]={0x1B,0x1C,0x1D,0x1E,0x1F,0x36,0x37},kNumberNode=0x1B;
static const int kMedalNodes[12]={0x1B,0x1C,0x1D,0x1E,0x1F,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B};
static const float kNumberX=960.0f,kNumberY=105.0f,kNumberBox[4]={760.0f,20.0f,1160.0f,220.0f};
// 투지 bars: under the HP bar frames (left 0x17, right 0x31; local288 rects as the fallback), kBarGap below them, kBarH high
static const int kHpFrame[2]={0x17,0x31},kHpFrameAt[2][4]={{390,92,484,20},{1046,92,484,20}},kBarGap=2,kBarH=8;
static const unsigned kNodePos=GA(GA_92F050);
struct Desc {int type,x,y,w,h,r14,id,r1c;const wchar_t *text;int minus1,r2c;};
static_assert(sizeof(Desc)==0x30,"descriptor entry is 0x30 bytes");
// nodes (local305): two text cells, the strip (four stances, 필살, 교대, 자동, 「확정」: duel_digits strip_rect order), the list's six
// places (list_rect order: duel_digits kListSpc for the 필살 list, the members on places 0-1 for the 교대 list, 「돌아가기」 on 5)
enum Node:int {NInfo=0,NGauge,NSt0,NSpcBtn=NSt0+4,NSwapBtn,NAutoBtn,NOk,NList,NCount=NList+6};
static_assert(NCount<=31,"one native click bit per node");
static_assert(NOk-NSt0==7,"the strip has eight places");
static const wchar_t *const kCmdName[sc::kPlanCommands]={L"공격중시",L"방어중시",L"투지중시",L"일발중시",L"필살기",L"기합",L"견수",L"급소",L"무쌍",L"교대",L"퇴각"};
static const wchar_t *const kActionName[6]={L"필살기",L"기합",L"견수",L"퇴각",L"급소",L"무쌍"};   // by engine action (3 퇴각: local306)
static const int kActionSticker[6]={duel_digits::SSpecial,duel_digits::SKiai,duel_digits::SGuard,duel_digits::SRetreat,duel_digits::SCrit,duel_digits::SMusou};   // local303 / 306
// local306: the critical moves of the attacking side (engine SideResult::move 5 / 6 / 7: San11 0x509480) as stickers and captions
static const wchar_t *const kMoveName[3]={L"연속 공격",L"자동 견수",L"투지 상승"};
static const int kMoveSticker[3]={duel_digits::SCombo,duel_digits::SAutoGuard,duel_digits::SSpirit};

struct Round {s11::RoundResult r;int hp[2][kMembers],hp_in[2];};   // hp: every member after the round; hp_in: the fighter before it
struct Combo {int side,n,i,seq[4];};   // 무쌍 in a round: receiver side, hits, hits played, HP steps (seq[n] = the engine's HP)

alignas(16) static int orig[sc::kInts];
alignas(16) static int blk[sc::kInts];           // the block on stage (copied to +0x60)
alignas(16) static int fin[0x40];                // the settlement call's block
alignas(16) static unsigned char fin_arg[0x40];  // its argument: the final pair in the arg rows (37AA90's too)
static s11::Duel duel;
static Round rounds[kBlockRounds];
static Combo combo[kBlockRounds];
static int pre_hp[2][kMembers],disp_hp[2][kMembers];   // members' HP before the block on stage / as the stage shows now
static Obj arg_person[2],scene,member[2][kMembers],fin_person[2];
static unsigned char saved_f1[2],member_f1[2][kMembers];
static unsigned entry_serial,seen_serial,row_of_side[2],side_of_row[2],player_side;
static bool entry_ok,decided,on,engine_on,ended,bailed,holding,hold_due,auto_original,auto_used,final_ready,applied,applied_twice,end_capped;
static bool shown[2][kMembers];
static int block_base;                    // engine rounds before the block on stage (its round k = block_base + k)
static bool number_hidden;                // the gauge's round-number nodes are ours (hidden) this scene
static bool number_logged,bars_logged;
static unsigned number_frames,number_seen;   // number_seen: digit nodes showing when first hidden (bit per kDigitNodes entry)
static unsigned reshow_frames,reshow_mask;    // local299: frames a hidden digit node showed again before our hide, which ones
static bool reshow_logged;
static int blk_morale[2],disp_morale[2];  // 투지 before the block on stage / as the stage shows now (with disp_hp)
static int seated[2],block_n;
static unsigned blocks,holds,clicks,polls,natives,autos,swaps,joins,rebuilds,state_logs,last_state,records,combos;
// flow (local305)
static bool vis[NCount],en[NCount];
static bool flow_on;                 // a 합 was played from a hold: the strip takes clicks between holds too
static int cmd_stance;               // the stance kept for every 합 (0..3)
static int pend,pend_who;            // a 필살 (4..8) or 교대 (9) for the next 합 (-1 none; a join hold's pick), the 교대's member
static bool auto_on;                 // 자동: the engine plays the player's side
static int want_hold;                // sc::HoldSpecial / HoldSwap pressed during a 합 (HoldGap none)
static int hold_kind;                // this hold's sc::HoldKind
static int list_open,pick,pm_n,pm[2];   // the list up (0 none, 1 필살, 2 교대 members), the picked place (-1), the members listed
static bool join_hold,hold_clicked;  // this hold is a join's / a click in this hold
static int join_cmd,join_who,join_peeked[2];   // the command the look-ahead ran with, its member; the joiner seated per side (-1)
static ULONGLONG t_round_start,t_round_end;   // the 합 on stage started / the last one ended (0 none)
static unsigned setup_ms,round_ms,gap_holds,full_holds,join_holds,join_misses;
static unsigned auto_flows;   // local308: 합 played straight on under 자동 (no gap); local312 also the installer's 「없음」 gap
// hold
static ULONGLONG hold_start;
static unsigned hold_frames;
static float pc_x,pc_z,ph,ph0,ride_t;                 // the pair's middle, heading now / at the ride start, seconds circled (local307: a gap's straight ride does not count)
static float hs_cx,hs_cz,hs_h,hs_f[2],hs_l[2],hs_dh[2];   // the pair as the original seated it (untouched: the release puts it back)
static float off_f[2],off_l[2],off_dh[2],off0_f[2],off0_l[2],off0_dh[2];   // each seat in the pair's frame (forward, left, heading) now / at the ride start
static bool side_wait,verify_due,flipped,orig_ok,heading_forced,cam_dead;   // flipped (local307): our camera moved to the player's side
static bool gap_shifted;   // local307: this hold's gap start was placed (gap_start)
static unsigned gap_left_ms;   // local308: the gap's time left at that cut (duel_manual_arrive: how much of a short gap was filmed)
static unsigned side_mode,side_tries,eye_clamps,ground_misses,snaps;   // side_mode HoldCam (0 not decided yet)
static float cam_uT,cam_vT,cam_yT,cam_D,ground_last;   // the side camera's aim in the pair's frame and distance
alignas(16) static float orig_eye[4],orig_q[4];        // the original camera at the decision (put back if the side camera fails)
static LARGE_INTEGER qpc_last,qpc_freq;
// the camera's projection (one run): scales, screen right = rside * cross(view, up) (0 = not read yet). abs_dead: a written rotation read
// back off once (no rotation writes for the run: the follow camera).
static bool abs_dead;
static unsigned qraw_logs;
static float cam_xs,cam_ys,cam_rside;
// local307: the side our camera sits on, cs = cam_side * (-f.z, f.x) (local298-306: always cam_rside, the pair turned around instead)
static float cam_side=1.0f;
static unsigned seat_logs,arrive_logs;
// local300 hold distance: the original's eye-to-pair distance measured at the scene's first hold, the distance in use (local301: fixed kHoldDist)
static bool orig_measured;
static float orig_dist,hold_D;
// the follow camera (the original rotation): its view forward (block), the eye we left, writes this hold, frames with the pair off view;
// fol_dead: the eye did not take (run), fol_stopped: this hold's ride ended for a lost pair
static bool fol_f_ok,fol_written,fol_dead,fol_stopped;
static float fol_f[3];
// the original view at the hold decision (its block's forward / up; local299: 30 degrees down at every hold start, the getter identity)
static bool dec_ok;
static float dec_f[3],dec_u[3];
alignas(16) static float fol_last[4];
static unsigned fol_frames,fol_off;
// the ride this hold: frames, length, each seat's height over its ground (lowest / highest)
static unsigned ride_frames;
static float ride_len,ride_dmin,ride_dmax;
// window and clicks
static Obj pane,btn[NCount];
static bool ov_mode,ov_dead,ov_seen;   // local299: the pane is the drawn window's click catcher / the overlay drew nothing (this scene) /
                                       // it drew since this show
static int gx[NCount],gy[NCount],gw[NCount],gh[NCount];
static unsigned bind_mask;
static wchar_t label[NCount][64];
static volatile LONG native_bits;
static bool was_down[2];
static int press_node[2];
static ULONGLONG last_ms[NCount];
// caption
static Obj cap_pane;
static wchar_t cap_text[128];
static bool cap_want,cap_shown;
static ULONGLONG cap_until;
static unsigned cap_builds;
// settlement (adjusted 3747A0 call)
static int *force_block;
static DWORD force_thread;
static int force_target[2];
static unsigned force_hits,onehit_skips;
static bool force_raise;

static Obj seat(Obj m,unsigned side) {return dm::slot_of(m,side);}
static unsigned seat_hp(Obj m) {return (dword(seat(m,0),0x3C)<<16)|(dword(seat(m,1),0x3C)&0xFFFF);}
static void set_state(Obj m,int state) {fn<void(__fastcall *)(Obj,int,int)>(dm::kSetState)(m,state,1);}
static int stat(Obj p,int which) {return fn<int(__fastcall *)(Obj,int,int)>(dm::kStat)(p,which,0xFFF);}
static int clamp_hp(int v) {return v<0?0:v>100?100:v;}
static int *row_of(int *b,unsigned side) {return b+sc::row_base(static_cast<int>(row_of_side[side]));}
// 37A820: a round with code 3 in either row plays as state 0xE, code 1 as 0xD, anything else 0xB (round 1 of a block here).
static int kind_of(const int *b) {
    const int a=b[sc::row_base(0)+0x11],c=b[sc::row_base(1)+0x11];
    return a==3 || c==3?0xE:a==1 || c==1?0xD:0xB;
}
// Row copy +0x48 and HP +0x3C (max +0x40 raised) of both seats from a block: HP before round 1, or the final HP.
static void seat_rows(Obj m,const int *block,bool final_hp) {
    for(unsigned s=0;s<2;++s) {
        Obj x=seat(m,s);
        const int *r=block+sc::row_base(static_cast<int>(row_of_side[s]));
        memcpy(x+0x48,r,0x58);
        const unsigned hp=static_cast<unsigned>(clamp_hp(final_hp?r[3]:r[4]));
        dm::put32(x,0x3C,hp);if(dword(x,0x40)<hp)dm::put32(x,0x40,hp);
    }
}
static void put_block(Obj m,const int *b,bool final_hp) {memcpy(m+0x60,b,0xC0);seat_rows(m,b,final_hp);}
static bool pages(Obj m) {   // 3 resets the round banner counter (+0x1B4), 4 shows the gauge again (827270), as duel_multi play()
    Obj d2=pointer(m,0x5A0),ctx=dm::ui_context();
    const bool ok=d2 && ctx && dword(d2,0)>=1 && dword(d2,0)<=4;
    if(ok) {fn<void(__fastcall *)(Obj,unsigned,Obj)>(dm::kPage)(d2,3,ctx);fn<void(__fastcall *)(Obj,unsigned,Obj)>(dm::kPage)(d2,4,ctx);}
    return ok;
}
static bool page4(Obj m) {   // what 37D1C0 calls before the next round (827790 page 4)
    Obj d2=pointer(m,0x5A0),ctx=dm::ui_context();
    const bool ok=d2 && ctx && dword(d2,0)>=1 && dword(d2,0)<=4;
    if(ok)fn<void(__fastcall *)(Obj,unsigned,Obj)>(dm::kPage)(d2,4,ctx);
    return ok;
}
static void gauge_refresh(Obj m) {   // the original gauge reads both seats through +0x180 / +0x188 (8269A0)
    Obj d2=pointer(m,0x5A0),page=d2 && dword(d2,0)==4?pointer(d2,0x30):nullptr,g=page?pointer(page,0x1C8):nullptr;
    if(g)dm::gauge_fill(g);
}
// ---- own round number and 투지 bars (local298, user 「숫자그림 1~5합 … 확장」, 「투지 … 노란색바로 하자」; drawing: native_duel_digits.inl,
// overlay row duel_digits) ----
// From the round setup (state 9) to the result (0x11) of a relay scene the gauge's round markers and digit nodes (kDigitNodes) are
// hidden every frame and duel_digits draws the round on stage over the medallion on a disc of the medallion's colour (local299: it
// covers the original digit even if its animation shows the node again between our hide and the frame; duel_manual_number3 counts
// that): the next engine round while a hold is on or due, else the block's first engine round + the round in the block (+0x4A8). The
// medallion's other nodes (0x35, 0x38-0x3B; 8269A0 shows 0x35 on every refresh, 400 x 216) stay as the original sets them; logged once a
// scene (duel_manual_number2). Place: the centre of node kNumberNode (92F050 and its size, as Layout's ui_node_rects reads a table
// node) inside kNumberBox, else kNumberX / Y. Nothing hidden (no UI scope, no gauge) or the drawing off for the process: the original
// digits stay. From the result on the digits stay hidden and nothing is drawn (the original digit would be the block's own 1-5). A
// bail or the original scene shows again the digit nodes that were showing when they were first hidden (the original's next round
// update sets its own); at the scene's end the gauge goes with the scene.
// 투지 bars: once the engine runs, while the number is drawn: under the HP bar frames (nodes kHpFrame, else kHpFrameAt), the side the
// gauge shows in that half (+0x180 left, +0x188 right); the 투지 of the round on stage (disp_morale, set with disp_hp) or, while a
// hold is on or due, the engine's; red = the player's 필살 picked in the hold's list or pending (local305).
static Obj gauge_of(Obj m) {   // the original duel gauge (page 1-4 +0x1C8, as duel_multi's bar tick)
    Obj d2=pointer(m,0x5A0);
    const unsigned page_no=d2?dword(d2,0):0u;
    Obj page=page_no>=1 && page_no<=4?pointer(d2,0x30):nullptr;
    return page?pointer(page,0x1C8):nullptr;
}
static Obj table_node(Obj g,int id) {   // table +0x130: count +0x10, nodes +8 (the caller holds SEH)
    Obj table=g+0x130;
    if(id<0 || dword(table,0x10)<=static_cast<unsigned>(id))return nullptr;
    Obj nodes=pointer(table,8);
    return nodes?pointer(nodes,static_cast<unsigned>(id)*8):nullptr;
}
static bool node_rect(Obj g,int id,int r[4]) {   // left, top, width, height in 1920 x 1080 units
    __try {
        Obj nd=table_node(g,id);
        if(!nd || !kNodePos)return false;
        const unsigned long long xy=fn<unsigned long long(__fastcall *)(Obj)>(kNodePos)(nd);
        float sx=dm::flt(nd,0xFC),sy=dm::flt(nd,0x100);
        if(!(sx>1e-3f))sx=dm::flt(nd,0xF4);
        if(!(sy>1e-3f))sy=dm::flt(nd,0xF8);
        r[0]=static_cast<int>(static_cast<unsigned>(xy));r[1]=static_cast<int>(static_cast<unsigned>(xy>>32));
        r[2]=static_cast<int>(static_cast<float>(*reinterpret_cast<const short *>(nd+0xF0))*sx);
        r[3]=static_cast<int>(static_cast<float>(*reinterpret_cast<const short *>(nd+0xF2))*sy);
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
static unsigned node_shown(Obj g,const int *ids,int n) {   // bit i = node ids[i] is not hidden (+0x104 bit 0x40, ACC350)
    unsigned mask=0;
    __try {for(int i=0;i<n && i<32;++i){Obj nd=table_node(g,ids[i]);if(nd && !(dword(nd,0x104)&0x40))mask|=1u<<i;}}
    __except(EXCEPTION_EXECUTE_HANDLER) {mask|=1u<<31;}
    return mask;
}
// duel_manual_number3 (once a scene, when the number ends): army frames a hidden digit node showed again (cap 65535), value which
// (bit per kDigitNodes entry), hex frames with a number.
static void reshow_log() {
    if(!number_logged || reshow_logged)return;
    reshow_logged=true;
    log_event("duel_manual_number3",reshow_frames<65535u?reshow_frames:65535u,reshow_mask,number_frames==~0u?0u:number_frames);
}
static void number_forget() {   // the scene (and its gauge) is gone
    const int none[2]={0,0};
    duel_digits::publish(0,0.0f,0.0f);duel_digits::publish_bars(false,none,none);duel_digits::clear_sticker();duel_digits::clear_badge();number_hidden=false;
    reshow_log();
}
// duel_manual_number: army 1 = first number of the scene | place << 4 (1 node, 2 fixed) | digit nodes hidden << 8, value the number,
// hex node left | top << 16 (1920 x 1080); army 2 = the digits given back (value 1 bail, 2 original scene), hex frames with a number |
// nodes shown again << 16; army 3 = nothing could be hidden (value page), once a scene. 8 / 9: native_duel_digits.inl.
// duel_manual_number2 (once a scene, before the first hide): army state, value nodes kMedalNodes not hidden (bit per node, bit 31 =
// fault), hex round +0x4A8.
static void number_restore(Obj m,unsigned why) {
    const int none[2]={0,0};
    duel_digits::publish(0,0.0f,0.0f);duel_digits::publish_bars(false,none,none);duel_digits::clear_badge();
    reshow_log();
    if(!number_hidden)return;
    number_hidden=false;
    unsigned back=0;
    Obj g=gauge_of(m);
    if(g) {
        layout_client::UiScope scope(dm::kOwner);
        if(scope.ok)for(int i=0;i<kDigitCount;++i)if((number_seen>>i)&1u && layout_client::ui_node_visible(g,kDigitNodes[i],1)==0)++back;
    }
    log_event("duel_manual_number",2,why,(number_frames&0xFFFF)|(back<<16));
}
static int red_cost() {   // local305: the 필살 picked in the hold's list, else the one pending for the next 합 (local306: or 퇴각)
    if(list_open==1 && pick>=0 && pick<5)return sc::kPlanCost[duel_digits::kListSpc[pick]];
    if(list_open==3 && pick==0)return sc::kPlanCost[sc::kCmdRetreat];
    return sc::plan_special(pend) || sc::plan_retreat(pend)?sc::kPlanCost[pend]:0;
}
// duel_manual_bars (once a scene): army 1 | place << 4 (1 nodes, 2 fixed) | left side << 8, value left bar x | y << 16, hex right bar
// x | y << 16 (1920 x 1080).
static void bars_step(Obj m,Obj g) {
    const int none[2]={0,0};
    if(!engine_on || !g) {duel_digits::publish_bars(false,none,none);return;}
    Obj left_seat=pointer(g,0x180);
    const unsigned left=left_seat==seat(m,1)?1u:0u;
    int fr[2][4];
    bool nodes=true;
    for(int h=0;h<2;++h) {
        int *r=fr[h];
        if(!node_rect(g,kHpFrame[h],r) || r[2]<200 || r[2]>700 || r[3]<4 || r[3]>60 || r[1]<20 || r[1]>240 || r[0]<0 || r[0]+r[2]>1920) {
            nodes=false;
            for(int k=0;k<4;++k)r[k]=kHpFrameAt[h][k];
        }
    }
    for(int h=0;h<2;++h)duel_digits::place_bar(h,static_cast<float>(fr[h][0]+1),static_cast<float>(fr[h][1]+fr[h][3]+kBarGap),
                                               static_cast<float>(fr[h][2]-2),static_cast<float>(kBarH));
    int morale[2],red[2]={0,0};
    for(int h=0;h<2;++h) {
        const unsigned s=h==0?left:1u-left;
        morale[h]=holding || hold_due?duel.side[s].morale:disp_morale[s];
        if(s==player_side)red[h]=red_cost();
    }
    duel_digits::publish_bars(true,morale,red);
    if(!bars_logged) {
        bars_logged=true;
        log_event("duel_manual_bars",1|(nodes?1u<<4:2u<<4)|(left<<8)|(left_seat!=seat(m,0) && left_seat!=seat(m,1)?1u<<12:0u),
                  (static_cast<unsigned>(fr[0][0])&0xFFFF)|((static_cast<unsigned>(fr[0][1]+fr[0][3]+kBarGap)&0xFFFF)<<16),
                  (static_cast<unsigned>(fr[1][0])&0xFFFF)|((static_cast<unsigned>(fr[1][1]+fr[1][3]+kBarGap)&0xFFFF)<<16));
    }
}
static void number_step(Obj m,unsigned state,unsigned round) {
    const int none[2]={0,0};
    if(bailed || !on) {number_restore(m,bailed?1u:2u);return;}
    if(state<9 || duel_digits::broken()) {duel_digits::publish(0,0.0f,0.0f);duel_digits::publish_bars(false,none,none);return;}
    Obj g=gauge_of(m);
    if(!g){duel_digits::publish(0,0.0f,0.0f);duel_digits::publish_bars(false,none,none);return;}
    if(!number_hidden && !number_logged) {
        number_logged=true;
        number_seen=node_shown(g,kDigitNodes,kDigitCount);
        log_event("duel_manual_number2",state,node_shown(g,kMedalNodes,static_cast<int>(sizeof kMedalNodes/sizeof kMedalNodes[0])),round);
    }
    if(number_hidden) {   // shown again since our last hide (the original's animation or refresh)
        const unsigned again=node_shown(g,kDigitNodes,kDigitCount)&~(1u<<31);
        if(again){++reshow_frames;reshow_mask|=again;}
    }
    unsigned hidden=0;
    {
        layout_client::UiScope scope(dm::kOwner);
        if(scope.ok)for(int n:kDigitNodes)if(layout_client::ui_node_visible(g,n,0)==0)++hidden;
    }
    if(!hidden) {   // the original digits stay; nothing over them
        duel_digits::publish(0,0.0f,0.0f);duel_digits::publish_bars(false,none,none);
        if(!number_hidden && number_frames!=~0u){number_frames=~0u;log_event("duel_manual_number",3,dword(pointer(m,0x5A0),0),0);}
        return;
    }
    if(number_frames==~0u)number_frames=0;
    number_hidden=true;
    if(state>=0x11){duel_digits::publish(0,0.0f,0.0f);duel_digits::publish_bars(false,none,none);return;}
    float x=kNumberX,y=kNumberY;
    int r[4]={0,0,0,0};
    bool node=node_rect(g,kNumberNode,r);
    if(node) {
        x=static_cast<float>(r[0])+static_cast<float>(r[2])*0.5f;y=static_cast<float>(r[1])+static_cast<float>(r[3])*0.5f;
        node=x>kNumberBox[0] && x<kNumberBox[2] && y>kNumberBox[1] && y<kNumberBox[3];
    }
    if(!node){x=kNumberX;y=kNumberY;}
    const int now=holding || hold_due?duel.round+1:block_base+static_cast<int>(round);
    const unsigned n=now<1?1u:now>99?99u:static_cast<unsigned>(now);
    duel_digits::publish(n,x,y);
    bars_step(m,g);
    if(!number_frames++)
        log_event("duel_manual_number",1|(node?1u<<4:2u<<4)|(hidden<<8),n,(static_cast<unsigned>(r[0])&0xFFFF)|((static_cast<unsigned>(r[1])&0xFFFF)<<16));
}
// Every HP column at hp[side], round codes 0, W/L 2/2 (a valid pair until the settlement), no 일격.
static void neutral(int *b,const int hp[2]) {
    memcpy(b,orig,sizeof orig);b[2]=0;
    for(unsigned s=0;s<2;++s) {
        int *r=row_of(b,s);
        r[0]=2;r[2]=r[3]=hp[s];
        for(int k=0;k<=kBlockRounds;++k)r[4+k]=hp[s];
        for(int k=1;k<=kBlockRounds;++k)r[0xA+k]=r[0x10+k]=0;
    }
}
static void name_of(Obj p,wchar_t *out,unsigned cap) {   // the person's name (230950, as the HUD), empty when missing
    const wchar_t *n=p && valid(p)?fn<const wchar_t *(__fastcall *)(Obj)>(dm::kName)(p):nullptr;
    unsigned i=0;
    if(n)for(;i+1<cap && n[i];++i)out[i]=n[i];
    out[i]=0;
}
static void reset() {   // per scene; the entry snapshot (orig, arg_person, saved_f1, entry_serial) belongs to entry()
    scene=nullptr;decided=on=engine_on=ended=bailed=holding=hold_due=auto_original=auto_used=final_ready=applied=applied_twice=end_capped=false;
    number_forget();block_base=0;number_frames=number_seen=reshow_frames=reshow_mask=0;number_logged=bars_logged=reshow_logged=false;
    memset(blk_morale,0,sizeof blk_morale);memset(disp_morale,0,sizeof disp_morale);
    memset(member,0,sizeof member);memset(member_f1,0,sizeof member_f1);memset(fin_person,0,sizeof fin_person);
    memset(shown,0,sizeof shown);memset(pre_hp,0,sizeof pre_hp);memset(disp_hp,0,sizeof disp_hp);memset(combo,0,sizeof combo);
    seated[0]=seated[1]=block_n=0;
    flow_on=auto_on=join_hold=hold_clicked=false;cmd_stance=s11::CmdStance2;pend=pend_who=join_cmd=join_who=-1;
    want_hold=sc::HoldGap;hold_kind=sc::HoldStart;list_open=pm_n=0;pick=-1;pm[0]=pm[1]=join_peeked[0]=join_peeked[1]=-1;
    t_round_start=t_round_end=0;setup_ms=round_ms=gap_holds=full_holds=join_holds=join_misses=auto_flows=0;
    blocks=holds=clicks=polls=natives=autos=swaps=joins=rebuilds=state_logs=records=combos=snaps=0;last_state=~0u;
    hold_frames=0;side_mode=0;side_wait=verify_due=flipped=orig_ok=cam_dead=false;cam_side=1.0f;seat_logs=arrive_logs=0;
    orig_measured=false;orig_dist=hold_D=0.0f;ov_dead=false;
    cap_want=cap_shown=false;cap_builds=0;
    native_bits=0;force_block=nullptr;force_hits=onehit_skips=0;
}

// ---- engine (duel_s11_rules.h) ----
// San11's personality (0 소심 .. 3 저돌: strategy bundle, join HP 80 .. 50) has no San14 record (docs/MANUAL_DUEL.md 4): 무력 - 지력.
static int personality(Obj p) {const int d=stat(p,1)-stat(p,2);return d>=30?3:d>=10?2:d<=-20?0:1;}
// local301 duel traits (user 「개성효과 추천안」): 374440's power share of an officer, read as the original reads it: 26C070(무장, 0x1E,
// result, 0) leaves the effect value at result +0x78 (its result is 0x200 bytes on 374440's stack), 2C7810(무장, 0x1F) and the age
// 2A8090(무장) give the age step (duel_manual_script trait_power).
static int duel_power(Obj p) {
    alignas(16) unsigned char res[0x400]={};
    fn<void(__fastcall *)(Obj,int,void *,int)>(GA(GA_26C070))(p,0x1E,res,0);
    int e=0;memcpy(&e,res+0x78,sizeof e);
    const bool has=fn<int(__fastcall *)(Obj,int)>(GA(GA_2C7810))(p,0x1F)!=0;
    return sc::trait_power(e,has,has?fn<int(__fastcall *)(Obj)>(GA(GA_2A8090))(p):0);
}
// Roster: slot 0 the seated duelist (start HP = the original block's row +2, the +0xF1 before 376020's 3747A0), slots 1-2 duel_multi's
// extras of this scene that ride a slot with a model (their own +0xF1). Relations stay at the engine's defaults (JoinRel all 0: any
// bench fighter may step in from 6 landed hits on with 37 %, gated by its HP and personality); the San14 relation map waits for the
// user (docs/MANUAL_DUEL.md 4). duel_manual_fighter (per member): army side | slot << 4 | personality << 8 | injury << 12 | player
// side << 16, value id | 무력 << 16, hex start HP.
static bool eng_start(Obj m) {
    duel=s11::Duel{};
    duel_digits::clear_badge();   // local313: a 연장 badge belongs to one duel
    const bool extras=dm::alive && dm::scene_manager==m;
    for(unsigned s=0;s<2;++s) {
        member[s][0]=pointer(seat(m,s),0xB0);
        member[s][1]=extras?dm::picked[0][s]:nullptr;member[s][2]=extras?dm::picked[1][s]:nullptr;
        for(unsigned i=1;i<kMembers;++i)if(member[s][i] && !dm::holder(m,s,member[s][i]))member[s][i]=nullptr;
        if(!member[s][1]){member[s][1]=member[s][2];member[s][2]=nullptr;}
        s11::Side &sd=duel.side[s];
        sd.count=0;
        for(unsigned i=0;i<kMembers;++i) {
            Obj p=member[s][i];
            if(!p)break;
            const int hp=clamp_hp(i?static_cast<int>(static_cast<unsigned char>(p[0xF1])):orig[sc::row_base(static_cast<int>(row_of_side[s]))+2]);
            const unsigned injury=static_cast<unsigned char>(p[0x161]);
            s11::Fighter &f=sd.f[i];
            f.id=static_cast<int>(dm::id_of(p));f.str=stat(p,1);f.hp=hp;f.injury=injury>3?3:static_cast<int>(injury);f.personality=personality(p);
            f.power=duel_power(p);
            member_f1[s][i]=static_cast<unsigned char>(hp);
            ++sd.count;
            log_event("duel_manual_fighter",s|(i<<4)|(static_cast<unsigned>(f.personality)<<8)|(static_cast<unsigned>(f.injury)<<12)|(player_side<<16),
                      dm::id_of(p)|(static_cast<unsigned>(f.str)<<16),static_cast<unsigned>(hp)|(static_cast<unsigned>(f.power)<<8));
        }
        seated[s]=0;shown[s][0]=true;
    }
    if(!duel.side[0].count || !duel.side[1].count)return false;
    {   // local301 first pair's entry cut (duel_manual_script entry_cut); +0xF1 / member_f1 keep the entry HP (settlement, bail)
        const int a=duel.side[0].f[0].str,b=duel.side[1].f[0].str;
        if(a!=b) {
            const unsigned weak=a<b?0u:1u;
            s11::Fighter &f=duel.side[weak].f[0];
            const int before=f.hp,cut=sc::entry_cut(weak?a:b,weak?b:a);
            f.hp=sc::entry_hp(before,cut);
            log_event("duel_manual_entry",weak|(static_cast<unsigned>(cut&0xFF)<<8),static_cast<unsigned>(before)|(static_cast<unsigned>(f.hp)<<8),
                      static_cast<unsigned>(a)|(static_cast<unsigned>(b)<<8));
        }
    }
    duel.cfg.drop_swap_on_join=true;   // local298: a 교대 queued for a join 합 is dropped
    duel.cfg.join_cut_den=sc::kEntryDen;   // local303 (user 「무장이 난입할때도 적용해주면 안되나? 교대할때는 적용X」): a join's pair, the entry cut's rule
    duel.cfg.swap_limit=1;   // local305 (user 「교체는 1회로 제한하자」, 1A): one switch per side a duel, the computer's too; joins do not count
    duel.cfg.dmg_num=duel.cfg.dmg_den=1;   // local305 (3A): non-special damage as San11 (local298-304: 2/3)
    duel.cfg.retreat=true;   // local306 (「1B 2A 3A 4A」): San11's retreat for both sides (투지 100), end HP by retreat_end
    {   // local312: the 합 cap by the scene's fighters and its one extension from 4 (duel_manual_script relay_round_cap; local298-311: 30).
        // duel_manual_cap: army fighters, value cap, hex extension 합 | percent << 8
        const int fighters=duel.side[0].count+duel.side[1].count;
        duel.cfg.round_cap=sc::relay_round_cap(fighters);duel.cfg.round_ext=sc::relay_round_ext(fighters);duel.cfg.round_ext_pct=sc::kRoundExtPct;
        log_event("duel_manual_cap",static_cast<unsigned>(fighters),static_cast<unsigned>(duel.cfg.round_cap),
                  static_cast<unsigned>(duel.cfg.round_ext)|(static_cast<unsigned>(duel.cfg.round_ext_pct)<<8));
    }
    s11::start(duel,static_cast<std::uint32_t>(GetTickCount64())^(dm::scene_serial*0x9E3779B9u));
    cmd_stance=duel.side[player_side].stance&3;   // local305: the stance kept from the start (the engine's start stance)
    for(unsigned s=0;s<2;++s)for(unsigned i=0;i<kMembers;++i)disp_hp[s][i]=duel.side[s].f[i].hp;
    engine_on=true;
    return true;
}
// local303 duel_manual_join_cut (a join's 무력-gap cut, s11 join_cut): army side cut | slot << 4 | joined sides << 8 (bit 0 side 0, bit
// 1 side 1) | round << 12, value HP before | cut << 8, hex the cut fighter's 무력.
static void eng_round(const int cmd[2],Round &o) {
    int pre[2][kMembers];
    for(unsigned s=0;s<2;++s)for(unsigned i=0;i<kMembers;++i)pre[s][i]=duel.side[s].f[i].hp;
    o.r=s11::step(duel,cmd);
    for(unsigned s=0;s<2;++s) {
        for(unsigned i=0;i<kMembers;++i)o.hp[s][i]=duel.side[s].f[i].hp;
        const s11::SideResult &sr=o.r.side[s];
        const int a=sr.active;
        // a joiner the join cut hit comes in at its HP after the cut (the opponent's cut shows with the round's damage)
        o.hp_in[s]=a>=0 && a<kMembers?pre[s][a]-(sr.join_in==a?sr.join_cut:0):sr.hp;
        if(sr.join_cut>0) {
            const int slot=sr.swap_out>=0?sr.swap_out:a;
            const unsigned joined=(o.r.side[0].join_in>=0?1u:0u)|(o.r.side[1].join_in>=0?2u:0u);
            log_event("duel_manual_join_cut",s|((static_cast<unsigned>(slot)&0xF)<<4)|(joined<<8)|((static_cast<unsigned>(duel.round)&0xFF)<<12),
                      (slot>=0 && slot<kMembers?static_cast<unsigned>(pre[s][slot])&0xFF:0u)|(static_cast<unsigned>(sr.join_cut&0xFF)<<8),
                      slot>=0 && slot<kMembers?static_cast<unsigned>(duel.side[s].f[slot].str):0u);
        }
    }
}
// duel_manual_cmd (per 합): army engine round | player command << 8 (0xF computer) | special side 0 << 12 | side 1 << 16 (0xF none) |
// failed side 0 << 20 | side 1 << 21 | swap side 0 << 22 | side 1 << 23 | join side 0 << 24 | side 1 << 25 | finished << 26, value
// damage side 0 | side 1 << 8 | stance side 0 << 16 | side 1 << 20 | fighter side 0 << 24 | side 1 << 28, hex HP side 0 | side 1 << 8 |
// 투지 side 0 / 2 << 16 | side 1 / 2 << 24.
static int play_round(int cmd,int who) {   // local305: the engine's next 합 into rounds[0] (cmd -1 = the computer); 1, or 0 after the end
    for(unsigned s=0;s<2;++s)for(unsigned i=0;i<kMembers;++i)pre_hp[s][i]=duel.side[s].f[i].hp;
    if(duel.finished)return 0;
    int c[2]={s11::CmdAi,s11::CmdAi};
    c[player_side]=cmd;
    if(cmd==s11::CmdSwap)duel.swap_pick[player_side]=who;
    Round &R=rounds[0];
    eng_round(c,R);
    const s11::SideResult &a=R.r.side[0],&b=R.r.side[1];
    auto sp=[](int v){return v<0?0xFu:static_cast<unsigned>(v)&0xFu;};
    log_event("duel_manual_cmd",(static_cast<unsigned>(duel.round)&0xFF)|((cmd<0?0xFu:static_cast<unsigned>(cmd)&0xFu)<<8)|(sp(a.special)<<12)|(sp(b.special)<<16)|
                  (a.special_failed?1u<<20:0u)|(b.special_failed?1u<<21:0u)|(a.swap_in>=0?1u<<22:0u)|(b.swap_in>=0?1u<<23:0u)|
                  (a.join_in>=0?1u<<24:0u)|(b.join_in>=0?1u<<25:0u)|(R.r.finished?1u<<26:0u),
              (static_cast<unsigned>(a.damage)&0xFF)|((static_cast<unsigned>(b.damage)&0xFF)<<8)|((static_cast<unsigned>(a.stance)&0xF)<<16)|
                  ((static_cast<unsigned>(b.stance)&0xF)<<20)|((static_cast<unsigned>(a.active)&0xF)<<24)|((static_cast<unsigned>(b.active)&0xF)<<28),
              (static_cast<unsigned>(a.hp)&0xFF)|((static_cast<unsigned>(b.hp)&0xFF)<<8)|((static_cast<unsigned>(a.morale/2)&0xFF)<<16)|((static_cast<unsigned>(b.morale/2)&0xFF)<<24));
    // local306 duel_manual_crit (a critical move, San11 0x509480): army side | move << 4 (5 연속 공격, 6 자동 견수, 7 투지 상승) | engine
    // round << 8, value damage dealt | hits << 8 | landed << 12, hex guard side 0 | side 1 << 8 | the side's 투지 / 2 << 16.
    // duel_manual_retreat: army side | played by the computer << 4 | winner << 8 (2 draw) | engine round << 12, value HP before | HP
    // after << 8 | cut << 16, hex the other fighter's HP | the side's 투지 / 2 after << 8.
    for(unsigned s=0;s<2;++s) {
        const s11::SideResult &sr=R.r.side[s];
        if(sr.move>=5 && sr.move<=7)
            log_event("duel_manual_crit",s|(static_cast<unsigned>(sr.move)<<4)|((static_cast<unsigned>(duel.round)&0xFF)<<8),
                      (static_cast<unsigned>(sr.damage)&0xFF)|((static_cast<unsigned>(sr.hits)&0xF)<<8)|((static_cast<unsigned>(sr.landed)&0xF)<<12),
                      (static_cast<unsigned>(duel.side[0].guard)&0xFF)|((static_cast<unsigned>(duel.side[1].guard)&0xFF)<<8)|((static_cast<unsigned>(duel.side[s].morale/2)&0xFF)<<16));
        if(sr.special==3) {
            const int a0=sr.active>=0 && sr.active<kMembers?sr.active:0;
            log_event("duel_manual_retreat",s|(c[s]==s11::CmdAi?1u<<4:0u)|((static_cast<unsigned>(duel.winner)&0xF)<<8)|((static_cast<unsigned>(duel.round)&0xFF)<<12),
                      (static_cast<unsigned>(pre_hp[s][a0])&0xFF)|((static_cast<unsigned>(sr.hp)&0xFF)<<8)|((static_cast<unsigned>(sr.retreat_cut)&0xFF)<<16),
                      (static_cast<unsigned>(R.r.side[s^1u].hp)&0xFF)|((static_cast<unsigned>(sr.morale/2)&0xFF)<<8));
        }
    }
    return 1;
}
// The block of rounds[0..n): per side the fighter of each round (HP before round 1 = the round-1 fighter's), damage dealt, codes
// (special presentation: duel_manual_script special_codes). Rounds after the end of the duel keep the last HP with no damage. A 무쌍
// that dealt damage is split into 회심 hits (musou_split): the block holds the first one, the round end replays the rest (combo_next).
static void build_block(int n) {
    memcpy(blk,orig,sizeof blk);blk[2]=0;
    memset(combo,0,sizeof combo);
    int code[kBlockRounds][2]={};
    for(int k=0;k<n;++k) {
        const s11::SideResult &a=rounds[k].r.side[0],&b=rounds[k].r.side[1];
        const int sp[2]={a.special,b.special},dmg[2]={a.damage,b.damage};
        code[k][0]=a.code;code[k][1]=b.code;
        sc::special_codes(sp,dmg,code[k]);
    }
    for(unsigned s=0;s<2;++s) {
        int *r=row_of(blk,s);
        const int before=clamp_hp(rounds[0].hp_in[s]);
        r[0]=2;r[2]=r[4]=before;
        int hp=before;
        for(int k=1;k<=kBlockRounds;++k) {
            const bool played=k<=n;
            if(played)hp=clamp_hp(rounds[k-1].r.side[s].hp);
            r[4+k]=hp;
            r[0xA+k]=played?rounds[k-1].r.side[s].damage:0;
            r[0x10+k]=played?code[k-1][s]:0;
        }
        r[3]=hp;
    }
    for(int k=1;k<=n;++k)for(unsigned s=0;s<2;++s) {
        const s11::SideResult &sr=rounds[k-1].r.side[s];
        if(sr.special!=5 || sr.damage<=0)continue;
        const unsigned o=s^1u;
        int *ro=row_of(blk,o),*rs=row_of(blk,s);
        Combo &c=combo[k-1];
        c.n=sc::musou_split(clamp_hp(rounds[k-1].hp_in[o]),ro[4+k],c.seq);   // the receiver's own HP before the round (a swap-in's too)
        if(c.n<2){c.n=0;continue;}
        c.side=static_cast<int>(o);c.i=1;
        ro[4+k]=c.seq[1];rs[0xA+k]=c.seq[0]-c.seq[1];
    }
}

// ---- settlement ----
// Every +0xF1 the duel changes is written by the original 3747A0 (SESSION_RULES 2: no direct writes for effects); the mod only puts
// back the pre-duel value 3747A0 reads as its start HP (the value 376020's own call replaced, as bail() does) and forces the damage
// columns of its own buffer `fin` (374A22 hook, force_damage). One adjusted call: rows p0 / p1, all damage forced so the call ends on
// t0 / t1; raise: a row whose start HP is below its target starts at the target (the bench draw's partner). False on a fault.
static bool calc_pair(Obj m,Obj p0,Obj p1,int t0,int t1,bool raise) {
    memcpy(fin_arg,m+0x20,sizeof fin_arg);
    *reinterpret_cast<Obj *>(fin_arg)=p0;*reinterpret_cast<Obj *>(fin_arg+8)=p1;
    force_target[0]=t0;force_target[1]=t1;force_raise=raise;
    memset(fin,0,sizeof fin);
    force_hits=onehit_skips=0;force_thread=GetCurrentThreadId();force_block=fin;
    bool fault=false;
    __try {fn<int *(__fastcall *)(int *,Obj)>(dm::kCalc)(fin,reinterpret_cast<Obj>(fin_arg));}
    __except(EXCEPTION_EXECUTE_HANDLER) {fault=true;}
    force_block=nullptr;force_raise=false;
    return !fault;
}
// Once, when the engine finished. First every member who fought and was swapped out (engine state 1): a forced draw against the
// other side's final fighter, so 3747A0 writes the engine HP into their +0xF1 and returns before 375340 (a draw: no death or capture
// roll, 3747A0 decompiled 2026-11-22). Then the final pair (which rewrites the partner's +0xF1). duel_manual_bench (per bench call):
// army side | slot << 4 | ok << 8 | fault << 12, value id | partner id << 16, hex start HP | target << 8 | +0xF1 after << 16.
// duel_manual_settle: army W/L match | final HP match << 1 | +0xF1 match << 2 | fault << 4 | forced columns << 8 | 일격 rolls skipped
// << 12 | winner << 16 (2 draw) | end reason << 20, value final HP row 0 | row 1 << 8 | +0xF1 row 0 << 16 | row 1 << 24, hex W/L row 0
// | row 1 << 4 | fate << 8 | outcome [0] << 16 | final pair changed << 24.
static bool settle(Obj m) {
    const int b0=sc::row_base(0),b1=sc::row_base(1);
    for(unsigned s=0;s<2;++s)for(unsigned i=0;i<kMembers;++i) {
        Obj p=member[s][i];
        if(!p || static_cast<int>(i)==duel.side[s].active || duel.side[s].f[i].state!=1)continue;
        const unsigned o=1-s;
        const int oa=duel.side[o].active,t=clamp_hp(duel.side[s].f[i].hp);
        Obj q=member[o][oa];
        if(!q)return false;
        p[0xF1]=member_f1[s][i];q[0xF1]=member_f1[o][oa];
        const bool first=row_of_side[s]==0;
        const bool called=calc_pair(m,first?p:q,first?q:p,t,t,true);
        const unsigned after=static_cast<unsigned char>(p[0xF1]);
        const bool ok=called && fin[b0]==2 && fin[b1]==2 && fin[b0+3]==t && fin[b1+3]==t && static_cast<int>(after)==t && !fin[2] && force_hits;
        log_event("duel_manual_bench",s|(i<<4)|(ok?1u<<8:0u)|(called?0u:1u<<12),dm::id_of(p)|(dm::id_of(q)<<16),
                  static_cast<unsigned>(member_f1[s][i])|(static_cast<unsigned>(t)<<8)|(after<<16));
        if(!ok)return false;
    }
    int want_wl[2]={2,2},target[2]={0,0};
    for(unsigned s=0;s<2;++s) {
        const int a=duel.side[s].active;
        Obj p=member[s][a];
        const unsigned row=row_of_side[s];
        fin_person[row]=p;
        target[row]=clamp_hp(duel.side[s].f[a].hp);
        p[0xF1]=member_f1[s][a];
        if(duel.winner==0 || duel.winner==1)want_wl[row]=static_cast<int>(s)==duel.winner?0:1;
    }
    const bool fault=!calc_pair(m,fin_person[0],fin_person[1],target[0],target[1],false);
    const unsigned f0=static_cast<unsigned char>(fin_person[0][0xF1]),f1=static_cast<unsigned char>(fin_person[1][0xF1]);
    const bool wl=fin[b0]==want_wl[0] && fin[b1]==want_wl[1];
    const bool hp=fin[b0+3]==force_target[0] && fin[b1+3]==force_target[1];
    const bool f1ok=static_cast<int>(f0)==force_target[0] && static_cast<int>(f1)==force_target[1];
    const bool changed=fin_person[0]!=arg_person[0] || fin_person[1]!=arg_person[1];
    log_event("duel_manual_settle",(wl?1u:0u)|(hp?2u:0u)|(f1ok?4u:0u)|(fault?0x10u:0u)|((force_hits&0xF)<<8)|((onehit_skips&0xF)<<12)|
                  ((static_cast<unsigned>(duel.winner)&0xF)<<16)|((static_cast<unsigned>(duel.end_reason)&0xF)<<20),
              (static_cast<unsigned>(fin[b0+3])&0xFF)|((static_cast<unsigned>(fin[b1+3])&0xFF)<<8)|((f0&0xFF)<<16)|((f1&0xFF)<<24),
              (fin[b0]&0xF)|((fin[b1]&0xF)<<4)|((fin[3]&0xFF)<<8)|((fin[0]&0xFF)<<16)|(changed?1u<<24:0u));
    if(fault || !wl || !hp || !f1ok || fin[2] || !force_hits)return false;
    blk[0]=fin[0];blk[1]=fin[1];blk[3]=fin[3];
    for(unsigned row=0;row<2;++row) {
        int *r=blk+sc::row_base(static_cast<int>(row));const int *f=fin+sc::row_base(static_cast<int>(row));
        r[0]=f[0];r[2]=f[2];r[3]=f[3];
    }
    final_ready=true;
    return true;
}

// ---- window (Layout, owner duel_multi; local305: built at the first hold, up to the scene end, released then) ----
static void __cdecl clicked_cb(void *,void *ctx) {InterlockedOr(&native_bits,1L<<static_cast<int>(reinterpret_cast<intptr_t>(ctx)));}
static Desc make_desc(int node,int type,int x,int y,int w,int h,int id=0,const wchar_t *text=nullptr) {
    gx[node]=x;gy[node]=y;gw[node]=w;gh[node]=h;
    Desc d{};d.type=type;d.x=x;d.y=y;d.w=w;d.h=h;d.id=id;d.text=text;d.minus1=-1;return d;
}
// local305: a 필살 the engine allows now (legal() on the duel as it stands: the strip's 필살 and the list's places; one offered never
// fails), any of them.
static bool special_ok(int c) {return sc::plan_special(c) && s11::legal(duel,static_cast<int>(player_side),s11::command_action(c));}
static bool any_special() {for(int c=s11::CmdSpecial1;c<=s11::CmdSpecial3;++c)if(special_ok(c))return true;return false;}
// local301 / local305 (user 「교대하는 무장을 선택할 수 있었으면 좋겠어」, 「교체는 1회로 제한하자」): the members a 교대 may bring in now —
// waiting members who already fought (state 1, the San11 rule) with HP left — none once the side's switch is used.
static int swap_cands(int out[3]) {
    out[0]=out[1]=out[2]=-1;
    if(!s11::swap_left(duel,static_cast<int>(player_side)))return 0;
    const s11::Side &me=duel.side[player_side];
    sc::SwapView v;
    v.count=me.count;v.active=me.active;
    for(int i=0;i<3;++i){v.hp[i]=me.f[i].hp;v.state[i]=me.f[i].state;}
    return sc::swap_candidates(v,out);
}
// The strip's 필살 / 교대 (duel_manual_script special_button / swap_button): one pressed during a 합 shows as pending.
static int spc_state() {return sc::special_button(any_special(),want_hold==sc::HoldSpecial?static_cast<int>(s11::CmdSpecial1):pend);}
static int swp_state() {
    int c[3];
    return sc::swap_button(s11::swap_left(duel,static_cast<int>(player_side)),swap_cands(c),want_hold==sc::HoldSwap?static_cast<int>(s11::CmdSwap):pend);
}
// local306 (「2A」, 「4A」): 퇴각 the engine allows now (Config::retreat, 투지 100), and the 교대 place's 퇴각 state
// (duel_manual_script retreat_button: 0 the 교대 plate, 1 on, 2 pending, 3 short of 투지).
static bool retreat_ok() {return s11::legal(duel,static_cast<int>(player_side),3);}
static int ret_state() {return sc::retreat_button(swp_state(),retreat_ok(),want_hold==sc::HoldRetreat?sc::kCmdRetreat:pend);}
static bool full_hold() {return holding && hold_kind!=sc::HoldGap;}
// This hold's list: 1 the 필살 list (a pending 필살 picked), 2 the 교대 members (a lone member, or the pending one, picked), 3 퇴각
// (picked when the engine allows it: the hold was opened for it).
static void open_list(int which) {
    list_open=which;pick=-1;pm_n=0;pm[0]=pm[1]=-1;
    if(which==3) {if(retreat_ok())pick=0;return;}
    if(which==1) {for(int k=0;k<5;++k)if(duel_digits::kListSpc[k]==pend && special_ok(pend))pick=k;return;}
    int c[3];
    const int n=swap_cands(c);
    for(int j=0;j<n && pm_n<2;++j)pm[pm_n++]=c[j];
    if(pm_n==1)pick=0;
    for(int j=0;j<pm_n;++j)if(pend==s11::CmdSwap && pm[j]==pend_who)pick=j;
}
static void view_state() {   // every node's label (the plain window), visible and enabled
    const s11::Side &me=duel.side[player_side];
    const bool full=full_hold();
    const int spc=spc_state(),swp=swp_state(),ret=ret_state();
    // local298 (user 「투지 … 글자만 지워주고 노란색바로 하자」): the 투지 is the yellow bar under the HP bar (duel_digits); the text cells
    // come back only when that overlay is off for the process.
    // local306: the drawn window's status line as text (the plain window): the critical's charge, both guard timers
    wchar_t stat[64],part[24];
    const int crit=sc::crit_view(me.stance,me.charge),guard=sc::guard_view(me.guard),foe=sc::guard_view(duel.side[player_side^1u].guard);
    if(crit>=6)swprintf_s(stat,L" · 크리티컬 가능");
    else swprintf_s(stat,L" · 크리티컬 %d/6",crit);
    if(guard>0){swprintf_s(part,L" · 견수 %d합",guard);wcsncat_s(stat,part,_TRUNCATE);}
    if(foe>0){swprintf_s(part,L" · 적 견수 %d합",foe);wcsncat_s(stat,part,_TRUNCATE);}
    if(duel_digits::broken()) {
        if(full)swprintf_s(label[NInfo],L"%d합 · 지금 %ls%ls",duel.round+1,kCmdName[me.stance&3],stat);
        else swprintf_s(label[NInfo],L"지금 %ls%ls",kCmdName[me.stance&3],stat);
        sc::morale_cells(me.morale,red_cost(),label[NGauge],64);
    }
    else {
        if(full)swprintf_s(label[NInfo],L"%d합",duel.round+1);
        else swprintf_s(label[NInfo],L" ");
        swprintf_s(label[NGauge],L"지금 %ls%ls",kCmdName[me.stance&3],stat);
    }
    for(int i=0;i<4;++i){const int n=NSt0+i;const bool on_now=i==cmd_stance;swprintf_s(label[n],L"%ls%ls%ls",on_now?L"[":L"",kCmdName[i],on_now?L"]":L"");vis[n]=en[n]=true;}
    swprintf_s(label[NSpcBtn],L"%ls",spc==2?L"필살 (대기)":L"필살");vis[NSpcBtn]=true;en[NSpcBtn]=spc!=0;
    if(ret) {swprintf_s(label[NSwapBtn],L"%ls",ret==2?L"퇴각 (대기)":ret==3?L"퇴각 (투지 부족)":L"퇴각");vis[NSwapBtn]=true;en[NSwapBtn]=ret!=3;}
    else {
        swprintf_s(label[NSwapBtn],L"%ls",swp==2?L"교대 (대기)":swp==3?L"교대 사용함":swp==0?L"교대 (대기 무장 없음)":L"교대");
        vis[NSwapBtn]=true;en[NSwapBtn]=swp==1 || swp==2;
    }
    swprintf_s(label[NAutoBtn],L"%ls",auto_on?L"자동: 켬":L"자동: 끔");vis[NAutoBtn]=en[NAutoBtn]=true;
    swprintf_s(label[NOk],L"확정");vis[NOk]=en[NOk]=full;
    for(int k=0;k<6;++k) {
        const int n=NList+k;
        bool v=false,e=false;
        swprintf_s(label[n],L" ");
        if(full && list_open==1) {
            const int c=duel_digits::kListSpc[k];
            if(k==5)swprintf_s(label[n],L"돌아가기");
            else swprintf_s(label[n],L"%ls%ls%ls",pick==k?L"[":L"",kCmdName[c],pick==k?L"]":L"");
            v=true;e=k==5 || special_ok(c);
        }
        else if(full && list_open==3) {   // local306 the 퇴각 hold
            if(k==0){swprintf_s(label[n],L"%ls퇴각%ls",pick==0?L"[":L"",pick==0?L"]":L"");v=true;e=retreat_ok();}
            else if(k==5){swprintf_s(label[n],L"돌아가기");v=e=true;}
        }
        else if(full && list_open==2) {
            if(k<pm_n) {
                wchar_t who[40];name_of(member[player_side][pm[k]],who,40);
                swprintf_s(label[n],L"%ls%ls (체력 %d)%ls",pick==k?L"[":L"",who,me.f[pm[k]].hp,pick==k?L"]":L"");
                v=e=true;
            }
            else if(k==5){swprintf_s(label[n],L"돌아가기");v=e=true;}
        }
        vis[n]=v;en[n]=e;
    }
    vis[NInfo]=vis[NGauge]=true;en[NInfo]=en[NGauge]=false;
}
// ---- the window the mod draws (local299; local305 strip and list) ----
// duel_digits draws the window on Present (native_duel_digits.inl draw_window: the strip — four stances, 필살, 교대, 자동, 「확정」 in a
// full hold — and a full hold's list over it; strip_rect / list_rect) from the state publish_view sends. Clicks: this button window
// built at the same place (pane = duel_digits kWin*, the whole hold panel, never resized: a hold never rebuilds it) with every button
// inside its plate (kOvInsetX / Y in from the plate's torn edge: the plate is opaque there and the button never shows), blank labels, a
// disabled button hidden; the poll takes the whole plate (gx..gh), the native callback the button. The overlay drew nothing for
// kOvFrames focused frames and kOvMs after a show: the pane is hidden (the owner's release at the scene end frees it), ov_dead, and the
// plain button window is built for the rest of the scene; its build failed or a draw faulted (duel_digits flags for the process): the
// same, for the process.
static const int kOvInsetX=20,kOvInsetY=7;
static const unsigned kOvFrames=30,kOvMs=2000;
static LONG ov_draws;        // duel_digits window_draws at the show
static unsigned ov_wait;     // focused frames since the show without a drawn window
static ULONGLONG ov_since;   // the wait's start (again after the game lost the focus: Present may stop then)
static int pane_x=kPaneX,pane_y=kPaneY;
static void ov_rect(int n,int r[4]) {   // node n's plate in pane coordinates (NInfo / NGauge: a blank 4 x 4 text cell)
    namespace dd=duel_digits;
    if(n>=NSt0 && n<=NOk)dd::strip_rect(n-NSt0,r);
    else if(n>=NList && n<NList+6)dd::list_rect(n-NList,r);
    else {r[0]=r[1]=0;r[2]=r[3]=4;return;}
    r[0]-=dd::kWinX;r[1]-=dd::kWinY;
}
static void publish_view() {   // the drawn window's state (local305 WinView: the strip, a full hold's kind, list, pick and members)
    namespace dd=duel_digits;
    const s11::Side &me=duel.side[player_side];
    dd::NameSet ns{};   // local301: the player's members (names, HP) for the 교대 list
    for(int i=0;i<kMembers && i<3;++i){name_of(member[player_side][i],ns.name[i],dd::kNameCap);ns.hp[i]=i<me.count?me.f[i].hp:0;}
    dd::publish_names(ns);
    dd::WinView v;
    const bool full=full_hold();
    v.mode=full?hold_kind:0;v.list=full?list_open:0;v.next=full?duel.round+1:0;
    v.now=me.stance&3;v.stance=cmd_stance&3;v.spc=spc_state();v.swp=swp_state();v.autoplay=auto_on;v.ok=full;
    for(int k=0;k<5;++k)v.item[k]=v.list==1?special_ok(dd::kListSpc[k]):v.list==2?k<pm_n:v.list==3?k==0 && retreat_ok():false;
    // local306: 퇴각 on the 교대 place; the status line (the critical's charge, both guard timers)
    v.ret=ret_state();
    v.stat=true;v.crit=sc::crit_view(me.stance,me.charge);v.guard=sc::guard_view(me.guard);v.foe=sc::guard_view(duel.side[player_side^1u].guard);
    v.pick=v.list?pick:-1;
    for(int j=0;j<2;++j)v.member[j]=v.list==2 && j<pm_n?pm[j]:-1;
    dd::publish_window(dd::window_state(v));
}
static void push_locked() {   // labels, visibility and enable state to the controls (UI scope held by the caller)
    if(!ov_mode) {
        layout_client::ui_node_text(pane,NInfo,label[NInfo]);
        layout_client::ui_node_text(pane,NGauge,label[NGauge]);
    }
    for(int n=NSt0;n<NCount;++n) {
        if(!btn[n])continue;
        if(!ov_mode)layout_client::ui_text(btn[n],label[n]);
        layout_client::ui_show(btn[n],vis[n] && (en[n] || !ov_mode)?1:0);
        layout_client::ui_enable(btn[n],en[n]?1:0);
    }
    if(ov_mode)publish_view();
}
static void push_view() {
    view_state();
    if(!pane)return;
    layout_client::UiScope scope(dm::kOwner);
    if(scope.ok)push_locked();
}
static void owner_released() {   // duel_multi bar_release took every pane of the owner: window and caption come back when needed
    pane=nullptr;memset(btn,0,sizeof btn);ov_mode=false;duel_digits::publish_window(0);
    cap_pane=nullptr;
    if(cap_shown && GetTickCount64()<cap_until)cap_want=true;
    cap_shown=false;
}
// duel_manual_window_release: army why (1 hold end, 2 scene end, 3 build failed, 9 bail), value panes released (-1 no scope), hex block.
static void release_window(unsigned why) {
    if(!pane && !cap_pane)return;
    if(dm::bar_any()) {dm::bar_release(0x20|why);owner_released();cap_want=false;return;}   // owner-wide: takes ours too
    layout_client::UiScope scope(dm::kOwner);
    const int n=scope.ok?layout_client::ui_release(dm::kOwner,dm::ui_context()):-1;
    log_event("duel_manual_window_release",why,static_cast<unsigned>(n),blocks);
    owner_released();cap_want=false;
}
// At the scene end with a caption still due the window is only hidden (duel_manual_window_hide: army why, value ui_show result, hex
// block); a failed hide releases.
static void hide_window(unsigned why) {
    duel_digits::publish_window(0);
    if(!pane)return;
    int r=-1;
    {
        layout_client::UiScope scope(dm::kOwner);
        if(scope.ok)r=layout_client::ui_show(pane,0);
    }
    log_event("duel_manual_window_hide",why,static_cast<unsigned>(r),blocks);
    if(r<0)release_window(why);
}
// duel_manual_window: army 0 built / shown again | why << 4 (1 no UI scope, 3 window, 4 buttons, 5 activate, 6 show) | block << 8 |
// rebuild << 12 | reused << 16 | drawn window (click catcher) << 20, value pane flags +0x40, hex native clicks bound (bit per node).
static unsigned build_window() {
    layout_client::UiScope scope(dm::kOwner);
    unsigned why=0;
    const bool reused=pane!=nullptr;
    view_state();
    if(ov_mode || (!reused && !ov_dead && !duel_digits::broken() && !duel_digits::window_broken()))
        {ov_seen=false;ov_wait=0;ov_since=GetTickCount64();ov_draws=duel_digits::window_draws();}
    if(!scope.ok)why=1;
    else if(reused) {push_locked();if(layout_client::ui_show(pane,1)<0)why=6;}
    else {
        namespace dd=duel_digits;
        ov_mode=!ov_dead && !dd::broken() && !dd::window_broken();
        pane_x=ov_mode?dd::kWinX:kPaneX;pane_y=ov_mode?dd::kWinY:kPaneY;
        bind_mask=0;
        Desc d[NCount];
        if(ov_mode)for(int n=0;n<NCount;++n) {   // buttons inside their plates; the poll takes the whole plate
            int r[4];ov_rect(n,r);
            if(n<NSt0)d[n]=make_desc(n,1,r[0],r[1],r[2],r[3],0,L" ");
            else {d[n]=make_desc(n,2,r[0]+kOvInsetX,r[1]+kOvInsetY,r[2]-2*kOvInsetX,r[3]-2*kOvInsetY,kImageId);gx[n]=r[0];gy[n]=r[1];gw[n]=r[2];gh[n]=r[3];}
        }
        else {
            d[NInfo]=make_desc(NInfo,1,0,0,kPaneW,kTextH,0,L" ");
            d[NGauge]=make_desc(NGauge,1,0,kTextH,kPaneW,kTextH,0,L" ");
            for(int k=0;k<6;++k)d[NList+k]=make_desc(NList+k,2,(k/3)*(kColW+kGap),kListTop+(k%3)*(kBtnH+kGap),kColW,kBtnH,kImageId);
            for(int i=0;i<8;++i)d[NSt0+i]=make_desc(NSt0+i,2,(i/4)*(kColW+kGap),kStripTop+(i%4)*(kBtnH+kGap),kColW,kBtnH,kImageId);
        }
        pane=static_cast<Obj>(layout_client::ui_window(nullptr,pane_x,pane_y,ov_mode?dd::kWinW:kPaneW,ov_mode?dd::kWinH:kPaneH,d,NCount));
        if(!pane)why=3;
        else {
            for(int n=NSt0;n<NCount && !why;++n) {
                btn[n]=static_cast<Obj>(layout_client::ui_button(pane,n,3,0));
                if(!btn[n]){why=4;break;}
                if(layout_client::ui_click(btn[n],&clicked_cb,reinterpret_cast<void *>(static_cast<intptr_t>(n)))==0)bind_mask|=1u<<n;
            }
            if(!why) {
                push_locked();
                if(layout_client::ui_activate(pane)<0)why=5;
            }
        }
    }
    log_event("duel_manual_window",(why<<4)|((blocks&0xF)<<8)|((rebuilds&0xF)<<12)|(reused?1u<<16:0u)|(ov_mode?1u<<20:0u),pane?dword(pane,0x40):0u,bind_mask);
    if(why && pane) {
        const int n=scope.ok?layout_client::ui_release(dm::kOwner,dm::ui_context()):-1;
        log_event("duel_manual_window_release",3,static_cast<unsigned>(n),blocks);
        owner_released();
    }
    return why;
}
static unsigned build_guarded() {
    __try {return build_window();}
    __except(EXCEPTION_EXECUTE_HANDLER) {owner_released();log_event("duel_manual_exception",2,blocks,0);return 9;}
}
// ---- caption (one Layout text line under the original gauge: joins and specials) ----
static unsigned cap_tries;   // builds for the caption now due (a waiting-bar rebuild takes the pane away: built again, up to 3 times)
static void caption(const wchar_t *text,unsigned ms=kCaptionMs) {
    wcsncpy_s(cap_text,text,_TRUNCATE);
    cap_want=true;cap_until=GetTickCount64()+ms;cap_tries=0;
}
// local298 (user 「30합 종료 자막」): a duel the 합 cap (or San11's landed-hit limit) ended on HP says so as it ends: 「30합 종료 — 체력
// 우세로 ○○ 승」, equal HP 「… — 무승부」. Once per scene; the caption outlives the result state's start (end_sync only hides the window).
// local303 (user 「하얀 글씨 … 난입처럼 스티커화」): the sticker 「판정승!」 on the winner's half, 「무승부!」 in the middle (the medal shows
// the 합, the HUD the winner); the caption only without the sticker. duel_manual_end_sticker: army kind + 1 | place << 4 (0 left, 1
// right, 2 centre) | caption kept << 8, value end reason | winner << 8, hex round.
// local306: a retreat's end says so as a caption (its 「퇴각!」 sticker showed as the 합 started): 「○○ 퇴각 — △△ 승」, a draw
// (against a fighter at 1 HP: retreat_end) 「○○ 퇴각 — 무승부」. duel_manual_end_sticker army 0xE for it (no sticker).
static unsigned gauge_left_side(Obj m);
static void retreat_caption() {
    int s=-1;
    for(int i=0;i<2;++i)if(rounds[block_n>0?block_n-1:0].r.side[i].special==3)s=i;
    if(s<0)s=duel.winner==0?1:0;   // a skip played it: the loser retreated
    wchar_t text[128],name[40],other[40];
    const int a=duel.side[s].active,b=duel.side[1-s].active;
    name_of(a>=0 && a<kMembers?member[s][a]:nullptr,name,40);
    name_of(b>=0 && b<kMembers?member[1-s][b]:nullptr,other,40);
    if(duel.winner==1-s)swprintf_s(text,L"%ls 퇴각 — %ls 승",name,other);
    else swprintf_s(text,L"%ls 퇴각 — 무승부",name);
    log_event("duel_manual_end_sticker",0xE|(static_cast<unsigned>(s)<<4)|(1u<<8),static_cast<unsigned>(duel.end_reason)|((static_cast<unsigned>(duel.winner)&0xFF)<<8),
              static_cast<unsigned>(duel.round));
    caption(text,kEndCaptionMs);
}
// local312 (user 「낮은 확률로 … 최대 35합까지」): the 합 that reached the cap rolled an extension (s11 cfg.round_ext): the caption
// 「연장! 최대 N합」 as that 합 ends. local313 (user 「연장 스티커가 생겼다가 교대에 금방 대체된다는거야 연장은 조금 크기 작게해서 우하단에 계속
// 떠있게하자」, 「반듯한 네모 느낌으로」): the 「연장 +5합」 badge right above the skip buttons until the scene ends (duel_digits
// publish_badge); the centre 「연장!」 sticker (local312) only while the badge is not made. duel_manual_extend: army 1 badge / 2 the
// sticker / 0 caption only, value the new cap, hex engine round.
static void extend_note() {
    const bool overlay=!duel_digits::broken() && !ov_dead;
    const bool badge=overlay && duel_digits::badge_ready();
    const bool stick=!badge && overlay && duel_digits::sticker_ready(duel_digits::SExtend);
    if(badge)duel_digits::publish_badge();
    else if(stick)duel_digits::publish_sticker(-1,-1,duel_digits::SExtend);
    wchar_t text[64];swprintf_s(text,L"연장! 최대 %d합",duel.cfg.round_cap);
    caption(text,kEndCaptionMs);
    log_event("duel_manual_extend",badge?1u:stick?2u:0u,static_cast<unsigned>(duel.cfg.round_cap),static_cast<unsigned>(duel.round));
}
static void end_caption(Obj m) {
    if(!end_capped && duel.finished && duel.end_reason==s11::EndRetreat){end_capped=true;retreat_caption();return;}
    if(end_capped || !duel.finished || (duel.end_reason!=s11::EndRoundCap && duel.end_reason!=s11::EndHitLimit))return;
    end_capped=true;
    const bool won=duel.winner==0 || duel.winner==1;
    const int kind=won?duel_digits::SWin:duel_digits::SDraw;
    const unsigned place=won?(static_cast<unsigned>(duel.winner)==gauge_left_side(m)?0u:1u):2u;
    const bool stick=!duel_digits::broken() && !ov_dead && duel_digits::sticker_ready(kind);
    log_event("duel_manual_end_sticker",static_cast<unsigned>(kind+1)|(place<<4)|(stick?0u:1u<<8),static_cast<unsigned>(duel.end_reason)|((static_cast<unsigned>(duel.winner)&0xFF)<<8),
              static_cast<unsigned>(duel.round));
    if(stick) {
        duel_digits::publish_sticker(place==0?kind:-1,place==1?kind:-1,place==2?kind:-1);
        return;
    }
    wchar_t head[32],text[128],name[40];
    if(duel.end_reason==s11::EndRoundCap)swprintf_s(head,L"%d합 종료",duel.cfg.round_cap);
    else swprintf_s(head,L"%d타 종료",duel.cfg.hit_limit);
    if(duel.winner==0 || duel.winner==1) {
        const int a=duel.side[duel.winner].active;
        name_of(a>=0 && a<kMembers?member[duel.winner][a]:nullptr,name,40);
        swprintf_s(text,L"%ls — 체력 우세로 %ls 승",head,name);
    }
    else swprintf_s(text,L"%ls — 무승부",head);
    caption(text,kEndCaptionMs);
}
// Built or shown once the waiting bars settled (their rebuild releases every pane of the owner), hidden when due. duel_manual_caption:
// army why (0 shown, 1 no UI scope, 2 show, 3 window, 4 activate, 5 builds used up) | builds << 4 | reused << 8, value block, hex
// state.
static void caption_step(Obj m) {
    if(cap_want && !dm::bar_dirty) {
        cap_want=false;
        layout_client::UiScope scope(dm::kOwner);
        unsigned why=0;
        const bool reused=cap_pane!=nullptr;
        if(!scope.ok)why=1;
        else if(reused) {layout_client::ui_node_text(cap_pane,0,cap_text);if(layout_client::ui_show(cap_pane,1)<0)why=2;}
        else if(cap_tries<kCaptionTries) {
            ++cap_tries;++cap_builds;
            Desc d[1]={};
            d[0].type=1;d[0].w=kCapW;d[0].h=kCapH;d[0].text=L" ";d[0].minus1=-1;
            cap_pane=static_cast<Obj>(layout_client::ui_window(nullptr,kCapX,kCapY,kCapW,kCapH,d,1));
            if(!cap_pane)why=3;
            else {layout_client::ui_node_text(cap_pane,0,cap_text);if(layout_client::ui_activate(cap_pane)<0)why=4;}
        }
        else why=5;
        cap_shown=!why;
        log_event("duel_manual_caption",why|(cap_builds<<4)|(reused?1u<<8:0u),blocks,dword(m,0x4A0));
    }
    if(cap_shown && GetTickCount64()>=cap_until) {
        cap_shown=false;
        layout_client::UiScope scope(dm::kOwner);
        if(scope.ok && cap_pane)layout_client::ui_show(cap_pane,0);
    }
}
// ---- mouse poll (speed_buttons absolute_rect / cursor_ui) ----
static bool control_rect(Obj control,int &x,int &y,int &w,int &h) {
    if(!control)return false;
    w=static_cast<int>(dword(control,0x28));h=static_cast<int>(dword(control,0x2C));
    x=0;y=0;Obj c=control;
    for(unsigned depth=0;c && depth<30;++depth) {x+=static_cast<int>(dword(c,0x20));y+=static_cast<int>(dword(c,0x24));c=pointer(c,8);}
    return !c && w>0 && h>0 && w<1920 && h<1080;
}
static bool cursor_ui(int &x,int &y) {   // the cursor in the 1920x1080 UI space of the game window, only while it has the focus
    HWND fg=GetForegroundWindow();DWORD pid=0;
    if(!fg)return false;
    GetWindowThreadProcessId(fg,&pid);
    if(pid!=GetCurrentProcessId())return false;
    HWND w=GetActiveWindow();
    if(!w)w=fg;
    POINT pt;RECT rc;
    if(!GetCursorPos(&pt) || !ScreenToClient(w,&pt) || !GetClientRect(w,&rc))return false;
    const double cw=rc.right-rc.left,ch=rc.bottom-rc.top;
    if(cw<16 || ch<16)return false;
    const double scale=(std::min)(cw/1920.0,ch/1080.0),ox=(cw-1920.0*scale)/2,oy=(ch-1080.0*scale)/2;
    x=static_cast<int>((pt.x-ox)/scale);y=static_cast<int>((pt.y-oy)/scale);return true;
}
static bool live(int n) {return n>=NSt0 && n<NCount && vis[n] && en[n];}   // a button that takes a click now
static int node_at(int x,int y,unsigned &how) {   // how: 2 control rectangle, 3 descriptor geometry (the drawn window: its plate)
    for(int n=NSt0;n<NCount;++n) {
        if(!live(n))continue;
        int rx,ry,rw,rh;
        if(btn[n] && !ov_mode && control_rect(btn[n],rx,ry,rw,rh))how=2;
        else {rx=pane_x+gx[n];ry=pane_y+gy[n];rw=gw[n];rh=gh[n];how=3;}
        if(x>=rx && x<rx+rw && y>=ry && y<ry+rh)return n;
    }
    return -1;
}
static void poll_arm() {   // the plates under the cursor changed (a full hold, its list): a button already held down never counts as a press
    const bool swap=GetSystemMetrics(SM_SWAPBUTTON)!=0;
    was_down[0]=(GetAsyncKeyState(swap?VK_RBUTTON:VK_LBUTTON)&0x8000)!=0;was_down[1]=(GetAsyncKeyState(swap?VK_LBUTTON:VK_RBUTTON)&0x8000)!=0;
    press_node[0]=press_node[1]=-1;
}
static int poll(ULONGLONG now,unsigned &path) {   // a left release on the node the left button was pressed on
    const bool swap=GetSystemMetrics(SM_SWAPBUTTON)!=0;
    const bool down[2]={(GetAsyncKeyState(swap?VK_RBUTTON:VK_LBUTTON)&0x8000)!=0,(GetAsyncKeyState(swap?VK_LBUTTON:VK_RBUTTON)&0x8000)!=0};
    int x=0,y=0;
    const bool cur=cursor_ui(x,y);
    int hit=-1;
    for(unsigned b=0;b<2;++b) {
        unsigned how=0;
        if(down[b] && !was_down[b])press_node[b]=cur?node_at(x,y,how):-1;
        else if(!down[b] && was_down[b]) {
            const int n=cur?node_at(x,y,how):-1;
            if(b==0 && n>=0 && n==press_node[b] && hit<0 && now-last_ms[n]>=kClickGapMs){hit=n;path=how;last_ms[n]=now;++polls;}
            press_node[b]=-1;
        }
        was_down[b]=down[b];
    }
    return hit;
}
static int take_click(unsigned &path) {   // path 1 native callback, 2 / 3 poll
    const ULONGLONG now=GetTickCount64();
    const LONG bits=InterlockedExchange(&native_bits,0);
    for(int n=NSt0;n<NCount;++n)
        if(bits&(1L<<n)) {
            if(!live(n) || now-last_ms[n]<kClickGapMs)continue;
            last_ms[n]=now;path=1;++natives;
            return n;
        }
    return poll(now,path);
}
// Every held frame while the drawn window is up. duel_manual_ovwin: army 1 the overlay drew it (once a show) | 2 nothing drawn (kOvFrames
// focused frames and kOvMs) | 3 its build failed | 4 the overlay faulted, value focused held frames since the show, hex block. 2-4: the
// plain button window.
static void ov_check() {
    if(!ov_mode || !pane)return;
    const bool built_bad=duel_digits::window_broken(),draw_bad=duel_digits::broken();
    if(!built_bad && !draw_bad) {
        if(ov_seen)return;
        if(duel_digits::window_draws()!=ov_draws){ov_seen=true;log_event("duel_manual_ovwin",1,ov_wait,blocks);return;}
        const ULONGLONG now=GetTickCount64();
        HWND fg=GetForegroundWindow();DWORD pid=0;
        if(fg)GetWindowThreadProcessId(fg,&pid);
        if(!fg || pid!=GetCurrentProcessId()){ov_since=now;return;}   // alt-tab / minimised: the wait starts again on the way back
        if(++ov_wait<kOvFrames || now-ov_since<kOvMs)return;
    }
    log_event("duel_manual_ovwin",built_bad?3u:draw_bad?4u:2u,ov_wait,blocks);
    ov_dead=true;
    {
        layout_client::UiScope scope(dm::kOwner);
        if(scope.ok)layout_client::ui_show(pane,0);
    }
    pane=nullptr;memset(btn,0,sizeof btn);ov_mode=false;duel_digits::publish_window(0);
    poll_arm();
    build_guarded();
}

// ---- hold ride (local298) ----
static float wrap_pi(float a) {while(a>kPi)a-=2.0f*kPi;while(a<-kPi)a+=2.0f*kPi;return a;}
static void dir_of(float h,float &x,float &z) {x=std::sin(h);z=std::cos(h);}   // 37D930: x += sin(+0xF0) * step, z += cos(+0xF0) * step
// Both seats from the pair's middle and heading (n(h) = (cos h, -sin h), the turn direction of a rising heading). Display only:
// position, the 37D930 target +0xE0 on it, facing +0xF0.
static void place_pair(Obj m,float cx,float cz,float h) {
    float fx,fz;dir_of(h,fx,fz);
    const float lx=fz,lz=-fx;
    for(unsigned s=0;s<2;++s) {
        Obj x=seat(m,s);
        const float px=cx+off_f[s]*fx+off_l[s]*lx,pz=cz+off_f[s]*fz+off_l[s]*lz;
        dm::flt(x,0xC0)=px;dm::flt(x,0xC8)=pz;dm::flt(x,0xE0)=px;dm::flt(x,0xE8)=pz;dm::flt(x,0xF0)=h+off_dh[s];
    }
}
static float frame_dt() {   // seconds since the last hold frame (1/60 after a pause or on the first frame, at most 0.05)
    LARGE_INTEGER now;QueryPerformanceCounter(&now);
    if(!qpc_freq.QuadPart)QueryPerformanceFrequency(&qpc_freq);
    float dt=1.0f/60.0f;
    if(qpc_last.QuadPart && qpc_freq.QuadPart)dt=static_cast<float>(static_cast<double>(now.QuadPart-qpc_last.QuadPart)/static_cast<double>(qpc_freq.QuadPart));
    qpc_last=now;
    if(dt<0.0f || dt>0.25f)dt=1.0f/60.0f;
    else if(dt>0.05f)dt=0.05f;
    return dt;
}
// The stage camera +0x518 (375D60's stage): vt+0x80 / +0x88 read the eye / rotation (x, y, z, w), vt+0x68 / +0x70 set them, vt+0x418
// applies (the calls 37D2C0 makes for a clip, duel_multi zoom_step's precedent).
static Obj stage_camera(Obj &st) {st=dm::stage_obj();Obj c=st?pointer(st,0x518):nullptr;return c && pointer(c,0)?c:nullptr;}
static void cam_get(Obj c,float e[4],float q[4]) {
    dm::zoom_vslot<void *(__fastcall *)(Obj,float *)>(c,0x80)(c,e);
    dm::zoom_vslot<void *(__fastcall *)(Obj,float *)>(c,0x88)(c,q);
}
static void cam_set(Obj st,Obj c,float e[4],float q[4]) {
    dm::zoom_vslot<void *(__fastcall *)(Obj,float *)>(c,0x68)(c,e);
    dm::zoom_vslot<void *(__fastcall *)(Obj,float *)>(c,0x70)(c,q);
    dm::zoom_vslot<void *(__fastcall *)(Obj,Obj)>(c,0x418)(c,pointer(st,0x510));
}
// A fault of the rotation getter / setter turns the side camera off for the scene (the pair then gallops in place) instead of ending
// the relay. duel_manual_cam army 9: value 1 get / 2 set, hex frames.
static bool cam_get_safe(Obj c,float e[4],float q[4]) {
    __try {cam_get(c,e,q);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER) {cam_dead=true;log_event("duel_manual_cam",9,1,hold_frames);return false;}
}
static bool cam_set_safe(Obj st,Obj c,float e[4],float q[4]) {
    __try {cam_set(st,c,e,q);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER) {cam_dead=true;log_event("duel_manual_cam",9,2,hold_frames);return false;}
}
static bool cam_eye_safe(Obj st,Obj c,float e[4]) {   // the eye alone (the pull-back writes no rotation)
    __try {
        dm::zoom_vslot<void *(__fastcall *)(Obj,float *)>(c,0x68)(c,e);
        dm::zoom_vslot<void *(__fastcall *)(Obj,Obj)>(c,0x418)(c,pointer(st,0x510));
        return true;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {cam_dead=true;log_event("duel_manual_cam",9,3,hold_frames);return false;}
}
struct V3 {float x,y,z;};
static V3 vadd(V3 a,V3 b) {return V3{a.x+b.x,a.y+b.y,a.z+b.z};}
static V3 vsub(V3 a,V3 b) {return V3{a.x-b.x,a.y-b.y,a.z-b.z};}
static V3 vmul(V3 a,float s) {return V3{a.x*s,a.y*s,a.z*s};}
static float vdot(V3 a,V3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
static V3 vcross(V3 a,V3 b) {return V3{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static bool vunit(V3 &a) {const float n=std::sqrt(vdot(a,a));if(!(n>1e-6f) || n!=n)return false;a=vmul(a,1.0f/n);return true;}
static unsigned enc_dot(float d) {return static_cast<unsigned>(static_cast<int>((d<-1.0f?-1.0f:d>1.0f?1.0f:d)*1000.0f)+1000)&0xFFFF;}
static unsigned enc_i16(float v) {const float c=v<-32768.0f?-32768.0f:v>32767.0f?32767.0f:v;return static_cast<unsigned>(static_cast<int>(c)+32768)&0xFFFF;}
static unsigned enc_u4(float d) {return static_cast<unsigned>(static_cast<int>((d<-1.0f?-1.0f:d>1.0f?1.0f:d)*10000.0f)+10000)&0xFFFF;}
static unsigned enc_u16(float v) {return v>0.0f?(v<65535.0f?static_cast<unsigned>(v):65535u):0u;}
// duel_manual_qraw (up to kQrawLogs a run): army kind (0 the getter's rotation with the block it matched at the hold decision, 1 a
// rotation we wrote that read back off, with the block read back in the same tick) | result << 4 (side_verify's, 0xF for kind 0) | log
// number << 8, value q.x | q.y << 16, hex q.z | q.w << 16. duel_manual_qraw2 (right after): army forward x | y << 16, value forward z
// | up x << 16, hex up y | up z << 16 (the block's, unit length). Every number * 10000 + 10000.
static const unsigned kQrawLogs=48;
static void qraw_log(unsigned kind,unsigned result,const float q[4],const float f[3],const float u[3]) {
    if(qraw_logs>=kQrawLogs)return;
    ++qraw_logs;
    float F[3]={f[0],f[1],f[2]},U[3]={u[0],u[1],u[2]};
    sc::cam_unit(F,3);sc::cam_unit(U,3);
    log_event("duel_manual_qraw",kind|((result&0xF)<<4)|((qraw_logs&0xFF)<<8),enc_u4(q[0])|(enc_u4(q[1])<<16),enc_u4(q[2])|(enc_u4(q[3])<<16));
    log_event("duel_manual_qraw2",enc_u4(F[0])|(enc_u4(F[1])<<16),enc_u4(F[2])|(enc_u4(U[0])<<16),enc_u4(U[1])|(enc_u4(U[2])<<16));
}
// The model the side camera frames (the rehearsal's boxes): per side the duelist on its seat at the ride start (local307: the original
// seats; the player's on the camera side) and the extras on stage at duel_multi's clock spots (out from their duelist, away from the
// other one), each rider a box of the horse and rider (forward -155..210, side -40..40, height 0..295 over the ground).
static unsigned model_points(V3 P,V3 f,V3 cs,float g,V3 *pts,unsigned &riders_p,unsigned &riders_e) {
    unsigned n=0;riders_p=riders_e=1;
    for(unsigned s=0;s<2;++s) {
        const bool mine=s==player_side;
        const float u0=off0_f[s],v0=-cam_side*off0_l[s];   // the seat along the travel / towards the camera (cs = -cam_side * left)
        const float sign=v0>0.0f?1.0f:v0<0.0f?-1.0f:mine?1.0f:-1.0f;
        for(unsigned r=0;r<3;++r) {
            if(r && !dm::rider_on_stage(r-1,s))continue;
            if(r)++(mine?riders_p:riders_e);
            const float rear=r==0?0.0f:r==1?dm::kRear0:dm::kRear1,out=r==0?0.0f:r==1?dm::kSide0:dm::kSide1;
            const float u=u0-rear,v=v0+sign*out;
            for(float du:{-155.0f,210.0f})for(float dv:{-40.0f,40.0f})for(float y:{0.0f,295.0f}) {
                V3 p=vadd(vadd(P,vmul(f,u+du)),vmul(cs,v+dv));p.y=g+y;pts[n++]=p;
            }
        }
    }
    return n;
}
static void side_axes(V3 f,V3 cs,V3 &o,V3 &d,V3 &rs,V3 &up) {   // eye direction o (target to eye), view d, screen right / up
    o=vadd(vmul(vadd(vmul(f,std::cos(kCamAzim)),vmul(cs,std::sin(kCamAzim))),std::cos(kCamPitch)),V3{0.0f,std::sin(kCamPitch),0.0f});
    d=vmul(o,-1.0f);
    V3 rg=vcross(d,V3{0.0f,1.0f,0.0f});vunit(rg);
    rs=vmul(rg,cam_rside);up=vcross(rg,d);vunit(up);
}
static float rect_top(unsigned riders_p,unsigned riders_e) {return riders_p>1 || riders_e>1?kRectTopBars:kRectTop;}
// The side camera's aim at the hold distance D (local303: fixed kHoldDist 2500; local301 3500, user 「지금멀리하는 수치의
// 45%정도」): the rehearsal's aim correction (30 rounds: move the aim by the riders' box offset from the free rectangle's centre)
// with this run's projection scales; the distance is never changed. out: aim forward, camera side, height over the ground. False: a
// rider came behind the eye (the riders' middle is the aim then).
static bool side_aim(float g,float D,float out[3],unsigned &riders_p,unsigned &riders_e) {
    float fx,fz;dir_of(ph0,fx,fz);
    const V3 f{fx,0.0f,fz},cs{-cam_side*fz,0.0f,cam_side*fx},P{pc_x,0.0f,pc_z};
    V3 o,d,rs,up;side_axes(f,cs,o,d,rs,up);
    V3 pts[48];
    const unsigned n=model_points(P,f,cs,g,pts,riders_p,riders_e);
    V3 T{0.0f,0.0f,0.0f};
    for(unsigned i=0;i<n;++i)T=vadd(T,pts[i]);
    T=vmul(T,1.0f/static_cast<float>(n));T.y=g+kChest;
    const V3 T0=T;
    const float y0=rect_top(riders_p,riders_e),cx=(kRectLeft+kRectRight)*0.5f,cy=(y0+kRectBottom)*0.5f,sx=960.0f*cam_xs,sy=540.0f*cam_ys;
    bool good=true;
    for(int it=0;it<30 && good;++it) {
        const V3 E=vadd(T,vmul(o,D));
        float b[4]={1e9f,1e9f,-1e9f,-1e9f};
        for(unsigned i=0;i<n;++i) {
            const V3 q=vsub(pts[i],E);const float z=vdot(q,d);
            if(z<5.0f){good=false;break;}
            const float px=960.0f+sx*vdot(q,rs)/z,py=540.0f-sy*vdot(q,up)/z;
            if(px<b[0])b[0]=px;if(py<b[1])b[1]=py;if(px>b[2])b[2]=px;if(py>b[3])b[3]=py;
        }
        if(good)T=vadd(T,vadd(vmul(rs,((b[0]+b[2])*0.5f-cx)/sx*D),vmul(up,-((b[1]+b[3])*0.5f-cy)/sy*D)));
    }
    if(!good)T=T0;
    const V3 rel=vsub(T,P);
    out[0]=vdot(rel,f);out[1]=vdot(rel,cs);out[2]=T.y-g;
    for(int i=0;i<3;++i)if(out[i]!=out[i]){const V3 r0=vsub(T0,P);out[0]=vdot(r0,f);out[1]=vdot(r0,cs);out[2]=kChest;return false;}
    return good;
}
// The pair back on its hold-start heading and offsets about its middle now: the release cut, a skip, a bail, a stopped follow ride.
static void pair_home(Obj m) {
    memcpy(off_f,hs_f,sizeof off_f);memcpy(off_l,hs_l,sizeof off_l);memcpy(off_dh,hs_dh,sizeof off_dh);
    ph=ph0=hs_h;place_pair(m,pc_x,pc_z,hs_h);
}
// The ride's first frame: the original seats and heading (local307; local298-306 turned the pair around here when the player sat on
// the far side). With our side camera the camera goes to the player's side (cam_side: the player's duelist nearest the camera; flipped
// = not the side cam_rside gave); the follow camera keeps the original rotation. duel_manual_seats (local307, up to kSeatLogs a
// scene): army hold kind | heading forced << 4 | side camera << 5 | camera side + 1 << 8 | moved << 10, value seat forward side 0 +
// 32768 | side 1 << 16, hex seat left side 0 + 32768 | side 1 << 16 (the pair's frame at the hold start).
// local307 (user 「A 뒤에서 출발」): a gap starts behind the original seats by the ride still ahead (kRideSpeed * the gap's time left)
// so the pair rides straight into the seats as the gap ends. Moved at the camera decision (this hold's cut: our side camera or the
// follow camera takes over here, where local298-306 turned the pair around), before its ground and aim are read; once a hold.
// local308: the installer's short gap; local312 「없음」 0 (a gap kept for a join's hold ends on its first poll)
static unsigned gap_len() {return sc::gap_none(mod_features.duel_relay_gap)?0u:sc::gap_ms(mod_features.duel_relay_gap);}
static void gap_start() {
    if(gap_shifted)return;
    gap_shifted=true;
    if(hold_kind!=sc::HoldGap)return;
    const ULONGLONG used=GetTickCount64()-hold_start;
    const unsigned len=gap_len();
    gap_left_ms=used<len?static_cast<unsigned>(len-used):0;
    const float left=static_cast<float>(gap_left_ms)*0.001f;
    float fx,fz;dir_of(hs_h,fx,fz);
    pc_x=hs_cx-fx*kRideSpeed*left;pc_z=hs_cz-fz*kRideSpeed*left;
}
static void ride_begin(bool side) {
    gap_start();
    const float l=hs_l[player_side],base=cam_rside!=0.0f?cam_rside:1.0f;
    cam_side=side && l!=0.0f?(l>0.0f?-1.0f:1.0f):base;
    flipped=side && cam_side!=base;
    ph0=ph=hs_h;
    for(unsigned s=0;s<2;++s) {
        off0_f[s]=hs_f[s];off0_l[s]=hs_l[s];off0_dh[s]=heading_forced?0.0f:hs_dh[s];
        off_f[s]=off0_f[s];off_l[s]=off0_l[s];off_dh[s]=off0_dh[s];
    }
    ride_t=0.0f;
    if(seat_logs<kSeatLogs) {
        ++seat_logs;
        log_event("duel_manual_seats",(static_cast<unsigned>(hold_kind)&0xF)|(heading_forced?1u<<4:0u)|(side?1u<<5:0u)|(static_cast<unsigned>(cam_side+1.0f)<<8)|(flipped?1u<<10:0u),
                  enc_i16(hs_f[0])|(enc_i16(hs_f[1])<<16),enc_i16(hs_l[0])|(enc_i16(hs_l[1])<<16));
    }
}
// The side camera failed (camera fault, eye not taken, rotation read back off, no block): the original view back, then the follow
// camera from where the pair rides now (the ride goes on). The original view is the absolute rotation of the forward / up its block
// showed at the decision: the getter's rotation is not that view (written back it gives a level look). Only when a written rotation
// read back off (why 6: the absolute model failed this run) does the getter's rotation go back as local299 did.
static void side_off(Obj m,unsigned why) {
    (void)m;
    Obj st=nullptr,cam=stage_camera(st);
    if(cam && orig_ok && !cam_dead) {
        alignas(16) float q[4],e[4];memcpy(e,orig_eye,sizeof e);
        if(why!=6 && dec_ok && sc::cam_abs(dec_f,dec_u,q))cam_set_safe(st,cam,e,q);
        else cam_set_safe(st,cam,e,orig_q);
    }
    if(dec_ok){memcpy(fol_f,dec_f,sizeof fol_f);fol_f_ok=true;}
    side_mode=kModeFollow;verify_due=false;
    log_event("duel_manual_side",kModeFollow|(why<<4),hold_frames,0);
}
// duel_manual_side: army mode (1 our side camera, 2 the follow camera: the original rotation, the eye following the pair, 3 in place:
// the follow camera lost the pair, which is back at its hold-start place galloping in place) | why << 4 (mode 1: 1 the aim fell back
// to the riders' middle; mode 2: 1 no block of this camera within kQuatTries, 3 no ground, 4 camera fault, 5 eye not applied (hex 1:
// the follow camera's, no eye writes for the run), 6 a written rotation read back off (no rotation writes for the run), 7 no block to
// read back, 9 rotation writes off for the run; mode 3: 8) | turned around << 8 | riders player << 12 | riders enemy << 14 | screen-right sign + 1 << 16, value distance | aim
// height over the ground << 16, hex aim forward + 32768 | camera side + 32768 << 16 (mode 1).
// duel_manual_dist (once a hold): army 1 measured now (the scene's first hold) / 2 the scene's measure kept / 3 none (no block in
// time) / 4 a gap became a full hold or a join hold began (hex: side mode) | hold kind << 4 (local305) | riders on stage << 8, value the original's eye-to-pair distance | distance used << 16, hex block /
// getter eye gap.
static unsigned riders_now() {
    unsigned n=2;
    for(unsigned r=0;r<2;++r)for(unsigned s=0;s<2;++s)if(dm::rider_on_stage(r,s))++n;
    return n;
}
static void dist_set(unsigned kind,unsigned gap) {   // local305: a gap's kGapDist, a full hold's kHoldDist (duel_manual_script flow_distance)
    hold_D=sc::flow_distance(hold_kind);
    log_event("duel_manual_dist",kind|((static_cast<unsigned>(hold_kind)&7)<<4)|((riders_now()&0xF)<<8),enc_u16(orig_measured?orig_dist:0.0f)|(enc_u16(hold_D)<<16),gap);
}
static bool try_side(Obj m) {   // true = decided, false = try again next frame
    Obj st=nullptr,cam=stage_camera(st);
    if(cam_dead){ride_begin(false);dist_set(3,0);side_mode=kModeFollow;log_event("duel_manual_side",kModeFollow|(4u<<4),hold_frames,0);return true;}
    if(!cam)return false;
    if(!cam_get_safe(cam,orig_eye,orig_q)){ride_begin(false);dist_set(3,0);side_mode=kModeFollow;log_event("duel_manual_side",kModeFollow|(4u<<4),hold_frames,0);return true;}
    orig_ok=true;
    float c[80];unsigned vt=0;
    if(dm::read_cam(c,vt))return false;
    const float gap=dm::zoom_dist(c,orig_eye);
    if(gap>=kEyeSame)return false;   // wait for a frame whose block is this camera (the original applied what the getter holds)
    V3 R{c[60],c[64],c[68]},U{c[61],c[65],c[69]},F{c[63],c[67],c[71]};
    const float xs=std::sqrt(vdot(R,R)),ys=std::sqrt(vdot(U,U));
    if(!(vunit(R) && vunit(U) && vunit(F) && xs>0.1f && ys>0.1f))return false;
    {   // projection scales and the screen-right sign (constant for the run)
        V3 h=vcross(F,V3{0.0f,1.0f,0.0f});
        if(vunit(h)) {
            const float hand=vdot(R,h);
            if(hand>0.5f || hand<-0.5f){cam_rside=hand>0.0f?1.0f:-1.0f;cam_xs=xs;cam_ys=ys;}
        }
    }
    if(cam_rside==0.0f)return false;
    fol_f[0]=F.x;fol_f[1]=F.y;fol_f[2]=F.z;fol_f_ok=true;
    dec_f[0]=F.x;dec_f[1]=F.y;dec_f[2]=F.z;dec_u[0]=U.x;dec_u[1]=U.y;dec_u[2]=U.z;dec_ok=true;   // the original view (side_off)
    {const float f3[3]={F.x,F.y,F.z},u3[3]={U.x,U.y,U.z};qraw_log(0,0xF,orig_q,f3,u3);}
    unsigned kind=2;
    if(!orig_measured) {   // the scene's first hold: the original eye to the pair's middle at chest height
        Obj a=seat(m,0),b=seat(m,1);
        const V3 C{pc_x,(dm::flt(a,0xC4)+dm::flt(b,0xC4))*0.5f+kChest,pc_z},de=vsub(V3{orig_eye[0],orig_eye[1],orig_eye[2]},C);
        orig_dist=std::sqrt(vdot(de,de));orig_measured=true;kind=1;
    }
    dist_set(kind,static_cast<unsigned>(gap));
    gap_start();   // local307: the gap's start (the original eye distance above was measured on the seats)
    bool ok=false;
    const float g=dm::ground_at(m,pc_x,pc_z,ok);
    if(!ok || abs_dead) {   // no side camera: the original rotation follows the pair
        if(!ok)++ground_misses;
        ride_begin(false);side_mode=kModeFollow;
        log_event("duel_manual_side",kModeFollow|((ok?9u:3u)<<4)|(static_cast<unsigned>(cam_rside+1.0f)<<16),enc_u16(hold_D),0);
        return true;
    }
    ground_last=g;
    ride_begin(true);
    float aim[3];unsigned rp=1,re=1;
    const bool aimed=side_aim(g,hold_D,aim,rp,re);
    cam_uT=aim[0];cam_vT=aim[1];cam_yT=aim[2];cam_D=hold_D;
    side_mode=kModeSide;verify_due=true;
    place_pair(m,pc_x,pc_z,ph);
    log_event("duel_manual_side",kModeSide|(aimed?0u:1u<<4)|(flipped?1u<<8:0u)|((rp&3)<<12)|((re&3)<<14)|(static_cast<unsigned>(cam_rside+1.0f)<<16),
              enc_u16(cam_D)|(enc_u16(cam_yT)<<16),enc_i16(cam_uT)|(enc_i16(cam_vT)<<16));
    return true;
}
// A gap hold became a full one or a join hold began (local305): the full hold's distance from this frame; our side camera's aim is
// worked out again for it from where the pair rides now (no new camera decision: the ride goes on). A camera not decided yet reads the
// kind itself (dist_set).
static void hold_refocus() {
    if(side_wait)return;
    hold_D=sc::flow_distance(hold_kind);
    if(side_mode==kModeSide) {
        float aim[3];unsigned rp=1,re=1;
        side_aim(ground_last,hold_D,aim,rp,re);
        cam_uT=aim[0];cam_vT=aim[1];cam_yT=aim[2];cam_D=hold_D;
    }
    log_event("duel_manual_dist",4|((static_cast<unsigned>(hold_kind)&7)<<4)|((riders_now()&0xF)<<8),enc_u16(orig_measured?orig_dist:0.0f)|(enc_u16(hold_D)<<16),side_mode);
}
// The first write of a hold read back in the same tick (the apply rewrites the block): 0 good, 1 no block, 2 the eye is not ours, 3
// forward or up off, 4 the boxes outside the free rectangle (+- kRectSlack; logged and kept: the distance is the user's 1.8x).
// duel_manual_cam: army result | side mode << 4 | 1 << 8 (absolute rotation), value forward dot * 1000 + 1000 | up dot << 16, hex eye
// gap | box count << 16. duel_manual_camf: army side mode, value box left | right << 16, hex top | bottom << 16 (pixels + 32768).
// duel_manual_camg: value ground at the middle + 32768 | eye over its ground + 32768 << 16, hex xs * 1000 | ys * 1000 << 16.
static unsigned side_verify(V3 E,V3 F,float g,float ge,const float q[4]) {
    float c[80];unsigned vt=0;
    if(dm::read_cam(c,vt)){log_event("duel_manual_cam",1|(side_mode<<4)|(1u<<8),0,vt&0xFFFF);return 1;}
    const float fb[3]={c[63],c[67],c[71]},ub[3]={c[61],c[65],c[69]};
    const float e3[3]={E.x,E.y,E.z},gap=dm::zoom_dist(c,e3);
    V3 Fb{c[63],c[67],c[71]},Ub{c[61],c[65],c[69]},Ue=vsub(V3{0.0f,1.0f,0.0f},vmul(F,F.y));
    const bool axes=vunit(Fb) && vunit(Ub) && vunit(Ue);
    const float df=axes?vdot(Fb,F):-1.0f,du=axes?vdot(Ub,Ue):-1.0f;
    float fx,fz;dir_of(ph,fx,fz);
    const V3 f{fx,0.0f,fz},cs{-cam_side*fz,0.0f,cam_side*fx};
    V3 pts[48];unsigned rp=1,re=1;
    const unsigned n=model_points(V3{pc_x,0.0f,pc_z},f,cs,g,pts,rp,re);
    float b[4]={1e9f,1e9f,-1e9f,-1e9f};bool behind=false;
    for(unsigned i=0;i<n;++i) {
        const dm::ZoomClip k=dm::zoom_clip(c,pts[i].x,pts[i].y,pts[i].z);
        if(k.w<1.0f){behind=true;continue;}
        const float px=960.0f+960.0f*k.x/k.w,py=540.0f-540.0f*k.y/k.w;
        if(px<b[0])b[0]=px;if(py<b[1])b[1]=py;if(px>b[2])b[2]=px;if(py>b[3])b[3]=py;
    }
    const float y0=rect_top(rp,re);
    const bool framed=!behind && b[0]>=kRectLeft-kRectSlack && b[2]<=kRectRight+kRectSlack && b[1]>=y0-kRectSlack && b[3]<=kRectBottom+kRectSlack;
    const unsigned r=gap>=kEyeApplied?2u:!(df>kQuatFwd && du>kQuatUp)?3u:framed?0u:4u;
    log_event("duel_manual_cam",r|(side_mode<<4)|(1u<<8),enc_dot(df)|(enc_dot(du)<<16),enc_u16(gap)|((n&0xFFFF)<<16));
    log_event("duel_manual_camf",side_mode,enc_i16(b[0])|(enc_i16(b[2])<<16),enc_i16(b[1])|(enc_i16(b[3])<<16));
    log_event("duel_manual_camg",0,enc_i16(g)|(enc_i16(E.y-ge)<<16),(static_cast<unsigned>(cam_xs*1000.0f)&0xFFFF)|((static_cast<unsigned>(cam_ys*1000.0f)&0xFFFF)<<16));
    if(r==2 || r==3)qraw_log(1,r,q,fb,ub);
    return r;
}
// The side camera this frame: aim T on the pair's frame, eye from T along the fixed direction (kCamAzim off the travel towards the
// camera side, kCamPitch down) at the hold distance, never under the ground at the eye + kEyeClear; the absolute rotation of that
// view (duel_manual_script.h cam_abs). The hold's first write is read back in the same tick.
static bool side_camera(Obj m,Obj st,Obj cam) {
    float fx,fz;dir_of(ph,fx,fz);
    const V3 f{fx,0.0f,fz},cs{-cam_side*fz,0.0f,cam_side*fx};
    bool ok=false;float g=dm::ground_at(m,pc_x,pc_z,ok);
    if(ok)ground_last=g;else{g=ground_last;++ground_misses;}
    V3 o,d,rs,up;side_axes(f,cs,o,d,rs,up);
    V3 T=vadd(vadd(V3{pc_x,0.0f,pc_z},vmul(f,cam_uT)),vmul(cs,cam_vT));T.y=g+cam_yT;
    V3 E=vadd(T,vmul(o,cam_D));
    bool eok=false;const float ge=dm::ground_at(m,E.x,E.z,eok);
    if(eok && E.y<ge+kEyeClear){E.y=ge+kEyeClear;++eye_clamps;}
    V3 F=vsub(T,E);
    if(!vunit(F))return false;
    const V3 U=vsub(V3{0.0f,1.0f,0.0f},vmul(F,F.y));
    const float f3[3]={F.x,F.y,F.z},u3[3]={U.x,U.y,U.z};
    alignas(16) float e[4]={E.x,E.y,E.z,orig_eye[3]},q[4];
    if(!sc::cam_abs(f3,u3,q))return false;
    if(!cam_set_safe(st,cam,e,q))return false;
    if(!verify_due)return true;
    verify_due=false;
    const unsigned r=side_verify(E,F,g,eok?ge:g,q);
    if(r==3){abs_dead=true;side_off(m,6);}
    else if(r==1 || r==2)side_off(m,r==2?5u:7u);
    return true;
}
// The follow camera (no side camera: rotation writes off, no ground, a camera fault, no block in time, or the side camera failed):
// the original rotation kept (its view forward read from the block every frame, we write no rotation), the eye kHoldDist
// back along it from the pair's middle at chest height, never under its ground + kEyeClear. Riding, the pair is
// checked on the frame just drawn (whoever placed that eye): kFollowOff frames in a row with its middle outside kFollowEdge of the
// screen (or behind the eye) end the ride for this hold — the only false. Anything else (no camera, no forward yet, the eye not taken)
// leaves the camera as it is and the ride goes on.
static bool follow_camera(Obj m,bool riding) {
    Obj a=seat(m,0),b=seat(m,1);
    const float C[3]={pc_x,(dm::flt(a,0xC4)+dm::flt(b,0xC4))*0.5f+kChest,pc_z};
    float c[80];unsigned vt=0;
    if(dm::read_cam(c,vt)==0) {
        V3 F{c[63],c[67],c[71]};
        if(vunit(F)){fol_f[0]=F.x;fol_f[1]=F.y;fol_f[2]=F.z;fol_f_ok=true;}
        if(riding) {
            const dm::ZoomClip k=dm::zoom_clip(c,C[0],C[1],C[2]);
            const bool off=k.w<1.0f || std::fabs(k.x)>kFollowEdge*k.w || std::fabs(k.y)>kFollowEdge*k.w;
            fol_off=off?fol_off+1:0;
            if(fol_off>=kFollowOff)return false;
        }
    }
    if(cam_dead || fol_dead || !fol_f_ok)return true;
    Obj st=nullptr,cam=stage_camera(st);
    if(!cam)return true;
    alignas(16) float eye[4]={0.0f,0.0f,0.0f,orig_eye[3]};
    sc::follow_eye(C,fol_f,hold_D,eye);
    bool gok=false;const float ge=dm::ground_at(m,eye[0],eye[2],gok);
    if(gok && eye[1]<ge+kEyeClear){eye[1]=ge+kEyeClear;++eye_clamps;}
    if(!cam_eye_safe(st,cam,eye))return true;
    if(!fol_written) {   // the first write of the hold: did the camera take our eye? (not taken: no eye writes for the run)
        alignas(16) float back[4],bq[4];
        if(!cam_get_safe(cam,back,bq))return true;
        if(dm::zoom_dist(back,eye)>1.0f){fol_dead=true;log_event("duel_manual_side",kModeFollow|(5u<<4),hold_frames,1);return true;}
    }
    memcpy(fol_last,eye,sizeof fol_last);fol_written=true;++fol_frames;
    return true;
}
// The hold ends: the original's eye back where the follow camera's still stands (local299 pull_end); our side camera is left to the
// round's own camera (the pair is put home about its middle now, still in that view).
static void follow_end() {
    if(!fol_written || cam_dead || !orig_ok)return;
    Obj st=nullptr,cam=stage_camera(st);
    alignas(16) float pos[4],q[4];
    if(cam && cam_get_safe(cam,pos,q) && dm::zoom_dist(pos,fol_last)<1.0f)cam_eye_safe(st,cam,orig_eye);
}
// Ride bookkeeping: frames, length, each seat's height over its ground (37D930 never sets a height from the terrain).
static void ride_note(Obj m,float dt) {
    ++ride_frames;ride_len+=kRideSpeed*dt;
    for(unsigned s=0;s<2;++s) {
        Obj x=seat(m,s);
        bool ok=false;const float g=dm::ground_at(m,dm::flt(x,0xC0),dm::flt(x,0xC8),ok);
        if(!ok){++ground_misses;continue;}
        const float d=dm::flt(x,0xC4)-g;
        if(d<ride_dmin)ride_dmin=d;
        if(d>ride_dmax)ride_dmax=d;
    }
}
// One hold frame: decide the camera (up to kQuatTries frames), then ride the circle (the centre on the camera side, the original
// riding speed), the enemy easing to its lead, the camera following (ours from the side, else the original rotation). local300 (user
// 「원형으로 돌게하면서 실제로 움직이게 바꿔준다며」): the ride no longer depends on our side camera; only a follow camera that lost the
// pair stops it (the pair home, gallop in place).
static void hold_ride(Obj m) {
    const float dt=frame_dt();
    if(side_wait) {
        if(try_side(m))side_wait=false;
        else if(++side_tries>=kQuatTries) {
            side_wait=false;ride_begin(false);dist_set(3,0);side_mode=kModeFollow;
            log_event("duel_manual_side",kModeFollow|(1u<<4),hold_frames,side_tries);
        }
        if(side_wait)return;
    }
    if(side_mode==kModeStill){follow_camera(m,false);return;}
    // local307: a gap rides straight on along the original heading (the 합 after it starts where the gap ends: nothing to put back), a
    // full hold circles towards the camera side (the pair as one: both on the same angular speed); a gap turned full circles from here
    if(hold_kind!=sc::HoldGap) {
        ride_t+=dt;
        ph=wrap_pi(ph-cam_side*(kRideSpeed/kCircleR)*dt);
    }
    float fx,fz;dir_of(ph,fx,fz);
    pc_x+=fx*kRideSpeed*dt;pc_z+=fz*kRideSpeed*dt;
    const float s=ride_t>=kLeadEase?1.0f:ride_t/kLeadEase,e=s*s*(3.0f-2.0f*s);
    for(unsigned k=0;k<2;++k){off_f[k]=off0_f[k];off_l[k]=off0_l[k];off_dh[k]=off0_dh[k]*(1.0f-e);}
    place_pair(m,pc_x,pc_z,ph);
    ride_note(m,dt);
    if(side_mode==kModeSide) {
        Obj st=nullptr,cam=stage_camera(st);
        if(!cam || cam_dead || !side_camera(m,st,cam)) {if(side_mode==kModeSide)side_off(m,cam_dead?4u:5u);}
    }
    if(side_mode==kModeFollow && !follow_camera(m,true)) {   // the pair left the view: back to the hold-start place, gallop in place
        side_mode=kModeStill;fol_stopped=true;
        pc_x=hs_cx;pc_z=hs_cz;pair_home(m);++snaps;
        log_event("duel_manual_side",kModeStill|(8u<<4),hold_frames,fol_frames);
        follow_camera(m,false);
    }
}
// The pair back on its hold-start heading and offsets about its middle (the release cut, a skip, a bail); the round's own camera
// takes over from there. duel_manual_turnback: army why (1 release, 2 skip, 3 bail, 4 end) | side mode << 4 | turned around << 8,
// value turn in degrees + 1000, hex frames held | eye clamps << 16 | ground misses << 24. duel_manual_ride (local300): army side mode
// | why << 4 | follow stopped << 8 | ground misses << 16, value lowest seat height over its ground + 32768 | highest + 32768 << 16
// (both 32768 when no ground was read), hex frames ridden | length ridden / 10 << 16.
// local307 (user 「A 뒤에서 출발」): a gap released as a gap (never circled) arrives at the original seats; the few units a frame or a
// hitch leaves are closed at the release cut so the 합 plays on the original spot. duel_manual_arrive (up to kSeatLogs a scene): army
// why | closed << 4 | side mode << 8, value the middle's distance from the original one, hex frames held | gap ms filmed / 10 (time
// left at the camera decision) << 16 | gap length / 10 << 24 (local308: the installer's gap).
static void hold_finish(Obj m,unsigned why) {
    if(!scene || m!=scene)return;
    const float turn=wrap_pi(ph-hs_h);
    if(hold_kind==sc::HoldGap && ride_t==0.0f && side_mode) {
        const float dx=pc_x-hs_cx,dz=pc_z-hs_cz,miss=std::sqrt(dx*dx+dz*dz);
        const bool close=why==1 && miss<=kArriveSnap;
        if(close){pc_x=hs_cx;pc_z=hs_cz;}
        if(arrive_logs<kSeatLogs) {
            ++arrive_logs;
            log_event("duel_manual_arrive",why|(close?1u<<4:0u)|((side_mode&0xF)<<8),enc_u16(miss),
                      (hold_frames&0xFFFF)|(((gap_left_ms/10)&0xFF)<<16)|(((gap_len()/10)&0xFF)<<24));
        }
    }
    if(side_mode){pair_home(m);++snaps;}
    follow_end();
    log_event("duel_manual_turnback",why|(side_mode<<4)|(flipped?1u<<8:0u),static_cast<unsigned>(static_cast<int>(turn*180.0f/kPi)+1000),
              (hold_frames&0xFFFF)|((eye_clamps&0xFF)<<16)|((ground_misses&0xFF)<<24));
    const bool seen=ride_dmin<=ride_dmax;
    log_event("duel_manual_ride",side_mode|(why<<4)|(fol_stopped?1u<<8:0u)|((ground_misses&0xFF)<<16),
              enc_i16(seen?ride_dmin:0.0f)|(enc_i16(seen?ride_dmax:0.0f)<<16),(ride_frames&0xFFFF)|((static_cast<unsigned>(ride_len*0.1f)&0xFFFF)<<16));
}
// duel_multi's round crane: rounds keep the original rotation (local300: the user asked for the hold only; local299 never aimed either).
static bool look_at(const float *,const float *,float *) {return false;}
static bool aim_after(Obj,Obj,float *,float *) {return true;}

// The engine side the gauge shows on the left (bars_step's rule: the gauge's left seat is seat 1 -> side 1).
static unsigned gauge_left_side(Obj m) {
    Obj g=gauge_of(m);
    return g && pointer(g,0x180)==seat(m,1)?1u:0u;
}
// ---- stage ----
// The engine's fighters of round k of the block on stage (k 1..block_n): a member who came in leaves the ground, a new fighter takes
// the seat. A join or a special of round k shows its caption now. duel_manual_swap: army side
// | round << 4 | block << 8 | swap_in result << 12 | joined << 16, value incoming id | outgoing id << 16, hex incoming HP | slot in << 8
// | slot out << 12.
static void show_round(Obj m,int k) {
    const Round &R=rounds[k-1];
    for(unsigned s=0;s<2;++s)for(unsigned i=0;i<kMembers;++i)disp_hp[s][i]=k==1?pre_hp[s][i]:rounds[k-2].hp[s][i];
    for(unsigned s=0;s<2;++s) {   // local299 (user 「필살기 써도 필살기 바가 안달고」): a special's cost leaves the bar as its round starts
        int v=k==1?blk_morale[s]:rounds[k-2].r.side[s].morale;   // (the round's other gains show with the next round)
        const int a=R.r.side[s].special;   // -1 none, else the San11 action
        if(a>=0 && a<8)v-=s11::kCost[a];
        disp_morale[s]=v<0?0:v;
    }
    bool any=false,cut_shown=false;
    wchar_t text[256]=L"",name[40];
    // local301 (user 「하얀색 텍스트로 난입 … 만화책 스럽게 난입 !~ 이런 큰글자로 한단어로 딱 스티커?처럼」): a join is the drawn sticker
    // 「난입!」 on the joiner's half of the screen (the HUD shows who came in); local303 (user 「아직도 하얀 글씨 뭐 뜨는데 아무것도
    // 안보여 난입처럼 스티커화」): a swap 「교대!」 and a special 「필살기! / 기합! / 견수! / 급소! / 무쌍!」 the same way; local306 「퇴각!」
    // and the criticals 「연속 공격! / 자동 견수! / 투지 상승!」. A side mostly has one event a round (a special round has no joins,
    // swaps or exchange, a join drops that round's swap; a joiner in 일격 중시 may roll a critical at once); a second one, or a kind Present has not
    // made yet on this device, or a scene whose overlay never drew (ov_dead), keeps its caption (「X 난입!」, 「교대! A → B」, 「X 필살기!」).
    // duel_manual_sticker: army left kind + 1 | right kind + 1 << 4 | left side << 8 | caption kept << 9, value round | block << 8.
    const bool overlay=!duel_digits::broken() && !ov_dead;
    int on_half[2]={-1,-1};   // the sticker kind on the left / right half
    const unsigned left_side=gauge_left_side(m);
    for(unsigned s=0;s<2;++s) {
        const s11::SideResult &sr=R.r.side[s];
        const bool joined=sr.join_in>=0 && sr.join_in<kMembers;
        if(joined && !shown[s][sr.join_in]){shown[s][sr.join_in]=true;++joins;}
        // local305: a joiner the join hold seated already showed its sticker; a join hold whose 합 brought in someone else is logged
        // (duel_manual_join_miss: army side | joiner seated + 1 << 4 | joiner now + 1 << 8, value engine round)
        const bool peeked=joined && join_peeked[s]==sr.join_in;
        if(join_peeked[s]>=0 && !peeked) {
            ++join_misses;
            log_event("duel_manual_join_miss",s|(static_cast<unsigned>(join_peeked[s]+1)<<4)|(static_cast<unsigned>(sr.join_in+1)<<8),static_cast<unsigned>(duel.round),0);
        }
        const int a=sr.active;
        const unsigned half=s==left_side?0u:1u;
        auto take=[&](int kind){if(kind<0 || on_half[half]>=0 || !overlay || !duel_digits::sticker_ready(kind))return false;on_half[half]=kind;return true;};
        // local298 (user 「교대」 feedback): a join names the joiner (join_in), a swap reads 「교대! A → B」, a special its fighter.
        auto add=[&](const wchar_t *part){if(text[0])wcsncat_s(text,L"   ",_TRUNCATE);wcsncat_s(text,part,_TRUNCATE);};
        wchar_t part[96];
        if(joined && !peeked && !take(duel_digits::SJoin)) {name_of(member[s][sr.join_in],name,40);swprintf_s(part,L"%ls 난입!",name);add(part);}
        if(sr.swap_in>=0 && sr.swap_in<kMembers && sr.swap_out>=0 && sr.swap_out<kMembers && !take(duel_digits::SSwap)) {
            wchar_t from[40];
            name_of(member[s][sr.swap_out],from,40);name_of(member[s][sr.swap_in],name,40);
            swprintf_s(part,L"교대! %ls → %ls",from,name);add(part);
        }
        if(sr.special>=0 && sr.special<6 && kActionName[sr.special][0] && !take(kActionSticker[sr.special])) {
            name_of(a>=0 && a<kMembers?member[s][a]:nullptr,name,40);
            swprintf_s(part,L"%ls %ls!",name,kActionName[sr.special]);add(part);
        }
        // local306: the attacking side's critical (연속 공격 / 자동 견수 / 투지 상승) on its half
        if(sr.move>=5 && sr.move<=7 && !take(kMoveSticker[sr.move-5])) {
            name_of(a>=0 && a<kMembers?member[s][a]:nullptr,name,40);
            swprintf_s(part,L"%ls %ls!",name,kMoveName[sr.move-5]);add(part);
        }
        // local303: a seated fighter the join cut hit (the other side's stronger joiner) drops as the round starts — the incoming
        // fighter's writes (the round's HP-before cell of the live block and blk, the seat's HP and its copy) — so the cut shows apart
        // from the round's damage. duel_manual_join_cut_show: army side | round << 4 | block << 8, value HP before | after << 8.
        if(sr.join_cut>0 && sr.join_in<0 && sr.swap_in<0 && a>=0 && a<kMembers && a==seated[s]) {
            const int before=clamp_hp(R.hp_in[s]),hp=clamp_hp(R.hp_in[s]-sr.join_cut);
            Obj x=seat(m,s);
            dm::put32(x,0x3C,static_cast<unsigned>(hp));
            const int at=sc::row_base(static_cast<int>(row_of_side[s]))+4+k-1;
            reinterpret_cast<int *>(m+0x60)[at]=hp;blk[at]=hp;dm::put32(x,0x48+static_cast<unsigned>(4+k-1)*4,static_cast<unsigned>(hp));
            disp_hp[s][a]=hp;cut_shown=true;
            log_event("duel_manual_join_cut_show",s|(static_cast<unsigned>(k)<<4)|((blocks&0xF)<<8),static_cast<unsigned>(before)|(static_cast<unsigned>(hp)<<8),0);
        }
        if(a<0 || a>=kMembers || a==seated[s] || !member[s][a])continue;
        shown[s][a]=true;
        Obj in=member[s][a],out=member[s][seated[s]];
        const unsigned r=dm::swap_in(m,s,in,blocks);
        const int hp=clamp_hp(R.hp_in[s]);
        if(!r) {
            const int old=seated[s];
            seated[s]=a;++swaps;any=true;
            Obj x=seat(m,s);
            dm::put32(x,0x3C,static_cast<unsigned>(hp));if(dword(x,0x40)<static_cast<unsigned>(hp))dm::put32(x,0x40,static_cast<unsigned>(hp));
            const int at=sc::row_base(static_cast<int>(row_of_side[s]))+4+k-1;   // HP before round k: the incoming fighter's
            reinterpret_cast<int *>(m+0x60)[at]=hp;blk[at]=hp;dm::put32(x,0x48+static_cast<unsigned>(4+k-1)*4,static_cast<unsigned>(hp));
            dm::hud_guarded(m,s,in,member[s][0]);
            log_event("duel_manual_swap",s|(static_cast<unsigned>(k)<<4)|((blocks&0xF)<<8)|(joined?1u<<16:0u),dm::id_of(in)|(dm::id_of(out)<<16),
                      static_cast<unsigned>(hp)|(static_cast<unsigned>(a)<<8)|(static_cast<unsigned>(old)<<12));
        }
        else log_event("duel_manual_swap",s|(static_cast<unsigned>(k)<<4)|((blocks&0xF)<<8)|(r<<12)|(joined?1u<<16:0u),dm::id_of(in)|(dm::id_of(out)<<16),
                       static_cast<unsigned>(hp)|(static_cast<unsigned>(a)<<8)|(static_cast<unsigned>(seated[s])<<12));
    }
    join_peeked[0]=join_peeked[1]=-1;
    if(any){gauge_refresh(m);dm::bar_dirty=true;}
    else if(cut_shown)gauge_refresh(m);   // the seat's HP changed, the bench did not
    if(on_half[0]>=0 || on_half[1]>=0)duel_digits::publish_sticker(on_half[0],on_half[1],-1);
    if(on_half[0]>=0 || on_half[1]>=0 || text[0])
        log_event("duel_manual_sticker",static_cast<unsigned>(on_half[0]+1)|(static_cast<unsigned>(on_half[1]+1)<<4)|(left_side<<8)|(text[0]?1u<<9:0u),
                  static_cast<unsigned>(k)|((blocks&0xFF)<<8),0);
    if(text[0])caption(text);
}
// Bail-out: the original block and the +0xF1 of every member back, the original pair seated, the window gone (duel_manual_bail army
// why: 9 fault, 0x20 settlement at a release, 0x21 no 합 left at a release, 0x30 settlement at a skip, 0x31 engine at a skip; value block, hex seat HP).
static void bail(Obj m,unsigned why) {
    if(bailed)return;
    const bool was_holding=holding;
    if(was_holding)hold_finish(m,3);
    bailed=true;on=false;holding=hold_due=final_ready=flow_on=false;force_block=nullptr;
    release_window(9);
    if(scene && m==scene) {
        for(unsigned s=0;s<2;++s)
            if(seated[s] && member[s][0] && !dm::swap_in(m,s,member[s][0],blocks)){seated[s]=0;dm::hud_guarded(m,s,member[s][0],member[s][0]);}
        const unsigned state=dword(m,0x4A0);
        put_block(m,orig,state>=0x11);
        for(unsigned r=0;r<2;++r)if(arg_person[r])arg_person[r][0xF1]=saved_f1[r];
        for(unsigned s=0;s<2;++s)for(unsigned i=1;i<kMembers;++i)if(member[s][i])member[s][i][0xF1]=member_f1[s][i];
        if(was_holding && state==0xB)set_state(m,kind_of(orig));   // round 1 of the original block from here
    }
    log_event("duel_manual_bail",why,blocks,seat_hp(m));
}
// The engine's next 합 on stage (its fighters shown); a finished duel is settled first. duel_manual_block: army engine round | finished
// << 8 | computer << 9 | winner << 12 (0xF none, 2 draw) | end reason << 16, value codes side 0 rounds 1-5 (4 bits each) | side 1 << 20,
// hex HP after the 합 side 0 | side 1 << 8 | 투지 side 0 / 2 << 16 | side 1 / 2 << 24.
static void start_block(Obj m,int cmd,int who) {
    block_base=duel.round;
    for(unsigned s=0;s<2;++s)blk_morale[s]=duel.side[s].morale;
    block_n=play_round(cmd,who);
    if(!block_n) {bail(m,0x21);return;}   // no hold follows the end: never expected
    build_block(block_n);
    if(duel.finished && !settle(m)) {bail(m,0x20);return;}
    put_block(m,blk,false);
    show_round(m,1);
    unsigned c0=0,c1=0;
    for(int k=1;k<=kBlockRounds;++k){c0|=(static_cast<unsigned>(row_of(blk,0)[0x10+k])&0xF)<<((k-1)*4);c1|=(static_cast<unsigned>(row_of(blk,1)[0x10+k])&0xF)<<((k-1)*4);}
    log_event("duel_manual_block",(static_cast<unsigned>(duel.round)&0xFF)|(duel.finished?1u<<8:0u)|(cmd<0?1u<<9:0u)|
                  ((duel.finished?static_cast<unsigned>(duel.winner)&0xF:0xFu)<<12)|((static_cast<unsigned>(duel.end_reason)&0xF)<<16),c0|(c1<<20),
              (static_cast<unsigned>(row_of(blk,0)[3])&0xFF)|((static_cast<unsigned>(row_of(blk,1)[3])&0xFF)<<8)|
                  ((static_cast<unsigned>(duel.side[0].morale/2)&0xFF)<<16)|((static_cast<unsigned>(duel.side[1].morale/2)&0xFF)<<24));
    ++blocks;
}
static void original_scene(Obj m) {   // the engine did not start: the original block, manual off for the scene (extras stay away)
    auto_original=true;on=false;hold_due=false;
    put_block(m,orig,false);
    pages(m);
    set_state(m,kind_of(orig));
}
// The join hold (local305, user 「난입할때나오고」): the 합 looked ahead brings in a joiner (duel_manual_script flow_peeks: exact for a
// stance or a 교대). Right away, while the pair rides on: the joiner on its seat (duel_multi swap_in, the HUD, its HP after the join cut
// in the seat and the round-1 cell), its 「난입!」 sticker on its half (the caption without the sticker), the full hold's camera and
// 「확정」. Its 합 plays at this hold's end with the command class looked ahead with (sc::join_command: a stance chosen here, or the
// 교대); a 필살 or 교대 picked here waits for the 합 after (pend). show_round leaves the sticker out when that 합 starts.
// duel_manual_join_hold: army sides joined (bit 0 side 0, bit 1 side 1) | seated now << 4 (per side) | left sticker + 1 << 8 | right +
// 1 << 12 | caption << 16 | engine round << 20 (the 합 done), value joiner id side 0 | side 1 << 16 (0xFFFF none), hex command + 1 |
// seat HP side 0 << 8 | side 1 << 16.
static void enter_join_hold(Obj m,const s11::RoundResult &rr,int cmd,int who) {
    join_hold=true;join_cmd=cmd;join_who=who;hold_kind=sc::HoldJoin;++join_holds;
    list_open=0;pick=-1;pm_n=0;
    const bool overlay=!duel_digits::broken() && !ov_dead;
    const unsigned left_side=gauge_left_side(m);
    int on_half[2]={-1,-1};
    unsigned joined=0,seated_now=0,ids[2]={65535u,65535u},hps[2]={0,0};
    wchar_t text[256]=L"",name[40],part[96];
    bool any=false;
    for(unsigned s=0;s<2;++s) {
        const int j=rr.side[s].join_in;
        if(j<0 || j>=kMembers || !member[s][j])continue;
        joined|=1u<<s;
        if(!shown[s][j]){shown[s][j]=true;++joins;}
        join_peeked[s]=j;
        Obj in=member[s][j];
        ids[s]=dm::id_of(in)&65535u;
        const int hp=clamp_hp(duel.side[s].f[j].hp-rr.side[s].join_cut);
        hps[s]=static_cast<unsigned>(hp);
        if(j!=seated[s] && !dm::swap_in(m,s,in,blocks)) {
            seated[s]=j;++swaps;any=true;seated_now|=1u<<s;
            Obj x=seat(m,s);
            dm::put32(x,0x3C,static_cast<unsigned>(hp));if(dword(x,0x40)<static_cast<unsigned>(hp))dm::put32(x,0x40,static_cast<unsigned>(hp));
            const int at=sc::row_base(static_cast<int>(row_of_side[s]))+4;   // HP before the 합: the joiner's
            reinterpret_cast<int *>(m+0x60)[at]=hp;blk[at]=hp;dm::put32(x,0x48+4u*4u,static_cast<unsigned>(hp));
            dm::hud_guarded(m,s,in,member[s][0]);
        }
        const unsigned half=s==left_side?0u:1u;
        if(overlay && on_half[half]<0 && duel_digits::sticker_ready(duel_digits::SJoin))on_half[half]=duel_digits::SJoin;
        else {
            name_of(in,name,40);swprintf_s(part,L"%ls 난입!",name);
            if(text[0])wcsncat_s(text,L"   ",_TRUNCATE);
            wcsncat_s(text,part,_TRUNCATE);
        }
    }
    if(any){gauge_refresh(m);dm::bar_dirty=true;}
    if(on_half[0]>=0 || on_half[1]>=0)duel_digits::publish_sticker(on_half[0],on_half[1],-1);
    if(text[0])caption(text);
    hold_start=GetTickCount64();hold_clicked=false;
    hold_refocus();
    poll_arm();
    push_view();
    log_event("duel_manual_join_hold",joined|(seated_now<<4)|(static_cast<unsigned>(on_half[0]+1)<<8)|(static_cast<unsigned>(on_half[1]+1)<<12)|(text[0]?1u<<16:0u)|
                  ((static_cast<unsigned>(duel.round)&0xFF)<<20),ids[0]|(ids[1]<<16),(static_cast<unsigned>(cmd+1)&0xFF)|((hps[0]&0xFF)<<8)|((hps[1]&0xFF)<<16));
}
// The hold ends and its 합 plays (local305). duel_manual_release: army cause (1 확정, 3 자동 turned on in a full hold, 4 no click in a
// full hold within kAutoMs: the stance plays, the pick dropped, 5 kHoldCapMs: the pick plays, 6 the engine did not start, 7 the hold
// left by the round logic (hold_step), 8 the gap's end, 9 자동: no gap, local308, 10 the 「없음」 gap: no gap, local312) | kind << 4 | join hold << 7 | side camera mode << 8 | turned around << 10 |
// window << 11 | engine round << 12 (the 합 done) | look-ahead << 20 (0 none, 1 no join, 2 join: the join hold begins) | 자동 << 22,
// value frames held | clicks << 16, hex command + 1 | member + 1 << 4 | pending + 1 << 8 | hold ms / 10 << 12 (cap 0xFFFFF).
// duel_manual_flow (each 합 played from a hold): army engine round of the 합 | kind << 8 | cause << 12 | command + 1 << 16 | 자동 << 20 |
// join hold << 21, value hold ms | setup ms << 16 (the last 합's end to this hold), hex the last 합's play ms (each capped at 65535).
// duel_manual_pend_drop: army the pending command, value engine round (a 필살 the engine no longer allows, a 교대 with no member left).
static void release(Obj m,unsigned cause) {
    if(cause==6) {   // the engine did not start (no hold began): the original scene
        log_event("duel_manual_release",6,0,0);
        original_scene(m);return;
    }
    const ULONGLONG now=GetTickCount64();
    const unsigned held_ms=static_cast<unsigned>(now-hold_start);
    const bool was_full=full_hold(),was_join=join_hold;
    const int kind=hold_kind;
    if((cause==1 || cause==5) && pick>=0) {   // the pick goes in (cause 4: dropped)
        if(list_open==1 && pick<5){pend=duel_digits::kListSpc[pick];pend_who=-1;}
        else if(list_open==2 && pick<pm_n){pend=s11::CmdSwap;pend_who=pm[pick];}
        else if(list_open==3 && pick==0){pend=sc::kCmdRetreat;pend_who=-1;}   // local306
    }
    list_open=0;pick=-1;
    if(cause==3)auto_used=true;
    int cand[3];
    if((sc::plan_special(pend) && !special_ok(pend)) || (pend==s11::CmdSwap && !swap_cands(cand)) ||
       (sc::plan_retreat(pend) && !retreat_ok())) {   // no longer possible: the stance plays
        log_event("duel_manual_pend_drop",static_cast<unsigned>(pend),static_cast<unsigned>(duel.round),0);
        pend=pend_who=-1;
    }
    int who=-1,cmd=-1;
    auto log_release=[&](unsigned look) {
        const unsigned ms10=held_ms/10<1048575u?held_ms/10:1048575u;
        log_event("duel_manual_release",(cause&0xF)|((static_cast<unsigned>(kind)&7)<<4)|(was_join?1u<<7:0u)|((side_mode&3)<<8)|(flipped?1u<<10:0u)|(pane?1u<<11:0u)|
                      ((static_cast<unsigned>(duel.round)&0xFF)<<12)|((look&3)<<20)|(auto_on?1u<<22:0u),
                  (hold_frames&0xFFFF)|(clicks<<16),
                  (static_cast<unsigned>(cmd+1)&0xF)|((static_cast<unsigned>(who+1)&0xF)<<4)|((static_cast<unsigned>(pend+1)&0xF)<<8)|(ms10<<12));
    };
    unsigned look=0;
    if(was_join) {cmd=sc::join_command(join_cmd,cmd_stance);who=cmd==s11::CmdSwap?join_who:-1;}
    else {
        sc::FlowInput in;in.stance=cmd_stance;in.pend=pend;in.who=pend_who;in.autoplay=auto_on;
        cmd=sc::flow_command(in,who);
        if(cmd>=0 && cmd==pend)pend=pend_who=-1;   // a pending 필살 / 교대 goes in once
        if(sc::flow_peeks(cmd,false)) {   // the 합 looked ahead on a copy: a join gets its hold first
            s11::Duel probe=duel;
            int c[2]={s11::CmdAi,s11::CmdAi};
            c[player_side]=cmd;
            if(cmd==s11::CmdSwap)probe.swap_pick[player_side]=who;
            const s11::RoundResult rr=s11::step(probe,c);
            look=rr.side[0].join_in>=0 || rr.side[1].join_in>=0?2u:1u;
            if(look==2) {log_release(look);enter_join_hold(m,rr,cmd,who);return;}
        }
    }
    log_release(look);
    holding=false;join_hold=false;
    hold_finish(m,1);
    start_block(m,cmd,who);
    if(bailed)return;
    pages(m);
    set_state(m,kind_of(blk));
    flow_on=!duel.finished;t_round_start=now;
    auto cap16=[](unsigned v){return v<65535u?v:65535u;};
    log_event("duel_manual_flow",(static_cast<unsigned>(duel.round)&0xFF)|((static_cast<unsigned>(kind)&7)<<8)|((cause&0xF)<<12)|((static_cast<unsigned>(cmd+1)&0xF)<<16)|
                  (auto_on?1u<<20:0u)|(was_join?1u<<21:0u),cap16(held_ms)|(cap16(setup_ms)<<16),cap16(round_ms));
    if(!flow_on) {hide_window(1);return;}   // the duel's last 합: no strip over its end (the scene end releases the window)
    if(was_full)poll_arm();   // the list's plates go away under the cursor
    push_view();
}
static void ride(Obj m) {   // both duelists gallop in place (0x15); the mod moves them (an action change restarts the animation)
    for(unsigned s=0;s<2;++s){Obj x=seat(m,s);if(static_cast<int>(dword(x,0x10))!=kActStill)fn<void(__fastcall *)(Obj,int)>(dm::kAction)(x,kActStill);}
}
// local312: would the gap's 합 bring a join? The release's own decision (a pending 필살 / 교대 / 퇴각 no longer possible plays the
// stance; the command; the look-ahead on a copy) without its side effects, for the 「없음」 gap at its hold's start.
static bool join_ahead() {
    int p=pend,pw=pend_who,cand[3];
    if((sc::plan_special(p) && !special_ok(p)) || (p==s11::CmdSwap && !swap_cands(cand)) || (sc::plan_retreat(p) && !retreat_ok()))p=pw=-1;
    sc::FlowInput in;in.stance=cmd_stance;in.pend=p;in.who=pw;in.autoplay=auto_on;
    int who=-1;
    const int cmd=sc::flow_command(in,who);
    if(!sc::flow_peeks(cmd,false))return false;
    s11::Duel probe=duel;
    int c[2]={s11::CmdAi,s11::CmdAi};
    c[player_side]=cmd;
    if(cmd==s11::CmdSwap)probe.swap_pick[player_side]=who;
    const s11::RoundResult rr=s11::step(probe,c);
    return rr.side[0].join_in>=0 || rr.side[1].join_in>=0;
}
// duel_manual_hold: army engine round of the next 합 | kind << 8 (sc::HoldKind) | window ok << 11 | heading forced << 12 | 자동 << 13 |
// list << 14, value holds | clicks << 16, hex 투지 player | 투지 computer << 12 | stance player << 24 | stance computer << 28.
static void hold_begin(Obj m) {
    hold_due=false;
    const bool first=!engine_on;
    if(first && !eng_start(m)) {log_event("duel_manual_hold",0xFF,0,0);release(m,6);return;}
    const ULONGLONG now=GetTickCount64();
    setup_ms=t_round_end?static_cast<unsigned>(now-t_round_end):0u;
    round_ms=t_round_end && t_round_start && t_round_end>=t_round_start?static_cast<unsigned>(t_round_end-t_round_start):0u;
    holding=true;hold_frames=0;hold_start=now;++holds;rebuilds=0;hold_clicked=false;join_hold=false;
    list_open=0;pick=-1;pm_n=0;
    hold_kind=sc::next_hold(first,want_hold,auto_on);want_hold=sc::HoldGap;
    int cand[3];
    if(hold_kind==sc::HoldSpecial){if(any_special())open_list(1);else hold_kind=sc::HoldGap;}
    else if(hold_kind==sc::HoldSwap){if(swap_cands(cand))open_list(2);else hold_kind=sc::HoldGap;}
    else if(hold_kind==sc::HoldRetreat){if(retreat_ok())open_list(3);else hold_kind=sc::HoldGap;}   // local306
    {   // the pair as the original seated it (hs_*, untouched): middle, heading (mean of both), each seat in that frame
        Obj a=seat(m,0),b=seat(m,1);
        const float ha=dm::flt(a,0xF0),hb=dm::flt(b,0xF0);
        hs_h=ph0=ph=std::atan2(std::sin(ha)+std::sin(hb),std::cos(ha)+std::cos(hb));
        hs_cx=pc_x=(dm::flt(a,0xC0)+dm::flt(b,0xC0))*0.5f;hs_cz=pc_z=(dm::flt(a,0xC8)+dm::flt(b,0xC8))*0.5f;
        float fx,fz;dir_of(hs_h,fx,fz);
        heading_forced=false;
        for(unsigned s=0;s<2;++s) {
            Obj x=seat(m,s);
            const float dx=dm::flt(x,0xC0)-pc_x,dz=dm::flt(x,0xC8)-pc_z;
            hs_f[s]=dx*fx+dz*fz;hs_l[s]=dx*fz-dz*fx;hs_dh[s]=wrap_pi(dm::flt(x,0xF0)-hs_h);
            if(std::fabs(hs_dh[s])>kSameHeading)heading_forced=true;   // not riding the same way: the ride eases both to one heading
        }
        memcpy(off_f,hs_f,sizeof off_f);memcpy(off_l,hs_l,sizeof off_l);memcpy(off_dh,hs_dh,sizeof off_dh);
    }
    side_mode=0;side_wait=true;gap_shifted=false;gap_left_ms=0;verify_due=flipped=orig_ok=false;ride_t=0.0f;side_tries=eye_clamps=ground_misses=0;qpc_last.QuadPart=0;
    fol_f_ok=fol_written=fol_stopped=dec_ok=false;fol_frames=fol_off=ride_frames=0;ride_len=0.0f;ride_dmin=1e9f;ride_dmax=-1e9f;
    // local308 (user 「자동 버튼 누르면 끝까지 쭈욱 가는거야 짧은대기없이 교체 난입 다 포함해서 자연스럽게」): while 자동 plays, no gap: the next
    // 합 plays from the seats at once (release cause 9). The engine's own 교대 / 난입 show as their 합 starts (no look-ahead under
    // 자동, so no join hold); a stance / 필살 / 교대 / 자동 press stops 자동 and the holds come back. The 합-to-합 cut is the dissolve
    // (duel_digits, local308).
    if(hold_kind==sc::HoldGap && auto_on && !first) {++auto_flows;release(m,9);return;}
    // local312 (user 「중간에 넣은 짧은대기 빼고 스피디하게 진행하는거 테스트 해보고 싶어」): the installer's 「없음」 plays the next 합 at
    // once as 자동 does (release cause 10), unless that 합 brings a join (join_ahead: the release's own look-ahead): then the gap
    // begins as usual (camera, window) and ends on its first poll, so the join hold starts from a set-up hold as with a short gap.
    if(hold_kind==sc::HoldGap && !first && sc::gap_none(mod_features.duel_relay_gap) && !join_ahead()) {++auto_flows;release(m,10);return;}
    ride(m);
    hold_ride(m);   // the camera from this first frame when the original's block is there (the turn-around is this cut)
    const bool full=hold_kind!=sc::HoldGap;
    unsigned w=0;
    if(!pane){poll_arm();w=build_guarded();}   // the first hold (or after a release of the owner's panes)
    else {if(full)poll_arm();push_view();}     // a gap keeps the strip under the cursor: a press begun in the 합 still counts
    if(full)++full_holds;else ++gap_holds;
    const s11::Side &me=duel.side[player_side],&cpu=duel.side[player_side^1u];
    log_event("duel_manual_hold",(static_cast<unsigned>(duel.round+1)&0xFF)|((static_cast<unsigned>(hold_kind)&7)<<8)|(w?0u:1u<<11)|(heading_forced?1u<<12:0u)|
                  (auto_on?1u<<13:0u)|((static_cast<unsigned>(list_open)&3)<<14),holds|(clicks<<16),
              (static_cast<unsigned>(me.morale)&0xFFF)|((static_cast<unsigned>(cpu.morale)&0xFFF)<<12)|((static_cast<unsigned>(me.stance)&0xF)<<24)|
                  ((static_cast<unsigned>(cpu.stance)&0xF)<<28));
}
// A gap hold becomes a full one (local305, 필살 / 교대 pressed in it, user 「필살버튼 누를때 나오는거야」): its list, 「확정」, the full
// hold's camera, the clock from now (a gap turned full counts in both duel_manual_scene2 counters).
static void hold_full(int kind) {
    hold_kind=kind;open_list(kind==sc::HoldSpecial?1:kind==sc::HoldRetreat?3:2);
    hold_start=GetTickCount64();hold_clicked=false;++full_holds;
    hold_refocus();
}
// duel_manual_click: army node (2-5 stances, 6 필살, 7 교대, 8 자동, 9 확정, 10-15 list places) | path << 8 (1 native, 2 poll on the
// control rectangle, 3 poll on our geometry) | kind << 12 (7 between holds) | list << 15, value frames held, hex stance | pending + 1
// << 4 | wanted hold << 8 | 자동 << 11 | pick + 1 << 12 | clicks << 16.
static void on_click(Obj m,int n,unsigned path) {
    hold_clicked=true;++clicks;
    const bool full=full_hold();
    bool rearm=false;
    const int ret=n==NSwapBtn?ret_state():0;   // local306: the 교대 place pressed as 퇴각
    if(n>=NSt0 && n<NSt0+4){cmd_stance=n-NSt0;auto_on=false;}
    else if(n==NSwapBtn && ret==3) {}   // 퇴각 short of 투지 (a disabled button takes no click; a poll on its plate might)
    else if(n==NSpcBtn || n==NSwapBtn) {
        const int want=n==NSpcBtn?sc::HoldSpecial:ret?sc::HoldRetreat:sc::HoldSwap,which=n==NSpcBtn?1:ret?3:2;
        auto_on=false;
        if(full) {   // this hold's list: opened, switched, or closed again
            if(list_open==which){list_open=0;pick=-1;}
            else open_list(which);
            rearm=true;
        }
        else if(holding){hold_full(want);rearm=true;}   // a gap: the full hold now
        else {   // during a 합: the hold after it; pressed again (or over a pending one of its kind) cancels
            const bool mine=which==1?sc::plan_special(pend):which==3?sc::plan_retreat(pend):pend==s11::CmdSwap;
            if(want_hold==want || mine){want_hold=sc::HoldGap;if(mine)pend=pend_who=-1;}
            else want_hold=want;
        }
    }
    else if(n==NAutoBtn) {
        auto_on=!auto_on;
        if(auto_on){want_hold=sc::HoldGap;pend=pend_who=-1;}   // the computer plays: nothing waits
    }
    else if(n>=NList && n<NList+6 && full) {
        const int k=n-NList;
        if(k==5){list_open=0;pick=-1;rearm=true;}
        else if(list_open==1 && special_ok(duel_digits::kListSpc[k]))pick=pick==k?-1:k;
        else if(list_open==2 && k<pm_n)pick=pick==k?-1:k;
        else if(list_open==3 && k==0 && retreat_ok())pick=pick==0?-1:0;
    }
    log_event("duel_manual_click",static_cast<unsigned>(n)|(path<<8)|((holding?static_cast<unsigned>(hold_kind)&7:7u)<<12)|((static_cast<unsigned>(list_open)&3)<<15),hold_frames,
              (static_cast<unsigned>(cmd_stance)&0xF)|((static_cast<unsigned>(pend+1)&0xF)<<4)|((static_cast<unsigned>(want_hold)&7)<<8)|(auto_on?1u<<11:0u)|
                  ((static_cast<unsigned>(pick+1)&0xF)<<12)|(clicks<<16));
    if(n==NOk && full){release(m,1);return;}
    if(n==NAutoBtn && auto_on && full){release(m,3);return;}
    if(rearm)poll_arm();   // the plates under the cursor changed (a list opened or closed)
    push_view();
}
// duel_manual_hold_probe (held frames 1, 30, 60, 120, 300, 600): army frame | side camera mode << 16 | clip << 18 | turned around <<
// 19, value turn from the hold-start heading in degrees + 1000 | eye clamps << 16, hex camera centre x | z << 16 (stage +0x5D0, whole
// units, low 16 bits).
static void hold_step(Obj m,unsigned state,unsigned round) {
    if(state!=0xB || round!=1) {   // anything else: the round logic takes over (a skip goes through end_sync)
        const bool was_full=full_hold();
        holding=false;join_hold=false;list_open=0;pick=-1;hold_finish(m,2);
        log_event("duel_manual_release",7|((static_cast<unsigned>(hold_kind)&7)<<4),hold_frames|(clicks<<16),state|(round<<8));
        if(was_full)poll_arm();
        push_view();
        return;
    }
    ++hold_frames;
    dm::flt(m,0x4B0)=0.0f;
    ride(m);
    hold_ride(m);
    for(unsigned f:kProbe)
        if(hold_frames==f) {
            Obj st=dm::stage_obj();
            constexpr unsigned kLow16=65535;   // low 16 bits of the camera centre; all ones = no stage object
            const unsigned cx=st?static_cast<unsigned>(static_cast<int>(dm::flt(st,0x5D0)))&kLow16:kLow16,cz=st?static_cast<unsigned>(static_cast<int>(dm::flt(st,0x5D8)))&kLow16:kLow16;
            log_event("duel_manual_hold_probe",f|((side_mode&3)<<16)|(st && pointer(st,0x5C8)?1u<<18:0u)|(flipped?1u<<19:0u),
                      (static_cast<unsigned>(static_cast<int>(wrap_pi(ph-hs_h)*180.0f/kPi)+1000)&0xFFFF)|((eye_clamps&0xFFFF)<<16),cx|(cz<<16));
        }
    if(!pane && (!dm::bar_dirty || dm::bar_failed) && rebuilds<kRebuilds){++rebuilds;build_guarded();}
    ov_check();
    unsigned path=0;
    const int n=take_click(path);
    if(n>=0) {on_click(m,n,path);if(!holding)return;}
    const unsigned long long waited=GetTickCount64()-hold_start;
    if(hold_kind==sc::HoldGap) {if(waited>=gap_len())release(m,8);return;}   // local305: the gap rides its length (local308: installer), no click needed
    if(!hold_clicked && waited>=kAutoMs) {++autos;release(m,4);return;}
    if(waited>=kHoldCapMs) {++autos;release(m,5);return;}
}
// Between holds once a 합 was played (local305): the strip stays up and takes clicks for the 합 after the one on stage (a waiting-bar
// rebuild that took the owner's panes builds it again).
static void flow_step(Obj m) {
    if(!pane && (!dm::bar_dirty || dm::bar_failed) && rebuilds<kRebuilds){++rebuilds;build_guarded();}
    ov_check();
    unsigned path=0;
    const int n=take_click(path);
    if(n>=0)on_click(m,n,path);
}
// 기합 (local311, user 「배포 끝나고 기합에 따로거는 모션 그거 꺼줘 로컬에는」): no pose of its own any more. local295-310 set the
// unused action 0x14 on its duelist at state 0xB of its round; 37D930 moves 0x0C (the charge) along the heading but never 0x14,
// so the shouter stopped mid-charge and the pair met off their marks (MANUAL_DUEL 10-16). 기합 shows by its sticker and caption.

// ---- scene decision ----
// duel_manual_begin: army why (0 relay, 1 no presentation, 2 no entry snapshot, 3 duel_multi round / effect / damage / 일격 hooks
// missing, 4 no UI owner row, 5 block changed since the entry, 6 arg rows, 7 persons, 8 +0xF1 changed since the entry, 0x11 win/loss
// pair, 0x12 일격 (the original scene), 0x13 start HP) | player row << 8, value ids row 0 << 16 | row 1, hex start HP row 0 | row 1
// << 8 | original final HP row 0 << 16 | row 1 << 24.
static void decide(Obj m) {
    if(dword(m,0x4A0)<2 || !pointer(seat(m,0),dm::kModel) || !pointer(seat(m,1),dm::kModel))return;   // as duel_multi after()
    decided=true;
    const int prow=static_cast<int>(dword(m,0x3C));
    unsigned why=0;
    if(prow<0 || prow>1)why=1;
    else if(!entry_ok || entry_serial!=dm::scene_serial)why=2;
    else if(!dm::hooks_all || !dm::damage_hooked || !dm::onehit_hooked)why=3;
    else if(!dm::bar_registered)why=4;
    else if(memcmp(orig,m+0x60,sizeof orig))why=5;
    else if(!sc::valid_pair(orig))why=0x11;
    else if(orig[2])why=0x12;
    else {
        for(unsigned s=0;s<2;++s)row_of_side[s]=dword(seat(m,s),0xA4);
        if(row_of_side[0]>1 || row_of_side[1]>1 || row_of_side[0]==row_of_side[1])why=6;
        else {
            for(unsigned s=0;s<2;++s)side_of_row[row_of_side[s]]=s;
            for(unsigned r=0;r<2 && !why;++r)
                if(!valid(arg_person[r]) || pointer(m,0x20+r*8)!=arg_person[r] || pointer(seat(m,side_of_row[r]),0xB0)!=arg_person[r])why=7;
            for(unsigned r=0;r<2 && !why;++r)if(static_cast<unsigned char>(arg_person[r][0xF1])!=saved_f1[r])why=8;
            for(unsigned r=0;r<2 && !why;++r){const int hp=orig[sc::row_base(static_cast<int>(r))+2];if(hp<1 || hp>100)why=0x13;}
        }
    }
    log_event("duel_manual_begin",why|((static_cast<unsigned>(prow)&0xF)<<8),(dm::id_of(arg_person[0])<<16)|dm::id_of(arg_person[1]),
              (static_cast<unsigned>(orig[sc::row_base(0)+2])&0xFF)|((static_cast<unsigned>(orig[sc::row_base(1)+2])&0xFF)<<8)|
              ((static_cast<unsigned>(orig[sc::row_base(0)+3])&0xFF)<<16)|((static_cast<unsigned>(orig[sc::row_base(1)+3])&0xFF)<<24));
    if(why)return;
    scene=m;on=true;hold_due=true;player_side=side_of_row[prow];
    const int hp[2]={orig[sc::row_base(static_cast<int>(row_of_side[0]))+2],orig[sc::row_base(static_cast<int>(row_of_side[1]))+2]};
    neutral(blk,hp);
    put_block(m,blk,false);   // before state 7 seats the clash HP from row +4
}
// duel_manual_end (first state >= 0x11): army block | finished before << 4 | original << 5 | holding << 6 | bailed << 7 | state 0x11
// restarted << 8 | computer rounds << 12, value seat HP, hex state | engine rounds << 8 | winner << 16.
static void end_sync(Obj m) {
    if(ended)return;
    ended=true;
    const bool was_holding=holding,done=final_ready;
    if(was_holding)hold_finish(m,4);
    holding=hold_due=flow_on=join_hold=false;list_open=0;pick=-1;
    if(cap_want || cap_shown)hide_window(2);   // the end caption stays until it is due (the scene end releases the owner)
    else release_window(2);
    const unsigned state=dword(m,0x4A0);
    bool restart=false;
    unsigned extra=0;
    if(on && !bailed && !auto_original && !final_ready) {   // a skip: the computer plays the rest at once
        if(!engine_on && !eng_start(m))bail(m,0x31);
        else {
            for(unsigned s=0;s<2;++s)for(unsigned i=0;i<kMembers;++i)pre_hp[s][i]=duel.side[s].f[i].hp;
            while(!duel.finished && extra<64){const int cmd[2]={s11::CmdAi,s11::CmdAi};eng_round(cmd,rounds[0]);++extra;}
            const int hp[2]={clamp_hp(s11::current(duel,0).hp),clamp_hp(s11::current(duel,1).hp)};
            neutral(blk,hp);
            if(!settle(m))bail(m,0x30);
            else {
                for(unsigned s=0;s<2;++s) {
                    const int a=duel.side[s].active;
                    if(a!=seated[s] && member[s][a] && !dm::swap_in(m,s,member[s][a],blocks)){seated[s]=a;shown[s][a]=true;dm::hud_guarded(m,s,member[s][a],member[s][0]);}
                }
                put_block(m,blk,true);
                gauge_refresh(m);
                end_caption(m);
                if(state==0x11){set_state(m,0x11);restart=true;}
            }
        }
    }
    log_event("duel_manual_end",(blocks&0xF)|(done?1u<<4:0u)|(auto_original?1u<<5:0u)|(was_holding?1u<<6:0u)|(bailed?1u<<7:0u)|(restart?1u<<8:0u)|((extra&0xFF)<<12),
              seat_hp(m),(state&0xFF)|((static_cast<unsigned>(duel.round)&0xFF)<<8)|((static_cast<unsigned>(duel.winner)&0xFF)<<16));
}

// ---- dispatch (duel_multi hooks) ----
// duel_manual_state: army state | round << 8 | holding << 12 | hold kind << 13 | engine round << 16, value frames held, hex 합 played
// from holds. Up to kStateLogs per scene.
static void frame_raw(Obj m) {
    if(seen_serial!=dm::scene_serial) {   // a new scene entered through 376020
        seen_serial=dm::scene_serial;
        if(scene)release_window(2);
        reset();
    }
    if(!dword(m,0x588)) {
        if(scene) {
            // duel_manual_scene: army holds | clicks << 8 | auto releases << 16 | bailed << 24 | original << 25 | 자동 used << 26 |
            // final pair applied << 27 | applied twice << 28, value swaps | joins << 8 | records << 16 | poll clicks << 24, hex +0xF1 final
            // pair row 0 | row 1 << 8 | engine rounds << 16 | winner << 24. duel_manual_scene2: army 무쌍 replays | (0: no 기합 pose since local311) << 8 | turnbacks
            // << 16 | 합 played straight on under 자동 << 24 (local308), value captions built | join misses << 16, hex gap holds | full
            // holds << 12 (a gap turned full counts in both) | join holds << 20 (local305).
            log_event("duel_manual_scene",(holds&0xFF)|((clicks&0xFF)<<8)|((autos&0xFF)<<16)|(bailed?1u<<24:0u)|(auto_original?1u<<25:0u)|
                          (auto_used?1u<<26:0u)|(applied?1u<<27:0u)|(applied_twice?1u<<28:0u),
                      (swaps&0xFF)|((joins&0xFF)<<8)|((records&0xFF)<<16)|((polls&0xFF)<<24),
                      (fin_person[0]?static_cast<unsigned char>(fin_person[0][0xF1]):0xFFu)|((fin_person[1]?static_cast<unsigned char>(fin_person[1][0xF1]):0xFFu)<<8)|
                          ((static_cast<unsigned>(duel.round)&0xFF)<<16)|((static_cast<unsigned>(duel.winner)&0xFF)<<24));
            log_event("duel_manual_scene2",(combos&0xFF)|((snaps&0xFF)<<16)|((auto_flows&0xFF)<<24),(cap_builds&0xFFFF)|(join_misses<<16),
                      (gap_holds&0xFFF)|((full_holds&0xFF)<<12)|((join_holds&0xFF)<<20));
            release_window(2);
            number_forget();   // the gauge goes with the scene
            scene=nullptr;on=false;holding=flow_on=false;
        }
        decided=true;return;
    }
    if(!decided) {decide(m);return;}
    if(!scene || m!=scene)return;
    const unsigned state=dword(m,0x4A0),round=dword(m,0x4A8);
    if(state!=last_state) {
        last_state=state;
        if(state_logs<kStateLogs) {
            ++state_logs;
            log_event("duel_manual_state",(state&0xFF)|((round&0xF)<<8)|(holding?1u<<12:0u)|((static_cast<unsigned>(hold_kind)&7)<<13)|((static_cast<unsigned>(duel.round)&0xFF)<<16),
                      hold_frames,blocks);
        }
    }
    if(state>=0x11) {end_sync(m);number_step(m,state,round);if(!bailed)caption_step(m);return;}
    number_step(m,state,round);   // also gives the digits back after a bail / the original scene
    if(bailed || !on)return;
    caption_step(m);
    if(holding) {hold_step(m,state,round);return;}
    if(hold_due && state==0xB && round==1) {hold_begin(m);return;}
    if(flow_on)flow_step(m);
}
static void fault(Obj m) {
    __try {log_event("duel_manual_exception",1,blocks,dword(m,0x4A0));bail(m,9);}
    __except(EXCEPTION_EXECUTE_HANDLER) {bailed=true;on=false;holding=hold_due=final_ready=flow_on=false;force_block=nullptr;owner_released();}
}
static void frame(Obj m) {
    if(!mod_features.duel_relay || !m)return;
    __try {frame_raw(m);}
    __except(EXCEPTION_EXECUTE_HANDLER) {fault(m);}
}
// A relay scene (also after 「자동」 put the original block back): duel_multi picks its extras within 2 hexes; no other scene runs it.
static bool scene_on(Obj m) {return mod_features.duel_relay && scene && m==scene && !bailed;}
static void entry(Obj m) {
    if(!mod_features.duel_relay || !m)return;
    __try {
        memcpy(orig,m+0x60,sizeof orig);
        for(unsigned r=0;r<2;++r){arg_person[r]=pointer(m,0x20+r*8);saved_f1[r]=arg_person[r]?static_cast<unsigned char>(arg_person[r][0xF1]):0;}
        entry_ok=true;entry_serial=dm::scene_serial;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {entry_ok=false;log_event("duel_manual_exception",3,0,0);}
}
// The next 회심 hit of a 무쌍 round r: the receiver's HP before / after round r and the striker's damage take the next step, both seats'
// row copies and the receiver's seat HP follow, then state 0xD plays round r again (the round counter stays: 37D1C0 is not called).
// duel_manual_combo: army side hit | round << 4 | hit << 8 | hits << 12 | block << 16, value HP from | to << 8 | engine HP << 16, hex
// page 4 shown.
static void combo_next(Obj m,int r) {
    Combo &c=combo[r-1];
    const unsigned o=static_cast<unsigned>(c.side),s=o^1u;
    int *ro=row_of(blk,o),*rs=row_of(blk,s);
    const int from=c.seq[c.i],to=c.seq[c.i+1];
    ro[4+r-1]=from;ro[4+r]=to;rs[0xA+r]=from-to;
    ++c.i;
    memcpy(m+0x60,blk,0xC0);
    for(unsigned x=0;x<2;++x)memcpy(seat(m,x)+0x48,row_of(blk,x),0x58);
    dm::put32(seat(m,o),0x3C,static_cast<unsigned>(from));
    const bool shown4=page4(m);
    set_state(m,0xD);
    ++combos;
    log_event("duel_manual_combo",o|(static_cast<unsigned>(r)<<4)|(static_cast<unsigned>(c.i)<<8)|(static_cast<unsigned>(c.n)<<12)|((blocks&0xF)<<16),
              (static_cast<unsigned>(from)&0xFF)|((static_cast<unsigned>(to)&0xFF)<<8)|((static_cast<unsigned>(c.seq[c.n])&0xFF)<<16),shown4?1u:0u);
}
// Round r of the block on stage ended (+0x4A8 = r): the next hit of a 무쌍, the next round's fighters, or the end of the block.
static bool round_end_raw(Obj m) {
    const int r=static_cast<int>(dword(m,0x4A8));
    if(r>=1 && r<=block_n && combo[r-1].n>=2 && combo[r-1].i<combo[r-1].n) {combo_next(m,r);return true;}
    if(r>=1 && r<block_n) {show_round(m,r+1);return false;}   // the original round end goes on to round r + 1
    if(r!=block_n)return false;
    t_round_end=GetTickCount64();   // local305 duel_manual_flow: the 합's play time, the setup to the next hold
    for(unsigned s=0;s<2;++s)for(unsigned i=0;i<kMembers;++i)disp_hp[s][i]=rounds[block_n-1].hp[s][i];
    if(block_n>=1 && rounds[block_n-1].r.extended)extend_note();   // local312
    if(final_ready) {
        end_caption(m);
        if(r<kBlockRounds && dword(seat(m,0),0x3C) && dword(seat(m,1),0x3C)) {set_state(m,0x11);return true;}   // ended without a KO (50 hits)
        return false;   // KO or round 5: the original end
    }
    {
        const int hp[2]={clamp_hp(s11::current(duel,0).hp),clamp_hp(s11::current(duel,1).hp)};
        neutral(blk,hp);put_block(m,blk,false);hold_due=true;
    }
    pages(m);
    set_state(m,9);   // the original round-1 setup, then state 0xB: the next hold (or the block the computer just played)
    return true;
}
static bool round_end(Obj m) {
    if(!mod_features.duel_relay || !on || bailed || !scene || m!=scene || !engine_on || !block_n)return false;
    __try {return round_end_raw(m);}
    __except(EXCEPTION_EXECUTE_HANDLER) {fault(m);return false;}
}
static bool force_damage(int *block) {
    if(!force_block || block!=force_block || GetCurrentThreadId()!=force_thread)return false;
    sc::force_targets(block,force_target);++force_hits;
    if(force_raise)
        for(int row=0;row<2;++row)if(block[sc::row_base(row)+4]<force_target[row])block[sc::row_base(row)+4]=force_target[row];
    return true;
}
static bool force_onehit() {
    if(!force_block || GetCurrentThreadId()!=force_thread)return false;
    ++onehit_skips;return true;
}
static void note_effects(int *) {}
// ---- results (local308) ----
// User (local307 play) 「승자 패자는 팀으로 적용되는데 사기증가/ 사기저하가 한명한테만 적용되서 그래. 그건 공통으로 가져가야지」 and 「싸운무장 모두
// 받는데 … 체력 패자 체력 60이하 승자체력 30이하로 바꾸자」: before 37AA90 gives the final pair the original result, every other member
// who fought (engine state 1, settle()'s bench rows: +0xF1 = the engine HP) takes its side's original per-person effect against the
// final fighter across —
//   winners: 37B170(member, final loser) (injury at HP <= 141A4C3F4 = 30, 2E9820, record, its unit's morale +141A4C3FC = 40) and
//            2BBED0(member, 0, final loser) (merit, 무력 exp);
//   losers:  37AD40(final winner, member, fate 0, kNoReport) (its unit's morale loss by the original formula, record, injury) and
//            2BBED0(member, 1, final winner); 37AD40's fourth argument only orders the 37B420 report pair (0 / 1), any other value
//            leaves the report out (one report a duel: the final pair's);
//   a draw:  37AB70 takes both of a pair, so per member: injury at HP <= 141A4C400 (27F250(member, 3, opponent, 0) as every duel
//            injury site calls it: r9 0), 2E9820, its unit's morale + 141A4C408, 2BBED0(member, 2, opponent).
// Members first: a capture or death of the final loser takes its unit, and 37B170 / 2BBED0 return early on an invalid opponent. A
// member whose unit already took a result (shared or missing unit) takes merit only. Capture and death stay the final loser's.
// The loser injury (37AD40, HP < 141A4C3DC = 75, call 37B016) during these calls and the final pair's: only HP <= kLoseInjury
// (duel_multi injury_hook; without the hook the original 75).
static const unsigned kWinFx=GA(GA_37B170),kLoseFx=GA(GA_37AD40),kMerit=GA(GA_2BBED0),kInjure=GA(GA_27F250),kRecover=GA(GA_2E9820),
                      kMoraleAdd=GA(GA_225240),kDrawInjury=GA(GA_1A4C400),kDrawMorale=GA(GA_1A4C408);
static const int kLoseInjury=60,kNoReport=2;
static DWORD fx_thread;
static bool fx_on;
static unsigned fx_rolls,fx_skips;   // loser injury calls let through / left out (HP above kLoseInjury) in this apply
static bool injury_skip(Obj person) {
    if(!fx_on || !person || GetCurrentThreadId()!=fx_thread)return false;
    const bool skip=static_cast<int>(static_cast<unsigned char>(person[0xF1]))>kLoseInjury;
    ++(skip?fx_skips:fx_rolls);
    return skip;
}
static Obj unit_of(Obj p) {return p?fn<Obj(__fastcall *)(Obj,int)>(GA(GA_22BAB0))(p,1):nullptr;}
struct FxSnap {unsigned hp,grade,morale;};
static FxSnap fx_snap(Obj p,Obj unit) {
    FxSnap s{static_cast<unsigned char>(p[0xF1]),static_cast<unsigned char>(p[0x161]),0xFFu};
    if(valid(unit))s.morale=static_cast<unsigned char>(unit[0x1A]);
    return s;
}
// duel_manual_team (per person): army side | slot << 4 | kind << 8 (0 win, 1 lose, 2 draw) | final pair << 11 | injury grade rose << 12
// | fault << 13 | unit shared or missing << 14, value id | opponent id << 16, hex +0xF1 before | grade before << 8 | grade after << 12 |
// unit +0x1A (morale) before << 16 | after << 24 (0xFF no unit). The final pair's rows come after 37AA90 (its loser's unit may be gone).
static void fx_log(unsigned s,unsigned slot,unsigned kind,bool final_pair,bool fault,bool shared,Obj p,Obj opp,const FxSnap &a,const FxSnap &b) {
    log_event("duel_manual_team",(s&1)|((slot&0xF)<<4)|((kind&7)<<8)|(final_pair?1u<<11:0u)|(b.grade>a.grade?1u<<12:0u)|(fault?1u<<13:0u)|
                  (shared?1u<<14:0u),
              dm::id_of(p)|(dm::id_of(opp)<<16),
              (a.hp&0xFF)|((a.grade&0xF)<<8)|((b.grade&0xF)<<12)|((a.morale&0xFF)<<16)|((b.morale&0xFF)<<24));
}
static void draw_one(Obj p,Obj opp,Obj unit) {
    const int cap=static_cast<int>(dword(image,kDrawInjury));
    if(static_cast<unsigned char>(p[0x161])<2 && static_cast<int>(static_cast<unsigned char>(p[0xF1]))<=cap)
        fn<void *(__fastcall *)(Obj,int,Obj,int)>(kInjure)(p,3,opp,0);
    fn<unsigned long long(__fastcall *)(Obj)>(kRecover)(p);
    if(valid(unit))fn<void(__fastcall *)(Obj,int)>(kMoraleAdd)(unit,static_cast<int>(dword(image,kDrawMorale)));
}
static void member_fx(unsigned kind,Obj p,Obj opp,Obj unit,bool shared) {
    if(!shared) {
        if(kind==0)fn<void(__fastcall *)(Obj,Obj)>(kWinFx)(p,opp);
        else if(kind==1)fn<void(__fastcall *)(Obj,Obj,int,int)>(kLoseFx)(opp,p,0,kNoReport);
        else draw_one(p,opp,unit);
    }
    fn<void(__fastcall *)(Obj,int,Obj)>(kMerit)(p,static_cast<int>(kind),opp);
}
// The members' effects. w = block[4] (row 0's W/L: 0 row 0 won, 1 row 1 won, 2 draw) must agree with the engine's winner side through
// row_of_side; otherwise (or an unresolved address) nothing is applied: duel_manual_team army 0x8000 | w, value engine winner, hex
// resolved. Returns the members applied.
static unsigned team_effects(const int *block) {
    const int w=block[4];
    const bool draw=w==2,won=duel.winner==0 || duel.winner==1;
    const bool match=draw?!won:(w==0 || w==1) && won && static_cast<int>(row_of_side[duel.winner])==w;
    const bool resolved=kWinFx && kLoseFx && kMerit && kInjure && kRecover && kMoraleAdd && kDrawInjury && kDrawMorale;
    if(!match || !resolved) {
        constexpr unsigned kTeamSkipped=32768;   // army bit 15: nothing applied
        log_event("duel_manual_team",kTeamSkipped|(static_cast<unsigned>(w)&0xFF),static_cast<unsigned>(duel.winner)&0xFF,resolved?1u:0u);
        return 0;
    }
    Obj units[2*kMembers]={};
    unsigned nu=0,done=0;
    for(unsigned r=0;r<2;++r){Obj u=unit_of(fin_person[r]);if(valid(u))units[nu++]=u;}
    for(unsigned s=0;s<2;++s)for(unsigned i=0;i<kMembers;++i) {
        Obj p=member[s][i];
        if(!p || static_cast<int>(i)==duel.side[s].active || duel.side[s].f[i].state!=1)continue;
        const unsigned row=row_of_side[s];
        Obj opp=fin_person[row^1u];
        if(!opp)continue;
        const unsigned kind=draw?2u:static_cast<int>(row)==w?0u:1u;
        Obj u=unit_of(p);
        bool shared=!valid(u);
        for(unsigned k=0;k<nu && !shared;++k)if(units[k]==u)shared=true;
        if(!shared && nu<2*kMembers)units[nu++]=u;
        const FxSnap a=fx_snap(p,u);
        bool fault=false;
        __try {member_fx(kind,p,opp,u,shared);}
        __except(EXCEPTION_EXECUTE_HANDLER) {fault=true;}
        fx_log(s,i,kind,false,fault,shared,p,opp,a,fx_snap(p,u));
        ++done;
    }
    return done;
}
// 378890 (state 0x15): the members' effects, then 37AA90 with the final pair; a second pass on the same block is left out.
// duel_manual_apply: army 1 applied | 2 second pass left out | fault << 4 | members' fault << 5 | members applied << 8 | loser injury
// rolls let through << 12 | left out << 16, value ids final row 0 << 16 | row 1, hex W/L row 0 | row 1 << 4 | fate << 8.
static bool apply(Obj,int *block) {
    if(!mod_features.duel_relay || !final_ready || bailed || !scene || block!=reinterpret_cast<int *>(scene+0x60))return false;
    if(applied) {applied_twice=true;log_event("duel_manual_apply",2,0,0);return true;}
    applied=true;
    fx_rolls=fx_skips=0;fx_thread=GetCurrentThreadId();fx_on=true;
    unsigned members=0;
    bool team_fault=false,fault=false;
    __try {members=team_effects(block);}
    __except(EXCEPTION_EXECUTE_HANDLER) {team_fault=true;}
    Obj fu[2]={};FxSnap fb[2]={};
    for(unsigned r=0;r<2;++r)if(fin_person[r]){fu[r]=unit_of(fin_person[r]);fb[r]=fx_snap(fin_person[r],fu[r]);}
    __try {fn<void(__fastcall *)(Obj,int *)>(dm::kApply)(reinterpret_cast<Obj>(fin_arg),block);}
    __except(EXCEPTION_EXECUTE_HANDLER) {fault=true;}
    fx_on=false;
    const int w=block[4];
    for(unsigned r=0;r<2;++r)
        if(fin_person[r])
            fx_log(side_of_row[r],static_cast<unsigned>(duel.side[side_of_row[r]].active),w==2?2u:static_cast<int>(r)==w?0u:1u,true,fault,false,
                   fin_person[r],fin_person[r^1u],fb[r],fx_snap(fin_person[r],fu[r]));
    log_event("duel_manual_apply",1|(fault?0x10u:0u)|(team_fault?0x20u:0u)|((members&0xF)<<8)|((fx_rolls&0xF)<<12)|((fx_skips&0xF)<<16),
              (dm::id_of(fin_person[0])<<16)|dm::id_of(fin_person[1]),
              (static_cast<unsigned>(block[4])&0xF)|((static_cast<unsigned>(block[0x1A])&0xF)<<4)|((static_cast<unsigned>(block[3])&0xFF)<<8));
    return true;
}
// The 378890 records (37BA10) of a settled relay scene name the final pair (an original duelist maps to the final fighter of its row);
// after a left-out second pass they are dropped. duel_manual_record: army kind | dropped << 4, value ids a << 16 | b, hex fate.
static bool record(Obj a,Obj b,unsigned kind,int fate) {
    if(!mod_features.duel_relay || !final_ready || bailed)return false;
    Obj x=a,y=b;
    for(unsigned r=0;r<2;++r){if(a==arg_person[r] && fin_person[r])x=fin_person[r];if(b==arg_person[r] && fin_person[r])y=fin_person[r];}
    ++records;
    log_event("duel_manual_record",(kind&0xF)|(applied_twice?0x10u:0u),(dm::id_of(x)<<16)|dm::id_of(y),static_cast<unsigned>(fate));
    if(!applied_twice)fn<void(__fastcall *)(Obj,Obj,unsigned,int)>(dm::kRecord)(x,y,kind,fate);
    return true;
}
// duel_multi riders: an extra of a relay scene stays under the ground until the engine brought it in (before the engine starts:
// every pick of the scene).
static bool rider_hidden(Obj person) {
    if(!mod_features.duel_relay || !scene || !person)return false;
    if(!engine_on) {
        for(unsigned r=0;r<2;++r)for(unsigned s=0;s<2;++s)if(dm::picked[r][s]==person)return true;
        return false;
    }
    for(unsigned s=0;s<2;++s)for(unsigned i=1;i<kMembers;++i)if(member[s][i]==person)return !shown[s][i];
    return false;
}
// duel_multi waiting bars of a relay scene (local295, user 「장비,유비,관우 순으로 일기토 참여함 그런데 왜 UI가 2명까지밖에 안보이지?」): per
// side every waiting member (came in, not seated, HP left) as the stage shows it, most HP in row 0, the next in row 1; nobody = that
// cell hidden (809110 still needs a person in it).
static bool bar_want(Obj who[2][2],int hp[2][2],bool show[2][2]) {
    if(!mod_features.duel_relay || !scene || bailed || dm::scene_manager!=scene)return false;
    for(unsigned s=0;s<2;++s) {
        int top[2]={-1,-1};
        if(engine_on)
            for(int i=0;i<kMembers;++i) {
                if(i==seated[s] || !member[s][i] || !shown[s][i] || disp_hp[s][i]<=0)continue;
                if(top[0]<0 || disp_hp[s][i]>disp_hp[s][top[0]]){top[1]=top[0];top[0]=i;}
                else if(top[1]<0 || disp_hp[s][i]>disp_hp[s][top[1]])top[1]=i;
            }
        for(unsigned r=0;r<2;++r) {
            if(top[r]>=0) {who[r][s]=member[s][top[r]];hp[r][s]=disp_hp[s][top[r]];show[r][s]=true;}
            else {who[r][s]=pointer(seat(scene,s),0xB0);hp[r][s]=0;show[r][s]=false;}
        }
    }
    return true;
}
static bool hold_dir(Obj m,float f[2]) {
    if(!mod_features.duel_relay || !holding || !scene || m!=scene)return false;
    dir_of(ph,f[0],f[1]);return true;
}
static unsigned snap_serial() {return snaps;}
static bool holding_now(Obj m) {return mod_features.duel_relay && holding && scene && m==scene;}
static void scene_close(Obj) {   // the day's close (44AEAF, before the original 37D040)
    if(!mod_features.duel_relay)return;
    __try {release_window(2);number_forget();scene=nullptr;on=false;holding=flow_on=false;}
    __except(EXCEPTION_EXECUTE_HANDLER) {owner_released();}
}
}
