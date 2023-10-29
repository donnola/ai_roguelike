#include "dungeonUtils.h"
#include "raylib.h"
#include "math.h"

Position dungeon::find_walkable_tile(flecs::world &ecs)
{
  static auto dungeonDataQuery = ecs.query<const DungeonData>();

  Position res{0, 0};
  dungeonDataQuery.each([&](const DungeonData &dd)
  {
    // prebuild all walkable and get one of them
    std::vector<Position> posList;
    for (size_t y = 0; y < dd.height; ++y)
      for (size_t x = 0; x < dd.width; ++x)
        if (dd.tiles[y * dd.width + x] == dungeon::floor)
          posList.push_back(Position{int(x), int(y)});
    size_t rndIdx = size_t(GetRandomValue(0, int(posList.size()) - 1));
    res = posList[rndIdx];
  });
  return res;
}

Position dungeon::find_neares_unexplore_tile(flecs::world &ecs, Position pos)
{
  static auto dungeonDataQuery = ecs.query<const DungeonData>();

  Position res{-1, -1};
  float closestDist = FLT_MAX;
  dungeonDataQuery.each([&](const DungeonData &dd)
  {
    for (size_t y = 0; y < dd.height; ++y)
    {
      for (size_t x = 0; x < dd.width; ++x)
      {
        if (dd.tilesExplore[y * dd.width + x] == dungeon::unexplored && dd.tiles[y * dd.width + x] == floor)
        {
          float dist_to_tile = dist(pos, Position{int(x), int(y)});
          if (dist_to_tile < closestDist)
          {
            closestDist = dist_to_tile;
            res = Position{int(x), int(y)};
          }
        }
      }
    }
  });
  return res;
}

bool dungeon::is_tile_walkable(flecs::world &ecs, Position pos)
{
  static auto dungeonDataQuery = ecs.query<const DungeonData>();

  bool res = false;
  dungeonDataQuery.each([&](const DungeonData &dd)
  {
    if (pos.x < 0 || pos.x >= int(dd.width) ||
        pos.y < 0 || pos.y >= int(dd.height))
      return false;
    res = dd.tiles[size_t(pos.y) * dd.width + size_t(pos.x)] == dungeon::floor;
  });
  return res;
}

bool dungeon::is_tile_visible(flecs::world &ecs, Position pos_from, Position pos_to)
{
  static auto dungeonDataQuery = ecs.query<const DungeonData>();
  bool res = true;
  dungeonDataQuery.each([&](const DungeonData &dd)
  {
    Position new_tile_pos = pos_from;
    while (new_tile_pos != pos_to)
    {
      int deltaX = pos_to.x - new_tile_pos.x;
      int deltaY = pos_to.y - new_tile_pos.y;
      if (abs(deltaX) > abs(deltaY))
        new_tile_pos.x += deltaX > 0 ? 1 : -1;
      else
        new_tile_pos.y += deltaY < 0 ? -1 : +1;
      if (!is_tile_walkable(ecs, new_tile_pos))
      {
        res = false;
        return;
      }
    }
  });
  return res;
}