#pragma once

#include <algorithm>
#include <cstdlib>
#include <Adafruit_GFX.h>
#include "GameDisplay.h"

// Small, filled 16x8 bodies let the aquarium draw distinct species and
// animate their fins without allocating a second screen-sized framebuffer.
namespace AquariumArt {
static const uint8_t FISH_BITMAPS[12][16] PROGMEM = {
  {0x00,0x00,0x03,0x00,0x0F,0xC0,0xDF,0xF0,0xFF,0xFC,0xDF,0xF0,0x0F,0xC0,0x03,0x00}, // Guppy
  {0x00,0x00,0x07,0xC0,0x3F,0xF0,0xDF,0xFC,0xFF,0xFE,0xDF,0xFC,0x3F,0xF0,0x07,0xC0}, // Goldfish
  {0x07,0x00,0x1F,0xC0,0x3F,0xF0,0xDF,0xFE,0xFF,0xFE,0xDF,0xFC,0x1F,0xE0,0x07,0xC0}, // Betta
  {0x00,0x00,0x00,0x00,0x3F,0xF0,0xDF,0xFE,0xFF,0xFE,0xDF,0xFE,0x3F,0xF0,0x00,0x00}, // Neon tetra
  {0x00,0x00,0x0F,0xC0,0x3F,0xF0,0xDF,0xFE,0xFF,0xFE,0xDF,0xFE,0x1F,0xF0,0x07,0x80}, // Molly
  {0x00,0x00,0x00,0x00,0x1F,0xE0,0xDF,0xFE,0xFF,0xFE,0xFF,0xFE,0x3F,0xF8,0x40,0x08}, // Cory
  {0x01,0x80,0x03,0xC0,0x0F,0xF0,0xDF,0xFE,0xFF,0xFE,0xDF,0xFE,0x0F,0xF0,0x03,0xC0}, // Angelfish
  {0x00,0x00,0x1F,0xE0,0x7F,0xF8,0xDF,0xFE,0xFF,0xFE,0xDF,0xFE,0x7F,0xF8,0x1F,0xE0}, // Koi
  {0x13,0x20,0x0F,0xC0,0x7F,0xF8,0xFF,0xFE,0xFF,0xFE,0x7F,0xF8,0x0F,0xC0,0x13,0x20}, // Puffer
  {0x00,0x00,0x0F,0xE0,0x3F,0xF8,0xDF,0xFE,0xFF,0xFE,0xDF,0xFE,0x3F,0xF8,0x0F,0xE0}, // Clownfish
  {0x03,0xC0,0x0F,0xF0,0x3F,0xFC,0xDF,0xFE,0xFF,0xFE,0xDF,0xFE,0x1F,0xF8,0x07,0xE0}, // Moonfin
  {0x01,0x00,0x0F,0xE0,0x3F,0xF8,0xDF,0xFE,0xFF,0xFE,0xDF,0xFE,0x1F,0xF0,0x07,0x40}  // Starfin
};

struct SwimPoint {
  int x;
  int y;
  bool facingRight;
};

inline uint8_t preferredDecor(uint8_t species, uint8_t seed) {
  return static_cast<uint8_t>((seed + species * 3U) % 5U + 1U);
}

inline SwimPoint swimPoint(uint8_t seed, uint8_t rank, uint8_t total,
                           uint16_t frame) {
  const uint8_t personality = seed & 3U;
  const int homeX = total <= 1 ? 64 : ((rank & 1U) ? 91 : 35);
  const uint16_t tempo = personality == 0 ? 4U : personality == 1 ? 2U :
                         personality == 2 ? 5U : 3U;
  const uint16_t cycle = 42U + ((seed >> 2U) & 3U) * 4U;
  const uint16_t phase = static_cast<uint16_t>((frame * tempo + seed * 3U) % (cycle * 2U));
  const bool right = phase < cycle;
  const uint16_t travel = right ? phase : cycle * 2U - phase;
  const int amplitude = personality == 0 ? 9 : personality == 1 ? 4 :
                        personality == 2 ? 6 : 8;
  const int sway = -amplitude + static_cast<int>(travel * (2 * amplitude) / cycle);
  const int vertical = static_cast<int>((frame / (personality == 1 ? 5U : 7U) +
                                          seed * 3U + rank * 5U) % 9U) - 4;
  const int baseY = rank < 2 ? 40 + rank * 3 : 64 + (rank - 2) * 3;
  return {homeX + sway, baseY + vertical, right};
}

inline bool bodyPixel(uint8_t species, uint8_t x, uint8_t y) {
  const uint8_t bits = pgm_read_byte(&FISH_BITMAPS[species % 12][y * 2 + x / 8]);
  return (bits & (0x80U >> (x & 7U))) != 0;
}

struct Brush {
  GameDisplay &display;
  int left, top;
  uint8_t scale;
  bool right;

  int x(int sourceX) const {
    const int mirrored = right ? sourceX : 15 - sourceX;
    return left + (mirrored * scale) / 2;
  }
  int y(int sourceY) const { return top + (sourceY * scale) / 2; }
  int extentX(int sourceX) const {
    return ((sourceX + 1) * scale + 1) / 2 - (sourceX * scale) / 2;
  }
  int extentY(int sourceY) const {
    return ((sourceY + 1) * scale + 1) / 2 - (sourceY * scale) / 2;
  }
  void pixel(int sourceX, int sourceY, uint16_t ink = 1) const {
    const int px = x(sourceX), py = y(sourceY);
    display.fillRect(px, py, extentX(sourceX), extentY(sourceY), ink);
  }
  void line(int x0, int y0, int x1, int y1, uint16_t ink = 1) const {
    const int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    const int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    while (true) {
      pixel(x0, y0, ink);
      if (x0 == x1 && y0 == y1) break;
      const int twice = error * 2;
      if (twice >= dy) { error += dy; x0 += sx; }
      if (twice <= dx) { error += dx; y0 += sy; }
    }
  }
};

inline void fish(GameDisplay &d, uint8_t species, int centerX, int centerY,
                 uint8_t lifeStage, bool facingRight, uint8_t frame,
                 uint8_t quirk, bool selected, uint8_t pose = 0,
                 uint8_t growthBranch = 1) {
  species %= 12;
  uint8_t scale = lifeStage == 0 ? 1 : lifeStage == 1 ? 2 :
                  lifeStage == 2 ? 3 : 4;
  if (selected && scale < 3) ++scale;
  const int width = (16 * scale + 1) / 2;
  const int height = (8 * scale + 1) / 2;
  const int left = centerX - width / 2;
  const int top = centerY - height / 2;
  Brush b{d, left, top, scale, facingRight};

  // Fill each silhouette row so a moving bubble or plant cannot show through
  // a stray mask gap. Species markings are cut in afterward on purpose.
  for (uint8_t sy = 0; sy < 8; ++sy) {
    int first = 16, last = -1;
    for (uint8_t sx = 0; sx < 16; ++sx) {
      if (!bodyPixel(species, sx, sy)) continue;
      first = std::min(first, static_cast<int>(sx));
      last = static_cast<int>(sx);
    }
    for (int sx = first; sx <= last; ++sx) b.pixel(sx, sy);
  }

  const int tail = ((frame >> 1) & 3U) == 0 ? -2 :
                   ((frame >> 1) & 3U) == 2 ? 1 : 0;
  const int tailEnd = species == 2 ? -5 : species == 0 || species == 7 ? -4 : -3;
  b.line(2, 4, tailEnd, 1 + tail, 1);
  b.line(2, 4, tailEnd - (species == 1 ? 1 : 0), 7 + tail, 1);
  b.line(2, 4, tailEnd + 1, 4 + tail, 1);

  // Side fins and gills move on their own, independently of the swim path.
  const int fin = ((frame >> 1) & 3U) == 0 ? -2 :
                  ((frame >> 1) & 3U) == 2 ? 2 : 0;
  if (species == 6 || species == 11) {
    b.line(8, 3, 6, -2 + fin, 1);
    b.line(8, 4, 6, 10 - fin, 1);
    if (species == 6) {
      b.line(12, 6, 13, 10 + fin, 1);
      b.line(13, 10 + fin, 14, 12 + fin, 1);
    } else {
      b.line(1, 3, -1, 1 + fin, 1);
      b.line(1, 5, -1, 7 - fin, 1);
    }
  } else if (species == 2) {
    b.line(4, 2, 5, -2 + fin, 1);
    b.line(5, -2 + fin, 10, -1 + fin, 1);
    b.line(5, 6, 8, 9 - fin, 1);
  } else if (species == 8) {
    b.line(5, 2, 4, -1 + fin, 1);
    b.line(8, 1, 8, -2 - fin, 1);
    b.line(11, 2, 12, -1 + fin, 1);
    b.line(6, 6, 5, 9 - fin, 1);
  } else {
    b.line(7, 2, 8, -1 + fin, 1);
    b.line(8, 6, 9, 8 - fin, 1);
  }

  // Species markings keep the silhouettes recognizable at a glance.
  switch (species) {
    case 1: // flowing goldfish fins and scales
      b.line(5, 2, 8, 0 + fin, 1); b.pixel(7, 3, 0); b.pixel(9, 5, 0);
      break;
    case 2: // betta veil rays
      b.line(1, 2, -3, 0 + fin, 1); b.line(1, 5, -3, 7 - fin, 1);
      b.pixel(8, 3, 0);
      break;
    case 3: // neon stripe
      b.line(5, 3, 11, 3, 0); b.line(6, 4, 11, 4, 1);
      break;
    case 4: // molly dorsal crest
      b.line(7, 2, 9, -1 + fin, 1); b.pixel(10, 3, 0);
      break;
    case 5: // cory barbels and armor spots
      b.line(12, 5, 14, 6, 1); b.line(12, 6, 14, 7, 1);
      b.pixel(6, 3, 0); b.pixel(8, 4, 0);
      break;
    case 6: // angel stripes and long feelers
      b.line(7, 0, 7, 7, 0); b.line(9, 0, 9, 7, 0);
      break;
    case 7: // koi patches and barbels
      b.pixel(5, 2, 0); b.pixel(6, 3, 0); b.pixel(8, 5, 0);
      b.line(13, 4, 15, 3, 1); b.line(13, 5, 15, 6, 1);
      break;
    case 8: // puffer spots
      b.pixel(5, 3, 0); b.pixel(8, 5, 0); b.pixel(10, 2, 0);
      break;
    case 9: // clownfish bars
      b.line(5, 1, 5, 6, 0); b.line(8, 1, 8, 6, 0);
      b.line(11, 2, 11, 5, 0); b.pixel(6, 3, 1);
      break;
    case 10: // moon spots
      b.pixel(6, 2, 0); b.pixel(7, 3, 0); b.pixel(6, 4, 0);
      b.pixel(9, 5, 0); b.pixel(11, 2, 1);
      break;
    case 11: // starry scales
      b.pixel(5, 3, 0); b.pixel(8, 2, 0); b.pixel(9, 5, 0);
      break;
    default: // guppy fan tail
      b.line(3, 3, 1, 1, 0); b.line(3, 5, 1, 7, 0);
      break;
  }

  // A fish carries the same small mark from youth into its grown form. The
  // mark is tied to its temperament seed, so siblings can look related while
  // still having a personal pattern.
  if (lifeStage >= 2) {
    switch (quirk & 3U) {
      case 0: b.pixel(5, 2, 0); b.pixel(6, 2, 1); b.pixel(7, 2, 0); break;
      case 1: b.pixel(5, 5, 0); b.pixel(6, 5, 1); b.pixel(7, 5, 0); break;
      case 2: b.line(9, 2, 9, 5, 0); b.pixel(10, 3, 1); break;
      default: b.line(4, 4, 10, 4, 0); b.pixel(7, 3, 1); break;
    }
  }
  if (lifeStage >= 3) {
    switch (growthBranch & 3U) {
      case 0: b.pixel(6, 2, 0); b.pixel(10, 5, 0); break;
      case 1: b.line(5, 2, 5, 5, 0); b.pixel(6, 3, 1); break;
      case 2: b.line(5, 2, 10, 2, 0); b.line(6, 5, 10, 5, 0); break;
      default:
        b.pixel(5, 2, 0); b.pixel(6, 2, 1); b.pixel(7, 2, 0);
        b.pixel(9, 5, 0); b.pixel(10, 5, 1); b.pixel(11, 5, 0);
        b.line(2, 1, -2, 0 + fin, 1);
        break;
    }
  }

  // Eye is a dark pixel inside the filled body, with a glint on the next beat.
  b.pixel(12, 3, 0);
  if ((frame & 7U) != 0) b.pixel(13, 3, 1);
  if (selected) {
    d.drawPixel(centerX - 2, centerY - height / 2 - 2, 1);
    d.drawPixel(centerX, centerY - height / 2 - 3, 1);
    d.drawPixel(centerX + 2, centerY - height / 2 - 2, 1);
  }
  if (pose == 1) {
    d.drawLine(centerX, centerY - height / 2 - 5,
               centerX + 2, centerY - height / 2 - 8, 1);
    d.drawPixel(centerX + 4, centerY - height / 2 - 7, 1);
  } else if (pose == 2 || (quirk == 1 && ((frame >> 2) & 7U) == 0)) {
    const int bubbleX = centerX + (facingRight ? 7 : -7);
    d.drawCircle(bubbleX, centerY - height / 2 - 4, 2, 1);
    d.drawPixel(bubbleX + 1, centerY - height / 2 - 7, 1);
  } else if (pose == 3) {
    d.drawCircle(centerX + 5, centerY - height / 2 - 4, 2, 1);
    d.drawCircle(centerX + 9, centerY - height / 2 - 7, 1, 1);
  }
}

} // namespace AquariumArt
