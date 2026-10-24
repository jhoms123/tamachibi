#pragma once
#include "NativeUI.h"

// Native 128x128 scene. Furniture stays behind the pet and its headphones.
inline void drawMusicRoom(GameDisplay &display, uint8_t species, uint8_t stage,
                          uint8_t evolutionPath, bool playing, uint32_t now) {
  const uint8_t frame = playing ? (now / 180) % 16 : 0;
  display.drawRect(4, 25, 120, 50, 1);
  display.drawLine(5, 66, 122, 66, 1);
  display.drawRect(10, 30, 23, 23, 1);
  display.drawLine(21, 30, 21, 52, 1);
  display.drawLine(10, 41, 32, 41, 1);
  display.fillCircle(27, 35, 2, 1);
  display.drawRect(96, 52, 19, 18, 1);
  display.drawCircle(105, 61, 5, 1);
  display.drawCircle(105, 61, 2, 1);
  display.fillRoundRect(41, 66, 47, 5, 2, 1);
  const int bob = (frame & 1) ? 1 : 0;
  NativeUI::pet(display, 48, 35, species, stage, frame, false, 2, evolutionPath);
  // Only the headband goes above the head; the face remains unobstructed.
  display.drawRoundRect(45, 32 + bob, 38, 25, 10, 1);
  display.fillRect(44, 45 + bob, 5, 12, 1);
  display.fillRect(79, 45 + bob, 5, 12, 1);
  display.drawLine(46, 47 + bob, 46, 54 + bob, 0);
  display.drawLine(81, 47 + bob, 81, 54 + bob, 0);
  if (playing) {
    for (int i = 0; i < 2; ++i) {
      int x = i ? 89 : 35;
      int y = 57 - ((now / 150 + i * 7) % 18);
      display.fillCircle(x, y, 2, 1);
      display.drawLine(x + 2, y, x + 2, y - 8, 1);
      display.drawLine(x + 2, y - 8, x + 6, y - 6, 1);
    }
  }
}
