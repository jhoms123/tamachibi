#include "Botany.h"
#include "NativeUI.h"
#include "LifeWorld.h"
#include "GameplayActions.h"
#include <stddef.h>
#include <string.h>

extern byte happy, energy, health, clean, discipline;
extern byte ownerBond, ownerTrust, skillKind, skillSmart, skillStyle;
extern byte gardenLevel, gardenHarvests;
extern byte berryInv, herbInv, vegInv, spiceInv;
extern unsigned int coins;
extern unsigned long ageTicks;
extern bool rtcCatchUpMode;
extern LifeWorldSystem lifeWorld;

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
extern unsigned long difficultyTicks(unsigned long normalTicks);

namespace {
constexpr uint8_t EMPTY_PLANT = 255;
constexpr uint32_t TICKS_PER_DAY_BT = 288UL;
constexpr uint8_t ROUTINE_CARE_BT = 4;
constexpr uint8_t ROUTINE_PLAY_BT = 16;
constexpr uint16_t FLORIST_ORDER_DONE = 1U << 10;
constexpr uint16_t FLORIST_RANK_MASK = 7U << 11;

uint8_t addBounded(uint8_t value, uint8_t amount, uint8_t maximum = 10) {
  const uint16_t next = static_cast<uint16_t>(value) + amount;
  return static_cast<uint8_t>(next > maximum ? maximum : next);
}

uint8_t subBounded(uint8_t value, uint8_t amount) {
  return value > amount ? static_cast<uint8_t>(value - amount) : 0;
}

}

BotanySystem::BotanySystem(size_t storageBase, GameDisplay &display,
                           uint8_t buttonLeft, uint8_t buttonSelect,
                           uint8_t buttonRight)
    : storeA_(storageBase), storeB_(storageBase + sizeof(BotanySave)),
      display_(display), btnL_(buttonLeft), btnS_(buttonSelect), btnR_(buttonRight) {
  reset();
}

void BotanySystem::reset() {
  sequence_ = 0;
  for (uint8_t i = 0; i < BOTANY_GARDEN_PLOTS; ++i) {
    cropSpecies_[i] = EMPTY_PLANT;
    cropAge_[i] = 0;
    cropWater_[i] = 6;
    cropHealth_[i] = 10;
    cropCare_[i] = 0;
  }
  for (uint8_t i = 0; i < BOTANY_INDOOR_POTS; ++i) {
    indoorSpecies_[i] = EMPTY_PLANT;
    indoorAge_[i] = 0;
    indoorWater_[i] = 6;
    indoorHealth_[i] = 10;
    indoorCare_[i] = 0;
  }
  gardenCapacity_ = 2;
  indoorCapacity_ = 2;
  selectedGarden_ = 0;
  selectedIndoor_ = 0;
  discoveredCrops_ = 0;
  discoveredIndoor_ = 0;
  lastBoostDay_ = 0xFFFFFFFFUL;
}

uint32_t BotanySystem::checksum(const BotanySave &save) const {
  const uint8_t *raw = reinterpret_cast<const uint8_t *>(&save);
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < offsetof(BotanySave, checksum); ++i) {
    hash ^= raw[i];
    hash *= 16777619UL;
  }
  return hash;
}

bool BotanySystem::valid(const BotanySave &save) const {
  return save.magic == BOTANY_SAVE_MAGIC &&
         save.version == BOTANY_SAVE_VERSION &&
         save.checksum == checksum(save);
}

void BotanySystem::fillSave(BotanySave &save) const {
  memset(&save, 0, sizeof(save));
  save.magic = BOTANY_SAVE_MAGIC;
  save.version = BOTANY_SAVE_VERSION;
  save.sequence = sequence_ + 1UL;
  memcpy(save.cropSpecies, cropSpecies_, sizeof(cropSpecies_));
  memcpy(save.cropAge, cropAge_, sizeof(cropAge_));
  memcpy(save.cropWater, cropWater_, sizeof(cropWater_));
  memcpy(save.cropHealth, cropHealth_, sizeof(cropHealth_));
  memcpy(save.cropCare, cropCare_, sizeof(cropCare_));
  memcpy(save.indoorSpecies, indoorSpecies_, sizeof(indoorSpecies_));
  memcpy(save.indoorAge, indoorAge_, sizeof(indoorAge_));
  memcpy(save.indoorWater, indoorWater_, sizeof(indoorWater_));
  memcpy(save.indoorHealth, indoorHealth_, sizeof(indoorHealth_));
  memcpy(save.indoorCare, indoorCare_, sizeof(indoorCare_));
  save.gardenCapacity = gardenCapacity_;
  save.indoorCapacity = indoorCapacity_;
  save.selectedGarden = selectedGarden_;
  save.selectedIndoor = selectedIndoor_;
  save.discoveredCrops = discoveredCrops_;
  save.discoveredIndoor = discoveredIndoor_;
  save.lastBoostDay = lastBoostDay_;
  save.checksum = checksum(save);
}

void BotanySystem::applySave(const BotanySave &save) {
  sequence_ = save.sequence;
  memcpy(cropSpecies_, save.cropSpecies, sizeof(cropSpecies_));
  memcpy(cropAge_, save.cropAge, sizeof(cropAge_));
  memcpy(cropWater_, save.cropWater, sizeof(cropWater_));
  memcpy(cropHealth_, save.cropHealth, sizeof(cropHealth_));
  memcpy(cropCare_, save.cropCare, sizeof(cropCare_));
  memcpy(indoorSpecies_, save.indoorSpecies, sizeof(indoorSpecies_));
  memcpy(indoorAge_, save.indoorAge, sizeof(indoorAge_));
  memcpy(indoorWater_, save.indoorWater, sizeof(indoorWater_));
  memcpy(indoorHealth_, save.indoorHealth, sizeof(indoorHealth_));
  memcpy(indoorCare_, save.indoorCare, sizeof(indoorCare_));
  gardenCapacity_ = save.gardenCapacity;
  indoorCapacity_ = save.indoorCapacity;
  selectedGarden_ = save.selectedGarden;
  selectedIndoor_ = save.selectedIndoor;
  discoveredCrops_ = save.discoveredCrops;
  discoveredIndoor_ = save.discoveredIndoor;
  lastBoostDay_ = save.lastBoostDay;
  clampState();
}

void BotanySystem::clampState() {
  gardenCapacity_ = constrain(gardenCapacity_, static_cast<uint8_t>(2), static_cast<uint8_t>(BOTANY_GARDEN_PLOTS));
  indoorCapacity_ = constrain(indoorCapacity_, static_cast<uint8_t>(2), static_cast<uint8_t>(BOTANY_INDOOR_POTS));
  selectedGarden_ %= gardenCapacity_;
  selectedIndoor_ %= indoorCapacity_;
  for (uint8_t i = 0; i < BOTANY_GARDEN_PLOTS; ++i) {
    if (cropSpecies_[i] >= BOTANY_CROP_COUNT) cropSpecies_[i] = EMPTY_PLANT;
    cropWater_[i] = min(static_cast<uint8_t>(10), cropWater_[i]);
    cropHealth_[i] = min(static_cast<uint8_t>(10), cropHealth_[i]);
    cropCare_[i] = min(static_cast<uint8_t>(10), cropCare_[i]);
  }
  for (uint8_t i = 0; i < BOTANY_INDOOR_POTS; ++i) {
    if (indoorSpecies_[i] >= BOTANY_INDOOR_COUNT) indoorSpecies_[i] = EMPTY_PLANT;
    indoorWater_[i] = min(static_cast<uint8_t>(10), indoorWater_[i]);
    indoorHealth_[i] = min(static_cast<uint8_t>(10), indoorHealth_[i]);
    indoorCare_[i] = min(static_cast<uint8_t>(10), indoorCare_[i]);
  }
}

bool BotanySystem::load() {
  const BotanySave a = storeA_.read();
  const BotanySave b = storeB_.read();
  const bool validA = valid(a);
  const bool validB = valid(b);
  if (!validA && !validB) {
    reset();
    return false;
  }
  applySave(validA && validB ? (a.sequence >= b.sequence ? a : b) : (validA ? a : b));
  return true;
}

void BotanySystem::saveNow() {
  BotanySave save;
  fillSave(save);
  if (save.sequence & 1UL) storeA_.write(save);
  else storeB_.write(save);
  sequence_ = save.sequence;
}

const __FlashStringHelper *BotanySystem::cropName(uint8_t species) const {
  switch (species % BOTANY_CROP_COUNT) {
    case 0: return F("Tomato");
    case 1: return F("Strawberry");
    case 2: return F("Carrot");
    case 3: return F("Corn");
    case 4: return F("Potato");
    case 5: return F("Pumpkin");
    case 6: return F("Mushroom");
    case 7: return F("Basil");
    case 8: return F("Chili");
    case 9: return F("Melon");
    case 10: return F("Rice");
    default: return F("Starfruit");
  }
}

const __FlashStringHelper *BotanySystem::indoorName(uint8_t species) const {
  switch (species % BOTANY_INDOOR_COUNT) {
    case 0: return F("Pothos");
    case 1: return F("Aloe");
    case 2: return F("Lavender");
    case 3: return F("Bonsai");
    case 4: return F("Fern");
    case 5: return F("Orchid");
    case 6: return F("Cactus");
    case 7: return F("Peace Lily");
    case 8: return F("Mint");
    default: return F("Moonbloom");
  }
}

uint8_t BotanySystem::cropCost(uint8_t species) const {
  static const uint8_t costs[BOTANY_CROP_COUNT] = {1,2,1,2,1,3,2,2,2,3,2,5};
  return costs[species % BOTANY_CROP_COUNT];
}

uint16_t BotanySystem::cropGrowTicks(uint8_t species) const {
  static const uint16_t times[BOTANY_CROP_COUNT] = {48,60,42,72,54,96,36,42,54,84,72,120};
  return static_cast<uint16_t>(difficultyTicks(times[species % BOTANY_CROP_COUNT]));
}

uint8_t BotanySystem::indoorCost(uint8_t species) const {
  static const uint8_t costs[BOTANY_INDOOR_COUNT] = {4,5,6,8,5,10,5,9,4,14};
  return costs[species % BOTANY_INDOOR_COUNT];
}

uint16_t BotanySystem::indoorGrowTicks(uint8_t species) const {
  static const uint16_t times[BOTANY_INDOOR_COUNT] = {288,240,336,576,264,480,384,432,216,720};
  return static_cast<uint16_t>(difficultyTicks(times[species % BOTANY_INDOOR_COUNT]));
}

uint8_t BotanySystem::indoorIdealLow(uint8_t species) const {
  static const uint8_t values[BOTANY_INDOOR_COUNT] = {4,3,4,4,6,4,2,5,5,4};
  return values[species % BOTANY_INDOOR_COUNT];
}

uint8_t BotanySystem::indoorIdealHigh(uint8_t species) const {
  static const uint8_t values[BOTANY_INDOOR_COUNT] = {8,7,7,7,10,7,5,9,9,7};
  return values[species % BOTANY_INDOOR_COUNT];
}

uint8_t BotanySystem::indoorWaterInterval(uint8_t species) const {
  static const uint8_t values[BOTANY_INDOOR_COUNT] = {24,30,22,28,14,24,48,18,18,32};
  return values[species % BOTANY_INDOOR_COUNT];
}

uint8_t BotanySystem::cropStage(uint8_t plot) const {
  if (plot >= gardenCapacity_ || cropSpecies_[plot] == EMPTY_PLANT) return 0;
  const uint16_t grow = cropGrowTicks(cropSpecies_[plot]);
  if (cropAge_[plot] >= grow) return 4;
  const uint16_t safeGrow = (grow == 0) ? 1 : grow;
  return static_cast<uint8_t>(1 + (static_cast<uint32_t>(cropAge_[plot]) * 3UL) / safeGrow);
}

uint8_t BotanySystem::indoorStage(uint8_t pot) const {
  if (pot >= indoorCapacity_ || indoorSpecies_[pot] == EMPTY_PLANT) return 0;
  const uint16_t grow = indoorGrowTicks(indoorSpecies_[pot]);
  if (indoorAge_[pot] >= grow) return 4;
  const uint16_t safeGrow = (grow == 0) ? 1 : grow;
  return static_cast<uint8_t>(1 + (static_cast<uint32_t>(indoorAge_[pot]) * 3UL) / safeGrow);
}

bool BotanySystem::cropReady(uint8_t plot) const {
  return plot < gardenCapacity_ && cropSpecies_[plot] != EMPTY_PLANT && cropStage(plot) >= 4 && cropHealth_[plot] > 0;
}

bool BotanySystem::gardenReady() const {
  for (uint8_t i = 0; i < gardenCapacity_; ++i) if (cropReady(i)) return true;
  return false;
}

bool BotanySystem::needsCare() const {
  for (uint8_t i = 0; i < gardenCapacity_; ++i) {
    if (cropSpecies_[i] != EMPTY_PLANT && (cropWater_[i] <= 2 || cropHealth_[i] <= 4)) return true;
  }
  for (uint8_t i = 0; i < indoorCapacity_; ++i) {
    if (indoorSpecies_[i] != EMPTY_PLANT) {
      const uint8_t low = indoorIdealLow(indoorSpecies_[i]);
      const uint8_t high = indoorIdealHigh(indoorSpecies_[i]);
      if (indoorWater_[i] + 1 < low || indoorWater_[i] > high + 2 || indoorHealth_[i] <= 4) return true;
    }
  }
  return false;
}

bool BotanySystem::hasIndoorPlants() const { return indoorCount() > 0; }

bool BotanySystem::indoorNeedsTending() const {
  for (uint8_t i = 0; i < indoorCapacity_; ++i)
    if (indoorSpecies_[i] != EMPTY_PLANT && (indoorCare_[i] < 6 || indoorHealth_[i] < 10)) return true;
  return false;
}

uint8_t BotanySystem::gardenCount() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < gardenCapacity_; ++i) if (cropSpecies_[i] != EMPTY_PLANT) ++count;
  return count;
}

uint8_t BotanySystem::indoorCount() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < indoorCapacity_; ++i) if (indoorSpecies_[i] != EMPTY_PLANT) ++count;
  return count;
}

uint8_t BotanySystem::gardenMatureCount() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < gardenCapacity_; ++i) if (cropReady(i)) ++count;
  return count;
}

uint8_t BotanySystem::gardenAverageStage() const {
  uint16_t sum = 0; uint8_t count = 0;
  for (uint8_t i = 0; i < gardenCapacity_; ++i) if (cropSpecies_[i] != EMPTY_PLANT) { sum += cropStage(i); ++count; }
  return count ? static_cast<uint8_t>(sum / count) : 0;
}

uint8_t BotanySystem::gardenAverageHealth() const {
  uint16_t sum = 0; uint8_t count = 0;
  for (uint8_t i = 0; i < gardenCapacity_; ++i) if (cropSpecies_[i] != EMPTY_PLANT) { sum += cropHealth_[i]; ++count; }
  return count ? static_cast<uint8_t>(sum / count) : 0;
}

uint8_t BotanySystem::gardenAverageWater() const {
  uint16_t sum = 0; uint8_t count = 0;
  for (uint8_t i = 0; i < gardenCapacity_; ++i) if (cropSpecies_[i] != EMPTY_PLANT) { sum += cropWater_[i]; ++count; }
  return count ? static_cast<uint8_t>(sum / count) : 0;
}

uint8_t BotanySystem::indoorAverageStage() const {
  uint16_t sum = 0; uint8_t count = 0;
  for (uint8_t i = 0; i < indoorCapacity_; ++i) if (indoorSpecies_[i] != EMPTY_PLANT) { sum += indoorStage(i); ++count; }
  return count ? static_cast<uint8_t>(sum / count) : 0;
}

uint8_t BotanySystem::indoorAverageHealth() const {
  uint16_t sum = 0; uint8_t count = 0;
  for (uint8_t i = 0; i < indoorCapacity_; ++i) if (indoorSpecies_[i] != EMPTY_PLANT) { sum += indoorHealth_[i]; ++count; }
  return count ? static_cast<uint8_t>(sum / count) : 0;
}

void BotanySystem::eventWaterGarden(uint8_t amount) {
  for (uint8_t i = 0; i < gardenCapacity_; ++i) if (cropSpecies_[i] != EMPTY_PLANT) cropWater_[i] = addBounded(cropWater_[i], amount);
  markSaveDirty();
}

void BotanySystem::eventTendGarden(uint8_t amount) {
  for (uint8_t i = 0; i < gardenCapacity_; ++i) if (cropSpecies_[i] != EMPTY_PLANT) { cropCare_[i] = addBounded(cropCare_[i], amount); cropHealth_[i] = addBounded(cropHealth_[i], max<uint8_t>(1, amount / 2)); }
  markSaveDirty();
}

void BotanySystem::eventDamageGarden(uint8_t amount) {
  for (uint8_t i = 0; i < gardenCapacity_; ++i) if (cropSpecies_[i] != EMPTY_PLANT) { cropHealth_[i] = cropHealth_[i] > amount ? cropHealth_[i] - amount : 0; cropWater_[i] = cropWater_[i] > amount ? cropWater_[i] - amount : 0; }
  markSaveDirty();
}

void BotanySystem::eventWaterIndoor(uint8_t amount) {
  for (uint8_t i = 0; i < indoorCapacity_; ++i) if (indoorSpecies_[i] != EMPTY_PLANT) indoorWater_[i] = addBounded(indoorWater_[i], amount);
  markSaveDirty();
}

void BotanySystem::eventTendIndoor(uint8_t amount) {
  for (uint8_t i = 0; i < indoorCapacity_; ++i) if (indoorSpecies_[i] != EMPTY_PLANT) { indoorCare_[i] = addBounded(indoorCare_[i], amount); indoorHealth_[i] = addBounded(indoorHealth_[i], max<uint8_t>(1, amount / 2)); }
  markSaveDirty();
}

void BotanySystem::eventDamageIndoor(uint8_t amount) {
  for (uint8_t i = 0; i < indoorCapacity_; ++i) if (indoorSpecies_[i] != EMPTY_PLANT) { indoorHealth_[i] = indoorHealth_[i] > amount ? indoorHealth_[i] - amount : 0; }
  markSaveDirty();
}

const __FlashStringHelper *BotanySystem::cropDisplayName(uint8_t species) const {
  return cropName(species);
}

const __FlashStringHelper *BotanySystem::indoorDisplayName(uint8_t species) const {
  return indoorName(species);
}

bool BotanySystem::cropDiscovered(uint8_t species) const {
  if (species >= BOTANY_CROP_COUNT) return false;
  return (discoveredCrops_ & static_cast<uint16_t>(1U << species)) != 0;
}

bool BotanySystem::indoorDiscovered(uint8_t species) const {
  if (species >= BOTANY_INDOOR_COUNT) return false;
  return (discoveredIndoor_ & static_cast<uint16_t>(1U << species)) != 0;
}

uint8_t BotanySystem::discoveredCropCount() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < BOTANY_CROP_COUNT; ++i) if (cropDiscovered(i)) ++count;
  return count;
}

uint8_t BotanySystem::discoveredIndoorCount() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < BOTANY_INDOOR_COUNT; ++i) if (indoorDiscovered(i)) ++count;
  return count;
}

void BotanySystem::drawCropCatalogSprite(uint8_t species, int x, int groundY,
                                         uint8_t stage) const {
  drawCropSprite(species, x, groundY, min(static_cast<uint8_t>(4), stage), false);
}

void BotanySystem::drawIndoorCatalogSprite(uint8_t species, int x, int baseY,
                                           uint8_t stage) const {
  drawIndoorSprite(species, x, baseY, min(static_cast<uint8_t>(4), stage), false);
}

void BotanySystem::drawCropSprite(uint8_t species, int x, int baseY,
                                  uint8_t stage, bool unhealthy,
                                  uint8_t frame) const {
  NativeUI::crop(display_, x - 12, baseY - 22, species, stage, unhealthy,
                 frame);
}

void BotanySystem::drawIndoorSprite(uint8_t species, int x, int baseY,
                                    uint8_t stage, bool unhealthy,
                                    uint8_t frame) const {
  NativeUI::plant(display_, x - 12, baseY - 23, species, stage, unhealthy,
                  frame);
}

void BotanySystem::drawGardenOverview(uint8_t frame) const {
  NativeUI::Scope native(display_);
  display_.clearDisplay();
  char counter[12];
  snprintf(counter, sizeof(counter), "%u/%u", selectedGarden_ + 1,
           gardenCapacity_);
  NativeUI::header(display_, "GARDEN", counter);

  display_.drawCircle(8, 27, 3, SSD1306_WHITE);
  display_.drawFastHLine(8, 22, 1, SSD1306_WHITE);
  NativeUI::text(display_, "FOOD TO PANTRY", 16, 22, 108);

  for (uint8_t local = 0; local < BOTANY_GARDEN_PLOTS; ++local) {
    const uint8_t i = local;
    const int x = 3 + (local % 2) * 62;
    const int y = 34 + (local / 2) * 27;
    if (i >= gardenCapacity_) {
      display_.drawRoundRect(x, y, 60, 25, 3, SSD1306_WHITE);
      display_.drawRoundRect(x + 6, y + 9, 10, 8, 2, SSD1306_WHITE);
      display_.drawRoundRect(x + 8, y + 5, 6, 8, 3, SSD1306_WHITE);
      char plot[5];
      snprintf(plot, sizeof(plot), "P%u", local + 1);
      NativeUI::text(display_, plot, x + 27, y + 4, 31);
      NativeUI::text(display_, "LOCK", x + 27, y + 14, 31);
      continue;
    }
    const bool active = i == selectedGarden_;
    const uint8_t sp = cropSpecies_[i];
    const uint8_t stage = sp == EMPTY_PLANT ? 0 : cropStage(i);
    display_.drawRoundRect(x, y, 60, 25, 3, SSD1306_WHITE);
    if (active) {
      display_.drawFastHLine(x + 3, y + 23, 54, SSD1306_WHITE);
      display_.drawPixel(x + 4, y + 3, SSD1306_WHITE);
    }
    if (sp == EMPTY_PLANT) {
      NativeUI::crop(display_, x + 2, y, static_cast<uint8_t>(local * 3), 0,
                     false, frame + local * 2);
      char plot[5];
      snprintf(plot, sizeof(plot), "P%u", local + 1);
      NativeUI::text(display_, plot, x + 28, y + 4, 30);
      NativeUI::text(display_, "EMPTY", x + 28, y + 14, 30);
    } else {
      drawCropSprite(sp, x + 14, y + 22, stage,
                     cropHealth_[i] <= 4 || cropWater_[i] <= 1,
                     frame + local * 3);
      char plot[5];
      snprintf(plot, sizeof(plot), "P%u", local + 1);
      NativeUI::text(display_, plot, x + 27, y + 3, 31);
      NativeUI::text(display_, cropReady(i) ? "READY" :
                     (cropWater_[i] <= 2 || cropHealth_[i] <= 4 ? "CARE" : "GROW"),
                     x + 27, y + 13, 31);
      NativeUI::meter(display_, x + 28, y + 20, 27, stage, 4);
    }
  }

  if (cropSpecies_[selectedGarden_] == EMPTY_PLANT) {
    NativeUI::text(display_, "EMPTY PLOT", 5, 91, 64);
  } else {
    NativeUI::text(display_, cropName(cropSpecies_[selectedGarden_]), 5, 91, 66);
  }
  NativeUI::text(display_, "KITCHEN", 75, 91, 49);
  NativeUI::text(display_, "W", 5, 104, 7);
  NativeUI::meter(display_, 14, 104, 43, cropWater_[selectedGarden_]);
  NativeUI::text(display_, "H", 68, 104, 7);
  NativeUI::meter(display_, 77, 104, 43, cropHealth_[selectedGarden_]);
  NativeUI::footer(display_, "PREV", "OPEN", "NEXT");
}

void BotanySystem::drawPlantRoomOverview(uint8_t frame) const {
  NativeUI::Scope native(display_);
  display_.clearDisplay();
  const uint32_t day = ageTicks / 288UL;
  const uint8_t order = static_cast<uint8_t>((day * 7UL + 3UL) % 10U);
  const uint8_t rank = static_cast<uint8_t>((discoveredIndoor_ & (7U << 11)) >> 11);
  const bool orderDone = (discoveredIndoor_ & (1U << 10)) != 0;
  char counter[12];
  snprintf(counter, sizeof(counter), "%u/%u R%u", selectedIndoor_ + 1,
           indoorCapacity_, rank);
  NativeUI::header(display_, "PLANT ROOM", counter);

  NativeUI::text(display_, "FLORIST COLLECTION", 5, 21, 118);
  display_.drawRoundRect(3, 30, 122, 15, 3, SSD1306_WHITE);
  NativeUI::text(display_, "ORDER", 7, 34, 30);
  NativeUI::text(display_, indoorName(order), 42, 34, 60);
  NativeUI::text(display_, orderDone ? "OK" : "DUE", 105, 34, 18);

  for (uint8_t local = 0; local < BOTANY_INDOOR_POTS; ++local) {
    const uint8_t i = local;
    const int x = 3 + (local % 2) * 62;
    const int y = 48 + (local / 2) * 26;
    if (i >= indoorCapacity_) {
      display_.drawRoundRect(x, y, 60, 25, 3, SSD1306_WHITE);
      display_.drawRoundRect(x + 6, y + 9, 10, 8, 2, SSD1306_WHITE);
      display_.drawRoundRect(x + 8, y + 5, 6, 8, 3, SSD1306_WHITE);
      char pot[5];
      snprintf(pot, sizeof(pot), "P%u", local + 1);
      NativeUI::text(display_, pot, x + 27, y + 4, 31);
      NativeUI::text(display_, "LOCK", x + 27, y + 14, 31);
      continue;
    }
    const bool active = i == selectedIndoor_;
    const uint8_t sp = indoorSpecies_[i];
    const uint8_t stage = sp == EMPTY_PLANT ? 0 : indoorStage(i);
    display_.drawRoundRect(x, y, 60, 25, 3, SSD1306_WHITE);
    if (active) {
      display_.drawFastHLine(x + 3, y + 23, 54, SSD1306_WHITE);
      display_.drawPixel(x + 4, y + 3, SSD1306_WHITE);
    }
    drawIndoorSprite(sp == EMPTY_PLANT ? static_cast<uint8_t>(local % BOTANY_INDOOR_COUNT) : sp,
                     x + 14, y + 22, stage,
                     sp != EMPTY_PLANT && indoorHealth_[i] <= 4,
                     frame + local * 3);
    if (sp == EMPTY_PLANT) {
      char pot[5];
      snprintf(pot, sizeof(pot), "P%u", local + 1);
      NativeUI::text(display_, pot, x + 27, y + 4, 31);
      NativeUI::text(display_, "EMPTY", x + 27, y + 14, 31);
    } else {
      char pot[5];
      snprintf(pot, sizeof(pot), "P%u", local + 1);
      NativeUI::text(display_, pot, x + 27, y + 3, 31);
      const bool wanted = !orderDone && sp == order;
      NativeUI::text(display_, wanted ? "ORDER" :
                     (indoorWater_[i] <= 2 || indoorHealth_[i] <= 4 ? "CARE" :
                      (stage >= 4 ? "READY" : "GROW")),
                     x + 27, y + 13, 31);
      NativeUI::meter(display_, x + 28, y + 20, 27, stage, 4);
    }
  }

  if (indoorSpecies_[selectedIndoor_] == EMPTY_PLANT) {
    NativeUI::text(display_, "EMPTY POT", 5, 101, 62);
    NativeUI::text(display_, "-", 78, 101, 42);
  } else {
    const uint8_t species = indoorSpecies_[selectedIndoor_];
    const uint8_t stage = indoorStage(selectedIndoor_);
    uint16_t value = static_cast<uint16_t>(
        (indoorCost(species) * (stage + 1) * indoorHealth_[selectedIndoor_]) / 18);
    if (value < 1) value = 1;
    if (value > 255) value = 255;
    char saleValue[8];
    snprintf(saleValue, sizeof(saleValue), "$%u", value);
    NativeUI::text(display_, indoorName(species), 5, 101, 68);
    NativeUI::text(display_, saleValue, 78, 101, 42);
  }
  NativeUI::text(display_, "W", 5, 107, 7);
  NativeUI::meter(display_, 14, 109, 42, indoorWater_[selectedIndoor_]);
  NativeUI::text(display_, "H", 68, 107, 7);
  NativeUI::meter(display_, 77, 109, 43, indoorHealth_[selectedIndoor_]);
  NativeUI::footer(display_, "PREV", "OPEN", "NEXT");
}

void BotanySystem::drawGardenPreview(int x, int y, uint8_t frame) const {
  // v36: a wider four-plot preview replaces the cramped three-sprite cluster.
  // The real crop artwork is used at a readable spacing, and empty soil mounds
  // make the garden visible before every plot has been planted.
  const uint8_t shown = min(static_cast<uint8_t>(4), gardenCapacity_);
  display_.drawLine(x, y + 20, x + 70, y + 20, SSD1306_WHITE);
  for (uint8_t i = 0; i < shown; ++i) {
    const int px = x + 8 + i * 18;
    display_.drawLine(px - 7, y + 21, px + 7, y + 21, SSD1306_WHITE);
    if (cropSpecies_[i] != EMPTY_PLANT) {
      drawCropSprite(cropSpecies_[i], px, y + 20,
                     min(static_cast<uint8_t>(4), cropStage(i)),
                     cropHealth_[i] <= 4 || cropWater_[i] <= 1,
                     frame + i * 2);
    } else {
      display_.drawLine(px - 5, y + 19, px, y + 17, SSD1306_WHITE);
      display_.drawLine(px, y + 17, px + 5, y + 19, SSD1306_WHITE);
      display_.drawPixel(px, y + 15, SSD1306_WHITE);
    }
  }
  if (gardenReady() && ((frame / 4) & 1) == 0) {
    display_.drawPixel(x + 68, y + 2, SSD1306_WHITE);
    display_.drawLine(x + 66, y + 4, x + 70, y + 4, SSD1306_WHITE);
  }
}

void BotanySystem::drawPlantRoomPreview(int x, int y, uint8_t frame) const {
  display_.drawLine(x, y + 20, x + 37, y + 20, SSD1306_WHITE);
  for (uint8_t i = 0; i < min(static_cast<uint8_t>(3), indoorCapacity_); ++i) {
    const int px = x + 7 + i * 12;
    if (indoorSpecies_[i] != EMPTY_PLANT) drawIndoorSprite(indoorSpecies_[i], px, y + 19, min(static_cast<uint8_t>(3), indoorStage(i)), indoorHealth_[i] <= 4, frame + i * 3);
    else display_.drawRoundRect(px - 3, y + 16, 6, 4, 1, SSD1306_WHITE);
  }
  if (needsCare() && ((frame / 4) & 1) == 0) display_.drawPixel(x + 34, y + 2, SSD1306_WHITE);
}

void BotanySystem::popup(const __FlashStringHelper *line1,
                         const __FlashStringHelper *line2,
                         uint16_t holdMs) {
  display_.clearDisplay();
  display_.setTextWrap(false);
  display_.setTextSize(1);
  display_.setTextColor(SSD1306_WHITE);
  display_.drawRoundRect(3, 13, 122, 39, 5, SSD1306_WHITE);
  display_.setCursor(10, 23); display_.print(line1);
  display_.setCursor(10, 36); display_.print(line2);
  display_.display();
  delay(holdMs);
}

void BotanySystem::chooseCrop() {
  if (cropSpecies_[selectedGarden_] != EMPTY_PLANT) { popup(F("Plot is occupied"), F("Gather or clear it")); return; }
  uint8_t pick = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? BOTANY_CROP_COUNT - 1 : pick - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { pick = static_cast<uint8_t>((pick + 1) % BOTANY_CROP_COUNT); clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) {
      if (!spendCoins(cropCost(pick))) { badSound(); popup(F("Not enough coins"), F("Work or sell plants")); return; }
      cropSpecies_[selectedGarden_] = pick;
      cropAge_[selectedGarden_] = 0;
      cropWater_[selectedGarden_] = 6;
      cropHealth_[selectedGarden_] = 10;
      cropCare_[selectedGarden_] = 0;
      discoveredCrops_ |= static_cast<uint16_t>(1U << pick);
      addSkill(skillKind, 1); changeBond(1); careSound(); markSaveDirty();
      completeGameplayAction(M_GARDEN);
      popup(F("Food seed planted"), cropName(pick));
      return;
    }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); display_.setTextSize(1); display_.setTextColor(SSD1306_WHITE);
    display_.setCursor(24, 0); display_.print(F("CHOOSE FOOD CROP"));
    display_.drawRoundRect(5, 15, 118, 38, 5, SSD1306_WHITE);
    drawCropSprite(pick, 26, 47, 4, false);
    display_.setCursor(49, 23); display_.print(cropName(pick));
    display_.setCursor(49, 37); display_.print(F("Cost $")); display_.print(cropCost(pick));
    display_.display(); delay(20);
  }
}

void BotanySystem::chooseIndoorPlant() {
  if (indoorSpecies_[selectedIndoor_] != EMPTY_PLANT) { popup(F("Pot is occupied"), F("Sell or clear it")); return; }
  uint8_t pick = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? BOTANY_INDOOR_COUNT - 1 : pick - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { pick = static_cast<uint8_t>((pick + 1) % BOTANY_INDOOR_COUNT); clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) {
  if (!spendCoins(indoorCost(pick))) { badSound(); popup(F("Not enough coins"), F("Work or sell plants")); return; }
      indoorSpecies_[selectedIndoor_] = pick;
      indoorAge_[selectedIndoor_] = 0;
      indoorWater_[selectedIndoor_] = (indoorIdealLow(pick) + indoorIdealHigh(pick)) / 2;
      indoorHealth_[selectedIndoor_] = 10;
      indoorCare_[selectedIndoor_] = 0;
      discoveredIndoor_ |= static_cast<uint16_t>(1U << pick);
      addSkill(skillKind, 1); changeBond(1); careSound(); markSaveDirty();
      completeGameplayAction(M_PLANTROOM);
      popup(F("Plant adopted"), indoorName(pick));
      return;
    }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); display_.setTextSize(1); display_.setTextColor(SSD1306_WHITE);
    display_.setCursor(20, 0); display_.print(F("CHOOSE DECOR PLANT"));
    display_.drawRoundRect(5, 15, 118, 38, 5, SSD1306_WHITE);
    drawIndoorSprite(pick, 27, 48, 4, false);
    display_.setCursor(49, 23); display_.print(indoorName(pick));
    display_.setCursor(49, 37); display_.print(F("Cost $")); display_.print(indoorCost(pick));
    display_.display(); delay(20);
  }
}

void BotanySystem::waterCrop() {
  if (cropSpecies_[selectedGarden_] == EMPTY_PLANT) { popup(F("Empty plot"), F("Plant a seed first")); return; }
  if (cropWater_[selectedGarden_] >= 6) { popup(F("Soil is still moist"), F("No watering needed")); return; }
  cropWater_[selectedGarden_] = addBounded(cropWater_[selectedGarden_], 4);
  cropHealth_[selectedGarden_] = addBounded(cropHealth_[selectedGarden_], 1);
  addSkill(skillKind, 1); markRoutineAction(ROUTINE_CARE_BT); careSound(); markSaveDirty();
  completeGameplayAction(M_GARDEN);
  popup(F("Crop watered"), cropName(cropSpecies_[selectedGarden_]));
}

void BotanySystem::tendCrop() {
  if (cropSpecies_[selectedGarden_] == EMPTY_PLANT) { popup(F("Empty plot"), F("Nothing to tend")); return; }
  if (cropCare_[selectedGarden_] >= 6) { popup(F("Crop is already tended"), F("Let it grow")); return; }
  if (energy < 1) { popup(F("Too tired"), F("Rest first")); return; }
  cropCare_[selectedGarden_] = addBounded(cropCare_[selectedGarden_], 2);
  cropHealth_[selectedGarden_] = addBounded(cropHealth_[selectedGarden_], 2);
  subStat(energy, 1); addSkill(skillKind, 1); changeBond(1); markRoutineAction(ROUTINE_CARE_BT); goodSound(); markSaveDirty();
  completeGameplayAction(M_GARDEN);
  popup(F("Crop tended"), F("Pests and weeds cleared"));
}

void BotanySystem::applyCropHarvest(uint8_t species, uint8_t quality) {
  const uint8_t bonus = quality >= 8 ? (naturalActivity() == M_GARDEN ? 2 : 1) : 0;
  switch (species % BOTANY_CROP_COUNT) {
    case 0: vegInv = addBounded(vegInv, static_cast<uint8_t>(2 + bonus), 99); break;
    case 1: berryInv = addBounded(berryInv, static_cast<uint8_t>(2 + bonus), 99); break;
    case 2: vegInv = addBounded(vegInv, static_cast<uint8_t>(2 + bonus), 99); break;
    case 3: vegInv = addBounded(vegInv, static_cast<uint8_t>(3 + bonus), 99); break;
    case 4: vegInv = addBounded(vegInv, static_cast<uint8_t>(2 + bonus), 99); break;
    case 5: vegInv = addBounded(vegInv, static_cast<uint8_t>(4 + bonus), 99); break;
    case 6: spiceInv = addBounded(spiceInv, static_cast<uint8_t>(2 + bonus), 99); break;
    case 7: herbInv = addBounded(herbInv, static_cast<uint8_t>(2 + bonus), 99); break;
    case 8: spiceInv = addBounded(spiceInv, static_cast<uint8_t>(2 + bonus), 99); break;
    case 9: berryInv = addBounded(berryInv, static_cast<uint8_t>(3 + bonus), 99); break;
    case 10: vegInv = addBounded(vegInv, static_cast<uint8_t>(2 + bonus), 99); break;
    default:
      berryInv = addBounded(berryInv, 1, 99);
      herbInv = addBounded(herbInv, 1, 99);
      spiceInv = addBounded(spiceInv, 1, 99);
      break;
  }
}

void BotanySystem::harvestCrop() {
  if (!cropReady(selectedGarden_)) { popup(F("Not ready yet"), F("Keep caring for it")); return; }
  const uint8_t species = cropSpecies_[selectedGarden_];
  const uint8_t quality = static_cast<uint8_t>((cropHealth_[selectedGarden_] + cropWater_[selectedGarden_] + cropCare_[selectedGarden_]) / 3);
  applyCropHarvest(species, quality);
  cropSpecies_[selectedGarden_] = EMPTY_PLANT;
  cropAge_[selectedGarden_] = 0;
  cropWater_[selectedGarden_] = 6;
  cropHealth_[selectedGarden_] = 10;
  cropCare_[selectedGarden_] = 0;
  gardenLevel = addBounded(gardenLevel, 1, 100);
  gardenHarvests = addBounded(gardenHarvests, 1, 250);
  addStat(happy, 2); addSkill(skillKind, 2); changeBond(1); recordGoalEvent(2, 1); markRoutineAction(ROUTINE_PLAY_BT);
  winSound(); markSaveDirty();
  completeGameplayAction(M_GARDEN);
  popup(F("Added to the pantry"), cropName(species));
}

void BotanySystem::clearCrop() {
  if (cropSpecies_[selectedGarden_] == EMPTY_PLANT) return;
  cropSpecies_[selectedGarden_] = EMPTY_PLANT;
  cropAge_[selectedGarden_] = 0;
  cropWater_[selectedGarden_] = 6;
  cropHealth_[selectedGarden_] = 10;
  cropCare_[selectedGarden_] = 0;
  badSound(); markSaveDirty();
  popup(F("Plot cleared"), F("The crop was removed"));
}

void BotanySystem::waterIndoor() {
  if (indoorSpecies_[selectedIndoor_] == EMPTY_PLANT) { popup(F("Empty pot"), F("Adopt a plant first")); return; }
  if (indoorWater_[selectedIndoor_] >= indoorIdealHigh(indoorSpecies_[selectedIndoor_])) { popup(F("Soil has enough water"), F("Too much harms roots")); return; }
  indoorWater_[selectedIndoor_] = addBounded(indoorWater_[selectedIndoor_], 3);
  const uint8_t species = indoorSpecies_[selectedIndoor_];
  if (indoorWater_[selectedIndoor_] <= indoorIdealHigh(species) + 1) indoorHealth_[selectedIndoor_] = addBounded(indoorHealth_[selectedIndoor_], 1);
  addSkill(skillKind, 1); markRoutineAction(ROUTINE_CARE_BT); careSound(); markSaveDirty();
  completeGameplayAction(M_PLANTROOM);
  popup(F("Plant watered"), indoorName(species));
}

void BotanySystem::tendIndoor() {
  if (indoorSpecies_[selectedIndoor_] == EMPTY_PLANT) { popup(F("Empty pot"), F("Nothing to tend")); return; }
  if (indoorCare_[selectedIndoor_] >= 6) { popup(F("Leaves already tended"), F("Let the plant settle")); return; }
  indoorCare_[selectedIndoor_] = addBounded(indoorCare_[selectedIndoor_], 2);
  indoorHealth_[selectedIndoor_] = addBounded(indoorHealth_[selectedIndoor_], 2);
  addStat(happy, 1); addSkill(skillKind, 1); changeBond(1); goodSound(); markSaveDirty();
  completeGameplayAction(M_PLANTROOM);
  popup(F("Plant tended"), F("Leaves cleaned and turned"));
}

void BotanySystem::sellIndoor() {
  if (indoorSpecies_[selectedIndoor_] == EMPTY_PLANT) { popup(F("Empty pot"), F("Nothing to sell")); return; }
  const uint8_t species = indoorSpecies_[selectedIndoor_];
  const uint8_t stage = indoorStage(selectedIndoor_);
  const uint32_t day = ageTicks / TICKS_PER_DAY_BT;
  const uint8_t order = static_cast<uint8_t>((day * 7UL + 3UL) % BOTANY_INDOOR_COUNT);
  const uint8_t rank = static_cast<uint8_t>((discoveredIndoor_ & FLORIST_RANK_MASK) >> 11);
  const bool fillsOrder = !(discoveredIndoor_ & FLORIST_ORDER_DONE) &&
                          species == order && stage >= 4 &&
                          indoorHealth_[selectedIndoor_] >= 7;
  uint16_t rawPrice = static_cast<uint16_t>((indoorCost(species) * (stage + 1) * indoorHealth_[selectedIndoor_]) / 18);
  if (stage >= 4) rawPrice += lifeWorld.projectLevel(3) + lifeWorld.reputation(3) / 25 + (naturalActivity() == M_PLANTROOM ? 2 : 0);
  if (rawPrice < 1) rawPrice = 1;
  if (rawPrice > 255) rawPrice = 255;
  const uint8_t price = static_cast<uint8_t>(rawPrice);
  completeGameplayAction(M_PLANTROOM);
  addCoins(price);
  uint8_t bonus = 0;
  if (fillsOrder) {
    bonus = static_cast<uint8_t>(6 + rank * 2);
    addCoins(bonus);
    discoveredIndoor_ |= FLORIST_ORDER_DONE;
    const uint8_t nextRank = rank < 7 ? rank + 1 : 7;
    discoveredIndoor_ = static_cast<uint16_t>(
        (discoveredIndoor_ & ~FLORIST_RANK_MASK) | (nextRank << 11));
    addStat(happy, 2);
    addSkill(skillStyle, 2);
    changeBond(2);
    changeTrust(1);
    markRoutineAction(ROUTINE_PLAY_BT);
    lifeWorld.recordFloristOrder(species, day);
  } else {
    lifeWorld.recordPlantSale(species);
  }
  indoorSpecies_[selectedIndoor_] = EMPTY_PLANT;
  indoorAge_[selectedIndoor_] = 0;
  indoorWater_[selectedIndoor_] = 6;
  indoorHealth_[selectedIndoor_] = 10;
  indoorCare_[selectedIndoor_] = 0;
  goodSound(); markSaveDirty();
  display_.clearDisplay(); display_.setTextSize(1); display_.setTextColor(SSD1306_WHITE);
  display_.drawRoundRect(3, 13, 122, 45, 5, SSD1306_WHITE);
  display_.setCursor(10, 22);
  display_.print(fillsOrder ? F("Florist order filled") : F("Plant sold"));
  display_.setCursor(10, 35); display_.print(F("Sale $")); display_.print(price);
  if (fillsOrder) {
    display_.setCursor(10, 48); display_.print(F("Order bonus +$")); display_.print(bonus);
  } else {
    display_.setCursor(10, 48); display_.print(F("Grow decor to sell"));
  }
  display_.display(); delay(850);
}

void BotanySystem::clearIndoor() {
  if (indoorSpecies_[selectedIndoor_] == EMPTY_PLANT) return;
  indoorSpecies_[selectedIndoor_] = EMPTY_PLANT;
  indoorAge_[selectedIndoor_] = 0;
  indoorWater_[selectedIndoor_] = 6;
  indoorHealth_[selectedIndoor_] = 10;
  indoorCare_[selectedIndoor_] = 0;
  badSound(); markSaveDirty(); popup(F("Pot cleared"), F("The plant was removed"));
}

void BotanySystem::upgradeGarden() {
  if (gardenCapacity_ >= BOTANY_GARDEN_PLOTS) { popup(F("Garden is full size"), F("Four plots available")); return; }
  const uint8_t price = gardenCapacity_ == 2 ? 12 : 24;
  if (!spendCoins(price)) { badSound(); popup(F("Not enough coins"), F("Work or sell plants")); return; }
  ++gardenCapacity_; goodSound(); markSaveDirty(); popup(F("Garden expanded"), F("A new plot opened"));
}

void BotanySystem::upgradePlantRoom() {
  if (indoorCapacity_ >= BOTANY_INDOOR_POTS) { popup(F("Plant room is full"), F("Four pots available")); return; }
  const uint8_t price = indoorCapacity_ == 2 ? 15 : 28;
  if (!spendCoins(price)) { badSound(); popup(F("Not enough coins"), F("Work or sell plants")); return; }
  ++indoorCapacity_; goodSound(); markSaveDirty(); popup(F("Shelf expanded"), F("A new pot opened"));
}

void BotanySystem::gardenActionMenu() {
  const uint8_t count = 6;
  uint8_t pick = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? count - 1 : pick - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { pick = static_cast<uint8_t>((pick + 1) % count); clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) {
      waitButtonRelease();
      if (pick == 0) chooseCrop();
      else if (pick == 1) waterCrop();
      else if (pick == 2) tendCrop();
      else if (pick == 3) harvestCrop();
      else if (pick == 4) clearCrop();
      else upgradeGarden();
      return;
    }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); display_.setTextSize(1); display_.setTextColor(SSD1306_WHITE);
    display_.setCursor(32, 0); display_.print(F("GARDEN ACTION"));
    display_.drawRoundRect(7, 17, 114, 36, 5, SSD1306_WHITE);
    display_.setCursor(18, 31);
    if (pick == 0) display_.print(F("Plant food crop"));
    else if (pick == 1) display_.print(F("Water food crop"));
    else if (pick == 2) display_.print(F("Tend food crop"));
    else if (pick == 3) display_.print(F("Gather to pantry"));
    else if (pick == 4) display_.print(F("Clear food plot"));
    else display_.print(F("Expand garden"));
    display_.display(); delay(20);
  }
}

void BotanySystem::indoorActionMenu() {
  const uint8_t count = 6;
  uint8_t pick = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? count - 1 : pick - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { pick = static_cast<uint8_t>((pick + 1) % count); clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) {
      waitButtonRelease();
      if (pick == 0) chooseIndoorPlant();
      else if (pick == 1) waterIndoor();
      else if (pick == 2) tendIndoor();
      else if (pick == 3) sellIndoor();
      else if (pick == 4) clearIndoor();
      else upgradePlantRoom();
      return;
    }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); display_.setTextSize(1); display_.setTextColor(SSD1306_WHITE);
    display_.setCursor(24, 0); display_.print(F("PLANT ROOM ACTION"));
    display_.drawRoundRect(7, 17, 114, 36, 5, SSD1306_WHITE);
    display_.setCursor(18, 31);
    if (pick == 0) display_.print(F("Adopt decor plant"));
    else if (pick == 1) display_.print(F("Water selected"));
    else if (pick == 2) display_.print(F("Clean and tend"));
    else if (pick == 3) display_.print(F("Sell to florist"));
    else if (pick == 4) display_.print(F("Remove plant"));
    else display_.print(F("Expand shelves"));
    display_.display(); delay(20);
  }
}

void BotanySystem::runGarden() {
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  uint8_t frame = 0;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { selectedGarden_ = selectedGarden_ == 0 ? gardenCapacity_ - 1 : selectedGarden_ - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { selectedGarden_ = static_cast<uint8_t>((selectedGarden_ + 1) % gardenCapacity_); clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) { gardenActionMenu(); oldL = oldS = oldR = HIGH; waitButtonRelease(); }
    oldL = l; oldS = s; oldR = r;
    drawGardenOverview(frame++);
    display_.display();
    delay(90);
  }
}

void BotanySystem::runPlantRoom() {
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  uint8_t frame = 0;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { selectedIndoor_ = selectedIndoor_ == 0 ? indoorCapacity_ - 1 : selectedIndoor_ - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { selectedIndoor_ = static_cast<uint8_t>((selectedIndoor_ + 1) % indoorCapacity_); clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) { indoorActionMenu(); oldL = oldS = oldR = HIGH; waitButtonRelease(); }
    oldL = l; oldS = s; oldR = r;
    drawPlantRoomOverview(frame++);
    display_.display();
    delay(90);
  }
}

void BotanySystem::applyIndoorDailyBoost(uint8_t species) {
  switch (species % BOTANY_INDOOR_COUNT) {
    case 0: addStat(energy, 1); break;                 // Pothos
    case 1: addStat(health, 1); break;                 // Aloe
    case 2: addStat(happy, 1); break;                  // Lavender
    case 3: addStat(discipline, 1); addSkill(skillSmart, 1); break; // Bonsai
    case 4: addStat(clean, 1); break;                  // Fern
    case 5: addStat(happy, 1); addSkill(skillStyle, 1); break;      // Orchid
    case 6: changeTrust(1); addStat(health, 1); break; // Cactus
    case 7: addStat(clean, 1); addStat(health, 1); break;           // Peace lily
    case 8: addStat(energy, 1); addStat(happy, 1); break;           // Mint
    default: changeBond(1); addSkill(skillSmart, 1); break;         // Moonbloom
  }
}

void BotanySystem::tick(bool catchUpMode) {
  (void)catchUpMode;
  for (uint8_t i = 0; i < gardenCapacity_; ++i) {
    if (cropSpecies_[i] == EMPTY_PLANT) continue;
    if (cropAge_[i] < 65535) ++cropAge_[i];
    if (cropAge_[i] % difficultyTicks(12UL) == 0) cropWater_[i] = subBounded(cropWater_[i], 1);
    if (cropAge_[i] % difficultyTicks(24UL) == 0) {
      if (cropWater_[i] == 0) cropHealth_[i] = subBounded(cropHealth_[i], 2);
      else if (cropWater_[i] <= 2 || cropCare_[i] == 0) cropHealth_[i] = subBounded(cropHealth_[i], 1);
      else cropHealth_[i] = addBounded(cropHealth_[i], 1);
      cropCare_[i] = subBounded(cropCare_[i], 1);
    }
  }

  for (uint8_t i = 0; i < indoorCapacity_; ++i) {
    if (indoorSpecies_[i] == EMPTY_PLANT) continue;
    if (indoorAge_[i] < 65535) ++indoorAge_[i];
    const uint8_t species = indoorSpecies_[i];
    const uint8_t interval = indoorWaterInterval(species);
    if (indoorAge_[i] % difficultyTicks(interval) == 0) indoorWater_[i] = subBounded(indoorWater_[i], 1);
    if (indoorAge_[i] % difficultyTicks(24UL) == 0) {
      const uint8_t low = indoorIdealLow(species);
      const uint8_t high = indoorIdealHigh(species);
      if (indoorWater_[i] + 1 < low || indoorWater_[i] > high + 2) indoorHealth_[i] = subBounded(indoorHealth_[i], 2);
      else if (indoorCare_[i] == 0 && indoorStage(i) >= 3) indoorHealth_[i] = subBounded(indoorHealth_[i], 1);
      else indoorHealth_[i] = addBounded(indoorHealth_[i], 1);
      indoorCare_[i] = subBounded(indoorCare_[i], 1);
    }
  }

  const uint32_t day = ageTicks / TICKS_PER_DAY_BT;
  if (day != lastBoostDay_) {
    if (lastBoostDay_ != 0xFFFFFFFFUL && day > lastBoostDay_)
      discoveredIndoor_ &= static_cast<uint16_t>(~FLORIST_ORDER_DONE);
    lastBoostDay_ = day;
    uint8_t boosts = 0;
    for (uint8_t i = 0; i < indoorCapacity_ && boosts < 3; ++i) {
      if (indoorSpecies_[i] != EMPTY_PLANT && indoorStage(i) >= 4 && indoorHealth_[i] >= 7) {
        applyIndoorDailyBoost(indoorSpecies_[i]);
        ++boosts;
      }
    }
  }
  clampAll();
  markSaveDirty();
}

void BotanySystem::developerUnlockAll() {
  gardenCapacity_ = BOTANY_GARDEN_PLOTS;
  indoorCapacity_ = BOTANY_INDOOR_POTS;
  discoveredCrops_ = static_cast<uint16_t>((1U << BOTANY_CROP_COUNT) - 1U);
  discoveredIndoor_ = static_cast<uint16_t>((1U << BOTANY_INDOOR_COUNT) - 1U);
  saveNow();
}

void BotanySystem::developerFillGarden() {
  gardenCapacity_ = BOTANY_GARDEN_PLOTS;
  for (uint8_t i = 0; i < BOTANY_GARDEN_PLOTS; ++i) {
    cropSpecies_[i] = static_cast<uint8_t>((i * 3) % BOTANY_CROP_COUNT);
    cropAge_[i] = cropGrowTicks(cropSpecies_[i]);
    cropWater_[i] = 8;
    cropHealth_[i] = 10;
    cropCare_[i] = 5;
  }
  developerUnlockAll();
}

void BotanySystem::developerFillPlantRoom() {
  indoorCapacity_ = BOTANY_INDOOR_POTS;
  for (uint8_t i = 0; i < BOTANY_INDOOR_POTS; ++i) {
    indoorSpecies_[i] = static_cast<uint8_t>((i * 2 + 1) % BOTANY_INDOOR_COUNT);
    indoorAge_[i] = indoorGrowTicks(indoorSpecies_[i]);
    indoorWater_[i] = static_cast<uint8_t>((indoorIdealLow(indoorSpecies_[i]) + indoorIdealHigh(indoorSpecies_[i])) / 2);
    indoorHealth_[i] = 10;
    indoorCare_[i] = 5;
  }
  developerUnlockAll();
}

void BotanySystem::developerMaxCare() {
  for (uint8_t i = 0; i < BOTANY_GARDEN_PLOTS; ++i) { cropWater_[i] = 8; cropHealth_[i] = 10; cropCare_[i] = 10; }
  for (uint8_t i = 0; i < BOTANY_INDOOR_POTS; ++i) {
    indoorHealth_[i] = 10; indoorCare_[i] = 10;
    if (indoorSpecies_[i] != EMPTY_PLANT) indoorWater_[i] = static_cast<uint8_t>((indoorIdealLow(indoorSpecies_[i]) + indoorIdealHigh(indoorSpecies_[i])) / 2);
  }
  saveNow();
}

void BotanySystem::developerNeglect() {
  for (uint8_t i = 0; i < BOTANY_GARDEN_PLOTS; ++i) if (cropSpecies_[i] != EMPTY_PLANT) { cropWater_[i] = 0; cropHealth_[i] = 2; cropCare_[i] = 0; }
  for (uint8_t i = 0; i < BOTANY_INDOOR_POTS; ++i) if (indoorSpecies_[i] != EMPTY_PLANT) { indoorWater_[i] = 0; indoorHealth_[i] = 2; indoorCare_[i] = 0; }
  saveNow();
}
