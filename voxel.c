#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define GRID_SIZE 50
#define MAX_COLORS 10

typedef struct
{
  unsigned char r, g, b, a;
  int filled;
} Voxel;

static Voxel grid[GRID_SIZE][GRID_SIZE][GRID_SIZE];

static Color palette[MAX_COLORS] = { { 229, 76, 76, 255 },  { 232, 125, 42, 255 },  { 232, 198, 42, 255 },
                                     { 76, 232, 106, 255 }, { 42, 125, 232, 255 },  { 155, 76, 232, 255 },
                                     { 232, 76, 168, 255 }, { 255, 255, 255, 255 }, { 0, 0, 0, 255 } };

static int currentColor = 7;
static int mode = 0;

int
InBounds (int x, int y, int z)
{
  return x >= 0 && y >= 0 && z >= 0 && x < GRID_SIZE && y < GRID_SIZE && z < GRID_SIZE;
}

int
IsFilled (int x, int y, int z)
{
  if (!InBounds (x, y, z))
    return 0;
  return grid[x][y][z].filled;
}

// Draw only faces that are exposed (neighbour is empty)
void
DrawVoxel (int x, int y, int z, Color color)
{
  float fx = (float)x, fy = (float)y, fz = (float)z;
  Color top = ColorBrightness (color, 0.15f);
  Color mid = color;
  Color side = ColorBrightness (color, -0.10f);
  Color bot = ColorBrightness (color, -0.20f);

  // rlgl uses CCW winding viewed from outside the face
  // +Y top (normal points up, viewed from above: CCW = NW NE SE SW)
  if (!IsFilled (x, y + 1, z))
    {
      rlBegin (RL_QUADS);
      rlColor4ub (top.r, top.g, top.b, top.a);
      rlVertex3f (fx, fy + 1, fz + 1);
      rlVertex3f (fx + 1, fy + 1, fz + 1);
      rlVertex3f (fx + 1, fy + 1, fz);
      rlVertex3f (fx, fy + 1, fz);
      rlEnd ();
    }
  // -Y bottom (normal points down, viewed from below: CCW)
  if (!IsFilled (x, y - 1, z))
    {
      rlBegin (RL_QUADS);
      rlColor4ub (bot.r, bot.g, bot.b, bot.a);
      rlVertex3f (fx, fy, fz);
      rlVertex3f (fx + 1, fy, fz);
      rlVertex3f (fx + 1, fy, fz + 1);
      rlVertex3f (fx, fy, fz + 1);
      rlEnd ();
    }
  // +Z front (normal +Z, viewed from front)
  if (!IsFilled (x, y, z + 1))
    {
      rlBegin (RL_QUADS);
      rlColor4ub (mid.r, mid.g, mid.b, mid.a);
      rlVertex3f (fx + 1, fy, fz + 1);
      rlVertex3f (fx + 1, fy + 1, fz + 1);
      rlVertex3f (fx, fy + 1, fz + 1);
      rlVertex3f (fx, fy, fz + 1);
      rlEnd ();
    }
  // -Z back (normal -Z, viewed from back)
  if (!IsFilled (x, y, z - 1))
    {
      rlBegin (RL_QUADS);
      rlColor4ub (mid.r, mid.g, mid.b, mid.a);
      rlVertex3f (fx, fy, fz);
      rlVertex3f (fx, fy + 1, fz);
      rlVertex3f (fx + 1, fy + 1, fz);
      rlVertex3f (fx + 1, fy, fz);
      rlEnd ();
    }
  // +X right (normal +X, viewed from right)
  if (!IsFilled (x + 1, y, z))
    {
      rlBegin (RL_QUADS);
      rlColor4ub (side.r, side.g, side.b, side.a);
      rlVertex3f (fx + 1, fy, fz + 1);
      rlVertex3f (fx + 1, fy + 1, fz + 1);
      rlVertex3f (fx + 1, fy + 1, fz);
      rlVertex3f (fx + 1, fy, fz);
      rlEnd ();
    }
  // -X left (normal -X, viewed from left)
  if (!IsFilled (x - 1, y, z))
    {
      rlBegin (RL_QUADS);
      rlColor4ub (side.r, side.g, side.b, side.a);
      rlVertex3f (fx, fy, fz);
      rlVertex3f (fx, fy + 1, fz);
      rlVertex3f (fx, fy + 1, fz + 1);
      rlVertex3f (fx, fy, fz + 1);
      rlEnd ();
    }
}

float
RayAABB (Vector3 ro, Vector3 rd, Vector3 bmin, Vector3 bmax)
{
  float tmin = -1e9f, tmax = 1e9f;
  float *o = (float *)&ro, *d = (float *)&rd;
  float *mn = (float *)&bmin, *mx = (float *)&bmax;
  for (int i = 0; i < 3; i++)
    {
      if (fabsf (d[i]) < 1e-6f)
        {
          if (o[i] < mn[i] || o[i] > mx[i])
            return -1.0f;
        }
      else
        {
          float t1 = (mn[i] - o[i]) / d[i], t2 = (mx[i] - o[i]) / d[i];
          if (t1 > t2)
            {
              float tmp = t1;
              t1 = t2;
              t2 = tmp;
            }
          if (t1 > tmin)
            tmin = t1;
          if (t2 < tmax)
            tmax = t2;
          if (tmin > tmax)
            return -1.0f;
        }
    }
  return tmin > 0 ? tmin : (tmax > 0 ? tmax : -1.0f);
}

typedef struct
{
  int x, y, z, valid;
} Hit;

Hit
Raycast (Camera3D cam)
{
  Hit r = { 0 };
  int sw = GetScreenWidth (), sh = GetScreenHeight ();
  Ray ray = GetMouseRay ((Vector2){ sw / 2.0f, sh / 2.0f }, cam);
  float best = 1e9f;
  for (int x = 0; x < GRID_SIZE; x++)
    for (int y = 0; y < GRID_SIZE; y++)
      for (int z = 0; z < GRID_SIZE; z++)
        {
          if (!grid[x][y][z].filled)
            continue;
          Vector3 bmin = { x, y, z }, bmax = { x + 1, y + 1, z + 1 };
          float t = RayAABB (ray.position, ray.direction, bmin, bmax);
          if (t > 0 && t < best)
            {
              best = t;
              r.x = x;
              r.y = y;
              r.z = z;
              r.valid = 1;
            }
        }
  return r;
}

Hit
RaycastPlace (Camera3D cam)
{
  Hit r = { 0 };
  int sw = GetScreenWidth (), sh = GetScreenHeight ();
  Ray ray = GetMouseRay ((Vector2){ sw / 2.0f, sh / 2.0f }, cam);
  float best = 1e9f;
  int bx = 0, by = 0, bz = 0;
  Vector3 bhit = { 0 };
  for (int x = 0; x < GRID_SIZE; x++)
    for (int y = 0; y < GRID_SIZE; y++)
      for (int z = 0; z < GRID_SIZE; z++)
        {
          if (!grid[x][y][z].filled)
            continue;
          Vector3 bmin = { x, y, z }, bmax = { x + 1, y + 1, z + 1 };
          float t = RayAABB (ray.position, ray.direction, bmin, bmax);
          if (t > 0 && t < best)
            {
              best = t;
              bx = x;
              by = y;
              bz = z;
              bhit = Vector3Add (ray.position, Vector3Scale (ray.direction, t - 0.001f));
            }
        }
  if (best >= 1e9f)
    return r;
  int fx = bx, fy = by, fz = bz;
  Vector3 center = { bx + 0.5f, by + 0.5f, bz + 0.5f };
  Vector3 d = Vector3Subtract (bhit, center);
  float ax = fabsf (d.x), ay = fabsf (d.y), az = fabsf (d.z);
  if (ax >= ay && ax >= az)
    fx += d.x > 0 ? 1 : -1;
  else if (ay >= ax && ay >= az)
    fy += d.y > 0 ? 1 : -1;
  else
    fz += d.z > 0 ? 1 : -1;
  if (!InBounds (fx, fy, fz) || grid[fx][fy][fz].filled)
    return r;
  r.x = fx;
  r.y = fy;
  r.z = fz;
  r.valid = 1;
  return r;
}

void
InitFloor (void)
{
  for (int x = 0; x < GRID_SIZE; x++)
    for (int z = 0; z < GRID_SIZE; z++)
      {
        grid[x][0][z] = (Voxel){ 107, 166, 89, 255, 1 };
      }
}

int
main (void)
{
  const int SW = 1024, SH = 768;
  InitWindow (SW, SH, "Voxel Editor");
  SetTargetFPS (60);
  DisableCursor ();

  // camera angles
  // look from (24,12,24) toward (8,3,8) — center of grid
  float dx = (GRID_SIZE / 2.0f) - (GRID_SIZE / 2.0f + 14);
  float dz = (GRID_SIZE / 2.0f) - (GRID_SIZE / 2.0f + 14);
  float dy = 3.0f - 12.0f;
  float yaw = atan2f (dx, dz);
  float pitch = atan2f (dy, sqrtf (dx * dx + dz * dz));
  float camSpeed = 8.0f;
  float mouseSens = 0.002f;

  Vector3 pos = { GRID_SIZE / 2.0f + 14, 12, GRID_SIZE / 2.0f + 14 };

  Camera3D camera = { 0 };
  camera.up = (Vector3){ 0, 1, 0 };
  camera.fovy = 60.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  memset (grid, 0, sizeof (grid));
  InitFloor ();

  int showCursor = 0;

  while (!WindowShouldClose ())
    {
      float dt = GetFrameTime ();

      // TAB toggles cursor / camera
      if (IsKeyPressed (KEY_TAB))
        {
          showCursor = !showCursor;
          if (showCursor)
            EnableCursor ();
          else
            DisableCursor ();
        }

      if (!showCursor)
        {
          // mouse look
          Vector2 md = GetMouseDelta ();
          yaw -= md.x * mouseSens;
          pitch -= md.y * mouseSens;
          if (pitch > 1.5f)
            pitch = 1.5f;
          if (pitch < -1.5f)
            pitch = -1.5f;
        }

      // forward vector from yaw/pitch
      Vector3 fwd = { cosf (pitch) * sinf (yaw), sinf (pitch), cosf (pitch) * cosf (yaw) };
      Vector3 right = Vector3Normalize (Vector3CrossProduct (fwd, (Vector3){ 0, 1, 0 }));
      Vector3 up2 = { 0, 1, 0 };

      if (IsKeyDown (KEY_W))
        pos = Vector3Add (pos, Vector3Scale (fwd, camSpeed * dt));
      if (IsKeyDown (KEY_S))
        pos = Vector3Add (pos, Vector3Scale (fwd, -camSpeed * dt));
      if (IsKeyDown (KEY_A))
        pos = Vector3Add (pos, Vector3Scale (right, -camSpeed * dt));
      if (IsKeyDown (KEY_D))
        pos = Vector3Add (pos, Vector3Scale (right, camSpeed * dt));
      if (IsKeyDown (KEY_SPACE))
        pos.y += camSpeed * dt;
      if (IsKeyDown (KEY_LEFT_SHIFT))
        pos.y -= camSpeed * dt;

      camera.position = pos;
      camera.target = Vector3Add (pos, fwd);
      camera.up = up2;

      // place / remove
      if (IsMouseButtonPressed (MOUSE_BUTTON_LEFT) && !showCursor)
        {
          if (mode == 0)
            {
              Hit h = RaycastPlace (camera);
              if (h.valid)
                {
                  Color c = palette[currentColor];
                  grid[h.x][h.y][h.z] = (Voxel){ c.r, c.g, c.b, c.a, 1 };
                }
            }
          else
            {
              Hit h = Raycast (camera);
              if (h.valid)
                grid[h.x][h.y][h.z].filled = 0;
            }
        }

      if (IsKeyPressed (KEY_E))
        mode = !mode;
      for (int i = 0; i < 9; i++)
        if (IsKeyPressed (KEY_ONE + i))
          currentColor = i;
      if (IsKeyPressed (KEY_C))
        {
          memset (grid, 0, sizeof (grid));
          InitFloor ();
        }

      BeginDrawing ();
      ClearBackground ((Color){ 30, 30, 35, 255 });
      BeginMode3D (camera);

      rlDrawRenderBatchActive ();
      rlDisableBackfaceCulling ();
      for (int x = 0; x < GRID_SIZE; x++)
        for (int y = 0; y < GRID_SIZE; y++)
          for (int z = 0; z < GRID_SIZE; z++)
            {
              if (!grid[x][y][z].filled)
                continue;
              Color c = { grid[x][y][z].r, grid[x][y][z].g, grid[x][y][z].b, grid[x][y][z].a };
              DrawVoxel (x, y, z, c);
            }
      rlDrawRenderBatchActive ();
      rlEnableBackfaceCulling ();

      if (!showCursor)
        {
          if (mode == 0)
            {
              Hit h = RaycastPlace (camera);
              if (h.valid)
                DrawCubeWires ((Vector3){ h.x + 0.5f, h.y + 0.5f, h.z + 0.5f }, 1.02f, 1.02f, 1.02f, WHITE);
            }
          else
            {
              Hit h = Raycast (camera);
              if (h.valid)
                DrawCubeWires ((Vector3){ h.x + 0.5f, h.y + 0.5f, h.z + 0.5f }, 1.02f, 1.02f, 1.02f, RED);
            }
        }

      EndMode3D ();

      // crosshair
      int sw2 = GetScreenWidth (), sh2 = GetScreenHeight ();
      DrawLine (sw2 / 2 - 12, sh2 / 2, sw2 / 2 + 12, sh2 / 2, WHITE);
      DrawLine (sw2 / 2, sh2 / 2 - 12, sw2 / 2, sh2 / 2 + 12, WHITE);

      // mode
      const char *ms = mode == 0 ? "[ PLACE ]" : "[ REMOVE ]";
      DrawText (ms, 10, 10, 20, mode == 0 ? GREEN : RED);
      DrawText ("WASD+mouse - move  |  Space/Shift - up/down  |  E - mode  |  "
                "TAB - cursor  |  C - clear",
                10, 36, 13, LIGHTGRAY);

      // palette
      for (int i = 0; i < MAX_COLORS; i++)
        {
          Rectangle r = { 10 + i * 32.0f, sh2 - 48.0f, 26, 26 };
          DrawRectangleRec (r, palette[i]);
          DrawRectangleLinesEx (r, i == currentColor ? 3 : 1, i == currentColor ? WHITE : DARKGRAY);
          if (i < 9)
            {
              char buf[3];
              sprintf (buf, "%d", i + 1);
              DrawText (buf, (int)r.x + 8, (int)r.y - 16, 13, LIGHTGRAY);
            }
        }

      DrawFPS (SW - 80, 10);
      EndDrawing ();
    }

  CloseWindow ();
  return 0;
}
