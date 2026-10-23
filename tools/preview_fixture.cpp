#include "NativeUI.h"
#include "AquariumArt.h"
#include <cstdio>
#define WHITE SSD1306_WHITE
#define BLACK SSD1306_BLACK
uint32_t fixtureMillis = 100;
Adafruit_SH1107 panel;
GameDisplay display(panel);
using NativeUI::text;
byte stage = 4, speciesType = 0, eggType = 0, idleStyle = 0,
     roomStyle = 0, anim = 0, full = 8, happy = 9,
     energy = 7, clean = 9, poop = 0, partnerId = 2, partnerBond = 90,
     mainCategory = 0, outfit = 0;
unsigned int careMistakes = 0;
byte discipline = 5;
bool dead = false, sick = false, petAway = false, sleeping = false,
     lightOff = false, menuVisible = true;
char petName[] = "MOCHI";
constexpr byte STAGE_EGG = 0, STAGE_BABY = 1, STAGE_CHILD = 2,
               STAGE_TEEN = 3, STAGE_ADULT = 4;
constexpr byte CATEGORY_COUNT = 6, HOUSE_ROOM_COUNT = 5, CITY_PLACE_COUNT = 6;
bool needsCare() { return false; }
bool isScheduledAwayTime() { return false; }
byte visualHour() { return 14; }
byte clockMinute() { return 28; }
byte petLineage() { return stage < STAGE_ADULT ? speciesType % 10 : eggType % 10; }
byte currentEvolutionPath() { return (idleStyle & 0x80U) ? 1 : 0; }
struct World {
  byte room = 0, place = 0;
  byte houseRoom() { return room; }
  byte cityPlace() { return place; }
  const __FlashStringHelper *houseRoomName(byte r) {
    static const char *n[] = {"Bedroom", "Aquarium", "Plant Room", "Garden",
                              "Kitchen"};
    return reinterpret_cast<const __FlashStringHelper *>(n[r % 5]);
  }
  const __FlashStringHelper *cityPlaceName(byte p) {
    static const char *n[] = {"School", "Market", "Museum",
                              "Park",   "Work",   "Spaceport"};
    return reinterpret_cast<const __FlashStringHelper *>(n[p % 6]);
  }
  bool partnerLivesAtHome(byte, byte) { return true; }
} cityWorld;
struct Care {
  bool needsCare() { return false; }
} botany, aquarium;
void drawCurrentPetTiny(int x, int y) {
  NativeUI::pet(display, x, y, speciesType, stage, anim);
}
struct ParentDialogueContext {
  uint8_t stage = 4;
};
struct ParentDialogueOutcome {
  int trust = 3, bond = 2;
};
class ParentDialogueSystem {
public:
  GameDisplay &display_ = display;
  void drawDialoguePage(const ParentDialogueContext &, const char *, uint8_t,
                        uint8_t) const;
  void drawChoicePage(const ParentDialogueContext &, uint8_t, uint8_t) const;
  void drawResultPage(const ParentDialogueContext &, const char *,
                      const ParentDialogueOutcome &) const;
  const char *choiceLabel(uint8_t, uint8_t i) const {
    const char *n[] = {"Tell me about your day.", "Let's try it together.",
                       "Take a little break."};
    return n[i];
  }
};
constexpr int AQUARIUM_FISH_SLOTS = 4;
class AquariumSystem {
public:
  GameDisplay &display_ = display;
  char names_[4][12] = {"BUBBLE", "LUNA", "FINN", "NORI"};
  byte species_[4] = {0, 1, 2, 3}, tankDecor_ = 3, waterQuality_ = 8,
       tankClean_ = 9;
  byte personalitySeed_[4] = {1, 5, 10, 15}, actionPose_[4] = {0, 0, 0, 0},
       actionFrames_[4] = {0, 0, 0, 0}, growthBranch_[4] = {0, 1, 2, 3};
  uint16_t ageTicks_[4] = {288, 700, 1600, 2600};
  bool slotOccupied(byte) const { return true; }
  byte fishCount() const { return 4; }
  byte capacity() const { return 4; }
  bool needsCare() const { return false; }
  byte fishLifeStage(byte slot) const {
    return ageTicks_[slot] < 288 ? 0 : ageTicks_[slot] < 864 ? 1 :
           ageTicks_[slot] < 2016 ? 2 : 3;
  }
  void drawTankScene(uint8_t, uint16_t, bool);
};
constexpr byte EMPTY_PLANT = 255;
class BotanySystem {
public:
  GameDisplay &display_ = display;
  byte selectedGarden_ = 1, selectedIndoor_ = 2, gardenCapacity_ = 4,
       indoorCapacity_ = 4;
  byte cropSpecies_[4] = {0, 1, 2, 255}, indoorSpecies_[4] = {1, 2, 3, 4},
       cropWater_[4] = {6, 8, 3, 0}, indoorWater_[4] = {8, 4, 7, 3},
       cropHealth_[4] = {8, 7, 6, 0}, indoorHealth_[4] = {9, 8, 7, 9};
  byte cropStage(byte i) const { return i + 1; }
  byte indoorStage(byte i) const { return i + 1; }
  const __FlashStringHelper *cropName(byte) const { return F("MOON BERRIES"); }
  const __FlashStringHelper *indoorName(byte) const { return F("STAR FLOWER"); }
  void drawGardenOverview(uint8_t) const;
  void drawPlantRoomOverview(uint8_t) const;
};
class Esp32MusicPlayer {
public:
  GameDisplay &display_ = display;
  byte trackCount_ = 12, selectedTrack_ = 2;
  float gain_ = .65f;
  String status_ = "PLAYING";
  String trackName(byte) const { return "03 - Evening in the little town"; }
  bool isPlaying() const { return true; }
  void drawPlayerScreen() const;
};
// The runner inserts actual firmware function definitions here.

const __FlashStringHelper *categoryShortTitle(byte) { return F("FOOD"); }
byte categoryActionAt(byte, byte i) { return i; }
bool actionUnlocked(byte i) { return i < 5; }
const __FlashStringHelper *menuTitle(byte i) {
  const char *n[] = {"Berry bowl",   "Warm soup",   "Garden salad",
                     "Crispy snack", "Fruit plate", "Royal feast"};
  return reinterpret_cast<const __FlashStringHelper *>(n[i % 6]);
}
const __FlashStringHelper *menuDescription(byte) {
  return F("A fresh little meal");
}
const __FlashStringHelper *menuUnlockShort(byte) {
  return F("Adult stage needed");
}
const __FlashStringHelper *gameName(byte) { return F("Star Catch"); }
int featuredGameSlot(byte) { return 0; }
byte favoriteGameId = 3, dislikedGameId = 5;

struct FamilyFixture {
  byte childCount() { return 1; }
  bool hasCompanion() { return true; }
} deepLife, lifeWorld;
