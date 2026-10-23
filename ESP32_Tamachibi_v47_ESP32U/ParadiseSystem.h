#pragma once

#include <Arduino.h>
#include "Esp32Platform.h"

constexpr uint8_t PARADISE_MISSION_COUNT = 12;
constexpr uint8_t PARADISE_FIELD_COUNT = 8;
constexpr uint8_t PARADISE_TOY_COUNT = 8;
constexpr uint8_t PARADISE_DESTINATION_COUNT = 8;
constexpr uint32_t PARADISE_SAVE_MAGIC = 0x50524431UL; // PRD1
constexpr uint8_t PARADISE_SAVE_VERSION = 1;

struct ParadiseSave {
  uint32_t magic;
  uint8_t version;
  uint32_t sequence;
  uint16_t missionClaimedMask;
  uint16_t biofuel;
  uint16_t toyOwnedMask;
  uint8_t placedToy[PARADISE_FIELD_COUNT];
  uint8_t fieldExpandedMask;
  uint32_t lastEggHuntDay;
  uint32_t lastConnectionDay;
  uint8_t connectionUses;
  uint32_t emergencyDay;
  uint8_t emergencyType;
  uint8_t emergencyPending;
  uint8_t emergencyWins;
  uint8_t emergencyLosses;
  uint16_t travelVisitedMask;
  uint16_t researchPoints;
  uint32_t checksum;
};

class ParadiseSystem {
public:
  explicit ParadiseSystem(size_t storageBase);

  void reset();
  bool load();
  void saveNow();

  bool missionClaimed(uint8_t mission) const;
  void claimMission(uint8_t mission);
  uint8_t claimedMissionCount() const;

  uint16_t biofuel() const { return biofuel_; }
  void addBiofuel(uint16_t amount);
  bool spendBiofuel(uint16_t amount);

  bool toyOwned(uint8_t toy) const;
  void unlockToy(uint8_t toy);
  uint8_t placedToy(uint8_t field) const;
  void placeToy(uint8_t field, uint8_t toy);
  bool fieldExpanded(uint8_t field) const;
  void expandField(uint8_t field);

  uint32_t lastEggHuntDay() const { return lastEggHuntDay_; }
  void setLastEggHuntDay(uint32_t day) { lastEggHuntDay_ = day; }

  void beginConnectionDay(uint32_t day);
  uint8_t connectionUses() const { return connectionUses_; }
  bool useConnection();

  void scheduleEmergency(uint32_t day, uint8_t type);
  bool emergencyPending() const { return emergencyPending_ != 0; }
  uint8_t emergencyType() const { return emergencyType_; }
  uint32_t emergencyDay() const { return emergencyDay_; }
  void resolveEmergency(bool success);
  uint8_t emergencyWins() const { return emergencyWins_; }
  uint8_t emergencyLosses() const { return emergencyLosses_; }

  bool destinationVisited(uint8_t destination) const;
  void markDestinationVisited(uint8_t destination);
  uint8_t visitedDestinationCount() const;

  uint16_t researchPoints() const { return researchPoints_; }
  void addResearch(uint16_t amount);
  bool spendResearch(uint16_t amount);

private:
  Esp32EEPROMStorage<ParadiseSave> storeA_;
  Esp32EEPROMStorage<ParadiseSave> storeB_;

  uint32_t sequence_;
  uint16_t missionClaimedMask_;
  uint16_t biofuel_;
  uint16_t toyOwnedMask_;
  uint8_t placedToy_[PARADISE_FIELD_COUNT];
  uint8_t fieldExpandedMask_;
  uint32_t lastEggHuntDay_;
  uint32_t lastConnectionDay_;
  uint8_t connectionUses_;
  uint32_t emergencyDay_;
  uint8_t emergencyType_;
  uint8_t emergencyPending_;
  uint8_t emergencyWins_;
  uint8_t emergencyLosses_;
  uint16_t travelVisitedMask_;
  uint16_t researchPoints_;

  uint32_t checksum(const ParadiseSave &save) const;
  bool valid(const ParadiseSave &save) const;
  void fillSave(ParadiseSave &save) const;
  void applySave(const ParadiseSave &save);
};
