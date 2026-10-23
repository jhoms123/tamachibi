# ESP32-U wiring and microSD music setup

This map assumes a classic ESP32-WROOM-32U development breakout. GPIO numbers are chip GPIO numbers, not header positions. Confirm the labels on your ESP32 carrier board before connecting wires.

## Main wiring

| Part | Pin or signal | ESP32 connection |
|---|---|---:|
| Main 128×128 SH1107 OLED | SDA / SCL | GPIO21 / GPIO22 |
| Side 128×32 SSD1306 OLED | SDA / SCL | GPIO16 / GPIO17 |
| Optional left button (D0) | one side of switch | GPIO32 |
| Existing button (Select / D1) | one side of switch | GPIO33 |
| Optional left / existing select buttons | other side of each switch | GND |
| Passive piezo | positive | GPIO26 |
| Passive piezo | negative | GND |
| Rotary encoder module | CLK | GPIO35 |
| Rotary encoder module | DT | VN (GPIO39) |
| Rotary encoder module | SW (push switch) | GPIO25 (Right / D2) |
| Rotary encoder module | `+` | 3V3 |
| Rotary encoder module | GND | common GND |
| Separate SPI microSD adapter | CS | GPIO27 |
| Separate SPI microSD adapter | SCK | GPIO18 |
| Separate SPI microSD adapter | MOSI | GPIO23 |
| Separate SPI microSD adapter | MISO | GPIO19 |
| I²S audio board, when selected | BCLK / LRCLK / DATA | GPIO14 / GPIO13 / GPIO4 |

Wire the main screen's four pins as follows: **GND → GND, VCC → 3V3, SCL → GPIO22, SDA → GPIO21**. Keep the panel on 3.3V logic. The firmware uses Adafruit's open-source SH110X driver for the 128×128 SH1107 panel, and detects its address at `0x3C` or `0x3D`. The small status OLED remains on I²C pins GPIO16/GPIO17 and uses the SSD1306 driver. Both OLEDs may use address `0x3C` because they are on separate I²C buses. For your two-control setup, wire the existing button between GPIO33 and GND for Select/D1, and the encoder SW between GPIO25 and GND for Right/D2. GPIO32 is the optional Left/D0 button. The firmware enables pull-ups for each switch input and debounces the encoder push switch.

The supplied photo confirms the main module as **MC01506, Driver: SH1107, Dots: 128×128**. The printed 0x78 / 0x7A address options are the 8-bit forms of the 7-bit 0x3C / 0x3D addresses used by the firmware. Leave the address jumper unchanged; the firmware checks both.

The pictured black control is a **rotary encoder**, not an analog potentiometer. On your ESP32 board, GPIO39 is printed as `VN`; connect the encoder's DT wire to the header pin labeled `VN`. In the main game the encoder cycles Pet / House / City; on the Music Player screen it changes volume. Pressing the encoder shaft acts as the Right/D2 button on GPIO25, including the hold-to-exit action. GPIO35 and GPIO39 are input-only and have no internal pull-ups. Connect the encoder's `+` to 3V3 and GND to common GND, then add a 10 kΩ resistor from **CLK/GPIO35 to 3V3** and another 10 kΩ resistor from **DT/VN/GPIO39 to 3V3**. The photo does not show pull-up resistors on the visible side of this encoder module, so add these if the dial jumps, moves by itself, or responds inconsistently. Never power the encoder from 5V or feed 5V to an ESP32 GPIO. If turning it clockwise lowers the volume or cycles views backward, swap CLK and DT.

For the push switch, connect **SW to GPIO25** and keep the encoder module's **GND connected to ESP32 GND**. GPIO25 uses its internal pull-up, so the switch should pull it to ground only while pressed. To check the switch wiring, disconnect SW from GPIO25 and briefly touch a jumper from GPIO25 to GND; the firmware should register Right/D2. If that works, the issue is in the encoder's SW/GND wiring or the module's push switch.

## Separate microSD adapter shown in your photo

Connect the module's SPI pins as follows:

| Adapter label | ESP32 connection |
|---|---|
| CS | GPIO27 |
| SCK | GPIO18 |
| MOSI | GPIO23 |
| MISO | GPIO19 |
| GND | common GND |
| VCC | regulated 5V |

The photo matches the common six-pin module with an `1117` regulator and a 16-pin level-conversion IC. That board version is specified for 4.5–5.5V at VCC; its onboard regulator supplies the card at 3.3V and its MISO conversion outputs 3.3V logic. https://keszoox.com/products/micro-sd-tf-card-storage-module-with-spi-interface-and-level-conversion-chip-for-arduino The AMS1117 regulator itself may need up to 1.3V headroom, so powering its VCC from only 3.3V can underpower the card. https://www.advanced-monolithic.com/pdf/ds1117.pdf Do not connect 5V to CS, SCK, MOSI, or MISO. If your module has a different revision or its labels/chip differ from the photo, check its own specifications before powering it. Keep SPI wires short.

## Future I²S audio board

The code decodes MP3 from the SD card and sends I²S to BCLK GPIO14, LRCLK GPIO13, and DATA GPIO4. Connect those three signals and GND when you choose the audio board. Its VCC, any MCLK/SCK requirement, and speaker/headphone connections depend on the exact module. A PCM5102A is a line-output DAC and normally needs powered speakers or an amplifier; a board such as MAX98357A includes a mono speaker amplifier. Do not connect a speaker directly to the ESP32 GPIOs or to line-output pins.

There will be no sound until a compatible I²S DAC/amplifier is connected. The red HW-261/VS1053 shield from the earlier photo is not used by this wiring or firmware.

## LiPo battery and separate charger

The exact charger/boost board has not been identified, so its pads cannot be assigned safely from this guide alone. A one-cell LiPo is nominally 3.7V and charges to 4.2V; do not connect a raw cell directly to the ESP32 5V/VIN input or 3V3 rail.

- If the board has `BAT+ / BAT−` and a documented regulated `5V OUT+ / OUT−`, connect the cell only to the battery pads. Route the regulated 5V output through a switch to the ESP32 carrier's documented 5V/VIN input. The microSD adapter's VCC may use this same regulated 5V rail if its 5V input is confirmed.
- If it is a charger-only board whose output follows battery voltage, add a suitable 5V boost converter between its protected battery output and the ESP32 5V/VIN input. Basic charger boards may not support running the device while charging; turn the device off while charging unless the board explicitly provides power-path/load sharing.
- Join grounds between the ESP32, OLEDs, SD adapter, future audio board, and regulated power supply. Do not connect USB 5V and boost 5V at the same time unless the ESP32 carrier documents power-path isolation.

Measure the power board's output voltage and polarity before connecting it to the ESP32. A front-and-back photo or exact model number of the charger/boost board is needed to give its battery wiring pin by pin.

## Format and load the microSD card

1. Format the card as FAT32. A small 8–32 GB card is a practical starting point; avoid exFAT for this firmware.
2. Create a folder named `music` in the card's root directory.
3. Copy MP3 files directly into that folder. The player scans this folder's top level only, not nested folders.
4. Name files in the order you want, for example:

   ```text
   /music/01_theme.mp3
   /music/02_walk.mp3
   /music/03_night.mp3
   ```

5. Insert the card and choose **Play → Music Player**. The screen reports an SD error if the card cannot mount or asks for `/music` if the folder is missing. D0/D1/D2 select and play tracks; turning the encoder changes volume.

Use ordinary, non-DRM MP3 files. For predictable testing, start with 44.1 kHz mono or stereo MP3s at a moderate bitrate such as 128–192 kb/s. The player indexes up to 32 tracks. The code does not include music files.

## First power-up sequence

1. Upload and test the ESP32 over USB with peripherals disconnected.
2. Add the displays, buttons, piezo, and rotary encoder.
3. Add the microSD adapter and test that the card mounts and `/music` is found.
4. When you pick an audio board, connect its I²S signals and test it with powered speakers or the board's specified load.
5. Test the charger's regulated output with a multimeter while it is disconnected from the ESP32; only then connect the battery supply.
