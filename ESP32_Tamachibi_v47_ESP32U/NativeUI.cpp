#include "NativeUI.h"
#include <algorithm>
#include <cstring>

namespace NativeUI {
void flashCopy(char *out, const __FlashStringHelper *s, size_t size) {
  if (!size)
    return;
  if (!s) {
    out[0] = 0;
    return;
  }
  strncpy_P(out, reinterpret_cast<PGM_P>(s), size - 1);
  out[size - 1] = 0;
}
void text(GameDisplay &d, const char *s, int x, int y, int width,
          uint16_t ink) {
  if (!s || width < 6)
    return;
  const int cap = std::min(21, width / 6);
  char b[22];
  strncpy(b, s, cap);
  b[cap] = 0;
  if (static_cast<int>(strlen(s)) > cap && cap > 2) {
    b[cap - 3] = '.';
    b[cap - 2] = '.';
    b[cap - 1] = '.';
  }
  d.setTextSize(1);
  d.setTextWrap(false);
  d.setTextColor(ink);
  d.setCursor(x, y);
  d.print(b);
}
void text(GameDisplay &d, const __FlashStringHelper *s, int x, int y, int width,
          uint16_t ink) {
  char b[160];
  flashCopy(b, s, sizeof(b));
  text(d, b, x, y, width, ink);
}
void header(GameDisplay &d, const char *title, const char *context,
            bool alert) {
  d.fillRect(0, 0, 128, 18, 0);
  int right = (context && *context)
                  ? std::min(42, static_cast<int>(strlen(context)) * 6)
                  : 0;
  text(d, title, 5, 5, 118 - right - (right ? 6 : 0) - (alert ? 8 : 0));
  if (right)
    text(d, context, 123 - right, 5, right);
  if (alert) {
    d.fillCircle(118 - right - (right ? 6 : 0), 8, 2, 1);
  }
  d.drawFastHLine(4, 17, 120, 1);
}
void header(GameDisplay &d, const __FlashStringHelper *title,
            const char *context, bool alert) {
  char b[64];
  flashCopy(b, title, sizeof(b));
  header(d, b, context, alert);
}
void footer(GameDisplay &d, const char *a, const char *b, const char *c) {
  d.fillRect(0, 114, 128, 14, 0);
  d.drawFastHLine(4, 114, 120, 1);
  const char *labels[] = {a, b, c};
  for (int i = 0; i < 3; ++i) {
    const char *s = labels[i] ? labels[i] : "";
    int len = std::min(6, static_cast<int>(strlen(s)));
    text(d, s, i * 43 + (42 - len * 6) / 2, 119, 40);
  }
}
int paragraph(GameDisplay &d, const char *s, int x, int y, int width,
              int maxLines, uint16_t ink) {
  if (!s)
    return 0;
  int lines = 0, cap = std::min(21, width / 6);
  while (*s && lines < maxLines) {
    while (*s == ' ')
      ++s;
    if (!*s)
      break;
    int n = 0;
    while (s[n] && s[n] != '\n' && n < cap)
      ++n;
    if (n == cap && s[n] && s[n] != '\n') {
      int cut = n;
      while (cut > 0 && s[cut] != ' ')
        --cut;
      if (cut > 0)
        n = cut;
    }
    char b[22];
    memcpy(b, s, n);
    b[n] = 0;
    if (lines == maxLines - 1 && s[n] && s[n] != '\n' && n >= 3) {
      b[n - 3] = '.';
      b[n - 2] = '.';
      b[n - 1] = '.';
    }
    text(d, b, x, y + lines * 10, width, ink);
    ++lines;
    s += n;
    if (*s == '\n')
      ++s;
  }
  return lines;
}
void meter(GameDisplay &d, int x, int y, int width, int value, int maximum) {
  d.drawRoundRect(x, y, width, 5, 2, 1);
  int fill = maximum > 0
                 ? (width - 4) * std::max(0, std::min(value, maximum)) / maximum
                 : 0;
  if (fill)
    d.fillRect(x + 2, y + 2, fill, 1, 1);
}
// New 16px symbols share a 1px stroke and a 2px internal margin.
void icon(GameDisplay &d, uint8_t id, int x, int y, uint16_t ink) {
  switch (id % 12) {
  case 0: // bowl
    d.drawLine(x + 2, y + 8, x + 5, y + 13, ink);
    d.drawFastHLine(x + 5, y + 13, 7, ink);
    d.drawLine(x + 11, y + 13, x + 14, y + 8, ink);
    d.drawFastHLine(x + 2, y + 8, 13, ink);
    d.drawPixel(x + 6, y + 4, ink);
    d.drawPixel(x + 10, y + 2, ink);
    break;
  case 1: // care cross
    d.drawRoundRect(x + 1, y + 1, 14, 14, 3, ink);
    d.fillRect(x + 6, y + 4, 4, 8, ink);
    d.fillRect(x + 4, y + 6, 8, 4, ink);
    break;
  case 2: // game controller
    d.drawRoundRect(x + 1, y + 4, 14, 9, 3, ink);
    d.drawFastHLine(x + 3, y + 8, 5, ink);
    d.drawFastVLine(x + 5, y + 6, 5, ink);
    d.drawPixel(x + 11, y + 7, ink);
    d.drawPixel(x + 12, y + 10, ink);
    break;
  case 3: // heart
    d.fillCircle(x + 5, y + 5, 3, ink);
    d.fillCircle(x + 11, y + 5, 3, ink);
    d.fillTriangle(x + 2, y + 6, x + 14, y + 6, x + 8, y + 14, ink);
    break;
  case 4: // compass
    d.drawCircle(x + 8, y + 8, 7, ink);
    d.fillTriangle(x + 10, y + 3, x + 5, y + 9, x + 9, y + 11, ink);
    break;
  case 5: // chart
    d.drawRoundRect(x + 1, y + 1, 14, 14, 2, ink);
    d.drawFastVLine(x + 4, y + 9, 3, ink);
    d.drawFastVLine(x + 8, y + 6, 6, ink);
    d.drawFastVLine(x + 12, y + 3, 9, ink);
    break;
  case 6: // note
    d.drawFastVLine(x + 10, y + 2, 10, ink);
    d.drawLine(x + 10, y + 2, x + 14, y + 4, ink);
    d.fillCircle(x + 7, y + 12, 3, ink);
    break;
  case 7: // plant
    d.drawFastVLine(x + 8, y + 4, 9, ink);
    d.drawLine(x + 8, y + 8, x + 3, y + 4, ink);
    d.drawLine(x + 8, y + 6, x + 12, y + 2, ink);
    d.drawRoundRect(x + 3, y + 11, 11, 4, 1, ink);
    break;
  case 8: // house
    d.drawLine(x + 1, y + 7, x + 8, y + 1, ink);
    d.drawLine(x + 8, y + 1, x + 15, y + 7, ink);
    d.drawRect(x + 3, y + 7, 11, 8, ink);
    d.drawRect(x + 7, y + 10, 3, 5, ink);
    break;
  case 9: // fish
    d.drawRoundRect(x + 5, y + 4, 10, 8, 3, ink);
    d.drawTriangle(x, y + 3, x + 5, y + 8, x, y + 13, ink);
    d.drawPixel(x + 12, y + 7, ink);
    break;
  case 10: // bag
    d.drawRoundRect(x + 2, y + 4, 12, 11, 2, ink);
    d.drawRoundRect(x + 5, y + 1, 6, 5, 2, ink);
    d.drawFastHLine(x + 5, y + 9, 6, ink);
    break;
  default: // message
    d.drawRoundRect(x + 1, y + 2, 14, 10, 3, ink);
    d.drawLine(x + 4, y + 12, x + 3, y + 15, ink);
    d.drawLine(x + 3, y + 15, x + 8, y + 12, ink);
    d.drawPixel(x + 5, y + 7, ink);
    d.drawPixel(x + 8, y + 7, ink);
    d.drawPixel(x + 11, y + 7, ink);
    break;
  }
}
void pet(GameDisplay &d, int x, int y, uint8_t species, uint8_t stage,
         uint8_t frame, bool asleep, int scale) {
  // Original 16x16 mascot, with six silhouettes and live stage/eye changes.
  // Draw each pixel uniformly: scale 2 always means 2x2, never 1x2.
  uint16_t rows[16] = {0,      0,      0x0ff0, 0x1ff8, 0x3ffc, 0x3ffc,
                       0x7ffe, 0x7ffe, 0x7ffe, 0x7ffe, 0x3ffc, 0x3ffc,
                       0x1ff8, 0x0ff0, 0x0c30, 0};
  if (stage == 0) {
    const uint16_t egg[16] = {0,      0,      0x0180, 0x03c0, 0x07e0, 0x0ff0,
                              0x0ff0, 0x1ff8, 0x1ff8, 0x1ff8, 0x1ff8, 0x0ff0,
                              0x07e0, 0,      0,      0};
    memcpy(rows, egg, sizeof(rows));
    rows[7] &= ~0x0240;
    rows[9] &= ~0x0100;
  } else {
    switch (species % 6) {
    case 0:
      rows[0] = 0x300c;
      rows[1] = 0x3c3c;
      rows[2] |= 0x3c3c;
      break;
    case 1:
      rows[0] = 0x0c30;
      rows[1] = 0x0c30;
      rows[2] |= 0x0c30;
      break;
    case 2:
      rows[1] = 0x0660;
      rows[2] |= 0x0660;
      rows[6] |= 0xc003;
      rows[7] |= 0xc003;
      break;
    case 3:
      rows[0] = 0x0180;
      rows[1] = 0x03c0;
      break;
    case 4:
      rows[2] = 0x1ff8;
      rows[3] = 0x3ffc;
      rows[4] |= 0x6006;
      break;
    default:
      rows[0] = 0x03c0;
      rows[1] = 0x07e0;
      rows[2] |= 0x07e0;
      break;
    }
    rows[7] &= ~(asleep || ((frame % 32) == 31) ? 0x0c30 : 0x0420);
    rows[8] &= ~(asleep ? 0 : 0x0420);
    rows[10] &= ~0x0180;
    switch (species / 6) {
    case 1:
      rows[9] &= ~0x0810;
      break;
    case 2:
      rows[11] &= ~0x03c0;
      break;
    case 3:
      rows[4] &= ~0x0180;
      break;
    case 4:
      rows[8] &= ~0x300c;
      break;
    case 5:
      rows[12] &= ~0x0180;
      break;
    case 6:
      rows[5] &= ~0x07e0;
      break;
    case 7:
      rows[5] &= ~0x0240;
      rows[11] &= ~0x0420;
      break;
    case 8:
      rows[4] &= ~0x0180;
      rows[5] &= ~0x0240;
      rows[6] &= ~0x0180;
      break;
    }
    if (stage == 1) {
      rows[0] = rows[1] = 0;
    }
    if (stage == 5) {
      rows[10] &= ~0x0810;
    }
  }
  int bob = (frame / 8) % 2;
  for (int yy = 0; yy < 16; ++yy)
    for (int xx = 0; xx < 16; ++xx)
      if (rows[yy] & (0x8000 >> xx))
        d.fillRect(x + xx * scale, y + (yy - bob) * scale, scale, scale, 1);
}
void accessory(GameDisplay &d, int x, int y, uint8_t outfit) {
  if (!outfit)
    return;
  switch ((outfit - 1) % 4) {
  case 0:
    d.drawFastHLine(x + 3, y + 1, 11, 1);
    d.fillRect(x + 5, y - 1, 7, 3, 1);
    break;
  case 1:
    d.fillTriangle(x + 9, y + 2, x + 13, y + 5, x + 13, y - 1, 1);
    d.fillTriangle(x + 9, y + 2, x + 6, y + 5, x + 6, y - 1, 1);
    break;
  case 2:
    d.drawFastHLine(x + 4, y + 11, 8, 0);
    d.drawFastVLine(x + 11, y + 11, 4, 0);
    break;
  case 3:
    d.drawRect(x + 3, y + 6, 4, 4, 0);
    d.drawRect(x + 9, y + 6, 4, 4, 0);
    d.drawFastHLine(x + 7, y + 7, 2, 0);
    break;
  }
}
void plant(GameDisplay &d, int x, int y, uint8_t species, uint8_t growth,
           bool bad) {
  d.drawRoundRect(x + 2, y + 15, 12, 5, 1, 1);
  d.drawFastHLine(x + 4, y + 21, 8, 1);
  if (!growth) {
    d.drawPixel(x + 8, y + 13, 1);
    return;
  }
  int top = std::max(1, 13 - static_cast<int>(growth) * 2);
  d.drawLine(x + 8, y + 15, x + 8, y + top, 1);
  d.drawLine(x + 8, y + 11, x + 3, y + 8 + (bad ? 3 : 0), 1);
  d.drawLine(x + 8, y + 8, x + 13, y + 5 + (bad ? 4 : 0), 1);
  if (growth >= 3) {
    d.drawCircle(x + 8, y + top, 3, 1);
    if (species % 2)
      d.fillCircle(x + 8, y + top, 1, 1);
  }
  if (bad)
    d.drawPixel(x + 15, y + 18, 1);
}
void fish(GameDisplay &d, int x, int y, uint8_t species, bool right,
          bool selected) {
  d.drawRoundRect(x + 3, y + 1, 11, 7, 3, 1);
  d.fillTriangle(x + 3, y + 4, x, y, x, y + 8, 1);
  d.drawPixel(x + (right ? 11 : 5), y + 3, 1);
  d.drawLine(x + 7, y + 1, x + 9, y - 1, 1);
  if (species % 3 == 0)
    d.drawFastVLine(x + 7, y + 2, 4, 1);
  if (selected) {
    // The marker shares the fish's lane; it cannot collide with the next lane.
    d.fillRect(x - 4, y + 3, 2, 2, 1);
  }
}
namespace {
void wall(GameDisplay &d) {
  d.drawRoundRect(4, 21, 120, 78, 3, 1);
  d.drawFastHLine(6, 25, 116, 1);
  for (int y = 34; y < 95; y += 16)
    for (int x = 12; x < 120; x += 16) {
      d.drawPixel(x, y, 1);
      d.drawPixel(x + 1, y, 1);
    }
  d.fillRect(49, 21, 30, 5, 0);
  d.drawRect(49, 21, 30, 5, 1);
  d.drawFastVLine(64, 21, 5, 1);
  d.fillRect(58, 96, 14, 4, 0);
  d.drawLine(58, 96, 58, 99, 1);
  d.drawLine(72, 96, 72, 99, 1);
}
void bed(GameDisplay &d, int x, int y) {
  d.fillRect(x, y, 25, 29, 0);
  d.drawRoundRect(x, y, 25, 29, 2, 1);
  d.drawRoundRect(x + 4, y + 3, 17, 7, 2, 1);
  d.drawFastHLine(x + 1, y + 12, 23, 1);
  d.drawFastHLine(x + 1, y + 15, 23, 1);
}
void shelf(GameDisplay &d, int x, int y) {
  d.fillRect(x, y, 25, 12, 0);
  d.drawRect(x, y, 25, 12, 1);
  for (int i = 4; i < 22; i += 4)
    d.drawFastVLine(x + i, y + 2, 8, 1);
}
} // namespace
void room(GameDisplay &d, uint8_t roomId, uint8_t theme, uint8_t hour,
          uint8_t frame) {
  wall(d);
  // All furniture is authored in plan view; every item has a reserved
  // footprint.
  if (roomId == 0) {
    bed(d, 11, 31);
    shelf(d, 91, 29);
    d.fillRect(45, 57, 40, 27, 0);
    d.drawRoundRect(45, 57, 40, 27, 3, 1);
    d.drawRoundRect(48, 60, 34, 21, 2, 1);
    d.fillRect(11, 74, 25, 16, 0);
    d.drawRoundRect(11, 74, 25, 16, 3, 1);
    d.drawFastHLine(12, 79, 23, 1);
    d.drawFastVLine(23, 80, 9, 1);
    d.fillRect(96, 72, 19, 16, 0);
    d.drawRoundRect(96, 72, 19, 16, 3, 1);
    plant(d, 97, 45, theme, 3, false);
  } else if (roomId == 1) {
    d.fillRect(13, 31, 102, 36, 0);
    d.drawRoundRect(13, 31, 102, 36, 3, 1);
    d.drawRoundRect(16, 34, 96, 30, 2, 1);
    fish(d, 28 + (frame / 4) % 12, 43, 1, true, false);
    fish(d, 76 - (frame / 4) % 10, 48, 3, false, false);
    d.drawCircle(98, 41, 2, 1);
    d.drawCircle(101, 53, 1, 1);
    shelf(d, 12, 79);
  } else if (roomId == 2 || roomId == 3) {
    for (int i = 0; i < 6; ++i) {
      int x = 14 + (i % 3) * 36, y = 33 + (i / 3) * 30;
      d.fillRect(x - 2, y - 2, 23, 27, 0);
      plant(d, x, y, i + theme, 2 + i % 3, false);
    }
  } else if (roomId == 4) {
    d.fillRect(11, 31, 105, 16, 0);
    d.drawRoundRect(11, 31, 105, 16, 2, 1);
    d.drawCircle(25, 39, 5, 1);
    d.drawCircle(43, 39, 5, 1);
    d.drawRect(92, 33, 20, 12, 1);
    d.fillRect(43, 63, 42, 25, 0);
    d.drawRoundRect(43, 63, 42, 25, 3, 1);
    d.drawCircle(64, 75, 6, 1);
    d.drawRect(31, 68, 9, 13, 1);
    d.drawRect(88, 68, 9, 13, 1);
  } else {
    bed(d, 11, 31);
    bed(d, 91, 31);
    shelf(d, 52, 31);
    d.drawRoundRect(46, 70, 35, 18, 4, 1);
  }
  if (hour < 7 || hour >= 19) {
    d.fillRect(51, 22, 26, 3, 0);
    d.drawPixel(55, 23, 1);
    d.drawPixel(68, 23, 1);
  }
}
void family(GameDisplay &d, uint8_t children, bool spouse,
            uint8_t spouseSpecies, bool companion, uint8_t frame) {
  // This row is empty floor, away from beds, shelves, plants and the main pet.
  if (children || spouse)
    d.fillRect(40, 32, 51, 23, 0);
  for (uint8_t i = 0; i < std::min<uint8_t>(2, children); ++i)
    pet(d, 42 + i * 17, 36, i + 2, 2, frame);
  if (spouse)
    pet(d, 76, 36, spouseSpecies, 4, frame);
  if (companion) {
    d.fillCircle(42, 91, 3, 1);
    d.drawPixel(40, 87, 1);
    d.drawPixel(44, 87, 1);
    d.drawPixel(41, 90, 0);
    d.drawPixel(43, 90, 0);
  }
}
void town(GameDisplay &d, uint8_t place, uint8_t frame) {
  // Quiet dotted grass, continuous paved paths, and readable building
  // footprints.
  for (int y = 25; y < 101; y += 8)
    for (int x = 6; x < 125; x += 11)
      if ((x + y) % 3)
        d.drawPixel(x, y, 1);
  d.fillRect(0, 61, 128, 15, 0);
  d.drawFastHLine(0, 61, 128, 1);
  d.drawFastHLine(0, 75, 128, 1);
  d.fillRect(57, 20, 15, 82, 0);
  d.drawFastVLine(57, 20, 82, 1);
  d.drawFastVLine(71, 20, 82, 1);
  for (int x = 6; x < 128; x += 12)
    d.drawFastHLine(x, 68, 5, 1);
  const int xs[] = {8, 85, 8, 85};
  const int ys[] = {24, 24, 80, 80};
  for (int i = 0; i < 4; ++i) {
    int x = xs[i], y = ys[i];
    d.fillRect(x - 2, y - 1, 36, 24, 0);
    d.drawRoundRect(x, y, 33, 21, 2, 1);
    d.drawFastHLine(x + 1, y + 5, 31, 1);
    d.drawRect(x + 5, y + 9, 6, 5, 1);
    d.drawRect(x + 21, y + 9, 6, 5, 1);
    d.drawRect(x + 14, y + 14, 5, 7, 1);
    if (i == place % 4) {
      d.drawRoundRect(x - 3, y - 3, 39, 27, 3, 1);
    }
  }
  pet(d, 57, 79, place, 4, frame, false, 1);
}
void message(GameDisplay &d, const char *title, const char *body,
             const char *action) {
  Scope scope(d);
  d.clearDisplay();
  header(d, title);
  icon(d, 11, 56, 28);
  paragraph(d, body, 8, 55, 114, 5);
  footer(d, "", action, "");
  d.display();
}
} // namespace NativeUI
