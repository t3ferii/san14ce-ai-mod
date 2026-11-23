// duel_manual_script.h fixture ([diagnostics] duel_manual_test, docs/MANUAL_DUEL.md section 7). Pure: no game image, no game memory.
// Mock checks of the scripted blocks and of the settlement-test columns, not in-game verification.
#include "../src/duel_manual_script.h"
#include "../src/duel_s11_rules.h"
#include <cstdio>
#include <cwchar>
#include <initializer_list>
static int checks=0,failures=0;
static void ok(bool v,const char *what) {++checks;if(!v){++failures;std::printf("FAIL %s\n",what);}}
using namespace duel_manual_script;
// An original-shaped block: header, rows with W/L/D, start / final HP, a 무력 gap in row +4 and some filler in the unknown fields.
static void make_orig(int *b,int w0,int w1,int s0,int f0,int s1,int f1,int fate) {
    for(int i=0;i<kInts;++i)b[i]=1000+i;
    b[0]=w0;b[1]=7;b[2]=0;b[3]=fate;
    const int w[2]={w0,w1},s[2]={s0,s1},f[2]={f0,f1};
    for(int row=0;row<2;++row) {
        const int rb=row_base(row);
        b[rb]=w[row];b[rb+2]=s[row];b[rb+3]=f[row];b[rb+4]=s[row]-3;
        for(int r=1;r<=5;++r){b[rb+4+r]=s[row]-3*r;b[rb+0xA+r]=9;b[rb+0x10+r]=r&3;}
    }
}
// 3747A0's round loop as the decompile reads it: HP after round r = HP before - the other row's damage of round r, a KO stops.
// both_first = both hits of a round land before the KO check; otherwise row 0 first, then row 1 (the order the forcing must survive).
static void play_loop(const int *b,int out_final[2],bool both_first,bool row1_first) {
    int hp[2]={b[row_base(0)+4],b[row_base(1)+4]};
    for(int r=1;r<=kRounds;++r) {
        const int d0=b[row_base(1)+0xA+r],d1=b[row_base(0)+0xA+r];   // damage taken by row 0 / row 1
        if(both_first) {hp[0]-=d0;hp[1]-=d1;}
        else {
            const int order[2]={row1_first?1:0,row1_first?0:1};
            bool ko=false;
            for(int i:order){hp[i]-=(i==0?d0:d1);if(hp[i]<=0){hp[i]=0;ko=true;break;}}
            if(ko)break;
        }
        if(hp[0]<0)hp[0]=0;
        if(hp[1]<0)hp[1]=0;
        if(hp[0]==0 || hp[1]==0)break;
    }
    out_final[0]=hp[0];out_final[1]=hp[1];
}
static void check_plan(const int *orig,int segments,const char *name) {
    char what[160];
    const Plan p=build(orig,segments);
    std::snprintf(what,sizeof what,"%s: plan built",name);ok(p.why==0 && p.segments==segments,what);
    if(p.why)return;
    const int rounds=segments*kRounds;
    bool header=true,cont=true,mono=true,early_ko=false,blocks_zero=true,damage_ok=true,codes_ok=true,hp_ok=true,small=true;
    for(int row=0;row<2;++row) {
        const int rb=row_base(row),other=row_base(1-row);
        int prev=orig[rb+2];
        for(int k=0;k<segments;++k) {
            const int *b=p.block[k];
            for(int i:{0,1,2,3})if(b[i]!=orig[i])header=false;
            if(b[rb]!=orig[rb] || b[rb+2]!=orig[rb+2] || b[rb+1]!=orig[rb+1] || b[rb+0xA]!=orig[rb+0xA] || b[rb+0x10]!=orig[rb+0x10])header=false;
            if(b[rb+4]!=prev)cont=false;
            for(int r=1;r<=kRounds;++r) {
                const int g=k*kRounds+r,before=b[rb+4+r-1],after=b[rb+4+r],dealt=b[other+0xA+r];
                if(after>before)mono=false;
                if(before-after!=dealt)damage_ok=false;
                if(dealt<0)damage_ok=false;
                if(segments>=4 && dealt>12)small=false;   // San11-sized hits over 20+ rounds (at most 100 HP spread, crit / counter double)
                if(after<=0 && g<rounds)early_ko=true;
                if(kind_of(g)==Block && dealt!=0)blocks_zero=false;
                const int c=b[rb+0x10+r];
                if(c<0 || c>3)codes_ok=false;
                if(kind_of(g)==Normal && c!=0)codes_ok=false;
            }
            if(b[rb+3]!=b[rb+4+kRounds])hp_ok=false;
            prev=b[rb+4+kRounds];
        }
        if(p.block[segments-1][rb+3]!=orig[rb+3])hp_ok=false;
    }
    std::snprintf(what,sizeof what,"%s: header, W/L, start HP and unknown fields stay original",name);ok(header,what);
    std::snprintf(what,sizeof what,"%s: each block starts where the last one ended",name);ok(cont,what);
    std::snprintf(what,sizeof what,"%s: HP never rises",name);ok(mono,what);
    std::snprintf(what,sizeof what,"%s: HP after a round = before - the other row's damage",name);ok(damage_ok,what);
    std::snprintf(what,sizeof what,"%s: no KO before the last round",name);ok(!early_ko,what);
    std::snprintf(what,sizeof what,"%s: blocked rounds deal nothing",name);ok(blocks_zero,what);
    std::snprintf(what,sizeof what,"%s: codes 0..3, normal rounds 0",name);ok(codes_ok,what);
    std::snprintf(what,sizeof what,"%s: block end HP = row +3, last = original final HP",name);ok(hp_ok,what);
    std::snprintf(what,sizeof what,"%s: per-round damage stays small",name);ok(small,what);
    // The round states 37A820 would pick: some 회심 (0xD), some 반격 (0xE), some plain (0xB) over the whole script.
    int st[3]={0,0,0};
    for(int k=0;k<segments;++k)for(int r=1;r<=kRounds;++r) {
        const int a=p.block[k][row_base(0)+0x10+r],b=p.block[k][row_base(1)+0x10+r];
        st[(a==3 || b==3)?2:(a==1 || b==1)?1:0]++;
    }
    std::snprintf(what,sizeof what,"%s: plain, 회심 and 반격 rounds all appear",name);ok(st[0]>0 && st[1]>0 && st[2]>0,what);
    // Settlement columns: the forced original block reaches the original final HP under every hit order.
    int forced[0x30];
    for(int i=0;i<kInts;++i)forced[i]=orig[i];
    const int target[2]={orig[row_base(0)+3],orig[row_base(1)+3]};
    force_targets(forced,target);
    bool reach=true;
    for(int mode=0;mode<3;++mode) {
        int fin[2];play_loop(forced,fin,mode==0,mode==2);
        if(fin[0]!=target[0] || fin[1]!=target[1])reach=false;
    }
    std::snprintf(what,sizeof what,"%s: forced columns end on the target HP (any hit order)",name);ok(reach,what);
    ok(forced[row_base(0)+4]==orig[row_base(0)+2] && forced[row_base(1)+4]==orig[row_base(1)+2],"forced: 무력 gap cancelled (row +4 = start HP)");
}
// local305 relay flow: the command of the next 합 from the player's input, the look-ahead and the join hold's command, the next hold,
// the strip buttons, the 투지 gating against the engine's own legality, special round codes and the 무쌍 hit split.
static void check_plan_list() {
    namespace s11=duel_s11;
    bool cost_ok=true;
    for(int c=0;c<kPlanCommands;++c) {
        const int a=s11::command_action(c);
        if(kPlanCost[c]!=(a>=0?s11::kCost[a]:0))cost_ok=false;
        if((plan_special(c) || plan_retreat(c))!=(a>=0))cost_ok=false;
    }
    ok(cost_ok && kPlanCommands==11 && plan_retreat(kCmdRetreat) && kCmdRetreat==s11::CmdRetreat && s11::command_action(kCmdRetreat)==3,
       "plan: costs = the engine's kCost by command_action, specials and 퇴각 (local306, action 3) = commands with an action");
    {
        int who=7;
        FlowInput in;in.stance=1;
        ok(flow_command(in,who)==1 && who==-1,"flow: no pending command -> the stance kept");
        in.pend=7;ok(flow_command(in,who)==7 && who==-1,"flow: a pending 필살 goes first");
        in.pend=9;in.who=2;ok(flow_command(in,who)==9 && who==2,"flow: a pending 교대 with its member");
        in.autoplay=true;ok(flow_command(in,who)==-1 && who==-1,"flow: 자동 -> the engine's AI, nothing pending is sent");
        FlowInput bad;bad.stance=5;ok(flow_command(bad,who)==2,"flow: a stance out of range -> 투지 중시");
        ok(flow_peeks(0,false) && flow_peeks(3,false) && flow_peeks(9,false),"flow: a stance or a 교대 is looked ahead");
        ok(!flow_peeks(4,false) && !flow_peeks(8,false) && !flow_peeks(-1,false) && !flow_peeks(1,true),
           "flow: a 필살, 자동 and a join hold's own 합 are not");
        ok(join_command(0,3)==3 && join_command(2,0)==0 && join_command(9,1)==9 && join_command(1,6)==1,
           "flow: a join hold's 합 takes the stance chosen in it, a 교대 stays a 교대");
        ok(next_hold(true,HoldSwap,true)==HoldStart && next_hold(false,HoldSpecial,false)==HoldSpecial && next_hold(false,HoldSwap,false)==HoldSwap,
           "flow: the start hold first, then a 필살 / 교대 asked for");
        ok(next_hold(false,0,false)==HoldGap && next_hold(false,HoldSpecial,true)==HoldGap && next_hold(false,HoldJoin,false)==HoldGap,
           "flow: else the gap ride (also while 자동 plays)");
        ok(flow_distance(HoldGap)==1200.0f && flow_distance(HoldStart)==2500.0f && flow_distance(HoldJoin)==2500.0f && kGapMs==1250u,
           "flow: the gap rides 2 s filmed from 1200, every full hold from 2500");
        ok(gap_ms(1)==500u && gap_ms(2)==750u && gap_ms(3)==1000u && gap_ms(4)==1250u && gap_ms(5)==1500u && gap_ms(0)==250u && gap_ms(6)==kGapMs,
           "flow (local308 / local309): the installer's short gap 0..5 = 0.25-1.5 s in 0.25 s steps, anything else 1.25 s");
        ok(gap_none(6) && !gap_none(0) && !gap_none(5) && !gap_none(7) && kGapNone==6u,"flow (local312): choice 6 = no gap");
        ok(relay_round_cap(2)==15 && relay_round_cap(3)==15 && relay_round_cap(4)==20 && relay_round_cap(5)==25 && relay_round_cap(6)==30 &&
           relay_round_ext(2)==0 && relay_round_ext(3)==0 && relay_round_ext(4)==5 && relay_round_ext(6)==5 && kRoundExtPct==15,
           "cap (local312): 15 for 2-3 fighters, +5 a fighter from 4 to 30 at 6, one +5 extension from 4 at 15 %");
        {   // local313: the join ask, duel_relay_join 0..10 = 0..100 % per asked extra
            std::uint32_t st=join_seed(0,0);
            bool never=true,always=true;
            for(int i=0;i<1000;++i){never=never && !join_accepts(st,0);always=always && join_accepts(st,10) && join_accepts(st,12);}
            ok(never && always && join_seed(0,0)!=0u && kJoinStepDefault==5u,"join (local313): step 0 never joins, 10 and above always, the seed is never 0");
            int counts[11]={};
            for(unsigned step=1;step<10;++step){std::uint32_t s2=join_seed(12345u+step,step);for(int i=0;i<20000;++i)if(join_accepts(s2,step))++counts[step];}
            bool near=true;
            for(unsigned step=1;step<10;++step){const int want=static_cast<int>(step)*2000;near=near && counts[step]>want-400 && counts[step]<want+400;}
            ok(near,"join (local313): steps 1-9 accept about step * 10 % (20000 asks each, within 2 %)");
            std::uint32_t a=join_seed(777,3),b=join_seed(777,3),z=0;
            bool same=true;
            for(int i=0;i<64;++i)same=same && join_accepts(a,5)==join_accepts(b,5);
            join_next(z);
            ok(same && z!=0u && join_seed(777,3)!=join_seed(777,4),"join (local313): one seed, one answer sequence; a zero state recovers");
        }
        ok(special_button(false,-1)==0 && special_button(true,-1)==1 && special_button(true,5)==2 && special_button(false,8)==2 && special_button(true,9)==1,
           "strip: 필살 off / on / pending");
        ok(swap_button(true,0,-1)==0 && swap_button(true,1,-1)==1 && swap_button(true,1,9)==2 && swap_button(false,2,-1)==3 && swap_button(false,0,9)==2,
           "strip: 교대 no member / on / pending / used");
        // local306 (「2A」): the 교대 place turns into 퇴각 once no 교대 can be made now, and a 퇴각 pending keeps it
        ok(retreat_button(1,true,-1)==0 && retreat_button(2,true,-1)==0 && retreat_button(0,true,-1)==1 && retreat_button(3,true,-1)==1 &&
           retreat_button(3,false,-1)==3 && retreat_button(0,false,5)==3 && retreat_button(1,true,kCmdRetreat)==2 && retreat_button(3,false,kCmdRetreat)==2,
           "strip: 퇴각 on the 교대 place (교대 possible / pending: none; used or no member: on, short of 투지: off; pending)");
        in.autoplay=false;in.pend=kCmdRetreat;in.who=1;
        ok(flow_command(in,who)==kCmdRetreat && who==-1 && !flow_peeks(kCmdRetreat,false),"flow: a pending 퇴각 goes first and is not looked ahead");
        ok(next_hold(false,HoldRetreat,false)==HoldRetreat && next_hold(false,HoldRetreat,true)==HoldGap && flow_distance(HoldRetreat)==2500.0f,
           "flow: 퇴각 pressed during a 합 opens its full hold (not while 자동 plays)");
        ok(crit_view(0,0)==0 && crit_view(0,5)==5 && crit_view(0,6)==6 && crit_view(1,9)==6 && crit_view(2,-1)==0 && crit_view(3,0)==6 &&
           guard_view(0)==0 && guard_view(7)==7 && guard_view(20)==15 && guard_view(-2)==0,
           "status: charge 0-5 of 6, ready from 6 or in 일격 중시; guard timers 0-15");
    }
    {   // gating as the strip does it: a 필살 is offered when the engine's legal() allows it now, and then never fails in step()
        bool gate_ok=true,none_failed=true;
        for(int morale=0;morale<=300;morale+=10)for(int c=4;c<=8;++c) {
            s11::Duel d{};d.side[0].morale=morale;
            if(s11::legal(d,0,s11::command_action(c))!=(kPlanCost[c]<=morale))gate_ok=false;
        }
        for(unsigned seed=1;seed<=200;++seed) {
            s11::Duel d{};
            for(int s=0;s<2;++s){d.side[s].count=1;d.side[s].f[0].id=s+1;d.side[s].f[0].str=70+10*s;d.side[s].f[0].hp=100;}
            d.side[0].morale=static_cast<int>(seed*7%301);
            s11::start(d,seed);
            for(int k=0;k<30 && !d.finished;++k) {
                int c=d.side[0].stance;
                for(int x=8;x>=4;--x)if(s11::legal(d,0,s11::command_action(x))){c=x;break;}
                const int cmd[2]={c,s11::CmdAi};
                if(s11::step(d,cmd).side[0].special_failed)none_failed=false;
            }
        }
        ok(gate_ok,"gate: a 필살 is offered exactly when its cost fits the 투지");
        ok(none_failed,"gate: a 필살 the strip offers never fails in the engine (200 seeds, 30 합)");
    }
    {   // the look-ahead (local305): a copy stepped with one stance / 교대 brings in the same joiners with the same cuts as the real 합
        // stepped with another stance (the stance chosen in the join hold) or with the 교대 itself, for the scene's config. The AI side's
        // own roll comes first, so a copy played by the AI may differ (counted, not checked: why 자동 is never looked ahead).
        // local306: the scene's config has retreat on — the computer's retreat (its special roll, before the stance phase) must come out
        // the same in the copy and the real 합.
        bool same=true;int joins=0,looks=0,ai_differs=0,ai_retreats=0;
        for(unsigned seed=1;seed<=400;++seed) {
            s11::Duel d{};
            d.side[0].count=d.side[1].count=3;
            for(int i=0;i<3;++i){d.side[0].f[i].str=62+12*i;d.side[1].f[i].str=88-9*i;}
            if(seed>200)for(int i=0;i<3;++i){d.side[0].f[i].str=84+4*i;d.side[1].f[i].str=58+5*i;}   // local306: a weaker computer side, so it retreats
            d.cfg.drop_swap_on_join=true;d.cfg.join_cut_den=kEntryDen;d.cfg.swap_limit=1;d.cfg.dmg_num=d.cfg.dmg_den=1;d.cfg.retreat=true;
            s11::start(d,seed);
            for(int k=0;k<30 && !d.finished;++k) {
                const int a=static_cast<int>((seed+k)%4),b=static_cast<int>((seed*3+k)%4);
                const int look=k%3==1?s11::CmdSwap:a;
                s11::Duel probe=d;
                if(look==s11::CmdSwap)probe.swap_pick[0]=2;
                const int cp[2]={look,s11::CmdAi};
                const s11::RoundResult pr=s11::step(probe,cp);
                s11::Duel alt=d;
                const int ca[2]={s11::CmdAi,s11::CmdAi};
                const s11::RoundResult ar=s11::step(alt,ca);
                if(ar.side[0].join_in!=pr.side[0].join_in || ar.side[1].join_in!=pr.side[1].join_in)++ai_differs;
                const int real=join_command(look,b);
                if(real==s11::CmdSwap)d.swap_pick[0]=2;
                const int cr[2]={real,s11::CmdAi};
                const s11::RoundResult rr=s11::step(d,cr);
                ++looks;
                for(int s=0;s<2;++s)if(rr.side[s].join_in!=pr.side[s].join_in || rr.side[s].join_cut!=pr.side[s].join_cut)same=false;
                if((rr.side[1].special==3)!=(pr.side[1].special==3) || (pr.side[1].special==3 && (!rr.finished || rr.winner!=pr.winner)))same=false;
                if(rr.side[1].special==3)++ai_retreats;
                if(rr.side[0].join_in>=0 || rr.side[1].join_in>=0)++joins;
            }
        }
        std::printf("{\"look_ahead\":{\"looks\":%d,\"joins\":%d,\"ai_side_differs\":%d,\"ai_retreats\":%d}}\n",looks,joins,ai_differs,ai_retreats);
        ok(same && joins>=20 && ai_retreats>0,"look-ahead (retreat on, the scene's config): a stance / 교대 copy brings in the same joiners with the same cuts, "
           "and the same computer retreat, as the real 합 (400 seeds)");
    }
    {
        int sp[2]={4,-1},dm[2]={30,0},code[2]={1,0};special_codes(sp,dm,code);
        ok(code[0]==3 && code[1]==0,"codes: 급소 that hit = 반격 on its side");
        int sp2[2]={-1,2},dm2[2]={0,0},code2[2]={2,2};special_codes(sp2,dm2,code2);
        ok(code2[0]==0 && code2[1]==2,"codes: 견수 = 막음 on its own side only");
        int sp3[2]={4,-1},dm3[2]={0,0},code3[2]={2,2};special_codes(sp3,dm3,code3);
        ok(code3[0]==2 && code3[1]==2,"codes: 급소 without damage keeps the engine's codes");
        int sp4[2]={5,-1},dm4[2]={40,0},code4[2]={1,0};special_codes(sp4,dm4,code4);
        ok(code4[0]==1 && code4[1]==0,"codes: 무쌍 / 필살기 keep 회심");
        int sp5[2]={1,-1},dm5[2]={0,0},code5[2]={2,2};special_codes(sp5,dm5,code5);
        ok(code5[0]==2 && code5[1]==2,"codes: 기합 keeps 막음 on both sides");
    }
    bool split_ok=true,n_ok=true;
    for(int before=1;before<=100;++before)for(int after=0;after<=before;++after) {
        int seq[4];const int n=musou_split(before,after,seq);
        const int d=before-after;
        if(n<1 || n>3 || seq[0]!=before || seq[n]!=after){split_ok=false;continue;}
        for(int i=1;i<=n;++i)if(seq[i]>=seq[i-1] && d>0)split_ok=false;
        for(int i=1;i<n;++i)if(seq[i]<1 || seq[i]<=after)split_ok=false;
        if(d>=9 && n!=3)n_ok=false;
        if(d<4 && n!=1)n_ok=false;
    }
    ok(split_ok,"무쌍: steps strictly lower, intermediate HP >= 1, ends on the engine HP");
    ok(n_ok,"무쌍: 3 hits from 9 damage, 1 below 4");
    wchar_t cells[32];
    morale_cells(160,100,cells,32);
    ok(std::wcscmp(cells,L"■■■▣▣ ▣▣▣□□ □□□□□")==0,"gauge: 160 with 100 promised");
    morale_cells(300,0,cells,32);
    ok(std::wcscmp(cells,L"■■■■■ ■■■■■ ■■■■■")==0,"gauge: full");
    morale_cells(0,0,cells,32);
    ok(std::wcscmp(cells,L"□□□□□ □□□□□ □□□□□")==0,"gauge: empty");
}
// local300 relay camera: the absolute rotation of a wanted forward / up. The four rotations local299 wrote and the forward / up the
// camera block showed for each in the same tick (duel_manual_qraw / qraw2, 4 decimals): the rotation built from that forward / up is
// the one that was written, and Rot(q) z / Rot(q) y give back the forward / up.
static void check_camera() {
    static const float kSeen[4][10]={
        {-0.0897f,0.7855f,0.1174f,0.6008f,0.9229f,0.2923f,-0.2504f,-0.2821f,0.9563f,0.0765f},
        {0.0897f,-0.7855f,-0.1174f,0.6008f,-0.9651f,0.0765f,-0.2504f,0.0f,0.9563f,0.2923f},
        {-0.0897f,0.739f,-0.2911f,0.6008f,0.9403f,-0.3223f,-0.1085f,0.217f,0.8143f,-0.5382f},
        {0.0897f,-0.739f,0.2911f,0.6008f,-0.8358f,-0.5382f,-0.1085f,-0.4825f,0.8143f,-0.3223f}};
    bool same=true,back=true;
    for(const float *r:kSeen) {
        float q[4],m[9];
        if(!cam_abs(r+4,r+7,q)){same=back=false;continue;}
        const float d=std::fabs(q[0]*r[0]+q[1]*r[1]+q[2]*r[2]+q[3]*r[3]);
        if(!(d>0.9995f))same=false;
        float w[4]={r[0],r[1],r[2],r[3]};cam_unit(w,4);cam_quat_mat(w,m);
        const float df=m[2]*r[4]+m[5]*r[5]+m[8]*r[6],du=m[1]*r[7]+m[4]*r[8]+m[7]*r[9];
        if(!(df>0.9995f && du>0.999f))back=false;
    }
    ok(same,"camera: the rotation built from a seen forward / up is the one written (local299 samples)");
    ok(back,"camera: Rot(q) z / Rot(q) y are the forward / up the block showed");
    {   // a side-camera view: forward down 13 degrees, a level up made square
        float f[3]={0.6f,-0.22f,-0.77f};cam_unit(f,3);
        const float up[3]={0.0f,1.0f,0.0f};
        float q[4],m[9];
        const bool built=cam_abs(f,up,q);
        cam_quat_mat(q,m);
        const float df=m[2]*f[0]+m[5]*f[1]+m[8]*f[2];
        float u[3]={up[0]-f[0]*f[1],up[1]-f[1]*f[1],up[2]-f[2]*f[1]};cam_unit(u,3);
        const float du=m[1]*u[0]+m[4]*u[1]+m[7]*u[2],level=m[3];   // side x up: the screen right stays level (its y is m[3])
        ok(built && df>0.99999f && du>0.99999f && std::fabs(level)<1e-4f,"camera: wanted forward and level up come back exactly");
    }
    float q[4];
    const float zero[3]={0.0f,0.0f,0.0f},up[3]={0.0f,1.0f,0.0f},down[3]={0.0f,-1.0f,0.0f};
    ok(!cam_abs(zero,up,q) && !cam_abs(down,up,q),"camera: no rotation for a zero forward or one along the up");
    bool fb=false;
    ok(hold_distance(1572.0f,fb)==2500.0f && !fb && hold_distance(4320.0f,fb)==2500.0f && !fb,"hold distance: fixed 2500 whatever the original's (local303)");
    ok(hold_distance(0.0f,fb)==2500.0f && fb,"hold distance: no measure -> fallback flag, same distance");
    ok(hold_distance(9000.0f,fb)==2500.0f && fb,"hold distance: a measure out of range -> fallback flag");
    const float c[3]={100.0f,50.0f,-20.0f},f[3]={0.0f,-0.5f,0.8660254f};
    float e[3];follow_eye(c,f,2000.0f,e);
    ok(std::fabs(e[0]-100.0f)<0.01f && std::fabs(e[1]-1050.0f)<0.01f && std::fabs(e[2]+1752.05f)<0.05f,"follow eye: back along the view");
}
// local301: the 교대 member pick (A: waiting members who fought), the entry 무력 gap cut (first pair, a third of the gap), the duel
// traits' power (단기 + 확삭 by age) and the engine's side of them.
static void check_local301() {
    namespace s11=duel_s11;
    SwapView v;v.count=3;v.active=0;v.hp[0]=50;v.hp[1]=30;v.hp[2]=60;v.state[0]=0;v.state[1]=1;v.state[2]=1;
    ok(swap_pick(v,1)==1 && swap_pick(v,-1)==2 && swap_pick(v,0)==2,"pick: the wanted member, else the most HP");
    v.state[2]=2;
    ok(swap_pick(v,2)==1,"pick: a member not entered yet is never picked (A)");
    int out[3]={-1,-1,-1};
    ok(swap_candidates(v,out)==1 && out[0]==1,"pick: candidates = waiting members who fought");
    SwapView after=v;after.state[0]=1;after.state[1]=0;after.active=1;
    ok(swap_candidates(after,out)==1 && out[0]==0,"pick: after a 교대 the one who left waits");
    v.hp[1]=0;
    ok(swap_candidates(v,out)==0,"pick: no HP, no candidate");
    bool same=true;   // the engine's swap_target / manual_swap_target agree with the window's swap_pick on every small case
    for(int st=0;st<27;++st)for(int hp=0;hp<8;++hp)for(int want=-1;want<3;++want) {
        s11::Duel d{};
        SwapView w;w.count=3;w.active=0;
        for(int i=0;i<3;++i) {
            const int s3=i==0?0:(st/(i==1?1:3))%3;
            d.side[0].f[i].state=w.state[i]=i==0?0:s3;
            d.side[0].f[i].hp=w.hp[i]=(hp>>i)&1?40+i:0;
        }
        d.side[0].count=3;d.side[0].active=0;
        if(s11::swap_target(d,0,want)!=swap_pick(w,want) || s11::manual_swap_target(d,0)!=swap_pick(w,-1))same=false;
    }
    ok(same,"pick: engine swap_target = window swap_pick (all small cases)");
    int picked=0,engine=0,trials=0;bool cleared=true;
    for(unsigned seed=1;seed<=60;++seed)for(int want=-1;want<=1;want+=2) {
        s11::Duel d{};
        for(int s=0;s<2;++s){d.side[s].count=1;d.side[s].f[0].id=s+1;d.side[s].f[0].str=75;d.side[s].f[0].hp=90;}
        d.side[0].count=3;d.side[0].f[0].state=0;   // start() keeps the states when exactly one slot fights
        for(int i=1;i<3;++i){d.side[0].f[i].id=10+i;d.side[0].f[i].str=70;d.side[0].f[i].state=1;}
        d.side[0].f[1].hp=30;d.side[0].f[2].hp=60;
        s11::start(d,seed);
        d.side[0].morale=d.side[1].morale=0;
        d.swap_pick[0]=want;
        const int cmd[2]={s11::CmdSwap,s11::CmdAi};
        const s11::RoundResult r=s11::step(d,cmd);
        if(d.swap_pick[0]!=-1)cleared=false;
        if(r.side[0].join_in>=0 || r.side[0].swap_in<0)continue;
        ++trials;
        if(want==1 && d.side[0].active==1)++picked;
        if(want<0 && d.side[0].active==2)++engine;
    }
    ok(trials>20 && picked+engine==trials && cleared,"pick: step brings in the asked member, the engine's pick without one");
    ok(entry_cut(97,81)==6 && entry_cut(100,99)==1 && entry_cut(81,97)==0 && entry_cut(50,50)==0,"entry: a third of the 무력 gap, rounded up");
    ok(entry_hp(100,6)==94 && entry_hp(5,10)==1 && entry_hp(1,3)==1 && entry_hp(80,0)==80,"entry: HP cut, never below 1");
    ok(age_step(59)==0 && age_step(60)==3 && age_step(64)==3 && age_step(65)==4 && age_step(70)==6 && age_step(79)==6 && age_step(80)==8 &&
       age_step(90)==10,"traits: 확삭 age steps (rules_combat 9.5)");
    ok(trait_power(10,false,90)==10 && trait_power(10,true,72)==16 && trait_power(0,true,59)==0 && trait_power(95,true,95)==100 &&
       trait_power(-5,false,0)==0,"traits: 단기 + 확삭 power, 0..100");
    s11::Duel d{};
    d.side[0].f[0].power=20;
    ok(s11::boosted(d,0,10)==12 && s11::boosted(d,0,0)==0 && s11::boosted(d,1,10)==10,"traits: a regular hit grows by the power");
}
int main() {
    int b[kInts];
    make_orig(b,0,1,100,72,100,0,2);check_plan(b,4,"row 0 wins by KO");
    make_orig(b,1,0,64,0,90,81,1);check_plan(b,4,"row 1 wins by KO");
    make_orig(b,0,1,80,55,95,30,0);check_plan(b,4,"row 0 wins on HP (no KO)");
    make_orig(b,2,2,70,40,70,40,0);check_plan(b,4,"draw");
    make_orig(b,0,1,100,100,3,0,0);check_plan(b,4,"lopsided: winner untouched, tiny loser");
    make_orig(b,0,1,100,72,100,0,2);check_plan(b,1,"single block");
    make_orig(b,0,1,100,72,100,0,2);check_plan(b,6,"six blocks (30 rounds)");
    {   // no KO the original did not have: a judged loser keeps HP above 0 in every block
        make_orig(b,1,0,90,5,60,20,0);
        const Plan p=build(b,4);
        bool alive=true;
        for(int k=0;k<4;++k)for(int r=0;r<=kRounds;++r)if(p.block[k][row_base(0)+4+r]<=0)alive=false;
        ok(p.why==0 && alive,"judged loss: the loser never reaches 0");
    }
    make_orig(b,0,1,100,72,100,0,2);b[2]=1;ok(build(b,4).why==2,"일격 refused");
    make_orig(b,0,0,100,72,100,0,2);ok(build(b,4).why==1,"two winners refused");
    make_orig(b,0,1,100,120,100,0,2);ok(build(b,4).why==3,"final above start refused");
    make_orig(b,0,1,100,72,100,0,2);ok(build(b,0).why==4 && build(b,7).why==4,"segment count range");
    check_plan_list();
    check_camera();
    check_local301();
    std::printf("{\"status\":\"%s\",\"checks\":%d,\"failures\":%d}\n",failures?"duel_manual_fixture_failed":"duel_manual_fixture_passed",checks,failures);
    return failures?1:0;
}
