#pragma once
#include <cmath>
#include <cstdint>
// Manual-duel gate script ([extensions] duel_relay, local298-308 [diagnostics] duel_manual_test; docs/MANUAL_DUEL.md sections 3 and 7). Pure: no game memory, no Windows.
// The original duel block (376020 -> 3747A0, 0xC0 B = 48 ints): [0] outcome for the player, [1] ?, [2] 일격, [3] fate; per arg row
// (stride 0x16 from [4]) +0 win/loss/draw (0/1/2), +2 start HP, +3 final HP, +4 HP before round 1, +5..+9 HP after rounds 1-5,
// +0xB..+0xF damage dealt in rounds 1-5 (HP after round r = HP before - the OTHER row's damage of round r), +0x11..+0x15 round codes
// (0 normal, 1 회심, 2 막음, 3 반격; 37A820 picks state 0xE / 0xD / 0xB from them).
// The gate replays that block as `segments` blocks of five rounds each (the scene plays them back to back, a pause before each).
// Each block is a copy of the original with only the HP, damage and code columns rewritten, so the header (outcome, 일격, fate), win /
// loss and start HP stay the original's and the last block ends on the original final HP of both rows (no KO the original did not
// have: a row reaches 0 only when the original final HP is 0, and only in the very last round).
// force_targets: the settlement test. Inside an adjusted 3747A0 call (after its damage columns, 37BC70) both rows start from their
// start HP (the 무력 gap cancelled) and all damage lands in round 1, so the call's own round loop ends on the given final HP.
namespace duel_manual_script {
constexpr int kInts=0x30,kRow=0x16,kSegMax=6,kRounds=5;
constexpr int row_base(int row) {return 4+row*kRow;}
enum Kind:int {Normal=0,Crit=1,Block=2,Counter=3};
// Global round g (1-based): normal, 회심 by the leading row, 막음 by the other row, 반격 by the other row, normal. The last round
// of every block is normal, so both rows take damage in the final round and reach their final HP there.
constexpr int kind_of(int g) {return (g-1)%5==1?Crit:(g-1)%5==2?Block:(g-1)%5==3?Counter:Normal;}
// Damage weight row `taker` gets in round g (lead = the winner row, row 0 for a draw).
constexpr int weight(int g,int taker,int lead) {
    return kind_of(g)==Normal?1:kind_of(g)==Crit?(taker!=lead?2:0):kind_of(g)==Counter?(taker==lead?2:0):0;
}
// Code row `row` shows in round g.
constexpr int code_of(int g,int row,int lead) {
    return kind_of(g)==Crit?(row==lead?1:0):kind_of(g)==Block?(row!=lead?2:0):kind_of(g)==Counter?(row!=lead?3:0):0;
}
struct Plan {
    int block[kSegMax][kInts];
    int segments=0,lead=0,winner=-1;
    int start[2]={0,0},final_hp[2]={0,0};
    unsigned why=0;   // 0 ok, 1 win/loss/draw pair invalid, 2 일격, 3 HP out of range, 4 segment count
};
inline bool valid_pair(const int *b) {
    const int w0=b[row_base(0)],w1=b[row_base(1)];
    return (w0==0 && w1==1) || (w0==1 && w1==0) || (w0==2 && w1==2);
}
// Cumulative damage row `taker` has taken after global round g (floor of the share; exactly the total at the last round).
inline int taken(int g,int taker,int lead,int total,int rounds) {
    int all=0,upto=0;
    for(int r=1;r<=rounds;++r) {const int w=weight(r,taker,lead);all+=w;if(r<=g)upto+=w;}
    return all>0?static_cast<int>(static_cast<long long>(total)*upto/all):0;
}
inline Plan build(const int *orig,int segments) {
    Plan p{};
    for(auto &b:p.block)for(int &v:b)v=0;
    if(segments<1 || segments>kSegMax) {p.why=4;return p;}
    if(!valid_pair(orig)) {p.why=1;return p;}
    if(orig[2]!=0) {p.why=2;return p;}
    for(int row=0;row<2;++row) {
        const int s=orig[row_base(row)+2],f=orig[row_base(row)+3];
        if(s<1 || s>200 || f<0 || f>s) {p.why=3;return p;}
        p.start[row]=s;p.final_hp[row]=f;
    }
    const int w0=orig[row_base(0)],w1=orig[row_base(1)];
    p.winner=w0==0?0:w1==0?1:-1;
    p.lead=p.winner>=0?p.winner:0;
    p.segments=segments;
    const int rounds=segments*kRounds;
    for(int k=0;k<segments;++k) {
        int *b=p.block[k];
        for(int i=0;i<kInts;++i)b[i]=orig[i];
        for(int row=0;row<2;++row) {
            const int rb=row_base(row),other=row_base(1-row),total=p.start[row]-p.final_hp[row];
            auto hp_after=[&](int g) {return p.start[row]-taken(g,row,p.lead,total,rounds);};
            b[rb+4]=hp_after(k*kRounds);
            for(int r=1;r<=kRounds;++r) {
                const int g=k*kRounds+r;
                b[rb+4+r]=hp_after(g);
                b[other+0xA+r]=taken(g,row,p.lead,total,rounds)-taken(g-1,row,p.lead,total,rounds);   // dealt by the other row
                b[rb+0x10+r]=code_of(g,row,p.lead);
            }
            b[rb+3]=hp_after(k*kRounds+kRounds);
        }
    }
    return p;
}
// A row whose target is 0 takes its loss in round 2 (after the other row's in round 1), so a KO never cuts the other row's loss
// short, whichever order the round loop applies the two hits in.
inline void force_targets(int *block,const int target[2]) {
    for(int row=0;row<2;++row) {
        const int rb=row_base(row),other=row_base(1-row);
        block[rb+4]=block[rb+2];
        int loss=block[rb+2]-target[row];
        if(loss<0)loss=0;
        for(int r=1;r<=kRounds;++r)block[other+0xA+r]=0;
        block[other+0xA+(target[row]==0?2:1)]=loss;
    }
}

// ---- relay commands (local295 list; local305 flow) ----
// Commands are the rules engine's (duel_s11_rules.h): 0..3 stances (공격 / 방어 / 투지 / 일발 중시), 4 필살기, 5 기합, 6 견수, 7 급소,
// 8 무쌍, 9 교대, 10 퇴각 (local306, engine action 3 under Config::retreat; -1 = the engine's AI plays the side). kPlanCost = the
// engine's 투지 cost of each command (kCost of command_action; the fixture checks they agree).
constexpr int kPlanCommands=11;
constexpr int kCmdRetreat=10;
constexpr int kPlanCost[kPlanCommands]={0,0,0,0,100,100,100,200,300,0,100};
constexpr bool plan_stance(int c) {return c>=0 && c<=3;}
constexpr bool plan_special(int c) {return c>=4 && c<=8;}
constexpr bool plan_retreat(int c) {return c==kCmdRetreat;}
// The side's members now (count 1..3, the fighter, HP, the engine's 참가 상태 0 fighting / 1 fought and waiting / 2 not entered).
// local301 (user 「교대하는 무장을 선택할 수 있었으면」, 「교대후보 a안」): a 교대 brings in the member asked for when it can come, else
// the engine's pick (the waiting member with the most HP, earlier slot on ties); candidates = every waiting member who already fought
// (state 1, the San11 rule) with HP left, other than the fighter.
struct SwapView {int count=1,active=0,hp[3]={0,0,0},state[3]={0,2,2};};
inline int swap_pick(const SwapView &v,int want) {
    if(want>=0 && want<v.count && want!=v.active && v.state[want]==1 && v.hp[want]>0)return want;
    int pick=-1;
    for(int i=0;i<v.count;++i)if(i!=v.active && v.state[i]==1 && v.hp[i]>0 && (pick<0 || v.hp[i]>v.hp[pick]))pick=i;
    return pick;
}
inline int swap_candidates(const SwapView &v,int out[3]) {
    int n=0;
    for(int i=0;i<v.count && i<3;++i)if(i!=v.active && v.state[i]==1 && v.hp[i]>0)out[n++]=i;
    return n;
}
// local305 relay flow (user 2026-11-23 「대기화면은 일기토 처음 시작하고나서 나오고 난입할때나오고 교체버튼 누를때나오고 필살버튼 누를때
// 나오는거야. 5합씩 멈추는게 아니라 … 1합을 2초정도 진행하면서 천천히 진행되는데 플레이어가 실시간으로 태세를 바꾸는거지 필살버튼도
// 실시간으로 누르고 … 선택 안되면 그전 입력이 들어가고 … 교체는 1회로 제한하자」, confirmed 「1A 2C 3A」): the engine plays one 합 at
// a time. Between two 합 the pair rides on for kGapMs (the gap, camera kGapDist); a full hold (camera kHoldDist, 「확정」) comes only at
// the duel's start, for a 필살 or 교대 asked for, and right before a 합 with a join. The player's input: the stance kept (every 합
// until changed), a pending 필살 or 교대 (one 합), 자동 (the engine plays the side).
constexpr unsigned kGapMs=1250;   // local307 (user 「짧은 대기가 지금 2초인가? 1.25초로 해도 될거같아」; local305-306: 2000)
// local308 (user 「짧은대기 시간을 0.5초부터 0.25초 단위로 설치기에서 드롭다운 형식으로 최대 1.5초까지」): [extensions] duel_relay_gap
// (local308-309 [diagnostics] duel_manual_gap) 1..5 = 500 / 750 / 1000 / 1250 / 1500 ms; local309 (user 「0.5초도 엄청 긴느낌?? …
// 0.25초까지 줄여볼까」) 0 = 250 ms, the v16.7 default (user 「0.25초 박자 괜찮네 이대로 배포하자」); anything else is kGapMs (never 0)
// local312 (user 「중간에 넣은 짧은대기 빼고 스피디하게 진행하는거 테스트 해보고 싶어」): choice 6 = no gap (kGapNone): the next 합 plays
// at once as under 자동, a join still gets its hold (gap_none); gap_ms keeps kGapMs for it (unused then)
constexpr unsigned kGapNone=6;
inline unsigned gap_ms(unsigned choice) {return choice<=5?250u+250u*choice:kGapMs;}
inline bool gap_none(unsigned choice) {return choice==kGapNone;}
// local312 (user 「양측 난입무장 없으면 최대 15합 난입무장이 4명이면 최대 20합 5명이면 25합 6명이면 30합 … 4명부터 +5합 해서 6명일때도
// 최대 35합까지 갈 수 있게? 낮은 확률로」, confirmed with 3 fighters = 15, 15 %, rolled at the cap): the 합 cap by the scene's fighters
// (both sides' members at the start, joiners still to come included), and the extension (kRoundExt 합 at kRoundExtPct %) from 4.
constexpr int kRoundExt=5,kRoundExtPct=15;
inline int relay_round_cap(int fighters) {return fighters<=3?15:fighters>=6?30:15+5*(fighters-3);}
inline int relay_round_ext(int fighters) {return fighters>=4?kRoundExt:0;}
// local313 (user 「최대 두명까지만 물어봐야해 2헥스 이내에 무력 높은 순으로 물어보고 거절하면 그쪽은 이제 최대3명에서 최대2명으로 줄어드는거야」,
// 「확률은 설치기에서 조절할 수 있게하자」): each of a side's two strongest extras is asked once at step * 10 % ([extensions]
// duel_relay_join 0..10, default 5); a refusal leaves the place empty, nobody else is asked. xorshift32, the state never 0.
constexpr unsigned kJoinStepDefault=5,kJoinSteps=10;
inline std::uint32_t join_seed(std::uint64_t tick,unsigned serial) {const std::uint32_t s=static_cast<std::uint32_t>(tick^(tick>>32))^(serial*0x9E3779B9u)^0xA5A5A5A5u;return s?s:1u;}
inline std::uint32_t join_next(std::uint32_t &st) {if(!st)st=1u;st^=st<<13;st^=st>>17;st^=st<<5;return st;}
inline bool join_accepts(std::uint32_t &st,unsigned step) {return step>=kJoinSteps || (step>0 && join_next(st)%100u<step*10u);}
constexpr float kGapDist=1200.0f;
// local306 HoldRetreat: 퇴각 pressed (its hold offers 퇴각 picked and 「확정」).
enum HoldKind:int {HoldGap=0,HoldStart=1,HoldSpecial=2,HoldSwap=3,HoldJoin=4,HoldRetreat=5};
struct FlowInput {int stance=2,pend=-1,who=-1;bool autoplay=false;};
// The engine command of the next 합 (who: the 교대 member, -1 none): 자동 -> -1 (the AI); a pending 필살 / 교대 / 퇴각; else the stance.
inline int flow_command(const FlowInput &in,int &who) {
    who=-1;
    if(in.autoplay)return -1;
    if(plan_special(in.pend) || plan_retreat(in.pend))return in.pend;
    if(in.pend==9){who=in.who;return 9;}
    return plan_stance(in.stance)?in.stance:2;
}
// A command is looked ahead for a join when the side is not the AI and asks for no 필살 or 퇴각 (a special 합 has no joins); a join
// hold's own 합 is not looked at again. The look-ahead is exact for a stance or a 교대: step() decides the AI's special roll and both
// joins before it reads either (the fixture checks it); the AI's command rolls first, so 자동 is never looked ahead.
inline bool flow_peeks(int cmd,bool join_hold) {return !join_hold && cmd>=0 && !plan_special(cmd) && !plan_retreat(cmd);}
// The 합 of a join hold keeps the class looked ahead with: a stance takes the stance chosen in the hold, a 교대 stays a 교대.
inline int join_command(int peeked,int stance) {return plan_stance(peeked)?(plan_stance(stance)?stance:peeked):peeked;}
// The hold at the next gap: the duel's start, else a 필살 / 교대 / 퇴각 asked for during the 합 (not while 자동 plays), else the ride.
inline int next_hold(bool first,int want,bool autoplay) {
    if(first)return HoldStart;
    return !autoplay && (want==HoldSpecial || want==HoldSwap || want==HoldRetreat)?want:HoldGap;
}
// Strip buttons. 필살: 0 off, 1 on (a 필살 the engine allows now), 2 pending. 교대: 0 no member, 1 on, 2 pending, 3 the side's
// 교대 used (local305 Config::swap_limit 1).
inline int special_button(bool any_legal,int pend) {return plan_special(pend)?2:any_legal?1:0;}
inline int swap_button(bool left,int cands,int pend) {return pend==9?2:!left?3:cands>0?1:0;}
// local306 (user 「교대 횟수 다쓰면 퇴각으로 변하기」, 「2A」): the 교대 place is 퇴각 while no 교대 can be made now (the side's 교대
// used, or no member to bring in) and none is pending; a 퇴각 pending keeps it. 0 the 교대 plate (swap_button), 1 퇴각 on, 2 퇴각
// pending (or its hold), 3 퇴각 off (투지 below its cost, San11's 100: 「4A」).
inline int retreat_button(int swap_state,bool legal,int pend) {
    if(plan_retreat(pend))return 2;
    if(swap_state==1 || swap_state==2)return 0;
    return legal?1:3;
}
// local306 status line under the strip: the critical's charge (San11 0x5072A0: a stance other than 일격 중시 needs 6 cycles without
// a critical, a stance change or a switch; 일격 중시 always rolls one) as 0-5 of 6, 6 = ready; the guard timers 0-15.
inline int crit_view(int stance,int charge) {return stance==3 || charge>=6?6:charge<0?0:charge;}
inline int guard_view(int guard) {return guard<0?0:guard>15?15:guard;}
// The first pair's entry cut (local301, user 「진입 무력차 데미지는 첫무장 대결에 한해서 … 데미지 약하게」): 3747A0 starts the weaker of
// the two duelists at its HP minus the 무력 gap (231770(무장,1,0xFFF) each); the relay takes kEntryDen-th of the gap (rounded up),
// only for the two first duelists, and never ends the duel before it starts (at least 1 HP left).
constexpr int kEntryDen=3;
inline int entry_cut(int strong,int weak) {const int d=strong-weak;return d<=0?0:(d+kEntryDen-1)/kEntryDen;}
inline int entry_hp(int hp,int cut) {if(cut<=0 || hp<=1)return hp;const int v=hp-cut;return v<1?1:v;}
// San14 duel trait bonus (374440, local301): effect 0x1E value plus, for an effect 0x1F holder, its age step (60-64 3, 65-69 4, 70-79
// 6, 80-89 8, 90 on 10; under 60 none), in percent of the round damage base.
inline int age_step(int age) {return age>=90?10:age>=80?8:age>=70?6:age>=65?4:age>=60?3:0;}
inline int trait_power(int e1e,bool has1f,int age) {const int v=e1e+(has1f?age_step(age):0);return v<0?0:v>100?100:v;}
// Round codes of a 합 with a special (engine actions 0 필살기, 1 기합, 2 견수, 4 급소, 5 무쌍; -1 none). The engine gives a special that
// dealt damage 회심 (1) and a 합 nobody attacked 막음 on both sides (2). Presentation (user 2026-11-22 「3. 4 둘다 A」): 급소 that dealt
// damage plays as 반격 (3: 37A820 picks state 0xE, the code-3 side strikes) with 0 on the other side; 견수 shows 막음 on its own side
// only (the other side's swing is stopped). Contract kept: a side with 2 or 3 means the other side dealt 0 (one side acts per special).
inline void special_codes(const int special[2],const int damage[2],int code[2]) {
    for(int s=0;s<2;++s) {
        const int o=1-s;
        if(special[s]==4 && damage[s]>0 && damage[o]==0){code[s]=3;code[o]=0;}
        else if(special[s]==2 && damage[o]==0){code[s]=2;code[o]=0;}
    }
}
// 무쌍 (engine action 5) as a combo of 회심 scenes (「4 A」): the receiver's HP before -> after the 합 in n hits (3 from 9 damage on,
// 2 from 4, else 1), even steps. out[0] = before, out[n] = after, every step strictly lower, every HP before the last at least 1 (0 on
// screen ends the duel). Returns n.
inline int musou_split(int before,int after,int out[4]) {
    out[0]=before;
    const int d=before-after;
    int n=d>=9?3:d>=4?2:1;
    for(;n>1;--n) {
        bool good=true;
        for(int i=1;i<n;++i) {
            out[i]=before-(d*i+n/2)/n;
            if(out[i]<1 || out[i]>=out[i-1] || out[i]<=after)good=false;
        }
        if(good)break;
    }
    out[n]=after;
    return n;
}
// 투지 gauge (user: 「투지 100 투지 200 이런거는 … 게임내부 값 … 빼자」, 「투지가 차오르면(UI로 표시해줘야겠지만)」): 15 cells in three
// groups of five (one group = 100, the cost unit of the 필살 commands): ■ held, ▣ held but promised to the 필살 picked or pending,
// □ empty (only while the yellow bars are off).
inline void morale_cells(int morale,int reserved,wchar_t *out,int cap) {
    if(!out || cap<1)return;
    int full=morale/20;if(full<0)full=0;if(full>15)full=15;
    int res=reserved/20;if(res<0)res=0;if(res>full)res=full;
    int w=0;
    for(int i=0;i<15 && w+1<cap;++i) {
        if(i && i%5==0 && w+1<cap)out[w++]=L' ';
        if(w+1>=cap)break;
        out[w++]=i<full-res?L'■':i<full?L'▣':L'□';
    }
    out[w]=0;
}
// ---- relay hold camera (local300) ----
// The stage camera's view frame follows the rotation q (x, y, z, w) it is given: Rot(q) z = forward, Rot(q) y = up (Rot(q) v = q v q*).
// local299 read four written rotations back in the same tick and every one gave exactly that (MANUAL_DUEL 10-8). The getter's rotation
// of the original camera is NOT its view (identity while the view looked 30 degrees down), so nothing is built on it: a wanted view is
// written as the absolute rotation of its frame. Frames are row-major 3 x 3 with the columns side = up x forward, up, forward.
inline bool cam_unit(float *v,int n) {
    float s=0.0f;for(int i=0;i<n;++i)s+=v[i]*v[i];
    s=std::sqrt(s);
    if(!(s>1e-6f) || s!=s)return false;
    for(int i=0;i<n;++i)v[i]/=s;
    return true;
}
inline bool cam_frame(const float fwd[3],const float up[3],float m[9]) {   // up made square to forward
    float f[3]={fwd[0],fwd[1],fwd[2]};
    if(!cam_unit(f,3))return false;
    const float d=up[0]*f[0]+up[1]*f[1]+up[2]*f[2];
    float u[3]={up[0]-f[0]*d,up[1]-f[1]*d,up[2]-f[2]*d};
    if(!cam_unit(u,3))return false;
    const float s[3]={u[1]*f[2]-u[2]*f[1],u[2]*f[0]-u[0]*f[2],u[0]*f[1]-u[1]*f[0]};
    for(int r=0;r<3;++r){m[r*3]=s[r];m[r*3+1]=u[r];m[r*3+2]=f[r];}
    return true;
}
inline void cam_quat_mat(const float q[4],float m[9]) {   // Rot(q) for a unit q
    const float x=q[0],y=q[1],z=q[2],w=q[3];
    m[0]=1.0f-2.0f*(y*y+z*z);m[1]=2.0f*(x*y-z*w);m[2]=2.0f*(x*z+y*w);
    m[3]=2.0f*(x*y+z*w);m[4]=1.0f-2.0f*(x*x+z*z);m[5]=2.0f*(y*z-x*w);
    m[6]=2.0f*(x*z-y*w);m[7]=2.0f*(y*z+x*w);m[8]=1.0f-2.0f*(x*x+y*y);
}
inline void cam_mat_quat(const float a[9],float q[4]) {   // the unit q with Rot(q) = a (a a rotation)
    const float t=a[0]+a[4]+a[8];
    if(t>0.0f) {
        const float s=std::sqrt(t+1.0f)*2.0f;
        q[3]=0.25f*s;q[0]=(a[7]-a[5])/s;q[1]=(a[2]-a[6])/s;q[2]=(a[3]-a[1])/s;
    } else if(a[0]>a[4] && a[0]>a[8]) {
        const float s=std::sqrt(1.0f+a[0]-a[4]-a[8])*2.0f;
        q[3]=(a[7]-a[5])/s;q[0]=0.25f*s;q[1]=(a[1]+a[3])/s;q[2]=(a[2]+a[6])/s;
    } else if(a[4]>a[8]) {
        const float s=std::sqrt(1.0f+a[4]-a[0]-a[8])*2.0f;
        q[3]=(a[2]-a[6])/s;q[0]=(a[1]+a[3])/s;q[1]=0.25f*s;q[2]=(a[5]+a[7])/s;
    } else {
        const float s=std::sqrt(1.0f+a[8]-a[0]-a[4])*2.0f;
        q[3]=(a[3]-a[1])/s;q[0]=(a[2]+a[6])/s;q[1]=(a[5]+a[7])/s;q[2]=0.25f*s;
    }
    cam_unit(q,4);
}
// The rotation to store for a wanted forward / up (false: degenerate).
inline bool cam_abs(const float fwd[3],const float up[3],float q[4]) {
    float m[9];
    if(!cam_frame(fwd,up,m))return false;
    cam_mat_quat(m,q);
    return q[0]==q[0] && q[1]==q[1] && q[2]==q[2] && q[3]==q[3];
}
// The hold camera's distance. local301 (user 「지금멀리하는 수치의 45%정도」): fixed kHoldDist, 45 % of local300's 7776 (= 1.8 x the
// original's 4320 at that scene's first hold; local299's scene measured 1572, so a multiple of the original swings by scene); local303
// (user 「대기거리 2500으로 바꾸자」): 2500. The
// original's eye-to-pair distance at the scene's first hold is still measured and logged; outside kOrigMin..kOrigMax it reads as no
// measure (fallback). The eye of the follow camera (original rotation kept) for the pair's middle c and the original's view forward f:
// c - f * distance.
constexpr float kHoldDist=2500.0f,kOrigMin=400.0f,kOrigMax=6000.0f;
inline float hold_distance(float measured,bool &fallback) {
    fallback=!(measured>=kOrigMin && measured<=kOrigMax);
    return kHoldDist;
}
// local305: the gap's ride is filmed from kGapDist (user 「1합하고 근거리에서 달리기 유지하면서」), every full hold from kHoldDist.
inline float flow_distance(int kind) {return kind==HoldGap?kGapDist:kHoldDist;}
inline void follow_eye(const float c[3],const float f[3],float dist,float eye[3]) {
    for(int i=0;i<3;++i)eye[i]=c[i]-f[i]*dist;
}
}
