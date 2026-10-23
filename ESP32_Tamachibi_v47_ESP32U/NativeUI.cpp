#include "NativeUI.h"
#include "GrowthSprites.h"
#include "RosterSprites.h"
#include <algorithm>
#include <cstring>

namespace NativeUI {
namespace {
void cutEggPixel(uint32_t rows[32], int x, int y) {
  if (x >= 0 && x < 32 && y >= 0 && y < 32)
    rows[y] &= ~(0x80000000UL >> x);
}

void addEggEmblem(uint32_t rows[32], uint8_t family) {
  // The shell carries a distinct cutout emblem for its hatch lineage.
  switch (family % TAMACHIBI_FAMILY_COUNT) {
  case 0: cutEggPixel(rows,14,11); cutEggPixel(rows,15,12); cutEggPixel(rows,13,12); cutEggPixel(rows,16,13); cutEggPixel(rows,12,13); cutEggPixel(rows,17,12); break;
  case 1: cutEggPixel(rows,12,11); cutEggPixel(rows,19,11); cutEggPixel(rows,14,14); cutEggPixel(rows,17,14); cutEggPixel(rows,15,17); cutEggPixel(rows,16,17); break;
  case 2: cutEggPixel(rows,12,12); cutEggPixel(rows,13,13); cutEggPixel(rows,14,14); cutEggPixel(rows,19,14); cutEggPixel(rows,20,13); cutEggPixel(rows,21,12); break;
  case 3: cutEggPixel(rows,15,10); cutEggPixel(rows,16,10); cutEggPixel(rows,13,13); cutEggPixel(rows,18,13); cutEggPixel(rows,14,16); cutEggPixel(rows,17,16); break;
  case 4: for (int x=12;x<=19;++x) cutEggPixel(rows,x,12); for (int x=14;x<=17;++x) cutEggPixel(rows,x,15); break;
  case 5: cutEggPixel(rows,12,15); cutEggPixel(rows,13,13); cutEggPixel(rows,14,11); cutEggPixel(rows,19,15); cutEggPixel(rows,18,13); cutEggPixel(rows,17,11); break;
  case 6: cutEggPixel(rows,13,12); cutEggPixel(rows,14,12); cutEggPixel(rows,18,12); cutEggPixel(rows,19,12); cutEggPixel(rows,14,15); cutEggPixel(rows,18,15); cutEggPixel(rows,16,18); break;
  case 7: cutEggPixel(rows,13,16); cutEggPixel(rows,14,14); cutEggPixel(rows,15,12); cutEggPixel(rows,16,10); cutEggPixel(rows,17,12); cutEggPixel(rows,18,14); cutEggPixel(rows,19,16); break;
  case 8: for (int x=14;x<=17;++x) cutEggPixel(rows,x,10); cutEggPixel(rows,12,15); cutEggPixel(rows,19,15); cutEggPixel(rows,14,18); cutEggPixel(rows,17,18); break;
  default: cutEggPixel(rows,13,12); cutEggPixel(rows,14,12); cutEggPixel(rows,18,12); cutEggPixel(rows,19,12); cutEggPixel(rows,14,14); cutEggPixel(rows,18,14); cutEggPixel(rows,16,17); cutEggPixel(rows,17,17); break;
  }
}

void buildEggRows(uint8_t family, uint32_t rows[32]) {
  static const uint8_t halfWidth[32] = {
      0, 0, 0, 1, 2, 4, 5, 6, 7, 8, 8, 9, 9, 9, 9, 9,
      9, 9, 9, 9, 8, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0};
  memset(rows, 0, sizeof(uint32_t) * 32);
  for (int y = 3; y <= 28; ++y) {
    for (int x = 16 - halfWidth[y]; x <= 16 + halfWidth[y]; ++x)
      rows[y] |= 0x80000000UL >> x;
  }
  // A short zig-zag crack makes the egg readable at both native sizes.
  rows[14] &= ~(0x80000000UL >> 14);
  rows[15] &= ~(0x80000000UL >> 15);
  rows[15] &= ~(0x80000000UL >> 16);
  rows[16] &= ~(0x80000000UL >> 17);
  rows[17] &= ~(0x80000000UL >> 16);
  addEggEmblem(rows, family);
}

void clear32(uint32_t rows[32], int x, int y) {
  if (x >= 0 && x < 32 && y >= 0 && y < 32)
    rows[y] &= ~(0x80000000UL >> x);
}

bool rowBounds(const uint32_t rows[32], int y, int &left, int &right) {
  const uint32_t row = rows[y];
  if (!row)
    return false;
  left = 0;
  while (left < 32 && !(row & (0x80000000UL >> left)))
    ++left;
  right = 31;
  while (right >= 0 && !(row & (0x80000000UL >> right)))
    --right;
  return true;
}

bool maskBounds(const uint32_t rows[32], int &left, int &right, int &top,
                int &bottom) {
  left = 32;
  right = -1;
  top = 32;
  bottom = -1;
  for (int y = 0; y < 32; ++y) {
    int rowLeft, rowRight;
    if (!rowBounds(rows, y, rowLeft, rowRight))
      continue;
    left = std::min(left, rowLeft);
    right = std::max(right, rowRight);
    top = std::min(top, y);
    bottom = std::max(bottom, y);
  }
  return right >= left;
}

void findSideAnchor(const uint32_t rows[32], bool useRight, int preferredY,
                    int &anchorX, int &anchorY) {
  int bestDistance = 33;
  anchorX = useRight ? 31 : 0;
  anchorY = preferredY;
  for (int y = 0; y < 32; ++y) {
    int left, right;
    if (!rowBounds(rows, y, left, right))
      continue;
    const int distance = abs(y - preferredY);
    if (distance < bestDistance) {
      bestDistance = distance;
      anchorX = useRight ? right : left;
      anchorY = y;
    }
  }
}

void animateEggRows(uint32_t rows[32], uint8_t family, uint8_t frame) {
  int left, right, top, bottom;
  if (!maskBounds(rows, left, right, top, bottom))
    return;
  // A tiny hatchling eye peeks through a different shell pixel each pose.
  // It is cleared from the shell mask so the room underneath stays visible.
  static const int peekX[4] = {-2, -1, 1, 2};
  const uint8_t phase = (frame >> 1) & 3U;
  clear32(rows, (left + right) / 2 + peekX[(phase + family) & 3U], top + 6);
}

void buildAgeRows(uint8_t species, uint8_t stage, uint8_t evolutionPath,
                  uint8_t frame, uint32_t rows[32]) {
  memset(rows, 0, sizeof(uint32_t) * 32);
  if (stage == 0) {
    buildEggRows(species, rows);
    animateEggRows(rows, species, frame);
    return;
  }
  if (stage < 4) {
    buildGrowthRows(species % TAMACHIBI_FAMILY_COUNT, stage, evolutionPath,
                    rows);
    return;
  }
  const uint8_t id = species % 50;
  for (int y = 0; y < 32; ++y)
    rows[y] = pgm_read_dword(&ROSTER_SPRITES[id][y]);
}

int floorScale(int source, int outputSize) {
  const int value = source * outputSize;
  return value >= 0 ? value / 32 : -((-value + 31) / 32);
}

void drawPartPixel(GameDisplay &d, int x, int y, int outputSize, int bob,
                   int sourceX, int sourceY, bool facingLeft) {
  const int mirroredX = facingLeft ? 31 - sourceX : sourceX;
  const int x0 = floorScale(mirroredX, outputSize);
  const int x1 = -floorScale(-mirroredX - 1, outputSize);
  const int y0 = floorScale(sourceY, outputSize);
  const int y1 = -floorScale(-sourceY - 1, outputSize);
  d.fillRect(x + x0, y + y0 - bob, std::max(1, x1 - x0),
             std::max(1, y1 - y0), 1);
}

void drawPartLine(GameDisplay &d, int x, int y, int outputSize, int bob,
                  int x0, int y0, int x1, int y1, bool facingLeft) {
  const int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  const int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int error = dx + dy;
  while (true) {
    drawPartPixel(d, x, y, outputSize, bob, x0, y0, facingLeft);
    if (x0 == x1 && y0 == y1)
      break;
    const int twice = 2 * error;
    if (twice >= dy) { error += dy; x0 += sx; }
    if (twice <= dx) { error += dx; y0 += sy; }
  }
}

void drawPartSquare(GameDisplay &d, int x, int y, int outputSize, int bob,
                    int sourceX, int sourceY, int size, bool facingLeft) {
  for (int py = 0; py < size; ++py)
    for (int px = 0; px < size; ++px)
      drawPartPixel(d, x, y, outputSize, bob, sourceX + px, sourceY + py,
                    facingLeft);
}

void findTopAnchor(const uint32_t rows[32], int top, int centerX, int &x) {
  int left, right;
  rowBounds(rows, top, left, right);
  x = std::max(left, std::min(right, centerX));
  while (x <= right && !(rows[top] & (0x80000000UL >> x)))
    ++x;
  if (x > right) {
    x = std::max(left, std::min(right, centerX));
    while (x >= left && !(rows[top] & (0x80000000UL >> x)))
      --x;
  }
}

void findBottomAnchor(const uint32_t rows[32], int bottom, int centerX, int &x) {
  int left, right;
  rowBounds(rows, bottom, left, right);
  x = std::max(left, std::min(right, centerX));
  while (x <= right && !(rows[bottom] & (0x80000000UL >> x)))
    ++x;
  if (x > right) {
    x = std::max(left, std::min(right, centerX));
    while (x >= left && !(rows[bottom] & (0x80000000UL >> x)))
      --x;
  }
}

void drawAnimatedPart(GameDisplay &d, int x, int y, int outputSize, int bob,
                      const uint32_t rows[32], uint8_t species,
                      uint8_t stage, uint8_t frame, bool facingLeft) {
  if (stage == 0)
    return;
  const uint8_t family = stage < 4
                             ? species % TAMACHIBI_FAMILY_COUNT
                             : growthFamilyForAdult(species % 50);
  const uint8_t phase = (frame >> 1) & 3U;
  int left, right, top, bottom;
  if (!maskBounds(rows, left, right, top, bottom))
    return;
  const int centerX = (left + right) / 2;
  const int preferredY = top + (bottom - top) * 2 / 3;
  int anchorX, anchorY;
  static const int swing[4] = {-3, -1, 3, 1};

  switch (family) {
  case 0: { // Skyling wing: a broad fan flaps through four angles.
    findSideAnchor(rows, false, preferredY, anchorX, anchorY);
    const int tipY = anchorY + swing[phase];
    drawPartLine(d, x, y, outputSize, bob, anchorX, anchorY,
                 anchorX - 6, tipY, facingLeft);
    drawPartLine(d, x, y, outputSize, bob, anchorX - 1, anchorY - 1,
                 anchorX - 4, tipY - 2, facingLeft);
    drawPartLine(d, x, y, outputSize, bob, anchorX - 1, anchorY + 1,
                 anchorX - 4, tipY + 2, facingLeft);
    break;
  }
  case 1: { // Tidekin fin: two rays sweep behind the swimmer.
    findSideAnchor(rows, true, preferredY, anchorX, anchorY);
    const int tipY = anchorY + swing[phase] / 2;
    drawPartLine(d, x, y, outputSize, bob, anchorX, anchorY,
                 anchorX + 6, tipY, facingLeft);
    drawPartLine(d, x, y, outputSize, bob, anchorX + 2, anchorY - 1,
                 anchorX + 6, tipY - 3, facingLeft);
    drawPartLine(d, x, y, outputSize, bob, anchorX + 2, anchorY + 1,
                 anchorX + 6, tipY + 3, facingLeft);
    break;
  }
  case 2: case 5: { // Pouncer/Hornling tail: a long curl whips up and down.
    findSideAnchor(rows, family == 2, preferredY, anchorX, anchorY);
    const int direction = family == 2 ? 1 : -1;
    const int tipY = anchorY + swing[phase];
    drawPartLine(d, x, y, outputSize, bob, anchorX, anchorY,
                 anchorX + direction * 3, anchorY + 1, facingLeft);
    drawPartLine(d, x, y, outputSize, bob, anchorX + direction * 3,
                 anchorY + 1, anchorX + direction * 5, tipY, facingLeft);
    drawPartLine(d, x, y, outputSize, bob, anchorX + direction * 5, tipY,
                 anchorX + direction * 7, tipY - 2, facingLeft);
    drawPartLine(d, x, y, outputSize, bob, anchorX + direction * 6,
                 tipY - 2, anchorX + direction * 7, tipY - 2, facingLeft);
    break;
  }
  case 3: { // Shadelet wisp: the lower hem trails in a loose S curve.
    findBottomAnchor(rows, bottom, centerX, anchorX);
    const int sway = swing[phase];
    drawPartLine(d, x, y, outputSize, bob, anchorX, bottom,
                 anchorX + sway / 2, bottom + 2, facingLeft);
    drawPartLine(d, x, y, outputSize, bob, anchorX + sway / 2,
                 bottom + 2, anchorX + sway, bottom + 5, facingLeft);
    drawPartLine(d, x, y, outputSize, bob, anchorX + sway,
                 bottom + 5, anchorX + sway + 2, bottom + 6, facingLeft);
    break;
  }
  case 4: { // Crumbkin bun: sesame seeds bounce independently above it.
    int topAnchor;
    findTopAnchor(rows, top, centerX, topAnchor);
    const int hop = (phase == 0 || phase == 3) ? 1 : 3;
    drawPartSquare(d, x, y, outputSize, bob, topAnchor - 5, top - hop, 2,
                   facingLeft);
    drawPartSquare(d, x, y, outputSize, bob, topAnchor - 1, top - (4 - hop), 2,
                   facingLeft);
    drawPartSquare(d, x, y, outputSize, bob, topAnchor + 3, top - hop, 2,
                   facingLeft);
    break;
  }
  case 6: { // Pupkin ears: floppy tips move separately on both sides.
    int topAnchor;
    findTopAnchor(rows, top, centerX, topAnchor);
    const int leftTipY = top - (phase == 0 ? 6 : phase == 1 ? 3 : phase == 2 ? 2 : 4);
    const int rightTipY = top - (phase == 2 ? 6 : phase == 3 ? 3 : phase == 0 ? 2 : 4);
    drawPartLine(d, x, y, outputSize, bob, topAnchor - 2, top,
                 topAnchor - 6, leftTipY, facingLeft);
    drawPartLine(d, x, y, outputSize, bob, topAnchor - 6, leftTipY,
                 topAnchor - 8, leftTipY, facingLeft);
    drawPartLine(d, x, y, outputSize, bob, topAnchor + 2, top,
                 topAnchor + 6, rightTipY, facingLeft);
    drawPartLine(d, x, y, outputSize, bob, topAnchor + 6, rightTipY,
                 topAnchor + 8, rightTipY, facingLeft);
    break;
  }
  case 7: { // Sproutling leaf: a stem bends and the leaf swaps sides.
    int topAnchor;
    findTopAnchor(rows, top, centerX, topAnchor);
    const int direction = (phase == 0 || phase == 3) ? -1 : 1;
    drawPartLine(d, x, y, outputSize, bob, topAnchor, top,
                 topAnchor, top - 5, facingLeft);
    drawPartLine(d, x, y, outputSize, bob, topAnchor, top - 4,
                 topAnchor + direction * 4, top - 6, facingLeft);
    drawPartLine(d, x, y, outputSize, bob, topAnchor + direction * 4,
                 top - 6, topAnchor + direction * 5, top - 4, facingLeft);
    drawPartLine(d, x, y, outputSize, bob, topAnchor + direction * 5,
                 top - 4, topAnchor, top - 4, facingLeft);
    break;
  }
  case 8: { // Tinker clapper: the bell's hanging striker swings.
    findBottomAnchor(rows, bottom, centerX, anchorX);
    static const int strikerOffset[4] = {-3, -5, 3, 5};
    const int tipX = anchorX + strikerOffset[phase];
    const int tipY = bottom + ((phase & 1U) ? 5 : 3);
    drawPartLine(d, x, y, outputSize, bob, anchorX, bottom,
                 anchorX + strikerOffset[phase] / 2, bottom + 2, facingLeft);
    drawPartLine(d, x, y, outputSize, bob,
                 anchorX + strikerOffset[phase] / 2, bottom + 2,
                 tipX, tipY, facingLeft);
    drawPartSquare(d, x, y, outputSize, bob, tipX - 1, tipY, 2, facingLeft);
    break;
  }
  default: { // Gearling antenna: the mast stays still as its signal pulses.
    int topAnchor;
    findTopAnchor(rows, top, centerX, topAnchor);
    drawPartLine(d, x, y, outputSize, bob, topAnchor, top,
                 topAnchor, top - 5, facingLeft);
    const int direction = (phase & 1U) ? 1 : -1;
    drawPartLine(d, x, y, outputSize, bob, topAnchor, top - 5,
                 topAnchor + direction * 3, top - 5, facingLeft);
    drawPartLine(d, x, y, outputSize, bob, topAnchor + direction * 3,
                 top - 5, topAnchor + direction * 4, top - 4, facingLeft);
    if (phase & 1U)
      drawPartSquare(d, x, y, outputSize, bob, topAnchor + direction * 4,
                     top - 6, 2, facingLeft);
    break;
  }
  }
}
} // namespace

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
         uint8_t frame, bool asleep, int scale, uint8_t evolutionPath,
         bool facingLeft) {
  uint32_t rows[32];
  buildAgeRows(species, stage, evolutionPath, frame, rows);
  const int outputSize = 16 * std::max(1, scale);
  const int bob = ((frame & 1U) != 0) ? std::max(1, outputSize / 32) : 0;

  // The old callers use scale 1 for a 16px icon and scale 2 for a 32px
  // character. Larger preview scales keep the same pixel-art proportions.
  for (int yy = 0; yy < 32; ++yy) {
    const uint32_t row = rows[yy];
    if (!row)
      continue;
    const int y0 = (yy * outputSize) / 32 - bob;
    const int y1 = ((yy + 1) * outputSize + 31) / 32 - bob;
    for (int xx = 0; xx < 32; ++xx) {
      const int sourceX = facingLeft ? 31 - xx : xx;
      if (!(row & (0x80000000UL >> sourceX)))
        continue;
      const int x0 = (xx * outputSize) / 32;
      const int x1 = ((xx + 1) * outputSize + 31) / 32;
      d.fillRect(x + x0, y + y0, std::max(1, x1 - x0),
                 std::max(1, y1 - y0), 1);
    }
  }

  // Appendages are layered as separate transparent pixels so they can flap,
  // wag, sway, or swing without translating the creature's whole body.
  if (!asleep && stage > 0)
    drawAnimatedPart(d, x, y, outputSize, bob, rows, species, stage, frame,
                     facingLeft);

  if (asleep && stage > 0) {
    const int eyeY = y + outputSize / 2 - bob;
    const int eyeW = std::max(1, outputSize / 12);
    d.drawFastHLine(x + outputSize * 3 / 10, eyeY, eyeW, 0);
    d.drawFastHLine(x + outputSize * 6 / 10, eyeY, eyeW, 0);
  }

  if (stage == 5 && outputSize >= 32) {
    const int unit = outputSize / 32;
    // Leave the cane just outside the creature silhouette so it remains
    // readable at the device's 32px render size as well as in larger previews.
    const int caneX = facingLeft ? x - unit : x + outputSize + unit;
    d.drawLine(caneX, y + 20 * unit, caneX, y + 31 * unit - bob, 1);
    d.drawCircle(caneX, y + 19 * unit, 2 * unit, 1);
    d.drawFastHLine(caneX - 2 * unit, y + 31 * unit - bob, 5 * unit, 1);
  }
}
void accessory(GameDisplay &d, int x, int y, uint8_t outfit, int scale) {
  if (!outfit)
    return;
  const int u = std::max(1, scale);
  switch ((outfit - 1) % 4) {
  case 0:
    d.drawFastHLine(x + 3 * u, y + u, 11 * u, 1);
    d.fillRect(x + 5 * u, y - u, 7 * u, 3 * u, 1);
    break;
  case 1:
    d.fillTriangle(x + 9 * u, y + 2 * u, x + 13 * u, y + 5 * u,
                   x + 13 * u, y - u, 1);
    d.fillTriangle(x + 9 * u, y + 2 * u, x + 6 * u, y + 5 * u,
                   x + 6 * u, y - u, 1);
    break;
  case 2:
    d.drawFastHLine(x + 4 * u, y + 11 * u, 8 * u, 0);
    d.drawFastVLine(x + 11 * u, y + 11 * u, 4 * u, 0);
    break;
  case 3:
    d.drawRect(x + 3 * u, y + 6 * u, 4 * u, 4 * u, 0);
    d.drawRect(x + 9 * u, y + 6 * u, 4 * u, 4 * u, 0);
    d.drawFastHLine(x + 7 * u, y + 7 * u, 2 * u, 0);
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
