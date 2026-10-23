#pragma once
#include <stdint.h>
#include "GameplayActions.h"

// Runtime reward ledger. Actual activities may repeat; cross-system growth is
// earned at most once per activity per simulation tick. No EEPROM layout change.
class GameplayProgress {
 public:
  GameplayProgress() { reset(); }
  void reset() { tick_ = UINT32_MAX; generation_ = 0; actions_ = 0; routines_ = 0; }
  bool claim(uint8_t action, uint32_t tick, uint16_t generation) {
    if (action >= 64) return false;
    synchronize(tick, generation);
    const uint64_t bit = uint64_t(1) << action;
    if (actions_ & bit) return false;
    actions_ |= bit;
    return true;
  }
  bool claimRoutine(uint8_t routine, uint32_t tick, uint16_t generation) {
    synchronize(tick, generation);
    if (routines_ & routine) return false;
    routines_ |= routine;
    return true;
  }
  static bool finishedGame(bool resolved) {
    return resolved;
  }
  static uint8_t interest(uint8_t lineage, uint8_t temperament, uint16_t birth) {
    static const uint8_t interests[10] = {M_GARDEN, M_PLANTROOM, M_AQUARIUM,
      M_STUDY, M_TRAIN, M_MUSIC, M_EXPLORE, M_COOK, M_CRAFT, M_GAME};
    return interests[(uint32_t(birth) + lineage * 7U + temperament * 3U) % 10U];
  }
  static bool familyReady(uint8_t stage, uint8_t bond) {
    return stage >= 4 && bond >= 80;
  }
 private:
  void synchronize(uint32_t tick, uint16_t generation) {
    if (tick_ == tick && generation_ == generation) return;
    tick_ = tick; generation_ = generation; actions_ = 0; routines_ = 0;
  }
  uint32_t tick_;
  uint16_t generation_;
  uint64_t actions_;
  uint8_t routines_;
};
