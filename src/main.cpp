// MinecraftPSP - main.cpp
// PSP Entry point, basic game loop

#include <pspctrl.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspkernel.h>
#include <psppower.h>

#include "input/PSPInput.h"
#include "render/ChunkRenderer.h"
#include "render/CloudRenderer.h"
#include "render/PSPRenderer.h"
#include "render/SkyRenderer.h"
#include "render/TextureAtlas.h"
#include "world/Blocks.h"
#include "world/Mth.h"
#include "world/Random.h"
#include <math.h>

// PSP module metadata
PSP_MODULE_INFO("MinecraftPSP", PSP_MODULE_USER, 1, 0);
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER | PSP_THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024); // Use all available RAM minus 1MB for the kernel

// Exit callback (HOME button)
int exit_callback(int arg1, int arg2, void *common) {
  sceKernelExitGame();
  return 0;
}

int callback_thread(SceSize args, void *argp) {
  int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
  sceKernelRegisterExitCallback(cbid);
  sceKernelSleepThreadCB();
  return 0;
}

void setup_callbacks() {
  int thid = sceKernelCreateThread("update_thread", callback_thread, 0x11,
                                   0xFA0, PSP_THREAD_ATTR_USER, NULL);
  if (thid >= 0)
    sceKernelStartThread(thid, 0, NULL);
}

// ====================================================
// Simple player state
// ====================================================
struct PlayerState {
  float x, y, z;          // position
  float yaw, pitch;       // camera rotation (degrees)
  float velY;             // vertical velocity (gravity)
  bool onGround;
  bool isFlying;          // creative flight active
  float jumpDoubleTapTimer; // countdown for double-tap detection
};

// ====================================================
// Game state global
// ====================================================
static PlayerState g_player;
static Level *g_level = nullptr;
static SkyRenderer *g_skyRenderer = nullptr;
static CloudRenderer *g_cloudRenderer = nullptr;
static ChunkRenderer *g_chunkRenderer = nullptr;
static TextureAtlas *g_atlas = nullptr;

// ====================================================
// Init
// ====================================================
static bool game_init() {
  // Overclock PSP to max for performance
  scePowerSetClockFrequency(333, 333, 166);

  // Init block tables
  Blocks_Init();

  // Init sin/cos lookup table
  Mth::init();

  // Init PSP renderer (sceGu)
  if (!PSPRenderer_Init())
    return false;

  // Load terrain.png from MS0:/PSP/GAME/MinecraftPSP/res/
  g_atlas = new TextureAtlas();
  if (!g_atlas->load("res/terrain.png"))
    return false;

  g_level = new Level();
  g_skyRenderer = new SkyRenderer(g_level);
  g_cloudRenderer = new CloudRenderer(g_level);

  // Init chunk renderer
  g_chunkRenderer = new ChunkRenderer(g_atlas);
  g_chunkRenderer->setLevel(g_level);

  // Generate a test world
  Random rng(12345LL);
  g_level->generate(&rng);

  // Player start position
  g_player.x = 8.0f;
  g_player.y = 65.0f;
  g_player.z = 8.0f;
  g_player.yaw = 0.0f;
  g_player.pitch = 0.0f;
  g_player.velY = 0.0f;
  g_player.onGround = false;
  g_player.isFlying = false;
  g_player.jumpDoubleTapTimer = 0.0f;

  return true;
}

// ====================================================
// Game Loop
// ====================================================
static void game_update(float dt) {
  PSPInput_Update();
  if (g_level) {
    g_level->tick();
  }

  float moveSpeed = (g_player.isFlying ? 10.0f : 5.0f) * dt;
  float lookSpeed = 120.0f * dt;

  // Rotation with right stick (Face Buttons)
  float lx = PSPInput_StickX(1);
  float ly = PSPInput_StickY(1);
  g_player.yaw += lx * lookSpeed;
  g_player.pitch += ly * lookSpeed;
  g_player.pitch = Mth::clamp(g_player.pitch, -89.0f, 89.0f);

  // Movement with left stick (Analog)
  float fx = -PSPInput_StickX(0);
  float fz = -PSPInput_StickY(0);

  float yawRad = g_player.yaw * Mth::DEGRAD;

  float dx = (fx * Mth::cos(yawRad) + fz * Mth::sin(yawRad)) * moveSpeed;
  float dz = (-fx * Mth::sin(yawRad) + fz * Mth::cos(yawRad)) * moveSpeed;

  const float R = 0.4f;
  const float H = 1.75f;

  auto isSolid = [&](float px, float py, float pz) -> bool {
    int bx = (int)px, by = (int)py, bz = (int)pz;
    if (bx < 0 || bx >= WORLD_CHUNKS_X * CHUNK_SIZE_X)
      return false;
    if (bz < 0 || bz >= WORLD_CHUNKS_Z * CHUNK_SIZE_Z)
      return false;
    if (by < 0 || by >= CHUNK_SIZE_Y)
      return false;
    return g_blockProps[g_level->getBlock(bx, by, bz)].isSolid();
  };

  // X collision - check at z-R, z-center, z+R (AABB corners)
  g_player.x += dx;
  {
    float cx = (dx > 0) ? g_player.x + R : g_player.x - R;
    bool hitX = false;
    for (int yi = 0; yi < 3 && !hitX; yi++) {
      float cy = g_player.y + (yi == 0 ? 0.05f : yi == 1 ? 0.9f : H);
      if (isSolid(cx, cy, g_player.z - R) || isSolid(cx, cy, g_player.z) ||
          isSolid(cx, cy, g_player.z + R))
        hitX = true;
    }
    if (hitX) {
      // FIX CRITIC: Pushes the player back exactly at R distance from the
      // block!
      if (dx > 0) {
        g_player.x = floorf(g_player.x + R) - R - 0.001f;
      } else {
        g_player.x = ceilf(g_player.x - R) + R + 0.001f;
      }
    }
  }

  // Z collision - check at x-R, x-center, x+R (AABB corners)
  g_player.z += dz;
  {
    float cz = (dz > 0) ? g_player.z + R : g_player.z - R;
    bool hitZ = false;
    for (int yi = 0; yi < 3 && !hitZ; yi++) {
      float cy = g_player.y + (yi == 0 ? 0.05f : yi == 1 ? 0.9f : H);
      if (isSolid(g_player.x - R, cy, cz) || isSolid(g_player.x, cy, cz) ||
          isSolid(g_player.x + R, cy, cz))
        hitZ = true;
    }
    if (hitZ) {
      // FIX CRITIC: Pushes the player back exactly at R distance from the
      // block!
      if (dz > 0) {
        g_player.z = floorf(g_player.z + R) - R - 0.001f;
      } else {
        g_player.z = ceilf(g_player.z - R) + R + 0.001f;
      }
    }
  }

  // ── Fly mode vertical movement ──────────────────────────────────────────
  if (g_player.isFlying) {
    float flySpeed = 10.0f * dt;
    if (PSPInput_IsHeld(PSP_CTRL_RTRIGGER))
      g_player.y += flySpeed;  // Ascend
    if (PSPInput_IsHeld(PSP_CTRL_LTRIGGER))
      g_player.y -= flySpeed;  // Descend
    g_player.velY = 0.0f;      // No gravity while flying
  } else if (!g_player.onGround) {
    // Normal gravity
    g_player.velY -= 20.0f * dt;
    g_player.y += g_player.velY * dt;
  }

  // Vertical collision: ground + ceiling
  // Check all 4 AABB corners on X/Z
  {
    const float WORLD_MAX_X = (float)(WORLD_CHUNKS_X * CHUNK_SIZE_X - 1);
    const float WORLD_MAX_Z = (float)(WORLD_CHUNKS_Z * CHUNK_SIZE_Z - 1);

    if (g_player.x < 0.5f)
      g_player.x = 0.5f;
    if (g_player.x > WORLD_MAX_X)
      g_player.x = WORLD_MAX_X;
    if (g_player.z < 0.5f)
      g_player.z = 0.5f;
    if (g_player.z > WORLD_MAX_Z)
      g_player.z = WORLD_MAX_Z;

    // GROUND collision (velY <= 0): search block below feet
    if (g_player.velY <= 0.0f) {
      // Test the 4 corners of the box
      int byFoot = (int)(g_player.y - 0.01f);
      if (byFoot < 0)
        byFoot = 0;
      bool hitFloor = false;
      float corners[4][2] = {{g_player.x - R, g_player.z - R},
                             {g_player.x + R, g_player.z - R},
                             {g_player.x - R, g_player.z + R},
                             {g_player.x + R, g_player.z + R}};
      for (int ci = 0; ci < 4 && !hitFloor; ci++) {
        if (isSolid(corners[ci][0], (float)byFoot, corners[ci][1]))
          hitFloor = true;
      }
      if (hitFloor) {
        g_player.y = (float)(byFoot + 1);
        g_player.velY = 0.0f;
        g_player.onGround = true;
      } else {
        g_player.onGround = false;
      }
    } else {
      // CEILING collision (velY > 0): head is at g_player.y + H
      float headY = g_player.y + H;
      int byCeil = (int)headY;
      bool hitCeil = false;
      float corners[4][2] = {{g_player.x - R, g_player.z - R},
                             {g_player.x + R, g_player.z - R},
                             {g_player.x - R, g_player.z + R},
                             {g_player.x + R, g_player.z + R}};
      for (int ci = 0; ci < 4 && !hitCeil; ci++) {
        if (isSolid(corners[ci][0], (float)byCeil, corners[ci][1]))
          hitCeil = true;
      }
      if (hitCeil) {
        // Head hit - stop ascent instantly and snap below the block
        g_player.y = (float)byCeil - H - 0.01f;
        g_player.velY = 0.0f;
      }
      g_player.onGround = false;
    }
  }

  // ── Double-tap R-Trigger to toggle fly (Revival-style) ──────────────────
  static const float DOUBLE_TAP_WINDOW = 0.35f;
  if (g_player.jumpDoubleTapTimer > 0.0f)
    g_player.jumpDoubleTapTimer -= dt;

  if (PSPInput_JustPressed(PSP_CTRL_RTRIGGER)) {
    if (g_player.jumpDoubleTapTimer > 0.0f) {
      // Second tap within 0.35s → TOGGLE fly mode (enter or exit)
      g_player.isFlying = !g_player.isFlying;
      g_player.velY = 0.0f;
      g_player.jumpDoubleTapTimer = 0.0f;
    } else {
      // First tap: normal jump (if on ground and not flying), start double-tap window
      if (!g_player.isFlying && g_player.onGround) {
        g_player.velY = 6.5f;
        g_player.onGround = false;
      }
      g_player.jumpDoubleTapTimer = DOUBLE_TAP_WINDOW;
    }
  }
}

static void game_render() {
  float _tod = g_level->getTimeOfDay();

  // Camera setup
  ScePspFVector3 camPos = {g_player.x, g_player.y + 1.6f, g_player.z};
  float yawRad = g_player.yaw * Mth::DEGRAD;
  float pitchRad = g_player.pitch * Mth::DEGRAD;

  ScePspFVector3 lookDir = {
      Mth::sin(yawRad) * Mth::cos(pitchRad), // X
      Mth::sin(pitchRad),                    // Y
      Mth::cos(yawRad) * Mth::cos(pitchRad)  // Z
  };

  ScePspFVector3 lookAt = {camPos.x + lookDir.x, camPos.y + lookDir.y,
                           camPos.z + lookDir.z};

  // Compute clear color (fog color) BEFORE beginning frame
  uint32_t clearColor = 0xFF000000;
  if (g_skyRenderer) {
      clearColor = g_skyRenderer->getFogColor(_tod, lookDir);
  }

  PSPRenderer_BeginFrame(clearColor);

  PSPRenderer_SetCamera(&camPos, &lookAt);

  if (g_skyRenderer)
    g_skyRenderer->renderSky(g_player.x, g_player.y, g_player.z, lookDir);

  // Render chunks
  g_chunkRenderer->render(g_player.x, g_player.y, g_player.z);

  if (g_cloudRenderer)
    g_cloudRenderer->renderClouds(g_player.x, g_player.y, g_player.z, 0.0f);

  // TODO: HUD (hotbar, crosshair)

  PSPRenderer_EndFrame();
}

// ====================================================
// Entry Point
// ====================================================
int main(int argc, char *argv[]) {
  setup_callbacks();

  if (!game_init()) {
    pspDebugScreenInit();
    pspDebugScreenPrintf("Init error!\n");
    sceKernelSleepThread();
    return 1;
  }

  uint64_t lastTime = sceKernelGetSystemTimeWide();

  while (true) {
    uint64_t now = sceKernelGetSystemTimeWide();
    float dt = (float)(now - lastTime) / 1000000.0f; // microseconds -> seconds
    if (dt > 0.05f)
      dt = 0.05f; // cap at 20 FPS min
    lastTime = now;

    game_update(dt);
    game_render();
  }

  return 0;
}
