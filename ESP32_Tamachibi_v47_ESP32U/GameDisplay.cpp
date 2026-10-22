#include "GameDisplay.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace {
void copyText(char *dst, const char *src, size_t cap) {
  if (!cap)
    return;
  if (!src)
    src = "";
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = 0;
}
// Abbreviate common stat labels before considering an ellipsis.
void compactScalarLabel(char *s) {
  const char *words[][2] = {{"confidence", "conf."},
                            {"Attachment", "Attach."},
                            {"conversations", "talks"},
                            {"Generation", "Gen."},
                            {"Temperature", "Temp."}};
  for (const auto &word : words) {
    char *at = strstr(s, word[0]);
    if (!at)
      continue;
    size_t a = strlen(word[0]), b = strlen(word[1]);
    memmove(at + b, at + a, strlen(at + a) + 1);
    memcpy(at, word[1], b);
  }
}
// Short button verbs keep all three actions within their own 42-pixel lane.
void buttonText(char *dst, const char *s, size_t cap) {
  if (!s || !*s) {
    copyText(dst, "", cap);
    return;
  }
  if (s[0] == 'D' && s[1] >= '0' && s[1] <= '2')
    s += 2;
  while (*s == ' ')
    ++s;
  copyText(dst, s, cap);
}
} // namespace
GameDisplay::GameDisplay(Adafruit_SH1107 &panel)
    : Adafruit_GFX(128, 128), panel_(panel), requestedNative_(false),
      frameNative_(false), primitive_(false), gameCanvas_(false), runCount_(0),
      selectedY_(-100), pageFingerprint_(0), pageStartedMs_(0) {
  memset(art_, 0, sizeof(art_));
  title_[0] = context_[0] = 0;
  memset(footer_, 0, sizeof(footer_));
  memset(gameButtons_, 0, sizeof(gameButtons_));
}
bool GameDisplay::begin(uint8_t address, bool reset) {
  return panel_.begin(address, reset);
}
void GameDisplay::clearDisplay() {
  frameNative_ = requestedNative_;
  panel_.clearDisplay();
  memset(art_, 0, sizeof(art_));
  runCount_ = 0;
  selectedY_ = -100;
  title_[0] = context_[0] = 0;
  memset(footer_, 0, sizeof(footer_));
  setCursor(0, 0);
  setTextSize(1);
  setTextWrap(false);
  setTextColor(SH110X_WHITE);
}
void GameDisplay::drawPixel(int16_t x, int16_t y, uint16_t ink) {
  if (frameNative_) {
    panel_.drawPixel(x, y, ink);
    return;
  }
  if (x < 0 || x >= 128 || y < 0 || y >= 64)
    return;
  uint8_t &b = art_[y * 16 + (x >> 3)];
  const uint8_t mask = 0x80 >> (x & 7);
  if (ink == SH110X_WHITE)
    b |= mask;
  else if (ink == SH110X_BLACK)
    b &= ~mask;
  else
    b ^= mask;
}
void GameDisplay::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) {
  if (!frameNative_ && !gameCanvas_ && !primitive_ && w > 100 &&
      (y < 12 || y > 51))
    return;
  for (int16_t i = 0; i < w; ++i)
    drawPixel(x + i, y, c);
}
void GameDisplay::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t c) {
  for (int16_t i = 0; i < h; ++i)
    drawPixel(x, y + i, c);
}
bool GameDisplay::container(int16_t, int16_t y, int16_t w, int16_t h,
                            uint16_t color, bool fill) {
  if (frameNative_ || gameCanvas_ || primitive_ || w < 60)
    return false;
  if (fill && color == SH110X_WHITE && h <= 16 && y >= 9 && y < 54)
    selectedY_ = y + h / 2;
  return true;
}
void GameDisplay::fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                           uint16_t c) {
  if (container(x, y, w, h, c, true))
    return;
  for (int16_t i = 0; i < h; ++i)
    drawFastHLine(x, y + i, w, c);
}
void GameDisplay::fillScreen(uint16_t c) {
  if (frameNative_)
    panel_.fillScreen(c);
  else
    memset(art_, c ? 255 : 0, sizeof(art_));
}
void GameDisplay::drawRect(int16_t x, int16_t y, int16_t w, int16_t h,
                           uint16_t c) {
  if (container(x, y, w, h, c, false))
    return;
  bool old = primitive_;
  primitive_ = true;
  Adafruit_GFX::drawRect(x, y, w, h, c);
  primitive_ = old;
}
void GameDisplay::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                                int16_t r, uint16_t c) {
  if (container(x, y, w, h, c, false))
    return;
  bool old = primitive_;
  primitive_ = true;
  Adafruit_GFX::drawRoundRect(x, y, w, h, r, c);
  primitive_ = old;
}
void GameDisplay::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                                int16_t r, uint16_t c) {
  if (container(x, y, w, h, c, true))
    return;
  bool old = primitive_;
  primitive_ = true;
  Adafruit_GFX::fillRoundRect(x, y, w, h, r, c);
  primitive_ = old;
}
size_t GameDisplay::write(uint8_t c) {
  if (frameNative_)
    return Adafruit_GFX::write(c);
  if (c == '\r')
    return 1;
  if (c == '\n') {
    cursor_x = 0;
    cursor_y += 8;
    return 1;
  }
  if (c < 32 || c > 126)
    c = '?';
  Run *r = nullptr;
  if (runCount_) {
    Run &last = runs_[runCount_ - 1];
    if (last.y == cursor_y && last.x + last.len * 6 == cursor_x)
      r = &last;
  }
  if (!r && runCount_ < MAX_RUNS) {
    r = &runs_[runCount_++];
    r->x = cursor_x;
    r->y = cursor_y;
    r->len = 0;
    r->selected = false;
    r->text[0] = 0;
  }
  if (r && r->len < RUN_CHARS - 1) {
    r->text[r->len++] = c;
    r->text[r->len] = 0;
  }
  cursor_x += 6;
  return 1;
}
void GameDisplay::setPageTitle(const char *s, const char *context) {
  copyText(title_, s, sizeof(title_));
  copyText(context_, context, sizeof(context_));
}
void GameDisplay::setGameButtons(const char *a, const char *b, const char *c) {
  copyText(gameButtons_[0], a, sizeof(gameButtons_[0]));
  copyText(gameButtons_[1], b, sizeof(gameButtons_[1]));
  copyText(gameButtons_[2], c, sizeof(gameButtons_[2]));
}
void GameDisplay::setPageFooter(const char *a, const char *b, const char *c) {
  buttonText(footer_[0], a, sizeof(footer_[0]));
  buttonText(footer_[1], b, sizeof(footer_[1]));
  buttonText(footer_[2], c, sizeof(footer_[2]));
}
void GameDisplay::physicalText(const char *s, int x, int y, int width,
                               uint16_t ink) {
  const int count = width / 6;
  if (count <= 0)
    return;
  char buf[24];
  const int n = std::min(count, 23);
  copyText(buf, s, n + 1);
  if (static_cast<int>(strlen(s)) > n && n >= 3) {
    buf[n - 3] = '.';
    buf[n - 2] = '.';
    buf[n - 1] = '.';
  }
  panel_.setTextSize(1);
  panel_.setTextWrap(false);
  panel_.setTextColor(ink);
  panel_.setCursor(x, y);
  panel_.print(buf);
}
void GameDisplay::compose() {
  panel_.clearDisplay();
  // Sort text geometrically, then reconstruct complete readable rows. Artwork
  // and text never share a region; inverse selections become native list rows.
  uint8_t order[MAX_RUNS];
  for (uint8_t i = 0; i < runCount_; ++i)
    order[i] = i;
  for (uint8_t i = 1; i < runCount_; ++i) {
    uint8_t k = order[i], j = i;
    while (j && (runs_[order[j - 1]].y > runs_[k].y ||
                 (runs_[order[j - 1]].y == runs_[k].y &&
                  runs_[order[j - 1]].x > runs_[k].x))) {
      order[j] = order[j - 1];
      --j;
    }
    order[j] = k;
  }
  struct Line {
    char text[32];
    char value[12];
    bool selected;
  };
  Line lines[48]{};
  uint8_t lineCount = 0;
  char row[256]{};
  int rowY = -1000;
  auto appendRow = [&]() {
    if (!row[0])
      return;
    bool selected = abs(rowY - selectedY_) <= 7;
    // Scalar values get a dedicated right column rather than wrapping alone.
    char *tail = strrchr(row, ' ');
    if (tail && tail[1] &&
        (isdigit(static_cast<unsigned char>(tail[1])) ||
         ((tail[1] == '+' || tail[1] == '-') &&
          isdigit(static_cast<unsigned char>(tail[2])))) &&
        strlen(tail + 1) <= 8 && lineCount < 48) {
      copyText(lines[lineCount].value, tail + 1,
               sizeof(lines[lineCount].value));
      *tail = 0;
      copyText(lines[lineCount].text, row, sizeof(lines[lineCount].text));
      compactScalarLabel(lines[lineCount].text);
      lines[lineCount++].selected = selected;
      row[0] = 0;
      return;
    }
    const char *s = row;
    while (*s && lineCount < 48) {
      while (*s == ' ')
        ++s;
      if (!*s)
        break;
      int n = std::min(19, static_cast<int>(strlen(s)));
      if (s[n] && n == 19) {
        int cut = n;
        while (cut > 0 && s[cut] != ' ')
          --cut;
        if (cut > 0)
          n = cut;
      }
      memcpy(lines[lineCount].text, s, n);
      lines[lineCount].text[n] = 0;
      lines[lineCount++].selected = selected;
      s += n;
    }
    row[0] = 0;
  };
  for (uint8_t i = 0; i < runCount_; ++i) {
    const Run &r = runs_[order[i]];
    if (!r.len)
      continue;
    if (r.y < 10) {
      if (!title_[0])
        copyText(title_, r.text, sizeof(title_));
      else if (!context_[0] && r.x >= 80)
        copyText(context_, r.text, sizeof(context_));
      continue;
    }
    if (r.y >= 55) {
      // Preserve a custom single-line instruction when no shared footer exists.
      if (!footer_[0][0] && !footer_[1][0] && !footer_[2][0]) {
        if (strstr(r.text, "D0/D2")) {
          setPageFooter("PREV", "SELECT", "NEXT");
        } else if (strstr(r.text, "D0") || strstr(r.text, "D1") ||
                   strstr(r.text, "D2")) {
          for (int b = 0; b < 3; ++b) {
            char token[3] = {'D', static_cast<char>('0' + b), 0};
            const char *p = strstr(r.text, token);
            if (!p)
              continue;
            p += 2;
            while (*p == ' ')
              ++p;
            int len = 0;
            while (p[len] && p[len] != ' ' && len < 6) {
              footer_[b][len] = toupper(static_cast<unsigned char>(p[len]));
              ++len;
            }
            footer_[b][len] = 0;
          }
        } else
          copyText(footer_[1], r.text, sizeof(footer_[1]));
      }
      continue;
    }
    if (rowY != r.y) {
      appendRow();
      rowY = r.y;
    }
    size_t used = strlen(row);
    if (used && used < sizeof(row) - 3) {
      strcat(row, "  ");
      used += 2;
    }
    strncat(row, r.text, sizeof(row) - used - 1);
  }
  appendRow();
  // Title is constrained separately from its counter, so both always fit.
  if (!title_[0])
    copyText(title_, "TAMACHIBI", sizeof(title_));
  if (context_[0]) {
    physicalText(title_, 5, 5, 85);
    physicalText(context_, 94, 5, 30);
  } else
    physicalText(title_, 5, 5, 118);
  panel_.drawFastHLine(4, 16, 120, SH110X_WHITE);
  int minX = 128, minY = 64, maxX = -1, maxY = -1, pixels = 0;
  for (int y = 0; y < 64; ++y)
    for (int x = 0; x < 128; ++x)
      if (art_[y * 16 + (x >> 3)] & (0x80 >> (x & 7))) {
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
        ++pixels;
      }
  int textY = 25, available = 9, step = 10;
  if ((pixels > 18 || (gameCanvas_ && pixels > 0)) &&
      (gameCanvas_ || lineCount <= 6)) {
    const int sceneH = gameCanvas_ ? 64 : (lineCount <= 3 ? 60 : 24);
    const int w = maxX - minX + 1, h = maxY - minY + 1;
    // Uniform fit, never an independent horizontal/vertical stretch.
    float scale =
        std::min(1.0f, std::min(128.0f / w, static_cast<float>(sceneH) / h));
    int sw = std::max(1, static_cast<int>(w * scale)),
        sh = std::max(1, static_cast<int>(h * scale));
    int ox = (128 - sw) / 2, oy = 22 + (sceneH - sh) / 2;
    for (int sy = minY; sy <= maxY; ++sy)
      for (int sx = minX; sx <= maxX; ++sx) {
        if (art_[sy * 16 + (sx >> 3)] & (0x80 >> (sx & 7)))
          panel_.drawPixel(ox + static_cast<int>((sx - minX) * scale),
                           oy + static_cast<int>((sy - minY) * scale),
                           SH110X_WHITE);
      }
    textY = 22 + sceneH + 6;
    step = 9;
  }
  if (pixels <= 18 && !gameCanvas_ && lineCount <= 3) {
    // A quiet document emblem gives plain utility pages the same visual rhythm.
    panel_.drawRoundRect(55, 28, 18, 20, 2, SH110X_WHITE);
    for (int y = 33; y <= 41; y += 4)
      panel_.drawFastHLine(59, y, 10, SH110X_WHITE);
    textY = 63;
    step = 16;
  } else if (pixels <= 18 && !gameCanvas_)
    step = std::max(
        10, std::min(16, (97 - textY) /
                             std::max(1, static_cast<int>(lineCount) - 1)));
  available = std::max(1, (106 - textY) / step + 1);
  if (lineCount > available)
    available = std::max(1, (97 - textY) / step + 1);
  int pages = (lineCount + available - 1) / available;
  // Dense status/journal pages remain readable through explicit page
  // indicators.
  uint32_t fingerprint = 2166136261UL;
  for (uint8_t i = 0; i < lineCount; ++i)
    for (const char *p = lines[i].text; *p; ++p)
      fingerprint = (fingerprint ^ static_cast<uint8_t>(*p)) * 16777619UL;
  if (fingerprint != pageFingerprint_) {
    pageFingerprint_ = fingerprint;
    pageStartedMs_ = millis();
  }
  int page = pages > 1 ? ((millis() - pageStartedMs_) / 5000UL) % pages : 0;
  if (pages > 1) {
    char n[24];
    snprintf(n, sizeof(n), "%d/%d", page + 1, pages);
    physicalText(n, 97, 106, 27);
  }
  for (int i = 0; i < available; ++i) {
    int n = page * available + i;
    if (n >= lineCount)
      break;
    int y = textY + i * step;
    if (lines[n].selected)
      panel_.fillRoundRect(3, y - 1, 122, 9, 2, SH110X_WHITE);
    uint16_t ink = lines[n].selected ? SH110X_BLACK : SH110X_WHITE;
    int valueWidth = strlen(lines[n].value) * 6;
    physicalText(lines[n].text, 7, y, 114 - (valueWidth ? valueWidth + 6 : 0),
                 ink);
    if (valueWidth)
      physicalText(lines[n].value, 121 - valueWidth, y, valueWidth, ink);
  }
  if (gameCanvas_) {
    for (int i = 0; i < 3; ++i)
      copyText(footer_[i], gameButtons_[i], sizeof(footer_[i]));
  }
  panel_.drawFastHLine(4, 114, 120, SH110X_WHITE);
  if (!footer_[0][0] && !footer_[2][0] && strlen(footer_[1]) > 6)
    physicalText(footer_[1], 5, 119, 118);
  else {
    if (!footer_[0][0] && !footer_[1][0] && !footer_[2][0])
      setPageFooter("PREV", "SELECT", "NEXT");
    for (int i = 0; i < 3; ++i) {
      int chars = std::min(6, static_cast<int>(strlen(footer_[i])));
      physicalText(footer_[i], i * 43 + (42 - chars * 6) / 2, 119, 40);
    }
  }
}
void GameDisplay::display() {
  if (!frameNative_)
    compose();
  panel_.display();
}
