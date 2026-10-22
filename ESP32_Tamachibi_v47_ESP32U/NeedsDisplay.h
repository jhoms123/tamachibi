#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

enum NeedsDisplayMode : uint8_t {
  NEEDS_MODE_LIVE = 0,
  NEEDS_MODE_SCHOOL = 1,
  NEEDS_MODE_WORK = 2,
  NEEDS_MODE_SLEEP = 3,
  NEEDS_MODE_SITTER = 4,
  NEEDS_MODE_EGG = 5,
  NEEDS_MODE_DEAD = 6
};

struct NeedsDisplayState {
  uint8_t health;
  uint8_t food;
  uint8_t energy;
  uint8_t fun;
  uint8_t clean;
  uint8_t poop;
  uint8_t conditionSeverity;
  uint8_t mode;
  uint8_t location;
  uint8_t weekday;
  uint8_t hour;
  uint8_t minute;
  uint8_t wakeHour;
  uint8_t returnHour;
  uint8_t sitterHours;
  bool sick;
};

class NeedsDisplay {
public:
  explicit NeedsDisplay(TwoWire *wire);

  bool begin(uint8_t address = 0x3C);
  void showStartup();
  void update(const NeedsDisplayState &state, uint32_t nowMs, bool force = false);
  bool ready() const { return ready_; }

private:
  Adafruit_SSD1306 oled_;
  bool ready_;
  uint32_t lastDrawMs_;

  static uint8_t urgencyFor(uint8_t value, bool critical = false);
  static bool blinkVisible(uint8_t urgency, uint32_t nowMs);
  void drawNeedField(int16_t x, int16_t y, const char *label,
                     uint8_t value, uint8_t urgency, uint32_t nowMs,
                     uint8_t maximum = 10);
  void drawLive(const NeedsDisplayState &state, uint32_t nowMs);
  void drawAway(const NeedsDisplayState &state, const char *title,
                const char *detail, uint8_t resumeHour);
  void drawTimeLine(const NeedsDisplayState &state, const char *location);
  static const char *weekdayName(uint8_t weekday);
  static const char *locationName(uint8_t location);
};
