#pragma once
#include "GameDisplay.h"

// One grid, one font, fixed screen regions. All measurements are physical
// pixels.
namespace NativeUI {
constexpr int WIDTH = 128, HEADER_BOTTOM = 17, CONTENT_TOP = 21,
              CONTENT_BOTTOM = 112, FOOTER_TOP = 115;
class Scope {
public:
  explicit Scope(GameDisplay &d) : d_(d), old_(d.nativeLayout()) {
    d_.useNativeLayout(true);
  }
  ~Scope() { d_.useNativeLayout(old_); }

private:
  GameDisplay &d_;
  bool old_;
};
class GameScope {
public:
  GameScope(GameDisplay &d, const char *left, const char *middle,
            const char *right)
      : d_(d), old_(d.gameCanvas()) {
    d_.useGameCanvas(true);
    d_.setGameButtons(left, middle, right);
  }
  ~GameScope() {
    d_.useGameCanvas(old_);
    d_.setGameButtons("", "", "");
  }

private:
  GameDisplay &d_;
  bool old_;
};
void text(GameDisplay &d, const char *s, int x, int y, int width,
          uint16_t ink = 1);
void text(GameDisplay &d, const __FlashStringHelper *s, int x, int y, int width,
          uint16_t ink = 1);
void header(GameDisplay &d, const char *title, const char *context = nullptr,
            bool alert = false);
void header(GameDisplay &d, const __FlashStringHelper *title,
            const char *context = nullptr, bool alert = false);
void footer(GameDisplay &d, const char *left, const char *middle,
            const char *right);
int paragraph(GameDisplay &d, const char *s, int x, int y, int width,
              int maxLines, uint16_t ink = 1);
void meter(GameDisplay &d, int x, int y, int width, int value,
           int maximum = 10);
void icon(GameDisplay &d, uint8_t id, int x, int y, uint16_t ink = 1);
void pet(GameDisplay &d, int x, int y, uint8_t species, uint8_t stage,
         uint8_t frame, bool asleep = false, int scale = 1,
         uint8_t evolutionPath = 0, bool facingLeft = false);
void accessory(GameDisplay &d, int x, int y, uint8_t outfit, int scale = 1);
void plant(GameDisplay &d, int x, int y, uint8_t species, uint8_t growth,
           bool bad = false, uint8_t frame = 0);
void crop(GameDisplay &d, int x, int y, uint8_t species, uint8_t growth,
          bool bad = false, uint8_t frame = 0);
void fish(GameDisplay &d, int x, int y, uint8_t species, bool right = true,
          bool selected = false);
void family(GameDisplay &d, uint8_t children, bool spouse,
            uint8_t spouseSpecies, bool companion, uint8_t frame);
void room(GameDisplay &d, uint8_t room, uint8_t theme, uint8_t hour,
          uint8_t frame);
void town(GameDisplay &d, uint8_t place, uint8_t frame);
void message(GameDisplay &d, const char *title, const char *body,
             const char *action = "OK");
void flashCopy(char *out, const __FlashStringHelper *s, size_t size);
} // namespace NativeUI
