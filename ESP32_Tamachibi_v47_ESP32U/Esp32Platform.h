#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include <stddef.h>
#if __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif
#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

// Keep the v47 dual-slot save layout used by the RP2040 port.
constexpr size_t TAMACHIBI_EEPROM_BYTES = 4096;

class Esp32RTCCompat {
public:
  Esp32RTCCompat() : epochBase_(0), millisBase_(0) {}

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
class Esp32EEPROMStorage {
public:
  explicit Esp32EEPROMStorage(size_t address) : address_(address) {}

  T read() const {
    T value{};
    if (address_ + sizeof(T) <= TAMACHIBI_EEPROM_BYTES) {
      EEPROM.get(address_, value);
    }
    return value;
  }

  void write(const T &value) const {
    if (address_ + sizeof(T) > TAMACHIBI_EEPROM_BYTES) return;
    EEPROM.put(address_, value);
    EEPROM.commit();
  }

private:
  size_t address_;
};

// Hide the Arduino-ESP32 LEDC API change between core 2.x and 3.x from game code.
inline void esp32BuzzerBegin(uint8_t pin) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(pin, 1000, 8);
  ledcWrite(pin, 0);
#else
  constexpr uint8_t channel = 0;
  ledcSetup(channel, 1000, 8);
  ledcAttachPin(pin, channel);
  ledcWrite(channel, 0);
#endif
}

inline void esp32BuzzerPlay(uint8_t pin, uint16_t frequency, uint8_t duty) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcChangeFrequency(pin, frequency, 8);
  ledcWrite(pin, duty);
#else
  constexpr uint8_t channel = 0;
  ledcSetup(channel, frequency, 8);
  ledcWrite(channel, duty);
#endif
}

inline void esp32BuzzerStop(uint8_t pin) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pin, 0);
#else
  constexpr uint8_t channel = 0;
  ledcWrite(channel, 0);
#endif
  digitalWrite(pin, LOW);
}
