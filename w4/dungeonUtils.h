#pragma once
#include "ecsTypes.h"
#include <flecs.h>

namespace dungeon
{
  constexpr char wall = '#';
  constexpr char floor = ' ';
  constexpr char unexplored = '?';
  constexpr char explored = ' ';

  Position find_walkable_tile(flecs::world &ecs);
  bool is_tile_walkable(flecs::world &ecs, Position pos);
  Position find_neares_unexplore_tile(flecs::world &ecs, Position pos);
  bool is_tile_visible(flecs::world &ecs, Position pos_from, Position pos_to);
};
