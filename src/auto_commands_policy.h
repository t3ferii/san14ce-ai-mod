// [game_flow] auto-commands pure policy (2026-11-09, user request): no game addresses, native calls or logging.
// Kept in its own header so the unit-test shell can exercise the decision table without the adapter.
#pragma once
#include <algorithm>
#include <vector>
namespace auto_commands_policy {
// Award decision for one base. 3 = nothing to do (no eligible officer), 2 = stop (no command
// tickets left for the force), 1 = skip this base (not enough gold for its officers), 0 = issue.
inline int reward_decision(int n, int gold, int tokens) {
    if (n <= 0) return 3;
    if (tokens < 1) return 2;
    if (gold < 100 * n) return 1;
    return 0;
}
inline int clamp_threshold(int value) { return value < 0 ? 0 : value > 100 ? 100 : value; }
// user 2026-11-09: one award for the whole force at the ruler's base. No ticket -> nobody; otherwise the lowest
// loyalty first (ties by id) as far as the base gold pays 100 each, at most 300 officers.
struct RewardCandidate { unsigned short id; unsigned char loyalty; };
inline std::vector<unsigned short> reward_pick(std::vector<RewardCandidate> list, int gold, int tokens) {
    std::vector<unsigned short> out;
    if (tokens < 1 || gold < 100) return out;
    std::sort(list.begin(), list.end(), [](const RewardCandidate &a, const RewardCandidate &b) {
        return a.loyalty != b.loyalty ? a.loyalty < b.loyalty : a.id < b.id;
    });
    const size_t afford = static_cast<size_t>(gold / 100);
    for (const auto &c : list) {
        if (out.size() >= afford || out.size() >= 300) break;
        out.push_back(c.id);
    }
    return out;
}
// 자동탐색 설정 세 개(2026-11-09, PW24b). 세이브 블록에는 int 세 개로 저장된다: +0x17B0 탐색 범위(0..3),
// +0x17B8 무장 기준, +0x17B4 지역. 화면의 드롭다운 표시 순서가 저장값과 다르고 목록마다 회전이 달라서
// 양방향 매핑을 여기에 명시한다(근거: confirms 734620 저장-back, enter 727000 복원).
// 두 번째 목록: 저장 0->위젯2, 1->0, 2->1 / 위젯 0->저장1, 1->2, 2->0.
// 세 번째 목록: 저장 0,1->위젯0, 2->1 / 위젯 0->저장1, 1->2, 그 밖은 저장값 유지.
struct SearchSettings { int scope; int basis; int region; };   // save form
struct SearchWidgets { int scope; int basis; int region; };    // dropdown index form
inline int clamp_scope(int value) { return value < 0 ? 0 : value > 3 ? 3 : value; }
inline int basis_from_save(int save) { return save == 1 ? 0 : save == 2 ? 1 : 2; }
inline int basis_to_save(int widget) { return widget == 0 ? 1 : widget == 1 ? 2 : 0; }
inline int region_from_save(int save) { return save == 2 ? 1 : 0; }
inline int region_to_save(int widget) { return widget == 1 ? 2 : 1; }
inline SearchWidgets widgets_from_save(SearchSettings save) {
    return SearchWidgets{clamp_scope(save.scope), basis_from_save(save.basis), region_from_save(save.region)};
}
// `current` supplies the third list's unchanged case (the native save-back leaves other widget values alone).
inline SearchSettings save_from_widgets(SearchWidgets widget, SearchSettings current) {
    SearchSettings save;
    save.scope = clamp_scope(widget.scope);
    save.basis = basis_to_save(widget.basis);
    save.region = (widget.region == 0 || widget.region == 1) ? region_to_save(widget.region) : current.region;
    return save;
}
// How many officers one pass may issue: the first min(count, tokens) of the candidate list, 0 when either is empty.
inline int search_take(int count, int tokens) {
    if (count <= 0 || tokens <= 0) return 0;
    return count < tokens ? count : tokens;
}
// Game day (30-day months, day 1 -> multiple of 30) to its 10-day turn; -1 stays -1.
inline int turn_of(int day) { return day < 0 ? -1 : day / 10; }
// True once per turn. A missing date (-1) keeps the per-call behaviour (always true) so no search is lost.
inline bool turn_changed(int turn, int &last) {
    if (turn < 0) return true;
    if (turn == last) return false;
    last = turn;
    return true;
}
// True once for a new game day; a missing date (-1) never runs. The caller keeps `last` in memory.
inline bool date_changed(int day, int &last) {
    if (day < 0 || day == last) return false;
    last = day;
    return true;
}
}
