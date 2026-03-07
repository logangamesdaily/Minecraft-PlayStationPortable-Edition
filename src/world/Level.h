#pragma once
#include "Chunk.h"
#include <math.h>

class Random;

// Match the 4J constant for ticks per in-game day
static const long long TICKS_PER_DAY = 24000LL;

class Level {
public:
  Level();
  ~Level();

  void generate(Random *rng);
  void computeLighting();

  Chunk *getChunk(int cx, int cz) const;
  void markDirty(int cx, int cz);

  uint8_t getBlock(int wx, int wy, int wz) const;
  void setBlock(int wx, int wy, int wz, uint8_t id);

  uint8_t getSkyLight(int wx, int wy, int wz) const;
  uint8_t getBlockLight(int wx, int wy, int wz) const;
  void setSkyLight(int wx, int wy, int wz, uint8_t val);
  void setBlockLight(int wx, int wy, int wz, uint8_t val);

  // Returns 0..1 normalized time of day using the same formula as 4J
  // Dimension::getTimeOfDay 0 = dawn, 0.25 = noon, 0.5 = dusk, 0.75 = midnight
  float getTimeOfDay() const {
    long long dayStep = m_time % TICKS_PER_DAY;
    float td = (float)dayStep / (float)TICKS_PER_DAY - 0.25f;
    if (td < 0.0f)
      td += 1.0f;
    if (td > 1.0f)
      td -= 1.0f;
    // 4J applies a cosine curve to smooth transitions
    float tdo = td;
    td = 1.0f - (cosf(tdo * 3.14159265f) + 1.0f) / 2.0f;
    td = tdo + (td - tdo) / 3.0f;
    return td;
  }

  // Returns 0.0 (night) to 1.0 (day) sun brightness
  float getSunBrightness() const {
    float td = getTimeOfDay();
    float br = cosf(td * 3.14159265f * 2.0f) * 2.0f + 0.5f;
    if (br < 0.0f) br = 0.0f;
    if (br > 1.0f) br = 1.0f;
    return br;
  }

  // Returns the discrete brightness stage the world is currently locked to
  float getLastSunBrightness() const { return m_lastSunBrightness; }

  int getDay() const { return (int)(m_time / TICKS_PER_DAY); }

  long long getTime() const { return m_time; }

  void tick() {
    // Advance time. Day/night is applied at render-time via sceGuAmbient — NO chunk rebuilds needed.
    m_time += 1; // ~400 seconds per full day at 60fps
  }

private:
  Chunk *m_chunks[WORLD_CHUNKS_X][WORLD_CHUNKS_Z];
  long long m_time = 6000LL; // Start at 6000 = dawn/sunrise
  float m_lastSunBrightness = 1.0f; // Track sun to trigger rebuilds
};
