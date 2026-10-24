# Tamachibi v47 — ESP32-WROOM-32U breakout port

This folder contains the v47 game port for a classic ESP32-WROOM-32U development breakout. It uses a 128×128 SH1107 main OLED and the optional 128×32 SSD1306 status OLED, along with the buttons, rotary encoder, piezo, save/profile system, life simulation, and original Music rhythm game. The visual system uses native square layouts and separates supporting-page text from undistorted artwork. It also adds a Music Player that scans MP3 files from a separate SPI microSD adapter and plays decoded audio through a wired I²S board or Bluetooth A2DP headphones.

The pictured blue module is used only as the microSD reader. The red HW-261/VS1053 shield is not part of this build. Bluetooth headphone playback uses the ESP32's built-in Classic Bluetooth radio. Wired output still needs a compatible I²S DAC/amplifier; its exact power and analog/speaker wiring depend on that board.

## Hardware assumption

The pin map is for a **classic ESP32-WROOM-32U on a development breakout** with exposed GPIO labels. The WROOM-32U is a module, not a complete USB development board by itself. If you have a bare module, a WROVER/PSRAM board, an ESP32-S2/S3/C3 board, or a carrier with different labels, check that board's schematic before wiring. GPIO16/17 are reserved for the second display in this map and may conflict with PSRAM on some other ESP32 modules.

## Build

### PlatformIO

Open this folder as a PlatformIO project and build the `esp32u` environment. `platformio.ini` selects the generic `esp32dev` target used for a classic WROOM-32U breakout and installs the display, MP3/I²S, and Bluetooth A2DP libraries.

### Arduino IDE

1. Install the Espressif **esp32** board package.
2. Install `Adafruit SH110X`, `Adafruit SSD1306`, `Adafruit GFX Library`, and `Adafruit BusIO` from Library Manager.
3. Install `ESP8266Audio` by Earle Philhower for MP3 decoding and wired I²S output.
4. Install `ESP32-A2DP` by Phil Schatzmann for Bluetooth headphone output.
5. Open `ESP32_Tamachibi_v47_ESP32U.ino` in this folder and select **ESP32 Dev Module** (or the exact WROOM-32U carrier board if listed).
6. Select the flash size printed for your board and the **Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)** partition scheme used by this build. EEPROM stores the live state; LittleFS stores save-profile snapshots, so keep the filesystem partition.
7. Upload over USB with the battery boost output switched off. Keep all `.cpp`, `.h`, and `.ino` files together.

## Controls

In the Play menu, choose **Music Player**:

- D0: previous track
- D1: play or pause/resume; hold about one second for options
- D2: next track; hold D2 for about one second to exit
- Rotary encoder: playback volume
- Encoder push switch: Right / D2
- On the main pet screen, the same encoder cycles Pet → House → City

The player reads up to 128 `.mp3` files beneath `/music`, including subfolders up to eight levels deep, and sorts them by full path. The options menu offers song/folder browsing, shuffle, repeat off/all/one, stop, Exit Player, and a separate Bluetooth menu. To pair headphones the first time, put them in pairing mode and open **BLUETOOTH MENU**; the screen reports STARTING, SEARCHING, CONNECTING, and CONNECTED as those states occur and shows a discovered device name. Choose **PAIR / SEARCH** to retry the saved connection if it has disconnected; a scan already in progress continues automatically. The saved device is used for reconnection on later playback. Bluetooth works without an audio board. Wired output uses I²S BCLK GPIO14, LRCLK GPIO13, and DATA GPIO4 and needs a compatible DAC/amplifier.

## Hardware and card setup

See [ESP32U_WIRING_AND_MUSIC.md](ESP32U_WIRING_AND_MUSIC.md) for the 128×128 OLED wiring, SD card formatting and file names, Bluetooth headphone pairing, wired I²S audio connections, and LiPo/charger notes. The charger board model is needed to confirm its exact battery and power connections.


## Native square visual system

The main UI uses the physical 128×128 grid. Install/copy the whole sketch folder,
including **GameDisplay.cpp**, **GameDisplay.h**, **NativeUI.cpp**, **NativeUI.h**,
**GrowthSprites.cpp**, **GrowthSprites.h**, **RosterSprites.cpp**, **RosterSprites.h**,
**AquariumArt.h**, and **ConversationMemory.h**.
No SD image pack or font download is needed. This port uses the existing Adafruit
SH110X / GFX / BusIO libraries, plus ESP8266Audio and ESP32-A2DP for audio.

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

## Garden and Plant Room

The Garden is the food-growing space: its twelve crop sprites progress from seed
to harvest, and harvests add ingredients to the cooking pantry. The Plant Room
is a florist collection: its ten potted ornament species have distinct animated
art, can be sold for coins, and can fill a changing daily customer order. Orders
raise florist rank, style skill, town reputation, and a scrapbook memory. Their
home scenes and activity screens are drawn separately, and both systems keep the
existing `BotanySave` record layout.

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

## Town and neighbors

Each of the eight residents has a distinct favorite way to spend time, preferred
gift, outing, conversation topic, and temperament. Those preferences affect
friendship gains and are revealed through repeat visits. Talk topics and gifts are
chosen directly; playing together is a short three-round reaction game; and cafe,
park, and neighborhood outings use a walk-together animation. Completing a
resident's three friendship chapters earns a personal invitation to their favorite
place. Adult pets can also date a close friend, with the existing partner and family
systems tracking that relationship.

Residents keep one saved shared-moment memory apiece, so a repeated activity gets a
different response from a first visit. Their story page shows chapter progress,
visit count, and invitations. The new three-byte neighbor-memory field uses padding
in the existing 32-byte city save record; version 2 saves remain loadable and keep
their relations and resident choices.

## Connected gameplay

Successful activities now share completion events across skills, goals, personal
promises and memories. Cancelling an activity does not earn progress. House and
City shortcuts follow the same stage, schedule and Sitter rules as the main menu;
Sitter pauses plants and fish as well as the Tama. Talk includes a direct Neighbors
entry, and family readiness grows from actual town relationships and shared life.
Status page 8 shows the individual pet's natural talent. Paradise has Applied
Research projects that improve crops, ornamentals, careers and community projects.

See [`../docs/GAMEPLAY_OVERHAUL.md`](../docs/GAMEPLAY_OVERHAUL.md) for the complete
resource connections and balance rules. Run `python3 tools/check_gameplay.py` from
the repository root for gameplay regression checks.

### Music player update

Put MP3 files in `/music` or its subfolders (up to eight nested levels).
The player indexes up to 128 songs, sorted by full path. Folder names can
represent albums or playlists. Choose Folder opens a list of folders containing songs, plus All Folders;
a folder selection includes the songs directly in that folder. Browse Songs
lets you start any indexed song. Left/Right navigate these lists; Select opens
the highlighted entry. Each list includes Back.

Left/right change tracks; Select pauses/resumes without restarting the song.
The player shows the estimated time left beside the playback state. A `~` marks
an estimate based on the first MP3 frame; MP3 files with Xing or VBRI frame
counts get a more accurate duration.
Turn the encoder for volume. Hold Select for the options menu, use Left/Right
to select an option, then Select to activate it. Options include shuffle,
repeat off/all/one, song and folder browsers, stop, Exit Player, and Bluetooth
Menu on the second options page. Opening Bluetooth Menu starts scanning. Choose
**PAIR / SEARCH** to retry the connection, **DISCONNECT** to disconnect, or **BACK TO OPTIONS**
to return. Leave headphones in pairing mode until the status reads CONNECTED.
Tamachibi connects to the first compatible audio device it finds and remembers
that device for later reconnection. Changing the output stops the current song;
press Select to start it again on the selected output. Holding Right also exits
directly. Exit stops the music, disconnects connected headphones, and restores
the normal care display.

The care screen scrolls long filenames. The main screen shows the current pet
wearing headphones in a room, animated notes during playback, output status, and
volume. The ESP32 resamples MP3 audio to the 44.1 kHz stereo PCM format used by
the Bluetooth A2DP output. The encoder controls both decoded audio gain and the
Bluetooth headset's reported volume. The music screen shows PAUSED explicitly
and displays the time remaining while a track plays.

Shuffle builds a queue so every song plays once per pass, and Previous retraces
that queue. Repeat Off stops at the end of the queue; Repeat All wraps it;
Repeat One replays the current song. Manual Previous/Next can wrap regardless
of repeat mode. Changing tracks while paused leaves playback stopped. Changing
folders stops playback; press Select to play the new selection. Playback options
and volume stay in memory during this power session. Scanning ignores hidden
files and reports when the bounded library/depth limit is reached. External
`.m3u` playlists, seeking and ID3 metadata are not implemented; titles use filenames.

Verification: `python tools/check_music.py` exercises actual library and control
methods with deterministic SD/audio/button doubles. `python tools/render_ui.py`
renders the listening scene, animation frame, paused screen, options and folders.
Physical audio timing and button feel still require testing on the device.

