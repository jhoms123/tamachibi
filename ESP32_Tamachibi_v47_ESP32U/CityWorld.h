#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "GameDisplay.h"
#include "Esp32Platform.h"

constexpr uint8_t CITY_RESIDENT_COUNT = 8;
constexpr uint8_t HOUSE_ROOM_COUNT = 5;
constexpr uint8_t CITY_PLACE_COUNT = 6;
constexpr uint32_t CITY_SAVE_MAGIC = 0x43545932UL; // CTY2
constexpr uint8_t CITY_SAVE_VERSION = 3;

struct CityWorldSave {
  uint32_t magic;
  uint8_t version;
  uint32_t sequence;
  uint8_t relation[CITY_RESIDENT_COUNT];
  uint16_t metMask;
  uint8_t selectedResident;
  uint8_t selectedHouseRoom;
  uint8_t selectedCityPlace;
  // Occupies the three padding bytes before checksum in v2, so the save pair
  // and every following EEPROM address stay the same size.
  uint8_t neighborMemory[3];
  uint32_t checksum;
};

static_assert(sizeof(CityWorldSave) == 32, "City save layout must remain 32 bytes");

class CityWorldSystem {
public:
  CityWorldSystem(size_t storageBase, GameDisplay &display,
                  uint8_t buttonLeft, uint8_t buttonSelect, uint8_t buttonRight);

  void reset();
  bool load();
  void saveNow();

  uint8_t houseRoom() const { return selectedHouseRoom_; }
  uint8_t cityPlace() const { return selectedCityPlace_; }
  void previousHouseRoom();
  void nextHouseRoom();
  void previousCityPlace();
  void nextCityPlace();

  const __FlashStringHelper *houseRoomName(uint8_t id) const;
  const __FlashStringHelper *cityPlaceName(uint8_t id) const;
  const __FlashStringHelper *residentName(uint8_t id) const;
  uint8_t residentSpecies(uint8_t id) const;
  uint8_t residentRoomStyle(uint8_t id) const;
  uint8_t relation(uint8_t id) const;
  uint8_t friendCountMet() const;

  void visitResidents();
  void drawResidentTiny(uint8_t id, int x, int y, bool wave = false) const;
  void drawPartnerAtHome(uint8_t id, int x, int y, bool wave = false) const;
  bool partnerLivesAtHome(uint8_t id, uint8_t bond) const;

  void developerMaxRelations();
  void developerResetRelations();

private:
  Esp32EEPROMStorage<CityWorldSave> storeA_;
  Esp32EEPROMStorage<CityWorldSave> storeB_;
  GameDisplay &display_;
  uint8_t btnL_, btnS_, btnR_;
  uint32_t sequence_;
  uint8_t relation_[CITY_RESIDENT_COUNT];
  uint16_t metMask_;
  uint8_t selectedResident_;
  uint8_t selectedHouseRoom_;
  uint8_t selectedCityPlace_;
  uint8_t neighborMemory_[3];

  uint32_t checksum(const CityWorldSave &save) const;
  bool valid(const CityWorldSave &save) const;
  void fillSave(CityWorldSave &save) const;
  void applySave(const CityWorldSave &save);
  void residentInteractionMenu();
  void chooseNeighborChat();
  void chooseNeighborGift();
  void runNeighborPlay();
  void chooseNeighborOuting(bool invited);
  void runNeighborOuting(uint8_t outing, bool invited);
  void runNeighborDate();
  void showNeighborMoment(uint8_t resident, uint8_t action, uint8_t choice,
                          uint8_t previousMemory, uint8_t visits,
                          uint8_t chapterBefore, uint8_t chapterAfter,
                          bool favorite, bool repeated);
  void drawNeighborChoice(uint8_t resident, uint8_t action, uint8_t choice,
                          uint8_t frame, bool invited = false) const;
  uint8_t lastNeighborMemory(uint8_t id) const;
  void rememberNeighbor(uint8_t id, uint8_t memory);
  void drawResidentRoom(uint8_t id, uint8_t frame, bool showFooter) const;
  void drawRoomFurniture(uint8_t style) const;
  void changeRelation(uint8_t id, int amount);
  void popup(const __FlashStringHelper *line1,
             const __FlashStringHelper *line2,
             uint16_t holdMs = 850);
};
