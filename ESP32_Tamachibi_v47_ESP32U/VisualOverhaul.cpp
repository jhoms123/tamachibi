#include "VisualOverhaul.h"

namespace {

void printTwoDigits(GameDisplay &display, uint8_t value) {
  if (value < 10) display.print('0');
  display.print(value);
}

void drawWorldIcon(GameDisplay &display, uint8_t icon, int x, int y) {
  const uint16_t ink = SSD1306_BLACK;
  if (icon == 0) {  // pet face
    display.drawRoundRect(x, y + 1, 8, 6, 2, ink);
    display.drawPixel(x + 2, y + 3, ink);
    display.drawPixel(x + 5, y + 3, ink);
  } else if (icon == 1) {  // house
    display.drawTriangle(x, y + 4, x + 4, y, x + 8, y + 4, ink);
    display.drawRect(x + 1, y + 4, 7, 4, ink);
  } else {  // city
    display.drawRect(x, y + 3, 3, 5, ink);
    display.drawRect(x + 4, y + 1, 4, 7, ink);
    display.drawPixel(x + 5, y + 3, ink);
  }
}

void drawWindowWeather(GameDisplay &display, int x, int y, uint8_t hour, uint8_t weather, uint8_t anim) {
  display.drawRect(x, y, 22, 15, SSD1306_WHITE);
  display.drawLine(x + 11, y, x + 11, y + 15, SSD1306_WHITE);
  display.drawLine(x, y + 7, x + 22, y + 7, SSD1306_WHITE);
  const bool night = hour < 7 || hour >= 19;
  if (night) {
    display.drawCircle(x + 5, y + 4, 3, SSD1306_WHITE);
    display.fillCircle(x + 7, y + 3, 3, SSD1306_BLACK);
    display.drawPixel(x + 16, y + 3, SSD1306_WHITE);
    if (anim & 1) display.drawPixel(x + 18, y + 11, SSD1306_WHITE);
  } else {
    display.drawCircle(x + 5, y + 4, 3, SSD1306_WHITE);
    display.drawLine(x + 5, y, x + 5, y + 1, SSD1306_WHITE);
    display.drawLine(x + 1, y + 4, x + 2, y + 4, SSD1306_WHITE);
    display.drawLine(x + 8, y + 4, x + 10, y + 4, SSD1306_WHITE);
  }
  if (weather == 1) {
    for (uint8_t i = 0; i < 4; ++i) {
      const int dx = x + 3 + i * 5 + ((anim / 2) & 1);
      display.drawLine(dx, y + 9, dx - 2, y + 13, SSD1306_WHITE);
    }
  } else if (weather == 2) {
    display.drawPixel(x + 4, y + 11, SSD1306_WHITE);
    display.drawPixel(x + 10, y + 9, SSD1306_WHITE);
    display.drawPixel(x + 17, y + 12, SSD1306_WHITE);
  } else if (weather == 3) {
    display.drawLine(x + 3, y + 10, x + 16, y + 10, SSD1306_WHITE);
    display.drawLine(x + 8, y + 13, x + 20, y + 13, SSD1306_WHITE);
  } else if (weather == 4 && (anim & 1)) {
    display.drawPixel(x + 16, y + 10, SSD1306_WHITE);
    display.drawLine(x + 16, y + 8, x + 16, y + 12, SSD1306_WHITE);
    display.drawLine(x + 14, y + 10, x + 18, y + 10, SSD1306_WHITE);
  }
}

void drawThickShell(GameDisplay &display, uint8_t hour, bool lightsOn) {
  // Thick cutaway architecture: ceiling beam, side posts, baseboard and floor.
  display.fillRect(0, 9, 128, 3, SSD1306_WHITE);
  display.fillRect(0, 9, 4, 46, SSD1306_WHITE);
  display.fillRect(124, 9, 4, 46, SSD1306_WHITE);
  display.fillRect(0, 52, 128, 3, SSD1306_WHITE);
  display.drawLine(4, 15, 124, 15, SSD1306_WHITE);
  display.drawLine(4, 48, 124, 48, SSD1306_WHITE);
  // Floor boards stop before becoming visually noisy.
  for (uint8_t x = 8; x < 124; x += 16) {
    display.drawLine(x, 49, x - 2, 52, SSD1306_WHITE);
  }
  // Ceiling lamp, brighter only when appropriate.
  display.drawLine(64, 12, 64, 16, SSD1306_WHITE);
  display.drawLine(58, 19, 70, 19, SSD1306_WHITE);
  display.drawLine(60, 16, 68, 16, SSD1306_WHITE);
  if (lightsOn && (hour < 7 || hour >= 18)) {
    display.drawPixel(56, 21, SSD1306_WHITE);
    display.drawPixel(72, 21, SSD1306_WHITE);
  }
}

void drawThemeEdgeProps(GameDisplay &display, uint8_t theme, uint8_t anim) {
  switch (theme % 10) {
    case 0:  // starter cottage
      display.drawRoundRect(7, 39, 24, 10, 4, SSD1306_WHITE);
      display.drawLine(10, 49, 10, 52, SSD1306_WHITE);
      display.drawLine(28, 49, 28, 52, SSD1306_WHITE);
      break;
    case 1:  // cozy
      display.drawRoundRect(6, 38, 27, 11, 4, SSD1306_WHITE);
      display.drawCircle(13, 43, 2, SSD1306_WHITE);
      display.drawCircle(25, 43, 2, SSD1306_WHITE);
      display.drawRoundRect(91, 45, 27, 5, 2, SSD1306_WHITE);
      break;
    case 2:  // forest
      display.drawLine(15, 48, 15, 30, SSD1306_WHITE);
      display.drawCircle(9, 32, 5, SSD1306_WHITE);
      display.drawCircle(21, 30, 5, SSD1306_WHITE);
      display.drawRect(9, 47, 13, 4, SSD1306_WHITE);
      break;
    case 3:  // study
      display.drawRect(6, 35, 28, 13, SSD1306_WHITE);
      display.drawRect(9, 29, 9, 6, SSD1306_WHITE);
      display.drawLine(22, 38, 31, 38, SSD1306_WHITE);
      break;
    case 4:  // arcade
      display.drawRoundRect(6, 23, 25, 27, 4, SSD1306_WHITE);
      display.drawRect(10, 27, 17, 9, SSD1306_WHITE);
      display.drawCircle(22, 42, 2, SSD1306_WHITE);
      display.drawPixel(13, 42, SSD1306_WHITE);
      break;
    case 5:  // space
      display.drawCircle(18, 31, 11, SSD1306_WHITE);
      display.drawCircle(18, 31, 7, SSD1306_WHITE);
      display.drawPixel(9, 19, SSD1306_WHITE);
      display.drawPixel(29, 24, SSD1306_WHITE);
      break;
    case 6:  // tea
      display.drawRoundRect(7, 42, 27, 7, 3, SSD1306_WHITE);
      display.drawRect(13, 36, 8, 6, SSD1306_WHITE);
      display.drawCircle(25, 39, 3, SSD1306_WHITE);
      break;
    case 7:  // carnival
      display.drawLine(5, 22, 35, 22, SSD1306_WHITE);
      for (uint8_t i = 0; i < 4; ++i) display.drawTriangle(5 + i * 8, 22, 9 + i * 8, 22, 7 + i * 8, 27, SSD1306_WHITE);
      display.drawRoundRect(7, 40, 26, 9, 4, SSD1306_WHITE);
      break;
    case 8:  // aquarium
      display.drawRect(6, 25, 28, 19, SSD1306_WHITE);
      display.drawLine(6, 31, 34, 31, SSD1306_WHITE);
      display.drawTriangle(14, 37, 20, 33, 20, 41, SSD1306_WHITE);
      display.drawCircle(28, 35, 2, SSD1306_WHITE);
      break;
    default:  // memory/family
      display.drawRect(8, 24, 18, 13, SSD1306_WHITE);
      display.drawCircle(17, 30, 4, SSD1306_WHITE);
      display.drawRoundRect(7, 41, 27, 8, 4, SSD1306_WHITE);
      if (anim & 1) display.drawPixel(30, 20, SSD1306_WHITE);
      break;
  }
}

void drawRoomSpecificProps(GameDisplay &display, uint8_t room, uint8_t anim, bool familyLayout) {
  if (room == 0) {  // bedroom/living room
    if (familyLayout) {
      // When a spouse or child is present, furniture hugs the walls and the
      // center/right floor becomes a real actor area instead of an empty
      // permanent placeholder.
      display.drawRoundRect(6, 39, 25, 10, 4, SSD1306_WHITE);
      display.drawCircle(13, 44, 2, SSD1306_WHITE);
      display.drawCircle(24, 44, 2, SSD1306_WHITE);
      display.drawLine(96, 20, 118, 20, SSD1306_WHITE);
      display.drawRect(100, 22, 5, 7, SSD1306_WHITE);
      display.drawRect(109, 22, 5, 7, SSD1306_WHITE);
    } else {
      // Solo layout uses the whole room. A bookshelf, lamp and reading cushion
      // replace the spouse-shaped empty box that previously wasted space.
      display.drawRect(96, 22, 22, 26, SSD1306_WHITE);
      display.drawLine(98, 30, 116, 30, SSD1306_WHITE);
      display.drawLine(98, 38, 116, 38, SSD1306_WHITE);
      display.drawRect(100, 24, 4, 5, SSD1306_WHITE);
      display.drawRect(107, 24, 6, 5, SSD1306_WHITE);
      display.drawRect(100, 32, 7, 5, SSD1306_WHITE);
      display.drawRect(110, 32, 4, 5, SSD1306_WHITE);
      display.drawLine(90, 31, 90, 44, SSD1306_WHITE);
      display.drawTriangle(84, 31, 96, 31, 90, 24, SSD1306_WHITE);
      display.drawRoundRect(82, 44, 16, 6, 3, SSD1306_WHITE);
    }
  } else if (room == 1) {  // aquarium room
    display.drawRect(8, 22, 31, 23, SSD1306_WHITE);
    display.drawLine(8, 29, 39, 29, SSD1306_WHITE);
    display.drawTriangle(17, 36, 24, 32, 24, 40, SSD1306_WHITE);
    display.drawCircle(33, 34, 2, SSD1306_WHITE);
    display.drawRect(94, 39, 24, 9, SSD1306_WHITE);
  } else if (room == 2) {  // plant room
    for (uint8_t i = 0; i < 3; ++i) {
      const int x = 8 + i * 12;
      display.drawLine(x + 4, 47, x + 4, 33 - i * 2, SSD1306_WHITE);
      display.drawCircle(x, 35 - i * 2, 4, SSD1306_WHITE);
      display.drawCircle(x + 8, 34 - i * 2, 4, SSD1306_WHITE);
      display.drawRect(x, 46, 9, 4, SSD1306_WHITE);
    }
    display.drawLine(94, 25, 119, 25, SSD1306_WHITE);
    display.drawLine(101, 25, 101, 32, SSD1306_WHITE);
    display.drawCircle(101, 35, 4, SSD1306_WHITE);
  } else if (room == 3) {  // garden
    display.drawLine(4, 46, 124, 46, SSD1306_WHITE);
    for (uint8_t x = 8; x < 40; x += 10) {
      display.drawLine(x, 46, x, 39, SSD1306_WHITE);
      display.drawCircle(x - 2, 38, 3, SSD1306_WHITE);
      display.drawCircle(x + 3, 37, 3, SSD1306_WHITE);
    }
    display.drawRect(96, 34, 23, 12, SSD1306_WHITE);
    display.drawTriangle(94, 34, 107, 27, 121, 34, SSD1306_WHITE);
  } else {  // kitchen
    display.drawRect(6, 32, 31, 16, SSD1306_WHITE);
    display.drawLine(6, 37, 37, 37, SSD1306_WHITE);
    display.drawCircle(13, 42, 3, SSD1306_WHITE);
    display.drawCircle(27, 42, 3, SSD1306_WHITE);
    display.drawRect(94, 30, 25, 18, SSD1306_WHITE);
    display.drawLine(94, 36, 119, 36, SSD1306_WHITE);
    display.drawLine(101, 30, 101, 48, SSD1306_WHITE);
    if (anim & 1) {
      display.drawPixel(21, 28, SSD1306_WHITE);
      display.drawPixel(24, 26, SSD1306_WHITE);
    }
  }
}

}  // namespace

namespace V46UI {

void drawHeader(GameDisplay &display,
                const __FlashStringHelper *title,
                uint8_t hour,
                uint8_t minute,
                bool showAlert,
                uint8_t worldIcon) {
  display.fillRoundRect(2, 0, 124, HEADER_H, 3, SSD1306_WHITE);
  drawWorldIcon(display, worldIcon, 1, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(12, 0);
  display.print(title);
  int16_t x1 = 0, y1 = 0;
  uint16_t titleWidth = 0, titleHeight = 0;
  display.getTextBounds(title, 0, 0, &x1, &y1, &titleWidth, &titleHeight);
  if (titleWidth <= 76) {
    display.setCursor(91, 0);
    uint8_t shown = hour % 12;
    if (shown == 0) shown = 12;
    display.print(shown);
    display.print(':');
    printTwoDigits(display, minute);
  }
  if (showAlert) {
    display.fillCircle(124, 4, 3, SSD1306_BLACK);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(122, 0);
    display.print('!');
    display.setTextColor(SSD1306_BLACK);
  }
}

void drawFooter(GameDisplay &display,
                const __FlashStringHelper *left,
                const __FlashStringHelper *center,
                const __FlashStringHelper *right) {
  display.fillRoundRect(2, FOOTER_Y, 124, 9, 3, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(2, FOOTER_Y + 1);
  display.print(left);
  display.setCursor(43, FOOTER_Y + 1);
  display.print(center);
  display.setCursor(86, FOOTER_Y + 1);
  display.print(right);
}

void drawCarouselFrame(GameDisplay &display,
                       uint8_t selected,
                       uint8_t total,
                       bool locked,
                       uint8_t anim) {
  const int bob = (anim & 2) ? -1 : 0;

  // Focused one-item layout: one large icon card and one information card.
  // Previous/next items are intentionally not drawn as miniature copies.
  display.fillRoundRect(5, 13 + bob, 39, 35, 6, SSD1306_BLACK);
  display.drawRoundRect(5, 13 + bob, 39, 35, 6, SSD1306_WHITE);
  display.fillRoundRect(48, 13, 75, 35, 6, SSD1306_BLACK);
  display.drawRoundRect(48, 13, 75, 35, 6, SSD1306_WHITE);

  // Simple chevrons communicate D0/D2 navigation without pretending that
  // adjacent miniature icons are selectable at the same time.
  display.drawTriangle(1, 31, 4, 27, 4, 35, SSD1306_WHITE);
  display.drawTriangle(127, 31, 124, 27, 124, 35, SSD1306_WHITE);

  if (locked) {
    display.drawRect(109, 17, 8, 7, SSD1306_WHITE);
    display.drawCircle(113, 16, 4, SSD1306_WHITE);
  }

  // Compact progress track replaces the row of tiny page dots.
  display.drawRect(49, 50, 73, 3, SSD1306_WHITE);
  if (total > 0) {
    const int markerW = total > 12 ? 3 : max(3, 69 / total);
    const int markerX = 51 + static_cast<int>((static_cast<uint16_t>(selected) * (69 - markerW)) / (total > 1 ? total - 1 : 1));
    display.fillRect(markerX, 51, markerW, 1, SSD1306_WHITE);
  }
}

void drawCutawayRoom(GameDisplay &display,
                     uint8_t room,
                     uint8_t theme,
                     uint8_t hour,
                     uint8_t weather,
                     uint8_t anim,
                     bool lightsOn,
                     bool familyLayout) {
  drawThickShell(display, hour, lightsOn);
  drawWindowWeather(display, 99, 18, hour, weather, anim);
  drawThemeEdgeProps(display, theme, anim);
  drawRoomSpecificProps(display, room, anim, familyLayout);
}

void drawPetRoomBackdrop(GameDisplay &display,
                         uint8_t theme,
                         uint8_t hour,
                         uint8_t weather,
                         uint8_t anim,
                         bool lightsOn) {
  drawCutawayRoom(display, 0, theme, hour, weather, anim, lightsOn, false);
  // Rug frames the character without adding texture behind its face.
  display.drawRoundRect(39, 45, 50, 7, 3, SSD1306_WHITE);
  display.drawLine(45, 49, 83, 49, SSD1306_WHITE);
}

void clearCharacterZone(GameDisplay &display,
                        int16_t x,
                        int16_t y,
                        int16_t w,
                        int16_t h,
                        bool outline) {
  display.fillRoundRect(x, y, w, h, 6, SSD1306_BLACK);
  if (outline) display.drawRoundRect(x, y, w, h, 6, SSD1306_WHITE);
}

void drawNotificationToast(GameDisplay &display,
                           const __FlashStringHelper *text,
                           uint8_t icon,
                           uint8_t anim) {
  const int y = (anim & 1) ? 10 : 11;
  display.fillRoundRect(5, y, 118, 13, 4, SSD1306_BLACK);
  display.drawRoundRect(5, y, 118, 13, 4, SSD1306_WHITE);
  display.fillCircle(12, y + 6, 4, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(10, y + 3);
  display.print(icon == 0 ? '!' : (icon == 1 ? '?' : '*'));
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, y + 3);
  display.print(text);
}

void drawTimeline(GameDisplay &display,
                  uint8_t wakeHour,
                  uint8_t startHour,
                  uint8_t endHour,
                  uint8_t bedHour,
                  uint8_t nowHour,
                  bool activePeriod) {
  const int x0 = 8;
  const int x1 = 120;
  const int y = 39;
  display.drawLine(x0, y, x1, y, SSD1306_WHITE);
  auto hourX = [=](uint8_t h) -> int {
    const uint16_t minutes = static_cast<uint16_t>(h) * 60U;
    return x0 + static_cast<int>((minutes * (x1 - x0)) / (24U * 60U));
  };
  const int wakeX = hourX(wakeHour);
  const int startX = hourX(startHour);
  const int endX = hourX(endHour);
  const int bedX = hourX(bedHour);
  const int nowX = hourX(nowHour);
  display.fillCircle(wakeX, y, 2, SSD1306_WHITE);
  display.fillCircle(startX, y, 2, SSD1306_WHITE);
  display.fillCircle(endX, y, 2, SSD1306_WHITE);
  display.fillCircle(bedX, y, 2, SSD1306_WHITE);
  if (activePeriod && endX > startX) display.drawLine(startX, y - 3, endX, y - 3, SSD1306_WHITE);
  display.drawTriangle(nowX, y + 2, nowX - 3, y + 7, nowX + 3, y + 7, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(4, 47); display.print(F("WAKE"));
  display.setCursor(47, 47); display.print(F("DAY"));
  display.setCursor(99, 47); display.print(F("BED"));
}

void playCheckerTransition(GameDisplay &display,
                           bool reducedMotion,
                           uint8_t direction) {
  if (reducedMotion) return;
  for (uint8_t frame = 0; frame < 3; ++frame) {
    display.clearDisplay();
    const uint8_t step = frame + 1;
    for (uint8_t y = 0; y < 64; y += 4) {
      for (uint8_t x = 0; x < 128; x += 4) {
        const bool on = (((x / 4) + (y / 4) + direction) % 3) < step;
        if (on) display.fillRect(x, y, 3, 3, SSD1306_WHITE);
      }
    }
    display.display();
    delay(55);
  }
}

}  // namespace V46UI
