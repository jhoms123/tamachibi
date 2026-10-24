#pragma once
#include <stdint.h>

// A bounded playback order: shuffle visits every song once, and Previous
// retraces that same order. Folder filtering is applied before building it.
class MusicQueue {
public:
  static constexpr uint16_t Capacity = 128;
  void reset() { count_ = position_ = 0; }
  void add(uint8_t track) { if (count_ < Capacity) order_[count_++] = track; }
  uint16_t count() const { return count_; }
  uint16_t position() const { return position_; }
  uint8_t current() const { return count_ ? order_[position_] : 0; }
  void select(uint8_t track) {
    for (uint16_t i = 0; i < count_; ++i) if (order_[i] == track) { position_ = i; return; }
    position_ = 0;
  }
  void shuffle(uint32_t seed, uint8_t first) {
    for (uint16_t i = count_; i > 1; --i) {
      seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
      const uint16_t j = seed % i;
      uint8_t tmp = order_[i - 1]; order_[i - 1] = order_[j]; order_[j] = tmp;
    }
    select(first);
    if (count_) { uint8_t tmp = order_[0]; order_[0] = order_[position_]; order_[position_] = tmp; }
    position_ = 0;
  }
  bool step(int direction, bool wrap) {
    if (!count_) return false;
    if (direction < 0) {
      if (!position_) { if (!wrap) return false; position_ = count_ - 1; }
      else --position_;
    } else {
      if (position_ + 1 == count_) { if (!wrap) return false; position_ = 0; }
      else ++position_;
    }
    return true;
  }
private:
  uint8_t order_[Capacity]{};
  uint16_t count_ = 0, position_ = 0;
};
