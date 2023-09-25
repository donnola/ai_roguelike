#pragma once

#include "stateMachine.h"

// states
State *create_attack_enemy_state();
State *create_move_to_enemy_state();
State *create_flee_from_enemy_state();
State *create_patrol_state(float patrol_dist);
State *create_nop_state();
State *create_selfheal_state(float heal_amount);
State *create_heal_teammate_state(float heal_amount, float triggerDist);
State *create_move_to_teammate_state(float patrol_dist);

// transitions
StateTransition *create_enemy_available_transition(float dist);
StateTransition *create_teammate_available_transition(float dist);
StateTransition *create_enemy_reachable_transition();
StateTransition *create_hitpoints_less_than_transition(float thres);
StateTransition *create_must_heal_teammate_transition(float thres, float dist);
StateTransition *create_negate_transition(StateTransition *in);
StateTransition *create_and_transition(StateTransition *lhs, StateTransition *rhs);
StateTransition *create_or_transition(StateTransition *lhs, StateTransition *rhs);

