# ESP32-U wiring and microSD music setup

This guide assumes a classic ESP32-WROOM-32U development breakout. GPIO numbers below are chip GPIO numbers, not header positions. Confirm the exact pin labels and board schematic before connecting power.

## Reference GPIO map

| Device / board label | Signal | ESP32 GPIO |
|---|---|---:|
| Main 128×64 SSD1306 | SDA / SCL | 21 / 22 |
| Side 128×32 SSD1306 | SDA / SCL | 16 / 17 |
| Left / select / right buttons | switch to GND | 32 / 33 / 25 |
| Rotary encoder module | CLK / DT | 35 / 39 |
| Passive piezo | positive | 26 |
| HW-261 D13 / SCK | VS1053 and TF-card SPI clock | 18 |
| HW-261 D12 / MISO | VS1053 and TF-card SPI MISO | 19 |
| HW-261 D11 / MOSI | VS1053 and TF-card SPI MOSI | 23 |
| HW-261 D9 / CS | TF-card chip select | 27 |
| HW-261 D8 / X_RESET | VS1053 reset | 4 |
| HW-261 D7 / X_DCS | VS1053 audio-data select | 13 |
| HW-261 D6 / X_CS | VS1053 control select | 14 |
| HW-261 D2 / DREQ | VS1053 data request | 34 |

These are individual wires between the shield and ESP32; do not stack the UNO shield onto the ESP32. The D-number labels are Arduino Uno header positions, not ESP32 GPIO numbers. The two OLEDs can both use address `0x3C` because they are on separate I2C controllers. Each button connects between its GPIO and GND; the firmware uses `INPUT_PULLUP`.

Power the encoder module's `+` pin from **3V3**, connect `GND` to common ground, and connect `CLK` and `DT` as listed above. Leave `SW` unconnected; the player does not use the encoder's push switch. GPIO35 and GPIO39 have no internal pull-ups, so use the module's onboard pull-ups or add one 10 kΩ resistor from each signal (`CLK`, `DT`) to 3V3. If clockwise raises volume in the wrong direction, swap the `CLK` and `DT` wires. Never put 5V on an ESP32 GPIO.

## HW-261 VS1053 shield

The shield's built-in TF slot and VS1053 decoder share the SPI bus, so this build uses the shield's slot instead of a separate microSD adapter and uses the VS1053 instead of a PCM5102A DAC. Wire the shield's Arduino header labels to the ESP32 GPIOs in the table. The control wires are `D9/CS`, `D8/X_RESET`, `D7/X_DCS`, `D6/X_CS`, and `D2/DREQ`.

Power the shield's pin marked `5V` from a regulated 5V rail and connect its `GND` to ESP32 ground. The board is specified for a 5V supply and has onboard regulators. Do not connect its 5V supply pin to any ESP32 GPIO. DREQ comes from the VS1053's 3.3V logic; connect it to GPIO34 only, never to the shield's `5V` or `VIN` pin.

Connect headphones to the shield's headphone/audio-output jack. Its stereo headphone output is suitable for headphones; it is not a power amp for a passive speaker. Use a separate speaker amplifier for a passive speaker. Do not connect a PCM5102A DAC to the same project audio path.

## LiPo and separate charging board

The firmware does not identify your charger board, so use the board's printed labels and schematic to determine its topology before wiring. A normal one-cell LiPo is nominally 3.7V and charges to 4.2V. Do not connect a raw cell directly to the ESP32 board's 5V/VIN input or 3V3 rail.

- If the board has **BAT+ / BAT−** and a clearly identified regulated **5V OUT+ / 5V OUT−** from a boost converter, connect the one-cell battery only to BAT+ / BAT−. Route regulated 5V OUT+ through the power switch to both the ESP32 breakout's documented 5V/VIN input and the HW-261 shield's `5V` pin. Connect OUT− to ESP32 GND, shield GND, and the shared ground rail.
- If it is a **charger only** (for example, a TP4056-style board whose output follows the cell voltage), it is not a 5V supply. It needs a suitable 5V boost converter between the battery/protected output and the ESP32 board's 5V/VIN input and shield's `5V` pin. Use a charger/power-path arrangement rated for the load if you expect the device to run while charging. Many basic charger boards are not load-sharing chargers; with those, charge with the device switched off.
- If the board labels differ, are unclear, or there is no protected battery output/undervoltage cutoff, do not connect it based on this generic description. The charger model or a clear front-and-back photo is needed to map its pads safely.

Keep all grounds common: ESP32, displays, HW-261 shield, boost output, and charger output ground. Put the switch in the **boosted 5V positive lead**, not in the LiPo negative wire. Do not feed USB 5V and boost 5V into the board at the same time unless its schematic documents power-path isolation. For USB programming, switch the battery boost off first.

Use a 5V boost supply with enough current headroom for Wi-Fi/ESP32 transients plus the audio circuitry. Use a protected cell/charger matched to a single-cell 4.2V LiPo. Check polarity with a meter before connecting the ESP32. This guide cannot confirm your separate charger/boost board without its model or markings.

## Format and load the microSD card

1. Use a microSD card formatted as FAT32. An 8–32 GB card is a practical starting point; avoid exFAT for this build.
2. Create a folder named `music` in the card's root directory.
3. Copy MP3 files directly into that folder. The player scans only this folder's top level, not subfolders.
4. Use filenames that sort in playback order, for example:

   ```text
   /music/01_theme.mp3
   /music/02_walk.mp3
   /music/03_night.mp3
   ```

5. Insert the card into the HW-261's TF slot, power the device, and select **Play → Music Player**. The screen shows an SD error if it cannot mount, or a prompt to create `/music` if the folder is missing. D0/D1/D2 control tracks and playback; turn the encoder to change volume. In the main pet screen, turning it cycles Pet → House → City; turning it in the other direction cycles back.

Use ordinary, non-DRM MP3 files. For predictable results, start with stereo or mono 44.1 kHz MP3s at a moderate bitrate such as 128–192 kb/s. No audio files are bundled; copy music you have the right to use.

## First power-up order

1. Upload and test the ESP32 with USB only and no peripherals.
2. Add the main OLED, then the second OLED, buttons, and rotary encoder.
3. Wire the HW-261 with power disconnected. First test its 5V supply and polarity with a multimeter; then insert a FAT32 card and connect headphones.
4. Test music playback and the encoder before assembling the battery wiring.
5. Test the charger/boost output disconnected from the ESP32 and shield; confirm its voltage and polarity with a multimeter.
6. Connect the battery and boost output only after confirming the charger/boost labels, 5V output, common ground, and switch placement.
