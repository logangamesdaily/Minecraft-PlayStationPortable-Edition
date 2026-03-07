#pragma once
#include <stdint.h>

// Texture atlas: terrain.png loaded into PSP VRAM
// 16x16 tiles grid, each tile 16x16 px => 256x256 atlas

class TextureAtlas {
public:
  void *vramPtr; // pointer in VRAM (256x256)
  void *vramPtr1; // pointer in VRAM (128x128)
  void *vramPtr2; // pointer in VRAM (64x64)
  int width;
  int height;

  TextureAtlas();
  ~TextureAtlas();

  bool load(const char *path);
  void bind();

  // Epsilon to push UVs slightly inward to prevent nearest-neighbor floating point rounding errors
  // at block borders. 0.00195f represents half a texel on the 256x256 base level (which maps to 0.125 texels on 64x64, plenty to stop FP bleed).
  static float tileEpsilon() { return 0.001953125f; }
  
  static float tileU(int tx) { return (tx / 16.0f) + tileEpsilon(); }
  static float tileV(int ty) { return (ty / 16.0f) + tileEpsilon(); }
  static float tileSz() { return (1.0f / 16.0f) - (tileEpsilon() * 2.0f); }
};
