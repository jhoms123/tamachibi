# ESP32-U wiring and microSD music setup

This guide assumes a classic ESP32-WROOM-32U development breakout. GPIO numbers below are chip GPIO numbers, not header positions. Confirm the exact pin labels and board schematic before connecting power.

## Reference GPIO map

| Device | Signal | ESP32 GPIO |
|---|---|---:|
| Main 128×64 SSD1306 | SDA / SCL | 21 / 22 |
| Side 128×32 SSD1306 | SDA / SCL | 16 / 17 |
| Left / select / right buttons | switch to GND | 32 / 33 / 25 |
| B10K dial | wiper | 34 (ADC1, input only) |
| Passive piezo | positive | 26 |
| SPI microSD | CS | 27 |
| SPI microSD | SCK / MISO / MOSI | 18 / 19 / 23 |
| PCM5102A I2S DAC | BCLK / LRCLK / DIN | 14 / 13 / 4 |

The two OLEDs can both use address `0x3C` because they are on separate I2C controllers. Each button connects between its GPIO and GND; the firmware uses `INPUT_PULLUP`. Connect the dial's outer legs to 3V3 and GND, and the center wiper to GPIO34. Never put 5V on an ESP32 GPIO.

## microSD adapter

Use a 3.3V-logic SPI microSD adapter. Connect:

| microSD adapter | ESP32 |
|---|---|
| CS / SS | GPIO27 |
| SCK / CLK | GPIO18 |
| MISO / DO | GPIO19 |
| MOSI / DI | GPIO23 |
| VCC | 3V3, unless the adapter's documentation requires a different input supply |
| GND | common GND |

The card signals must be 3.3V logic even if the adapter has a regulator or is marked for 5V input. Keep SPI wires short. A 10 µF capacitor and a 0.1 µF ceramic capacitor across adapter VCC and GND near the adapter help with card current spikes.

## I2S DAC and audio output

For a PCM5102A breakout, connect BCK/BCLK to GPIO14, LCK/LRCLK/WS to GPIO13, and DIN to GPIO4. Join DAC GND to ESP32 GND. Power the breakout using the voltage shown in its documentation: some boards accept 5V on VIN because they include a regulator, while bare or other breakouts need 3.3V. If the board requires SCK tied low for its internal PLL, follow that board's instructions. Leave module configuration pins at their documented default unless the board guide says otherwise.

The PCM5102A provides line-level left/right output. Connect L, R, and GND to powered speakers, an AUX input, or a stereo headphone amplifier. Do not connect regular headphones directly to the DAC line outputs. A small mono speaker amplifier is not a substitute for a stereo headphone amplifier.

## LiPo and separate charging board

The firmware does not identify your charger board, so use the board's printed labels and schematic to determine its topology before wiring. A normal one-cell LiPo is nominally 3.7V and charges to 4.2V. Do not connect a raw cell directly to the ESP32 board's 5V/VIN input or 3V3 rail.

- If the board has **BAT+ / BAT−** and a clearly identified regulated **5V OUT+ / 5V OUT−** from a boost converter, connect the one-cell battery only to BAT+ / BAT−. Route regulated 5V OUT+ through the power switch to the ESP32 breakout's documented 5V/VIN input. Connect OUT− to ESP32 GND and the shared ground rail.
- If it is a **charger only** (for example, a TP4056-style board whose output follows the cell voltage), it is not a 5V supply. It needs a suitable 5V boost converter between the battery/protected output and the ESP32 board's 5V/VIN input. Use a charger/power-path arrangement rated for the load if you expect the device to run while charging. Many basic charger boards are not load-sharing chargers; with those, charge with the device switched off.
- If the board labels differ, are unclear, or there is no protected battery output/undervoltage cutoff, do not connect it based on this generic description. The charger model or a clear front-and-back photo is needed to map its pads safely.

Keep all grounds common: ESP32, displays, SD adapter, DAC/amplifier, boost output, and charger output ground. Put the switch in the **boosted 5V positive lead**, not in the LiPo negative wire. Do not feed USB 5V and boost 5V into the board at the same time unless its schematic documents power-path isolation. For USB programming, switch the battery boost off first.

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

5. Insert the card, power the device, and select **Play → Music Player**. The screen shows an SD error if it cannot mount, or a prompt to create `/music` if the folder is missing. D0/D1/D2 control tracks and playback; the dial changes volume.

Use ordinary, non-DRM MP3 files. For predictable results, start with stereo or mono 44.1 kHz MP3s at a moderate bitrate such as 128–192 kb/s. No audio files are bundled; copy music you have the right to use.

## First power-up order

1. Upload and test the ESP32 with USB only and no peripherals.
2. Add the main OLED, then the second OLED, buttons, and dial.
3. Test the SD adapter with a FAT32 card.
4. Test the DAC into powered speakers/AUX, then add a headphone amplifier if needed.
5. Test the charger/boost output disconnected from the ESP32 and confirm its voltage and polarity with a multimeter.
6. Connect the battery and boost output only after confirming the charger/boost labels, output voltage, common ground, and switch placement.
