#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// All output is 128x128 with square pixels. Older activity drawing commands
// are collected as semantic text + artwork, then laid out into separate
// regions. Native scenes bypass the collector. No framebuffer is stretched.
class GameDisplay : public Adafruit_GFX {
public:
  explicit GameDisplay(Adafruit_SH1107 &panel);
  bool begin(uint8_t address, bool reset = true);
  void display();
  void clearDisplay();
  void useNativeLayout(bool native) { requestedNative_ = native; }
  bool nativeLayout() const { return requestedNative_; }
  void useGameCanvas(bool active) { gameCanvas_ = active; }
  bool gameCanvas() const { return gameCanvas_; }
  void setGameButtons(const char *left, const char *middle, const char *right);
  void setContrast(uint8_t c) { panel_.setContrast(c); }
  void oled_command(uint8_t c) { panel_.oled_command(c); }
  void invertDisplay(bool invert) override { panel_.invertDisplay(invert); }
  using Print::write;
  size_t write(uint8_t c) override;
  void drawPixel(int16_t x, int16_t y, uint16_t color) override;
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override;
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override;
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                uint16_t color) override;
  void fillScreen(uint16_t color) override;
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                     uint16_t color);
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                     uint16_t color);
  // Explicit, width-bounded text and chrome supplied by shared UI helpers.
  void setPageTitle(const char *title, const char *context = nullptr);
  void setPageFooter(const char *left, const char *middle, const char *right);

private:
  static constexpr uint8_t MAX_RUNS = 40;
  static constexpr uint8_t RUN_CHARS = 96;
  struct Run {
    int16_t x, y;
    uint8_t len;
    bool selected;
    char text[RUN_CHARS];
  };
  Adafruit_SH1107 &panel_;
  bool requestedNative_, frameNative_, primitive_, gameCanvas_;
  uint8_t art_[1024];
  Run runs_[MAX_RUNS];
  uint8_t runCount_;
  int16_t selectedY_;
  uint32_t pageFingerprint_, pageStartedMs_;
  char title_[64], context_[24], footer_[3][24], gameButtons_[3][24];
  void compose();
  bool container(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color,
                 bool fill);
  void physicalText(const char *s, int x, int y, int width,
                    uint16_t ink = SH110X_WHITE);
};
#ifndef SSD1306_WHITE
#define SSD1306_WHITE 1
#endif
#ifndef SSD1306_BLACK
#define SSD1306_BLACK 0
#endif
