#pragma once
#include <Adafruit_GFX.h>
#include <fstream>
#define SH110X_WHITE 1
#define SH110X_BLACK 0
#define SH110X_INVERSE 2
class Adafruit_SH1107 : public Adafruit_GFX {
public:
  uint8_t pixels[128 * 128]{};
  Adafruit_SH1107() : Adafruit_GFX(128, 128) {}
  bool begin(uint8_t, bool = true) { return true; }
  void drawPixel(int16_t x, int16_t y, uint16_t ink) override {
    if (x < 0 || y < 0 || x >= 128 || y >= 128)
      return;
    if (ink == 2)
      pixels[y * 128 + x] ^= 1;
    else
      pixels[y * 128 + x] = ink ? 1 : 0;
  }
  void display() {}
  void clearDisplay() { memset(pixels, 0, sizeof(pixels)); }
  void setContrast(uint8_t) {}
  void oled_command(uint8_t) {}
  void save(const char *name) {
    std::ofstream f(name);
    f << "P1\n128 128\n";
    for (int i = 0; i < 16384; ++i)
      f << int(!pixels[i]) << ((i % 128 == 127) ? '\n' : ' ');
  }
};
