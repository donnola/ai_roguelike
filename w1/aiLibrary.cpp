#include "aiLibrary.h"
#include <flecs.h>
#include "ecsTypes.h"
#include "raylib.h"
#include <cfloat>
#include <cmath>

class AttackEnemyState : public State
{
public:
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &/*ecs*/, flecs::entity /*entity*/) const override {}
};

template<typename T>
T sqr(T a){ return a*a; }

template<typename T, typename U>
static float dist_sq(const T &lhs, const U &rhs) { return float(sqr(lhs.x - rhs.x) + sqr(lhs.y - rhs.y)); }

template<typename T, typename U>
static float dist(const T &lhs, const U &rhs) { return sqrtf(dist_sq(lhs, rhs)); }

template<typename T, typename U>
static int move_towards(const T &from, const U &to)
{
  int deltaX = to.x - from.x;
  int deltaY = to.y - from.y;
  if (abs(deltaX) > abs(deltaY))
    return deltaX > 0 ? EA_MOVE_RIGHT : EA_MOVE_LEFT;
  return deltaY < 0 ? EA_MOVE_UP : EA_MOVE_DOWN;
}

static int inverse_move(int move)
{
  return move == EA_MOVE_LEFT ? EA_MOVE_RIGHT :
         move == EA_MOVE_RIGHT ? EA_MOVE_LEFT :
         move == EA_MOVE_UP ? EA_MOVE_DOWN :
         move == EA_MOVE_DOWN ? EA_MOVE_UP : move;
}


template<typename Callable>
static void on_closest_enemy_pos(flecs::world &ecs, flecs::entity entity, Callable c)
{
  static auto enemiesQuery = ecs.query<const Position, const Team>();
  entity.set([&](const Position &pos, const Team &t, Action &a)
  {
    flecs::entity closestEnemy;
    float closestDist = FLT_MAX;
    Position closestPos;
    enemiesQuery.each([&](flecs::entity enemy, const Position &epos, const Team &et)
    {
      if (t.team == et.team)
        return;
      float curDist = dist(epos, pos);
      if (curDist < closestDist)
      {
        closestDist = curDist;
        closestPos = epos;
        closestEnemy = enemy;
      }
    });
    if (ecs.is_valid(closestEnemy))
      c(a, pos, closestPos);
  });
}

class MoveToEnemyState : public State
{
public:
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) const override
  {
    on_closest_enemy_pos(ecs, entity, [&](Action &a, const Position &pos, const Position &enemy_pos)
    {
      a.action = move_towards(pos, enemy_pos);
    });

    entity.set([&](HealCooldown &hc)
    {
      hc.cooldown -= 1;
    });
  }
};

class FleeFromEnemyState : public State
{
public:
  FleeFromEnemyState() {}
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) const override
  {
    on_closest_enemy_pos(ecs, entity, [&](Action &a, const Position &pos, const Position &enemy_pos)
    {
      a.action = inverse_move(move_towards(pos, enemy_pos));
    });
  }
};

class PatrolState : public State
{
  float patrolDist;
public:
  PatrolState(float dist) : patrolDist(dist) {}
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) const override
  {
    entity.set([&](const Position &pos, const PatrolPos &ppos, Action &a)
    {
      if (dist(pos, ppos) > patrolDist)
        a.action = move_towards(pos, ppos); // do a recovery walk
      else
      {
        // do a random walk
        a.action = GetRandomValue(EA_MOVE_START, EA_MOVE_END - 1);
      }
    });
  }
};

class MoveToTeammatelState : public State
{
  float patrolDist;
public:
  MoveToTeammatelState(float dist) : patrolDist(dist) {}
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) const override
  {
    Position p;
    float min_dist = 100.f;

    static auto teammateQuery = ecs.query<const Position, const Team>();
    entity.get([&](const Position &pos, const Team &t)
    {
      teammateQuery.each([&](flecs::entity teammate, const Position &tpos, const Team &pt)
      {
        float curDist = dist(tpos, pos);
        if (t.team == pt.team && curDist <= min_dist && teammate != entity)
        {
          min_dist = curDist;
          p = tpos;
        }
      });
    });

    entity.set([&](const Position &pos, Action &a, HealCooldown &hc)
    {
      hc.cooldown -= 1;
      if (dist(pos, p) > patrolDist)
        a.action = move_towards(pos, p); // do a recovery walk
      else
      {
        // do a random walk
        a.action = GetRandomValue(EA_MOVE_START, EA_MOVE_END - 1);
      }
    });
  }
};


class NopState : public State
{
public:
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) const override {}
};

class SelfHealState : public State
{
  float healAmount;
public:
  SelfHealState(float amount) : healAmount(amount) {}
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) const override
  {
    entity.set([&](Hitpoints &hp)
    {
      hp.hitpoints += healAmount;
    });
  }
};

class TeammatefHealState : public State
{
  float healAmount;
  float triggerDist;
public:
  TeammatefHealState(float amount, float in_dist) : healAmount(amount), triggerDist(in_dist) {}
  void enter() const override {}
  void exit() const override {}
  void act(float/* dt*/, flecs::world &ecs, flecs::entity entity) const override
  {
    static auto teammateQuery = ecs.query<const Position, const Team>();
    entity.get([&](const Position &pos, const Team &t)
    {
      teammateQuery.each([&](flecs::entity teammate, const Position &tpos, const Team &pt)
      {
        float curDist = dist(tpos, pos);
        if (t.team == pt.team && curDist <= triggerDist && teammate != entity)
        {
          teammate.set([&](Hitpoints &hp)
          {
            hp.hitpoints += healAmount;
          });
        }
      });
    });
    entity.set([&](HealCooldown &hc)
    {
      hc.cooldown = 10;
    });
  }
};

class EnemyAvailableTransition : public StateTransition
{
  float triggerDist;
public:
  EnemyAvailableTransition(float in_dist) : triggerDist(in_dist) {}
  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    static auto enemiesQuery = ecs.query<const Position, const Team>();
    bool enemiesFound = false;
    entity.get([&](const Position &pos, const Team &t)
    {
      enemiesQuery.each([&](flecs::entity enemy, const Position &epos, const Team &et)
      {
        if (t.team == et.team)
          return;
        float curDist = dist(epos, pos);
        enemiesFound |= curDist <= triggerDist;
      });
    });
    return enemiesFound;
  }
};

class TeammateAvailableTransition: public StateTransition
{
  float triggerDist;
public:
  TeammateAvailableTransition(float in_dist) : triggerDist(in_dist) {}
  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    static auto teammateQuery = ecs.query<const Position, const Team>();
    bool teammateFound = false;
    entity.get([&](const Position &pos, const Team &t)
    {
      teammateQuery.each([&](flecs::entity teammate, const Position &tpos, const Team &pt)
      {
        if (t.team != pt.team || teammate == entity)
          return;
        float curDist = dist(tpos, pos);
        teammateFound |= curDist <= triggerDist;
      });
    });
    return teammateFound;
  }
};

class HitpointsLessThanTransition : public StateTransition
{
  float threshold;
public:
  HitpointsLessThanTransition(float in_thres) : threshold(in_thres) {}
  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    bool hitpointsThresholdReached = false;
    entity.get([&](const Hitpoints &hp)
    {
      hitpointsThresholdReached |= hp.hitpoints < threshold;
    });
    return hitpointsThresholdReached;
  }
};

class MustHealTeammateTransition : public StateTransition
{
  float threshold;
  float triggerDist;
public:
  MustHealTeammateTransition(float in_thres, float dist) : threshold(in_thres), triggerDist(dist) {}
  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    bool mustHealTeammate = false;
    static auto teammateQuery = ecs.query<const Position, const Team, const Hitpoints>();
    entity.get([&](const HealCooldown &hc, const Team &t, const Position &pos)
    {
      if (hc.cooldown <= 0)
      {
        teammateQuery.each([&](flecs::entity teammate, const Position &tpos, const Team &pt, const Hitpoints &hp)
        {
          mustHealTeammate |= t.team == pt.team && dist(pos, tpos) <= triggerDist && hp.hitpoints <= threshold && teammate != entity;
        });
      }
    });
    return mustHealTeammate;
  }
};

class EnemyReachableTransition : public StateTransition
{
public:
  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    return false;
  }
};

class NegateTransition : public StateTransition
{
  const StateTransition *transition; // we own it
public:
  NegateTransition(const StateTransition *in_trans) : transition(in_trans) {}
  ~NegateTransition() override { delete transition; }

  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    return !transition->isAvailable(ecs, entity);
  }
};

class AndTransition : public StateTransition
{
  const StateTransition *lhs; // we own it
  const StateTransition *rhs; // we own it
public:
  AndTransition(const StateTransition *in_lhs, const StateTransition *in_rhs) : lhs(in_lhs), rhs(in_rhs) {}
  ~AndTransition() override
  {
    delete lhs;
    delete rhs;
  }

  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    return lhs->isAvailable(ecs, entity) && rhs->isAvailable(ecs, entity);
  }
};

class OrTransition : public StateTransition
{
  const StateTransition *lhs; // we own it
  const StateTransition *rhs; // we own it
public:
  OrTransition(const StateTransition *in_lhs, const StateTransition *in_rhs) : lhs(in_lhs), rhs(in_rhs) {}
  ~OrTransition() override
  {
    delete lhs;
    delete rhs;
  }

  bool isAvailable(flecs::world &ecs, flecs::entity entity) const override
  {
    return lhs->isAvailable(ecs, entity) || rhs->isAvailable(ecs, entity);
  }
};


// states
State *create_attack_enemy_state()
{
  return new AttackEnemyState();
}
State *create_move_to_enemy_state()
{
  return new MoveToEnemyState();
}

State *create_flee_from_enemy_state()
{
  return new FleeFromEnemyState();
}


State *create_patrol_state(float patrol_dist)
{
  return new PatrolState(patrol_dist);
}

State *create_move_to_teammate_state(float patrol_dist)
{
  return new MoveToTeammatelState(patrol_dist);
}

State *create_nop_state()
{
  return new NopState();
}

State *create_selfheal_state(float heal_amount)
{
  return new SelfHealState(heal_amount);
}

State *create_heal_teammate_state(float heal_amount, float triggerDist)
{
  return new TeammatefHealState(heal_amount, triggerDist);
}

// transitions
StateTransition *create_enemy_available_transition(float dist)
{
  return new EnemyAvailableTransition(dist);
}

StateTransition *create_teammate_available_transition(float dist)
{
  return new TeammateAvailableTransition(dist);
}

StateTransition *create_enemy_reachable_transition()
{
  return new EnemyReachableTransition();
}

StateTransition *create_hitpoints_less_than_transition(float thres)
{
  return new HitpointsLessThanTransition(thres);
}

StateTransition *create_must_heal_teammate_transition(float thres, float dist)
{
  return new MustHealTeammateTransition(thres, dist);
}

StateTransition *create_negate_transition(StateTransition *in)
{
  return new NegateTransition(in);
}
StateTransition *create_and_transition(StateTransition *lhs, StateTransition *rhs)
{
  return new AndTransition(lhs, rhs);
}

StateTransition *create_or_transition(StateTransition *lhs, StateTransition *rhs)
{
  return new OrTransition(lhs, rhs);
}
