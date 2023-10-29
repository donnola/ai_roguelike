//
#include "raylib.h"
#include <flecs.h>
#include <algorithm>
#include "ecsTypes.h"
#include "roguelike.h"
#include "dungeonGen.h"
#include "dungeonUtils.h"

static void update_camera(Camera2D &cam, flecs::world &ecs)
{
  static auto playerQuery = ecs.query<const Position, const IsPlayer>();

  playerQuery.each([&](const Position &pos, const IsPlayer &)
  {
    cam.target.x += (pos.x * tile_size - cam.target.x) * 0.1f;
    cam.target.y += (pos.y * tile_size - cam.target.y) * 0.1f;
  });
}

static void update_vis_map(flecs::world &ecs, flecs::entity wallTex, flecs::entity floorTex)
{
  static auto playerQuery = ecs.query<const Position, const IsPlayer>();

  static auto dungeonDataQuery = ecs.query<DungeonData>();

  playerQuery.each([&](const Position &pos, const IsPlayer &)
  {
    dungeonDataQuery.each([&](DungeonData &dd)
    {
      for (int i = -3; i <= 3; ++i)
      {
        for (int j = -3; j <= 3; ++j)
        {
          if (pos.x + j < 0 || pos.x + j >= int(dd.width) || pos.y + i < 0 || pos.y + i >= int(dd.height))
            continue;
        
          if (abs(i) + abs(j) <= 3 && dd.tilesExplore[(pos.y + i) * dd.width + pos.x + j] == dungeon::unexplored)
          {
            dd.tilesExplore[(pos.y + i) * dd.width + pos.x + j] = dungeon::explored;
            char tile = dd.tiles[(pos.y + i) * dd.width + pos.x + j];
            static auto tilesQuery = ecs.query<const Position, const BackgroundTile>();
            flecs::entity tileEntity;
            tilesQuery.each([&](flecs::entity entity, const Position &tpos, const BackgroundTile &)
            {
              if (tpos == Position{int(pos.x + j), int(pos.y + i)})
              {
                tileEntity = entity;
              }
            });
            tileEntity.set(Color{255, 255, 255, 255});
            if (tile == dungeon::floor)
            {
              tileEntity.add<TextureSource>(floorTex);
            }
            else if (tile == dungeon::wall)
            {
              tileEntity.add<TextureSource>(wallTex);
            }
          }
        }
      }
    });
  });
}

int main(int /*argc*/, const char ** /*argv*/)
{
  int width = 1920;
  int height = 1080;
  InitWindow(width, height, "w3 AI MIPT");

  const int scrWidth = GetMonitorWidth(0);
  const int scrHeight = GetMonitorHeight(0);
  if (scrWidth < width || scrHeight < height)
  {
    width = std::min(scrWidth, width);
    height = std::min(scrHeight - 150, height);
    SetWindowSize(width, height);
  }

  flecs::world ecs;
  {
    constexpr size_t dungWidth = 50;
    constexpr size_t dungHeight = 50;
    char *tiles = new char[dungWidth * dungHeight];
    char *tilesExplore = new char[dungWidth * dungHeight];
    gen_drunk_dungeon(tiles, tilesExplore, dungWidth, dungHeight);
    init_dungeon(ecs, tiles, tilesExplore, dungWidth, dungHeight);
  }
  flecs::entity wallTex = ecs.entity("wall_tex")
    .set(Texture2D{LoadTexture("assets/wall.png")});
  flecs::entity floorTex = ecs.entity("floor_tex")
    .set(Texture2D{LoadTexture("assets/floor.png")});
  init_roguelike(ecs);
  update_vis_map(ecs, wallTex, floorTex);

  Camera2D camera = { {0, 0}, {0, 0}, 0.f, 1.f };
  camera.target = Vector2{ 0.f, 0.f };
  camera.offset = Vector2{ width * 0.5f, height * 0.5f };
  camera.rotation = 0.f;
  camera.zoom = 0.125f;

  SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
  while (!WindowShouldClose())
  {
    process_turn(ecs);
    update_vis_map(ecs, wallTex, floorTex);
    update_camera(camera, ecs);

    BeginDrawing();
      ClearBackground(BLACK);
      BeginMode2D(camera);
        ecs.progress();
      EndMode2D();
      print_stats(ecs);
      // Advance to next frame. Process submitted rendering primitives.
    EndDrawing();
  }

  CloseWindow();

  return 0;
}
