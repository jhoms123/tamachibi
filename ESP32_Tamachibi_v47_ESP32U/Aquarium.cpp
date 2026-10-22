#include "Aquarium.h"
#include <stddef.h>
#include <string.h>

// Main-game state and helpers used to integrate aquarium care with the pet.
extern byte happy, energy, health, clean, discipline;
extern byte ownerBond, ownerTrust, skillKind, skillSmart, skillStyle, cleanlinessScore;
extern byte stage, habitat;
extern unsigned int coins;
extern unsigned long ageTicks;
extern bool dead, sleeping, rtcCatchUpMode;
extern char petName[7];

extern void addStat(byte &value, byte amount);
extern void subStat(byte &value, byte amount);
extern void addSkill(byte &value, byte amount);
extern void addCoins(byte amount);
extern bool spendCoins(byte amount);
extern void changeBond(int amount);
extern void changeTrust(int amount);
extern void markRoutineAction(byte routineBit);
extern void recordGoalEvent(byte eventType, byte amount);
extern void markSaveDirty();
extern void clampAll();
extern void waitButtonRelease();
extern bool rightHoldExitCheck();
extern void clickSound();
extern void goodSound();
extern void badSound();
extern void winSound();
extern void careSound();
extern void drawCurrentPetTiny(int x, int y);
extern unsigned long difficultyTicks(unsigned long normalTicks);

namespace {
constexpr uint8_t EMPTY_FISH = 255;
constexpr uint32_t TICKS_PER_HOUR_AQ = 12UL;
constexpr uint32_t TICKS_PER_DAY_AQ = 288UL;
constexpr uint8_t ROUTINE_CARE_AQ = 4;
constexpr uint8_t ROUTINE_TALK_AQ = 8;
constexpr uint8_t ROUTINE_PLAY_AQ = 16;

// Twelve 16x8 monochrome fish silhouettes adapted for a 128x64 OLED.
// The source shapes were redrawn as original 1-bit sprites after reviewing
// the CC0 Kenney Fish Pack for clearer fins, tails, and species variation.
static const uint8_t FISH_BITMAPS[AQUARIUM_SPECIES_COUNT][16] PROGMEM = {
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

bool fishBitmapPixel(uint8_t species, uint8_t x, uint8_t y) {
  const uint8_t value = FISH_BITMAPS[species % AQUARIUM_SPECIES_COUNT][y * 2 + x / 8];
  return (value & (0x80U >> (x & 7))) != 0;
}

uint8_t boundedAdd(uint8_t value, uint8_t amount, uint8_t maximum = 10) {
  uint16_t next = static_cast<uint16_t>(value) + amount;
  return static_cast<uint8_t>(next > maximum ? maximum : next);
}

uint8_t boundedSub(uint8_t value, uint8_t amount) {
  return value > amount ? static_cast<uint8_t>(value - amount) : 0;
}
}

AquariumSystem::AquariumSystem(size_t storageBase, Adafruit_SSD1306 &display,
                               uint8_t buttonLeft, uint8_t buttonSelect,
                               uint8_t buttonRight)
    : storeA_(storageBase), storeB_(storageBase + sizeof(AquariumSave)),
      display_(display), btnL_(buttonLeft), btnS_(buttonSelect), btnR_(buttonRight) {
  reset();
}

void AquariumSystem::reset() {
  sequence_ = 0;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
    species_[i] = EMPTY_FISH;
    names_[i][0] = '\0';
    hunger_[i] = 8;
    mood_[i] = 8;
    health_[i] = 10;
    friendship_[i] = 0;
    ageTicks_[i] = 0;
  }
  tankClean_ = 10;
  waterQuality_ = 10;
  tankDecor_ = 0;
  tankLevel_ = 1;
  foodStock_ = 5;
  remedyStock_ = 1;
  selectedSlot_ = 0;
  aquariumVisits_ = 0;
  careStreak_ = 0;
  surpriseType_ = 0;
  discoveredMask_ = 0;
  lastRewardDay_ = 0;
  lastInteractionTick_ = 0;
}

uint32_t AquariumSystem::checksum(const AquariumSave &save) const {
  const uint8_t *raw = reinterpret_cast<const uint8_t *>(&save);
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < offsetof(AquariumSave, checksum); ++i) {
    hash ^= raw[i];
    hash *= 16777619UL;
  }
  return hash;
}

bool AquariumSystem::valid(const AquariumSave &save) const {
  return save.magic == AQUARIUM_SAVE_MAGIC &&
         save.version == AQUARIUM_SAVE_VERSION &&
         save.checksum == checksum(save);
}

void AquariumSystem::fillSave(AquariumSave &save) const {
  memset(&save, 0, sizeof(save));
  save.magic = AQUARIUM_SAVE_MAGIC;
  save.version = AQUARIUM_SAVE_VERSION;
  save.sequence = sequence_ + 1UL;
  memcpy(save.species, species_, sizeof(species_));
  memcpy(save.names, names_, sizeof(names_));
  memcpy(save.hunger, hunger_, sizeof(hunger_));
  memcpy(save.mood, mood_, sizeof(mood_));
  memcpy(save.health, health_, sizeof(health_));
  memcpy(save.friendship, friendship_, sizeof(friendship_));
  memcpy(save.ageTicks, ageTicks_, sizeof(ageTicks_));
  save.tankClean = tankClean_;
  save.waterQuality = waterQuality_;
  save.tankDecor = tankDecor_;
  save.tankLevel = tankLevel_;
  save.foodStock = foodStock_;
  save.remedyStock = remedyStock_;
  save.selectedSlot = selectedSlot_;
  save.aquariumVisits = aquariumVisits_;
  save.careStreak = careStreak_;
  save.surpriseType = surpriseType_;
  save.discoveredMask = discoveredMask_;
  save.lastRewardDay = lastRewardDay_;
  save.lastInteractionTick = lastInteractionTick_;
  save.checksum = checksum(save);
}

void AquariumSystem::applySave(const AquariumSave &save) {
  sequence_ = save.sequence;
  memcpy(species_, save.species, sizeof(species_));
  memcpy(names_, save.names, sizeof(names_));
  memcpy(hunger_, save.hunger, sizeof(hunger_));
  memcpy(mood_, save.mood, sizeof(mood_));
  memcpy(health_, save.health, sizeof(health_));
  memcpy(friendship_, save.friendship, sizeof(friendship_));
  memcpy(ageTicks_, save.ageTicks, sizeof(ageTicks_));
  tankClean_ = save.tankClean;
  waterQuality_ = save.waterQuality;
  tankDecor_ = save.tankDecor;
  tankLevel_ = save.tankLevel;
  foodStock_ = save.foodStock;
  remedyStock_ = save.remedyStock;
  selectedSlot_ = save.selectedSlot;
  aquariumVisits_ = save.aquariumVisits;
  careStreak_ = save.careStreak;
  surpriseType_ = save.surpriseType;
  discoveredMask_ = save.discoveredMask;
  lastRewardDay_ = save.lastRewardDay;
  lastInteractionTick_ = save.lastInteractionTick;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) names_[i][6] = '\0';
  clampState();
}

bool AquariumSystem::load() {
  AquariumSave a = storeA_.read();
  AquariumSave b = storeB_.read();
  const bool validA = valid(a);
  const bool validB = valid(b);
  if (!validA && !validB) {
    reset();
    return false;
  }
  if (validA && validB) applySave(a.sequence >= b.sequence ? a : b);
  else applySave(validA ? a : b);
  return true;
}

void AquariumSystem::saveNow() {
  AquariumSave save;
  fillSave(save);
  if (save.sequence & 1UL) storeA_.write(save);
  else storeB_.write(save);
  sequence_ = save.sequence;
}

void AquariumSystem::clampState() {
  tankClean_ = min((uint8_t)10, tankClean_);
  waterQuality_ = min((uint8_t)10, waterQuality_);
  tankDecor_ = min((uint8_t)5, tankDecor_);
  tankLevel_ = constrain(tankLevel_, (uint8_t)1, (uint8_t)3);
  selectedSlot_ %= AQUARIUM_FISH_SLOTS;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
    if (species_[i] >= AQUARIUM_SPECIES_COUNT) species_[i] = EMPTY_FISH;
    hunger_[i] = min((uint8_t)10, hunger_[i]);
    mood_[i] = min((uint8_t)10, mood_[i]);
    health_[i] = constrain(health_[i], (uint8_t)1, (uint8_t)10);
    friendship_[i] = min((uint8_t)100, friendship_[i]);
  }
}

bool AquariumSystem::slotOccupied(uint8_t slot) const {
  return slot < AQUARIUM_FISH_SLOTS && species_[slot] < AQUARIUM_SPECIES_COUNT;
}

uint8_t AquariumSystem::fishCount() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) if (slotOccupied(i)) ++count;
  return count;
}

uint8_t AquariumSystem::capacity() const {
  return min((uint8_t)AQUARIUM_FISH_SLOTS, (uint8_t)(tankLevel_ + 1));
}

bool AquariumSystem::hasFish() const { return fishCount() > 0; }

void AquariumSystem::developerUnlockAll() {
  tankLevel_ = 3;
  tankDecor_ = 5;
  foodStock_ = 99;
  remedyStock_ = 99;
  discoveredMask_ = static_cast<uint16_t>((1U << AQUARIUM_SPECIES_COUNT) - 1U);
  tankClean_ = 10;
  waterQuality_ = 10;
  markSaveDirty();
}

void AquariumSystem::developerAddFish(uint8_t species) {
  species %= AQUARIUM_SPECIES_COUNT;
  tankLevel_ = 3;
  uint8_t slot = firstEmptySlot();
  if (slot == EMPTY_FISH) slot = selectedSlot_ % AQUARIUM_FISH_SLOTS;
  species_[slot] = species;
  strncpy(names_[slot], defaultFishName(species), 7);
  names_[slot][6] = '\0';
  hunger_[slot] = 10;
  mood_[slot] = 10;
  health_[slot] = 10;
  friendship_[slot] = 100;
  ageTicks_[slot] = static_cast<uint16_t>(3UL * TICKS_PER_DAY_AQ);
  selectedSlot_ = slot;
  discoveredMask_ |= static_cast<uint16_t>(1U << species);
  foodStock_ = max((uint8_t)20, foodStock_);
  remedyStock_ = max((uint8_t)10, remedyStock_);
  tankClean_ = 10;
  waterQuality_ = 10;
  markSaveDirty();
}

void AquariumSystem::developerFillTank() {
  developerUnlockAll();
  const uint8_t testSpecies[AQUARIUM_FISH_SLOTS] = {0, 7, 10, 11};
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
    species_[i] = testSpecies[i];
    strncpy(names_[i], defaultFishName(testSpecies[i]), 7);
    names_[i][6] = '\0';
    hunger_[i] = 10;
    mood_[i] = 10;
    health_[i] = 10;
    friendship_[i] = 100;
    ageTicks_[i] = static_cast<uint16_t>(3UL * TICKS_PER_DAY_AQ);
  }
  selectedSlot_ = 0;
  markSaveDirty();
}

void AquariumSystem::developerMaxCare() {
  tankClean_ = 10;
  waterQuality_ = 10;
  foodStock_ = max((uint8_t)20, foodStock_);
  remedyStock_ = max((uint8_t)10, remedyStock_);
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
    if (!slotOccupied(i)) continue;
    hunger_[i] = 10;
    mood_[i] = 10;
    health_[i] = 10;
    friendship_[i] = 100;
  }
  markSaveDirty();
}

void AquariumSystem::developerNeglectTank() {
  if (!hasFish()) developerAddFish(0);
  tankClean_ = 1;
  waterQuality_ = 1;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
    if (!slotOccupied(i)) continue;
    hunger_[i] = 1;
    mood_[i] = 2;
    health_[i] = 3;
  }
  markSaveDirty();
}

bool AquariumSystem::healthy() const {
  if (!hasFish() || tankClean_ < 6 || waterQuality_ < 6) return false;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
    if (slotOccupied(i) && (hunger_[i] < 4 || health_[i] < 5 || mood_[i] < 4)) return false;
  }
  return true;
}

bool AquariumSystem::needsCare() const {
  if (!hasFish()) return false;
  if (tankClean_ <= 3 || waterQuality_ <= 3) return true;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
    if (slotOccupied(i) && (hunger_[i] <= 2 || health_[i] <= 3)) return true;
  }
  return false;
}

uint8_t AquariumSystem::firstEmptySlot() const {
  const uint8_t limit = capacity();
  for (uint8_t i = 0; i < limit; ++i) if (!slotOccupied(i)) return i;
  return EMPTY_FISH;
}

uint8_t AquariumSystem::nextOccupiedSlot(uint8_t from, int8_t direction) const {
  if (!hasFish()) return 0;
  uint8_t slot = from % AQUARIUM_FISH_SLOTS;
  for (uint8_t step = 0; step < AQUARIUM_FISH_SLOTS; ++step) {
    slot = direction < 0 ? (slot == 0 ? AQUARIUM_FISH_SLOTS - 1 : slot - 1)
                         : static_cast<uint8_t>((slot + 1) % AQUARIUM_FISH_SLOTS);
    if (slotOccupied(slot)) return slot;
  }
  return from;
}

const char *AquariumSystem::selectedFishName() const {
  if (!slotOccupied(selectedSlot_)) return "NO FISH";
  return names_[selectedSlot_][0] ? names_[selectedSlot_] : defaultFishName(species_[selectedSlot_]);
}

const __FlashStringHelper *AquariumSystem::speciesName(uint8_t species) const {
  switch (species % AQUARIUM_SPECIES_COUNT) {
    case 0: return F("Guppy");
    case 1: return F("Goldfish");
    case 2: return F("Betta");
    case 3: return F("Neon Tetra");
    case 4: return F("Molly");
    case 5: return F("Cory");
    case 6: return F("Angelfish");
    case 7: return F("Koi");
    case 8: return F("Puffer");
    case 9: return F("Clownfish");
    case 10: return F("Moonfin");
    default: return F("Starfin");
  }
}

const char *AquariumSystem::defaultFishName(uint8_t species) const {
  static const char names[AQUARIUM_SPECIES_COUNT][7] = {
    "GUPPY", "GOLDIE", "BETTA", "NEON", "MOLLY", "CORY",
    "ANGEL", "KOI", "PUFF", "CLOWN", "MOON", "STAR"
  };
  return names[species % AQUARIUM_SPECIES_COUNT];
}

uint8_t AquariumSystem::speciesCost(uint8_t species) const {
  static const uint8_t costs[AQUARIUM_SPECIES_COUNT] = {3, 5, 6, 5, 6, 7, 10, 12, 14, 12, 18, 22};
  return costs[species % AQUARIUM_SPECIES_COUNT];
}

bool AquariumSystem::speciesUnlocked(uint8_t species) const {
  if (species < 6) return true;
  if (species < 9) return tankLevel_ >= 2;
  return tankLevel_ >= 3;
}

const __FlashStringHelper *AquariumSystem::decorName(uint8_t decor) const {
  switch (decor) {
    case 0: return F("Plain gravel");
    case 1: return F("Water plants");
    case 2: return F("Tiny castle");
    case 3: return F("Bubble arch");
    case 4: return F("Coral garden");
    default: return F("Moon grotto");
  }
}

uint8_t AquariumSystem::fishLifeStage(uint8_t slot) const {
  if (!slotOccupied(slot)) return 0;
  if (ageTicks_[slot] < TICKS_PER_DAY_AQ) return 0;
  if (ageTicks_[slot] < 3UL * TICKS_PER_DAY_AQ) return 1;
  return 2;
}

bool AquariumSystem::allFishComfortable() const {
  if (!hasFish()) return false;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
    if (slotOccupied(i) && (hunger_[i] < 5 || mood_[i] < 5 || health_[i] < 6)) return false;
  }
  return tankClean_ >= 6 && waterQuality_ >= 6;
}

void AquariumSystem::showTextPopup(const char *line1, const char *line2, uint16_t holdMs) {
  display_.clearDisplay();
  display_.setTextWrap(false);
  display_.setTextSize(1);
  display_.setTextColor(SSD1306_WHITE);
  display_.drawRoundRect(3, 13, 122, 39, 5, SSD1306_WHITE);
  display_.setCursor(10, 23);
  display_.print(line1 ? line1 : "");
  display_.setCursor(10, 36);
  display_.print(line2 ? line2 : "");
  display_.display();
  delay(holdMs);
}

void AquariumSystem::showFlashPopup(const __FlashStringHelper *line1,
                                    const __FlashStringHelper *line2,
                                    uint16_t holdMs) {
  display_.clearDisplay();
  display_.setTextWrap(false);
  display_.setTextSize(1);
  display_.setTextColor(SSD1306_WHITE);
  display_.drawRoundRect(3, 13, 122, 39, 5, SSD1306_WHITE);
  display_.setCursor(10, 23);
  display_.print(line1);
  display_.setCursor(10, 36);
  display_.print(line2);
  display_.display();
  delay(holdMs);
}

void AquariumSystem::drawSmallIcon(int x, int y, bool inverse) {
  const uint16_t color = inverse ? SSD1306_BLACK : SSD1306_WHITE;
  display_.drawRect(x + 2, y + 3, 16, 12, color);
  display_.drawLine(x + 3, y + 7, x + 17, y + 7, color);
  display_.fillCircle(x + 10, y + 11, 2, color);
  display_.fillTriangle(x + 7, y + 11, x + 4, y + 8, x + 4, y + 14, color);
  display_.drawPixel(x + 11, y + 10, inverse ? SSD1306_WHITE : SSD1306_BLACK);
}

void AquariumSystem::drawFishSprite(uint8_t species, int x, int y, bool facingRight,
                                    uint8_t lifeStage, bool selected) {
  const uint8_t stage = min(static_cast<uint8_t>(2), lifeStage);
  const uint8_t targetW = stage == 0 ? 10 : (stage == 1 ? 13 : 16);
  const uint8_t targetH = stage == 0 ? 5 : (stage == 1 ? 7 : 8);
  const int left = x - targetW / 2;
  const int top = y - targetH / 2;
  const uint8_t id = species % AQUARIUM_SPECIES_COUNT;

  for (uint8_t dy = 0; dy < targetH; ++dy) {
    const uint8_t sy = static_cast<uint8_t>((static_cast<uint16_t>(dy) * 8U) / targetH);
    for (uint8_t dx = 0; dx < targetW; ++dx) {
      uint8_t sx = static_cast<uint8_t>((static_cast<uint16_t>(dx) * 16U) / targetW);
      if (!facingRight) sx = static_cast<uint8_t>(15U - sx);
      if (fishBitmapPixel(id, sx, sy)) display_.drawPixel(left + dx, top + dy, SSD1306_WHITE);
    }
  }

  const int eyeX = facingRight ? left + targetW - max(2, targetW / 4) : left + max(1, targetW / 4);
  const int eyeY = top + max(1, targetH / 3);
  display_.drawPixel(eyeX, eyeY, SSD1306_BLACK);

  // Species markings remain readable on a one-bit screen.
  if (id == 3) { // neon stripe
    display_.drawLine(left + 3, top + targetH / 2, left + targetW - 4, top + targetH / 2, SSD1306_BLACK);
  } else if (id == 7) { // koi patches
    display_.drawPixel(left + targetW / 2, top + 2, SSD1306_BLACK);
    display_.drawPixel(left + targetW / 2 + 2, top + targetH - 3, SSD1306_BLACK);
  } else if (id == 9) { // clownfish bands
    display_.drawFastVLine(left + targetW / 2 - 2, top + 1, max(1, targetH - 2), SSD1306_BLACK);
    display_.drawFastVLine(left + targetW / 2 + 3, top + 1, max(1, targetH - 2), SSD1306_BLACK);
  } else if (id == 10) { // moon crescent
    display_.drawPixel(left + targetW / 2, top + targetH / 2, SSD1306_BLACK);
    display_.drawPixel(left + targetW / 2 + 1, top + targetH / 2, SSD1306_BLACK);
  } else if (id == 11) { // star sparkle
    display_.drawPixel(left + targetW / 2, top + 1, SSD1306_BLACK);
    display_.drawPixel(left + targetW / 2, top + targetH - 2, SSD1306_BLACK);
  }

  // The old full rectangle looked like a graphical bug. A small caret now
  // indicates the selected fish without surrounding or obscuring the sprite.
  if (selected) {
    const int markerY = min(48, top + targetH + 2);
    display_.drawLine(x - 2, markerY, x, markerY + 2, SSD1306_WHITE);
    display_.drawLine(x, markerY + 2, x + 2, markerY, SSD1306_WHITE);
  }
}

void AquariumSystem::drawTankMeters() {
  display_.setTextSize(1);
  display_.setTextColor(SSD1306_WHITE);
  display_.setCursor(2, 56);
  display_.print(F("W")); display_.print(waterQuality_);
  display_.print(F(" C")); display_.print(tankClean_);
  display_.print(F(" F")); display_.print(foodStock_);
  display_.setCursor(91, 56);
  display_.print(fishCount()); display_.print(F("/")); display_.print(capacity());
}

void AquariumSystem::drawTankScene(uint8_t selected, uint16_t frame, bool includePet) {
  display_.clearDisplay();
  display_.setTextWrap(false);
  display_.setTextSize(1);
  display_.setTextColor(SSD1306_WHITE);
  display_.setCursor(2, 0);
  if (slotOccupied(selected)) display_.print(names_[selected]);
  else display_.print(F("Aquarium"));

  const int tankX = includePet ? 23 : 2;
  const int tankW = includePet ? 103 : 124;
  display_.drawRoundRect(tankX, 10, tankW, 44, 4, SSD1306_WHITE);
  display_.drawLine(tankX + 2, 17, tankX + tankW - 3, 17, SSD1306_WHITE);
  display_.drawLine(tankX + 2, 49, tankX + tankW - 3, 49, SSD1306_WHITE);

  if (includePet) {
    drawCurrentPetTiny(3, 35);
    display_.drawLine(1, 52, 21, 52, SSD1306_WHITE);
  }

  if (tankDecor_ >= 1) {
    display_.drawLine(tankX + 11, 48, tankX + 11, 37, SSD1306_WHITE);
    display_.drawCircle(tankX + 7, 37, 3, SSD1306_WHITE);
    display_.drawCircle(tankX + 15, 34, 3, SSD1306_WHITE);
  }
  if (tankDecor_ >= 2) {
    display_.drawRect(tankX + tankW - 27, 39, 18, 10, SSD1306_WHITE);
    display_.drawTriangle(tankX + tankW - 29, 39, tankX + tankW - 18, 31, tankX + tankW - 7, 39, SSD1306_WHITE);
  }
  if (tankDecor_ >= 3) {
    display_.drawCircle(tankX + tankW / 2, 26 + (frame % 5), 2, SSD1306_WHITE);
    display_.drawCircle(tankX + tankW / 2 + 7, 20 + (frame % 7), 1, SSD1306_WHITE);
  }
  if (tankDecor_ >= 4) {
    display_.drawCircle(tankX + 27, 45, 4, SSD1306_WHITE);
    display_.drawCircle(tankX + 34, 43, 5, SSD1306_WHITE);
  }
  if (tankDecor_ >= 5) {
    display_.drawCircle(tankX + tankW - 15, 24, 5, SSD1306_WHITE);
    display_.drawPixel(tankX + tankW - 15, 24, SSD1306_BLACK);
  }

  uint8_t visualIndex = 0;
  for (uint8_t slot = 0; slot < AQUARIUM_FISH_SLOTS; ++slot) {
    if (!slotOccupied(slot)) continue;

    // Each fish follows a slow triangular path in its own lane. Direction only
    // changes at the tank ends, eliminating the old rapid flipping and jumps.
    const uint16_t halfCycle = static_cast<uint16_t>(58 + slot * 11);
    const uint16_t cycle = static_cast<uint16_t>(halfCycle * 2U);
    const uint16_t phase = static_cast<uint16_t>((frame + slot * 29U) % cycle);
    const bool right = phase < halfCycle;
    const uint16_t travelPhase = right ? phase : static_cast<uint16_t>(cycle - phase);
    const int margin = 12;
    const int span = max(12, tankW - margin * 2 - 1);
    const int travel = static_cast<int>((static_cast<uint32_t>(travelPhase) * span) / halfCycle);
    const int px = tankX + margin + travel;

    const int laneY[4] = {24, 31, 38, 45};
    const int bobPhase = static_cast<int>((frame + slot * 7U) % 24U);
    const int bob = bobPhase < 6 ? 0 : (bobPhase < 12 ? 1 : (bobPhase < 18 ? 0 : -1));
    const int py = laneY[min(static_cast<uint8_t>(3), visualIndex)] + bob;
    drawFishSprite(species_[slot], px, py, right, fishLifeStage(slot), slot == selected);
    ++visualIndex;
  }

  if (tankClean_ <= 3) {
    for (uint8_t i = 0; i < 8; ++i) display_.drawPixel(tankX + 6 + (i * 13) % (tankW - 12), 46 - (i % 3), SSD1306_WHITE);
  }
  drawTankMeters();
}

void AquariumSystem::interactWithFish(uint8_t slot) {
  if (!slotOccupied(slot)) return;
  mood_[slot] = boundedAdd(mood_[slot], 2);
  friendship_[slot] = boundedAdd(friendship_[slot], 2, 100);
  if (ageTicks - lastInteractionTick_ >= TICKS_PER_HOUR_AQ || lastInteractionTick_ == 0) {
    lastInteractionTick_ = ageTicks;
    addStat(happy, 2);
    addStat(energy, 1);
    addSkill(skillKind, 1);
    changeBond(1);
    markRoutineAction(ROUTINE_PLAY_AQ);
    recordGoalEvent(2, 1);
  }
  goodSound();
  markSaveDirty();
}

void AquariumSystem::watchTank() {
  if (!hasFish()) {
    showFlashPopup(F("The tank is empty"), F("Buy your first fish"));
    return;
  }
  selectedSlot_ = slotOccupied(selectedSlot_) ? selectedSlot_ : nextOccupiedSlot(0, 1);
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  uint16_t frame = 0;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_);
    const bool s = digitalRead(btnS_);
    const bool r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { selectedSlot_ = nextOccupiedSlot(selectedSlot_, -1); clickSound(); delay(110); }
    if (oldR == HIGH && r == LOW) { selectedSlot_ = nextOccupiedSlot(selectedSlot_, 1); clickSound(); delay(110); }
    if (oldS == HIGH && s == LOW) { interactWithFish(selectedSlot_); delay(120); }
    oldL = l; oldS = s; oldR = r;
    drawTankScene(selectedSlot_, frame++, true);
    display_.display();
    delay(110);
  }
}

void AquariumSystem::fishStatusPage() {
  if (!hasFish()) {
    showFlashPopup(F("No fish yet"), F("Visit the fish shop"));
    return;
  }
  selectedSlot_ = slotOccupied(selectedSlot_) ? selectedSlot_ : nextOccupiedSlot(0, 1);
  bool oldL = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_);
    const bool r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { selectedSlot_ = nextOccupiedSlot(selectedSlot_, -1); clickSound(); delay(110); }
    if (oldR == HIGH && r == LOW) { selectedSlot_ = nextOccupiedSlot(selectedSlot_, 1); clickSound(); delay(110); }
    oldL = l; oldR = r;

    display_.clearDisplay();
    display_.setTextWrap(false);
    display_.setTextSize(1);
    display_.setTextColor(SSD1306_WHITE);
    display_.setCursor(0, 0); display_.print(names_[selectedSlot_]);
    display_.setCursor(72, 0); display_.print(speciesName(species_[selectedSlot_]));
    drawFishSprite(species_[selectedSlot_], 18, 28, true, fishLifeStage(selectedSlot_), false);
    display_.setCursor(39, 15); display_.print(F("Food ")); display_.print(hunger_[selectedSlot_]);
    display_.setCursor(39, 26); display_.print(F("Mood ")); display_.print(mood_[selectedSlot_]);
    display_.setCursor(39, 37); display_.print(F("Health ")); display_.print(health_[selectedSlot_]);
    display_.setCursor(39, 48); display_.print(F("Friend ")); display_.print(friendship_[selectedSlot_]);
    drawTankMeters();
    display_.display();
    delay(25);
  }
}

void AquariumSystem::feedFish() {
  if (!hasFish()) { showFlashPopup(F("No fish to feed"), F("Buy a fish first")); return; }
  if (foodStock_ == 0) { showFlashPopup(F("Fish food is empty"), F("Buy supplies")); return; }

  bool alreadyFull = true;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) if (slotOccupied(i) && hunger_[i] < 9) alreadyFull = false;
  --foodStock_;
  if (alreadyFull) {
    waterQuality_ = boundedSub(waterQuality_, 2);
    tankClean_ = boundedSub(tankClean_, 1);
    showFlashPopup(F("Too much food"), F("The water clouded"));
    badSound();
  } else {
    for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
      if (!slotOccupied(i)) continue;
      hunger_[i] = boundedAdd(hunger_[i], 4);
      mood_[i] = boundedAdd(mood_[i], 1);
      friendship_[i] = boundedAdd(friendship_[i], 1, 100);
    }
    addStat(happy, 1);
    addSkill(skillKind, 1);
    changeBond(1);
    markRoutineAction(ROUTINE_CARE_AQ);
    careSound();
    showFlashPopup(F("Fish fed"), F("Everyone is nibbling"));
  }
  markSaveDirty();
}

void AquariumSystem::cleanTankAction() {
  if (!hasFish()) { showFlashPopup(F("The tank is empty"), F("Nothing to clean")); return; }
  if (tankClean_ >= 9 && waterQuality_ >= 9) { showFlashPopup(F("Tank already clean"), F("Water looks clear")); return; }

  for (uint8_t frame = 0; frame < 9; ++frame) {
    drawTankScene(selectedSlot_, frame, true);
    display_.drawLine(25 + frame * 9, 18, 18 + frame * 9, 49, SSD1306_WHITE);
    display_.display();
    delay(90);
  }
  tankClean_ = 10;
  waterQuality_ = 10;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) if (slotOccupied(i)) mood_[i] = boundedAdd(mood_[i], 1);
  subStat(energy, 1);
  addStat(happy, 1);
  addSkill(skillKind, 1);
  cleanlinessScore = boundedAdd(cleanlinessScore, 2, 100);
  changeBond(1);
  changeTrust(1);
  markRoutineAction(ROUTINE_CARE_AQ);
  recordGoalEvent(3, 1);
  careSound();
  showFlashPopup(F("Tank cleaned"), F("The fish look calm"));
  markSaveDirty();
}

void AquariumSystem::renameFish(uint8_t slot) {
  if (!slotOccupied(slot)) return;
  static const char characters[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  const uint8_t characterCount = sizeof(characters) - 1;
  uint8_t position = 0;
  uint8_t selected = 1;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();

  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_);
    const bool s = digitalRead(btnS_);
    const bool r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) {
      selected = selected == 0 ? characterCount - 1 : selected - 1;
      names_[slot][position] = characters[selected]; clickSound(); delay(100);
    }
    if (oldR == HIGH && r == LOW) {
      selected = static_cast<uint8_t>((selected + 1) % characterCount);
      names_[slot][position] = characters[selected]; clickSound(); delay(100);
    }
    if (oldS == HIGH && s == LOW) {
      ++position;
      if (position >= 6) break;
      selected = 1;
      for (uint8_t i = 0; i < characterCount; ++i) if (characters[i] == names_[slot][position]) selected = i;
      clickSound(); delay(100);
    }
    oldL = l; oldS = s; oldR = r;

    display_.clearDisplay();
    display_.setTextWrap(false);
    display_.setTextSize(1);
    display_.setTextColor(SSD1306_WHITE);
    display_.setCursor(35, 4); display_.print(F("Fish name"));
    for (uint8_t i = 0; i < 6; ++i) {
      const int x = 25 + i * 13;
      display_.drawRect(x, 23, 11, 17, SSD1306_WHITE);
      display_.setCursor(x + 3, 28); display_.print(names_[slot][i] ? names_[slot][i] : ' ');
      if (i == position) display_.drawLine(x, 44, x + 10, 44, SSD1306_WHITE);
    }
    drawFishSprite(species_[slot], 64, 55, true, fishLifeStage(slot), false);
    display_.display();
    delay(20);
  }

  names_[slot][6] = '\0';
  for (int8_t i = 5; i >= 0 && names_[slot][i] == ' '; --i) names_[slot][i] = '\0';
  if (names_[slot][0] == '\0') strncpy(names_[slot], defaultFishName(species_[slot]), 7);
  goodSound();
  showTextPopup(names_[slot], "name saved");
  markSaveDirty();
}

void AquariumSystem::buyFish() {
  const uint8_t slot = firstEmptySlot();
  if (slot == EMPTY_FISH) { showFlashPopup(F("Tank is full"), F("Upgrade for space")); return; }

  uint8_t pick = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_);
    const bool s = digitalRead(btnS_);
    const bool r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? AQUARIUM_SPECIES_COUNT - 1 : pick - 1; clickSound(); delay(110); }
    if (oldR == HIGH && r == LOW) { pick = static_cast<uint8_t>((pick + 1) % AQUARIUM_SPECIES_COUNT); clickSound(); delay(110); }
    if (oldS == HIGH && s == LOW) {
      if (!speciesUnlocked(pick)) { badSound(); showFlashPopup(F("Fish is locked"), F("Upgrade the tank")); }
      else if (!spendCoins(speciesCost(pick))) { badSound(); showFlashPopup(F("Not enough coins"), F("Work or explore")); }
      else {
        species_[slot] = pick;
        strncpy(names_[slot], defaultFishName(pick), 7);
        hunger_[slot] = 8; mood_[slot] = 8; health_[slot] = 10; friendship_[slot] = 1; ageTicks_[slot] = 0;
        selectedSlot_ = slot;
        discoveredMask_ |= static_cast<uint16_t>(1U << pick);
        waterQuality_ = boundedSub(waterQuality_, 1);
        winSound();
        showFlashPopup(F("New fish!"), speciesName(pick));
        renameFish(slot);
        markSaveDirty();
        return;
      }
      waitButtonRelease();
    }
    oldL = l; oldS = s; oldR = r;

    display_.clearDisplay();
    display_.setTextWrap(false);
    display_.setTextSize(1);
    display_.setTextColor(SSD1306_WHITE);
    display_.setCursor(34, 0); display_.print(F("Fish Shop"));
    drawFishSprite(pick, 64, 28, true, pick < 6 ? 1 : 2, false);
    display_.setCursor(10, 45); display_.print(speciesName(pick));
    display_.setCursor(90, 45); display_.print(F("$")); display_.print(speciesCost(pick));
    display_.setCursor(10, 56); display_.print(speciesUnlocked(pick) ? F("Available") : F("Tank upgrade needed"));
    display_.display();
    delay(25);
  }
}

void AquariumSystem::renameSelectedFish() {
  if (!hasFish()) { showFlashPopup(F("No fish to name"), F("Buy a fish first")); return; }
  selectedSlot_ = slotOccupied(selectedSlot_) ? selectedSlot_ : nextOccupiedSlot(0, 1);
  renameFish(selectedSlot_);
}

void AquariumSystem::decorateTank() {
  if (tankDecor_ >= 5) { showFlashPopup(F("Decor complete"), F("Moon grotto active")); return; }
  const uint8_t next = tankDecor_ + 1;
  const uint8_t cost = static_cast<uint8_t>(3 + next * 2);
  if (!spendCoins(cost)) { badSound(); showFlashPopup(F("Need more coins"), F("Decor stays the same")); return; }
  tankDecor_ = next;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) if (slotOccupied(i)) mood_[i] = boundedAdd(mood_[i], 2);
  addStat(happy, 1);
  addSkill(skillStyle, 1);
  winSound();
  showFlashPopup(F("Tank decorated"), decorName(tankDecor_));
  markSaveDirty();
}

void AquariumSystem::upgradeTank() {
  if (tankLevel_ >= 3) { showFlashPopup(F("Largest tank owned"), F("Four fish can live here")); return; }
  const uint8_t cost = tankLevel_ == 1 ? 12 : 24;
  if (!spendCoins(cost)) { badSound(); showFlashPopup(F("Need more coins"), F("Upgrade later")); return; }
  ++tankLevel_;
  tankClean_ = 10;
  waterQuality_ = 10;
  winSound();
  showFlashPopup(F("Tank upgraded"), tankLevel_ == 2 ? F("Three fish capacity") : F("Four fish capacity"));
  markSaveDirty();
}

void AquariumSystem::suppliesShop() {
  uint8_t pick = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_);
    const bool s = digitalRead(btnS_);
    const bool r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { pick ^= 1; clickSound(); delay(110); }
    if (oldR == HIGH && r == LOW) { pick ^= 1; clickSound(); delay(110); }
    if (oldS == HIGH && s == LOW) {
      const uint8_t cost = pick == 0 ? 2 : 4;
      if (!spendCoins(cost)) { badSound(); showFlashPopup(F("Need more coins"), F("Try again later")); }
      else if (pick == 0) { foodStock_ = boundedAdd(foodStock_, 5, 99); goodSound(); showFlashPopup(F("Fish food +5"), F("Supplies stocked")); }
      else { remedyStock_ = boundedAdd(remedyStock_, 1, 20); goodSound(); showFlashPopup(F("Remedy +1"), F("Medicine stocked")); }
      markSaveDirty();
      return;
    }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay();
    display_.setTextWrap(false);
    display_.setTextSize(1);
    display_.setTextColor(SSD1306_WHITE);
    display_.setCursor(29, 2); display_.print(F("Tank Supplies"));
    display_.drawRoundRect(12, 18, 104, 30, 5, SSD1306_WHITE);
    display_.setCursor(25, 28);
    if (pick == 0) { display_.print(F("Food x5  $2")); display_.setCursor(25, 39); display_.print(F("Stock ")); display_.print(foodStock_); }
    else { display_.print(F("Remedy x1 $4")); display_.setCursor(25, 39); display_.print(F("Stock ")); display_.print(remedyStock_); }
    display_.display();
    delay(25);
  }
}

void AquariumSystem::treatFish() {
  if (!hasFish()) { showFlashPopup(F("No fish to treat"), F("The tank is empty")); return; }
  selectedSlot_ = slotOccupied(selectedSlot_) ? selectedSlot_ : nextOccupiedSlot(0, 1);
  if (health_[selectedSlot_] >= 9) { showTextPopup(names_[selectedSlot_], "already feels well"); return; }
  if (remedyStock_ == 0) { showFlashPopup(F("No fish remedy"), F("Buy supplies")); return; }
  --remedyStock_;
  health_[selectedSlot_] = boundedAdd(health_[selectedSlot_], 5);
  mood_[selectedSlot_] = boundedAdd(mood_[selectedSlot_], 2);
  friendship_[selectedSlot_] = boundedAdd(friendship_[selectedSlot_], 3, 100);
  addSkill(skillKind, 1);
  addStat(happy, 1);
  changeBond(1);
  markRoutineAction(ROUTINE_CARE_AQ);
  careSound();
  showTextPopup(names_[selectedSlot_], "is feeling better");
  markSaveDirty();
}

void AquariumSystem::maybeCreateFry(bool catchUpMode) {
  if (tankLevel_ < 3 || fishCount() < 2 || firstEmptySlot() == EMPTY_FISH || careStreak_ < 5) return;
  if (random(0, 14) != 0) return;
  const uint8_t slot = firstEmptySlot();
  const uint8_t newSpecies = random(0, 2) == 0 ? 10 : 11;
  species_[slot] = newSpecies;
  strncpy(names_[slot], "FRY", 7);
  hunger_[slot] = 8; mood_[slot] = 9; health_[slot] = 10; friendship_[slot] = 2; ageTicks_[slot] = 0;
  discoveredMask_ |= static_cast<uint16_t>(1U << newSpecies);
  surpriseType_ = 2;
  if (!catchUpMode) selectedSlot_ = slot;
}

void AquariumSystem::applyDailyTankEffects(bool catchUpMode) {
  const uint32_t day = ageTicks / TICKS_PER_DAY_AQ;
  if (lastRewardDay_ == day) return;
  lastRewardDay_ = day;

  if (allFishComfortable()) {
    careStreak_ = min((uint8_t)99, (uint8_t)(careStreak_ + 1));
    addStat(happy, 1);
    addSkill(skillKind, 1);
    cleanlinessScore = boundedAdd(cleanlinessScore, 1, 100);
    if ((day & 1U) == 0) changeBond(1);

    bool hasKoi = false, hasCory = false, hasMoon = false, hasStar = false;
    for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
      if (!slotOccupied(i)) continue;
      hasCory |= species_[i] == 5;
      hasKoi |= species_[i] == 7;
      hasMoon |= species_[i] == 10;
      hasStar |= species_[i] == 11;
    }
    if (hasCory) tankClean_ = boundedAdd(tankClean_, 1);
    if (hasKoi && random(0, 3) == 0) { addCoins(1); surpriseType_ = 1; }
    if (hasMoon) addStat(energy, 1);
    if (hasStar) addSkill(skillSmart, 1);
    maybeCreateFry(catchUpMode);
  } else {
    careStreak_ = 0;
    if (needsCare()) subStat(happy, 1);
  }
}

void AquariumSystem::tick(bool catchUpMode) {
  if (!hasFish()) return;

  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
    if (!slotOccupied(i)) continue;
    if (ageTicks_[i] < 65535) ++ageTicks_[i];
    if (ageTicks_[i] % difficultyTicks(6UL * TICKS_PER_HOUR_AQ) == 0) hunger_[i] = boundedSub(hunger_[i], 1);
    if (ageTicks_[i] % difficultyTicks(12UL * TICKS_PER_HOUR_AQ) == 0 && (tankClean_ < 6 || waterQuality_ < 6)) mood_[i] = boundedSub(mood_[i], 1);
    if (ageTicks_[i] % difficultyTicks(TICKS_PER_DAY_AQ) == 0) {
      if (hunger_[i] <= 2 || tankClean_ <= 3 || waterQuality_ <= 3) {
        health_[i] = max((uint8_t)1, boundedSub(health_[i], 1));
        mood_[i] = boundedSub(mood_[i], 1);
      } else {
        health_[i] = boundedAdd(health_[i], 1);
        mood_[i] = boundedAdd(mood_[i], tankDecor_ >= 2 ? 2 : 1);
        friendship_[i] = boundedAdd(friendship_[i], 1, 100);
      }
    }
  }

  const uint16_t totalFish = fishCount();
  if (ageTicks % difficultyTicks(12UL * TICKS_PER_HOUR_AQ) == 0) tankClean_ = boundedSub(tankClean_, totalFish >= 3 ? 2 : 1);
  if (ageTicks % difficultyTicks(18UL * TICKS_PER_HOUR_AQ) == 0) waterQuality_ = boundedSub(waterQuality_, 1);
  if (tankDecor_ >= 1 && ageTicks % difficultyTicks(TICKS_PER_DAY_AQ) == 0) waterQuality_ = boundedAdd(waterQuality_, 1);
  if (ageTicks % TICKS_PER_DAY_AQ == 0) applyDailyTankEffects(catchUpMode);

  clampState();
  markSaveDirty();
}

void AquariumSystem::showPetComment() {
  if (!hasFish()) {
    showTextPopup(petName, "wants a fish");
    return;
  }
  selectedSlot_ = slotOccupied(selectedSlot_) ? selectedSlot_ : nextOccupiedSlot(0, 1);
  if (surpriseType_ == 2) {
    surpriseType_ = 0;
    showTextPopup(petName, "found a tiny fry!");
    markSaveDirty();
    return;
  }
  if (surpriseType_ == 1) {
    surpriseType_ = 0;
    showTextPopup(petName, "found a lucky coin");
    markSaveDirty();
    return;
  }

  char line[22];
  const char *fishName = selectedFishName();
  if (hunger_[selectedSlot_] <= 2) snprintf(line, sizeof(line), "%s needs food", fishName);
  else if (health_[selectedSlot_] <= 3) snprintf(line, sizeof(line), "%s looks sick", fishName);
  else if (tankClean_ <= 3) snprintf(line, sizeof(line), "%s needs clean water", fishName);
  else if (friendship_[selectedSlot_] >= 60) snprintf(line, sizeof(line), "%s knows us", fishName);
  else if (mood_[selectedSlot_] >= 8) snprintf(line, sizeof(line), "%s waved at me", fishName);
  else {
    switch (species_[selectedSlot_] % 5) {
      case 0: snprintf(line, sizeof(line), "I watched %s", fishName); break;
      case 1: snprintf(line, sizeof(line), "%s made bubbles", fishName); break;
      case 2: snprintf(line, sizeof(line), "%s is very brave", fishName); break;
      case 3: snprintf(line, sizeof(line), "%s likes the plants", fishName); break;
      default: snprintf(line, sizeof(line), "%s swims so fast", fishName); break;
    }
  }
  showTextPopup(petName, line, 1200);
  markRoutineAction(ROUTINE_TALK_AQ);
  changeBond(1);
}

void AquariumSystem::runMenu() {
  static const uint8_t OPTION_COUNT = 10;
  uint8_t option = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  ++aquariumVisits_;

  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_);
    const bool s = digitalRead(btnS_);
    const bool r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { option = option == 0 ? OPTION_COUNT - 1 : option - 1; clickSound(); delay(110); }
    if (oldR == HIGH && r == LOW) { option = static_cast<uint8_t>((option + 1) % OPTION_COUNT); clickSound(); delay(110); }
    if (oldS == HIGH && s == LOW) {
      waitButtonRelease();
      switch (option) {
        case 0: watchTank(); break;
        case 1: fishStatusPage(); break;
        case 2: feedFish(); break;
        case 3: cleanTankAction(); break;
        case 4: buyFish(); break;
        case 5: renameSelectedFish(); break;
        case 6: decorateTank(); break;
        case 7: upgradeTank(); break;
        case 8: suppliesShop(); break;
        default: treatFish(); break;
      }
      oldL = oldS = oldR = HIGH;
      waitButtonRelease();
    }
    oldL = l; oldS = s; oldR = r;

    display_.clearDisplay();
    display_.setTextWrap(false);
    display_.setTextSize(1);
    display_.setTextColor(SSD1306_WHITE);
    display_.setCursor(33, 0); display_.print(F("Aquarium"));
    display_.drawRoundRect(9, 16, 110, 32, 5, SSD1306_WHITE);
    display_.setCursor(25, 28);
    switch (option) {
      case 0: display_.print(F("Watch the tank")); break;
      case 1: display_.print(F("Fish status")); break;
      case 2: display_.print(F("Feed fish")); break;
      case 3: display_.print(F("Clean tank")); break;
      case 4: display_.print(F("Buy fish")); break;
      case 5: display_.print(F("Name fish")); break;
      case 6: display_.print(F("Decorate tank")); break;
      case 7: display_.print(F("Upgrade tank")); break;
      case 8: display_.print(F("Buy supplies")); break;
      default: display_.print(F("Treat fish")); break;
    }
    drawTankMeters();
    if (needsCare() && (millis() / 350UL) % 2 == 0) {
      display_.setCursor(102, 0); display_.print(F("!"));
    }
    display_.display();
    delay(25);
  }
}
