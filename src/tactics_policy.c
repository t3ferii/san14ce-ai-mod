#include "tactics_policy.h"

int tactics_area_coefficient(int own, int player, int opponent, int native_value)
{
    if (own <= 0 || player <= 0 || opponent < 0 || native_value < 0) return -1;
    if (own == player) return native_value;
    if (opponent == player) return native_value;
    return 4;
}

int tactics_choose_facility(unsigned int legal)
{
    /* Facility definitions read from this game's Korean definition table:
     * 6 catapult platform, 4 fortress, 7 music, 3 camp;
     * traps: 11 fire, 12 fierce fire, 14 pit, 13 fire ship.
     * Ties inside the trap category are a provisional order. */
    static const int priority[] = {6, 4, 7, 3, 11, 12, 14, 13};
    unsigned int i;
    for (i = 0; i < sizeof(priority) / sizeof(priority[0]); ++i) {
        int id = priority[i];
        if ((legal & (1u << id)) != 0) return id;
    }
    return 0;
}

int tactics_decide(const TacticsSituation *s, TacticsDecision *out)
{
    int facility;
    if (s == 0 || out == 0) return 0;
    out->facility_id = 0;
    if (!s->computer_controlled) {
        out->action = TACTICS_KEEP_PLAYER_ORDER;
    } else if (s->valid_hostile_player_cut) {
        /* No leadership/intelligence threshold or group-mode-8 exclusion.
         * Feasibility must already include reachability and actual cut test. */
        out->action = TACTICS_CUT_PLAYER_LOGISTICS;
    } else if (s->active_legal_construction) {
        out->action = TACTICS_CONTINUE_CONSTRUCTION;
    } else if (s->captured_region_in_campaign &&
               (facility = tactics_choose_facility(s->legal_facilities)) != 0) {
        out->action = TACTICS_START_CONSTRUCTION;
        out->facility_id = facility;
    } else if (s->feasible_other_task) {
        out->action = TACTICS_OTHER_TASK;
    } else if (s->feasible_city_attack) {
        out->action = TACTICS_ATTACK_CITY;
    } else {
        out->action = TACTICS_NO_FEASIBLE_TASK;
    }
    return 1;
}
