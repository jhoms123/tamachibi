PICO TAMACHIBI v47 — RP2040 PROMICRO EDITION
================================================

OPEN THIS SKETCH
----------------
Pico_Tamachibi_v47_RP2040_ProMicro.ino

Keep every .ino, .cpp and .h file in this folder. Do not copy only the .ino.

ARDUINO IDE
-----------
Board package:
  Raspberry Pi Pico/RP2040 by Earle F. Philhower, III

Board selection:
  Tools > Board > Raspberry Pi RP2040 Boards > Generic RP2040

Recommended settings:
  CPU Speed: 133 MHz
  Flash Size: choose the actual flash capacity printed on your board
              (for example 4 MB or 16 MB) and reserve at least 64 KB for FS
  Optimize: Small or Optimize Even More (-O3)
  USB Stack: Pico SDK

Required libraries:
  Adafruit SSD1306
  Adafruit GFX Library
  Adafruit BusIO

UPLOAD MODE
-----------
Normally, select the detected port and click Upload. If it is not detected:

1. Hold BOOT.
2. Tap RESET once.
3. Release BOOT.
4. A USB drive should appear.
5. Upload from Arduino IDE.

Turn the battery switch OFF while programming through USB-C.

BOARD-SPECIFIC CHANGES
----------------------
- GP17 is reserved for the onboard RGB LED and is held LOW by the game.
- GP22 is kept free for future PCM5102A DIN audio wiring.
- The disabled optional battery-monitor gate moved from GP22 to GP29.
- All display, button, buzzer and potentiometer GPIOs match the supplied wiring.

ACTIVE v47 WIRING
-----------------
Main 128x64 OLED:
  SDA GP4, SCL GP5, VCC 3V3, GND GND

Side 128x32 OLED:
  SDA GP6, SCL GP7, VCC 3V3, GND GND

Buttons, each wired from GPIO to GND:
  Left GP10, Select GP11, Right GP12

Passive piezo:
  Positive GP15, negative GND

B10K potentiometer:
  Outer legs 3V3 and GND, center/wiper GP26

RESERVED FOR FUTURE MUSIC BUILD
-------------------------------
The wiring may remain connected, but v47 does not yet read the SD card or play
PCM5102A audio:

  SD: GP0 MISO, GP1 CS, GP2 SCK, GP3 MOSI
  PCM5102A: GP20 BCLK, GP21 LRCLK, GP22 DIN

The current M_MUSIC menu is the original Tamachibi music activity, not an MP3
player. An MP3-capable source version must be merged separately.

RED LED BEHAVIOR
----------------
The board arrived with a factory RGB blink program. After this game uploads and
the board restarts, that old blink program is replaced. The game reserves GP17
and holds it LOW.

If the red LED keeps repeating the exact factory blink after upload, the game
firmware was probably not installed. If it flashes irregularly while the board
repeatedly disconnects, check for a power short or unstable 5V supply.

FIRST POWER TEST
----------------
1. Disconnect peripherals.
2. Power only 5V and GND through the switch.
3. Confirm the board stays cool.
4. Upload the game over USB with battery power switched off.
5. Add the main OLED, then buttons, dial, side OLED and buzzer one at a time.

SAVE STORAGE
------------
The game uses 4096 bytes of emulated EEPROM and LittleFS snapshots. Select a
Flash Size option with at least 64 KB of filesystem space.
