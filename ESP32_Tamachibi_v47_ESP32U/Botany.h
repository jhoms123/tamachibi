#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "GameDisplay.h"
#include "Esp32Platform.h"

constexpr uint8_t BOTANY_GARDEN_PLOTS = 4;
constexpr uint8_t BOTANY_INDOOR_POTS = 4;
constexpr uint8_t BOTANY_CROP_COUNT = 12;
constexpr uint8_t BOTANY_INDOOR_COUNT = 10;
constexpr uint32_t BOTANY_SAVE_MAGIC = 0x42544E31UL; // BTN1
constexpr uint8_t BOTANY_SAVE_VERSION = 1;

struct BotanySave {
  uint32_t magic;
  uint8_t version;
  uint32_t sequence;

  uint8_t cropSpecies[BOTANY_GARDEN_PLOTS];
  uint16_t cropAge[BOTANY_GARDEN_PLOTS];
  uint8_t cropWater[BOTANY_GARDEN_PLOTS];
  uint8_t cropHealth[BOTANY_GARDEN_PLOTS];
  uint8_t cropCare[BOTANY_GARDEN_PLOTS];

  uint8_t indoorSpecies[BOTANY_INDOOR_POTS];
  uint16_t indoorAge[BOTANY_INDOOR_POTS];
  uint8_t indoorWater[BOTANY_INDOOR_POTS];
  uint8_t indoorHealth[BOTANY_INDOOR_POTS];
  uint8_t indoorCare[BOTANY_INDOOR_POTS];

  uint8_t gardenCapacity;
  uint8_t indoorCapacity;
  uint8_t selectedGarden;
  uint8_t selectedIndoor;
  uint16_t discoveredCrops;
  uint16_t discoveredIndoor;
  uint32_t lastBoostDay;
  uint32_t checksum;
};

class BotanySystem {
public:
  BotanySystem(size_t storageBase, GameDisplay &display,
               uint8_t buttonLeft, uint8_t buttonSelect, uint8_t buttonRight);

  void reset();
  bool load();
  void saveNow();
  void tick(bool catchUpMode);

  void runGarden();
  void runPlantRoom();
  void drawGardenPreview(int x, int y, uint8_t frame = 0) const;
  void drawPlantRoomPreview(int x, int y, uint8_t frame = 0) const;

  bool gardenReady() const;
  bool needsCare() const;
  bool hasIndoorPlants() const;
  bool indoorNeedsTending() const;
  uint8_t gardenCount() const;
  uint8_t indoorCount() const;
  uint8_t gardenMatureCount() const;
  uint8_t gardenAverageStage() const;
  uint8_t gardenAverageHealth() const;
  uint8_t gardenAverageWater() const;
  uint8_t indoorAverageStage() const;
  uint8_t indoorAverageHealth() const;
  void eventWaterGarden(uint8_t amount);
  void eventTendGarden(uint8_t amount);
  void eventDamageGarden(uint8_t amount);
  void eventWaterIndoor(uint8_t amount);
  void eventTendIndoor(uint8_t amount);
  void eventDamageIndoor(uint8_t amount);

  // Catalog helpers used by the Tama Book. They reuse the same finished
  // growth artwork shown in the garden and plant room.
  const __FlashStringHelper *cropDisplayName(uint8_t species) const;
  const __FlashStringHelper *indoorDisplayName(uint8_t species) const;
  bool cropDiscovered(uint8_t species) const;
  bool indoorDiscovered(uint8_t species) const;
  uint8_t discoveredCropCount() const;
  uint8_t discoveredIndoorCount() const;
  void drawCropCatalogSprite(uint8_t species, int x, int groundY,
                             uint8_t stage = 4) const;
  void drawIndoorCatalogSprite(uint8_t species, int x, int baseY,
                               uint8_t stage = 4) const;

  // Developer Lab helpers.
  void developerUnlockAll();
  void developerFillGarden();
  void developerFillPlantRoom();
  void developerMaxCare();
  void developerNeglect();

private:
  Esp32EEPROMStorage<BotanySave> storeA_;
  Esp32EEPROMStorage<BotanySave> storeB_;
  GameDisplay &display_;
  uint8_t btnL_, btnS_, btnR_;

  uint32_t sequence_;
  uint8_t cropSpecies_[BOTANY_GARDEN_PLOTS];
  uint16_t cropAge_[BOTANY_GARDEN_PLOTS];
  uint8_t cropWater_[BOTANY_GARDEN_PLOTS];
  uint8_t cropHealth_[BOTANY_GARDEN_PLOTS];
  uint8_t cropCare_[BOTANY_GARDEN_PLOTS];

  uint8_t indoorSpecies_[BOTANY_INDOOR_POTS];
  uint16_t indoorAge_[BOTANY_INDOOR_POTS];
  uint8_t indoorWater_[BOTANY_INDOOR_POTS];
  uint8_t indoorHealth_[BOTANY_INDOOR_POTS];
  uint8_t indoorCare_[BOTANY_INDOOR_POTS];

  uint8_t gardenCapacity_;
  uint8_t indoorCapacity_;
  uint8_t selectedGarden_;
  uint8_t selectedIndoor_;
  uint16_t discoveredCrops_;
  uint16_t discoveredIndoor_;
  uint32_t lastBoostDay_;

  uint32_t checksum(const BotanySave &save) const;
  bool valid(const BotanySave &save) const;
  void fillSave(BotanySave &save) const;
  void applySave(const BotanySave &save);
  void clampState();

  const __FlashStringHelper *cropName(uint8_t species) const;
  const __FlashStringHelper *indoorName(uint8_t species) const;
  uint8_t cropCost(uint8_t species) const;
  uint16_t cropGrowTicks(uint8_t species) const;
  uint8_t indoorCost(uint8_t species) const;
  uint16_t indoorGrowTicks(uint8_t species) const;
  uint8_t indoorIdealLow(uint8_t species) const;
  uint8_t indoorIdealHigh(uint8_t species) const;
  uint8_t indoorWaterInterval(uint8_t species) const;

  uint8_t cropStage(uint8_t plot) const;
  uint8_t indoorStage(uint8_t pot) const;
  bool cropReady(uint8_t plot) const;
  void drawCropSprite(uint8_t species, int x, int groundY, uint8_t stage,
                      bool unhealthy, uint8_t frame = 0) const;
  void drawIndoorSprite(uint8_t species, int x, int baseY, uint8_t stage,
                        bool unhealthy, uint8_t frame = 0) const;

  void drawGardenOverview(uint8_t frame) const;
  void drawPlantRoomOverview(uint8_t frame) const;
  void gardenActionMenu();
  void indoorActionMenu();
  void chooseCrop();
  void chooseIndoorPlant();
  void waterCrop();
  void tendCrop();
  void harvestCrop();
  void clearCrop();
  void waterIndoor();
  void tendIndoor();
  void sellIndoor();
  void clearIndoor();
  void upgradeGarden();
  void upgradePlantRoom();

  void applyCropHarvest(uint8_t species, uint8_t quality);
  void applyIndoorDailyBoost(uint8_t species);
  void popup(const __FlashStringHelper *line1,
             const __FlashStringHelper *line2,
             uint16_t holdMs = 850);
};
