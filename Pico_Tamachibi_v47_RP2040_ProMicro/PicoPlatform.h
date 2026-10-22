#pragma once

#include <Arduino.h>
#include <EEPROM.h>

// The Arduino-Pico core emulates EEPROM in a 4 KB flash sector.
// The full dual-slot Tamachibi save uses a little over 1 KB.
constexpr size_t PICO_EEPROM_BYTES = 4096;

class PicoRTCCompat {
public:
  PicoRTCCompat() : epochBase_(0), millisBase_(0) {}

  void begin(bool preserve = false) {
    if (!preserve || millisBase_ == 0) {
      epochBase_ = 0;
      millisBase_ = millis();
    }
  }

  void setEpoch(uint32_t epoch) {
    epochBase_ = epoch;
    millisBase_ = millis();
  }

  uint32_t getEpoch() const {
    return epochBase_ + static_cast<uint32_t>(millis() - millisBase_) / 1000UL;
  }

private:
  uint32_t epochBase_;
  uint32_t millisBase_;
};

template <typename T>
class PicoEEPROMStorage {
public:
  explicit PicoEEPROMStorage(size_t address) : address_(address) {}

  T read() const {
    T value{};
    if (address_ + sizeof(T) <= PICO_EEPROM_BYTES) {
      EEPROM.get(address_, value);
    }
    return value;
  }

  void write(const T &value) const {
    if (address_ + sizeof(T) > PICO_EEPROM_BYTES) return;
    EEPROM.put(address_, value);
    EEPROM.commit();
  }

private:
  size_t address_;
};
