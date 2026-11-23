// Shared duel-scene parts of the relay duel (duel_manual, [extensions] duel_relay; local298-308 [diagnostics] duel_manual_test): extra mounted officers in the
// original duel scene, their waiting bars, the round camera pull-back and the effect / record / damage / 일격 hooks its settlement
// uses. The multi-duel test feature ([extensions] duel_multi, background duel and segment chain) was removed on user 2026-11-22
// 「다중일기토는 지워 그건 최종적으로 없앨기능이야」; with the relay switch off this module leaves every duel original.
// Diagnostics (bar probes, rectangles, the camera shot survey) stay in diagnostic builds (#ifndef NO_RUNTIME_LOG).
// Original (offline decompile, build 1a4a447d):
//   duel model pool: model manager DAT_1421462A8, 1C6A40 builds its duel pool with CALL 1C46E0(pool, 2, allocator) at 1C6B0F
//     (count x 0x1A0 B; teardown 1C8980 loops over the stored capacity +0x38, so a larger count is released correctly).
//   duel manager DAT_142786780: side slots +0x120 / +0x2E0 (0x1C0 B each), +0x588 scene live, +0x3C player side (-1 = no scene),
//     +0x20..+0x5F the 376020 argument (+0 / +8 the two duelists, +0x1C player side index), +0x60 the result block (0xC0 B, ints:
//     [0] outcome for the player, [2] 일격, [3] fate; per arg row (stride 0x16 from [4]): +0 win/loss/draw (0/1/2), +2 start HP,
//     +3 final HP, +4..+9 HP before round 1 and after rounds 1-5, +0xA.. damage, +0x10.. round codes), +0x4A0 state, +0x4A8 round.
//     Slot init 3762D0(slot, hp, side, arg row, person, result row) stores +0xB0 person, +0xA8 id, +0x48..+0x9F a copy of the row,
//     +0xA4 the arg row (375BC0 reads it), then 376450 takes a model (1C30D0) into slot +0x1B0 (model +0x40 = the slot). Per frame
//     37D600 runs the state machine and, per slot, 37D810(slot+0x128) and 37D930(slot). 37D040 releases each slot.
// local274-292 history (chain, background duel, segment playback, its effects and 1/5 damage cut): docs/MULTI_DUEL.md (retired; last commit in docs/HISTORY.md).
#include "game_addresses.h"
#include "duel_manual_script.h"   // local313 join_accepts (pure)
// [extensions] duel_relay (docs/MANUAL_DUEL.md section 10; local298-308 [diagnostics] duel_manual_test): the San11-style relay duel lives in native_duel_manual.inl (included
// right after this file, no-ops in release builds); these are its dispatch points in the hooks below.
namespace duel_manual {
static void frame(Obj manager);          // after the scene tick, before this module's own frame (decides the scene first)
static bool scene_on(Obj manager);       // a relay scene: the only scene this module serves (extras within 2 hexes)
static void entry(Obj manager);          // right after 376020: its block and both +0xF1
static bool round_end(Obj manager);      // true = replaced the original 37D1C0 call
static bool force_damage(int *block);    // settlement: the adjusted 3747A0 call's damage columns
static bool force_onehit();              // settlement: no 일격 roll in that call
static void note_effects(int *block);    // 37AA90 calls on the scene block
static bool apply(Obj persons,int *block);               // true = 37AA90 handled (the final pair of a relay scene)
static bool record(Obj a,Obj b,unsigned kind,int fate);  // true = 37BA10 handled (renamed to the final pair, or dropped)
static bool injury_skip(Obj person);     // local308: true = leave out 37AD40's loser injury (37B016) in the relay's result calls
static bool rider_hidden(Obj person);    // an extra of a relay scene the engine has not brought in yet
static bool bar_want(Obj who[2][2],int hp[2][2],bool show[2][2]); // true = a relay scene's waiting bars ([row][side], row 0 the most HP)
static void owner_released();            // bar_release took every pane of the owner (the relay window too)
static void scene_close(Obj manager);    // day close
static bool hold_dir(Obj manager,float f[2]);   // local295: in a hold, the pair's riding direction (the extras' forward)
static unsigned snap_serial();           // local295: changes when the hold turns the pair back at once (the extras jump with it)
static bool holding_now(Obj manager);    // local295: a hold of this scene is on (no zoom pull: the hold keeps its own close shot)
static bool look_at(const float eye[3],const float target[3],float q[4]);   // local298: camera rotation eye -> target once the relay
                                                                           // matched the rotation convention on the original camera
static bool aim_after(Obj stage,Obj cam,float eye[4],float rot0[4]);      // local299: after an aimed apply; false = it read back
                                                                           // wrong (the rotation rot0 is back, no aim for the run)
}
namespace duel_multi {
static const unsigned kPoolCall=GA(GA_1C6B0F),kTickCall=GA(GA_44DF40),kCloseCall=GA(GA_44AEAF),kEntryCall=GA(GA_2CE966);
static const unsigned kPoolInit=GA(GA_1C46E0),kTick=GA(GA_37D600),kClose=GA(GA_37D040),kEntry=GA(GA_376020);
static const unsigned kSlotInit=GA(GA_3762D0),kAction=GA(GA_37C0C0),kTiming=GA(GA_37D810),kModelSet=GA(GA_1C9B70),kRelease=GA(GA_1C52C0);
static const unsigned kSlotSize=0x1C0,kFirstSlot=0x120,kModel=0x1B0,kNoHex=48400;
static const unsigned kRoundEnd=GA(GA_37D1C0),kSetState=GA(GA_374E00);
static const unsigned kRoundCallB=GA(GA_37788D),kRoundCallE=GA(GA_377DC9),kRoundCallD=GA(GA_3785EC);   // in states 0xB / 0xE / 0xD
static const unsigned kFaceSet=GA(GA_9312F0),kTextSet=GA(GA_9332B0),kName=GA(GA_230950),kStat=GA(GA_231770),kNumber=GA(GA_30B9D0),
                      kNode=GA(GA_92F390);
// local283
static const unsigned kCalc=GA(GA_3747A0),kApply=GA(GA_37AA90),kRecord=GA(GA_37BA10),kPage=GA(GA_827790),kGaugeFill=GA(GA_8269A0),
                      kApp=GA(GA_14DAE0);
static const unsigned kApplyCall=GA(GA_3788CF),kRecordCallA=GA(GA_37892B),kRecordCallB=GA(GA_378948),kGaugeHpCall=GA(GA_82AF0F);
static const unsigned kDamage=GA(GA_37BC70),kDamageCall=GA(GA_374A22);   // local284: 3747A0's damage columns (its only call)
static const unsigned kOneHit=GA(GA_376CC0),kOneHitCall=GA(GA_374901);   // local285: 3747A0's 일격 roll (its only call)
static const unsigned kInjury=GA(GA_27F250),kInjuryCall=GA(GA_37B016);    // local308: 37AD40's loser injury (HP < 141A4C3DC, kind 3, r9 0)
static const char kOwner[]="duel_multi";
// local284 (user 2026-11-20 「그냥 지금 두번째 적무장 있는곳에 관우 그대로 두면 카메라 시점만 바뀌니까 괜찮을거 같은데..」): rider spots are fixed
// to the duel, not to the camera; the axis u (side 0 -> side 1) and the normal p are locked per scene. A camera cut never moves a rider.
// local285 (user 2026-11-20 「플레이어 반대편 무장에 세워놔야 카메라 구도가 우리가 보기 편해. 그리고 메인 일기토에 비해서 조금 뒤쪽으로」): both riders
// of a rank took own duelist + e*900 + q*350 (e = toward the enemy end). That spot stood behind the intro camera and in line with the
// duelists in the side view of the charge.
// local286 (user 2026-11-20 「1사진첫 등장에 보였는데 갑자기 안보이네? 이건 바꾸지 말지」, 「여기까지도 전에 방식( 주 일기토 무장보다 뒤에서 더 늦게
// 부딪치기) 방식이 좋았어. 직전엔 겹쳐보였고」, 「조금더 앞으로 땡겨줘 적무장에 살짝 좌후방이었으면해」): two phases.
//   intro and charge (until the first round state 0xB / 0xD): local284's lane again. p = the axis normal turned away from the first
//     duel camera; first extra = own duelist + p*600 (behind the duelists in the side view of the charge; the easing makes the pair
//     meet a little later), second extra = p*1100 - (own forward)*400.
//   rounds: d = the player's duelist -> the enemy duelist as the original seats them for the rounds (side by side; +0x3C = the
//     player's arg row, its slot found through +0xA4, none = side 1 is the enemy), taken in every frame of the round states 0xB /
//     0xD (the riders snap at the first one, later changes are eased). First extra = own duelist + d*350 + left(d)*500, second extra = d*700 + left(d)*1000, left(d) = (d.z, -d.x) =
//     screen left when looking along d (checked against the local285 cam samples). Seen from behind the player's duelist the pair
//     stands a little left of and behind the enemy duelist. The round camera orbits, so other cuts see it elsewhere.
//   The incoming officer = own duelist + p*280 (the far side of its seat, as in local284 and local285).
// local287 (user 2026-11-20 「이 창 부딪치는 화면에서 뒤쪽 일기토 무장들 각자 진영에서 더 조금 뒤로」, 「조금도 적무장의 진행방향 왼쪽으로 가야돼
// 지금은 너무 뒤에있는 느낌이야 적무장을 기준으로 보면 남서쪽 시계로 약 8시쯤」): in the charge (states 6 / 7) both riders keep kChargeBack more
// toward their own end. In the rounds both duelists ride side by side (the local286 samples: +z at about 20 per frame, the player
// 280 on the enemy's right) and the eased riders trailed them by about 114, so the pair stood at 6-7 o'clock of the enemy. Now f =
// the enemy duelist's travel (its smoothed steps in the round states; before it moves, right of d), the side = left of f (the
// enemy's right when the player rides on its left), and each rider = own duelist + side*700 - f*300 (rank 1 1300 / 600): the
// pair's middle stands at about 8 o'clock of the enemy, 640 away. In the rounds a rider moves along with its duelist and only the
// spot change is eased.
static const float kLane0=600.0f,kLane1=1100.0f,kBack1=400.0f,kChargeBack=400.0f;
// local298 (user 2026-11-22 「아군 무장 기준으로 5시방향(두번쨰) 그리고 1시방향(세번쨰). 그리고 적군무장 기준으로 7시방향(두번쨰) 11시방향(세번쨰)
// 이렇게 일렬로 맞춰서 달렸으면해 삼11처럼」): in the rounds (and the hold) each rider sits on its own duelist's outside (lat: away from the
// other duelist, so ours at 5 / 1 o'clock and the enemy's at 7 / 11 when the player rides on the right; mirrored with the sides),
// 12 o'clock = the travel: the first extra kSide0 out and kRear0 behind, the second kSide1 out and kRear1 behind (negative = ahead),
// about 400 from the duelist at 30 degrees off the travel.
static const float kSide0=200.0f,kRear0=350.0f,kSide1=200.0f,kRear1=-350.0f;
static const float kEaseRate=0.15f,kEaseMax=80.0f,kCutEye=1500.0f,kCutMain=500.0f,kStage=20000.0f;
// Relay scene (duel_manual, user 2026-11-22 「합류하기전엔 체력바나 같이 달리기는 없어야하고」): an extra not brought in yet stands kHideFar to
// the side and kHideDepth under its spot; coming in it starts kEnterBack behind the spot (the easing brings it alongside).
static const float kHideFar=30000.0f,kHideDepth=5000.0f,kEnterBack=2500.0f;
// local284 (user 2026-11-20 「어려우면 일단 체력바 아래에 둬봐」): the waiting officers' bar sits right under the original one (y 200).
// local285 (user 2026-11-20 「ui가 너무 크다. 이렇게 조금씩 잘라서 메인 무장들 밑에 크기를 줄여서」): two copies of the original gauge drawn at
// 60 % (Layout ui_pane_scale, toward the pane's top-left corner): the left copy keeps only the left half (right group node 32
// hidden) at x 0, the right copy only the right half (left group node 6 hidden) at x 1920 * (1 - 0.6) = 768, so each half sits
// under the duelist the original gauge shows on that side. A copy whose scale is refused (old Layout, no scene root yet) stays at
// original size at x 0 (the local284 look) and is retried every frame.
// local286 (user 2026-11-20 「이거 제대로 짤라서 사용해야지 오버랩되고 크기변화도 안되잖아」): local285's scale reached the pane root only and the
// group nodes 6 / 32 do not hold the faces and frames. Now one frame after the build each copy is cut by the controls' screen
// rectangles (bar_cut_half), then scaled the original way (Layout ui_pane_scale, local286); the right copy moves to x 768 only once
// Layout reports the scale applied to the children (2). Not applied within kBarPending frames = both copies back to 1.0 at x 0.
// local288 (user 2026-11-20 「아직도 UI가 이상하게 오버랩되고 초상화는 왜 관우만 보이고 만총은 안보이고 크기는 저러나」): the faces, the 닫기
// button and the emblem are controls with their own scene root, placed by integer offsets, so the pane scale left them full size
// (만총's face at 768 + its unscaled offset, off screen) and the cut had no rectangle for the 46 ids without a control. Layout now
// scales the controls too (ui_pane_scale: root, size and offset * 0.6) and reads node ids from the table's node array; the cut
// classifies against the gauge's middle x 960 (pane at x 0 while cutting) and keeps only the kept side's face of all controls.
// local295 (user 2026-11-22 「장비,유비,관우 순으로 일기토 참여함 그런데 왜 UI가 2명까지밖에 안보이지?」): up to two waiting officers per side, in
// two rows of copies (copy k = row * 2 + half). Row 1 sits kBarRowGap under row 0: the tallest controls (the faces, 265 high from the
// pane's top in the local294 rectangles) take 159 at 60 %. local298 (user 「계단식」, rehearsal mock): rows 105 apart and each later row
// kBarStepX further out (the left copy to the left, the right copy to the right), so the faces step outward instead of stacking.
static const float kBarScale=0.6f;
static const int kBarY=200,kBarRightX=768,kBarNodes=128,kBarMid=960,kBarTol=8,kBarRows=2,kBarCopies=2*kBarRows,kBarRowGap=105,kBarStepX=60;
static const unsigned kBarPending=240;
static int bar_row_y(unsigned k) {return kBarY+static_cast<int>(k>>1)*kBarRowGap;}
static int bar_row_x(unsigned k) {   // after the scale (the cut runs at x 0)
    const int step=static_cast<int>(k>>1)*kBarStepX;
    return (k&1u)?kBarRightX+step:-step;
}

alignas(16) static unsigned char shadow[0x600];   // manager-shaped: the first extras (side 0 slot at +0x120, side 1 at +0x2E0)
alignas(16) static unsigned char reserve[0x600];  // the second extras, same shape
alignas(16) static unsigned char cells[2][0x58];  // read-only result rows for 3762D0 (shape of 37CBD0's defaults)
static Obj picked[2][2];        // [rank: 0 first extra (shadow), 1 second extra (reserve)][side]
static int last_action[2][2];
static unsigned entry_ids;      // the two duelists 376020 was entered with (a << 16 | b)
static Obj model_owner,scene_manager;
static bool alive,tried,broken,round_logged;
static unsigned pool_calls,step,scene_serial,seen_serial,wait_frames;
static unsigned watch_frames,state_logs,last_state,round_logs,cam_frames,cam_samples,cut_logs,shot_logs,last_shot,shot_frames;
static bool watching;
static wchar_t hud_text[2][2][64];          // name / 무력 text handed to 9332B0 per side

// ---- scene members and hooks ----
static Obj member[2][3];                // [side][0 duelist, 1 first extra, 2 second extra]
static unsigned row_of[2];              // arg row of side 0 / 1 (slot +0xA4)
static bool hooks_all;                  // round, effect and record hooks all published (the relay settlement needs them)
static bool damage_hooked,onehit_hooked,injury_hooked;
static unsigned damage_logs,onehit_logs;
// local292: the extras' delayed actions (a short queue per rider of due frame and action, see riders_step)
struct ActQ {unsigned due[8];int act[8];unsigned head,count;int last_src;unsigned last_from;};
static ActQ act_q[2][2];               // [rank][side]
static unsigned motion_frames,motion_pushes,motion_applies,motion_full,swap_rounds,swap_logs,swap_round,swap_diff;
static const unsigned kActDelay[2]={18,10};   // frames late in the rounds: the back pair about 0.3 s, the second extras a little less
// ---- riders ----
struct Rider {float x,z;bool ready;};
static Rider riders[2][2];               // [rank][side]
static bool fallen[2][2],eye_known,main_known,axis_known,lane_locked,round_locked,travel_known,travel_turned;
static bool rider_away[2][2];             // relay scene: kept off stage last frame (duel_manual::rider_hidden)
static float axis_u[2]={1.0f,0.0f},axis_p[2]={0.0f,1.0f},round_d[2]={1.0f,0.0f},last_eye[3],last_main[2][2];
static float travel_acc[2],travel_f[2]={0.0f,1.0f};   // local287: the enemy duelist's travel in the rounds (smoothed steps)
static unsigned enemy_side,lane_logs,snap_seen;
static float form_pt[2][2][3];   // local295: each rider's spot this frame (x, y, z; before the hide), for the round camera pull-back
static bool form_ok[2][2];
// ---- bottom bar ----
alignas(16) static unsigned char bar_cell[kBarRows][2][kSlotSize];   // [row][side] slot-shaped: +0xB0 person, +0x3C HP, +0x40 max
static Obj bar[kBarCopies],bar_page,bar_source,bar_person[kBarRows][2];   // local285: copy k&1 = 0 left half, 1 right half (row k>>1)
static int bar_hp[kBarRows][2];
static bool bar_shown[kBarRows][2];
static bool bar_registered,bar_dirty,bar_failed,bar_flip,gauge_logged,bar_scaled[kBarCopies],scale_off,bar_cut[kBarCopies],rects_logged[2];
static bool bar_face_off[kBarCopies];           // local288: Layout reported a face that did not take the scale (8): faces hidden
static unsigned bar_builds,bar_hp_logs,bar_frames,bar_writes[kBarCopies],bar_pending[kBarCopies];
static unsigned char bar_hide[kBarCopies][kBarNodes];   // local286: ids hidden in each copy (re-applied after a refresh)
static unsigned char bar_face[kBarCopies][kBarNodes];   // local288: face controls kept in each copy (hidden if the portrait does not scale)
static bool bar_any() {for(Obj b:bar)if(b)return true;return false;}
static unsigned bar_mask() {unsigned v=0;for(unsigned k=0;k<kBarCopies;++k)if(bar[k])v|=1u<<k;return v;}
static void (__fastcall *next_gauge_hp)(Obj);   // Layout manager: next of the 82AF0F CALL (original 8269A0 unless another owner sits in between)
// local308 helper chips (duel_digits mock C; user 「보조무장 C안으로 하고 메인일기토 무장의 필살기 게이지를 가리지 않도록 조심」, 「왜 군사보고로해?
// 지금 초상화만 줄여서 만들면 안되나?」): in a relay scene with the overlay working, each copy keeps only its half's round portrait (the
// original draws it), scaled to duel_digits::kChipScale and placed so the portrait's middle sits on its chip place (one row under the
// 투지 bars, row 0 next to the main portrait); the overlay draws the HP ring and 「name 무력」. The copy's groups, the centre band and
// both faces are hidden at the build (no full-size frame), the kept face comes back once the scale reached the children. A refused
// scale gives the chips up for the scene (chip_fail): the copies are built again the local298 way.
static bool chip_mode,chip_fail,chips_logged[kBarCopies];
static float bar_face_at[kBarCopies][2];   // the kept face control's top-left in the copy as cut (x 0, its build y), -1 none
static int bar_chip_x[kBarCopies];         // the copy's x after the scale
static bool chips_wanted() {return !chip_fail && !duel_digits::broken();}
static int chip_pane_y() {return static_cast<int>(std::floor(duel_digits::kChipCY-(1.0f+duel_digits::kFaceCY)*duel_digits::kChipScale+0.5f));}
static float face_x(unsigned k) {return bar_face_at[k][0]>=0.0f?bar_face_at[k][0]:(k&1u)?1516.0f:150.0f;}   // local307 rectangles as the fallback
static float face_y(unsigned k) {return bar_face_at[k][1]>=0.0f?bar_face_at[k][1]-static_cast<float>(chip_pane_y()):1.0f;}
static int chip_x(unsigned k) {return static_cast<int>(std::floor(duel_digits::kChipX[k&1u][k>>1]-(face_x(k)+duel_digits::kFaceCX)*duel_digits::kChipScale+0.5f));}
static void chips_clear() {duel_digits::ChipSet cs;memset(&cs,0,sizeof cs);duel_digits::publish_chips(cs);}
static float chip_scale() {return duel_digits::kChipScale;}

static Obj slot_of(Obj base,unsigned side) {return base+kFirstSlot+side*kSlotSize;}
static Obj bank(unsigned rank) {return rank?reserve:shadow;}
// local298: the ground height under x, z: BAAF50(terrain, x, z) (result in xmm0, 0 without terrain), the call the round state 377350
// makes for the stage centre +0x5D0 with the manager's terrain *(+0x5A8). ok = false for an unresolved id, no terrain or a fault.
static const unsigned kGround=GA(GA_BAAF50);
static float ground_at(Obj manager,float x,float z,bool &ok) {
    ok=false;
    Obj terrain=kGround && manager?pointer(manager,0x5A8):nullptr;
    if(!terrain)return 0.0f;
    float g=0.0f;
    __try {g=fn<float(__fastcall *)(Obj,float,float)>(kGround)(terrain,x,z);}
    __except(EXCEPTION_EXECUTE_HANDLER) {return 0.0f;}
    ok=g==g && g>-100000.0f && g<100000.0f;
    return ok?g:0.0f;
}
static Obj models() {return pointer(image,GA(GA_21462A8));}
static unsigned id_of(Obj person) {return person?word(person,0x10):0u;}
static unsigned as_int(float f) {return static_cast<unsigned>(static_cast<int>(f));}
static float &flt(Obj p,unsigned off) {return *reinterpret_cast<float *>(p+off);}
static void put32(Obj p,unsigned off,unsigned v) {*reinterpret_cast<unsigned *>(p+off)=v;}
static void putptr(Obj p,unsigned off,Obj v) {*reinterpret_cast<Obj *>(p+off)=v;}
static bool call_to(unsigned site,unsigned target) {
    return site && target && image[site]==0xE8 && site+5+*reinterpret_cast<int *>(image+site+1)==target;
}
static unsigned hex_distance(unsigned a,unsigned b) {   // same formula as ccz_bands / combat_v2 hex.h (220-wide offset grid)
    if(a>=kNoHex || b>=kNoHex)return ~0u;
    const int ax=static_cast<int>(a%220),bx=static_cast<int>(b%220);
    const int ay=static_cast<int>(a/220)-(ax+(ax&1))/2,by=static_cast<int>(b/220)-(bx+(bx&1))/2;
    auto ab=[](int v){return v<0?-v:v;};
    return static_cast<unsigned>((ab(ax-bx)+ab(ay-by)+ab(ax-bx+ay-by))/2);
}
static unsigned force_of(Obj unit) {return fn<unsigned char(__fastcall *)(Obj)>(GA(GA_22E690))(unit);}
static unsigned power_of(Obj person) {return static_cast<unsigned>(fn<int(__fastcall *)(Obj,int,int)>(kStat)(person,1,0xFFF));}   // 무력 as 3747A0 reads it
// local283 (Fable design): an extra must be able to fight: duel stamina +0xF1 above 0 and +0x161 below 2.
static bool fit(Obj p) {return p[0xF1]!=0 && static_cast<unsigned char>(p[0x161])<2;}
// 2026-11-22 user 「의용병무리는 일기토에서 아예빼버려 걸지도 걸리지도 않게」: a 의용병 무리 army is led by a generic commander 4033..4100
// (ccz_bands.inl kFirstGeneric / kSlots) and answers its patron force through 22E690, so it would pass the same-force test. Without the
// bands those commanders lead bandits and tribes (force 46..51), never the same force as a duelist (2CCC60 takes forces 1..45 only).
static bool band_army(Obj unit) {const unsigned c=word(unit,0x12);return c>=4033 && c<4033+68;}
// Commanders of up to two same-force field units within `radius` hexes of the duelist's unit (1 for the multi duel, 2 for a relay
// scene, user 2026-11-22 「일기토합류는 2헥스로 하자」), strongest 무력 first. taken = the other side's picks; out[] nullptr = none. why: 1 no
// unit, 2 no neighbour, 3 neighbours but nobody fit (create: 4 asked, all refused, local313).
static void neighbours(Obj rep,Obj a,Obj b,const Obj taken[2],Obj out[2],unsigned &why,unsigned radius=1) {
    out[0]=out[1]=nullptr;
    Obj unit=fn<Obj(__fastcall *)(Obj,int)>(GA(GA_22BAB0))(rep,1);
    if(!valid(unit)) {why=1;return;}
    const unsigned tile=word(unit,0x2A),force=force_of(unit);
    unsigned best[2]={0,0},seen=0;
    for(unsigned i=0;i<=500;++i) {
        Obj u=table(0x7E838,i,500);
        if(!u || u==unit || !valid(u) || u[0x10]!=1 || !word(u,0x16))continue;
        const unsigned dist=hex_distance(word(u,0x2A),tile);
        if(dist<1 || dist>radius || band_army(u) || force_of(u)!=force)continue;
        ++seen;
        Obj p=table(0x158,word(u,0x12),6000);
        if(!valid(p) || p==a || p==b || p==taken[0] || p==taken[1] || p==out[0] || p==out[1] || !fit(p))continue;
        const unsigned power=power_of(p)+1;   // +1: 0 means empty
        if(power>best[0]){out[1]=out[0];best[1]=best[0];out[0]=p;best[0]=power;}
        else if(power>best[1]){out[1]=p;best[1]=power;}
    }
    why=out[0]?0:(seen?3:2);
}
// local276 diagnostics after a skip 5. Header: army side|0x100 (no unit; value person id, hex person +0xE4 location) or
// side|0x200 (value tile | force << 16, hex troops | commander << 16). Then up to 12 units within 2 hexes: army side | distance << 4
// | kind +0x10 << 8 | same force << 16, value unit index << 16 | commander +0x12, hex troops +0x16 | tile +0x2A << 16.
static void dump_near(unsigned side,Obj rep) {
    Obj unit=fn<Obj(__fastcall *)(Obj,int)>(GA(GA_22BAB0))(rep,1);
    if(!valid(unit)) {log_event("duel_multi_near",side|0x100,id_of(rep),word(rep,0xE4));return;}
    const unsigned tile=word(unit,0x2A),force=force_of(unit);
    log_event("duel_multi_near",side|0x200,tile|(force<<16),word(unit,0x16)|(word(unit,0x12)<<16));
    for(unsigned i=1,n=0;i<=500 && n<12;++i) {
        Obj u=table(0x7E838,i,500);
        if(!u || u==unit || !valid(u))continue;
        const unsigned d=hex_distance(word(u,0x2A),tile);
        if(d>2)continue;
        ++n;
        log_event("duel_multi_near",side|(d<<4)|(static_cast<unsigned>(u[0x10])<<8)|(force_of(u)==force?1u<<16:0u),
                  (i<<16)|word(u,0x12),word(u,0x16)|(word(u,0x2A)<<16));
    }
}
// Camera (ccz_sprites read_camera): *(27D4258)+0x78 -> +0x20, vtable 16B62E0, +0x1340 80 floats: 0-2 eye, 4-6 target, 60-75 ViewProj
// row vectors. Returns 0 usable, 1 no camera, 2 other vtable (vt = its RVA), 3 torn copy, 4 projection test, 5 exception.
static unsigned read_cam_raw(float c[80],unsigned &vt) {
    if(!GA(GA_27D4258) || !GA(GA_16B62E0))return 1;
    Obj root=pointer(image,GA(GA_27D4258)),p=root?pointer(root,0x78):nullptr,cam=p?pointer(p,0x20):nullptr;
    if(!cam)return 1;
    vt=static_cast<unsigned>(pointer(cam,0)-image);
    if(vt!=GA(GA_16B62E0))return 2;
    float again[80];
    memcpy(c,cam+0x1340,sizeof again);memcpy(again,cam+0x1340,sizeof again);
    if(memcmp(c,again,sizeof again))return 3;
    if(!(c[44]>0 && c[49]>0 && std::fabs(std::fabs(c[55])-1)<1e-3f && c[59]==0))return 4;
    return 0;
}
static unsigned read_cam(float c[80],unsigned &vt) {
    vt=0;
    __try {return read_cam_raw(c,vt);}
    __except(EXCEPTION_EXECUTE_HANDLER) {return 5;}
}
struct Ndc {float x,y,w;};
static Ndc project(const float *c,float x,float y,float z) {   // row vector * ViewProj (same as ccz_sprites clip)
    const float *m=c+60;
    const float cx=x*m[0]+y*m[4]+z*m[8]+m[12],cy=x*m[1]+y*m[5]+z*m[9]+m[13],cw=x*m[3]+y*m[7]+z*m[11]+m[15];
    Ndc n{0,0,cw};
    if(cw>1e-3f){n.x=cx/cw;n.y=cy/cw;}
    return n;
}
static Ndc project_slot(const float *c,Obj slot) {return project(c,flt(slot,0xC0),flt(slot,0xC4),flt(slot,0xC8));}
static unsigned enc(float v) {if(v<-99.0f)v=-99.0f;if(v>99.0f)v=99.0f;return static_cast<unsigned>(static_cast<int>(v*1000.0f)+100000);}
static bool near_stage(const float *c,Obj manager) {   // local280: the eye is within kStage of the duelists' middle
    Obj o0=slot_of(manager,0),o1=slot_of(manager,1);
    const float dx=c[0]-(flt(o0,0xC0)+flt(o1,0xC0))*0.5f,dz=c[2]-(flt(o0,0xC8)+flt(o1,0xC8))*0.5f;
    return dx*dx+dz*dz<kStage*kStage;
}
// duel_multi_cam: army tag | point << 8 | sample << 16 (tag 1 create, 2 first round, 3 every 30 duel-camera frames, up to 80 per
// scene). Points 0/1 duelist side 0/1, 2/3 first extra side 0/1, 10/11 second extra side 0/1: value ndc x * 1000 + 100000, hex ndc y
// * 1000 + 100000 (+1000000 behind the camera). 4: eye x / z, 5: target x / z, 6: eye y / target y. 7 (create): vtable RVA. 8: value
// state +0x4A0 | round << 8, hex slot action side 0 | side 1 << 16. A camera that fails the check logs point 15.
// local285 world positions (value x, hex z): 9 / 12 duelist side 0 / 1, 13 / 14 first extra side 0 / 1.
static void cam_probe(unsigned tag,Obj manager,unsigned sample) {
    float c[80];unsigned vt=0;
    const unsigned why=read_cam(c,vt),head=tag|(sample<<16);
    if(why) {log_event("duel_multi_cam",head|(15u<<8),why,vt);return;}
    static const unsigned points[6]={0,1,2,3,10,11};
    for(unsigned k=0;k<6;++k) {
        Obj base=k<2?manager:k<4?shadow:reserve,slot=slot_of(base,k&1);
        if(k>=2 && !pointer(slot,kModel))continue;
        const Ndc n=project_slot(c,slot);
        log_event("duel_multi_cam",head|(points[k]<<8),enc(n.x),enc(n.y)+(n.w>1e-3f?0u:1000000u));
    }
    log_event("duel_multi_cam",head|(4u<<8),as_int(c[0]),as_int(c[2]));
    log_event("duel_multi_cam",head|(5u<<8),as_int(c[4]),as_int(c[6]));
    log_event("duel_multi_cam",head|(6u<<8),as_int(c[1]),as_int(c[5]));
    for(unsigned s=0;s<2;++s) {
        Obj o=slot_of(manager,s),x=slot_of(shadow,s);
        log_event("duel_multi_cam",head|((s?12u:9u)<<8),as_int(flt(o,0xC0)),as_int(flt(o,0xC8)));
        if(pointer(x,kModel))log_event("duel_multi_cam",head|((13u+s)<<8),as_int(flt(x,0xC0)),as_int(flt(x,0xC8)));
    }
    if(tag==1)log_event("duel_multi_cam",head|(7u<<8),vt,0);
    log_event("duel_multi_cam",head|(8u<<8),(dword(manager,0x4A0)&0xFF)|((dword(manager,0x4A8)&0xFF)<<8),
              (dword(slot_of(manager,0),0x10)&0xFFFF)|(dword(slot_of(manager,1),0x10)<<16));
}
static unsigned model_state(Obj model) {   // a few model fields for the log (+0x50 loaded, +0x74 / +0x78 the 1C9EC0 / 1C8CA0 flags)
    if(!model)return 0xFFFFFFFFu;
    return (dword(model,0x50)&0xFF)|((dword(model,0x74)&0xFF)<<8)|((dword(model,0x78)&0xFF)<<16);
}
static void log_side(const char *name,Obj manager,unsigned side,unsigned rank=0) {   // army side | rank << 4 | original action << 8
    Obj mine=slot_of(bank(rank),side),orig=slot_of(manager,side);
    log_event(name,side|(rank<<4)|(dword(orig,0x10)<<8),(as_int(flt(mine,0xC0))&0xFFFF)<<16|(as_int(flt(mine,0xC8))&0xFFFF),
              (as_int(flt(orig,0xC0))&0xFFFF)<<16|(as_int(flt(orig,0xC8))&0xFFFF));
    log_event("duel_multi_model",side,model_state(pointer(mine,kModel)),model_state(pointer(orig,kModel)));
}

// ---- bottom bar (Layout ui_duel_gauge, owner row 82AF0F) ----
static Obj ui_context() {   // *(14DAE0()+0x210): what 827790 and the table teardown 933BA0 hand to vt+0x28
    Obj app=fn<Obj(__fastcall *)()>(kApp)();
    return app?pointer(app,0x210):nullptr;
}
// duel_multi_bar_release: army why (1 other officers waiting, 2 rebuild after a swap, 3 page or gauge gone, 4 nothing waiting, 5 the
// chips given up (local308), 9 broken, 0x10 | release_all reason), value panes released (-1 no scope), hex builds this scene.
static void bar_release(unsigned why) {
    if(!bar_any())return;
    chips_clear();
    layout_client::UiScope scope(kOwner);
    const int n=scope.ok?layout_client::ui_release(kOwner,ui_context()):-1;   // every copy (all panes of the owner)
    duel_manual::owner_released();   // a relay scene's command window and caption belong to the same owner
    log_event("duel_multi_bar_release",why,static_cast<unsigned>(n),bar_builds);
    for(Obj &b:bar)b=nullptr;
    bar_page=bar_source=nullptr;
    for(unsigned k=0;k<kBarCopies;++k){bar_scaled[k]=bar_cut[k]=bar_face_off[k]=false;bar_writes[k]=bar_pending[k]=0;}
    memset(bar_hide,0,sizeof bar_hide);memset(bar_face,0,sizeof bar_face);
}
static bool gauge_fill(Obj gauge) {   // 8269A0: the original gauge refresh, reads the cells through gauge +0x180 / +0x188
    __try {fn<void(__fastcall *)(Obj)>(kGaugeFill)(gauge);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
static void bar_want(Obj who[kBarRows][2],int hp[kBarRows][2],bool show[kBarRows][2]) {   // [row][side]
    if(duel_manual::bar_want(who,hp,show))return;   // a relay scene: its waiting members, two rows
    for(unsigned r=0;r<kBarRows;++r)for(unsigned s=0;s<2;++s){who[r][s]=nullptr;hp[r][s]=0;show[r][s]=false;}   // no relay: no bar
}
#ifndef NO_RUNTIME_LOG
// duel_multi_bar_scale (frames 1 and 30 after a build, per copy): army copy | frame << 4 | scaled << 12, value root local m[0] * 1000
// | 65535 << 16 (local287: the control table holds controls, not nodes, so local285/286 read a control there; the children's scale
// is checked by Layout, layout_ui_scale), hex 65535 | 65535 << 16. duel_multi_bar_root (same army): value / hex
// root translation m[12] / m[13] (877220: x - 960 and -(y - 540) plus the 92EDB0 alignment term; x 0 / y 200 = -960 / 340 when
// that term is 0, i.e. the pane shrinks toward its top-left corner).
static void bar_probe(unsigned k,unsigned frame) {
    Obj g=bar[k];
    unsigned root_s=65535u,node_s=65535u,where=~0u,tx=~0u,ty=~0u;   // not read
    __try {
        Obj root=pointer(g,0xA0),rm=root?pointer(root,0xE8):nullptr;
        if(rm) {root_s=as_int(flt(rm,0)*1000.0f)&0xFFFFu;tx=as_int(flt(rm,0x30));ty=as_int(flt(rm,0x34));}
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
    log_event("duel_multi_bar_scale",k|(frame<<4)|(bar_scaled[k]?1u<<12:0u),root_s|(node_s<<16),where);
    log_event("duel_multi_bar_root",k|(frame<<4),tx,ty);
}
#endif
static void bar_apply_hide(unsigned k) {
    for(int i=0;i<kBarNodes;++i)if(bar_hide[k][i])layout_client::ui_node_visible(bar[k],i,0);
}
// local286: copy k keeps the left (0) or right (1) half, cut one frame after the build while the copy is still at full size and at
// x 0 (Layout ui_node_rects). local288: node ids come from the table's node array, controls from their integers (local287 read only
// controls and had no rectangle for the 46 node ids). An id without children (a node leaf or a control) is left / right when its
// rectangle lies on that side of the gauge's middle x 960 (+-8; a zero-sized one by its x), else centre (the round counter, the
// emblem, a band across both halves); a node with children takes the sides of all its descendants. Where geometry puts a leaf on the
// opposite side of what local285 found (left group node 6 / right group node 32 subtrees, faces 1 left / 3 right), that side wins and
// is counted. A node leaf is hidden unless it is on the kept side, a parent only when no descendant is. Of the controls only the kept
// side's faces (table kind 0xB / 0xD) stay: the 닫기 button, the emblem and anything else is hidden in both copies. No rectangles (old
// Layout, refused) = only the group nodes hidden at the build (the local285 cut).
// duel_multi_bar_node (per copy cut): army copy | result << 4 (1 cut, 0xF no rectangles) | ids << 8, value hidden | kept << 8 |
// controls << 16 | ids without node or control << 24, hex geometry overruled by the group map | centre leaves << 8 | faces kept << 16
// | other controls hidden << 24. duel_multi_bar_rect (first cut of the process, each copy, diagnostic build): army id | sides << 8 (1
// left, 2 right, 4 centre) | source << 12 (0 node, 1 control, 2 neither) | hidden << 14 | group side << 16 | table kind (controls) <<
// 24, value x | y << 16, hex w | h << 16.
static void bar_cut_half(unsigned k) {
    bar_cut[k]=true;
    static int rect[kBarNodes*5];
    const int n=layout_client::ui_node_rects(bar[k],rect,kBarNodes);
    if(n<=0) {log_event("duel_multi_bar_node",k|(0xFu<<4),static_cast<unsigned>(n),0);return;}
    auto link=[&](int i){return rect[i*5+4];};
    auto kind_of=[&](int i){return link(i)<=-3?-3-link(i):-1;};   // table kind of a control, -1 = not a control
    unsigned char sides[kBarNodes]={},group[kBarNodes]={};
    bool parent[kBarNodes]={};
    for(int i=0;i<n;++i){const int p=link(i);if(p>=0 && p<n)parent[p]=true;}
    for(int i=0;i<n;++i) {   // local285's map: under group node 6 = left, 32 = right
        if(link(i)<-1)continue;
        for(int p=i,depth=0;p>=0 && p<n && depth<16;p=link(p),++depth)
            if(p==6 || p==32){group[i]=static_cast<unsigned char>(p==6?1:2);break;}
    }
    for(int i=0;i<n;++i) {
        const int kind=kind_of(i);
        if((kind==0xB || kind==0xD) && (i==1 || i==3))group[i]=static_cast<unsigned char>(i==1?1:2);
    }
    unsigned overruled=0,centre=0;
    for(int i=0;i<n;++i) {
        const int *r=rect+i*5;
        if(link(i)==-2 || (link(i)>=-1 && parent[i]))continue;
        unsigned char side=static_cast<unsigned char>(r[2]<=0?(r[0]<kBarMid?1:2):r[0]+r[2]<=kBarMid+kBarTol?1:r[0]>=kBarMid-kBarTol?2:4);
        if(group[i] && side!=4 && side!=group[i]){++overruled;side=group[i];}
        if(side==4)++centre;
        sides[i]|=side;
        if(link(i)>=-1)for(int p=link(i),depth=0;p>=0 && p<n && depth<16;p=link(p),++depth)sides[p]|=side;   // up the parents
    }
    const unsigned char keep=(k&1u)?2:1;
    unsigned hidden=0,kept=0,controls=0,missing=0,faces=0,others=0;
    for(int i=0;i<n;++i) {
        if(link(i)==-2){++missing;continue;}
        const int kind=kind_of(i);
        bool hide;
        if(kind>=0) {
            ++controls;
            const bool face=kind==0xB || kind==0xD;
            hide=!face || sides[i]!=keep;
            if(face && !hide) {
                bar_face[k][i]=1;++faces;
                if(chip_mode) {   // local308: hidden until the scale reached it; its place for the chip
                    hide=true;
                    if(bar_face_at[k][0]<0.0f && rect[i*5+2]>0){bar_face_at[k][0]=static_cast<float>(rect[i*5]);bar_face_at[k][1]=static_cast<float>(rect[i*5+1]);}
                }
            }
            if(!face)++others;
        }
        else if(chip_mode)hide=parent[i]?!(sides[i]&keep):true;   // local308: every leaf goes (name, 무력, HP), the portrait is a control
        else hide=parent[i]?!(sides[i]&keep):sides[i]!=keep;
        if(hide) {bar_hide[k][i]=1;++hidden;}
        else ++kept;
    }
    if(bar_face_off[k])for(int i=0;i<n;++i)if(bar_face[k][i])bar_hide[k][i]=1;
    bar_apply_hide(k);
    log_event("duel_multi_bar_node",k|(1u<<4)|(static_cast<unsigned>(n)<<8),hidden|(kept<<8)|(controls<<16)|(missing<<24),
              overruled|(centre<<8)|(faces<<16)|(others<<24));
#ifndef NO_RUNTIME_LOG
    if(!rects_logged[k&1u]) {   // one copy of each half (both rows cut the same way)
        rects_logged[k&1u]=true;
        for(int i=0;i<n;++i) {
            const int *r=rect+i*5;
            const int kind=kind_of(i);
            const unsigned source=link(i)==-2?2u:kind>=0?1u:0u;
            log_event("duel_multi_bar_rect",static_cast<unsigned>(i)|(static_cast<unsigned>(sides[i])<<8)|(source<<12)|
                      (bar_hide[k][i]?1u<<14:0u)|(static_cast<unsigned>(group[i])<<16)|((kind>=0?static_cast<unsigned>(kind)&0xFFu:0u)<<24),
                      (static_cast<unsigned>(r[0])&0xFFFFu)|(static_cast<unsigned>(r[1])<<16),
                      (static_cast<unsigned>(r[2])&0xFFFFu)|(static_cast<unsigned>(r[3])<<16));
        }
    }
#endif
}
// local288 face fallback: Layout ui_pane_scale result 8 = a face control's portrait node did not take the scale (it would stay full
// size): that copy's faces are hidden for the rest of the build, the scaled bars, names and 무력 stay. duel_multi_bar_face: army copy,
// value faces hidden, hex frame.
static void bar_hide_faces(unsigned k) {
    if(bar_face_off[k])return;
    bar_face_off[k]=true;
    unsigned n=0;
    for(int i=0;i<kBarNodes;++i)if(bar_face[k][i]){bar_hide[k][i]=1;++n;}
    bar_apply_hide(k);
    log_event("duel_multi_bar_face",k,n,bar_frames);
}
#ifndef NO_RUNTIME_LOG
// duel_multi_bar_len (diagnostic build, up to 32 per scene, local298 「교대」 feedback): army copy (4 = the main gauge) | why << 4 (1
// build, 2 HP change) | cell A +0x3C << 8 | cell B +0x3C << 16 (0xFF no cell), value gauge +0x1A8 | hex +0x1B0 (the drawn values
// 809110 stores and 8269A0 compares, raw bits). duel_multi_bar_len2: same army, value +0x1AC, hex +0x1B4.
static unsigned bar_len_logs;
static void bar_len_log(Obj g,unsigned k,unsigned why) {
    if(!g || bar_len_logs>=32)return;
    ++bar_len_logs;
    __try {
        Obj a=pointer(g,0x180),b=pointer(g,0x188);
        const unsigned head=k|(why<<4)|((a?dword(a,0x3C)&0xFFu:0xFFu)<<8)|((b?dword(b,0x3C)&0xFFu:0xFFu)<<16);
        log_event("duel_multi_bar_len",head,dword(g,0x1A8),dword(g,0x1B0));
        log_event("duel_multi_bar_len2",head,dword(g,0x1AC),dword(g,0x1B4));
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
}
#endif
// The cells take the wanted HP and every copy of a changed row is refreshed with the original 8269A0 (why 1 the build: the cells hold
// 100 until here; 2 a later change). 8269A0 always shows node 0x35 (a round digit): hidden again here, with the cut once it ran.
// duel_multi_bar_hp: army refresh ok | why << 8 | row << 16, value ids, hex HP side 0 | side 1 << 8.
static void bar_fill_real(const int hp[kBarRows][2],unsigned why) {
    static const int pips[8]={0x1B,0x1C,0x1D,0x1E,0x1F,0x35,0x36,0x37};
#ifndef NO_RUNTIME_LOG
    bool logged_main=false;
#endif
    for(unsigned row=0;row<kBarRows;++row) {
        bool changed=false;
        for(unsigned s=0;s<2;++s)
            if(hp[row][s]!=bar_hp[row][s]){bar_hp[row][s]=hp[row][s];put32(bar_cell[row][s],0x3C,static_cast<unsigned>(hp[row][s]));changed=true;}
        if(!changed)continue;
        bool ok=true;
        for(unsigned k=row*2;k<row*2+2;++k) {
            if(!bar[k])continue;
            if(!gauge_fill(bar[k]))ok=false;
            for(int n:pips)layout_client::ui_node_visible(bar[k],n,0);
            if(bar_cut[k] || chip_mode)bar_apply_hide(k);   // in case the refresh shows a control again (local308 chips: from the build)
#ifndef NO_RUNTIME_LOG
            bar_len_log(bar[k],k,why);
#endif
        }
#ifndef NO_RUNTIME_LOG
        if(!logged_main){logged_main=true;bar_len_log(bar_source,4,why);}
#endif
        if(bar_hp_logs<12) {
            ++bar_hp_logs;
            log_event("duel_multi_bar_hp",(ok?1u:0u)|(why<<8)|(row<<16),(id_of(bar_person[row][0])<<16)|id_of(bar_person[row][1]),
                      (static_cast<unsigned>(hp[row][0])&0xFF)|((static_cast<unsigned>(hp[row][1])&0xFF)<<8));
        }
    }
}
// local308: the chips of the copies on their places (chip mode; nothing otherwise), published every frame (duel_digits keeps only a
// change). Name and 무력 are read again only for another person. duel_multi_chip (once per copy and build): army copy | portrait << 4
// | row << 8 | side << 12 | measured face place << 16, value middle x | y << 16 (1920 x 1080), hex ring radius * 10 | HP << 16 | copy x
// / 4 << 24 (wraps).
static Obj chip_who[kBarCopies];
static int chip_power[kBarCopies];
static wchar_t chip_name[kBarCopies][duel_digits::kNameCap];
static void chips_publish() {
    duel_digits::ChipSet cs;
    memset(&cs,0,sizeof cs);
    if(chip_mode)
        for(unsigned k=0;k<kBarCopies;++k) {
            if(!bar[k] || !bar_scaled[k])continue;
            const unsigned row=k>>1,half=k&1u,side=(half==0)==bar_flip?1u:0u;
            Obj person=bar_person[row][side];
            if(!valid(person))continue;
            if(chip_who[k]!=person) {
                chip_who[k]=person;chip_power[k]=static_cast<int>(power_of(person));
                const wchar_t *n=fn<const wchar_t *(__fastcall *)(Obj)>(kName)(person);
                int i=0;
                for(;n && i+1<duel_digits::kNameCap && n[i];++i)chip_name[k][i]=n[i];
                chip_name[k][i]=0;
            }
            duel_digits::ChipSet::Chip &ch=cs.chip[k];
            const float s=duel_digits::kChipScale;
            ch.on=true;ch.face=!bar_face_off[k];
            ch.cx=static_cast<float>(bar_chip_x[k])+(face_x(k)+duel_digits::kFaceCX)*s;
            ch.cy=static_cast<float>(chip_pane_y())+(face_y(k)+duel_digits::kFaceCY)*s;
            ch.r=duel_digits::kFaceR*s;
            ch.hp=bar_hp[row][side];ch.power=chip_power[k];
            memcpy(ch.name,chip_name[k],sizeof ch.name);
            if(!chips_logged[k]) {
                chips_logged[k]=true;
                log_event("duel_multi_chip",k|(ch.face?1u<<4:0u)|(row<<8)|(side<<12)|(bar_face_at[k][0]>=0.0f?1u<<16:0u),
                          (static_cast<unsigned>(ch.cx)&0xFFFFu)|(static_cast<unsigned>(ch.cy)<<16),
                          (static_cast<unsigned>(ch.r*10.0f)&0xFFFFu)|((static_cast<unsigned>(ch.hp)&0xFFu)<<16)|((static_cast<unsigned>(bar_chip_x[k]/4)&0xFFu)<<24));
            }
        }
    duel_digits::publish_chips(cs);
}
// duel_multi_bar (per copy built): army built | flip << 1 | shown side 0 << 2 | side 1 << 3 | copy << 4, value ids side 0
// << 16 | side 1, hex HP side 0 | side 1 << 8 | builds << 16. Cell A (gauge +0x180, left group node 6) takes the side the original
// gauge shows on the left; both copies read the same two cells. Copy 0 shows the left half, copy 1 the right half; a copy whose side
// has nobody waiting is not built.
static void bar_tick(Obj m) {
    if(!duel_manual::scene_on(m) || !bar_registered || bar_failed)return;
    Obj d2=pointer(m,0x5A0);
    const unsigned page_no=d2?dword(d2,0):0u;
    Obj page=d2 && page_no>=1 && page_no<=4?pointer(d2,0x30):nullptr;
    Obj g1=page?pointer(page,0x1C8):nullptr;
    if(bar_any() && (page!=bar_page || g1!=bar_source))bar_release(3);   // never touch the old gauge: its page may be gone
    Obj who[kBarRows][2];int hp[kBarRows][2];bool show[kBarRows][2];
    bar_want(who,hp,show);
    bool other=false,any=false;
    for(unsigned r=0;r<kBarRows;++r)for(unsigned s=0;s<2;++s){if(who[r][s]!=bar_person[r][s] || show[r][s]!=bar_shown[r][s])other=true;if(show[r][s])any=true;}
    if(bar_any() && (bar_dirty || other))bar_release(bar_dirty?2u:1u);
    bar_dirty=false;
    if(!g1)return;
    if(!any) {bar_release(4);return;}
    layout_client::UiScope scope(kOwner);
    if(!scope.ok) {bar_failed=true;log_event("duel_multi_bar",0xFF,0,0);return;}
    if(!bar_any()) {
        for(unsigned r=0;r<kBarRows;++r)if(!valid(who[r][0]) || !valid(who[r][1]))return;
        memset(bar_cell,0,sizeof bar_cell);
        bar_flip=pointer(g1,0x188)==slot_of(m,0);
        for(unsigned r=0;r<kBarRows;++r)for(unsigned s=0;s<2;++s) {
            // local298 (user 「빠진무장이 체력이 100이되어버렸어」): 809110 stores the bar width from the cells but never moves the bar
            // nodes (they keep the full default length) and 8269A0 moves them only when a cell differs from what 809110 stored. So the
            // copy is built at 100 / 100 (full nodes = stored full) and the real HP goes in below, in the same frame, with 8269A0.
            putptr(bar_cell[r][s],0xB0,who[r][s]);put32(bar_cell[r][s],0x3C,100);put32(bar_cell[r][s],0x40,100);
            bar_person[r][s]=who[r][s];bar_hp[r][s]=100;bar_shown[r][s]=show[r][s];
        }
        ++bar_builds;bar_frames=0;
        for(unsigned k=0;k<kBarCopies;++k){bar_scaled[k]=bar_cut[k]=bar_face_off[k]=false;bar_writes[k]=bar_pending[k]=0;}
        memset(bar_hide,0,sizeof bar_hide);memset(bar_face,0,sizeof bar_face);
        static const int pips[8]={0x1B,0x1C,0x1D,0x1E,0x1F,0x35,0x36,0x37};   // round numbers belong to the main duel only
        for(unsigned k=0;k<kBarCopies;++k)for(int n:pips)bar_hide[k][n]=1;   // local288: 8269A0 shows 0x35 again; re-hidden with the cut
        chip_mode=chips_wanted();
        static const int kChipPreHide[5]={5,6,32,1,3};   // local308: the centre band, both groups, both faces (local307 rectangles)
        for(unsigned k=0;k<kBarCopies;++k){bar_face_at[k][0]=bar_face_at[k][1]=-1.0f;bar_chip_x[k]=0;chips_logged[k]=false;if(chip_mode)for(int n:kChipPreHide)bar_hide[k][n]=1;}
        for(Obj &w:chip_who)w=nullptr;   // name and 무력 read again for this build
        for(unsigned k=0;k<kBarCopies;++k) {
            const unsigned row=k>>1,half=k&1u,side=(half==0)==bar_flip?1u:0u;   // the side the original gauge shows in this half
            if(!show[row][side])continue;
            Obj g=static_cast<Obj>(layout_client::ui_duel_gauge(bar_cell[row][bar_flip?1:0],bar_cell[row][bar_flip?0:1],0,chip_mode?chip_pane_y():bar_row_y(k)));
            log_event("duel_multi_bar",(g?1u:0u)|(bar_flip?2u:0u)|(show[row][0]?4u:0u)|(show[row][1]?8u:0u)|(k<<4),
                      (id_of(who[row][0])<<16)|id_of(who[row][1]),
                      (static_cast<unsigned>(hp[row][0])&0xFF)|((static_cast<unsigned>(hp[row][1])&0xFF)<<8)|(bar_builds<<16));
            if(!g) {bar_failed=true;bar_release(9);return;}
            bar[k]=g;
            for(int n:pips)layout_client::ui_node_visible(g,n,0);
            if(chip_mode)bar_apply_hide(k);
            else layout_client::ui_node_visible(g,half?6:32,0);   // the other half's group until bar_cut_half runs
        }
        bar_page=page;bar_source=g1;bar_hp_logs=0;
        bar_fill_real(hp,1);
    }
    ++bar_frames;
    for(unsigned k=0;k<kBarCopies;++k) {
        if(!bar[k])continue;
        if(!bar_cut[k] && bar_frames>=2)bar_cut_half(k);   // one frame after the build: world matrices valid, still full size
        // Scaled only after the cut, re-applied every frame (1 written, 6 not updated yet, 2 applied to the children). Given up for the
        // scene when the game resets it every frame (more than 4 writes in a row; one now and then, e.g. at an HP refresh, is fine) or
        // it is not applied within kBarPending frames: both copies get 1.0 back and stay at x 0 (duel_multi_bar_scale army copy |
        // 0x2000 resets / 0x4000 never applied, value writes in a row | pending frames << 16, hex frame).
        int r=-1;
        if(bar_cut[k] && !scale_off) {
            r=layout_client::ui_pane_scale(bar[k],chip_mode?chip_scale():kBarScale);
            if(r==8){bar_hide_faces(k);r=2;}   // local288: applied, but a portrait did not scale
            bar_writes[k]=r==1?bar_writes[k]+1:0;
            bar_pending[k]=r==6?bar_pending[k]+1:0;
            if(bar_writes[k]>4 || bar_pending[k]>kBarPending) {
                if(chip_mode) {   // local308: no chips this scene; the local298 copies from the next frame
                    chip_fail=true;
                    log_event("duel_multi_bar_scale",k|32768u|(bar_writes[k]>4?1u<<13:1u<<14),bar_writes[k]|(bar_pending[k]<<16),bar_frames);
                    bar_release(5);return;
                }
                scale_off=true;
                log_event("duel_multi_bar_scale",k|(bar_writes[k]>4?1u<<13:1u<<14),bar_writes[k]|(bar_pending[k]<<16),bar_frames);
                for(unsigned j=0;j<kBarCopies;++j) {
                    if(!bar[j])continue;
                    layout_client::ui_pane_scale(bar[j],1.0f);
                    if(bar_scaled[j] && bar_row_x(j))layout_client::ui_move(bar[j],0,bar_row_y(j));
                    bar_scaled[j]=false;
                }
                r=-1;
            }
        }
        if(r==2 && !bar_scaled[k]) {   // the children follow: the right half may move under its duelist
            bar_scaled[k]=true;
            if(chip_mode) {   // local308: onto its chip place, then its portrait back
                bar_chip_x[k]=chip_x(k);
                layout_client::ui_move(bar[k],bar_chip_x[k],chip_pane_y());
                if(!bar_face_off[k])for(int i=0;i<kBarNodes;++i)if(bar_face[k][i]){bar_hide[k][i]=0;layout_client::ui_node_visible(bar[k],i,1);}
            }
            else if(bar_row_x(k))layout_client::ui_move(bar[k],bar_row_x(k),bar_row_y(k));
        }
        layout_client::ui_follow(bar[k],g1);
#ifndef NO_RUNTIME_LOG
        if(bar_frames==1 || bar_frames==30)bar_probe(k,bar_frames);
#endif
    }
    bar_fill_real(hp,2);
    chips_publish();
}
// Layout row: the page update's gauge refresh, forwarded unchanged (the row makes duel_multi a published UI owner).
static void __fastcall gauge_hp(Obj gauge) { COST_SCOPE("duel_multi::gauge_hp");
    auto original=next_gauge_hp?next_gauge_hp:fn<void(__fastcall *)(Obj)>(kGaugeFill);
    original(gauge);
    if(!gauge_logged && alive) {gauge_logged=true;log_event("duel_multi_gauge1",gauge==bar_source?1u:0u,bar_mask(),bar_builds);}
}

// local298 relay hold framing: an extra slot (bank r, side s) with a model that the engine has brought in.
[[maybe_unused]] static bool rider_on_stage(unsigned r,unsigned s) {   // relay only (diagnostic builds)
    Obj slot=slot_of(bank(r),s);
    return pointer(slot,kModel) && !duel_manual::rider_hidden(pointer(slot,0xB0));
}
static void zoom_scene_reset();   // local290 round camera pull-back (defined with it below)
static void zoom_scene_log();
// Mirror of 37D040's per-slot release; the model is returned only to the manager it came from (a closed strategy screen frees
// every model itself through 1CAEE0).
static void release_all(unsigned why) {
    bar_release(0x10|why);
    if(!alive)return;
    Obj now=models();
    for(unsigned r=0;r<2;++r)for(unsigned s=0;s<2;++s) {   // a slot without a model is skipped
        Obj slot=slot_of(bank(r),s);
        *reinterpret_cast<Obj *>(slot+0x148)=nullptr;*reinterpret_cast<unsigned *>(slot+0x128)=0;*reinterpret_cast<Obj *>(slot+0x138)=nullptr;
        *reinterpret_cast<int *>(slot+0x1C)=-1;*reinterpret_cast<int *>(slot+0x10)=-1;
        step=40+r*2+s;
        if(now && now==model_owner && pointer(slot,kModel))fn<void(__fastcall *)(Obj,Obj,int)>(kRelease)(now,slot,0);   // the model whose +0x40 is this slot
        *reinterpret_cast<Obj *>(slot+kModel)=nullptr;*reinterpret_cast<int *>(slot+0xA0)=-1;*reinterpret_cast<unsigned *>(slot+0xA8)=0;
    }
    log_event("duel_multi_release",why,now==model_owner?1u:0u,
              (id_of(picked[0][0])<<16)|id_of(picked[0][1]));
    zoom_scene_log();
    // local292 duel_multi_motion: army actions queued, value actions given (delayed or not), hex queue overflows | swapped rounds << 8 |
    // swapped frames in which the two duelists had different actions << 16 (0 = every swap looked the same as no swap)
    log_event("duel_multi_motion",motion_pushes,motion_applies,(motion_full&0xFF)|((swap_rounds&0xFF)<<8)|((swap_diff<65535u?swap_diff:65535u)<<16));
    alive=false;scene_manager=model_owner=nullptr;
    for(unsigned r=0;r<2;++r)picked[r][0]=picked[r][1]=nullptr;
}

// ---- top HUD (the original gauge +0x1C8 of the 2D page) ----
static void copy_text(wchar_t *dst,const wchar_t *src) {   // bounded copy, empty for null
    unsigned n=0;
    if(src)for(;n<63 && src[n];++n)dst[n]=src[n];
    dst[n]=0;
}
static bool node_ok(Obj layout,int index) {   // layout node array +8, count +0x10 (9332B0's own check)
    return index<static_cast<int>(dword(layout,0x10)) && pointer(pointer(layout,8)+static_cast<unsigned>(index)*8,0)!=nullptr;
}
// local301 (user 「호위, 단기는 조운개성인데 유비도 그대로 이어받네? … 개성도 같이 움직여야하는데」): the gauge's 개성 labels follow the
// fighter with 809110's own fill — slots 0-8, ids 1..0x163 that 89D800(0x10, id) shows in a duel, not 0x57, at most five, on the
// block's labels kTraitNode in order (text 30E1B0(id), colour 859660(control, trait +0xBE, 1), shown; the rest hidden). traits9's
// nine-slot grid (ModPack DLL) is made once with the gauge for the first fighter: on a side where it is up the grid stays that
// fighter's and the labels stay hidden (the known limit; duel_multi_hud reports it). The tooltip over a label is the original's.
static const int kTraitNode[2][5]={{13,15,17,19,21},{39,41,43,45,47}};
static wchar_t hud_trait[2][5][64];   // label texts handed to 9332B0 per side (copy_text writes up to 63)
// The traits9 screens' own grid rule (integrated patch 26B0, kept in the ModPack's screen header): nine slots when more than five
// traits show, or when the last one sits past the fifth slot. Written here so the duel module does not reach into that module.
static bool traits9_grid(unsigned n,const int *slots) {return n>5 || (n!=0 && slots[n-1]>4);}
static unsigned duel_traits(Obj person,int ids[9],int slots[9]) {
    unsigned n=0;
    if(!valid(person))return 0;
    for(int i=0;i<9;++i) {
        const int id=fn<int(__fastcall *)(Obj,int)>(GA(GA_232030))(person,i);
        if(static_cast<unsigned>(id-1)>=0x163u || id==0x57 || !fn<int(__fastcall *)(int,int)>(GA(GA_89D800))(0x10,id))continue;
        ids[n]=id;slots[n]=i;++n;
    }
    return n;
}
// 0 labels set, 1 traits9 grid on this side (left alone), 2 a label node missing; shown = labels shown.
static unsigned trait_refresh(Obj layout,unsigned block,Obj person,Obj first,unsigned &shown) {
    shown=0;
    int ids[9],slots[9];
    if(mod_features.traits9 && first) {
        const unsigned n0=duel_traits(first,ids,slots);
        if(traits9_grid(n0,slots))return 1;
    }
    for(int k=0;k<5;++k)if(!fn<Obj(__fastcall *)(Obj,int)>(kNode)(layout,kTraitNode[block][k]))return 2;
    Obj root=world();
    const unsigned n=root?duel_traits(person,ids,slots):0;
    for(int k=0;k<5;++k)fn<void(__fastcall *)(Obj,int,int)>(GA(GA_931DA0))(layout,kTraitNode[block][k],0);
    for(unsigned k=0;k<n && k<5;++k) {
        const int node=kTraitNode[block][k],id=ids[k];
        copy_text(hud_trait[block][k],fn<const wchar_t *(__fastcall *)(int)>(GA(GA_30E1B0))(id));
        fn<void(__fastcall *)(Obj,int,const wchar_t *,int)>(kTextSet)(layout,node,hud_trait[block][k],1);
        Obj trait=pointer(root,0x7DD18+static_cast<unsigned>(id)*8u);
        if(Obj control=fn<Obj(__fastcall *)(Obj,int)>(kNode)(layout,node))
            if(trait)fn<void(__fastcall *)(Obj,unsigned char,int)>(GA(GA_859660))(control,*reinterpret_cast<const unsigned char *>(trait+0xBE),1);
        fn<void(__fastcall *)(Obj,int,int)>(GA(GA_931DA0))(layout,node,1);
        ++shown;
    }
    return 0;
}
// duel_multi_hud: army side | gauge block << 4 | why << 8 (0 set, 1 2D not on page 4, 2 no page / gauge, 3 slot not in the gauge,
// 4 node or person missing) | local301 개성 labels << 12 (0 set, 1 traits9 grid left, 2 a label missing) | labels shown << 14, value
// person id | 무력 << 16, hex nodes found (1 face child, 2 name, 4 무력).
static void hud_refresh(Obj m,unsigned side,Obj person,Obj first) {
    Obj orig=slot_of(m,side),d2=pointer(m,0x5A0),page=nullptr,gauge=nullptr;
    unsigned why=0,block=0,nodes=0,power=0,traits=0,shown=0;
    if(!d2 || dword(d2,0)!=4)why=1;
    else if(!(page=pointer(d2,0x30)) || !(gauge=pointer(page,0x1C8)))why=2;
    else if(pointer(gauge,0x180)==orig)block=0;
    else if(pointer(gauge,0x188)==orig)block=1;
    else why=3;
    if(!why) {
        Obj layout=gauge+0x130;
        const int face=block?3:1,name=block?0x24:10,stat=block?0x26:0xC;
        nodes=(fn<Obj(__fastcall *)(Obj,int)>(kNode)(layout,face)?1u:0u)|(node_ok(layout,name)?2u:0u)|(node_ok(layout,stat)?4u:0u);
        if(nodes!=7 || !valid(person))why=4;
        else {
            fn<void(__fastcall *)(Obj,int,Obj)>(kFaceSet)(layout,face,person);
            copy_text(hud_text[side][0],fn<const wchar_t *(__fastcall *)(Obj)>(kName)(person));
            fn<void(__fastcall *)(Obj,int,const wchar_t *,int)>(kTextSet)(layout,name,hud_text[side][0],1);
            power=power_of(person);
            copy_text(hud_text[side][1],fn<const wchar_t *(__fastcall *)(int)>(kNumber)(static_cast<int>(power)));
            fn<void(__fastcall *)(Obj,int,const wchar_t *,int)>(kTextSet)(layout,stat,hud_text[side][1],1);
            traits=trait_refresh(layout,block,person,first,shown);
        }
    }
    log_event("duel_multi_hud",side|(block<<4)|(why<<8)|(traits<<12)|(shown<<14),id_of(person)|(power<<16),nodes);
}
// The relay (diagnostic builds) is its only caller; first = the side's first fighter (the one the gauge was made with).
[[maybe_unused]] static void hud_guarded(Obj m,unsigned side,Obj person,Obj first) {
    __try {hud_refresh(m,side,person,first);}
    __except(EXCEPTION_EXECUTE_HANDLER) {log_event("duel_multi_exception",step,alive?1u:0u,3);}
}

// ---- playback ----
[[maybe_unused]] static Obj holder(Obj m,unsigned side,Obj person) {   // the slot riding `person` (seat, first or second extra bank)
    Obj c[3]={slot_of(m,side),slot_of(shadow,side),slot_of(reserve,side)};
    for(Obj x:c)if(person && pointer(x,0xB0)==person && pointer(x,kModel))return x;
    return nullptr;
}
// duel_multi_swap: army side | relay block << 4 | why << 8 (0 swapped, 1 incoming officer rides no slot, 2 model owner check failed, 3
// already seated), value incoming id | outgoing id << 16, hex bank of the incoming (1 first, 2 second extra) | owner flags << 4.
// Everything travels with the model: look +0x14..+0x27 (3753B0 model type, motion sets), model +0x1B0 (+0x40 back to the slot),
// person +0xB0 (827270's win banner, 809110) and id +0xA8. Positions stay with the slots, so the two riders change places at once.
[[maybe_unused]] static unsigned swap_in(Obj m,unsigned side,Obj person,unsigned k) {   // relay only (diagnostic builds)
    Obj seat=slot_of(m,side),from=holder(m,side,person),out=pointer(seat,0xB0);
    const unsigned head=side|(k<<4),ids=id_of(person)|(id_of(out)<<16);
    const unsigned where=from==slot_of(shadow,side)?1u:from==slot_of(reserve,side)?2u:0u;
    if(!from) {log_event("duel_multi_swap",head|(1u<<8),ids,0);return 1;}
    if(from==seat) {log_event("duel_multi_swap",head|(3u<<8),ids,0);return 3;}
    Obj mo=pointer(seat,kModel),mm=pointer(from,kModel);
    const unsigned owners=(mo && pointer(mo,0x40)==seat?1u:0u)|(mm && pointer(mm,0x40)==from?2u:0u);
    if(owners!=3) {log_event("duel_multi_swap",head|(2u<<8),ids,where|(owners<<4));return 2;}
    unsigned char look[0x14];
    memcpy(look,seat+0x14,sizeof look);memcpy(seat+0x14,from+0x14,sizeof look);memcpy(from+0x14,look,sizeof look);
    putptr(seat,kModel,mm);putptr(from,kModel,mo);putptr(mo,0x40,from);putptr(mm,0x40,seat);
    putptr(seat,0xB0,person);putptr(from,0xB0,out);
    const unsigned id_seat=dword(seat,0xA8),id_from=dword(from,0xA8);
    put32(seat,0xA8,id_from);put32(from,0xA8,id_seat);
    log_event("duel_multi_swap",head,ids,where|(owners<<4));
    return 0;
}

// ---- riders ----
// Spots fixed to the duel (see kLane0): the axis and the enemy side lock at the first frame, the lane side p at the first duel-camera
// frame (until then the default side, and the riders snap once when it locks), d in the round states. Snaps only when a duelist
// jumps (> kCutMain, the original repositions them), the rounds begin or a spot is new; a camera cut is logged only. duel_multi_cut
// (up to 20 per scene): army cuts | state << 8 | round << 16, value eye x / z jump. duel_multi_lane (up to 8 per scene): army 1 axis
// locked (| enemy side << 4, value / hex u x / z * 1000 + 100000) / 2 lane side locked (bit 8: from the duel camera, value / hex p x
// / z * 1000 + 100000) / 3 round direction d first taken or turned by more than about 8 degrees (| state << 8, value / hex d x / z *
// 1000 + 100000) / 4 (local287) the enemy's travel f first known or turned the same way (| mirrored side << 4 | state << 8, value /
// hex f x / z * 1000 + 100000); up to 12 per scene (1 and 2 within the first 8). Returns true while the duel
// camera is current.
static bool riders_step(Obj m) {
    Obj o[2]={slot_of(m,0),slot_of(m,1)};
    const float mx[2]={flt(o[0],0xC0),flt(o[1],0xC0)},mz[2]={flt(o[0],0xC8),flt(o[1],0xC8)};
    bool snap=false;
    float step_m[2][2]={};   // this frame's move of each duelist (0 on the first frame and on a jump)
    for(unsigned s=0;s<2;++s) {
        const float dx=mx[s]-last_main[s][0],dz=mz[s]-last_main[s][1];
        if(main_known && dx*dx+dz*dz>kCutMain*kCutMain)snap=true;
        else if(main_known){step_m[s][0]=dx;step_m[s][1]=dz;}
        last_main[s][0]=mx[s];last_main[s][1]=mz[s];
    }
    main_known=true;
    // local295: the relay hold turned the pair back to its hold-start heading at once (duel_manual hold_finish): every rider jumps with
    // it, and the travel starts again from the pair's heading (37D930 moves a rider along (sin, cos) of +0xF0)
    const unsigned snap_now=duel_manual::snap_serial();
    if(snap_now!=snap_seen) {
        snap_seen=snap_now;snap=true;
        const float ha=flt(o[0],0xF0),hb=flt(o[1],0xF0),sx=std::sin(ha)+std::sin(hb),sz=std::cos(ha)+std::cos(hb),sl=std::sqrt(sx*sx+sz*sz);
        if(sl>1e-3f) {travel_f[0]=sx/sl;travel_f[1]=sz/sl;travel_acc[0]=travel_f[0]*5.0f;travel_acc[1]=travel_f[1]*5.0f;travel_known=true;}
    }
    float hold_f[2]={0.0f,1.0f};
    const bool hold=duel_manual::hold_dir(m,hold_f);   // local295: the relay hold rides the pair; its direction is the riders' forward
    if(!axis_known) {
        float ux=mx[1]-mx[0],uz=mz[1]-mz[0];
        const float l=std::sqrt(ux*ux+uz*uz);
        if(l>1e-3f){ux/=l;uz/=l;}else{ux=1;uz=0;}
        axis_u[0]=ux;axis_u[1]=uz;axis_p[0]=-uz;axis_p[1]=ux;axis_known=true;
        // +0x3C = arg +0x1C, the player's arg ROW (3747A0 picks [0] by it); slots map to rows through +0xA4. None: side 1 is the enemy.
        const unsigned player_row=dword(m,0x3C);
        enemy_side=player_row<=1 && dword(o[0],0xA4)!=player_row?0u:1u;
        if(lane_logs<8){++lane_logs;log_event("duel_multi_lane",1|(enemy_side<<4),as_int(ux*1000.0f)+100000u,as_int(uz*1000.0f)+100000u);}
    }
    float c[80];unsigned vt=0;
    const bool cam=!read_cam(c,vt) && near_stage(c,m);
    if(cam) {
        const float ex=c[0]-last_eye[0],ey=c[1]-last_eye[1],ez=c[2]-last_eye[2];
        if(eye_known && ex*ex+ey*ey+ez*ez>kCutEye*kCutEye && cut_logs<20) {
            ++cut_logs;log_event("duel_multi_cut",cut_logs|((dword(m,0x4A0)&0xFF)<<8)|((dword(m,0x4A8)&0xFF)<<16),as_int(ex)+100000u,as_int(ez)+100000u);
        }
        last_eye[0]=c[0];last_eye[1]=c[1];last_eye[2]=c[2];eye_known=true;
        if(!lane_locked) {
            const float cx=(mx[0]+mx[1])*0.5f,cz=(mz[0]+mz[1])*0.5f;
            if((c[0]-cx)*axis_p[0]+(c[2]-cz)*axis_p[1]>0){axis_p[0]=-axis_p[0];axis_p[1]=-axis_p[1];snap=true;}   // far side (local284)
            lane_locked=true;
            if(lane_logs<8){++lane_logs;log_event("duel_multi_lane",2|(1u<<8),as_int(axis_p[0]*1000.0f)+100000u,as_int(axis_p[1]*1000.0f)+100000u);}
        }
    }
    const unsigned state=dword(m,0x4A0);
    if(state==0xB || state==0xD) {   // the rounds: the original has seated the duelists side by side (followed every round frame)
        const unsigned e=enemy_side,p=e^1u;
        float dx=mx[e]-mx[p],dz=mz[e]-mz[p];
        const float l=std::sqrt(dx*dx+dz*dz);
        if(l>1e-3f) {
            dx/=l;dz/=l;
            const bool changed=!round_locked || dx*round_d[0]+dz*round_d[1]<0.99f;
            round_d[0]=dx;round_d[1]=dz;
            if(!round_locked)snap=true;   // the phase change; later changes are eased (or snapped by a duelist jump)
            round_locked=true;
            if(changed && lane_logs<12) {
                ++lane_logs;log_event("duel_multi_lane",3|((state&0xFF)<<8),as_int(dx*1000.0f)+100000u,as_int(dz*1000.0f)+100000u);
            }
        }
        const float sx=step_m[enemy_side][0],sz=step_m[enemy_side][1],sl=sx*sx+sz*sz;   // local287: the enemy duelist's travel
        if(sl>0.25f && !hold) {   // local295: not the hold's ride (its direction comes from duel_manual)
            travel_acc[0]=travel_acc[0]*0.8f+sx;travel_acc[1]=travel_acc[1]*0.8f+sz;
            const float al=std::sqrt(travel_acc[0]*travel_acc[0]+travel_acc[1]*travel_acc[1]);
            if(al>1.0f) {
                const float fx=travel_acc[0]/al,fz=travel_acc[1]/al;
                if(!travel_known || fx*travel_f[0]+fz*travel_f[1]<0.99f)travel_turned=true;
                travel_f[0]=fx;travel_f[1]=fz;travel_known=true;
            }
        }
    }
    const bool in_rounds=round_locked && state>=8;
    float rf[2]={0.0f,1.0f},rl[2]={1.0f,0.0f};   // rounds: forward = the enemy duelist's travel, rl = the pair's side of it
    if(in_rounds) {
        if(hold){rf[0]=hold_f[0];rf[1]=hold_f[1];}
        else if(travel_known){rf[0]=travel_f[0];rf[1]=travel_f[1];}
        else {rf[0]=-round_d[1];rf[1]=round_d[0];}   // not moving yet: right of d (the local286 play: player on the enemy's right)
        rl[0]=rf[1];rl[1]=-rf[0];                    // left of the travel, (f.z, -f.x)
        const unsigned e=enemy_side,p=e^1u;
        const bool mirror=(mx[p]-mx[e])*rl[0]+(mz[p]-mz[e])*rl[1]>0;   // the player rides there: the enemy's right-behind instead
        if(mirror){rl[0]=-rl[0];rl[1]=-rl[1];}
        if(travel_turned && !hold && lane_logs<12) {
            ++lane_logs;log_event("duel_multi_lane",4|(mirror?1u<<4:0u)|((state&0xFF)<<8),as_int(rf[0]*1000.0f)+100000u,as_int(rf[1]*1000.0f)+100000u);
        }
        travel_turned=false;
    }
    // local295 relay (user 2026-11-22 「메인 일기토의 각 편 side에 서서 달려야지 우리편은 우리편뒤에 적편은 적편뒤에 (지금정도의 상대위치가 좋아)」):
    // each side's riders on its own outside, away from the other duelist across the forward (same distances: kSide / kRear); duelists
    // in line along the forward: the player side on rl, the enemy on the other side
    float lat[2][2]={{rl[0],rl[1]},{rl[0],rl[1]}};
    const bool own_side=in_rounds && duel_manual::scene_on(m);
    if(own_side)
        for(unsigned s=0;s<2;++s) {
            float vx=mx[s]-mx[s^1u],vz=mz[s]-mz[s^1u];
            const float along=vx*rf[0]+vz*rf[1];
            vx-=along*rf[0];vz-=along*rf[1];
            const float l=std::sqrt(vx*vx+vz*vz);
            if(l>1.0f){lat[s][0]=vx/l;lat[s][1]=vz/l;}
            else if(s==enemy_side){lat[s][0]=-rl[0];lat[s][1]=-rl[1];}
        }
    // local292: in states 0xA..0xE every extra gets its duelist's action kActDelay frames late instead of on the same frame.
    ++motion_frames;
    const bool delay_on=state>=0xA && state<=0xE,relay=duel_manual::scene_on(m);
    for(unsigned r=0;r<2;++r)for(unsigned s=0;s<2;++s) {
        Obj mine=slot_of(bank(r),s),orig=slot_of(m,s);
        if(!pointer(mine,kModel)){form_ok[r][s]=false;continue;}
        if(!fallen[r][s]) {
            float tx,tz;
            if(in_rounds) {
                const float side=r?kSide1:kSide0,rear=r?kRear1:kRear0;
                tx=mx[s]+lat[s][0]*side-rf[0]*rear;tz=mz[s]+lat[s][1]*side-rf[1]*rear;
            } else {
                const float lane=r?kLane1:kLane0,back=(r?kBack1:0.0f)+(state==6 || state==7?kChargeBack:0.0f);
                const float bx=s?axis_u[0]:-axis_u[0],bz=s?axis_u[1]:-axis_u[1];   // away from the other side
                tx=mx[s]+axis_p[0]*lane+bx*back;tz=mz[s]+axis_p[1]*lane+bz*back;
            }
            form_pt[r][s][0]=tx;form_pt[r][s][1]=flt(orig,0xC4);form_pt[r][s][2]=tz;form_ok[r][s]=true;
            Rider &d=riders[r][s];
            // relay scene (duel_manual): an extra the engine has not brought in stays far off and under the ground; when it comes in it
            // rides in from kEnterBack behind its spot (eased)
            const bool hide=duel_manual::rider_hidden(pointer(mine,0xB0));
            if(hide) {d.ready=false;rider_away[r][s]=true;}
            else if(rider_away[r][s]) {
                rider_away[r][s]=false;
                const float bx=in_rounds?-rf[0]:(s?axis_u[0]:-axis_u[0]),bz=in_rounds?-rf[1]:(s?axis_u[1]:-axis_u[1]);
                d.x=tx+bx*kEnterBack;d.z=tz+bz*kEnterBack;d.ready=true;
                log_event("duel_multi_enter",s|(r<<4)|((state&0xFF)<<8),id_of(pointer(mine,0xB0)),0);
            }
            if(in_rounds && d.ready && !snap){d.x+=step_m[s][0];d.z+=step_m[s][1];}   // ride along: no lag behind a running duelist
            const float dx=tx-d.x,dz=tz-d.z,dist=std::sqrt(dx*dx+dz*dz);
            if(!d.ready || snap || dist>4000.0f){d.x=tx;d.z=tz;d.ready=true;}
            else if(dist>1e-3f){const float stepd=dist*kEaseRate<kEaseMax?dist*kEaseRate:kEaseMax;d.x+=dx/dist*stepd;d.z+=dz/dist*stepd;}
            {   // the own duelist's action (always on since v16.6); local292: late in the rounds, the opposite duelist's on a swap
                const unsigned from=s;
                int action=static_cast<int>(dword(slot_of(m,from),0x10));
                if(relay && state>=0xB && state<=0xE)action=action==0x15?0x15:8;   // relay: a waiting member rides beside, no attack
                ActQ &q=act_q[r][s];
                int give=-1;
                if(!delay_on){q.count=0;give=action;}   // outside the rounds: the same frame, anything still queued is dropped
                else {
                    if(action>=0 && (action!=q.last_src || from!=q.last_from)) {
                        if(q.count==8){give=q.act[q.head];q.head=(q.head+1)&7u;--q.count;++motion_full;}
                        const unsigned at=(q.head+q.count)&7u;
                        q.due[at]=motion_frames+kActDelay[r];q.act[at]=action;++q.count;++motion_pushes;
                    }
                    while(q.count && static_cast<int>(motion_frames-q.due[q.head])>=0){give=q.act[q.head];q.head=(q.head+1)&7u;--q.count;}
                }
                q.last_src=action;q.last_from=from;
                if(give>=0 && give!=last_action[r][s]) {step=50+r*2+s;fn<void(__fastcall *)(Obj,int)>(kAction)(mine,give);last_action[r][s]=give;++motion_applies;}
            }
            memcpy(mine+0xC0,orig+0xC0,0x30);flt(mine,0xF0)=flt(orig,0xF0);   // height, rotation and facing of the own duelist
            flt(mine,0xC0)=d.x;flt(mine,0xC8)=d.z;flt(mine,0xE0)=d.x;flt(mine,0xE8)=d.z;
            if(hide) {
                const float fx=d.x+axis_p[0]*kHideFar,fz=d.z+axis_p[1]*kHideFar;
                flt(mine,0xC0)=fx;flt(mine,0xC8)=fz;flt(mine,0xE0)=fx;flt(mine,0xE8)=fz;flt(mine,0xC4)-=kHideDepth;
            }
        }
        step=60+r*2+s;fn<void(__fastcall *)(Obj)>(kTiming)(mine+0x128);
    }
    return cam;
}

static void create(Obj manager) {
    Obj owner=models();
    if(!owner) {log_event("duel_multi_skip",3,0,0);return;}
    Obj rep[2]={pointer(slot_of(manager,0),0xB0),pointer(slot_of(manager,1),0xB0)};
    if(!valid(rep[0]) || !valid(rep[1])) {log_event("duel_multi_skip",4,id_of(rep[0]),id_of(rep[1]));return;}
    unsigned why[2]={0,0};
    Obj pick[2][2]={{nullptr,nullptr},{nullptr,nullptr}};   // [side][rank]
    // A relay scene (duel_manual) takes its extras within 2 hexes, each side 0-2 (user 「합류범위 2헥스」, per side as San11).
    // Only a relay scene gets extras (the multi-duel feature is gone, user 2026-11-22): any other duel stays original.
    if(!duel_manual::scene_on(manager)) {log_event("duel_multi_skip",10,id_of(rep[0]),id_of(rep[1]));return;}
    const bool relay=true;
    step=10;neighbours(rep[0],rep[0],rep[1],pick[1],pick[0],why[0],2u);
    step=11;neighbours(rep[1],rep[0],rep[1],pick[0],pick[1],why[1],2u);
    {   // local313: each of the (at most two) strongest is asked once at duel_relay_join * 10 %; a refusal leaves the place empty and
        // nobody else is asked (user 「두번째 무장도 거절하면 그쪽은 1인으로 일기토 하게되는거지」). why 4: asked, all refused.
        // duel_multi_ask (a side with someone asked): army side | asked << 4 | accepted (bit per rank) << 8 | step << 12, value the asked
        // ids rank 0 << 16 | rank 1, hex the scene serial.
        const unsigned join_step=mod_features.duel_relay_join;
        std::uint32_t st=duel_manual_script::join_seed(GetTickCount64(),scene_serial);
        for(unsigned s=0;s<2;++s) {
            unsigned asked=0,yes=0;
            const unsigned ids=(id_of(pick[s][0])<<16)|id_of(pick[s][1]);
            for(unsigned r=0;r<2;++r) {
                if(!pick[s][r])continue;
                ++asked;
                if(duel_manual_script::join_accepts(st,join_step))yes|=1u<<r;
                else pick[s][r]=nullptr;
            }
            if(!pick[s][0]){pick[s][0]=pick[s][1];pick[s][1]=nullptr;}
            if(asked && !yes)why[s]=4;
            if(asked)log_event("duel_multi_ask",s|(asked<<4)|(yes<<8)|(join_step<<12),ids,scene_serial);
        }
    }
    if(!pick[0][0] && !pick[1][0]) {
        log_event("duel_multi_skip",5|(why[0]<<8)|(why[1]<<16),(id_of(rep[0])<<16)|id_of(rep[1]),(id_of(pick[0][0])<<16)|id_of(pick[1][0]));
        if(why[0]!=4 && why[1]!=4){step=12;dump_near(0,rep[0]);step=13;dump_near(1,rep[1]);}   // local313: not after a refusal
        return;
    }
    memset(shadow,0,sizeof shadow);memset(reserve,0,sizeof reserve);
    for(unsigned s=0;s<2;++s) {   // 37CBD0 defaults: 0x10 zero, six 100s, six zeros, six -1
        int *c=reinterpret_cast<int *>(cells[s]);
        for(unsigned k=0;k<4;++k)c[k]=0;
        for(unsigned k=0;k<6;++k){c[4+k]=100;c[10+k]=0;c[16+k]=-1;}
    }
    model_owner=owner;scene_manager=manager;alive=true;
    for(unsigned r=0;r<2;++r)for(unsigned s=0;s<2;++s)picked[r][s]=pick[s][r];
    bool ok=true;
    for(unsigned r=0;r<2;++r)for(unsigned s=0;s<2;++s) {
        if(!picked[r][s])continue;
        Obj mine=slot_of(bank(r),s),orig=slot_of(manager,s);
        step=20+r*2+s;
        fn<void(__fastcall *)(Obj,int,int,int,Obj,Obj)>(kSlotInit)(mine,100,static_cast<int>(s),static_cast<int>(dword(orig,0xA4)),picked[r][s],cells[s]);
        Obj model=pointer(mine,kModel);
        log_event("duel_multi_slot",(r<<20)|(s<<16)|id_of(picked[r][s]),(model?1u:0u)|(power_of(picked[r][s])<<8),(dword(mine,0x14)<<16)|(dword(mine,0x20)&0xFFFF));
        if(!model) {if(r || relay)picked[r][s]=nullptr;else ok=false;}   // a second extra (any relay extra) without a pool model stays out
    }
    if(relay && !picked[0][0] && !picked[0][1] && !picked[1][0] && !picked[1][1])ok=false;
    if(!ok) {log_event("duel_multi_skip",6,pool_calls,0);release_all(3);return;}   // pool still 2 (hook late) or full
    for(unsigned r=0;r<2;++r)for(unsigned s=0;s<2;++s){riders[r][s].ready=false;fallen[r][s]=false;rider_away[r][s]=false;form_ok[r][s]=false;}
    snap_seen=duel_manual::snap_serial();
    memset(act_q,0,sizeof act_q);motion_frames=motion_pushes=motion_applies=motion_full=swap_rounds=swap_logs=swap_round=swap_diff=0;
    eye_known=main_known=axis_known=lane_locked=round_locked=travel_known=travel_turned=false;lane_logs=0;
    travel_acc[0]=travel_acc[1]=0.0f;
    watching=false;watch_frames=state_logs=round_logs=cut_logs=0;last_state=~0u;
    round_logged=false;cam_frames=cam_samples=shot_logs=shot_frames=0;last_shot=~0u;
    zoom_scene_reset();   // local290: the round camera pull-back starts over (off stays for the process)
    bar_dirty=bar_failed=gauge_logged=scale_off=chip_mode=chip_fail=false;bar_builds=0;
#ifndef NO_RUNTIME_LOG
    bar_len_logs=0;
#endif
    for(unsigned r=0;r<2;++r)for(unsigned s=0;s<2;++s) {
        Obj mine=slot_of(bank(r),s),orig=slot_of(manager,s),model=pointer(mine,kModel);
        if(!model)continue;
        step=30+r*2+s;
        fn<void(__fastcall *)(Obj,unsigned,unsigned)>(kModelSet)(model,0,dword(mine,0x20));
        fn<void(__fastcall *)(Obj,unsigned,unsigned)>(kModelSet)(model,1,dword(mine,0x24));
        const int action=static_cast<int>(dword(orig,0x10));
        last_action[r][s]=action>=0?action:0;
        act_q[r][s].last_src=action;act_q[r][s].last_from=s;
        fn<void(__fastcall *)(Obj,int)>(kAction)(mine,last_action[r][s]);
    }
    step=38;riders_step(manager);   // first spots (snapped)
    for(unsigned r=0;r<2;++r)for(unsigned s=0;s<2;++s)if(pointer(slot_of(bank(r),s),kModel))log_side("duel_multi_created",manager,s,r);
    step=36;cam_probe(1,manager,0);
}
// duel_multi_result: army tag (0 scene entry, 1 scene end) | outcome +0x60 << 8 | 일격 +0x68 << 16 | fate +0x6C << 24 (low bytes),
// value side outcome +0x70 | +0xC8 << 16, hex duel HP +0xF1 of the arg persons +0x20 | +0x28 << 8.
static void result_log(unsigned tag,Obj m) {
    Obj a=pointer(m,0x20),b=pointer(m,0x28);
    const unsigned hp=(valid(a)?a[0xF1]:0xFFu)|((valid(b)?b[0xF1]:0xFFu)<<8);
    log_event("duel_multi_result",tag|((dword(m,0x60)&0xFF)<<8)|((dword(m,0x68)&0xFF)<<16)|((dword(m,0x6C)&0xFF)<<24),
              (dword(m,0x70)&0xFFFF)|(dword(m,0xC8)<<16),hp);
}
// ---- round camera pull-back (local290, fit as needed since local291) ----
// local290 (user 2026-11-21 「제안대로 진행」 after the local288 camera survey: the round camera orbits 500-900 from the duelists, the
// back pair 700-1000 away is often off screen, horizontal field of view about 63 degrees). local291 (user 2026-11-21 「필요한 만큼만」
// after the local290 play 「카메라가 왔다갔다해서 자연스럽게 이어지지 않는 느낌」: a fixed 2x made far shots tiny, and a wrong render check
// turned it off after 0.3 s, so the screen jumped back): in a scene where our extras are on stage (alive), during the round states
// (+0x4A0 9..0xE: preparation, round choice, the round actions) the duel camera is moved back along its own view only as far as
// every extra on stage needs to sit inside kFitEdge of the screen width (and below kFitTop), at most kFitShare of the duelists'
// depth and kFitBackMax. A shot where all four already show stays original. Within a shot the pull only grows (eased, kFitEase,
// at most kFitEaseMax a frame) toward the largest need so far, so the camera never dollies out and back in (the local290 samples:
// the back pair went out, in and out again inside one clip); a cut (the original eye jumps more than kZoomCut) starts the new shot
// at its own pull; outside the rounds it eases back to 0. The intro, the charge and the charge finishing blow 8 start at 0 and stay original;
// a plain 1:1 duel never gets here (alive is false).
// Original (offline decompile, build 1a4a447d): the duel camera belongs to the static stage object 375D60() (0x2786790, 0x620 B,
// built once by 373660); the manager DAT_142786780 is a separate 0x5C0 B heap object (374FB0). 37D2C0(stage), the last call of
// 37D600 (CALL 37D767), runs while a camera clip +0x5C8 plays: the clip's position (camera +0x518 vt+0x80 after vt+0x438) turned by
// the shot yaw plus the centre +0x5D0 goes to vt+0x68 (position, x y z w), the rotation to vt+0x70, then vt+0x418(*(+0x510))
// applies it and the camera work +0x520 takes the camera (vt+0x28(camera, 0)). Our tick runs right after 37D600 on the same
// thread, so the same position call and apply finish the frame with the new eye. A frame the original did not place (no clip: the
// camera still holds what we wrote) is left alone, so the pull never adds up.
// The camera block (read_cam) is rewritten by the apply itself (local290 play: read after 37D2C0 it held this frame's original eye,
// read after our apply it held ours), so in our tick it is this frame's original camera: the view direction is the ViewProj w
// column (w = depth along the view in world units), and moving the eye back by d along it adds d to every point's clip w while its
// clip x / y stay, so a point at clip (x, y, w) needs d >= |x| / kFitEdge - w. Used only when the block eye is within kZoomCut of
// the camera position (else the pull of the last frame is held along the last direction). The render check of local290 is log
// only now; the pull is turned off only by an exception or a set that leaves the camera at the original eye.
// local295 (user 2026-11-22 「카메라가 밀리는거 참 별로야」, 「합겨룰떄 줌아웃을 좀 더 땡겨야겠더라」): no easing inside a shot any more: the
// pull is taken at the cut (from the riders' spots, form_pt, not where the easing has them yet) and held for the whole shot; outside the
// rounds, and in a relay hold (its own close shot, duel_manual), the original camera at once. Wider: extras inside 72 % of the half
// width and 80 % of the top, up to 1.4 times the duelists' depth and 2400.
static const float kFitEdge=0.72f,kFitTop=0.8f,kFitShare=1.4f,kFitBackMax=2400.0f,kFitMinDepth=150.0f,kFitMaxDepth=3000.0f,
                   kZoomCut=150.0f,kBlockExact=5.0f;
// local298 (user 2026-11-22 「땅속」 in the round shots): the pulled eye never goes below the ground at its own x, z + kZoomClear, and
// once the relay matched the camera rotation convention (duel_manual::look_at) it is a crane: kCraneRise of the pull upward and the
// rotation re-aimed at the duelists' middle (zoom_depth along this frame's original view). The share stays 1.4 (local295; the 1.8 of
// the old installer note was the local291 value). local299: the rotation is the relay's change of a read-back reference; the first
// aimed frame of a scene is read back (duel_manual::aim_after), a wrong one puts the original rotation back and ends the aim.
static const float kZoomClear=200.0f,kCraneRise=0.35f;
static const unsigned kStageObj=GA(GA_375D60);
alignas(16) static float zoom_last[4];
static float zoom_prev[3],zoom_f[3],zoom_back;   // the last original eye, the last view direction, the pull applied now
static bool zoom_written,zoom_off,zoom_prev_ok,zoom_f_ok,zoom_fresh,zoom_hold_prev;
static Obj zoom_clip_last;   // local295: the stage's camera clip +0x5C8 last frame (a new one in a new state or round = a new shot)
static unsigned zoom_clip_key;   // state | round << 8 when the last clip started
static unsigned zoom_applied,zoom_logs,zoom_clamps,zoom_aims,zoom_ground_miss;
static float zoom_depth;   // the duelists' middle along the view at the cut (the crane's aim)
#ifndef NO_RUNTIME_LOG
// per scene, for duel_multi_zoom_scene / _check / _gap: 0 not placed this frame, round frames: 1 block is this frame's original,
// 2 block lags, 3 block not usable; 4 cuts, 5 apply reached the block, 6 it did not, 7 the camera gave back another eye than we set,
// round frames: 8 needed a pull, 9 all four already showed, 10 duelists' depth out of range; 11 cuts in the rounds with the block
// not usable (one original frame before the new pull), 12 frames whose block at tick start still held our last eye (would mean the
// block is not this frame's original)
static unsigned zoom_n[13];
static unsigned zoom_gap;   // the largest block eye - camera position distance on a round frame (whole units)
#endif
static void zoom_note(unsigned i) {
#ifndef NO_RUNTIME_LOG
    if(i<13)++zoom_n[i];
#else
    (void)i;
#endif
}
static void zoom_scene_reset() {   // per scene (off stays for the process)
    zoom_written=zoom_prev_ok=zoom_f_ok=zoom_fresh=zoom_hold_prev=false;zoom_back=0.0f;zoom_applied=zoom_logs=0;zoom_clip_last=nullptr;zoom_clip_key=~0u;
    zoom_clamps=zoom_aims=zoom_ground_miss=0;zoom_depth=0.0f;
#ifndef NO_RUNTIME_LOG
    memset(zoom_n,0,sizeof zoom_n);zoom_gap=0;
#endif
}
// duel_multi_zoom_scene (scene end): army pulled frames | off << 28, value not placed | needed << 16, hex already showing | cuts << 16.
// duel_multi_zoom_check: army block exact | lagging << 16, value not usable | getter differs << 16, hex apply reached the block |
// did not << 16. duel_multi_zoom_gap: army the largest block gap, value depth out of range | cuts with the block not usable << 16, hex
// block still held our last eye.
static void zoom_scene_log() {
#ifndef NO_RUNTIME_LOG
    const auto cap=[](unsigned v){return v<65535u?v:65535u;};
    log_event("duel_multi_zoom_scene",cap(zoom_applied)|(zoom_off?1u<<28:0u),cap(zoom_n[0])|(cap(zoom_n[8])<<16),cap(zoom_n[9])|(cap(zoom_n[4])<<16));
    log_event("duel_multi_zoom_check",cap(zoom_n[1])|(cap(zoom_n[2])<<16),cap(zoom_n[3])|(cap(zoom_n[7])<<16),cap(zoom_n[5])|(cap(zoom_n[6])<<16));
    log_event("duel_multi_zoom_gap",zoom_gap,cap(zoom_n[10])|(cap(zoom_n[11])<<16),zoom_n[12]);
    log_event("duel_multi_zoom_crane",cap(zoom_clamps),cap(zoom_aims),cap(zoom_ground_miss));   // local298: ground clamps, re-aimed frames, no ground
#endif
}
static Obj stage_obj() {return kStageObj?fn<Obj(__fastcall *)()>(kStageObj)():nullptr;}
template<class F> static F zoom_vslot(Obj object,unsigned off) {return *reinterpret_cast<F *>(pointer(object,0)+off);}
static float zoom_dist(const float *a,const float *b) {
    const float dx=a[0]-b[0],dy=a[1]-b[1],dz=a[2]-b[2];return std::sqrt(dx*dx+dy*dy+dz*dz);
}
struct ZoomClip {float x,y,w;};
static ZoomClip zoom_clip(const float *c,float x,float y,float z) {   // row vector * ViewProj (as project), unnormalized
    const float *m=c+60;
    return ZoomClip{x*m[0]+y*m[4]+z*m[8]+m[12],x*m[1]+y*m[5]+z*m[9]+m[13],x*m[3]+y*m[7]+z*m[11]+m[15]};
}
// The pull this frame needs (0 = all four show), from this frame's original camera block. depth = the duelists' middle along the
// view, worst = the largest |ndc x| of an extra before the pull (99 behind the camera), out_of_range = depth out of range.
static float zoom_need(Obj manager,const float *c,float &depth,float &worst,bool &out_of_range) {
    Obj o0=slot_of(manager,0),o1=slot_of(manager,1);
    depth=zoom_clip(c,(flt(o0,0xC0)+flt(o1,0xC0))*0.5f,(flt(o0,0xC4)+flt(o1,0xC4))*0.5f,(flt(o0,0xC8)+flt(o1,0xC8))*0.5f).w;
    worst=0.0f;out_of_range=!(depth>=kFitMinDepth && depth<=kFitMaxDepth);
    if(out_of_range)return 0.0f;   // looking away or from far: no need of its own
    float need=0.0f;
    for(unsigned r=0;r<2;++r)for(unsigned s=0;s<2;++s) {
        Obj slot=slot_of(bank(r),s);
        if(!pointer(slot,kModel) || duel_manual::rider_hidden(pointer(slot,0xB0)))continue;   // relay: not brought in yet
        const bool spot=form_ok[r][s] && !fallen[r][s];   // where the rider settles (a fallen one stays where it is)
        const ZoomClip k=spot?zoom_clip(c,form_pt[r][s][0],form_pt[r][s][1],form_pt[r][s][2]):zoom_clip(c,flt(slot,0xC0),flt(slot,0xC4),flt(slot,0xC8));
        const float ax=std::fabs(k.x),side=ax/kFitEdge-k.w,top=k.y>0.0f?k.y/kFitTop-k.w:0.0f;
        if(side>need)need=side;
        if(top>need)need=top;
        const float nx=k.w>1e-3f?ax/k.w:99.0f;
        if(nx>worst)worst=nx;
    }
    float cap=depth*kFitShare;
    if(cap>kFitBackMax)cap=kFitBackMax;
    return need<cap?need:cap;
}
// duel_multi_zoom (first pulled frame of a scene, then every 30th, up to 40 per scene): army state | source << 8 (0 block exact,
// 1 block lagging, 3 held) | cut << 12 | pulled frames << 16, value duelists' depth | wanted pull << 16, hex pull applied | worst
// extra |ndc x| * 1000 before it << 16. duel_multi_zoom_mode: army 8 = the set did not take (value pull, hex pulled frames), 9 = an
// exception; both turn the pull-back off for the process.
static void zoom_step(Obj manager) {
    if(zoom_off || !alive)return;
    Obj stage=stage_obj(),cam=stage?pointer(stage,0x518):nullptr;
    if(!cam || !pointer(cam,0))return;
    alignas(16) float pos[4];
    zoom_vslot<void *(__fastcall *)(Obj,float *)>(cam,0x80)(cam,pos);
    if(zoom_written && !memcmp(pos,zoom_last,3*sizeof(float))) {zoom_note(0);return;}   // not placed this frame: our eye stays
    const bool was_written=zoom_written;
    zoom_written=false;
    // a new shot: the eye jumped, or a camera clip started with a new state (local295: a round state restarting the same shot near the
    // same pose moves the eye less than kZoomCut; a clip that follows another inside one state is no cut)
    const unsigned state=dword(manager,0x4A0),key=(state&0xFF)|(dword(manager,0x4A8)<<8);
    Obj clip=pointer(stage,0x5C8);
    const bool clip_start=clip && clip!=zoom_clip_last,new_clip=clip_start && key!=zoom_clip_key;
    if(clip_start)zoom_clip_key=key;
    zoom_clip_last=clip;
    const bool cut=!zoom_prev_ok || new_clip || zoom_dist(pos,zoom_prev)>kZoomCut;
    memcpy(zoom_prev,pos,sizeof zoom_prev);zoom_prev_ok=true;
    if(cut){zoom_note(4);zoom_fresh=true;}
    // local295: a relay hold keeps its own camera (no pull); on the frame it ends the camera is still the hold's (measured at the next
    // shot instead)
    const bool hold_now=duel_manual::holding_now(manager),hold_end=zoom_hold_prev && !hold_now;
    zoom_hold_prev=hold_now;
    if(hold_now || hold_end) {zoom_back=0.0f;zoom_fresh=hold_end;return;}
    const bool rounds=state>=9 && state<=0xE;
    float c[80],f[3]={0.0f,0.0f,0.0f};unsigned vt=0,source=3;
    if(read_cam(c,vt)==0) {
        const float gap=zoom_dist(c,pos),*m=c+60;
#ifndef NO_RUNTIME_LOG
        const unsigned g=gap<65535.0f?static_cast<unsigned>(gap):65535u;
        if(rounds && g>zoom_gap)zoom_gap=g;
        if(was_written && zoom_dist(c,zoom_last)<kBlockExact)zoom_note(12);
#else
        (void)was_written;
#endif
        f[0]=m[3];f[1]=m[7];f[2]=m[11];
        const float n=std::sqrt(f[0]*f[0]+f[1]*f[1]+f[2]*f[2]);
        if(n>0.98f && n<1.02f && gap<kZoomCut) {source=gap<kBlockExact?0u:1u;f[0]/=n;f[1]/=n;f[2]/=n;}
    }
    if(rounds)zoom_note(source==0?1u:source==1?2u:3u);
    float target=0.0f,depth=0.0f,worst=0.0f;
    if(source<3) {
        memcpy(zoom_f,f,sizeof f);zoom_f_ok=true;
        if(rounds && zoom_fresh) {   // the pull of a new shot (measured once, at its cut)
            bool out_of_range=false;
            target=zoom_need(manager,c,depth,worst,out_of_range);
            zoom_depth=depth;
            zoom_note(out_of_range?10u:target>0.0f?8u:9u);
        }
    }
    if(!rounds) {if(zoom_fresh){zoom_back=0.0f;zoom_fresh=false;}}   // outside the rounds: the pull stays until the next cut, then none
    else if(zoom_fresh && source<3) {zoom_back=target;zoom_fresh=false;}   // a cut hides the change: the new shot's pull
    else if(zoom_fresh) {zoom_back=0.0f;zoom_note(11);}   // the block has not caught up with the cut yet: original until it does
    else target=zoom_back;   // inside a shot the pull stays as it was set at the cut (local295: no easing)
    if(zoom_back<1.0f){zoom_back=0.0f;return;}
    const float *dir=source<3?f:zoom_f_ok?zoom_f:nullptr;
    if(!dir)return;
    alignas(16) float eye[4]={pos[0]-dir[0]*zoom_back,pos[1]-dir[1]*zoom_back,pos[2]-dir[2]*zoom_back,pos[3]};
    alignas(16) float rot[4];
    const float aim[3]={pos[0]+dir[0]*zoom_depth,pos[1]+dir[1]*zoom_depth,pos[2]+dir[2]*zoom_depth};
    const bool crane=zoom_depth>kFitMinDepth && duel_manual::look_at(eye,aim,rot);   // the convention is known: rise and re-aim
    if(crane)eye[1]+=zoom_back*kCraneRise;
    bool ground_ok=false;
    const float ground=ground_at(manager,eye[0],eye[2],ground_ok);
    if(!ground_ok)++zoom_ground_miss;
    else if(eye[1]<ground+kZoomClear){eye[1]=ground+kZoomClear;++zoom_clamps;}
    const bool aimed=crane && duel_manual::look_at(eye,aim,rot);
    alignas(16) float rot0[4];   // local299: the original rotation, put back if the aim reads back wrong
    if(aimed)zoom_vslot<void *(__fastcall *)(Obj,float *)>(cam,0x88)(cam,rot0);
    zoom_vslot<void *(__fastcall *)(Obj,float *)>(cam,0x68)(cam,eye);
    if(aimed){zoom_vslot<void *(__fastcall *)(Obj,float *)>(cam,0x70)(cam,rot);++zoom_aims;}
    zoom_vslot<void *(__fastcall *)(Obj,Obj)>(cam,0x418)(cam,pointer(stage,0x510));
    if(aimed && !duel_manual::aim_after(stage,cam,eye,rot0))--zoom_aims;
#ifndef NO_RUNTIME_LOG
    {   // log only: did the apply put our eye in the camera block (what local290 wrongly read before the apply)?
        float a[80];unsigned avt=0;
        if(read_cam(a,avt)==0)zoom_note(zoom_dist(a,eye)<kBlockExact?5u:6u);
    }
#endif
    zoom_vslot<void *(__fastcall *)(Obj,float *)>(cam,0x80)(cam,zoom_last);   // what the camera holds now (the next frame's test)
    if(zoom_dist(zoom_last,eye)>1.0f) {   // the getter gives back another eye
        zoom_note(7);
        if(zoom_dist(zoom_last,pos)<1.0f) {   // still the original eye: the set did nothing we can see, stop for this process
            zoom_off=true;
            log_event("duel_multi_zoom_mode",8,static_cast<unsigned>(static_cast<int>(zoom_back)),zoom_applied);
            return;
        }
    }
    zoom_written=true;
    if(++zoom_applied%30==1 && zoom_logs<40) {
        ++zoom_logs;
        const float w1000=worst*1000.0f;
        const unsigned wv=w1000<65535.0f?static_cast<unsigned>(w1000):65535u;
        log_event("duel_multi_zoom",(state&0xFF)|(source<<8)|(cut?1u<<12:0u)|(zoom_applied<<16),
                  (as_int(depth)&0xFFFFu)|(as_int(target)<<16),(as_int(zoom_back)&0xFFFFu)|(wv<<16));
    }
}
static void zoom_guarded(Obj manager) {
    __try {zoom_step(manager);}
    __except(EXCEPTION_EXECUTE_HANDLER) {zoom_off=true;zoom_written=false;log_event("duel_multi_zoom_mode",9,zoom_applied,0);}
}
#ifndef NO_RUNTIME_LOG
// local288 camera survey (read only, user 2026-11-20 「카메라는 줌아웃 조사만해봐」): the camera shot +0x61C (37C7F0 stores the shot it hands
// 37C8B0; the shot table 1A40F60 holds yaw / pitch in degrees and a third value, 1000 / 500 / 1100 for shots 5 / 15 / 19 only, 0 for
// the rest) and whether a camera clip +0x5C8 plays (37D2C0, called by the state dispatcher 37D600, then places the camera every
// frame: the clip's position turned by the shot's yaw plus the duel centre +0x5D0). local290: these fields belong to the stage object
// 375D60(), not to the manager (local288 read the manager, whose 0x5C0 B end before them: that survey line is void).
// duel_multi_shot on a change of either and again kShotSettle frames into the same shot (local301, user 「장면별 카메라 거리도 다
// 조사해줘 그래야 내가 조정할때 감을 잡으니까」; up to 96 per scene): army shot | state << 8 | clip << 16 | relay hold on << 17 | the
// settled line << 18, value eye distance to the pair's middle (3D, at the centre's height) | eye height above the centre << 16, hex
// eye distance to the centre +0x5D0 on the ground | the two duelists' ground gap << 16 (whole units; 65535 = not read).
static const unsigned kShotSettle=20,kShotLogs=96;   // shot_frames (with shot_logs): frames on the current shot / clip
static void shot_step(Obj manager) {
    Obj m=stage_obj();
    if(!m)return;
    const unsigned shot=dword(m,0x61C),clip=pointer(m,0x5C8)?1u:0u,key=(shot&0xFFFFu)|(clip<<16);
    bool settled=false;
    if(key!=last_shot){last_shot=key;shot_frames=0;}
    else if(++shot_frames==kShotSettle)settled=true;
    else return;
    if(shot_logs>=kShotLogs)return;
    ++shot_logs;
    const float cx=flt(m,0x5D0),cy=flt(m,0x5D4),cz=flt(m,0x5D8);
    Obj a=slot_of(manager,0),b=slot_of(manager,1);
    const float mx=(flt(a,0xC0)+flt(b,0xC0))*0.5f,mz=(flt(a,0xC8)+flt(b,0xC8))*0.5f;
    const float gx=flt(a,0xC0)-flt(b,0xC0),gz=flt(a,0xC8)-flt(b,0xC8),gap=std::sqrt(gx*gx+gz*gz);
    float c[80];unsigned vt=0,pair=65535u,height=65535u,centre=65535u;
    if(read_cam(c,vt)==0) {
        const float dx=c[0]-mx,dy=c[1]-cy,dz=c[2]-mz,d=std::sqrt(dx*dx+dy*dy+dz*dz);
        pair=d<65535.0f?static_cast<unsigned>(d):65535u;
        height=static_cast<unsigned>(static_cast<int>(dy))&0xFFFFu;
        const float ex=c[0]-cx,ez=c[2]-cz,dc=std::sqrt(ex*ex+ez*ez);
        centre=dc<65535.0f?static_cast<unsigned>(dc):65535u;
    }
    const unsigned hold=duel_manual::holding_now(manager)?1u:0u;
    log_event("duel_multi_shot",(shot&0xFFu)|((dword(manager,0x4A0)&0xFFu)<<8)|(clip<<16)|(hold<<17)|(settled?1u<<18:0u),pair|(height<<16),
              centre|((gap<65535.0f?static_cast<unsigned>(gap):65535u)<<16));
}
#endif
static void follow(Obj manager) {
    step=69;const bool duel_cam=riders_step(manager);
    step=75;bar_tick(manager);
    step=77;zoom_guarded(manager);   // local290: after the riders, the bar and 37D600's camera placement
#ifndef NO_RUNTIME_LOG
    step=76;shot_step(manager);
#endif
    if(!round_logged && dword(slot_of(manager,0),0x10)==4) {   // first round start (state 6 sets action 4 on both sides)
        round_logged=true;log_side("duel_multi_round",manager,0);log_side("duel_multi_round",manager,1);
        step=66;cam_probe(2,manager,0);
    }
    if(duel_cam && ++cam_frames%30==0 && cam_samples<80){step=67;cam_probe(3,manager,++cam_samples);}
}
static void after(Obj manager) {
    if(!manager)return;
    if(broken) {bar_release(9);return;}
    if(seen_serial!=scene_serial) {   // local276: a new duel entered through 376020 (also back to back, +0x588 never read 0)
        seen_serial=scene_serial;
        if(alive)release_all(5);
        tried=false;wait_frames=0;
    }
    if(!dword(manager,0x588)) {if(alive){step=72;result_log(1,manager);}release_all(1);tried=false;return;}
    if(alive) {
        if(manager!=scene_manager || models()!=model_owner) {release_all(2);return;}
        follow(manager);return;
    }
    if(tried)return;
    const int side=static_cast<int>(dword(manager,0x3C));
    if(side<0 || side>1) {tried=true;log_event("duel_multi_skip",7,static_cast<unsigned>(side),scene_serial);return;}   // no presentation
    // local277: 37D040 leaves the slot person +0xB0 and model +0x1B0 of the last scene in place, and state 1 (37A170 -> 37CBD0)
    // rebuilds them, so wait for state 2.
    const unsigned state=dword(manager,0x4A0);
    const bool m0=pointer(slot_of(manager,0),kModel)!=nullptr,m1=pointer(slot_of(manager,1),kModel)!=nullptr;
    if(state<2 || !m0 || !m1) {
        if(++wait_frames==600)log_event("duel_multi_skip",8,(m0?1u:0u)|(m1?2u:0u)|(state<<8),scene_serial);
        return;
    }
    tried=true;
    const unsigned ids=(id_of(pointer(slot_of(manager,0),0xB0))<<16)|id_of(pointer(slot_of(manager,1),0xB0));
    if(entry_ids && ids!=entry_ids) {log_event("duel_multi_skip",9,ids,entry_ids);return;}   // slots are not this scene's duelists
    create(manager);
}
static void entry_log(Obj manager,Obj a,Obj b) {   // army serial | (side + 1) << 16, value ids a << 16 | b, hex state +0x4A0
    __try {
        log_event("duel_multi_entry",(scene_serial&0xFFFF)|((dword(manager,0x3C)+1u)&0xFF)<<16,(id_of(a)<<16)|id_of(b),dword(manager,0x4A0));
        result_log(0,manager);   // the outcome 376020 computed (3747A0), compared with the scene end
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {log_event("duel_multi_exception",90,0,0);}
}
static void guarded(Obj manager) {
    __try {after(manager);}
    __except(EXCEPTION_EXECUTE_HANDLER) {broken=true;log_event("duel_multi_exception",step,alive?1u:0u,0);}
}
static void guarded_release() {
    __try {release_all(4);}
    __except(EXCEPTION_EXECUTE_HANDLER) {broken=true;alive=false;log_event("duel_multi_exception",step,2,0);}
}
// ---- effects (378890, state 0x15) ----
static void __fastcall apply_hook(Obj persons,int *block) { COST_SCOPE("duel_multi::apply");
    duel_manual::note_effects(block);
    if(duel_manual::apply(persons,block))return;   // a settled relay scene: the final pair
    fn<void(__fastcall *)(Obj,int *)>(kApply)(persons,block);
}
static void __fastcall record_hook(Obj a,Obj b,unsigned kind,int fate) { COST_SCOPE("duel_multi::record");
    if(duel_manual::record(a,b,kind,fate))return;   // a settled relay scene: the final pair
    fn<void(__fastcall *)(Obj,Obj,unsigned,int)>(kRecord)(a,b,kind,fate);
}
// The relay settlement (duel_manual::force_damage) adjusts the damage columns of its own 3747A0 call right after 37BC70 (374A22).
static unsigned long long __fastcall damage_hook(int *block,void *a,void *b,unsigned long long strong_row,unsigned long long weak_row,Obj strong,Obj weak) {
    COST_SCOPE("duel_multi::damage");
    const unsigned long long result=fn<unsigned long long(__fastcall *)(int *,void *,void *,unsigned long long,unsigned long long,Obj,Obj)>(kDamage)(
        block,a,b,strong_row,weak_row,strong,weak);
    if(duel_manual::force_damage(block))return result;   // the manual gate's settlement test (its own block only)
    return result;
}
// The relay settlement (duel_manual::force_onehit) skips the 일격 roll 376CC0 (374901) inside its own 3747A0 call.
static int __fastcall onehit_hook(Obj strong,Obj weak) { COST_SCOPE("duel_multi::onehit");
    if(duel_manual::force_onehit())return 0;   // the manual gate's settlement test (same thread, inside its call only)
    return fn<int(__fastcall *)(Obj,Obj)>(kOneHit)(strong,weak);
}
// local308: 37AD40's loser injury 27F250(loser, 3, winner, 0) at 37B016 (original: HP < 75). Inside the relay's own result calls
// (duel_manual apply, same thread) a loser above duel_manual kLoseInjury is left out; everywhere else the original call.
static void *__fastcall injury_hook(Obj person,int kind,Obj other,int flag) { COST_SCOPE("duel_multi::injury");
    if(duel_manual::injury_skip(person))return nullptr;   // the call site ignores the result
    return fn<void *(__fastcall *)(Obj,int,Obj,int)>(kInjury)(person,kind,other,flag);
}
static char __fastcall pool_init(Obj pool,unsigned long long count,Obj allocator,unsigned long long flag) { COST_SCOPE("duel_multi::pool_init");
    const unsigned asked=static_cast<unsigned>(count);
    if(count<6)count=6;   // two originals + up to four of ours
    log_event("duel_multi_pool",asked,static_cast<unsigned>(count),++pool_calls);
    return fn<char(__fastcall *)(Obj,unsigned long long,Obj,unsigned long long)>(kPoolInit)(pool,count,allocator,flag);
}
static void __fastcall tick(Obj manager,unsigned long long a2) { COST_SCOPE("duel_multi::tick");
    fn<void(__fastcall *)(Obj,unsigned long long)>(kTick)(manager,a2);
    duel_manual::frame(manager);   // first: a relay scene is decided before create() picks its extras
    // Only a relay scene runs this module now (its extras, until they are released).
    if(alive || duel_manual::scene_on(manager))guarded(manager);
}
static void __fastcall close_day(Obj manager) { COST_SCOPE("duel_multi::close_day");
    duel_manual::scene_close(manager);
    guarded_release();
    fn<void(__fastcall *)(Obj)>(kClose)(manager);
}
// local276: the only scene entry (2CE966 CALL 376020(manager, 0x40 B argument: +0 / +8 the two duelists)).
static void __fastcall entry(Obj manager,Obj arg) { COST_SCOPE("duel_multi::entry");
    Obj a=arg?pointer(arg,0):nullptr,b=arg?pointer(arg,8):nullptr;
    fn<void(__fastcall *)(Obj,Obj)>(kEntry)(manager,arg);
    ++scene_serial;
    entry_ids=(id_of(a)<<16)|id_of(b);
    duel_manual::entry(manager);   // the block and both +0xF1 of the original 3747A0 call, before anything else touches them
    entry_log(manager,a,b);
}
// The round-end CALLs 37788D / 377DC9 / 3785EC (37D1C0(manager)). A boundary replaces the original call; otherwise the original
// runs unchanged (plain 1:1 duels and every relay round the manual gate does not claim).
static void __fastcall round_end(Obj manager) { COST_SCOPE("duel_multi::round_end");
    if(duel_manual::round_end(manager))return;   // a manual-gate block boundary
    fn<void(__fastcall *)(Obj)>(kRoundEnd)(manager);
}
static CallLink links[4],round_links[3],effect_links[3],damage_link,onehit_link,injury_link;
// duel_multi_installed: army motion | round hooks << 1 | effect hooks << 2 | bottom bar row << 3 | half damage << 4 | no 일격 << 5 |
// loser injury << 6 (local308; duel_multi_injury_rejected(site is E8 to 27F250, chain hooked)),
// value kBarY. Rejections (also duel_multi_damage_rejected / duel_multi_onehit_rejected(site is E8 to the target)):
// duel_multi_round_rejected(site mask, resolved, published), duel_multi_chain_rejected(site mask, resolved, published),
// duel_multi_bar_rejected(site is E8 to 8269A0, Layout handle).
bool install() {
    const unsigned mask=(call_to(kPoolCall,kPoolInit)?1u:0u)|(call_to(kTickCall,kTick)?2u:0u)|(call_to(kCloseCall,kClose)?4u:0u)|
                        (call_to(kEntryCall,kEntry)?8u:0u);
    const bool resolved=kSlotInit && kAction && kTiming && kModelSet && kRelease && GA(GA_21462A8) && GA(GA_22BAB0) && GA(GA_22E690);
    if(mask!=15 || !resolved) {log_event("duel_multi_guard_rejected",mask,resolved?1u:0u);return false;}
    Obj relay=allocate_near(image+kTickCall);if(!relay)return false;
    absolute_jump(relay,reinterpret_cast<void *>(&pool_init));
    absolute_jump(relay+32,reinterpret_cast<void *>(&tick));
    absolute_jump(relay+64,reinterpret_cast<void *>(&close_day));
    absolute_jump(relay+96,reinterpret_cast<void *>(&entry));
    absolute_jump(relay+128,reinterpret_cast<void *>(&round_end));     // shared by the three round-end CALLs
    absolute_jump(relay+160,reinterpret_cast<void *>(&apply_hook));    // local283 3788CF
    absolute_jump(relay+192,reinterpret_cast<void *>(&record_hook));   // local283 37892B / 378948
    absolute_jump(relay+224,reinterpret_cast<void *>(&damage_hook));   // local284 374A22
    absolute_jump(relay+256,reinterpret_cast<void *>(&onehit_hook));   // local285 374901
    absolute_jump(relay+288,reinterpret_cast<void *>(&injury_hook));   // local308 37B016
    DWORD old;
    if(!call_prepare(links[0],kPoolCall,relay) || !call_prepare(links[1],kTickCall,relay+32) ||
       !call_prepare(links[2],kCloseCall,relay+64) || !call_prepare(links[3],kEntryCall,relay+96) ||
       !VirtualProtect(relay,4096,PAGE_EXECUTE_READ,&old) ||
       !FlushInstructionCache(GetCurrentProcess(),relay,320)) {VirtualFree(relay,0,MEM_RELEASE);log_event("duel_multi_prepare_rejected");return false;}
    for(unsigned i=0;i<4;++i)if(!call_publish(links[i])) {log_event("duel_multi_publish_rejected",i);while(i)call_restore(links[--i]);return false;}
    // Round hooks: optional. Without them every duel stays original (duel_manual_begin why 3).
    unsigned published=0;
    const unsigned round_mask=(call_to(kRoundCallB,kRoundEnd)?1u:0u)|(call_to(kRoundCallE,kRoundEnd)?2u:0u)|(call_to(kRoundCallD,kRoundEnd)?4u:0u);
    const bool round_resolved=kSetState && kFaceSet && kTextSet && kName && kStat && kNumber && kNode;
    if(round_mask==7 && round_resolved) {
        const bool prepared=call_prepare(round_links[0],kRoundCallB,relay+128) && call_prepare(round_links[1],kRoundCallE,relay+128) &&
                            call_prepare(round_links[2],kRoundCallD,relay+128);
        if(prepared) {
            while(published<3 && call_publish(round_links[published]))++published;
            if(published<3)for(unsigned i=published;i;)call_restore(round_links[--i]);
        }
    }
    if(published!=3)log_event("duel_multi_round_rejected",round_mask,round_resolved?1u:0u,published);
    // local283 effect + record hooks: all three or none; the relay settlement needs them with the round hooks.
    unsigned effects=0;
    const unsigned effect_mask=(call_to(kApplyCall,kApply)?1u:0u)|(call_to(kRecordCallA,kRecord)?2u:0u)|(call_to(kRecordCallB,kRecord)?4u:0u);
    const bool effect_resolved=kCalc && kApply && kRecord && kPage && kApp && kGaugeFill;
    if(published==3 && effect_mask==7 && effect_resolved) {
        const bool prepared=call_prepare(effect_links[0],kApplyCall,relay+160) && call_prepare(effect_links[1],kRecordCallA,relay+192) &&
                            call_prepare(effect_links[2],kRecordCallB,relay+192);
        if(prepared) {
            while(effects<3 && call_publish(effect_links[effects]))++effects;
            if(effects<3)for(unsigned i=effects;i;)call_restore(effect_links[--i]);
        }
    }
    hooks_all=published==3 && effects==3;
    if(!hooks_all)log_event("duel_multi_chain_rejected",effect_mask,effect_resolved?1u:0u,effects);
    // local284 damage hook: the relay settlement forces its 3747A0 damage columns here (duel_manual::force_damage).
    if(hooks_all && kDamage && call_to(kDamageCall,kDamage))
        damage_hooked=call_prepare(damage_link,kDamageCall,relay+224) && call_publish(damage_link);
    if(!damage_hooked)log_event("duel_multi_damage_rejected",call_to(kDamageCall,kDamage)?1u:0u,hooks_all?1u:0u);
    // local285 일격 hook: the relay settlement skips the 일격 roll here (duel_manual::force_onehit).
    if(hooks_all && kOneHit && call_to(kOneHitCall,kOneHit))
        onehit_hooked=call_prepare(onehit_link,kOneHitCall,relay+256) && call_publish(onehit_link);
    if(!onehit_hooked)log_event("duel_multi_onehit_rejected",call_to(kOneHitCall,kOneHit)?1u:0u,hooks_all?1u:0u);
    // local308 loser-injury hook (optional): without it the relay's losers keep the original HP < 75 roll.
    if(hooks_all && kInjury && call_to(kInjuryCall,kInjury))
        injury_hooked=call_prepare(injury_link,kInjuryCall,relay+288) && call_publish(injury_link);
    if(!injury_hooked)log_event("duel_multi_injury_rejected",call_to(kInjuryCall,kInjury)?1u:0u,hooks_all?1u:0u);
    // Bottom bar: the UI owner row 82AF0F (Layout manager). Optional; without it the relay shows the top gauge only.
    int handle=0;
    if(hooks_all && call_to(kGaugeHpCall,kGaugeFill))handle=layout_client::call(kGaugeHpCall,reinterpret_cast<void *>(&gauge_hp),reinterpret_cast<void **>(&next_gauge_hp));
    bar_registered=handle>0;
    if(!bar_registered)log_event("duel_multi_bar_rejected",call_to(kGaugeHpCall,kGaugeFill)?1u:0u,static_cast<unsigned>(handle));
    log_event("duel_multi_installed",1u|(published==3?2u:0u)|(effects==3?4u:0u)|(bar_registered?8u:0u)|
              (damage_hooked?16u:0u)|(onehit_hooked?32u:0u)|(injury_hooked?64u:0u),
              static_cast<unsigned>(kBarY),0);
    return true;
}
}
