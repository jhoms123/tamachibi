#include "ParadiseSystem.h"
#include <stddef.h>
#include <string.h>

ParadiseSystem::ParadiseSystem(size_t storageBase)
  : storeA_(storageBase), storeB_(storageBase + sizeof(ParadiseSave)) {
  reset();
}

void ParadiseSystem::reset() {
  sequence_ = 0;
  missionClaimedMask_ = 0;
  biofuel_ = 0;
  toyOwnedMask_ = 1U; // simple ball unlocked from the beginning
  for (uint8_t i = 0; i < PARADISE_FIELD_COUNT; ++i) placedToy_[i] = 255;
  fieldExpandedMask_ = 0;
  lastEggHuntDay_ = 0xFFFFFFFFUL;
  lastConnectionDay_ = 0xFFFFFFFFUL;
  connectionUses_ = 0;
  emergencyDay_ = 0xFFFFFFFFUL;
  emergencyType_ = 0;
  emergencyPending_ = 0;
  emergencyWins_ = 0;
  emergencyLosses_ = 0;
  travelVisitedMask_ = 0;
  researchPoints_ = 0;
}

uint32_t ParadiseSystem::checksum(const ParadiseSave &save) const {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&save);
  uint32_t value = 2166136261UL;
  for (size_t i = 0; i < offsetof(ParadiseSave, checksum); ++i) {
    value ^= bytes[i];
    value *= 16777619UL;
  }
  return value;
}

bool ParadiseSystem::valid(const ParadiseSave &save) const {
  return save.magic == PARADISE_SAVE_MAGIC &&
         save.version == PARADISE_SAVE_VERSION &&
         save.checksum == checksum(save);
}

void ParadiseSystem::fillSave(ParadiseSave &save) const {
  memset(&save, 0, sizeof(save));
  save.magic = PARADISE_SAVE_MAGIC;
  save.version = PARADISE_SAVE_VERSION;
  save.sequence = sequence_ + 1UL;
  save.missionClaimedMask = missionClaimedMask_;
  save.biofuel = biofuel_;
  save.toyOwnedMask = toyOwnedMask_;
  memcpy(save.placedToy, placedToy_, sizeof(placedToy_));
  save.fieldExpandedMask = fieldExpandedMask_;
  save.lastEggHuntDay = lastEggHuntDay_;
  save.lastConnectionDay = lastConnectionDay_;
  save.connectionUses = connectionUses_;
  save.emergencyDay = emergencyDay_;
  save.emergencyType = emergencyType_;
  save.emergencyPending = emergencyPending_;
  save.emergencyWins = emergencyWins_;
  save.emergencyLosses = emergencyLosses_;
  save.travelVisitedMask = travelVisitedMask_;
  save.researchPoints = researchPoints_;
  save.checksum = checksum(save);
}

void ParadiseSystem::applySave(const ParadiseSave &save) {
  sequence_ = save.sequence;
  missionClaimedMask_ = save.missionClaimedMask;
  biofuel_ = save.biofuel > 999U ? 999U : save.biofuel;
  toyOwnedMask_ = save.toyOwnedMask | 1U;
  memcpy(placedToy_, save.placedToy, sizeof(placedToy_));
  for (uint8_t i = 0; i < PARADISE_FIELD_COUNT; ++i) {
    if (placedToy_[i] >= PARADISE_TOY_COUNT) placedToy_[i] = 255;
  }
  fieldExpandedMask_ = save.fieldExpandedMask;
  lastEggHuntDay_ = save.lastEggHuntDay;
  lastConnectionDay_ = save.lastConnectionDay;
  connectionUses_ = save.connectionUses > 3 ? 3 : save.connectionUses;
  emergencyDay_ = save.emergencyDay;
  emergencyType_ = save.emergencyType % 2;
  emergencyPending_ = save.emergencyPending ? 1 : 0;
  emergencyWins_ = save.emergencyWins;
  emergencyLosses_ = save.emergencyLosses;
  travelVisitedMask_ = save.travelVisitedMask;
  researchPoints_ = save.researchPoints > 9999U ? 9999U : save.researchPoints;
}

bool ParadiseSystem::load() {
  const ParadiseSave a = storeA_.read();
  const ParadiseSave b = storeB_.read();
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

void ParadiseSystem::saveNow() {
  ParadiseSave save;
  fillSave(save);
  if (save.sequence & 1UL) storeA_.write(save);
  else storeB_.write(save);
  sequence_ = save.sequence;
}

bool ParadiseSystem::missionClaimed(uint8_t mission) const {
  mission %= PARADISE_MISSION_COUNT;
  return (missionClaimedMask_ & (1U << mission)) != 0;
}

void ParadiseSystem::claimMission(uint8_t mission) {
  mission %= PARADISE_MISSION_COUNT;
  missionClaimedMask_ |= (1U << mission);
}

uint8_t ParadiseSystem::claimedMissionCount() const {
  uint16_t bits = missionClaimedMask_;
  uint8_t count = 0;
  while (bits) { count += bits & 1U; bits >>= 1; }
  return count;
}

void ParadiseSystem::addBiofuel(uint16_t amount) {
  { const uint32_t total = static_cast<uint32_t>(biofuel_) + amount; biofuel_ = total > 999U ? 999U : static_cast<uint16_t>(total); }
}

bool ParadiseSystem::spendBiofuel(uint16_t amount) {
  if (biofuel_ < amount) return false;
  biofuel_ -= amount;
  return true;
}

bool ParadiseSystem::toyOwned(uint8_t toy) const {
  toy %= PARADISE_TOY_COUNT;
  return (toyOwnedMask_ & (1U << toy)) != 0;
}

void ParadiseSystem::unlockToy(uint8_t toy) {
  toy %= PARADISE_TOY_COUNT;
  toyOwnedMask_ |= (1U << toy);
}

uint8_t ParadiseSystem::placedToy(uint8_t field) const {
  return placedToy_[field % PARADISE_FIELD_COUNT];
}

void ParadiseSystem::placeToy(uint8_t field, uint8_t toy) {
  field %= PARADISE_FIELD_COUNT;
  placedToy_[field] = toy < PARADISE_TOY_COUNT ? toy : 255;
}

bool ParadiseSystem::fieldExpanded(uint8_t field) const {
  return (fieldExpandedMask_ & (1U << (field % PARADISE_FIELD_COUNT))) != 0;
}

void ParadiseSystem::expandField(uint8_t field) {
  fieldExpandedMask_ |= (1U << (field % PARADISE_FIELD_COUNT));
}

void ParadiseSystem::beginConnectionDay(uint32_t day) {
  if (lastConnectionDay_ != day) {
    lastConnectionDay_ = day;
    connectionUses_ = 0;
  }
}

bool ParadiseSystem::useConnection() {
  if (connectionUses_ >= 3) return false;
  ++connectionUses_;
  return true;
}

void ParadiseSystem::scheduleEmergency(uint32_t day, uint8_t type) {
  emergencyDay_ = day;
  emergencyType_ = type % 2;
  emergencyPending_ = 1;
}

void ParadiseSystem::resolveEmergency(bool success) {
  emergencyPending_ = 0;
  if (success) {
    if (emergencyWins_ < 255) ++emergencyWins_;
  } else if (emergencyLosses_ < 255) {
    ++emergencyLosses_;
  }
}

bool ParadiseSystem::destinationVisited(uint8_t destination) const {
  destination %= PARADISE_DESTINATION_COUNT;
  return (travelVisitedMask_ & (1U << destination)) != 0;
}

void ParadiseSystem::markDestinationVisited(uint8_t destination) {
  destination %= PARADISE_DESTINATION_COUNT;
  travelVisitedMask_ |= (1U << destination);
}

uint8_t ParadiseSystem::visitedDestinationCount() const {
  uint16_t bits = travelVisitedMask_;
  uint8_t count = 0;
  while (bits) { count += bits & 1U; bits >>= 1; }
  return count;
}

void ParadiseSystem::addResearch(uint16_t amount) {
  { const uint32_t total = static_cast<uint32_t>(researchPoints_) + amount; researchPoints_ = total > 9999U ? 9999U : static_cast<uint16_t>(total); }
}
