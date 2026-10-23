#include "NativeUI.h"
#include "GrowthSprites.h"
#include "RosterSprites.h"
#include <cassert>
#include <cstdio>
#include <cstring>
uint32_t fixtureMillis = 100;
int main() {
  Adafruit_SH1107 panel;
  GameDisplay d(panel);
  d.useGameCanvas(true);
  d.setGameButtons("LEFT", "ACTION", "RIGHT");
  // Full source playfield, including both corner pixels, is preserved at 1:1.
  d.clearDisplay();
  d.drawPixel(0, 0, 1);
  d.drawPixel(127, 63, 1);
  d.display();
  assert(panel.pixels[22 * 128] == 1);
  assert(panel.pixels[85 * 128 + 127] == 1);
  // A circle keeps the same width and height. Text cannot change its scene
  // pixels.
  auto circle = [&](bool text) {
    d.clearDisplay();
    d.drawCircle(64, 32, 10, 1);
    if (text) {
      d.setCursor(0, 20);
      d.print("THIS TEXT IS SEPARATE");
    }
    d.display();
  };
  circle(false);
  uint8_t before[64 * 128];
  memcpy(before, panel.pixels + 22 * 128, sizeof(before));
  int x0 = 128, x1 = -1, y0 = 128, y1 = -1;
  for (int y = 22; y < 86; ++y)
    for (int x = 0; x < 128; ++x)
      if (panel.pixels[y * 128 + x]) {
        x0 = min(x0, x);
        x1 = max(x1, x);
        y0 = min(y0, y);
        y1 = max(y1, y);
      }
  assert(x1 - x0 == 20 && y1 - y0 == 20);
  circle(true);
  assert(memcmp(before, panel.pixels + 22 * 128, sizeof(before)) == 0);
  // Native scenes remain intact when a function returns before its caller
  // flushes.
  d.useGameCanvas(false);
  {
    NativeUI::Scope scope(d);
    d.clearDisplay();
    d.drawPixel(90, 90, 1);
  }
  d.display();
  assert(panel.pixels[90 * 128 + 90]);
  // The following supporting page starts fresh in the collector.
  d.clearDisplay();
  d.drawPixel(20, 20, 1);
  assert(panel.pixels[20 * 128 + 20] == 0);

  // The active pet renderer must use the 50 detailed species masks, and its
  // post-hatch forms must grow while retaining those species silhouettes.
  d.useNativeLayout(true);
  uint8_t firstAdult[128 * 128];
  uint64_t fingerprints[50]{};
  for (int species = 0; species < 50; ++species) {
    d.clearDisplay();
    NativeUI::pet(d, 48, 48, species, 4, 0, false, 2);
    d.display();
    uint64_t hash = 1469598103934665603ULL;
    for (int i = 0; i < 128 * 128; ++i) {
      hash ^= panel.pixels[i];
      hash *= 1099511628211ULL;
    }
    for (int prior = 0; prior < species; ++prior)
      assert(fingerprints[prior] != hash);
    fingerprints[species] = hash;
    if (species == 0)
      memcpy(firstAdult, panel.pixels, sizeof(firstAdult));
  }

  auto speciesZeroStage = [&](uint8_t age) {
    d.clearDisplay();
    NativeUI::pet(d, 48, 48, 0, age, 0, false, 2);
    d.display();
    int pixels = 0;
    for (uint8_t pixel : panel.pixels)
      pixels += pixel != 0;
    return pixels;
  };
  const int babyPixels = speciesZeroStage(1);
  const int childPixels = speciesZeroStage(2);
  const int teenPixels = speciesZeroStage(3);
  const int adultPixels = speciesZeroStage(4);
  // Juvenile growth increases the family sprite; adult art switches to the
  // selected full-size roster silhouette and therefore need not use more ink.
  assert(babyPixels < childPixels && childPixels < teenPixels);
  assert(adultPixels != teenPixels);
  d.clearDisplay();
  NativeUI::pet(d, 48, 48, 0, 3, 0, false, 2);
  d.display();
  assert(memcmp(firstAdult, panel.pixels, sizeof(firstAdult)) != 0);

  // The bird's outlined torso must cover background detail with black.
  const int holeX = 11, holeY = 15;
  d.clearDisplay();
  d.fillRect(48,48,32,32,1);
  NativeUI::pet(d,48,48,0,4,0,false,2);
  d.display();
  assert(!panel.pixels[(48+holeY)*128+48+holeX]);
  // Exterior space remains transparent, including mirrored small icons.
  assert(panel.pixels[48*128+48]);

  uint64_t familyPrints[4][10]{};
  for (uint8_t stage = 0; stage <= 3; ++stage) {
    for (uint8_t family = 0; family < TAMACHIBI_FAMILY_COUNT; ++family) {
      const uint8_t pathCount = stage == 3 ? 2 : 1;
      for (uint8_t path = 0; path < pathCount; ++path) {
        d.clearDisplay();
        NativeUI::pet(d, 48, 48, family, stage, 0, false, 2, path);
        d.display();
        uint64_t hash = 1469598103934665603ULL;
        for (int i = 0; i < 128 * 128; ++i) {
          hash ^= panel.pixels[i];
          hash *= 1099511628211ULL;
        }
        if (path == 0) {
          for (uint8_t prior = 0; prior < family; ++prior) {
            if (familyPrints[stage][prior] == hash)
              fprintf(stderr, "duplicate family sprite: stage=%u families=%u,%u\n", stage, prior, family);
            assert(familyPrints[stage][prior] != hash);
          }
          familyPrints[stage][family] = hash;
        } else {
          assert(familyPrints[stage][family] != hash);
        }
      }
    }
  }

  // Each lineage has an idle pose that moves its own appendage/feature. Frame
  // 0 and 2 keep the body bob identical, so this catches part animation rather
  // than translation or bouncing the whole sprite.
  auto petFingerprint = [&](uint8_t species, uint8_t stage, uint8_t frame) {
    d.clearDisplay();
    NativeUI::pet(d, 48, 48, species, stage, frame, false, 2);
    d.display();
    uint64_t hash = 1469598103934665603ULL;
    for (int i = 0; i < 128 * 128; ++i) {
      hash ^= panel.pixels[i];
      hash *= 1099511628211ULL;
    }
    return hash;
  };
  for (uint8_t family = 0; family < TAMACHIBI_FAMILY_COUNT; ++family) {
    for (uint8_t stage = 0; stage <= 5; ++stage) {
      const uint8_t species = stage < 4 ? family : growthAdultSpecies(family, 2);
      const uint64_t still = petFingerprint(species, stage, 0);
      const uint64_t articulated = petFingerprint(species, stage, 2);
      if (still == articulated)
        fprintf(stderr, "idle part did not move: family=%u stage=%u\n", family, stage);
      assert(still != articulated);
    }
  }

  bool adultSeen[50]{};
  for (uint8_t family = 0; family < TAMACHIBI_FAMILY_COUNT; ++family) {
    for (uint8_t tier = 0; tier < TAMACHIBI_CARE_TIER_COUNT; ++tier) {
      const uint8_t adult = growthAdultSpecies(family, tier);
      assert(adult < 50 && !adultSeen[adult]);
      adultSeen[adult] = true;
      assert(growthFamilyForAdult(adult) == family);
      assert(growthCareTierForAdult(adult) == tier);
    }
  }
  for (bool seen : adultSeen) assert(seen);

  d.clearDisplay();
  NativeUI::pet(d, 48, 48, 0, 3, 0, false, 2, 0, false);
  d.display();
  uint8_t facingRight[128 * 128];
  memcpy(facingRight, panel.pixels, sizeof(facingRight));
  d.clearDisplay();
  NativeUI::pet(d, 48, 48, 0, 3, 0, false, 2, 0, true);
  d.display();
  assert(memcmp(facingRight, panel.pixels, sizeof(facingRight)) != 0);

  puts("Renderer checks passed: square geometry, framebuffer isolation, "
       "opaque body interiors and transparent exterior, 50 adult masks, 10 distinct lineages, "
       "care-split teens, animated lineage parts through elder, 50 mapped "
       "adult outcomes, and facing animation.");
}
