#ifndef SAN14_TACTICS_POLICY_H
#define SAN14_TACTICS_POLICY_H

/* This module is executable policy, not a game-memory adapter. Native game
 * callers must supply verified combat context and feasibility results. */
#ifdef TACTICS_POLICY_EXPORTS
#define TACTICS_API __declspec(dllexport)
#else
#define TACTICS_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum TacticsAction {
    TACTICS_KEEP_PLAYER_ORDER = 0,
    TACTICS_CUT_PLAYER_LOGISTICS = 1,
    TACTICS_CONTINUE_CONSTRUCTION = 2,
    TACTICS_START_CONSTRUCTION = 3,
    TACTICS_OTHER_TASK = 4,
    TACTICS_ATTACK_CITY = 5,
    TACTICS_NO_FEASIBLE_TASK = 6
};

typedef struct TacticsSituation {
    int computer_controlled;
    int valid_hostile_player_cut;
    int captured_region_in_campaign;
    int active_legal_construction;
    unsigned int legal_facilities;
    int feasible_other_task;
    int feasible_city_attack;
} TacticsSituation;

typedef struct TacticsDecision {
    int action;
    int facility_id;
} TacticsDecision;

/* The opponent must be the actual participant in this combat calculation.
 * A nearby player, current movement target, or ruler name is insufficient.
 * opponent_force_id == 0 denotes no opponent, and returns base 4 for an AI.
 * Unknown context returns -1: the adapter must defer/reject this calculation. */
TACTICS_API int tactics_area_coefficient(int own_force_id, int player_force_id,
                                       int opponent_force_id,
                                       int native_coefficient);
TACTICS_API int tactics_choose_facility(unsigned int legal_facilities);
TACTICS_API int tactics_decide(const TacticsSituation *s, TacticsDecision *out);

#ifdef __cplusplus
}
#endif

#endif
