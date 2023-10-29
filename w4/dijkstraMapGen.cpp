#include "dijkstraMapGen.h"
#include "ecsTypes.h"
#include "dungeonUtils.h"
#include "math.h"

template<typename Callable>
static void query_dungeon_data(flecs::world &ecs, Callable c)
{
  static auto dungeonDataQuery = ecs.query<const DungeonData>();

  dungeonDataQuery.each(c);
}

template<typename Callable>
static void query_characters_positions(flecs::world &ecs, Callable c)
{
  static auto characterPositionQuery = ecs.query<const Position, const Team>();

  characterPositionQuery.each(c);
}

constexpr float invalid_tile_value = 1e5f;

static void init_tiles(std::vector<float> &map, const DungeonData &dd)
{
  map.resize(dd.width * dd.height);
  for (float &v : map)
    v = invalid_tile_value;
}

// scan version, could be implemented as Dijkstra version as well
static void process_dmap(std::vector<float> &map, const DungeonData &dd)
{
  bool done = false;
  auto getMapAt = [&](size_t x, size_t y, float def)
  {
    if (x < dd.width && y < dd.width && dd.tiles[y * dd.width + x] == dungeon::floor)
      return map[y * dd.width + x];
    return def;
  };
  auto getMinNei = [&](size_t x, size_t y)
  {
    float val = map[y * dd.width + x];
    val = std::min(val, getMapAt(x - 1, y + 0, val));
    val = std::min(val, getMapAt(x + 1, y + 0, val));
    val = std::min(val, getMapAt(x + 0, y - 1, val));
    val = std::min(val, getMapAt(x + 0, y + 1, val));
    return val;
  };
  while (!done)
  {
    done = true;
    for (size_t y = 0; y < dd.height; ++y)
      for (size_t x = 0; x < dd.width; ++x)
      {
        const size_t i = y * dd.width + x;
        if (dd.tiles[i] != dungeon::floor)
          continue;
        const float myVal = getMapAt(x, y, invalid_tile_value);
        const float minVal = getMinNei(x, y);
        if (minVal < myVal - 1.f)
        {
          map[i] = minVal + 1.f;
          done = false;
        }
      }
  }
}

void dmaps::gen_player_approach_map(flecs::world &ecs, std::vector<float> &map)
{
  query_dungeon_data(ecs, [&](const DungeonData &dd)
  {
    init_tiles(map, dd);
    query_characters_positions(ecs, [&](const Position &pos, const Team &t)
    {
      if (t.team == 0) // player team hardcode
        map[pos.y * dd.width + pos.x] = 0.f;
    });
    process_dmap(map, dd);
  });
}

void dmaps::gen_player_flee_map(flecs::world &ecs, std::vector<float> &map)
{
  gen_player_approach_map(ecs, map);
  for (float &v : map)
    if (v < invalid_tile_value)
      v *= -1.2f;
  query_dungeon_data(ecs, [&](const DungeonData &dd)
  {
    process_dmap(map, dd);
  });
}

void dmaps::gen_hive_pack_map(flecs::world &ecs, std::vector<float> &map)
{
  static auto hiveQuery = ecs.query<const Position, const Hive>();
  query_dungeon_data(ecs, [&](const DungeonData &dd)
  {
    init_tiles(map, dd);
    hiveQuery.each([&](const Position &pos, const Hive &)
    {
      map[pos.y * dd.width + pos.x] = 0.f;
    });
    process_dmap(map, dd);
  });
}

void dmaps::gen_magician_map(flecs::world &ecs, std::vector<float> &map)
{
  query_dungeon_data(ecs, [&](const DungeonData &dd)
  {
    std::vector<float> temp_map;
    gen_player_approach_map(ecs, temp_map);
    init_tiles(map, dd);
    query_characters_positions(ecs, [&](const Position &pos, const Team &t)
    {
      if (t.team == 0)
      {
        for (int y = -4; y <= 4; ++y)
        {
          for (int x = -4; x <= 4; ++x)
          {
            Position tile_pos = Position{pos.x + x, pos.y + y};
            if (temp_map[tile_pos.y * dd.width + tile_pos.x] == 4 && dungeon::is_tile_visible(ecs, tile_pos, pos))
            {
              map[tile_pos.y * dd.width + tile_pos.x] = 0;
            }
            else if (temp_map[tile_pos.y * dd.width + tile_pos.x] == 4 && !dungeon::is_tile_visible(ecs, tile_pos, pos))
            {
              Position last_visible_tile = tile_pos;
              while (!dungeon::is_tile_visible(ecs, last_visible_tile, pos) && last_visible_tile != pos)
              {
                bool find_tile = false;
                for (int k = -1; k <= 1; ++k)
                {
                  for (int l = -1; l <= 1; ++l)
                  {
                    if (temp_map[(last_visible_tile.y + k) * dd.width + last_visible_tile.x + l] < temp_map[last_visible_tile.y * dd.width + last_visible_tile.x])
                    { 
                      last_visible_tile.x += l;
                      last_visible_tile.y += k;
                      find_tile = true;
                      break;
                    }
                  }
                  if (find_tile)
                    break;
                }
              }
              map[last_visible_tile.y * dd.width + last_visible_tile.x] = 0;
            }
          }
        }
      }
    });
    process_dmap(map, dd);
  });
}

void dmaps::gen_explore_map(flecs::world &ecs, std::vector<float> &map)
{
  query_dungeon_data(ecs, [&](const DungeonData &dd)
  {
    init_tiles(map, dd);
    static auto playerQuery = ecs.query<const Position, const IsPlayer, NextExplorePos>();
    playerQuery.each([&](const Position &pos, const IsPlayer &, NextExplorePos &nextPos)
    {
      if (int(dist(pos, nextPos)) == 0 || (nextPos.x == -1 || nextPos.y == -1))
      {
        Position nex_tile = dungeon::find_neares_unexplore_tile(ecs, pos);
        if (nex_tile.x >= 0 && nex_tile.x < int(dd.width) && nex_tile.y >= 0 && nex_tile.y < int(dd.height))
        {
          map[nex_tile.y * dd.width + nex_tile.x] = 0.f;
          nextPos = NextExplorePos{nex_tile.x, nex_tile.y};
        }
      }
      else
      {
        map[nextPos.y * dd.width + nextPos.x] = 0.f;
      } 
    });
    process_dmap(map, dd);
  });
}

void dmaps::gen_teamamte_map(flecs::world &ecs, std::vector<float> &map)
{
  query_dungeon_data(ecs, [&](const DungeonData &dd)
  {
    init_tiles(map, dd);
    static auto playerQuery = ecs.query<const Position, const Team>();
    playerQuery.each([&](flecs::entity e, const Position &pos, const Team &t)
    {
      if (t.team == 1 && !e.has<IsMag>())
        map[pos.y * dd.width + pos.x] = 0.f;
    });
    process_dmap(map, dd);
  });
}