#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "GameDisplay.h"
#include "Esp32Platform.h"

constexpr uint8_t AQUARIUM_FISH_SLOTS = 4;
constexpr uint8_t AQUARIUM_SPECIES_COUNT = 12;
constexpr uint32_t AQUARIUM_SAVE_MAGIC = 0x41515531UL; // AQU1
constexpr uint8_t AQUARIUM_SAVE_VERSION = 1;

struct AquariumSave {
  uint32_t magic;
  uint8_t version;
  uint32_t sequence;
  uint8_t species[AQUARIUM_FISH_SLOTS];
  char names[AQUARIUM_FISH_SLOTS][7];
  uint8_t hunger[AQUARIUM_FISH_SLOTS];
  uint8_t mood[AQUARIUM_FISH_SLOTS];
  uint8_t health[AQUARIUM_FISH_SLOTS];
  uint8_t friendship[AQUARIUM_FISH_SLOTS];
  uint16_t ageTicks[AQUARIUM_FISH_SLOTS];
  uint8_t tankClean;
  uint8_t waterQuality;
  uint8_t tankDecor;
  uint8_t tankLevel;
  uint8_t foodStock;
  uint8_t remedyStock;
  uint8_t selectedSlot;
  uint8_t aquariumVisits;
  uint8_t careStreak;
  uint8_t surpriseType;
  uint16_t discoveredMask;
  uint32_t lastRewardDay;
  uint32_t lastInteractionTick;
  uint32_t checksum;
};

class AquariumSystem {
public:
  AquariumSystem(size_t storageBase, GameDisplay &display,
                 uint8_t buttonLeft, uint8_t buttonSelect, uint8_t buttonRight);

  void reset();
  bool load();
  void saveNow();
  void tick(bool catchUpMode);
  void runMenu();
  void drawSmallIcon(int x, int y, bool inverse = false);
  void showPetComment();

  bool hasFish() const;
  bool needsCare() const;
  bool healthy() const;
  uint8_t fishCount() const;
  uint8_t capacity() const;
  const char *selectedFishName() const;
  uint8_t tankClean() const { return tankClean_; }
  uint8_t waterQuality() const { return waterQuality_; }

  // Developer Lab helpers. These bypass costs and unlock requirements.
  void developerUnlockAll();
  void developerFillTank();
  void developerAddFish(uint8_t species);
  void developerMaxCare();
  void developerNeglectTank();

private:
  Esp32EEPROMStorage<AquariumSave> storeA_;
  Esp32EEPROMStorage<AquariumSave> storeB_;
  GameDisplay &display_;
  uint8_t btnL_, btnS_, btnR_;

  uint32_t sequence_;
  uint8_t species_[AQUARIUM_FISH_SLOTS];
  char names_[AQUARIUM_FISH_SLOTS][7];
  uint8_t hunger_[AQUARIUM_FISH_SLOTS];
  uint8_t mood_[AQUARIUM_FISH_SLOTS];
  uint8_t health_[AQUARIUM_FISH_SLOTS];
  uint8_t friendship_[AQUARIUM_FISH_SLOTS];
  uint16_t ageTicks_[AQUARIUM_FISH_SLOTS];
  uint8_t tankClean_;
  uint8_t waterQuality_;
  uint8_t tankDecor_;
  uint8_t tankLevel_;
  uint8_t foodStock_;
  uint8_t remedyStock_;
  uint8_t selectedSlot_;
  uint8_t aquariumVisits_;
  uint8_t careStreak_;
  uint8_t surpriseType_;
  uint16_t discoveredMask_;
  uint32_t lastRewardDay_;
  uint32_t lastInteractionTick_;

  uint32_t checksum(const AquariumSave &save) const;
  bool valid(const AquariumSave &save) const;
  void fillSave(AquariumSave &save) const;
  void applySave(const AquariumSave &save);

  uint8_t firstEmptySlot() const;
  uint8_t nextOccupiedSlot(uint8_t from, int8_t direction) const;
  bool slotOccupied(uint8_t slot) const;
  bool speciesUnlocked(uint8_t species) const;
  uint8_t speciesCost(uint8_t species) const;
  const __FlashStringHelper *speciesName(uint8_t species) const;
  const char *defaultFishName(uint8_t species) const;
  const __FlashStringHelper *decorName(uint8_t decor) const;
  uint8_t fishLifeStage(uint8_t slot) const;
  bool allFishComfortable() const;
  void clampState();

  void showTextPopup(const char *line1, const char *line2, uint16_t holdMs = 900);
  void showFlashPopup(const __FlashStringHelper *line1,
                      const __FlashStringHelper *line2,
                      uint16_t holdMs = 900);
  void drawFishSprite(uint8_t species, int x, int y, bool facingRight,
                      uint8_t lifeStage, bool selected);
  void drawTankScene(uint8_t selected, uint16_t frame, bool includePet);
  void drawTankMeters();
  void watchTank();
  void fishStatusPage();
  void feedFish();
  void cleanTankAction();
  void buyFish();
  void renameSelectedFish();
  void renameFish(uint8_t slot);
  void decorateTank();
  void upgradeTank();
  void suppliesShop();
  void treatFish();
  void interactWithFish(uint8_t slot);
  void maybeCreateFry(bool catchUpMode);
  void applyDailyTankEffects(bool catchUpMode);
};
