#include "GrowthSprites.h"

#include <Arduino.h>
#include <cstring>

namespace {

// One adult family is a short evolution line with five possible mature forms.
// The lineages deliberately keep related silhouettes together while preserving
// all 50 existing Tamachibi adult designs.
const uint8_t ADULT_BRANCHES[TAMACHIBI_FAMILY_COUNT][TAMACHIBI_CARE_TIER_COUNT]
    PROGMEM = {
        {0, 15, 43, 45, 40}, // sky: Bird, MiniBird, Duck, HornOwl, UFO
        {1, 17, 23, 36, 39}, // tide: Slug, LongSeal, Jelly, Whale, Octo
        {2, 20, 28, 29, 48}, // pouncer: ShellCat, BatCat, ShirtCat, Fox, StarCat
        {3, 10, 24, 30, 44}, // shade: Shadow, Hood, Devil, Candle, Ghost
        {4, 22, 25, 31, 37}, // crumb: Burger, Bean, Blob, Potato, Sandwich
        {5, 6, 33, 42, 47},  // horn: HornPig, LongDino, BigMouth, Unicorn, DinoHead
        {7, 16, 26, 35, 41}, // pup: RoundDog, CowBell, BigNose, Mouse, RoundPet
        {8, 9, 19, 27, 46},  // sprout: Mushroom, Leaf, Frog, HatStack, Cape
        {11, 12, 13, 18, 49}, // tinker: Runner, Square, Snowman, Tower, Bell
        {14, 21, 32, 34, 38}  // gear: Hoodie, BatBug, Helmet, NoseMan, Robot
};

void set16(uint16_t rows[16], int x, int y) {
  if (x >= 0 && x < 16 && y >= 0 && y < 16)
    rows[y] |= static_cast<uint16_t>(1U << x);
}

void clear16(uint16_t rows[16], int x, int y) {
  if (x >= 0 && x < 16 && y >= 0 && y < 16)
    rows[y] &= static_cast<uint16_t>(~(1U << x));
}

void fillRect16(uint16_t rows[16], int x, int y, int w, int h) {
  for (int py = y; py < y + h; ++py)
    for (int px = x; px < x + w; ++px)
      set16(rows, px, py);
}

void line16(uint16_t rows[16], int x0, int y0, int x1, int y1) {
  const int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  const int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int error = dx + dy;
  while (true) {
    set16(rows, x0, y0);
    if (x0 == x1 && y0 == y1) break;
    const int twice = 2 * error;
    if (twice >= dy) { error += dy; x0 += sx; }
    if (twice <= dx) { error += dx; y0 += sy; }
  }
}

void ellipse16(uint16_t rows[16], int cx, int cy, int rx, int ry) {
  if (rx < 1 || ry < 1) return;
  const int rx2 = rx * rx;
  const int ry2 = ry * ry;
  const int edge = rx2 * ry2;
  for (int y = cy - ry; y <= cy + ry; ++y) {
    for (int x = cx - rx; x <= cx + rx; ++x) {
      const int dx = x - cx, dy = y - cy;
      if (dx * dx * ry2 + dy * dy * rx2 <= edge)
        set16(rows, x, y);
    }
  }
}

void buildFamilySeed(uint8_t family, uint16_t rows[16]) {
  memset(rows, 0, sizeof(uint16_t) * 16);
  switch (family % TAMACHIBI_FAMILY_COUNT) {
  case 0: // Skylark: crest, beak, wing, and split tail
    ellipse16(rows, 8, 9, 4, 4);
    ellipse16(rows, 5, 10, 3, 2);
    line16(rows, 7, 5, 6, 2); line16(rows, 9, 5, 10, 2);
    line16(rows, 11, 8, 14, 7); line16(rows, 11, 9, 15, 10);
    line16(rows, 6, 12, 5, 15); line16(rows, 9, 12, 10, 15);
    clear16(rows, 9, 7);
    break;
  case 1: // Tidekin: low swimmer with eye stalks and a forked fin
    ellipse16(rows, 7, 10, 6, 3);
    line16(rows, 4, 8, 3, 4); line16(rows, 10, 8, 12, 4);
    ellipse16(rows, 3, 3, 1, 1); ellipse16(rows, 12, 3, 1, 1);
    line16(rows, 12, 9, 15, 6); line16(rows, 12, 10, 15, 10);
    line16(rows, 12, 11, 15, 14);
    clear16(rows, 5, 9); clear16(rows, 9, 9);
    break;
  case 2: // Pouncer: pointed ears, cheeky body, and curling tail
    ellipse16(rows, 8, 9, 4, 4);
    line16(rows, 5, 6, 4, 2); line16(rows, 4, 2, 8, 5);
    line16(rows, 8, 5, 12, 2); line16(rows, 12, 2, 11, 6);
    line16(rows, 11, 10, 14, 9); line16(rows, 14, 9, 14, 6);
    line16(rows, 6, 12, 5, 15); line16(rows, 10, 12, 11, 15);
    clear16(rows, 6, 8); clear16(rows, 10, 8);
    break;
  case 3: // Shadelet: floating ghost with a wavering hem and wisp
    ellipse16(rows, 8, 8, 6, 6);
    line16(rows, 3, 11, 3, 13); line16(rows, 3, 13, 5, 12);
    line16(rows, 5, 12, 7, 14); line16(rows, 7, 14, 9, 12);
    line16(rows, 9, 12, 11, 14); line16(rows, 11, 14, 13, 12);
    line16(rows, 13, 12, 14, 13);
    line16(rows, 8, 2, 8, 0); line16(rows, 8, 0, 10, 1);
    clear16(rows, 6, 8); clear16(rows, 10, 8);
    break;
  case 4: // Crumbkin: layered bun, filling, sesame, and tiny feet
    ellipse16(rows, 8, 5, 5, 3);
    fillRect16(rows, 3, 7, 10, 2);
    fillRect16(rows, 4, 9, 8, 2);
    ellipse16(rows, 8, 11, 5, 2);
    set16(rows, 5, 3); set16(rows, 8, 2); set16(rows, 11, 3);
    line16(rows, 5, 12, 4, 15); line16(rows, 10, 12, 11, 15);
    clear16(rows, 6, 8); clear16(rows, 10, 8);
    break;
  case 5: // Hornling: horns, ridge, strong body, and tail
    ellipse16(rows, 7, 9, 5, 4);
    line16(rows, 4, 6, 2, 2); line16(rows, 10, 6, 13, 2);
    line16(rows, 5, 5, 6, 3); line16(rows, 7, 5, 8, 3);
    line16(rows, 11, 9, 15, 7); line16(rows, 11, 10, 15, 12);
    line16(rows, 5, 12, 4, 15); line16(rows, 9, 12, 10, 15);
    clear16(rows, 6, 8); clear16(rows, 9, 8);
    break;
  case 6: // Puddle pup: floppy ears, muzzle, four paws, curved tail
    ellipse16(rows, 8, 9, 5, 4);
    ellipse16(rows, 3, 7, 2, 3); ellipse16(rows, 13, 7, 2, 3);
    ellipse16(rows, 10, 10, 2, 2);
    line16(rows, 12, 9, 15, 7); line16(rows, 15, 7, 15, 5);
    line16(rows, 5, 12, 4, 15); line16(rows, 8, 13, 8, 15);
    line16(rows, 11, 12, 12, 15);
    clear16(rows, 6, 8); clear16(rows, 9, 8); clear16(rows, 11, 10);
    break;
  case 7: // Sproutling: leaves, stem, round body, and roots
    ellipse16(rows, 8, 11, 4, 3);
    line16(rows, 8, 9, 8, 4);
    ellipse16(rows, 5, 5, 3, 2); ellipse16(rows, 11, 5, 3, 2);
    line16(rows, 6, 13, 5, 15); line16(rows, 8, 13, 8, 15);
    line16(rows, 10, 13, 11, 15);
    clear16(rows, 6, 10); clear16(rows, 10, 10);
    break;
  case 8: // Tinker: bell/tower body, side arms, and clapper
    fillRect16(rows, 6, 3, 5, 2);
    for (int y = 5; y <= 12; ++y) {
      const int half = 2 + (y - 5) / 2;
      for (int x = 8 - half; x <= 8 + half; ++x) set16(rows, x, y);
    }
    line16(rows, 5, 7, 2, 9); line16(rows, 11, 7, 14, 9);
    ellipse16(rows, 8, 14, 2, 1);
    clear16(rows, 6, 8); clear16(rows, 10, 8);
    break;
  default: // Gearling: square head, antenna, bolts, and little legs
    line16(rows, 8, 4, 8, 1); line16(rows, 8, 1, 10, 1);
    fillRect16(rows, 4, 4, 9, 6);
    fillRect16(rows, 5, 10, 7, 3);
    line16(rows, 4, 7, 1, 8); line16(rows, 12, 7, 15, 8);
    line16(rows, 6, 13, 5, 15); line16(rows, 10, 13, 11, 15);
    clear16(rows, 6, 6); clear16(rows, 10, 6);
    break;
  }
}

void set32(uint32_t rows[32], int x, int y) {
  if (x >= 0 && x < 32 && y >= 0 && y < 32)
    rows[y] |= 0x80000000UL >> x;
}

void clear32(uint32_t rows[32], int x, int y) {
  if (x >= 0 && x < 32 && y >= 0 && y < 32)
    rows[y] &= ~(0x80000000UL >> x);
}

void addThrivingTeenCrest(uint32_t rows[32], uint8_t family) {
  switch (family % TAMACHIBI_FAMILY_COUNT) {
  case 0: set32(rows, 15, 0); set32(rows, 16, 1); set32(rows, 17, 0); break;
  case 1: set32(rows, 29, 17); set32(rows, 31, 15); break;
  case 2: set32(rows, 7, 1); set32(rows, 8, 0); set32(rows, 9, 1); break;
  case 3: set32(rows, 15, 0); set32(rows, 16, 0); set32(rows, 17, 1); break;
  case 4: set32(rows, 10, 4); set32(rows, 16, 2); set32(rows, 22, 4); break;
  case 5: set32(rows, 5, 1); set32(rows, 6, 0); set32(rows, 25, 1); set32(rows, 26, 0); break;
  case 6: set32(rows, 3, 3); set32(rows, 4, 2); set32(rows, 27, 3); set32(rows, 28, 2); break;
  case 7: set32(rows, 11, 1); set32(rows, 12, 0); set32(rows, 20, 0); set32(rows, 21, 1); break;
  case 8: set32(rows, 15, 0); set32(rows, 15, 1); break;
  default: set32(rows, 15, 0); set32(rows, 16, 0); set32(rows, 17, 0); break;
  }
}

void addCareMissMark(uint32_t rows[32], uint8_t family) {
  // A broken slash reads clearly at the real 32px pet scale.
  const int x = 14 + (family % 3);
  clear32(rows, x, 17);
  clear32(rows, x + 1, 17);
  clear32(rows, x + 1, 18);
  clear32(rows, x + 2, 18);
}

void line32(uint32_t rows[32], int x0, int y0, int x1, int y1) {
  const int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  const int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int error = dx + dy;
  while (true) {
    set32(rows, x0, y0);
    if (x0 == x1 && y0 == y1) break;
    const int twice = 2 * error;
    if (twice >= dy) { error += dy; x0 += sx; }
    if (twice <= dx) { error += dx; y0 += sy; }
  }
}

void addChildAccents(uint32_t rows[32], uint8_t family) {
  switch (family % TAMACHIBI_FAMILY_COUNT) {
  case 0: // young bird's growing wing and split tail
    line32(rows, 4, 17, 9, 19); line32(rows, 5, 19, 9, 21);
    line32(rows, 22, 23, 27, 27); set32(rows, 28, 27);
    break;
  case 1: // swimmer's side fins and rising bubbles
    line32(rows, 24, 20, 30, 16); line32(rows, 24, 22, 30, 24);
    set32(rows, 5, 9); set32(rows, 3, 6); set32(rows, 6, 4);
    break;
  case 2: // catlike ears, paws, and a curling tail
    line32(rows, 24, 23, 29, 21); line32(rows, 29, 21, 30, 17);
    set32(rows, 10, 29); set32(rows, 20, 29);
    break;
  case 3: // wisp and a wavy ghost hem
    line32(rows, 24, 5, 27, 2); line32(rows, 27, 2, 30, 4);
    line32(rows, 6, 29, 10, 27); line32(rows, 14, 29, 18, 27);
    line32(rows, 22, 29, 26, 27);
    break;
  case 4: // sesame seeds and a second filling stripe
    line32(rows, 7, 22, 24, 22);
    set32(rows, 10, 9); set32(rows, 16, 7); set32(rows, 22, 9);
    break;
  case 5: // longer horns, ridge, and tail
    line32(rows, 9, 7, 7, 1); line32(rows, 22, 7, 25, 1);
    line32(rows, 23, 22, 30, 19); line32(rows, 23, 23, 29, 26);
    break;
  case 6: // floppy ears and four little paws
    line32(rows, 4, 13, 1, 19); line32(rows, 27, 13, 30, 19);
    set32(rows, 9, 29); set32(rows, 13, 30); set32(rows, 20, 30); set32(rows, 24, 29);
    break;
  case 7: // paired leaves, stem, and root marks
    line32(rows, 16, 10, 16, 1); line32(rows, 16, 5, 10, 2); line32(rows, 16, 6, 22, 2);
    set32(rows, 12, 29); set32(rows, 16, 30); set32(rows, 20, 29);
    break;
  case 8: // bell rim, side arms, and clapper
    line32(rows, 8, 27, 24, 27); line32(rows, 8, 28, 24, 28);
    line32(rows, 7, 17, 2, 20); line32(rows, 25, 17, 30, 20);
    set32(rows, 16, 30);
    break;
  default: // antenna, bolts, and square feet
    line32(rows, 18, 7, 18, 1); line32(rows, 18, 1, 23, 1);
    set32(rows, 5, 14); set32(rows, 26, 14);
    line32(rows, 10, 28, 8, 31); line32(rows, 22, 28, 24, 31);
    break;
  }
}

} // namespace

void buildGrowthRows(uint8_t family, uint8_t stage, uint8_t route,
                     uint32_t rows[32]) {
  memset(rows, 0, sizeof(uint32_t) * 32);
  if (stage == 0) return;

  uint16_t seed[16];
  buildFamilySeed(family % TAMACHIBI_FAMILY_COUNT, seed);

  int width = 16, height = 16;
  if (stage == 1) { width = 8; height = 7; }
  else if (stage == 2) { width = 13; height = 12; }
  else if (stage == 3 && route != 0) { width = 14; height = 11; }
  const int offsetX = (16 - width) / 2;
  const int offsetY = 16 - height;

  for (int y = 0; y < height; ++y) {
    const int sourceY = (y * 16) / height;
    for (int x = 0; x < width; ++x) {
      const int sourceX = (x * 16) / width;
      if (!(seed[sourceY] & static_cast<uint16_t>(1U << sourceX))) continue;
      const int targetX = 2 * (offsetX + x);
      const int targetY = 2 * (offsetY + y);
      set32(rows, targetX, targetY);
      set32(rows, targetX + 1, targetY);
      set32(rows, targetX, targetY + 1);
      set32(rows, targetX + 1, targetY + 1);
    }
  }

  if (stage >= 2) addChildAccents(rows, family);
  if (stage == 3) {
    if (route == 0) addThrivingTeenCrest(rows, family);
    else addCareMissMark(rows, family);
  }
}

uint8_t growthAdultSpecies(uint8_t family, uint8_t careTier) {
  family %= TAMACHIBI_FAMILY_COUNT;
  careTier = careTier < TAMACHIBI_CARE_TIER_COUNT
                 ? careTier
                 : TAMACHIBI_CARE_TIER_COUNT - 1;
  return pgm_read_byte(&ADULT_BRANCHES[family][careTier]);
}

uint8_t growthFamilyForAdult(uint8_t adultSpecies) {
  adultSpecies %= 50;
  for (uint8_t family = 0; family < TAMACHIBI_FAMILY_COUNT; ++family)
    for (uint8_t tier = 0; tier < TAMACHIBI_CARE_TIER_COUNT; ++tier)
      if (growthAdultSpecies(family, tier) == adultSpecies) return family;
  return adultSpecies % TAMACHIBI_FAMILY_COUNT;
}

uint8_t growthCareTierForAdult(uint8_t adultSpecies) {
  adultSpecies %= 50;
  for (uint8_t family = 0; family < TAMACHIBI_FAMILY_COUNT; ++family)
    for (uint8_t tier = 0; tier < TAMACHIBI_CARE_TIER_COUNT; ++tier)
      if (growthAdultSpecies(family, tier) == adultSpecies) return tier;
  return 0;
}
