#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// The game was originally laid out for a 128x64 display. Keep its drawing
// coordinates intact, then expand each logical row across two physical rows
// on the 128x128 SH1107. New square screens can switch to native coordinates.
class GameDisplay : public Adafruit_GFX {
 public:
  explicit GameDisplay(Adafruit_SH1107 &panel)
      : Adafruit_GFX(128, 64), panel_(panel), legacyLayout_(true) {}

  bool begin(uint8_t address, bool reset = true) {
    return panel_.begin(address, reset);
  }

  void display() {
    if (legacyLayout_) panel_.drawRect(0, 0, 128, 128, SH110X_WHITE);
    panel_.display();
  }
  void clearDisplay() { panel_.clearDisplay(); }
  void setContrast(uint8_t contrast) { panel_.setContrast(contrast); }
  void oled_command(uint8_t command) { panel_.oled_command(command); }
  void invertDisplay(bool invert) override { panel_.invertDisplay(invert); }

  void setLegacyLayout(bool legacy) {
    legacyLayout_ = legacy;
    HEIGHT = legacy ? 64 : 128;
    _height = legacy ? 64 : 128;
  }

  bool legacyLayout() const { return legacyLayout_; }

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    if (x < 0 || x >= 128 || y < 0 || y >= (legacyLayout_ ? 64 : 128)) return;
    const int16_t py = legacyLayout_ ? y * 2 : y;
    panel_.fillRect(x, py, 1, legacyLayout_ ? 2 : 1, color);
  }

  void drawFastHLine(int16_t x, int16_t y, int16_t w,
                     uint16_t color) override {
    if (y < 0 || y >= (legacyLayout_ ? 64 : 128) || w <= 0) return;
    panel_.fillRect(x, legacyLayout_ ? y * 2 : y, w,
                    legacyLayout_ ? 2 : 1, color);
  }

  void drawFastVLine(int16_t x, int16_t y, int16_t h,
                     uint16_t color) override {
    if (x < 0 || x >= 128 || h <= 0) return;
    panel_.fillRect(x, legacyLayout_ ? y * 2 : y, 1,
                    legacyLayout_ ? h * 2 : h, color);
  }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                uint16_t color) override {
    if (w <= 0 || h <= 0) return;
    panel_.fillRect(x, legacyLayout_ ? y * 2 : y, w,
                    legacyLayout_ ? h * 2 : h, color);
  }

  void fillScreen(uint16_t color) override { panel_.fillScreen(color); }

 private:
  Adafruit_SH1107 &panel_;
  bool legacyLayout_;
};

// Preserve the existing game code's color names while using SH110X values.
#ifndef SSD1306_WHITE
#define SSD1306_WHITE 1
#endif
#ifndef SSD1306_BLACK
#define SSD1306_BLACK 0
#endif
