#include "ecsTypes.h"
#include "dmapFollower.h"
#include <cmath>

void process_dmap_followers(flecs::world &ecs)
{
  static auto processDmapFollowers = ecs.query<const Position, Action, const DmapWeights>();
  static auto dungeonDataQuery = ecs.query<const DungeonData>();
  static auto playerQuery = ecs.query<const IsPlayer>();

  flecs::entity entP;
  playerQuery.each([&](flecs::entity e, const IsPlayer &)
  {
    entP = e;
  });

  auto get_dmap_at = [&](const DijkstraMapData &dmap, const DungeonData &dd, size_t x, size_t y, float mult, float pow)
  {
    const float v = dmap.map[y * dd.width + x];
    if (v < 1e5f)
      return powf(v * mult, pow);
    return v;
  };
  dungeonDataQuery.each([&](const DungeonData &dd)
  {
    processDmapFollowers.each([&](flecs::entity e, const Position &pos, Action &act, const DmapWeights &wt)
    {
      if (e == entP)
      {
        return;
      }
      float moveWeights[EA_MOVE_END];
      for (size_t i = 0; i < EA_MOVE_END; ++i)
        moveWeights[i] = 0.f;
      for (const auto &pair : wt.weights)
      {
        if (!pair.second.usefunc(e))
        {
          continue;
        }
        ecs.entity(pair.first.c_str()).get([&](const DijkstraMapData &dmap)
        {
          moveWeights[EA_NOP]         += get_dmap_at(dmap, dd, pos.x+0, pos.y+0, pair.second.mult, pair.second.pow);
          moveWeights[EA_MOVE_LEFT]   += get_dmap_at(dmap, dd, pos.x-1, pos.y+0, pair.second.mult, pair.second.pow);
          moveWeights[EA_MOVE_RIGHT]  += get_dmap_at(dmap, dd, pos.x+1, pos.y+0, pair.second.mult, pair.second.pow);
          moveWeights[EA_MOVE_UP]     += get_dmap_at(dmap, dd, pos.x+0, pos.y-1, pair.second.mult, pair.second.pow);
          moveWeights[EA_MOVE_DOWN]   += get_dmap_at(dmap, dd, pos.x+0, pos.y+1, pair.second.mult, pair.second.pow);
        });
      }
      float minWt = moveWeights[EA_NOP];
      for (size_t i = 0; i < EA_MOVE_END; ++i)
        if (moveWeights[i] < minWt)
        {
          minWt = moveWeights[i];
          act.action = i;
        }
    });
  });
}

void next_player_auto_action(flecs::world &ecs)
{
  float moveWeights[EA_MOVE_END];
  for (size_t i = 0; i < EA_MOVE_END; ++i)
    moveWeights[i] = 0.f;
  
  auto get_dmap_at = [&](const DijkstraMapData &dmap, const DungeonData &dd, size_t x, size_t y, float mult, float pow)
  {
    const float v = dmap.map[y * dd.width + x];
    if (v < 1e5f)
      return powf(v * mult, pow);
    return v;
  };

  static auto processDmapFollowers = ecs.query<const Position, Action, const DmapWeights, const IsPlayer>();
  static auto dungeonDataQuery = ecs.query<const DungeonData>();
  dungeonDataQuery.each([&](const DungeonData &dd)
  {
    processDmapFollowers.each([&](const Position &pos, Action &act, const DmapWeights &wt, const IsPlayer &)
    {
      for (const auto &pair : wt.weights)
      {
        ecs.entity(pair.first.c_str()).get([&](const DijkstraMapData &dmap)
        {
          moveWeights[EA_NOP]         += get_dmap_at(dmap, dd, pos.x+0, pos.y+0, pair.second.mult, pair.second.pow);
          moveWeights[EA_MOVE_LEFT]   += get_dmap_at(dmap, dd, pos.x-1, pos.y+0, pair.second.mult, pair.second.pow);
          moveWeights[EA_MOVE_RIGHT]  += get_dmap_at(dmap, dd, pos.x+1, pos.y+0, pair.second.mult, pair.second.pow);
          moveWeights[EA_MOVE_UP]     += get_dmap_at(dmap, dd, pos.x+0, pos.y-1, pair.second.mult, pair.second.pow);
          moveWeights[EA_MOVE_DOWN]   += get_dmap_at(dmap, dd, pos.x+0, pos.y+1, pair.second.mult, pair.second.pow);
        });
      }
      act.action = EA_PASS;
      float minWt = moveWeights[EA_NOP];
      for (size_t i = 1; i < EA_MOVE_END; ++i)
        if (moveWeights[i] < minWt)
        {
          minWt = moveWeights[i];
          act.action = i;
        }
    });
  });
}