#include "GrowthSprites.h"

#include <Arduino.h>
#include <cstring>
#include <initializer_list>

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


} // namespace

void buildGrowthRows(uint8_t family, uint8_t stage, uint8_t route,
                     uint32_t rows[32]) {
  memset(rows, 0, sizeof(uint32_t) * 32);
  if (stage == 0) return;

  uint16_t seed[16];
  buildFamilySeed(family % TAMACHIBI_FAMILY_COUNT, seed);

  // Babies are authored at their actual pixel size. Sampling the larger
  // creature used to drop eyes and limbs, leaving disconnected dots.
  if (stage == 1) {
    memset(seed, 0, sizeof(seed));
    ellipse16(seed, 8, 11, 3, 3);
    switch (family % TAMACHIBI_FAMILY_COUNT) {
    case 0: line16(seed, 7, 8, 6, 6); fillRect16(seed, 11, 10, 2, 2); break;
    case 1: ellipse16(seed, 8, 12, 4, 2); line16(seed, 11, 12, 14, 10); line16(seed, 14, 10, 14, 14); break;
    case 2: line16(seed, 5, 9, 5, 6); line16(seed, 5, 6, 8, 9); line16(seed, 8, 9, 11, 6); line16(seed, 11, 6, 11, 9); break;
    case 3: line16(seed, 8, 8, 9, 5); fillRect16(seed, 5, 13, 7, 2); clear16(seed, 7, 14); clear16(seed, 10, 14); break;
    case 4: ellipse16(seed, 8, 10, 4, 2); fillRect16(seed, 4, 12, 9, 2); clear16(seed, 8, 8); break;
    case 5: line16(seed, 5, 9, 4, 6); line16(seed, 11, 9, 12, 6); break;
    case 6: ellipse16(seed, 4, 10, 1, 2); ellipse16(seed, 12, 10, 1, 2); break;
    case 7: line16(seed, 8, 8, 8, 6); ellipse16(seed, 6, 6, 2, 1); ellipse16(seed, 10, 5, 2, 1); break;
    case 8: fillRect16(seed, 5, 9, 7, 5); fillRect16(seed, 4, 13, 9, 1); set16(seed, 8, 15); break;
    default: fillRect16(seed, 5, 9, 7, 5); line16(seed, 8, 9, 8, 6); fillRect16(seed, 7, 5, 3, 2); break;
    }
    clear16(seed, 7, 11); clear16(seed, 9, 11);
    set16(seed, 6, 15); set16(seed, 10, 15);
  }
  const int width = stage == 1 ? 16 : stage == 2 ? 13 : 16;
  const int height = stage == 1 ? 16 : stage == 2 ? 13 : (route ? 14 : 16);
  const int offsetX = (16 - width) / 2, offsetY = 16 - height;
  // Forward projection preserves every authored feature when the child is
  // smaller; inverse nearest-neighbor sampling could erase whole appendages.
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 16; ++x) {
      if (!(seed[y] & static_cast<uint16_t>(1U << x))) continue;
      const int tx = 2 * (offsetX + x * width / 16);
      const int ty = 2 * (offsetY + y * height / 16);
      for (int dy = 0; dy < 2; ++dy)
        for (int dx = 0; dx < 2; ++dx) set32(rows, tx + dx, ty + dy);
    }

  // Recut the face after projection so smaller bodies never lose their eyes.
  if (stage == 2) {
    const int eyeY = family == 7 ? 10 : family == 9 ? 6 : family == 0 ? 7 : 8;
    for (int eyeX : {6, 10}) {
      const int tx = 2 * (offsetX + eyeX * width / 16);
      const int ty = 2 * (offsetY + eyeY * height / 16);
      for (int dy=0;dy<2;++dy) for (int dx=0;dx<2;++dx) clear32(rows,tx+dx,ty+dy);
    }
  }
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

void addGrowthFamilyTrait(uint8_t family, uint32_t rows[32]) {
  int top = 32, left = 32, right = -1;
  // Anchor to the main body rather than a stray star, foot, or ornament.
  for (int y = 3; y < 27; ++y) {
    int count = 0, l = 32, r = -1;
    for (int x = 3; x < 29; ++x) if (rows[y] & (0x80000000UL >> x)) {
      ++count; if (l == 32) l = x; r = x;
    }
    if (count >= 6) { top = y; left = l; right = r; break; }
  }
  if (top == 32) return;
  const int cx = (left + right) / 2;
  const int y = top < 5 ? 5 : top;
  switch (family % TAMACHIBI_FAMILY_COUNT) {
  case 0: // forked feather crest
    line32(rows,cx,y,cx-3,y-4); line32(rows,cx,y,cx+2,y-4); break;
  case 1: // paired swept fins
    line32(rows,left,y+4,left-3,y+1); line32(rows,right,y+4,right+3,y+1); break;
  case 2: // triangular cat ears
    line32(rows,left,y+2,left,y-4); line32(rows,left,y-4,left+5,y+1);
    line32(rows,right,y+2,right,y-4); line32(rows,right,y-4,right-5,y+1); break;
  case 3: // curled spirit wisp
    line32(rows,cx,y,cx+3,y-3); line32(rows,cx+3,y-3,cx+1,y-5); break;
  case 4: // sesame crown attached to the bun
    line32(rows,cx-3,y+1,cx-3,y-1); line32(rows,cx+3,y+1,cx+3,y-1); break;
  case 5: // outward horns
    line32(rows,left+2,y+1,left,y-4); line32(rows,right-2,y+1,right,y-4); break;
  case 6: // rounded drooping ears
    line32(rows,left,y+1,left-2,y+3); line32(rows,left-2,y+3,left-2,y+6);
    line32(rows,right,y+1,right+2,y+3); line32(rows,right+2,y+3,right+2,y+6); break;
  case 7: // sprouting twin leaves
    line32(rows,cx,y,cx,y-4); line32(rows,cx,y-2,cx-4,y-4); line32(rows,cx-4,y-4,cx,y-4);
    line32(rows,cx,y-3,cx+4,y-5); line32(rows,cx+4,y-5,cx,y-5); break;
  case 8: // rounded bell handle
    line32(rows,cx-2,y,cx-2,y-3); line32(rows,cx-2,y-3,cx+2,y-3); line32(rows,cx+2,y-3,cx+2,y); break;
  default: // square receiver on a stalk
    line32(rows,cx,y,cx,y-3); line32(rows,cx-2,y-3,cx+2,y-3); line32(rows,cx-2,y-4,cx+2,y-4); break;
  }
}
