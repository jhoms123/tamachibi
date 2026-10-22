# Tamachibi v47 — ESP32-WROOM-32U breakout port

This folder contains the complete v47 Tamachibi game logic, ported from the RP2040 ProMicro build to the Arduino ESP32 core. It keeps the two SSD1306 displays, buttons, rotary encoder, piezo, save/profile system, life simulation, and the original Music rhythm game. It adds a separate **Music Player** action that scans `/music` on the HW-261 VS1053 shield's TF card and plays MP3 files through the shield's decoder and headphone output.

## Hardware assumption

The pin map is for a **classic ESP32-WROOM-32U on a development breakout** with exposed GPIO labels. The WROOM-32U is an ESP32 module, not a complete USB development board by itself. If you have a bare module, a WROVER/PSRAM board, an ESP32-S2/S3/C3 board, or a carrier with different labels, stop and use that board's schematic before wiring. GPIO16/17 are reserved for the second display in this map and can conflict with PSRAM on some different ESP32 modules.

## Build

### PlatformIO

Open this folder as a PlatformIO project and build the `esp32u` environment. `platformio.ini` selects the generic `esp32dev` target used by the classic WROOM-32U breakout and installs the display and audio libraries.

### Arduino IDE

1. Install the Espressif **esp32** board package.
2. Install `Adafruit SSD1306`, `Adafruit GFX Library`, and `Adafruit BusIO` from Library Manager.
3. Install `Adafruit VS1053 Library` from Library Manager.
4. Open `ESP32_Tamachibi_v47_ESP32U.ino` in this folder and select **ESP32 Dev Module** (or the exact WROOM-32U carrier board if listed).
5. Select the flash size printed for your board and a partition scheme with at least a 2 MB app slot. The included PlatformIO config uses `min_spiffs.csv`, which keeps OTA support and provides more app space than the default scheme. v47 stores saves in ESP32 EEPROM emulation and does not need a filesystem partition.
6. Upload over USB with the battery boost output switched off. Keep all `.cpp`, `.h`, and `.ino` files together.

The source includes both Arduino-ESP32 LEDC API variants (core 2.x and 3.x). A current Espressif board package is recommended.

## Controls

In the Play menu, choose **Music Player**. The new player is independent from the original rhythm mini-game:

- D0: previous track
- D1: start or stop the selected MP3
- D2: next track; hold D2 for about one second to exit
- Rotary encoder: cycles Pet / House / City in the main screen; controls music volume in Music Player

The player reads up to 32 `.mp3` files from the top level of `/music` and sorts filenames alphabetically. It does not scan nested folders or playlists. The HW-261 shield handles MP3 decoding and provides a stereo headphone output, so this configuration does not need a separate microSD adapter or PCM5102A DAC. Use a separate amplifier for passive speakers.

## Hardware and card setup

See [ESP32U_WIRING_AND_MUSIC.md](ESP32U_WIRING_AND_MUSIC.md) for the reference pin table, SD card folder layout, audio path, and LiPo/charger conditions. The charger board model is needed before its exact battery and power connections can be confirmed.
