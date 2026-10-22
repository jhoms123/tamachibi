#include "NeedsDisplay.h"

namespace {
constexpr int16_t SCREEN_W = 128;
constexpr int16_t SCREEN_H = 32;
constexpr uint32_t REFRESH_MS = 90;

void printTwoDigits(Adafruit_SSD1306 &display, uint8_t value) {
  if (value < 10) display.print('0');
  display.print(value);
}

void printTime12(Adafruit_SSD1306 &display, uint8_t hour24, uint8_t minute) {
  const bool pm = hour24 >= 12;
  uint8_t hour12 = static_cast<uint8_t>(hour24 % 12);
  if (hour12 == 0) hour12 = 12;
  display.print(hour12);
  display.print(':');
  printTwoDigits(display, minute);
  display.print(pm ? 'P' : 'A');
}
} // namespace

NeedsDisplay::NeedsDisplay(TwoWire *wire)
    : oled_(SCREEN_W, SCREEN_H, wire, -1), ready_(false), lastDrawMs_(0) {}

bool NeedsDisplay::begin(uint8_t address) {
  ready_ = oled_.begin(SSD1306_SWITCHCAPVCC, address, false, false);
  if (!ready_) return false;
  oled_.ssd1306_command(SSD1306_NORMALDISPLAY);
  oled_.ssd1306_command(SSD1306_DISPLAYON);
  oled_.ssd1306_command(SSD1306_SETCONTRAST);
  oled_.ssd1306_command(0x6F);
  oled_.clearDisplay();
  oled_.setTextColor(SSD1306_WHITE);
  oled_.setTextSize(1);
  oled_.setTextWrap(false);
  oled_.display();
  return true;
}

void NeedsDisplay::showStartup() {
  if (!ready_) return;
  oled_.clearDisplay();
  oled_.setTextSize(1);
  oled_.setTextColor(SSD1306_WHITE);
  oled_.setCursor(22, 1);
  oled_.print(F("TAMACHIBI STATUS"));
  oled_.drawLine(4, 11, 123, 11, SSD1306_WHITE);
  oled_.setCursor(25, 16);
  oled_.print(F("Starting v47..."));
  oled_.setCursor(13, 25);
  oled_.print(F("Set clock on main OLED"));
  oled_.display();
}

uint8_t NeedsDisplay::urgencyFor(uint8_t value, bool critical) {
  if (critical || value <= 2) return 2;
  if (value <= 4) return 1;
  return 0;
}

bool NeedsDisplay::blinkVisible(uint8_t urgency, uint32_t nowMs) {
  if (urgency == 0) return true;
  const uint32_t period = urgency >= 2 ? 180UL : 680UL;
  return ((nowMs / period) & 1UL) == 0;
}

void NeedsDisplay::drawNeedField(int16_t x, int16_t y, const char *label,
                                 uint8_t value, uint8_t urgency,
                                 uint32_t nowMs, uint8_t maximum) {
  if (!blinkVisible(urgency, nowMs)) return;

  oled_.setCursor(x, y);
  oled_.print(label);
  oled_.setCursor(x + 14, y);
  if (value < 10) oled_.print(' ');
  oled_.print(value);

  const int16_t barX = x + 29;
  const int16_t barY = y + 1;
  const int16_t barW = 31;
  oled_.drawRect(barX, barY, barW, 6, SSD1306_WHITE);
  uint8_t bounded = value > maximum ? maximum : value;
  int16_t fillW = maximum == 0 ? 0 : static_cast<int16_t>((bounded * (barW - 2)) / maximum);
  if (fillW > 0) oled_.fillRect(barX + 1, barY + 1, fillW, 4, SSD1306_WHITE);

  if (urgency >= 2) {
    oled_.drawPixel(x + 25, y + 1, SSD1306_WHITE);
    oled_.drawPixel(x + 25, y + 3, SSD1306_WHITE);
    oled_.drawPixel(x + 25, y + 6, SSD1306_WHITE);
  }
}

const char *NeedsDisplay::weekdayName(uint8_t weekday) {
  static const char *const names[7] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
  return names[weekday % 7];
}

const char *NeedsDisplay::locationName(uint8_t location) {
  switch (location % 8) {
    case 1: return "KITCHEN";
    case 2: return "SCHOOL";
    case 3: return "WORK";
    case 4: return "PARK";
    case 5: return "GARDEN";
    case 6: return "MARKET";
    case 7: return "SLEEP";
    default: return "HOME";
  }
}

void NeedsDisplay::drawTimeLine(const NeedsDisplayState &state, const char *location) {
  oled_.drawLine(0, 23, 127, 23, SSD1306_WHITE);
  oled_.setCursor(0, 24);
  oled_.print(location);
  oled_.setCursor(54, 24);
  oled_.print(weekdayName(state.weekday));
  oled_.setCursor(78, 24);
  printTime12(oled_, state.hour, state.minute);
}

void NeedsDisplay::drawLive(const NeedsDisplayState &state, uint32_t nowMs) {
  const uint8_t foodUrgency = urgencyFor(state.food);
  const uint8_t funUrgency = urgencyFor(state.fun);
  const uint8_t energyUrgency = urgencyFor(state.energy);
  const uint8_t cleanUrgency = urgencyFor(state.clean, state.poop >= 2);
  const uint8_t healthUrgency = urgencyFor(state.health, state.sick || state.conditionSeverity >= 2);
  const uint8_t poopUrgency = state.poop >= 2 ? 2 : (state.poop > 0 ? 1 : 0);

  drawNeedField(0, 0, "FD", state.food, foodUrgency, nowMs);
  drawNeedField(64, 0, "FN", state.fun, funUrgency, nowMs);
  drawNeedField(0, 8, "EN", state.energy, energyUrgency, nowMs);
  drawNeedField(64, 8, "CL", state.clean, cleanUrgency, nowMs);
  drawNeedField(0, 16, "HP", state.health, healthUrgency, nowMs);
  drawNeedField(64, 16, "PP", state.poop, poopUrgency, nowMs, 3);
  drawTimeLine(state, locationName(state.location));
}

void NeedsDisplay::drawAway(const NeedsDisplayState &state, const char *title,
                            const char *detail, uint8_t resumeHour) {
  oled_.setTextSize(1);
  int16_t titleX = static_cast<int16_t>((128 - static_cast<int16_t>(strlen(title)) * 6) / 2);
  if (titleX < 0) titleX = 0;
  oled_.setCursor(titleX, 0);
  oled_.print(title);
  oled_.drawLine(5, 9, 122, 9, SSD1306_WHITE);

  int16_t detailX = static_cast<int16_t>((128 - static_cast<int16_t>(strlen(detail)) * 6) / 2);
  if (detailX < 0) detailX = 0;
  oled_.setCursor(detailX, 12);
  oled_.print(detail);

  oled_.setCursor(0, 23);
  if (state.mode == NEEDS_MODE_SITTER) {
    oled_.print(F("SITTER "));
    oled_.print(state.sitterHours);
    oled_.print(F("h left"));
  } else {
    oled_.print(state.mode == NEEDS_MODE_SLEEP ? F("WAKE ") : F("BACK "));
    uint8_t hour12 = static_cast<uint8_t>(resumeHour % 12);
    if (hour12 == 0) hour12 = 12;
    oled_.print(hour12);
    oled_.print(F(":00 "));
    oled_.print(resumeHour >= 12 ? F("PM") : F("AM"));
  }

  oled_.setCursor(78, 23);
  printTime12(oled_, state.hour, state.minute);
}

void NeedsDisplay::update(const NeedsDisplayState &state, uint32_t nowMs, bool force) {
  if (!ready_) return;
  if (!force && nowMs - lastDrawMs_ < REFRESH_MS) return;
  lastDrawMs_ = nowMs;

  oled_.clearDisplay();
  oled_.setTextColor(SSD1306_WHITE);
  oled_.setTextSize(1);
  oled_.setTextWrap(false);

  switch (state.mode) {
    case NEEDS_MODE_SCHOOL:
      drawAway(state, "AT SCHOOL", "CARE COVERED", state.returnHour);
      break;
    case NEEDS_MODE_WORK:
      drawAway(state, "AT WORK", "CARE COVERED", state.returnHour);
      break;
    case NEEDS_MODE_SLEEP:
      drawAway(state, "ZZZ  SLEEPING  ZZZ", "ENERGY RECOVERING", state.wakeHour);
      break;
    case NEEDS_MODE_SITTER:
      drawAway(state, "WITH SITTER", "NEEDS ARE COVERED", 0);
      break;
    case NEEDS_MODE_EGG:
      oled_.setCursor(46, 2);
      oled_.print(F("EGG"));
      oled_.drawRoundRect(54, 11, 20, 13, 7, SSD1306_WHITE);
      oled_.setCursor(17, 25);
      oled_.print(F("Growing quietly..."));
      break;
    case NEEDS_MODE_DEAD:
      oled_.setCursor(30, 3);
      oled_.print(F("MEMORY STAR"));
      oled_.drawLine(24, 13, 103, 13, SSD1306_WHITE);
      oled_.setCursor(19, 20);
      oled_.print(F("Open main screen"));
      break;
    default:
      drawLive(state, nowMs);
      break;
  }

  oled_.display();
}
