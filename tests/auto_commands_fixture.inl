// [game_flow] auto-commands pure policy fixture (PW24a): reward decision table, threshold clamp and the
// once-per-game-day gate. Self-contained (no adapter state) so the unit-test shell can include it as-is.
#include <stdio.h>
#include <stdlib.h>
#include "../src/auto_commands_policy.h"
static void auto_commands_policy_tests() {
    using namespace auto_commands_policy;
    auto check=[](bool ok,const char *label) {if(!ok) {fprintf(stderr,"FAIL: %s\n",label);exit(1);}};
    // reward_decision: 3 nothing, 2 no ticket, 1 gold-short base, 0 issue.
    check(reward_decision(0,9999,9)==3,"auto_commands: no eligible officer does nothing");
    check(reward_decision(-2,9999,9)==3,"auto_commands: negative count does nothing");
    check(reward_decision(1,100,0)==2,"auto_commands: no command ticket stops the pass");
    check(reward_decision(3,9999,0)==2,"auto_commands: ticket check comes before the gold check");
    check(reward_decision(1,99,1)==1,"auto_commands: 99 gold cannot pay for one officer");
    check(reward_decision(1,100,1)==0,"auto_commands: 100 gold pays for one officer");
    check(reward_decision(3,299,5)==1,"auto_commands: 299 gold is short for three officers");
    check(reward_decision(3,300,5)==0,"auto_commands: 300 gold pays for three officers");
    // clamp_threshold.
    check(clamp_threshold(-5)==0 && clamp_threshold(0)==0,"auto_commands: threshold clamps to 0");
    check(clamp_threshold(55)==55,"auto_commands: threshold keeps 55");
    check(clamp_threshold(100)==100 && clamp_threshold(120)==100,"auto_commands: threshold clamps to 100");
    // date_changed: the same game day runs once; a new day runs again.
    int last=-1;
    check(!date_changed(-1,last) && last==-1,"auto_commands: a missing date never runs");
    check(date_changed(40,last) && last==40,"auto_commands: the first frame of a day runs");
    check(!date_changed(40,last) && last==40,"auto_commands: the same date twice runs once");
    check(date_changed(41,last) && last==41,"auto_commands: the next day runs again");
    // turn_changed: 탐색위임 searches once per 10-day turn (day 1/11/21 start a turn), not on every day of 진행.
    int turn=-1;
    check(turn_of(-1)==-1 && turn_of(0)==0 && turn_of(9)==0 && turn_of(10)==1 && turn_of(79110)==7911,"auto_search: day to turn");
    check(turn_changed(turn_of(79110),turn) && turn==7911,"auto_search: first day of a turn searches");
    check(!turn_changed(turn_of(79111),turn) && !turn_changed(turn_of(79119),turn),"auto_search: later days of the turn skip");
    check(turn_changed(turn_of(79120),turn) && turn==7912,"auto_search: the next turn searches again");
    check(turn_changed(-1,turn) && turn==7912,"auto_search: a missing date keeps searching");
    // 자동탐색 설정: 세이브 <-> 드롭다운 양방향 매핑과 이번 실행의 지급 상한.
    check(clamp_scope(-1)==0 && clamp_scope(0)==0,"auto_search: scope clamps to 0");
    check(clamp_scope(3)==3 && clamp_scope(9)==3,"auto_search: scope clamps to 3");
    check(basis_from_save(0)==2 && basis_from_save(1)==0 && basis_from_save(2)==1,
          "auto_search: officer basis is shown in the rotated dropdown order");
    check(basis_to_save(0)==1 && basis_to_save(1)==2 && basis_to_save(2)==0,"auto_search: officer basis saves back");
    check(basis_to_save(basis_from_save(0))==0 && basis_to_save(basis_from_save(1))==1 &&
          basis_to_save(basis_from_save(2))==2,"auto_search: officer basis round-trips");
    check(region_from_save(0)==0 && region_from_save(1)==0 && region_from_save(2)==1,"auto_search: region shows two save values as one");
    check(region_to_save(0)==1 && region_to_save(1)==2,"auto_search: region saves back");
    check(region_to_save(region_from_save(2))==2 && region_to_save(region_from_save(1))==1,"auto_search: region round-trips 1 and 2");
    check(region_to_save(region_from_save(0))==1,"auto_search: region save 0 collapses to 1 like the native save-back");
    const SearchWidgets shown=widgets_from_save(SearchSettings{9,1,2});
    check(shown.scope==3 && shown.basis==0 && shown.region==1,"auto_search: widget values come from the save block");
    const SearchSettings kept=save_from_widgets(SearchWidgets{2,2,7},SearchSettings{1,1,2});
    check(kept.scope==2 && kept.basis==0 && kept.region==2,"auto_search: an unknown region widget keeps the saved value");
    check(save_from_widgets(SearchWidgets{0,0,0},SearchSettings{0,0,0}).region==1,"auto_search: region widget 0 saves 1");
    check(save_from_widgets(SearchWidgets{0,1,1},SearchSettings{0,0,0}).region==2,"auto_search: region widget 1 saves 2");
    check(search_take(0,5)==0 && search_take(5,0)==0,"auto_search: no candidates or no tokens takes nobody");
    check(search_take(-3,5)==0 && search_take(5,-1)==0,"auto_search: negative counts take nobody");
    check(search_take(3,9)==3 && search_take(9,3)==3 && search_take(4,4)==4,"auto_search: takes min(count,tokens)");
}
