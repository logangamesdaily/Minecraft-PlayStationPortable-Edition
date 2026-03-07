// WorldGen.cpp - Procedural world generation
// Ported/adapted from Minecraft.World/RandomLevelSource.cpp (4J Studios, Oct 2014) 
// Noise -> NoiseGen, Trees -> TreeFeature, this class only handles base chunk gen

#include "WorldGen.h"
#include "Blocks.h"
#include "NoiseGen.h"
#include "Random.h"
#include "TreeFeature.h"
#include "chunk_defs.h"
#include <string.h>

// ============================================================
// Terrain height at a given world coordinate
// ============================================================
int WorldGen::getTerrainHeight(int wx, int wz, int64_t seed) {
  // 4 noise octaves, 64 block scale
  float n = NoiseGen::octaveNoise(wx / 64.0f, wz / 64.0f, seed);
  // Range: 40..60 (base level 50 = sea level)
  return 40 + (int)(n * 20.0f);
}

// ============================================================
// Generates the blocks for a full chunk
// ============================================================
void WorldGen::generateChunk(
    uint8_t out[CHUNK_SIZE_X][CHUNK_SIZE_Z][CHUNK_SIZE_Y], int cx, int cz,
    int64_t worldSeed) {

  memset(out, BLOCK_AIR, CHUNK_SIZE_X * CHUNK_SIZE_Z * CHUNK_SIZE_Y);

  Random rng(worldSeed ^ ((int64_t)cx * 341873128712LL) ^
             ((int64_t)cz * 132897987541LL));

  // === Base Terrain ===
  for (int lx = 0; lx < CHUNK_SIZE_X; lx++) {
    for (int lz = 0; lz < CHUNK_SIZE_Z; lz++) {
      int wx = cx * CHUNK_SIZE_X + lx;
      int wz = cz * CHUNK_SIZE_Z + lz;

      int surfaceY = getTerrainHeight(wx, wz, worldSeed);
      if (surfaceY >= CHUNK_SIZE_Y)
        surfaceY = CHUNK_SIZE_Y - 1;

      for (int y = 0; y <= surfaceY; y++) {
        uint8_t block;

        if (y == 0) {
          block = BLOCK_BEDROCK;
        } else if (y < surfaceY - 4) {
          block = BLOCK_STONE;
        } else if (y < surfaceY) {
          block = BLOCK_DIRT;
        } else {
          block = BLOCK_GRASS;
        }
        out[lx][lz][y] = block;
      }

      // Water at sea level (50) if surface is below it
      if (surfaceY < 50) {
        for (int y = surfaceY + 1; y <= 50; y++) {
          if (y < CHUNK_SIZE_Y)
            out[lx][lz][y] = BLOCK_WATER_STILL;
        }
      }
    }
  }
}
