# Tamachibi v47 — ESP32-WROOM-32U breakout port

This folder contains the v47 game port for a classic ESP32-WROOM-32U development breakout. It uses a 128×128 SH1107 main OLED and the optional 128×32 SSD1306 status OLED, along with the buttons, rotary encoder, piezo, save/profile system, life simulation, and original Music rhythm game. The visual system uses native square layouts and separates supporting-page text from undistorted artwork. It also adds a Music Player that scans MP3 files from a separate SPI microSD adapter and sends decoded audio over I²S to a separate audio board.

The pictured blue module is used only as the microSD reader. The red HW-261/VS1053 shield is not part of this build. The I²S audio board can be chosen later; its exact power and analog/speaker wiring depend on that board.

## Hardware assumption

The pin map is for a **classic ESP32-WROOM-32U on a development breakout** with exposed GPIO labels. The WROOM-32U is a module, not a complete USB development board by itself. If you have a bare module, a WROVER/PSRAM board, an ESP32-S2/S3/C3 board, or a carrier with different labels, check that board's schematic before wiring. GPIO16/17 are reserved for the second display in this map and may conflict with PSRAM on some other ESP32 modules.

## Build

### PlatformIO

Open this folder as a PlatformIO project and build the `esp32u` environment. `platformio.ini` selects the generic `esp32dev` target used for a classic WROOM-32U breakout and installs the display and MP3/I²S libraries.

### Arduino IDE

1. Install the Espressif **esp32** board package.
2. Install `Adafruit SH110X`, `Adafruit SSD1306`, `Adafruit GFX Library`, and `Adafruit BusIO` from Library Manager.
3. Install `ESP8266Audio` by Earle Philhower for MP3 decoding and I²S output.
4. Open `ESP32_Tamachibi_v47_ESP32U.ino` in this folder and select **ESP32 Dev Module** (or the exact WROOM-32U carrier board if listed).
5. Select the flash size printed for your board and the **Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)** partition scheme used by this build. EEPROM stores the live state; LittleFS stores save-profile snapshots, so keep the filesystem partition.
6. Upload over USB with the battery boost output switched off. Keep all `.cpp`, `.h`, and `.ino` files together.

## Controls

In the Play menu, choose **Music Player**:

- D0: previous track
- D1: start or stop the selected MP3
- D2: next track; hold D2 for about one second to exit
- Rotary encoder: playback volume
- Encoder push switch: Right / D2
- On the main pet screen, the same encoder cycles Pet → House → City

The player reads up to 32 `.mp3` files from the top level of `/music` and sorts them by filename. It does not scan nested folders or playlists. The firmware decodes MP3 and outputs I²S on BCLK GPIO14, LRCLK GPIO13, and DATA GPIO4. There is no sound until a compatible I²S DAC or amplifier board is connected.

## Hardware and card setup

See [ESP32U_WIRING_AND_MUSIC.md](ESP32U_WIRING_AND_MUSIC.md) for the 128×128 OLED wiring, SD card formatting and file names, the future I²S audio connection, and LiPo/charger notes. The charger board model is needed to confirm its exact battery and power connections.


## Native square visual system

The main UI uses the physical 128×128 grid. Install/copy the whole sketch folder,
including **GameDisplay.cpp**, **GameDisplay.h**, **NativeUI.cpp**, **NativeUI.h**,
**GrowthSprites.cpp**, **GrowthSprites.h**, **RosterSprites.cpp**, **RosterSprites.h**,
**AquariumArt.h**, and **ConversationMemory.h**.
No SD image pack, font download, or new Arduino library is needed beyond the existing
Adafruit SH110X / GFX / BusIO and audio libraries.

- Home and resident rooms use a new top-down plan with compact original pet art.
- House, town, garden, aquarium, music, dialogue, activity grid and game selectors
  have native square layouts.
- Supporting status, story, shop, settings and utility pages are composed from
  separate artwork and text layers. The compositor wraps text, keeps numeric
  values in a right column, and gives the title and buttons fixed screen regions.
- Mini-game playfields retain their gameplay coordinates and square pixels. Their
  drawing commands are placed in a separate scene region with a uniform fit;
  text is never stretched or rendered over that region.
- Dense supporting pages show a page indicator and advance every five seconds;
  a changed page starts at its first section. Native dialogue has manual paging.
- Bottom labels correspond to left / select / right buttons. Hold right to exit
  where the existing activity supports it. The encoder still selects world views
  and controls music volume in the player.

Firmware-rendered screenshots and the design/validation notes are in
[`../docs/NATIVE_UI.md`](../docs/NATIVE_UI.md). Run the screenshot harness from the
repository root with `python3 tools/render_ui.py` after PlatformIO installs GFX.

## Character growth and movement

The original roster now has ten lineage families with their own egg emblems,
baby, child, and two teen forms. A care challenged teen has a smaller, squatter
silhouette and a broken mark; a thriving teen gets a family crest. When the pet
reaches adulthood, its lineage can branch into any of five distinct roster
creatures. The adult tier uses the care record and the pet's health, stats, and
bond. Every adult can continue to Elder, keeping its chosen silhouette and
adding a cane.

Teen form is set when the pet reaches that stage: three or more care mistakes,
or discipline at three or lower, selects the care challenged route. The adult
choice uses five care quality bands. The save structure stays the same; v19
saves are accepted and their old species choices are migrated to the new
lineages on load.

Pet sprites preserve transparent exterior space, while enclosed body interiors
are opaque black so scenery cannot show through outlined creatures. Babies are
authored at their native pixel size, and every adult branch retains its family's
recognizable crest, ears, fins, horns, leaves, handle, wisp, or antenna.
Awake idle cycles animate a lineage-specific part independently of the body's
position: wings flap, fins and tails swish, ears flop, leaves sway, wisps trail,
and bells or antennae swing. These poses change every 700 ms; the home pet also
walks, turns, flips, and bobs every 350 ms. Walking pauses while the display is
asleep, the pet is away, or a menu is open. The branch chart plus live rendered
walking and part-animation previews can be regenerated from the repository root
with:

```sh
python3 tools/render_evolution.py
```

The evolution approach follows the care based branching pattern in the
[1996 Tamagotchi character list](https://tamagotchi.fandom.com/wiki/Tamagotchi_(1996_Pet)/Character_list#Generation_1_2).
All Tamachibi sprite designs in this project are original.

[View the evolution lineup](../docs/evolution/Tamachibi_Evolution_Tree.png),
[walking preview](../docs/evolution/Tamachibi_Walking_Preview.gif), and
[animated family features](../docs/evolution/Tamachibi_Part_Animation.gif).

## Aquarium life

The Aquarium activity unlocks at Child. It now has twelve original fish with
distinct silhouettes and independently animated tails, fins, gills, and eyes.
Each fish keeps a generated temperament (bold, shy, curious, or sociable), a
swimming quirk, a favorite food, and a preferred tank decoration. These traits
change its route, how it reacts to greetings and meals, and where it spends
time when its favorite decoration is installed.

Use **Watch Tank** to follow fish and greet one at a time, **Fish Profile** to
inspect needs and traits, **Fish Stories** to revisit its preferences and
memories, and **Play Bubbles** for a short timing game. Feeding, cleaning,
treatment, decoration, and play target the selected fish while still letting
the rest of the school react. Fish remember their first meal, a favorite food,
recovering from illness, and the decoration that feels most like home. The
story reply also reflects current care, friendship, temperament, and past
events, with a large rotating set of combinations.

Fish grow through fry, young, juvenile, and adult forms. Their adult markings
branch according to health, mood, bond, and tank care at the time they mature.
Daily care and a full tank can also lead to a fry joining the family. Neglect
can make fish unwell, but health never falls below one. The aquarium continues
using its separate dual-slot save; the 92-byte record size and following
EEPROM addresses stay fixed so existing fish migrate without resetting the
main pet save.

## Conversation and memory

Talk, Parent Dialogue, Growing Together, and the Parent Bond Journal now use a
shared view of the pet's existing saved history. A conversation chooses a
relevant detail for its subject: care moments include their day and time;
favorite questions use learned food preferences; play and work questions use
actual game or career records; and garden, companion, family, aquarium, and
mood conversations use matching current state. Direct Talk replies no longer
stack a second unrelated memory and several generic relationship lines onto
the answer.

Open **Talk → Parent Bond Journal → page 7** to review care, food, play, trip,
shared-place, and current-feeling memories. Page 8 changes how often the pet
initiates check-ins. The conversation layer reads existing save fields and
does not add or move any EEPROM data.

## Everyday care

Meals react to hunger, the Tama's learned likes and dislikes, and foods served
recently. The selector marks a favorite, a food they have grown tired of, a
frequent repeat, or a new option before it is chosen. Cleaning gives its full
health and trust benefits when the Tama actually needs a bath; an already-clean
pet still gets a small shared moment without turning baths into a stat-grinding
shortcut. Medicine replies reflect whether the remedy matched and whether the
Tama recovered.

The Love action now offers **Cuddle**, **Praise**, or **Quiet company**. Each
choice affects mood, bond, trust, discipline, or kindness differently, with
personality affinities and smaller repeat rewards when the same gesture is used
over and over. Care history records the selected meal, medicine, and comfort
choice, so later conversations can refer to what actually happened. These
details use the existing eight-entry care history and preserve the save layout.
