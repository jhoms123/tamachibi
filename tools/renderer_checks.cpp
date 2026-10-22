#include "NativeUI.h"
#include <cassert>
#include <cstdio>
uint32_t fixtureMillis = 100;
int main() {
  Adafruit_SH1107 panel;
  GameDisplay d(panel);
  d.useGameCanvas(true);
  d.setGameButtons("LEFT", "ACTION", "RIGHT");
  // Full source playfield, including both corner pixels, is preserved at 1:1.
  d.clearDisplay();
  d.drawPixel(0, 0, 1);
  d.drawPixel(127, 63, 1);
  d.display();
  assert(panel.pixels[22 * 128] == 1);
  assert(panel.pixels[85 * 128 + 127] == 1);
  // A circle keeps the same width and height. Text cannot change its scene
  // pixels.
  auto circle = [&](bool text) {
    d.clearDisplay();
    d.drawCircle(64, 32, 10, 1);
    if (text) {
      d.setCursor(0, 20);
      d.print("THIS TEXT IS SEPARATE");
    }
    d.display();
  };
  circle(false);
  uint8_t before[64 * 128];
  memcpy(before, panel.pixels + 22 * 128, sizeof(before));
  int x0 = 128, x1 = -1, y0 = 128, y1 = -1;
  for (int y = 22; y < 86; ++y)
    for (int x = 0; x < 128; ++x)
      if (panel.pixels[y * 128 + x]) {
        x0 = min(x0, x);
        x1 = max(x1, x);
        y0 = min(y0, y);
        y1 = max(y1, y);
      }
  assert(x1 - x0 == 20 && y1 - y0 == 20);
  circle(true);
  assert(memcmp(before, panel.pixels + 22 * 128, sizeof(before)) == 0);
  // Native scenes remain intact when a function returns before its caller
  // flushes.
  d.useGameCanvas(false);
  {
    NativeUI::Scope scope(d);
    d.clearDisplay();
    d.drawPixel(90, 90, 1);
  }
  d.display();
  assert(panel.pixels[90 * 128 + 90]);
  // The following supporting page starts fresh in the collector.
  d.clearDisplay();
  d.drawPixel(20, 20, 1);
  assert(panel.pixels[20 * 128 + 20] == 0);
  puts("Renderer checks passed: square geometry, bottom edge, text/art "
       "isolation, scoped frame lifetime.");
}
