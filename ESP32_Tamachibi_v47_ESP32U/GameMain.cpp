/*
  Tamachibi v47 ESP32-WROOM-32U breakout edition.

  This folder keeps the complete v47 game logic and replaces the RP2040 board
  layer with Arduino-ESP32 support. The reference pin map and build instructions
  are in README_ESP32U.md and ESP32U_WIRING_AND_MUSIC.md.

  Required libraries: Adafruit SH110X, Adafruit SSD1306 (small status OLED),
  Adafruit GFX, Adafruit BusIO, and ESP8266Audio.
*/

#include <Arduino.h>
#include <esp_system.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "GameDisplay.h"
#include "NativeUI.h"
#include "GrowthSprites.h"
#include "RosterSprites.h"
#include <EEPROM.h>
#include <stddef.h>
#include "Esp32Platform.h"
#include "Aquarium.h"
#include "Botany.h"
#include "CityWorld.h"
#include "NeighborStories.h"
#include "PolishedScenes.h"
#include "VisualOverhaul.h"
#include "NeedsDisplay.h"
#include "ParadiseSystem.h"
#include "LifeWorld.h"
#include "DeepLife.h"
#include "ParentDialogue.h"
#include "GrowingTogether.h"
#include "SaveProfiles.h"
#include "MusicPlayer.h"
#include "MusicScene.h"
#include "GameplayActions.h"
#include "GameplayProgress.h"

#define OLED_W 128
#define OLED_H 128
#define LEGACY_OLED_H 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C
#define NEEDS_OLED_ADDR 0x3C

Adafruit_SH1107 mainOled(OLED_W, OLED_H, &Wire, OLED_RESET, 400000UL, 100000UL);
GameDisplay display(mainOled);
NeedsDisplay needsDisplay(&Wire1);
Esp32RTCCompat rtc;
bool externalRtcReady = false;

#ifdef WHITE
#undef WHITE
#endif
#define WHITE SSD1306_WHITE

const byte SPECIES_COUNT = 50;
const byte OUTFIT_COUNT = 16;
const byte ROOM_STYLE_COUNT = 10;
const byte PLANT_COUNT = 12;
const byte HABITAT_COUNT = 8;
const byte MATERIAL_COUNT = 6;
const byte CRAFT_RECIPE_COUNT = 12;
const byte CARE_HISTORY_COUNT = 8;
// Care history stores specific choices as well as broad milestones. These fit
// the existing byte-sized history format, so older saves remain compatible.
const byte CARE_EVENT_FOOD_BASE = 20;
const byte CARE_EVENT_MED_BASE = 30;
const byte CARE_EVENT_COMFORT_BASE = 40;
const byte PARTNER_COUNT = 8;
const byte AQUARIUM_MENU_COUNT = 10;
#ifdef BLACK
#undef BLACK
#endif
#define BLACK SSD1306_BLACK

// Reference GPIO map for a classic ESP32-WROOM-32U development breakout.
// Recheck these numbers against the exact carrier-board pin labels before wiring.
#define SD_MISO_PIN 19
#define SD_CS_PIN 27
#define MAIN_OLED_SDA 21
#define MAIN_OLED_SCL 22
#define NEEDS_OLED_SDA 16
#define NEEDS_OLED_SCL 17
#define BTN_L 32
#define BTN_S 33
#define ENCODER_SW_PIN 25
#define BTN_R ENCODER_SW_PIN
#define BUZZER_PIN 26
#define VOLUME_ENCODER_CLK_PIN 35
#define VOLUME_ENCODER_DT_PIN 39

// Set to 1 while balancing: one five-minute pet tick passes each real second.
#define TAMACHIBI_TEST_MODE 0

// Version 33 remains intentionally always awake for reliable testing.
// 1 = ESP32 stays active, OLED stays on, and the virtual pet never sleeps.
#define ALWAYS_AWAKE_MODE 0

// Deep standby remains disabled in this ESP32 port.
#define ENABLE_DEEP_STANDBY 0

// Optional DS3231 on the same I2C bus. Enable only when the module is wired.
// It preserves real time through a completely switched-off main battery.
#define ENABLE_DS3231_RTC 0
#define ASK_FOR_CLOCK_ON_BOOT 1
#define DS3231_ADDR 0x68

// Optional battery meter is disabled; no ESP32 divider/gate pins are allocated.
// Add and validate a board-specific divider before enabling this feature.
#define BATTERY_MONITOR_ENABLED 0
#define BATTERY_DIVIDER_TOP_K 220UL
#define BATTERY_DIVIDER_BOTTOM_K 100UL

// Gameplay action IDs are shared with location systems.
#define MENU_COUNT 38

#define FOOD_BERRY 0
#define FOOD_RICE  1
#define FOOD_VEG   2
#define FOOD_SNACK 3
#define FOOD_TREAT 4
#define FOOD_COUNT 5

byte menu = 0;
byte eggType = 0;
byte geneA = 0;
byte geneB = 0;
byte geneC = 0;

// v8 deep genetics. These are rolled fresh every new egg.
byte bodyType = 0;
byte earType = 0;
byte eyeType = 0;
byte mouthType = 0;
byte tailType = 0;
byte headTrait = 0;
byte patternType = 0;
byte idleStyle = 0;
byte birthMark = 0;
byte speciesType = 0;       // v9: sensible character family
byte statBias = 0;
byte rareAllRounder = 0;
unsigned int birthCode = 0;
byte eggChoice[4] = {0, 1, 2, 3};

byte health = 10;
byte full = 8;
byte energy = 8;
byte happy = 8;
byte clean = 10;
byte discipline = 5;

bool sleeping = false;
bool sick = false;
bool dead = false;
bool lightOff = false;

// v12 expanded medicine/illness system
// conditionType: 0 none, 1 cold, 2 tummy, 3 rash, 4 exhaustion, 5 gloom, 6 fever
byte conditionType = 0;
byte conditionSeverity = 0;
unsigned int conditionTicks = 0;
byte medCold = 1;
byte medTummy = 1;
byte medSkin = 1;
byte medEnergy = 1;
byte medMood = 1;
byte medAll = 0;

byte poop = 0;
unsigned long ageTicks = 0;
byte stage = 0;
unsigned int careMistakes = 0;
unsigned int stageCareMistakes = 0;

enum GrowthStage : byte {
  STAGE_EGG = 0,
  STAGE_BABY = 1,
  STAGE_CHILD = 2,
  STAGE_TEEN = 3,
  STAGE_ADULT = 4,
  STAGE_ELDER = 5
};

byte lastFood = 255;
byte varietyStreak = 0;
byte walkWins = 0;
unsigned int crossyBest = 0;
byte studyStars = 0;
byte gardenLevel = 0;
byte musicStars = 0;
byte trainStars = 0;
byte gardenHarvests = 0;

// v8 genetics/customization systems
unsigned int coins = 8;
byte skillSmart = 0;
byte skillStyle = 0;
byte skillKind = 0;
byte skillFit = 0;
byte jobRank = 0;
byte outfit = 0;
byte roomStyle = 0;
byte toyItem = 0;
byte personality = 0;
byte mischief = 0;
byte friendCount = 0;       // friends met while exploring
byte tripBadges = 0;
byte cookedMeals = 0;

// v19 friendship, routine, memory, evolution and legacy systems.
byte ownerBond = 20;         // relationship with the player, 0-100
byte ownerTrust = 20;        // reliability/safety felt by the pet, 0-100
byte temperament = 0;        // independent of species, 0-7
char petName[7] = "CHIBI";   // six characters plus terminator
unsigned long lastCareTick = 0;
unsigned long lastTalkTick = 0;
byte dailyRoutineMask = 0;
byte weeklyCareBits = 0;
byte caredDayStreak = 0;
byte missedCareDays = 0;
unsigned long routineDay = 0;
byte lastTalkChoice = 255;
byte lastEventId = 0;
byte favoriteGivenFood = 255;
byte ignoredCallCount = 0;
byte rescuedFromSickness = 0;
byte lastTripEvent = 255;
byte hobbyType = 0;
byte favoriteGameId = 0;
byte dislikedGameId = 1;
byte activeGameId = 255;
unsigned long activeGameStartedAt = 0;
bool activeGameResolved = false;
bool activeGameWon = false;
byte activeGameScore = 0;
GameplayProgress gameplayProgress;
unsigned long lastGameBondTick = 0;
byte featuredPlayedMask = 0;
unsigned long featuredDay = 0;
byte gameCounts[13] = {0};
byte nutritionScore = 50;
byte cleanlinessScore = 50;
byte exerciseScore = 0;
byte sicknessHistory = 0;
byte overfeedCount = 0;
byte praiseCount = 0;
byte giftMilestones = 0;
byte weight = 5;
byte pendingCallType = 0;
unsigned long pendingCallTicks = 0;
unsigned long lastBirthdayRewardDay = 0;
byte dailyGoalType = 0;
byte dailyGoalProgress = 0;
byte dailyGoalTarget = 1;
byte weeklyGoalType = 0;
byte weeklyGoalProgress = 0;
byte weeklyGoalTarget = 5;
unsigned long goalDay = 0;
unsigned long goalWeek = 0;
unsigned long sitterTicksRemaining = 0;
unsigned long sitterCooldownTicks = 0;
byte soundMode = 2;          // 0 off, 1 quiet, 2 normal
unsigned long buzzerStopAt = 0; // non-blocking stop time for soft PWM notes
byte brightnessLevel = 1;   // 0 low, 1 medium, 2 high
byte deathReason = 0;        // 0 none, 1 illness, 2 old age, 3 moved on after severe neglect
byte archivedCurrent = 0;
unsigned int generation = 1;
byte albumCount = 0;
byte albumSpecies[8] = {0};
byte albumAge[8] = {0};
byte albumBond[8] = {0};
byte albumReason[8] = {0};
char albumName[8][7] = {{0}};
byte legacySpecies = 255;
byte legacySkill = 255;
byte legacyOutfit = 0;
byte mainCategory = 0;
const byte CATEGORY_COUNT = 6;


enum DialWorldView : byte { VIEW_PET = 0, VIEW_HOUSE = 1, VIEW_CITY = 2 };
byte dialWorldView = VIEW_PET;
unsigned long lastViewDialReadAt = 0;
uint8_t worldEncoderPreviousState = 0;
int8_t worldEncoderTransitionCount = 0;
bool worldEncoderInitialized = false;

// v21 living-world systems. These are stored in a separate checksummed save
// block so v19/v20 main saves remain loadable without resetting the pet.
byte difficultyMode = 1;       // 0 relaxed, 1 normal, 2 classic, 3 test
byte rescueMode = 1;           // 1 gives a runaway/recovery chance before loss
byte habitat = 0;              // 0 home, 1 forest, 2 tropical, 3 glacier...
unsigned int habitatOwned = 1U;
unsigned long discoveredSpeciesLo = 0;
unsigned long discoveredSpeciesHi = 0;
unsigned int discoveredPlants = 0;
unsigned int visitedLocations = 0;
unsigned int recipesCrafted = 0;
unsigned long discoveredOutfits = 1UL;
unsigned long discoveredRooms = 1UL;
byte craftMaterials[MATERIAL_COUNT] = {1, 1, 0, 0, 0, 0}; // wood, cloth, metal, crystal, flower, shell
byte foodExperience[FOOD_COUNT] = {0};
byte foodBoredom[FOOD_COUNT] = {0};
byte learnedFavoriteFood = 255;
byte learnedDislikedFood = 255;
byte favoritePlace = 0;
byte partnerId = 255;
byte partnerBond = 0;
byte familyReady = 0;
byte inheritedTrait = 255;
byte inheritedTemperament = 255;
byte inheritedFavoriteGame = 255;
byte inheritedBodyType = 255;
bool petAway = false;
unsigned long awayTicks = 0;
byte runawayCount = 0;
byte careHistoryType[CARE_HISTORY_COUNT] = {0};
unsigned int careHistoryDay[CARE_HISTORY_COUNT] = {0};
byte careHistoryHour[CARE_HISTORY_COUNT] = {0};
byte careHistoryHead = 0;
byte careHistoryCount = 0;
byte seasonalEventBest = 0;
unsigned long seasonalEventPlayedDay = 999999UL;
unsigned int seasonalRewardMask = 0;
byte albumPartner[8] = {255,255,255,255,255,255,255,255};
unsigned int albumGeneration[8] = {0};
unsigned long extrasSequence = 0;

// Care-mistake latches prevent one ignored need from counting every tick.
bool foodMistakeOpen = false;
bool cleanMistakeOpen = false;
bool happyMistakeOpen = false;
bool sickMistakeOpen = false;
bool lightMistakeOpen = false;
bool callMistakeOpen = false;
unsigned long zeroSickTicks = 0;
unsigned long wrongLightTicks = 0;
unsigned long criticalNeglectTicks = 0;
byte berryInv = 1;
byte herbInv = 0;
byte vegInv = 0;
byte spiceInv = 0;

// v11 connected cooking/garden systems
byte mealBerryBite = 0;
byte mealHerbSoup = 0;
byte mealVegPlate = 0;
byte mealSpicyJam = 0;
byte mealRoyalStew = 0;
byte mealBuffType = 0;        // 0 none, 1 berry, 2 soup, 3 veg, 4 jam, 5 stew
byte mealBuffTicks = 0;       // long food effect remaining, measured in life ticks
byte foodGuardTicks = 0;      // blocks hunger drops while > 0

byte gardenPlant = 255;       // 255 means empty plot
unsigned long gardenStartTick = 0;
byte gardenGrowTicks = 0;
byte gardenWater = 0;
byte gardenSun = 0;
byte gardenCare = 0;
byte gardenProblem = 0;
byte gardenQuality = 0;
unsigned long outfitOwned = 1UL;
unsigned long roomOwned = 1UL;

unsigned long lastGameTick = 0;  // retained for compatibility; life timing now uses RTC seconds
unsigned long lastLifeClockEpoch = 0;
unsigned long lifeSecondRemainder = 0;
unsigned long lastAnimTick = 0;
unsigned long lastButtonTick = 0;
bool rightButtonRawLast = HIGH;
bool rightButtonStable = HIGH;
unsigned long rightButtonRawChangedAt = 0;
bool rightButtonDebounceReady = false;

// Developer speed is intentionally not saved. Every reboot returns to normal.
// 0 normal, 1 10x, 2 60x, 3 300x, 4 paused.
byte developerSpeedMode = 0;

byte anim = 0;
bool oldL = HIGH;
bool oldS = HIGH;
bool oldR = HIGH;

// ---------- easy life-speed settings ----------
// Change these later to tune how fast the pet lives and loses stats.
// LIFE_TICK_MS is the main care clock. 300000 ms = 5 minutes per life tick.
// With the sleep settings below, a healthy/full pet can sleep for 10 hours safely.
const unsigned long SIM_SECONDS_PER_TICK = 300UL;
const unsigned long LIFE_TICK_MS = TAMACHIBI_TEST_MODE ? 1000UL : 300000UL;
const unsigned long GAME_TICK_MS = LIFE_TICK_MS;
const unsigned long RTC_SECONDS_PER_TICK = TAMACHIBI_TEST_MODE ? 1UL : SIM_SECONDS_PER_TICK;
const unsigned long ANIM_TICK_MS = 350UL;
const unsigned long ACTIVE_ANIMATION_WINDOW_MS = 12000UL;
const unsigned long NARRATIVE_COOLDOWN_MS = 15UL * 60UL * 1000UL;
const unsigned long TICKS_PER_HOUR = 12UL;
const unsigned long TICKS_PER_DAY = 288UL;

// Growth schedule: egg minutes, baby day 0-1, child days 1-3,
// teen days 3-6, adult days 6-20, elder day 20 onward.
const unsigned long EGG_HATCH_TICKS = 1UL;
const unsigned long CHILD_START_TICKS = 1UL * TICKS_PER_DAY;
const unsigned long TEEN_START_TICKS = 3UL * TICKS_PER_DAY;
const unsigned long ADULT_START_TICKS = 6UL * TICKS_PER_DAY;
const unsigned long ELDER_START_TICKS = 20UL * TICKS_PER_DAY;

// Daily-care balance. A full healthy pet needs a little attention each day,
// while serious neglect takes roughly two to three days to become fatal.
const byte HUNGER_DROP_EVERY_TICKS = 12;       // 1 hour
const byte ENERGY_DROP_EVERY_TICKS = 18;       // 1.5 hours
const byte MOOD_DROP_EVERY_TICKS = 24;         // 2 hours
const byte POOP_CLEAN_DROP_EVERY_TICKS = 12;   // 1 hour while poop exists

// Eight-hour automatic sleep with very slow hunger loss.
const byte SLEEP_FOOD_DROP_EVERY_TICKS = 72;   // 6 hours
const byte SLEEP_ENERGY_GAIN_EVERY_TICKS = 12; // 1 hour: real overnight sleep is required
const unsigned long AUTO_WAKE_AFTER_TICKS = 144UL; // safety fallback only; schedule normally wakes the pet
const bool WAKE_AUTOMATICALLY = false;

// One care mistake is counted per unresolved need, not once per damage tick.
const unsigned long FOOD_MISTAKE_AFTER_TICKS = 72UL;    // 6 hours empty
const unsigned long CLEAN_MISTAKE_AFTER_TICKS = 96UL;   // 8 hours dirty
const unsigned long SICK_MISTAKE_AFTER_TICKS = 144UL;   // 12 hours untreated
const unsigned long HAPPY_MISTAKE_AFTER_TICKS = 288UL;  // one day miserable
const unsigned long MILD_NEGLECT_TICKS = 288UL;          // 24 hours: mild effects
const unsigned long CRITICAL_GRACE_TICKS = 576UL;        // 48 hours: health damage begins
const unsigned long SEVERE_NEGLECT_TICKS = 864UL;        // 72 hours: severe outcome
const unsigned long CRITICAL_DAMAGE_EVERY_TICKS = 48UL; // one health each 4 hours after 48h
const unsigned long SICK_HEALTH_DAMAGE_EVERY_TICKS = 96UL; // 8 hours while seriously sick

// Random event odds. Bigger denominator = rarer.
const byte BASE_POOP_CHANCE = 120;
const byte GOOD_DISCIPLINE_POOP_CHANCE = 180;
const byte BAD_DISCIPLINE_POOP_CHANCE = 72;
const byte SICK_FROM_DIRT_CHANCE = 10;

// v12 medicine tuning. Bigger numbers = less illness / slower sickness.
const byte ILLNESS_CHECK_EVERY_TICKS = 18;       // about every 90 minutes while awake
const byte RANDOM_ILLNESS_CHANCE = 18;          // 1 in 18 checks when conditions are bad
const byte CONDITION_DAMAGE_EVERY_TICKS = 96;   // fever/bad illness damage pacing
const byte SLEEP_CONDITION_HEAL_EVERY_TICKS = 24; // sleep can slowly reduce mild issues
const byte MEDICINE_PRICE = 8;                  // coins to buy a dose if stock is empty

// v11 garden/cooking tuning. All are measured in LIFE_TICK_MS ticks.
// With LIFE_TICK_MS = 5 min, 3 ticks = 15 min, 12 ticks = 1 hour.
const byte GROW_BERRY_TICKS = 3;
const byte GROW_HERB_TICKS = 5;
const byte GROW_VEG_TICKS = 6;
const byte GROW_SPICE_TICKS = 8;
const byte GROW_FLOWER_TICKS = 4;

// Prepared meal long-effect durations.
const byte BUFF_BERRY_BITE_TICKS = 8;
const byte BUFF_HERB_SOUP_TICKS = 14;
const byte BUFF_VEG_PLATE_TICKS = 16;
const byte BUFF_SPICY_JAM_TICKS = 10;
const byte BUFF_ROYAL_STEW_TICKS = 24;

// UI behavior.
const unsigned long HOLD_RIGHT_EXIT_MS = 650UL;
const unsigned long GAME_EXIT_HOLD_MS = 2850UL;         // every game: hold D2 about 3 sec to exit
const unsigned long LONG_RIGHT_GAME_ACTION_MS = 850UL;  // D2 intermediate hold in grid games = move down
const unsigned long LONG_RIGHT_GAME_EXIT_MS = GAME_EXIT_HOLD_MS;
const unsigned long HOLD_RIGHT_HIDE_MENU_MS = 650UL;

const unsigned long OLED_OFF_TIMEOUT_MS = 25000UL;
const unsigned long STANDBY_TIMEOUT_MS = 45000UL;
const unsigned long DIRTY_SICK_TICKS = 144UL;         // 12 hours dirty = illness risk

// v19 dual-slot checksummed save system, stored in ESP32 EEPROM emulation.
// Do not save every loop because flash has limited write cycles.
const unsigned long AUTO_SAVE_MS = 600000UL;
const unsigned long SAVE_MAGIC = 0x544D4139UL; // TMA9
const byte SAVE_VERSION = 20; // v20 changes evolution tags; the byte layout is unchanged.
unsigned long saveSequence = 0;
bool saveDirty = false;
unsigned long lastSaveAt = 0;

struct SaveData {
  unsigned long magic;
  byte version;
  unsigned long sequence;
  byte menu, mainCategory;
  byte eggType, geneA, geneB, geneC;
  byte bodyType, earType, eyeType, mouthType, tailType, headTrait, patternType, idleStyle, birthMark, speciesType, statBias, rareAllRounder;
  unsigned int birthCode;
  byte health, full, energy, happy, clean, discipline;
  byte sleeping, sick, dead, lightOff;
  byte conditionType, conditionSeverity;
  unsigned int conditionTicks;
  byte medCold, medTummy, medSkin, medEnergy, medMood, medAll;
  byte poop;
  unsigned long ageTicks;
  byte stage;
  unsigned int careMistakes, stageCareMistakes;
  byte lastFood;
  byte varietyStreak;
  byte walkWins;
  unsigned int crossyBest;
  byte studyStars, gardenLevel, musicStars, trainStars, gardenHarvests;
  unsigned int coins;
  byte skillSmart, skillStyle, skillKind, skillFit;
  byte jobRank, outfit, roomStyle, toyItem, personality, mischief, friendCount, tripBadges, cookedMeals;
  byte berryInv, herbInv, vegInv, spiceInv;
  byte mealBerryBite, mealHerbSoup, mealVegPlate, mealSpicyJam, mealRoyalStew;
  byte mealBuffType, mealBuffTicks, foodGuardTicks;
  byte gardenPlant;
  unsigned long gardenStartTick;
  byte gardenGrowTicks, gardenWater, gardenSun, gardenCare, gardenProblem, gardenQuality;
  unsigned long sleepTicks;
  unsigned long outfitOwned;
  unsigned long roomOwned;

  byte ownerBond, ownerTrust, temperament;
  char petName[7];
  unsigned long lastCareTick, lastTalkTick;
  byte dailyRoutineMask, weeklyCareBits, caredDayStreak, missedCareDays;
  unsigned long routineDay;
  byte lastTalkChoice, lastEventId, favoriteGivenFood, ignoredCallCount, rescuedFromSickness, lastTripEvent, hobbyType;
  byte favoriteGameId, dislikedGameId, activeGameId;
  unsigned long lastGameBondTick;
  byte featuredPlayedMask;
  unsigned long featuredDay;
  byte gameCounts[13];
  byte nutritionScore, cleanlinessScore, exerciseScore, sicknessHistory, overfeedCount, praiseCount, giftMilestones, weight;
  byte pendingCallType;
  unsigned long pendingCallTicks, lastBirthdayRewardDay;
  byte dailyGoalType, dailyGoalProgress, dailyGoalTarget;
  byte weeklyGoalType, weeklyGoalProgress, weeklyGoalTarget;
  unsigned long goalDay, goalWeek, sitterTicksRemaining, sitterCooldownTicks;
  byte soundMode, brightnessLevel, deathReason, archivedCurrent;
  unsigned int generation;
  byte albumCount;
  byte albumSpecies[8], albumAge[8], albumBond[8], albumReason[8];
  char albumName[8][7];
  byte legacySpecies, legacySkill, legacyOutfit;
  byte foodMistakeOpen, cleanMistakeOpen, happyMistakeOpen, sickMistakeOpen, lightMistakeOpen, callMistakeOpen;
  unsigned long zeroFoodTicks, zeroCleanTicks, zeroHappyTicks, zeroSickTicks, wrongLightTicks, criticalNeglectTicks;
  unsigned long lastRtcEpoch;
  unsigned long checksum;
};

Esp32EEPROMStorage<SaveData> saveStoreA(0);
Esp32EEPROMStorage<SaveData> saveStoreB(sizeof(SaveData));

const unsigned long EXTRAS_MAGIC = 0x544D5831UL; // TMX1
const byte EXTRAS_VERSION = 1;
struct SaveExtras {
  unsigned long magic;
  byte version;
  unsigned long sequence;
  byte difficultyMode, rescueMode, habitat;
  unsigned int habitatOwned;
  unsigned long discoveredSpeciesLo, discoveredSpeciesHi;
  unsigned int discoveredPlants, visitedLocations, recipesCrafted;
  unsigned long discoveredOutfits, discoveredRooms;
  byte craftMaterials[MATERIAL_COUNT];
  byte foodExperience[FOOD_COUNT], foodBoredom[FOOD_COUNT];
  byte learnedFavoriteFood, learnedDislikedFood, favoritePlace;
  byte partnerId, partnerBond, familyReady, inheritedTrait;
  byte inheritedTemperament, inheritedFavoriteGame, inheritedBodyType;
  byte petAway;
  unsigned long awayTicks;
  byte runawayCount;
  byte careHistoryType[CARE_HISTORY_COUNT];
  unsigned int careHistoryDay[CARE_HISTORY_COUNT];
  byte careHistoryHour[CARE_HISTORY_COUNT], careHistoryHead, careHistoryCount;
  byte seasonalEventBest;
  unsigned long seasonalEventPlayedDay;
  unsigned int seasonalRewardMask;
  byte albumPartner[8];
  unsigned int albumGeneration[8];
  unsigned long checksum;
};
Esp32EEPROMStorage<SaveExtras> extraStoreA(sizeof(SaveData) * 2);
Esp32EEPROMStorage<SaveExtras> extraStoreB(sizeof(SaveData) * 2 + sizeof(SaveExtras));

const size_t AQUARIUM_SAVE_BASE = sizeof(SaveData) * 2 + sizeof(SaveExtras) * 2;
AquariumSystem aquarium(AQUARIUM_SAVE_BASE, display, BTN_L, BTN_S, BTN_R);
const size_t BOTANY_SAVE_BASE = AQUARIUM_SAVE_BASE + sizeof(AquariumSave) * 2;
BotanySystem botany(BOTANY_SAVE_BASE, display, BTN_L, BTN_S, BTN_R);
const size_t CITY_SAVE_BASE = BOTANY_SAVE_BASE + sizeof(BotanySave) * 2;
CityWorldSystem cityWorld(CITY_SAVE_BASE, display, BTN_L, BTN_S, BTN_R);
const size_t PARADISE_SAVE_BASE = CITY_SAVE_BASE + sizeof(CityWorldSave) * 2;
ParadiseSystem paradise(PARADISE_SAVE_BASE);
const size_t LIFEWORLD_SAVE_BASE = PARADISE_SAVE_BASE + sizeof(ParadiseSave) * 2;
LifeWorldSystem lifeWorld(LIFEWORLD_SAVE_BASE, display, BTN_L, BTN_S, BTN_R);
const size_t DEEPLIFE_SAVE_BASE = LIFEWORLD_SAVE_BASE + sizeof(LifeWorldSave) * 2;
DeepLifeSystem deepLife(DEEPLIFE_SAVE_BASE, display, BTN_L, BTN_S, BTN_R);
const size_t PARENT_DIALOGUE_SAVE_BASE = DEEPLIFE_SAVE_BASE + sizeof(DeepLifeSave) * 2;
ParentDialogueSystem parentDialogue(PARENT_DIALOGUE_SAVE_BASE, display, BTN_L, BTN_S, BTN_R);
const size_t GROWING_TOGETHER_SAVE_BASE = PARENT_DIALOGUE_SAVE_BASE + sizeof(ParentDialogueSave) * 2;
GrowingTogetherSystem growingTogether(GROWING_TOGETHER_SAVE_BASE, display, BTN_L, BTN_S, BTN_R);
SaveProfileManager saveProfiles(display, BTN_L, BTN_S, BTN_R);
Esp32MusicPlayer musicPlayer(display, BTN_L, BTN_S, BTN_R,
                             VOLUME_ENCODER_CLK_PIN, VOLUME_ENCODER_DT_PIN);
static_assert(GROWING_TOGETHER_SAVE_BASE + sizeof(GrowingTogetherSave) * 2 <= TAMACHIBI_EEPROM_BYTES,
              "Tamachibi save data exceeds the 4 KB EEPROM allocation");

// Save structures and their explicit prototypes must appear before the first
// function definition in this .ino. This prevents Arduino from generating
// invalid hidden prototypes before SaveData and SaveExtras are known.
unsigned long calculateExtrasChecksum(const SaveExtras &d);
bool extrasAreValid(const SaveExtras &d);
void fillSaveExtras(SaveExtras &d);
void applySaveExtras(const SaveExtras &d);
void saveExtrasNow();
bool loadExtrasSave();

unsigned long calculateSaveChecksum(const SaveData &d);
bool saveIsValid(const SaveData &d);
void fillSaveData(SaveData &d);
void applySaveData(const SaveData &d);
void saveGameNow();
bool loadGameSave();

unsigned long activeLifeTickSeconds() {
  if (developerSpeedMode == 1) return 30UL;
  if (developerSpeedMode == 2) return 5UL;
  if (developerSpeedMode == 3) return 1UL;
  if (developerSpeedMode == 4) return 0UL;
  return RTC_SECONDS_PER_TICK;
}

const __FlashStringHelper* developerSpeedName() {
  if (developerSpeedMode == 1) return F("10x");
  if (developerSpeedMode == 2) return F("60x");
  if (developerSpeedMode == 3) return F("300x");
  if (developerSpeedMode == 4) return F("Paused");
  return F("Normal");
}

unsigned long difficultyTicks(unsigned long normalTicks) {
  if (difficultyMode == 0) return max(1UL, normalTicks * 2UL);
  if (difficultyMode == 2) return max(1UL, (normalTicks * 3UL) / 4UL);
  if (difficultyMode == 3) return max(1UL, normalTicks / 12UL);
  return max(1UL, normalTicks);
}

unsigned long growthThreshold(unsigned long normalTicks) {
  if (difficultyMode == 3) return max(1UL, normalTicks / 24UL);
  return normalTicks;
}

byte adjustedGardenGrowTime(byte normalTicks) {
  if (difficultyMode == 3) return max((byte)1, (byte)(normalTicks / 4));
  return normalTicks;
}

const __FlashStringHelper* difficultyName() {
  if (difficultyMode == 0) return F("Relaxed");
  if (difficultyMode == 2) return F("Classic");
  if (difficultyMode == 3) return F("Test");
  return F("Normal");
}

bool menuVisible = true;
unsigned long rightMenuDownAt = 0;
bool rightMenuHoldUsed = false;

bool screenSleeping = false;
bool oledIsOff = false;
unsigned long lastInputAt = 0;
unsigned long lastNarrativeInteractionAtMs = 0;
unsigned long zeroFoodTicks = 0;
unsigned long zeroCleanTicks = 0;
unsigned long zeroHappyTicks = 0;
volatile bool deepWakeFlag = false;
unsigned long lastRtcEpoch = 0;
bool rtcWasPetSleeping = false;
bool rtcCatchUpMode = false;
byte lastWakeReason = 0; // 0 boot, 1 OLED wake, 2 deep standby wake
unsigned long sleepTicks = 0;
unsigned long rightPressStarted = 0;
bool rightHoldUsed = false;

// Battery thresholds for a single-cell protected Li-ion/LiPo pack.
const unsigned int BATTERY_LOW_MV = 3500;
const unsigned int BATTERY_CRITICAL_MV = 3350;
unsigned int batteryMv = 0;
unsigned long lastBatteryCheckAt = 0;

// Daily routine bits.
const byte ROUTINE_GREET = 1;
const byte ROUTINE_CHECK = 2;
const byte ROUTINE_CARE = 4;
const byte ROUTINE_TALK = 8;
const byte ROUTINE_PLAY = 16;
const byte ROUTINE_BED = 32;

// Rotary encoder view control and living-world overview.
void updateWorldViewFromEncoder(bool forceRead = false);
void resetWorldViewEncoder();
int8_t readWorldViewEncoderStep();
void drawHouseWorldView();
void drawCityWorldView();
void enterHouseSelection();
void enterCitySelection();

// ---------- forward declarations ----------
void drawScreen(bool doDisplay);
void drawSquarePet();
void petTone(unsigned int freq, unsigned int durationMs);
void snd(unsigned int freq, byte dur);
void serviceSoftBuzzer();
void stopSoftBuzzer();
void clickSound();
void stepSound();
void goodSound();
void badSound();
void winSound();
void careSound();
void addStat(byte &v, byte amount);
void subStat(byte &v, byte amount);
void addSkill(byte &v, byte amount);
void addCoins(byte amount);
bool spendCoins(byte amount);
const __FlashStringHelper* plantName(byte plant);
byte gardenGrowTime(byte plant);
byte gardenProgress();
bool gardenReady();
byte plantVisualStage(byte plant, byte progress, byte growTicks);
byte plantIngredientType(byte plant);
const __FlashStringHelper* plantBenefitText(byte plant);
void drawPlantSprite(byte plant, int cx, int baseY, byte visualStage, bool wilted);
void drawPlantProblemIcon(byte problem, int x, int y);
const __FlashStringHelper* mealName(byte meal);
byte getMealCount(byte meal);
void addMeal(byte meal);
bool useMeal(byte meal);
void startMealBuff(byte type, byte ticks, byte guardTicks);
const __FlashStringHelper* conditionName();
const __FlashStringHelper* conditionShort();
void setCondition(byte t, byte sev);
void clearCondition();
void reduceCondition();
void wrongMedicineEffect();
byte* medCountPtr(byte medId);
const __FlashStringHelper* medName(byte medId);
const __FlashStringHelper* medUseText(byte medId);
void applyMedicine(byte medId);
void tickConditionEffects();
void maybeStartRandomCondition();
void tickMealBuff();
void markSaveDirty();
void saveGameNow();
bool loadGameSave();
void maybeAutoSave(bool forceNow);
void clampAll();
unsigned int petAgeDays();
const __FlashStringHelper* stageName();
const __FlashStringHelper* temperamentName();
void changeBond(int amount);
void changeTrust(int amount);
void checkBondGifts();
void rewardBirthdayIfRemembered();
byte countCareWeekBits(byte bits);
void configureDailyGoal(unsigned long day);
void configureWeeklyGoal(unsigned long week);
void rewardDailyGoal();
void rewardWeeklyGoal();
void recordGoalEvent(byte eventType, byte amount);
void markRoutineAction(byte routineBit);
void finishRoutineDay();
void updateRoutineCalendar();
byte dominantHobby();
byte chooseAdultEvolution();
void showEvolutionMessage();
void updateGrowthStage();
unsigned int naturalLifespanDays();
void archiveCurrentPet();
void endPetLife(byte reason);
byte bcdToDec(byte value);
byte decToBcd(byte value);
bool rtcLeapYear(unsigned int year);
byte rtcDaysInMonth(unsigned int year, byte month);
unsigned long calendarToEpoch(unsigned int year, byte month, byte day, byte hour, byte minute, byte second);
byte compileMonthNumber();
bool readExternalRtcEpoch(unsigned long &epoch);
void setExternalRtcToCompileTime();
void initExternalRtc();
unsigned long currentClockEpoch();
void updateBatteryReading(bool force);
unsigned long compileDateEpoch();
byte clockWeekday();
byte clockMinute();
const __FlashStringHelper* weekdayName(byte day);
unsigned long promptForManualClock(unsigned long savedEpoch);
void setClockMenu();
byte bedtimeHour();
byte wakeHour();
bool isScheduledSleepTime();
bool isScheduledSchoolTime();
bool isScheduledWorkTime();
bool isScheduledAwayTime();
const __FlashStringHelper* scheduledPlaceName();
void synchronizeDailySchedule(bool announce);
void applyManualOfflineSchedule(unsigned long savedEpoch, unsigned long bootEpoch);
void drawScheduledAwayScene();
void drawClock12Hour(byte hour24, byte minute, bool includeSeconds=false);
bool batteryLow();
bool batteryCritical();
void applyBrightness();
const __FlashStringHelper* speciesName();
byte petLineage();
byte currentEvolutionPath();
void lockTeenEvolutionPath();
bool needsCare();
void waitButtonRelease();
bool pressed(byte pin);
bool rightHoldExitCheck();
void popup(const __FlashStringHelper *line1, const __FlashStringHelper *line2, int hold);
void drawFoodIcon(int x, int y, bool inv);
void drawLightIcon(int x, int y, bool inv);
void drawGameIcon(int x, int y, bool inv);
void drawMedIcon(int x, int y, bool inv);
void drawCleanIcon(int x, int y, bool inv);
void drawLoveIcon(int x, int y, bool inv);
void drawCallIcon(int x, int y, bool inv);
void drawWalkIcon(int x, int y, bool inv);
void drawStatsIcon(int x, int y, bool inv);
void drawTrainIcon(int x, int y, bool inv);
void drawStudyIcon(int x, int y, bool inv);
void drawGardenIcon(int x, int y, bool inv);
void drawPlantRoomIcon(int x, int y, bool inv);
void drawMusicIcon(int x, int y, bool inv);
void drawShopIcon(int x, int y, bool inv);
void drawToyIcon(int x, int y, bool inv);
void drawExploreIcon(int x, int y, bool inv);
void drawCookIcon(int x, int y, bool inv);
void drawJobIcon(int x, int y, bool inv);
void drawRoomIcon(int x, int y, bool inv);
void drawTalkIcon(int x, int y, bool inv);
void drawAquariumIcon(int x, int y, bool inv);
void drawIcon(byte id, int x, int y, bool inv);
byte categoryIcon(byte category);
void drawBatteryIcon(int x, int y);
void drawBatteryIcon(int x, int y);
void drawMenu();
void drawMenuName();
void drawMaskSprite(int x, int y, const uint32_t rows[32], byte scale);
void drawSprite32(byte id, int x, int y);
void drawSprite16(byte id, int x, int y);
byte favoriteFoodType();
const __FlashStringHelper* favoriteFoodName();
const __FlashStringHelper* petDialogue();
const __FlashStringHelper* petDreamLine();
void applySpeciesBirthBonus();
const __FlashStringHelper* favoriteThing();
const __FlashStringHelper* dislikedThing();
void drawGrowthSprite(int x, int y, byte sc, bool sleepEyes);
void drawCurrentPetMini(int x, int y, byte scale);
void drawCurrentPetTiny(int x, int y);
void drawCurrentPetSleepMini(int x, int y, byte scale);
void drawCurrentPetSleepTiny(int x, int y);
void drawCurrentPetGameBadge(int x, int y);
void drawBigMenuEmoji(byte id, int x, int y, bool inv);
const __FlashStringHelper* menuShortTitle(byte id);
const __FlashStringHelper* menuTitle(byte id);
void drawRosterFace(int x, int y, byte sc);
void drawAdultExtra(int x, int y, byte sc);
byte tinyHash(byte a, byte b);
byte statBirthRoll(byte low, byte high);
void rollEggMenu();
void rollPetGenes(byte chosenEgg);
void applyBirthStats();
void drawEggShape(int x, int y, byte type, bool selected);
void chooseEgg();
void drawSparkles();
void drawPoopPile(int x, int y);
void drawPoops();
void drawClearEyes(int cx, int y, int bob, byte style, bool onWhiteBody);
void drawClearMouth(int cx, int y, int bob, byte style, bool onWhiteBody);
void drawSpeciesPattern(int x, int y, int bob, byte w, byte h, bool onWhiteBody);
void spriteFace(int cx, int cy, byte mood, byte style);
void spritePattern(int x, int y, byte w, byte h);
void drawSpriteFoot(int x, int y);
void spriteTinyArms(int x, int y, byte w, byte h);
void drawFace(int x, int y, int bob, byte w);
void drawTraits(int x, int y, int bob, byte w, byte h);
void drawDeadPet();
void drawSleepPet();
void drawScheduledAwayScene();
void drawRoomBack();
void drawTopArc(int cx, int cy, int rx, int ry, uint16_t col);
const __FlashStringHelper* outfitName(byte id);
const __FlashStringHelper* outfitRequirementText(byte id);
void drawOutfitBackdrop(byte id, int x, int y, int bob, byte w, byte h);
void drawOutfitOnPet(int x, int y, int bob, byte w, byte h);
const __FlashStringHelper* roomName(byte id);
byte roomCost(byte id);
byte visualHour();
byte visualWeather();
void drawWeatherWindow(int x, int y, int w, int h);
void drawRoomDecor();
void drawNeedBubble();
void synchronizeDailySchedule(bool announce);
void drawPet();
void drawCareAlerts();
void drawBar(int x, int y, byte val, const __FlashStringHelper *lab);
void drawStats();
void animFeed();
void animBath();
void animLove();
void animMed();
void animLight();
void animCallBubble(const __FlashStringHelper *msg);
void drawFoodChoice(byte foodId, int x, int y, bool sel);
void drawStar(int x, int y);
void drawMiniPet();
void showGameResult(byte score, byte goal);
void rewardGameScore(byte score, byte goal, bool smartGame, bool fitGame);
bool gameBackCheck();
void drawRightText(const char *txt, byte y);
void drawRightScore(unsigned int a, unsigned int b, byte y);
void drawRightNumber(unsigned int a, byte y);
void starCatchGame();
void drawRPSChoice(byte choice, int x, int y, bool inv);
int rpsWinner(byte player, byte cpu);
void rpsGame();
void numberGame();
bool tbCell(byte shape, byte rot, byte cx, byte cy);
bool tbCollision(bool board[12][8], int px, int py, byte shape, byte rot);
void tbPlace(bool board[12][8], int px, int py, byte shape, byte rot);
byte tbClearLines(bool board[12][8]);
void tbDrawBlockCell(int x, int y, bool active);
void tbDrawBoard(bool board[12][8], int px, int py, byte shape, byte rot, byte nextShape, unsigned int score, byte lines);
void tinyBlocksGame();
byte mineCountAround(byte mines[5], byte cx, byte cy);
bool mineProtected(byte sx, byte sy, byte x, byte y);
void revealMineOpening(byte mines[5], byte revealed[5], byte sx, byte sy);
void minesweeperGame();
void lookGuessGame();
void memoryGame();
void sprintGame();
void brickBreakerGame();
void meteorPunchGame();
void doodleJumpGame();
void drawBurgerChoice(byte kind, int x, int y, bool selected);
void burgerGame();
void drawMatchCardFace(byte value, int x, int y, uint16_t col);
void drawMatchBoard(byte cards[12], uint16_t matchedMask, uint16_t faceUpMask, byte cursor, byte matches, byte mistakes, bool previewMode);
void slidePuzzleGame();
const __FlashStringHelper* gameName(byte gameId);
byte availableGameCount();
byte dailyFeaturedGame(byte slot);
int featuredGameSlot(byte gameId);
void finishGameSession(byte gameId);
void launchGame(byte gameId);
void drawGamePreviewIcon(byte gameId, int x, int y);
void showGameIntro(byte gameId);
void miniGameMenu();
void drawCar(int x, int y, bool truck);
bool hitCar(int petX, int petY, int carX, int laneY, int carW);
byte walkHash(unsigned int n);
byte walkTerrain(unsigned int row);
int walkCarX(unsigned int row, unsigned long now, byte level, byte lane);
void drawWalkRow(unsigned int row, int y, byte level, unsigned long now);
bool walkCollision(unsigned int row, int px, int py, byte level, unsigned long now);
void walkGame();
void trainGame();
void studyGame();
void gardenActivity();
void musicGame();
void drawShopItem(byte id, int x, int y, bool sel);
byte outfitCost(byte id);
bool outfitSkillOK(byte id);
void shopClothes();
void toyActivity();
const __FlashStringHelper* biomeName(byte loc);
void drawBiomeScene(byte loc, byte frame);
void exploreTrip();
bool useIngredient(byte recipe);
void cookActivity();
void jobActivity();
void roomActivity();
void aquariumActivity();
byte speciesPersonalityType();
const __FlashStringHelper* personalityName();
const __FlashStringHelper* ownerNickname();
const __FlashStringHelper* speciesSecretLine();
const __FlashStringHelper* petMoodTreeLine();
const __FlashStringHelper* petJokeReply();
const __FlashStringHelper* petPraiseReply();
const __FlashStringHelper* petAdviceReply();
void dialoguePopup(const __FlashStringHelper* youLine, const __FlashStringHelper* petLine,
                   byte talkTopic = 255);
void dialoguePopupText(const char *youLine, const char *petLine, byte talkTopic = 255);
TamachibiMemoryContext makeTamachibiMemoryContext();
ParentDialogueContext makeParentDialogueContext();
GrowingTogetherContext makeGrowingTogetherContext();
void applyParentDialogueOutcome(const ParentDialogueOutcome &outcome);
void runParentDialogueCheckIn(bool manual);
void applyGrowingTogetherOutcome(const GrowingTogetherOutcome &outcome);
void runGrowingTogetherCheckIn(bool manual);
void recordGrowingAction(uint8_t action);
void talkActivity();
void statusPage();
byte categoryActionCount(byte category);
byte categoryActionAt(byte category, byte index);
bool actionUnlocked(byte action);
const __FlashStringHelper* unlockHint(byte action);
byte characterSetIndex(char c);
void namePetActivity();
void albumPage();
void batteryPage();
bool confirmChoice(const __FlashStringHelper *title, const __FlashStringHelper *warning);
void sitterModePage();
void startReplacementEgg();
void settingsMenu();
void categoryMenu(byte category);
void startNewPet();
void drawFoodMenuIcon(byte pick, int x, int y);
void foodMenu();
void doFood();
void doLight();
void doMedicine();
void doClean();
void doLove();
void doCall();
byte recentCareCount(byte eventType, byte lookbackDays = 1);
byte careReplyVariant(byte count);
const __FlashStringHelper* careFoodReply(byte pick, bool favorite, bool disliked,
                                         bool overfed, bool hungryBefore, byte repeats);
const __FlashStringHelper* careCleanReply(bool needed, bool messy, byte repeats);
const __FlashStringHelper* careMedicineReply(byte medId, bool correct, bool recovered,
                                             byte repeats);
const __FlashStringHelper* careComfortReply(byte mode, bool personalityMatch, byte repeats);
const __FlashStringHelper* careCallReply(byte reason);
const __FlashStringHelper* careSleepReply(bool waking);
const __FlashStringHelper* careFoodDetail(byte pick, bool favorite, bool disliked,
                                          bool overfed, bool hungryBefore, byte repeats);
const __FlashStringHelper* careComfortDetail(byte mode, byte repeats);
void careChoiceAnimation(byte mode);
void executeAction(byte action);
void recordCareMistake();
void createPetCallIfNeeded();
void gameTick();
void buttonWakeISR();
void processElapsedSeconds(unsigned long elapsedSeconds);
void serviceLifeClock();
void forceDisplayOn();
void turnOledOffIdle();
void enterScreenSleep();
void wakeScreen();
bool anyButtonDown();
void noteInput();
void updateButtons();
void splash();
void setup();
void loop();


// v21 additional prototypes
unsigned long difficultyTicks(unsigned long); unsigned long growthThreshold(unsigned long); byte adjustedGardenGrowTime(byte); const __FlashStringHelper* difficultyName();
void discoverSpecies(byte); bool speciesDiscovered(byte); byte countBits16(unsigned int); byte countDiscoveredSpecies(); void logCareEvent(byte); const __FlashStringHelper* careEventName(byte); const __FlashStringHelper* habitatName(byte); const __FlashStringHelper* materialName(byte); void addMaterial(byte,byte); void updateLearnedFoodPreferences(byte); void updateDevelopedHobby(); byte currentMonthFromEpoch(); byte currentSeasonEvent(); const __FlashStringHelper* seasonEventName(byte);
void drawHabitatAccent(); const __FlashStringHelper* evolutionHint(byte); const __FlashStringHelper* speciesNameById(byte); const __FlashStringHelper* craftRecipeName(byte); bool craftCanMake(byte); void applyCraftReward(byte); void craftPage(); void habitatPage(); byte runSeasonEvent(byte,bool); void seasonalEventPage(); const __FlashStringHelper* partnerName(byte); byte partnerCompatibility(byte); void familyPage(); void diaryPage(); void careHistoryPage(); void inspectPage(); void tamaBookPage(); void diagnosticsPage();
byte planetLevel(); void paradiseLabPage(); void maybeScheduleParadiseEmergency(); void applyParadiseDailyEffects(); const __FlashStringHelper *weatherName(byte);
void developerLab();
void developerPetMenu();
void developerNeedsMenu();
void developerTimeMenu();
void developerUnlockMenu();
void developerFamilyMenu();
void developerConditionMenu();
void developerAquariumMenu();
void developerBotanyMenu();
void developerCityMenu();
void developerGenesMenu();
void developerSaveMenu();
void developerFullSaveReset();
void openSaveProfiles();
bool lifeResidentAvailable(byte resident, byte hour);
const __FlashStringHelper* lifeResidentLocation(byte resident, byte hour);
void lifeRecordNeighborInteraction(byte resident, byte interaction, byte relation);
byte lifeResidentStoryChapter(byte resident);
byte lifeResidentInteractionCount(byte resident);
bool lifeResidentInvitePending(byte resident);
void lifeSetResidentInvitePending(byte resident, bool pending);
void developerCommit(bool saveNow = false);
bool developerEditNumber(const __FlashStringHelper *title, long &value, long minimum, long maximum, long step);
void developerApplyStage(byte newStage);
void developerSelectStage();
void developerSelectSpecies();
void developerSelectTemperament();
void developerSelectSpeed();
void developerSelectPartner();
void developerStartNextGeneration();
void developerUnlockEverything();

// ---------- forward declarations ----------
void drawScreen(bool doDisplay) {
  if(screenSleeping)return;
  NativeUI::Scope frame(display);
  display.clearDisplay();
  if(menuVisible)drawMenu();
  else if(dialWorldView==VIEW_HOUSE)drawHouseWorldView();
  else if(dialWorldView==VIEW_CITY)drawCityWorldView();
  else drawSquarePet();
  if(doDisplay)display.display();
}


void stopSoftBuzzer() {
  esp32BuzzerStop(BUZZER_PIN);
  buzzerStopAt = 0;
}

void startSoftBuzzer(unsigned int freq, unsigned int durationMs) {
  if (soundMode == 0 || batteryCritical()) {
    stopSoftBuzzer();
    return;
  }
  freq = constrain(freq, 120U, 2400U);
  // Lower duty is gentler than tone()'s fixed 50% square wave on a passive piezo.
  esp32BuzzerPlay(BUZZER_PIN, freq, soundMode == 1 ? 8 : 18);
  buzzerStopAt = millis() + max(6U, durationMs);
}

void serviceSoftBuzzer() {
  if (buzzerStopAt != 0 && (long)(millis() - buzzerStopAt) >= 0) stopSoftBuzzer();
}

void petTone(unsigned int freq, unsigned int durationMs) {
  startSoftBuzzer(freq, soundMode == 1 ? max(6U, durationMs / 2U) : durationMs);
}

void snd(unsigned int freq, byte dur) {
  if (soundMode == 0 || batteryCritical()) return;
  const byte useDur = soundMode == 1 ? max((byte)7, (byte)(dur / 2)) : dur;
  startSoftBuzzer(freq, useDur);
  delay(useDur);
  stopSoftBuzzer();
  delay(12);
}

void clickSound() { startSoftBuzzer(1050, soundMode == 1 ? 7 : 12); }
void stepSound() { startSoftBuzzer(520, soundMode == 1 ? 8 : 15); }
void goodSound() { snd(660, 32); snd(880, 44); }
void badSound() { snd(300, 45); snd(220, 55); }
void winSound() { snd(523, 34); snd(659, 38); snd(784, 58); }
void careSound() { snd(520, 28); snd(660, 36); }


void addStat(byte &v, byte amount) {
  v = (v + amount > 10) ? 10 : v + amount;
}

void subStat(byte &v, byte amount) {
  v = (v > amount) ? v - amount : 0;
}

void addSkill(byte &v, byte amount) {
  v = (v + amount > 99) ? 99 : v + amount;
}

void addCoins(byte amount) {
  deepLife.recordEconomy(amount, true);
  coins += amount;
  if (coins > 999) coins = 999;
}

bool spendCoins(byte amount) {
  if (coins < amount) return false;
  deepLife.recordEconomy(amount, false);
  coins -= amount;
  return true;
}

const __FlashStringHelper* plantName(byte plant) {
  switch (plant % PLANT_COUNT) {
    case 0: return F("Berry");
    case 1: return F("Herb");
    case 2: return F("Veg");
    case 3: return F("Spice");
    case 4: return F("Sunflower");
    case 5: return F("Mushroom");
    case 6: return F("Clover");
    case 7: return F("Hibiscus");
    case 8: return F("Hydrangea");
    case 9: return F("Pine");
    case 10: return F("Crystal");
    default: return F("Glowbud");
  }
}


byte gardenGrowTime(byte plant) {
  switch (plant % PLANT_COUNT) {
    case 0: return GROW_BERRY_TICKS;
    case 1: return GROW_HERB_TICKS;
    case 2: return GROW_VEG_TICKS;
    case 3: return GROW_SPICE_TICKS;
    case 4: return GROW_FLOWER_TICKS;
    case 5: return 5;
    case 6: return 4;
    case 7: return 6;
    case 8: return 7;
    case 9: return 9;
    case 10: return 10;
    default: return 12;
  }
}


byte gardenProgress() {
  if (gardenPlant == 255) return 0;
  unsigned long elapsed = ageTicks - gardenStartTick;
  if (elapsed > 250) elapsed = 250;
  return (byte)elapsed;
}

bool gardenReady() {
  return gardenPlant != 255 && gardenProgress() >= gardenGrowTicks;
}


byte plantVisualStage(byte plant, byte progress, byte growTicks) {
  if (growTicks == 0) return 0;
  if (progress == 0) return 0;
  if (progress * 4UL < growTicks) return 1;
  if (progress * 2UL < growTicks) return 2;
  if (progress < growTicks) return 3;
  return 4;
}

byte plantIngredientType(byte plant) {
  switch (plant % PLANT_COUNT) {
    case 0: case 7: return 0; // berry-like
    case 1: case 5: case 6: case 8: return 1; // herb-like
    case 2: case 9: return 2; // vegetable-like
    case 3: case 10: case 11: return 3; // spice/rare
    default: return 4; // decorative flower
  }
}

const __FlashStringHelper* plantBenefitText(byte plant) {
  switch (plant % PLANT_COUNT) {
    case 0: return F("sweet cooking");
    case 1: return F("soups + care");
    case 2: return F("healthy meals");
    case 3: return F("rare recipes");
    case 4: return F("mood + style");
    case 5: return F("herb + spice");
    case 6: return F("luck + coins");
    case 7: return F("bond + style");
    case 8: return F("clean + calm");
    case 9: return F("health + veg");
    case 10: return F("coins + spice");
    default: return F("all ingredients");
  }
}

void drawPlantSprite(byte plant, int cx, int baseY, byte visualStage, bool wilted) {
  plant %= PLANT_COUNT;
  display.drawLine(cx - 14, baseY, cx + 14, baseY, WHITE);

  if (visualStage == 0) {
    display.fillCircle(cx, baseY - 2, 2, WHITE);
    display.drawPixel(cx - 4, baseY - 1, WHITE);
    return;
  }

  int topY = baseY - 5 - visualStage * 5;
  if (wilted) topY += 4;
  display.drawLine(cx, baseY - 1, cx, topY + 3, WHITE);

  if (visualStage >= 1) {
    int leafY = baseY - 7;
    display.drawLine(cx, leafY, cx - 6, leafY - (wilted ? -2 : 4), WHITE);
    display.drawLine(cx, leafY - 2, cx + 6, leafY - (wilted ? 0 : 5), WHITE);
  }
  if (visualStage >= 2) {
    int leafY = baseY - 13;
    display.drawLine(cx, leafY, cx - 7, leafY - (wilted ? -3 : 3), WHITE);
    display.drawLine(cx, leafY - 1, cx + 7, leafY - (wilted ? -1 : 4), WHITE);
  }
  if (visualStage < 3) return;

  int y = topY;
  if (plant == 0) { // berry bush
    display.drawCircle(cx, y, 6, WHITE);
    display.fillCircle(cx - 4, y + 1, 1, WHITE);
    display.fillCircle(cx + 4, y - 1, 1, WHITE);
    if (visualStage >= 4) display.fillCircle(cx, y + 4, 1, WHITE);
  } else if (plant == 1) { // herb
    display.drawLine(cx, y + 7, cx - 8, y, WHITE);
    display.drawLine(cx, y + 7, cx + 8, y + 1, WHITE);
    display.drawCircle(cx - 8, y, 3, WHITE);
    display.drawCircle(cx + 8, y + 1, 3, WHITE);
  } else if (plant == 2) { // vegetable
    display.drawCircle(cx, y + 3, 6, WHITE);
    display.drawLine(cx, y - 2, cx - 5, y - 7, WHITE);
    display.drawLine(cx, y - 2, cx + 5, y - 7, WHITE);
  } else if (plant == 3) { // spice pepper
    display.drawTriangle(cx - 7, y - 2, cx + 7, y - 2, cx + 2, y + 11, WHITE);
    display.drawLine(cx, y - 3, cx + 4, y - 7, WHITE);
  } else if (plant == 4) { // sunflower
    display.drawCircle(cx, y, 3, WHITE);
    for (byte a = 0; a < 8; a++) {
      int dx = (a == 0 || a == 4) ? 0 : (a < 4 ? 5 : -5);
      int dy = (a == 2 || a == 6) ? 0 : (a == 0 || a == 1 || a == 7 ? -5 : 5);
      display.drawCircle(cx + dx, y + dy, 2, WHITE);
    }
  } else if (plant == 5) { // mushroom
    display.fillRoundRect(cx - 9, y - 2, 18, 7, 4, WHITE);
    display.drawRect(cx - 3, y + 4, 6, 8, WHITE);
    display.drawPixel(cx - 5, y + 1, BLACK);
    display.drawPixel(cx + 4, y, BLACK);
  } else if (plant == 6) { // clover
    display.drawCircle(cx - 4, y, 4, WHITE);
    display.drawCircle(cx + 4, y, 4, WHITE);
    display.drawCircle(cx, y - 5, 4, WHITE);
    display.drawCircle(cx, y + 5, 4, WHITE);
  } else if (plant == 7) { // hibiscus
    display.drawCircle(cx, y, 3, WHITE);
    display.drawCircle(cx - 6, y, 4, WHITE);
    display.drawCircle(cx + 6, y, 4, WHITE);
    display.drawCircle(cx, y - 6, 4, WHITE);
    display.drawCircle(cx, y + 6, 4, WHITE);
    display.drawLine(cx + 2, y, cx + 10, y - 6, WHITE);
  } else if (plant == 8) { // hydrangea cluster
    display.drawCircle(cx - 5, y, 4, WHITE);
    display.drawCircle(cx + 4, y - 2, 4, WHITE);
    display.drawCircle(cx, y + 5, 4, WHITE);
    display.drawPixel(cx - 5, y, WHITE);
    display.drawPixel(cx + 4, y - 2, WHITE);
  } else if (plant == 9) { // pine
    display.drawTriangle(cx, y - 9, cx - 8, y + 3, cx + 8, y + 3, WHITE);
    display.drawTriangle(cx, y - 2, cx - 10, y + 10, cx + 10, y + 10, WHITE);
  } else if (plant == 10) { // crystal flower
    display.drawTriangle(cx, y - 9, cx - 5, y + 4, cx + 1, y + 2, WHITE);
    display.drawTriangle(cx + 1, y - 6, cx, y + 5, cx + 7, y + 3, WHITE);
    display.drawPixel(cx - 9, y - 6, WHITE);
    display.drawPixel(cx + 9, y - 8, WHITE);
  } else { // glowbud
    display.drawCircle(cx, y, 7, WHITE);
    display.drawCircle(cx, y, 3, WHITE);
    display.drawPixel(cx - 10, y, WHITE);
    display.drawPixel(cx + 10, y, WHITE);
    display.drawPixel(cx, y - 10, WHITE);
    display.drawPixel(cx, y + 10, WHITE);
  }

  if (wilted) {
    display.drawLine(cx - 10, y - 8, cx + 10, y + 10, WHITE);
    display.drawLine(cx + 10, y - 8, cx - 10, y + 10, WHITE);
  }
}

void drawPlantProblemIcon(byte problem, int x, int y) {
  if (problem == 0) { // bug
    display.drawCircle(x + 5, y + 5, 3, WHITE);
    display.drawLine(x + 2, y + 2, x, y, WHITE);
    display.drawLine(x + 8, y + 2, x + 10, y, WHITE);
  } else if (problem == 1) { // weeds
    display.drawLine(x + 5, y + 10, x + 5, y + 2, WHITE);
    display.drawLine(x + 5, y + 6, x, y + 2, WHITE);
    display.drawLine(x + 5, y + 7, x + 10, y + 3, WHITE);
  } else if (problem == 2) { // rock
    display.drawTriangle(x, y + 10, x + 4, y + 2, x + 10, y + 10, WHITE);
  } else if (problem == 3) { // bird
    display.drawLine(x, y + 5, x + 4, y + 2, WHITE);
    display.drawLine(x + 4, y + 2, x + 8, y + 5, WHITE);
  } else { // wilt/sun
    display.drawCircle(x + 5, y + 4, 3, WHITE);
    display.drawLine(x + 5, y + 8, x + 5, y + 11, WHITE);
  }
}

const __FlashStringHelper* mealName(byte meal) {
  if (meal == 0) return F("Berry bite");
  if (meal == 1) return F("Herb soup");
  if (meal == 2) return F("Veg plate");
  if (meal == 3) return F("Spicy jam");
  return F("Royal stew");
}

byte getMealCount(byte meal) {
  if (meal == 0) return mealBerryBite;
  if (meal == 1) return mealHerbSoup;
  if (meal == 2) return mealVegPlate;
  if (meal == 3) return mealSpicyJam;
  return mealRoyalStew;
}

void addMeal(byte meal) {
  if (meal == 0 && mealBerryBite < 9) mealBerryBite++;
  else if (meal == 1 && mealHerbSoup < 9) mealHerbSoup++;
  else if (meal == 2 && mealVegPlate < 9) mealVegPlate++;
  else if (meal == 3 && mealSpicyJam < 9) mealSpicyJam++;
  else if (meal == 4 && mealRoyalStew < 9) mealRoyalStew++;
}

bool useMeal(byte meal) {
  if (meal == 0 && mealBerryBite > 0) { mealBerryBite--; return true; }
  if (meal == 1 && mealHerbSoup > 0) { mealHerbSoup--; return true; }
  if (meal == 2 && mealVegPlate > 0) { mealVegPlate--; return true; }
  if (meal == 3 && mealSpicyJam > 0) { mealSpicyJam--; return true; }
  if (meal == 4 && mealRoyalStew > 0) { mealRoyalStew--; return true; }
  return false;
}

void startMealBuff(byte type, byte ticks, byte guardTicks) {
  mealBuffType = type;
  if (mealBuffTicks < ticks) mealBuffTicks = ticks;
  if (foodGuardTicks < guardTicks) foodGuardTicks = guardTicks;
}


const __FlashStringHelper* conditionName() {
  if (conditionType == 1) return F("Cold");
  if (conditionType == 2) return F("Tummy ache");
  if (conditionType == 3) return F("Rash");
  if (conditionType == 4) return F("Exhausted");
  if (conditionType == 5) return F("Gloomy");
  if (conditionType == 6) return F("Fever");
  return F("None");
}

const __FlashStringHelper* conditionShort() {
  if (conditionType == 1) return F("Cold");
  if (conditionType == 2) return F("Belly");
  if (conditionType == 3) return F("Rash");
  if (conditionType == 4) return F("Tired");
  if (conditionType == 5) return F("Gloom");
  if (conditionType == 6) return F("Fever");
  return F("OK");
}

void setCondition(byte t, byte sev) {
  if (t == 0) return;
  bool newIllness = conditionType == 0;
  if (conditionType == 0 || sev >= conditionSeverity || random(0, 3) == 0) {
    conditionType = t;
    conditionSeverity = sev;
    conditionTicks = 0;
  }
  sick = true;
  if (newIllness && sicknessHistory < 255) sicknessHistory++;
  saveDirty = true;
}

void clearCondition() {
  conditionType = 0;
  conditionSeverity = 0;
  conditionTicks = 0;
  sick = false;
  saveDirty = true;
}

void reduceCondition() {
  if (conditionType == 0) return;
  if (conditionSeverity > 1) conditionSeverity--;
  else clearCondition();
  saveDirty = true;
}

void popup(const __FlashStringHelper *line1, const __FlashStringHelper *line2=NULL, int hold=650);

void wrongMedicineEffect() {
  subStat(happy, 1);
  if (random(0, 5) == 0) setCondition(2, 1); // upset stomach from wrong meds sometimes
  badSound();
}

byte* medCountPtr(byte medId) {
  if (medId == 0) return &medCold;
  if (medId == 1) return &medTummy;
  if (medId == 2) return &medSkin;
  if (medId == 3) return &medEnergy;
  if (medId == 4) return &medMood;
  return &medAll;
}

const __FlashStringHelper* medName(byte medId) {
  if (medId == 0) return F("Cold Syrup");
  if (medId == 1) return F("Tummy Drops");
  if (medId == 2) return F("Skin Cream");
  if (medId == 3) return F("Energy Tonic");
  if (medId == 4) return F("Comfort Tea");
  return F("All Cure");
}

const __FlashStringHelper* medUseText(byte medId) {
  if (medId == 0) return F("Cold/fever");
  if (medId == 1) return F("Belly ache");
  if (medId == 2) return F("Rash/dirty");
  if (medId == 3) return F("Exhaustion");
  if (medId == 4) return F("Gloom/mood");
  return F("Any illness");
}

void applyMedicine(byte medId) {
  if (conditionType == 0 && !sick && health >= 8 && energy >= 4 && happy >= 4 && clean >= 4) {
    popup(F("Already healthy"), F("No medicine needed"), 700);
    return;
  }

  bool wasSick = sick || conditionType != 0;
  byte repeats = recentCareCount(CARE_EVENT_MED_BASE + medId, 1);
  byte *stock = medCountPtr(medId);
  bool boughtDose = false;

  if (*stock == 0) {
    const byte price = MEDICINE_PRICE - lifeWorld.projectLevel(2);
    if (coins >= price) {
      spendCoins(price);
      *stock = 1;
      boughtDose = true;
    } else {
      badSound();
      popup(F("No medicine"), F("Need coins"));
      return;
    }
  }

  (*stock)--;
  animMed();

  bool correct = false;

  if (medId == 0) { // cold syrup
    if (conditionType == 1 || conditionType == 6) correct = true;
    addStat(health, 1);
  } else if (medId == 1) { // tummy drops
    if (conditionType == 2) correct = true;
    addStat(full, 1);
  } else if (medId == 2) { // skin cream
    if (conditionType == 3 || clean <= 3) correct = true;
    addStat(clean, 3);
  } else if (medId == 3) { // energy tonic
    if (conditionType == 4 || energy <= 2) correct = true;
    addStat(energy, 4);
    subStat(full, 1);
  } else if (medId == 4) { // comfort tea
    if (conditionType == 5 || happy <= 3) correct = true;
    addStat(happy, 4);
    addStat(discipline, 1);
  } else { // all cure
    correct = conditionType != 0 || sick || health <= 4;
    addStat(health, 3);
    addStat(happy, 1);
  }

  bool recovered = false;
  if (medId == 5 && correct) {
    clearCondition();
    sick = false;
    winSound();
  } else if (correct) {
    if (conditionType != 0) reduceCondition();
    if (conditionType == 0) sick = false;
    addStat(health, 1);
    goodSound();
  } else {
    wrongMedicineEffect();
  }

  recovered = wasSick && conditionType == 0 && !sick;
  if (correct) {
    completeGameplayAction(M_MED);
    markRoutineAction(ROUTINE_CARE);
    recordGoalEvent(0, 1);
    changeTrust(2);
    if (wasSick && conditionType == 0) {
      if (rescuedFromSickness < 255) rescuedFromSickness++;
      zeroSickTicks = 0; sickMistakeOpen = false;
      changeBond(2);
    }
  }
  logCareEvent(CARE_EVENT_MED_BASE + medId);
  popup(careMedicineReply(medId, correct, recovered, repeats),
        boughtDose ? F("Bought and used") : medName(medId), 750);
  clampAll();
  saveDirty = true;
}

void tickConditionEffects() {
  if (conditionType == 0) {
    sick = false;
    conditionTicks = 0;
    return;
  }

  sick = true;
  conditionTicks++;

  if (conditionType == 1) { // cold
    if (!sleeping && conditionTicks % 24 == 0) subStat(energy, 1);
  } else if (conditionType == 2) { // tummy ache
    if (conditionTicks % 24 == 0) subStat(full, 1);
    if (conditionTicks % 36 == 0) subStat(happy, 1);
  } else if (conditionType == 3) { // rash
    if (conditionTicks % 24 == 0) subStat(clean, 1);
    if (conditionTicks % 36 == 0) subStat(happy, 1);
  } else if (conditionType == 4) { // exhaustion
    if (!sleeping && conditionTicks % 24 == 0) subStat(happy, 1);
    if (!sleeping && conditionTicks % 36 == 0) subStat(energy, 1);
  } else if (conditionType == 5) { // gloom/stress
    if (conditionTicks % 24 == 0) subStat(happy, 1);
    if (conditionTicks % 72 == 0 && discipline > 0) subStat(discipline, 1);
  } else if (conditionType == 6) { // fever
    if (conditionTicks % 18 == 0) subStat(energy, 1);
    if (conditionTicks % 24 == 0) subStat(happy, 1);
  }

  if (sleeping && conditionTicks % SLEEP_CONDITION_HEAL_EVERY_TICKS == 0 && conditionType != 6) reduceCondition();
  if ((mealBuffType == 2 || mealBuffType == 5) && conditionTicks % 24 == 0) reduceCondition();
}

void maybeStartRandomCondition() {
  if (conditionType != 0 || sleeping || dead || stage < STAGE_CHILD || sitterTicksRemaining > 0) return;
  if (ageTicks % ILLNESS_CHECK_EVERY_TICKS != 0) return;

  byte risk = RANDOM_ILLNESS_CHANCE;
  if (clean <= 3) risk = 8;
  if (full == 0 || happy <= 2 || energy <= 1) risk = 10;
  if (mealBuffType == 2 || mealBuffType == 5) risk += 8; // soup/stew protect
  if (discipline >= 8) risk += 4;

  if (random(0, risk) != 0) return;

  byte t;
  if (clean <= 2) t = 3;
  else if (full == 0) t = 2;
  else if (energy <= 1) t = 4;
  else if (happy <= 2) t = 5;
  else t = random(0, 2) ? 1 : 6;

  setCondition(t, 1 + random(0, 3));
  careSound();
}

void tickMealBuff() {
  if (foodGuardTicks > 0) foodGuardTicks--;

  if (mealBuffTicks == 0) {
    mealBuffType = 0;
    return;
  }

  mealBuffTicks--;

  // Slow drip benefits while the meal is still active.
  if (ageTicks % 6 == 0) {
    if (mealBuffType == 1) addStat(happy, 1);             // berry bite comfort
    else if (mealBuffType == 2) { addStat(health, 1); if (sick && random(0, 2) == 0) sick = false; }
    else if (mealBuffType == 3) { addStat(full, 1); addStat(clean, 1); }
    else if (mealBuffType == 4) addStat(happy, 2);
    else if (mealBuffType == 5) { addStat(full, 1); addStat(health, 1); addStat(happy, 1); }
  }
}



unsigned long calculateExtrasChecksum(const SaveExtras &d) {
  const byte *raw = reinterpret_cast<const byte*>(&d);
  unsigned long hash = 2166136261UL;
  for (size_t i = 0; i < offsetof(SaveExtras, checksum); i++) {
    hash ^= raw[i];
    hash *= 16777619UL;
  }
  return hash;
}

bool extrasAreValid(const SaveExtras &d) {
  return d.magic == EXTRAS_MAGIC && d.version == EXTRAS_VERSION && d.checksum == calculateExtrasChecksum(d);
}

void fillSaveExtras(SaveExtras &d) {
  memset(&d, 0, sizeof(d));
  d.magic = EXTRAS_MAGIC;
  d.version = EXTRAS_VERSION;
  d.sequence = extrasSequence + 1UL;
  d.difficultyMode = difficultyMode; d.rescueMode = rescueMode; d.habitat = habitat;
  d.habitatOwned = habitatOwned;
  d.discoveredSpeciesLo = discoveredSpeciesLo; d.discoveredSpeciesHi = discoveredSpeciesHi;
  d.discoveredPlants = discoveredPlants; d.visitedLocations = visitedLocations; d.recipesCrafted = recipesCrafted;
  d.discoveredOutfits = discoveredOutfits | outfitOwned; d.discoveredRooms = discoveredRooms | roomOwned;
  memcpy(d.craftMaterials, craftMaterials, sizeof(craftMaterials));
  memcpy(d.foodExperience, foodExperience, sizeof(foodExperience));
  memcpy(d.foodBoredom, foodBoredom, sizeof(foodBoredom));
  d.learnedFavoriteFood = learnedFavoriteFood; d.learnedDislikedFood = learnedDislikedFood; d.favoritePlace = favoritePlace;
  d.partnerId = partnerId; d.partnerBond = partnerBond; d.familyReady = familyReady; d.inheritedTrait = inheritedTrait;
  d.inheritedTemperament = inheritedTemperament; d.inheritedFavoriteGame = inheritedFavoriteGame; d.inheritedBodyType = inheritedBodyType;
  d.petAway = petAway; d.awayTicks = awayTicks; d.runawayCount = runawayCount;
  memcpy(d.careHistoryType, careHistoryType, sizeof(careHistoryType));
  memcpy(d.careHistoryDay, careHistoryDay, sizeof(careHistoryDay));
  memcpy(d.careHistoryHour, careHistoryHour, sizeof(careHistoryHour));
  d.careHistoryHead = careHistoryHead; d.careHistoryCount = careHistoryCount;
  d.seasonalEventBest = seasonalEventBest; d.seasonalEventPlayedDay = seasonalEventPlayedDay; d.seasonalRewardMask = seasonalRewardMask;
  memcpy(d.albumPartner, albumPartner, sizeof(albumPartner));
  memcpy(d.albumGeneration, albumGeneration, sizeof(albumGeneration));
  d.checksum = calculateExtrasChecksum(d);
}

void applySaveExtras(const SaveExtras &d) {
  extrasSequence = d.sequence;
  difficultyMode = min((byte)3, d.difficultyMode); rescueMode = d.rescueMode ? 1 : 0; habitat = d.habitat % HABITAT_COUNT;
  habitatOwned = d.habitatOwned | 1U;
  discoveredSpeciesLo = d.discoveredSpeciesLo; discoveredSpeciesHi = d.discoveredSpeciesHi;
  discoveredPlants = d.discoveredPlants; visitedLocations = d.visitedLocations; recipesCrafted = d.recipesCrafted;
  discoveredOutfits = d.discoveredOutfits | outfitOwned | 1UL; discoveredRooms = d.discoveredRooms | roomOwned | 1UL;
  memcpy(craftMaterials, d.craftMaterials, sizeof(craftMaterials));
  memcpy(foodExperience, d.foodExperience, sizeof(foodExperience));
  memcpy(foodBoredom, d.foodBoredom, sizeof(foodBoredom));
  learnedFavoriteFood = d.learnedFavoriteFood < FOOD_COUNT ? d.learnedFavoriteFood : 255;
  learnedDislikedFood = d.learnedDislikedFood < FOOD_COUNT ? d.learnedDislikedFood : 255;
  favoritePlace = d.favoritePlace % 9;
  partnerId = d.partnerId < PARTNER_COUNT ? d.partnerId : 255; partnerBond = d.partnerBond; familyReady = d.familyReady; inheritedTrait = d.inheritedTrait;
  inheritedTemperament = d.inheritedTemperament; inheritedFavoriteGame = d.inheritedFavoriteGame; inheritedBodyType = d.inheritedBodyType;
  petAway = d.petAway; awayTicks = d.awayTicks; runawayCount = d.runawayCount;
  memcpy(careHistoryType, d.careHistoryType, sizeof(careHistoryType));
  memcpy(careHistoryDay, d.careHistoryDay, sizeof(careHistoryDay));
  memcpy(careHistoryHour, d.careHistoryHour, sizeof(careHistoryHour));
  careHistoryHead = d.careHistoryHead % CARE_HISTORY_COUNT; careHistoryCount = min((byte)CARE_HISTORY_COUNT, d.careHistoryCount);
  seasonalEventBest = d.seasonalEventBest; seasonalEventPlayedDay = d.seasonalEventPlayedDay; seasonalRewardMask = d.seasonalRewardMask;
  memcpy(albumPartner, d.albumPartner, sizeof(albumPartner));
  memcpy(albumGeneration, d.albumGeneration, sizeof(albumGeneration));
}

void saveExtrasNow() {
  discoveredOutfits |= outfitOwned; discoveredRooms |= roomOwned;
  SaveExtras d;
  fillSaveExtras(d);
  if (d.sequence & 1UL) extraStoreA.write(d); else extraStoreB.write(d);
  extrasSequence = d.sequence;
}

bool loadExtrasSave() {
  SaveExtras a = extraStoreA.read();
  SaveExtras b = extraStoreB.read();
  bool va = extrasAreValid(a), vb = extrasAreValid(b);
  if (!va && !vb) return false;
  if (va && vb) applySaveExtras((a.sequence >= b.sequence) ? a : b);
  else applySaveExtras(va ? a : b);
  return true;
}

void markSaveDirty() {
  saveDirty = true;
}

unsigned long calculateSaveChecksum(const SaveData &d) {
  const byte *raw = reinterpret_cast<const byte*>(&d);
  unsigned long hash = 2166136261UL; // FNV-1a, compact corruption check
  for (size_t i = 0; i < offsetof(SaveData, checksum); i++) {
    hash ^= raw[i];
    hash *= 16777619UL;
  }
  return hash;
}

bool saveIsValid(const SaveData &d) {
  if (d.magic != SAVE_MAGIC ||
      (d.version != SAVE_VERSION && d.version != SAVE_VERSION - 1)) return false;
  return d.checksum == calculateSaveChecksum(d);
}

void fillSaveData(SaveData &d) {
  memset(&d, 0, sizeof(d));
  d.magic = SAVE_MAGIC;
  d.version = SAVE_VERSION;
  d.sequence = saveSequence + 1UL;
  d.menu = menu;
  d.mainCategory = mainCategory;
  d.eggType = eggType; d.geneA = geneA; d.geneB = geneB; d.geneC = geneC;
  d.bodyType = bodyType; d.earType = earType; d.eyeType = eyeType; d.mouthType = mouthType; d.tailType = tailType;
  d.headTrait = headTrait; d.patternType = patternType; d.idleStyle = idleStyle; d.birthMark = birthMark;
  d.speciesType = speciesType; d.statBias = statBias; d.rareAllRounder = rareAllRounder; d.birthCode = birthCode;
  d.health = health; d.full = full; d.energy = energy; d.happy = happy; d.clean = clean; d.discipline = discipline;
  d.sleeping = sleeping; d.sick = sick; d.dead = dead; d.lightOff = lightOff;
  d.conditionType = conditionType; d.conditionSeverity = conditionSeverity; d.conditionTicks = conditionTicks;
  d.medCold = medCold; d.medTummy = medTummy; d.medSkin = medSkin; d.medEnergy = medEnergy; d.medMood = medMood; d.medAll = medAll;
  d.poop = poop; d.ageTicks = ageTicks; d.stage = stage; d.careMistakes = careMistakes; d.stageCareMistakes = stageCareMistakes;
  d.lastFood = lastFood; d.varietyStreak = varietyStreak; d.walkWins = walkWins; d.crossyBest = crossyBest;
  d.studyStars = studyStars; d.gardenLevel = gardenLevel; d.musicStars = musicStars; d.trainStars = trainStars; d.gardenHarvests = gardenHarvests;
  d.coins = coins;
  d.skillSmart = skillSmart; d.skillStyle = skillStyle; d.skillKind = skillKind; d.skillFit = skillFit;
  d.jobRank = jobRank; d.outfit = outfit; d.roomStyle = roomStyle; d.toyItem = toyItem; d.personality = personality;
  d.mischief = mischief; d.friendCount = friendCount; d.tripBadges = tripBadges; d.cookedMeals = cookedMeals;
  d.berryInv = berryInv; d.herbInv = herbInv; d.vegInv = vegInv; d.spiceInv = spiceInv;
  d.mealBerryBite = mealBerryBite; d.mealHerbSoup = mealHerbSoup; d.mealVegPlate = mealVegPlate; d.mealSpicyJam = mealSpicyJam; d.mealRoyalStew = mealRoyalStew;
  d.mealBuffType = mealBuffType; d.mealBuffTicks = mealBuffTicks; d.foodGuardTicks = foodGuardTicks;
  d.gardenPlant = gardenPlant; d.gardenStartTick = gardenStartTick; d.gardenGrowTicks = gardenGrowTicks;
  d.gardenWater = gardenWater; d.gardenSun = gardenSun; d.gardenCare = gardenCare; d.gardenProblem = gardenProblem; d.gardenQuality = gardenQuality;
  d.sleepTicks = sleepTicks; d.outfitOwned = outfitOwned; d.roomOwned = roomOwned;

  d.ownerBond = ownerBond; d.ownerTrust = ownerTrust; d.temperament = temperament;
  memcpy(d.petName, petName, sizeof(petName));
  d.lastCareTick = lastCareTick; d.lastTalkTick = lastTalkTick;
  d.dailyRoutineMask = dailyRoutineMask; d.weeklyCareBits = weeklyCareBits; d.caredDayStreak = caredDayStreak; d.missedCareDays = missedCareDays;
  d.routineDay = routineDay;
  d.lastTalkChoice = lastTalkChoice; d.lastEventId = lastEventId; d.favoriteGivenFood = favoriteGivenFood; d.ignoredCallCount = ignoredCallCount;
  d.rescuedFromSickness = rescuedFromSickness; d.lastTripEvent = lastTripEvent; d.hobbyType = hobbyType;
  d.favoriteGameId = favoriteGameId; d.dislikedGameId = dislikedGameId; d.activeGameId = activeGameId;
  d.lastGameBondTick = lastGameBondTick; d.featuredPlayedMask = featuredPlayedMask; d.featuredDay = featuredDay;
  memcpy(d.gameCounts, gameCounts, sizeof(gameCounts));
  d.nutritionScore = nutritionScore; d.cleanlinessScore = cleanlinessScore; d.exerciseScore = exerciseScore;
  d.sicknessHistory = sicknessHistory; d.overfeedCount = overfeedCount; d.praiseCount = praiseCount;
  d.giftMilestones = giftMilestones; d.weight = weight; d.pendingCallType = pendingCallType;
  d.pendingCallTicks = pendingCallTicks; d.lastBirthdayRewardDay = lastBirthdayRewardDay;
  d.dailyGoalType = dailyGoalType; d.dailyGoalProgress = dailyGoalProgress; d.dailyGoalTarget = dailyGoalTarget;
  d.weeklyGoalType = weeklyGoalType; d.weeklyGoalProgress = weeklyGoalProgress; d.weeklyGoalTarget = weeklyGoalTarget;
  d.goalDay = goalDay; d.goalWeek = goalWeek; d.sitterTicksRemaining = sitterTicksRemaining; d.sitterCooldownTicks = sitterCooldownTicks;
  d.soundMode = soundMode; d.brightnessLevel = brightnessLevel; d.deathReason = deathReason; d.archivedCurrent = archivedCurrent;
  d.generation = generation; d.albumCount = albumCount;
  memcpy(d.albumSpecies, albumSpecies, sizeof(albumSpecies));
  memcpy(d.albumAge, albumAge, sizeof(albumAge));
  memcpy(d.albumBond, albumBond, sizeof(albumBond));
  memcpy(d.albumReason, albumReason, sizeof(albumReason));
  memcpy(d.albumName, albumName, sizeof(albumName));
  d.legacySpecies = legacySpecies; d.legacySkill = legacySkill; d.legacyOutfit = legacyOutfit;
  d.foodMistakeOpen = foodMistakeOpen; d.cleanMistakeOpen = cleanMistakeOpen; d.happyMistakeOpen = happyMistakeOpen; d.sickMistakeOpen = sickMistakeOpen;
  d.lightMistakeOpen = lightMistakeOpen; d.callMistakeOpen = callMistakeOpen;
  d.zeroFoodTicks = zeroFoodTicks; d.zeroCleanTicks = zeroCleanTicks; d.zeroHappyTicks = zeroHappyTicks; d.zeroSickTicks = zeroSickTicks;
  d.wrongLightTicks = wrongLightTicks; d.criticalNeglectTicks = criticalNeglectTicks; d.lastRtcEpoch = currentClockEpoch();
  d.checksum = calculateSaveChecksum(d);
}

void applySaveData(const SaveData &d) {
  gameplayProgress.reset();
  saveSequence = d.sequence;
  menu = d.menu; mainCategory = d.mainCategory % CATEGORY_COUNT;
  eggType = d.eggType; geneA = d.geneA; geneB = d.geneB; geneC = d.geneC;
  bodyType = d.bodyType; earType = d.earType; eyeType = d.eyeType; mouthType = d.mouthType; tailType = d.tailType;
  headTrait = d.headTrait; patternType = d.patternType; idleStyle = d.idleStyle; birthMark = d.birthMark;
  speciesType = d.speciesType % SPECIES_COUNT; statBias = d.statBias; rareAllRounder = d.rareAllRounder; birthCode = d.birthCode;
  health = d.health; full = d.full; energy = d.energy; happy = d.happy; clean = d.clean; discipline = d.discipline;
  sleeping = d.sleeping; sick = d.sick; dead = d.dead; lightOff = d.lightOff;
  conditionType = d.conditionType; conditionSeverity = d.conditionSeverity; conditionTicks = d.conditionTicks;
  medCold = d.medCold; medTummy = d.medTummy; medSkin = d.medSkin; medEnergy = d.medEnergy; medMood = d.medMood; medAll = d.medAll;
  poop = d.poop; ageTicks = d.ageTicks; stage = d.stage; careMistakes = d.careMistakes; stageCareMistakes = d.stageCareMistakes;
  if (d.version < SAVE_VERSION) {
    // v19 stored an adult species id in both fields before hatch. Convert the
    // old choice to its new family while preserving an already-grown adult.
    const byte family = growthFamilyForAdult(speciesType);
    eggType = family;
    if (stage < STAGE_ADULT) speciesType = family;
    markSaveDirty();
  }
  if (stage == STAGE_TEEN) lockTeenEvolutionPath();
  lastFood = d.lastFood; varietyStreak = d.varietyStreak; walkWins = d.walkWins; crossyBest = d.crossyBest;
  studyStars = d.studyStars; gardenLevel = d.gardenLevel; musicStars = d.musicStars; trainStars = d.trainStars; gardenHarvests = d.gardenHarvests;
  coins = d.coins;
  skillSmart = d.skillSmart; skillStyle = d.skillStyle; skillKind = d.skillKind; skillFit = d.skillFit;
  jobRank = d.jobRank; outfit = d.outfit; roomStyle = d.roomStyle; toyItem = d.toyItem; personality = d.personality;
  mischief = d.mischief; friendCount = d.friendCount; tripBadges = d.tripBadges; cookedMeals = d.cookedMeals;
  berryInv = d.berryInv; herbInv = d.herbInv; vegInv = d.vegInv; spiceInv = d.spiceInv;
  mealBerryBite = d.mealBerryBite; mealHerbSoup = d.mealHerbSoup; mealVegPlate = d.mealVegPlate; mealSpicyJam = d.mealSpicyJam; mealRoyalStew = d.mealRoyalStew;
  mealBuffType = d.mealBuffType; mealBuffTicks = d.mealBuffTicks; foodGuardTicks = d.foodGuardTicks;
  gardenPlant = d.gardenPlant; gardenStartTick = d.gardenStartTick; gardenGrowTicks = d.gardenGrowTicks;
  gardenWater = d.gardenWater; gardenSun = d.gardenSun; gardenCare = d.gardenCare; gardenProblem = d.gardenProblem; gardenQuality = d.gardenQuality;
  sleepTicks = d.sleepTicks; outfitOwned = d.outfitOwned; roomOwned = d.roomOwned;

  ownerBond = d.ownerBond; ownerTrust = d.ownerTrust; temperament = d.temperament % 8;
  memcpy(petName, d.petName, sizeof(petName)); petName[6] = '\0';
  lastCareTick = d.lastCareTick; lastTalkTick = d.lastTalkTick;
  dailyRoutineMask = d.dailyRoutineMask; weeklyCareBits = d.weeklyCareBits & 0x7F; caredDayStreak = d.caredDayStreak; missedCareDays = d.missedCareDays;
  routineDay = d.routineDay;
  lastTalkChoice = d.lastTalkChoice; lastEventId = d.lastEventId; favoriteGivenFood = d.favoriteGivenFood; ignoredCallCount = d.ignoredCallCount;
  rescuedFromSickness = d.rescuedFromSickness; lastTripEvent = d.lastTripEvent; hobbyType = d.hobbyType;
  favoriteGameId = d.favoriteGameId % 13; dislikedGameId = d.dislikedGameId % 13; activeGameId = 255;
  lastGameBondTick = d.lastGameBondTick; featuredPlayedMask = d.featuredPlayedMask; featuredDay = d.featuredDay;
  memcpy(gameCounts, d.gameCounts, sizeof(gameCounts));
  nutritionScore = d.nutritionScore; cleanlinessScore = d.cleanlinessScore; exerciseScore = d.exerciseScore;
  sicknessHistory = d.sicknessHistory; overfeedCount = d.overfeedCount; praiseCount = d.praiseCount;
  giftMilestones = d.giftMilestones; weight = d.weight; pendingCallType = d.pendingCallType;
  pendingCallTicks = d.pendingCallTicks; lastBirthdayRewardDay = d.lastBirthdayRewardDay;
  dailyGoalType = d.dailyGoalType; dailyGoalProgress = d.dailyGoalProgress; dailyGoalTarget = max((byte)1, d.dailyGoalTarget);
  weeklyGoalType = d.weeklyGoalType; weeklyGoalProgress = d.weeklyGoalProgress; weeklyGoalTarget = max((byte)1, d.weeklyGoalTarget);
  goalDay = d.goalDay; goalWeek = d.goalWeek; sitterTicksRemaining = d.sitterTicksRemaining; sitterCooldownTicks = d.sitterCooldownTicks;
  soundMode = min((byte)2, d.soundMode); brightnessLevel = min((byte)2, d.brightnessLevel);
  deathReason = d.deathReason; archivedCurrent = d.archivedCurrent; generation = max((unsigned int)1, d.generation); albumCount = min((byte)8, d.albumCount);
  memcpy(albumSpecies, d.albumSpecies, sizeof(albumSpecies)); memcpy(albumAge, d.albumAge, sizeof(albumAge));
  memcpy(albumBond, d.albumBond, sizeof(albumBond)); memcpy(albumReason, d.albumReason, sizeof(albumReason)); memcpy(albumName, d.albumName, sizeof(albumName));
  legacySpecies = d.legacySpecies; legacySkill = d.legacySkill; legacyOutfit = d.legacyOutfit;
  foodMistakeOpen = d.foodMistakeOpen; cleanMistakeOpen = d.cleanMistakeOpen; happyMistakeOpen = d.happyMistakeOpen; sickMistakeOpen = d.sickMistakeOpen;
  lightMistakeOpen = d.lightMistakeOpen; callMistakeOpen = d.callMistakeOpen;
  zeroFoodTicks = d.zeroFoodTicks; zeroCleanTicks = d.zeroCleanTicks; zeroHappyTicks = d.zeroHappyTicks; zeroSickTicks = d.zeroSickTicks;
  wrongLightTicks = d.wrongLightTicks; criticalNeglectTicks = d.criticalNeglectTicks;
  lastRtcEpoch = d.lastRtcEpoch;
  menuVisible = false;
  clampAll();
}

void saveGameNow() {
  SaveData d;
  fillSaveData(d);
  if (d.sequence & 1UL) saveStoreA.write(d);
  else saveStoreB.write(d);
  saveSequence = d.sequence;
  saveExtrasNow();
  aquarium.saveNow();
  botany.saveNow();
  cityWorld.saveNow();
  paradise.saveNow();
  lifeWorld.saveNow();
  deepLife.saveNow();
  parentDialogue.saveNow();
  growingTogether.saveNow();
  saveDirty = false;
  lastSaveAt = millis();
}

bool loadGameSave() {
  SaveData a = saveStoreA.read();
  SaveData b = saveStoreB.read();
  bool validA = saveIsValid(a);
  bool validB = saveIsValid(b);
  if (!validA && !validB) return false;

  if (validA && validB) applySaveData((a.sequence >= b.sequence) ? a : b);
  else applySaveData(validA ? a : b);
  loadExtrasSave();
  aquarium.load();
  botany.load();
  cityWorld.load();
  paradise.load();
  lifeWorld.load();
  deepLife.load();
  parentDialogue.load();
  growingTogether.load();
  return true;
}

void maybeAutoSave(bool forceNow=false) {
  if (forceNow) {
    saveGameNow();
    return;
  }
  if (saveDirty && millis() - lastSaveAt >= AUTO_SAVE_MS) saveGameNow();
}

void clampAll() {
  if (health > 10) health = 10;
  if (full > 10) full = 10;
  if (energy > 10) energy = 10;
  if (happy > 10) happy = 10;
  if (clean > 10) clean = 10;
  if (discipline > 10) discipline = 10;
  if (ownerBond > 100) ownerBond = 100;
  if (ownerTrust > 100) ownerTrust = 100;
}

unsigned int petAgeDays() {
  unsigned long days = ageTicks / TICKS_PER_DAY;
  return (days > 999UL) ? 999 : (unsigned int)days;
}

const __FlashStringHelper* stageName() {
  if (stage == STAGE_EGG) return F("Egg");
  if (stage == STAGE_BABY) return F("Baby");
  if (stage == STAGE_CHILD) return F("Child");
  if (stage == STAGE_TEEN) return F("Teen");
  if (stage == STAGE_ADULT) return F("Adult");
  return F("Elder");
}

byte petLineage() {
  // V19 saves are migrated on load. Live saves keep the family key in eggType.
  if (stage < STAGE_ADULT) return speciesType % TAMACHIBI_FAMILY_COUNT;
  return eggType % TAMACHIBI_FAMILY_COUNT;
}

uint8_t naturalActivity() {
  return GameplayProgress::interest(petLineage(), temperament, birthCode);
}

byte currentEvolutionPath() {
  return (idleStyle & 0x80U) ? 1 : 0;
}

void lockTeenEvolutionPath() {
  if (idleStyle & 0x40U) return;
  const bool careChallenged = careMistakes >= 3 || discipline <= 3;
  idleStyle = (idleStyle & 0x3FU) | 0x40U | (careChallenged ? 0x80U : 0U);
}

const __FlashStringHelper* temperamentName() {
  switch (temperament % 8) {
    case 0: return F("Shy");
    case 1: return F("Energetic");
    case 2: return F("Curious");
    case 3: return F("Affectionate");
    case 4: return F("Mischievous");
    case 5: return F("Calm");
    case 6: return F("Foodie");
    default: return F("Independent");
  }
}

void checkBondGifts();
void recordGoalEvent(byte eventType, byte amount = 1);

void changeBond(int amount) {
  int value = (int)ownerBond + amount;
  if (value < 0) value = 0;
  if (value > 100) value = 100;
  ownerBond = (byte)value;
  checkBondGifts();
}

void changeTrust(int amount) {
  int value = (int)ownerTrust + amount;
  if (value < 0) value = 0;
  if (value > 100) value = 100;
  ownerTrust = (byte)value;
}

void checkBondGifts() {
  const byte marks[4] = {30, 50, 70, 90};
  for (byte i = 0; i < 4; i++) {
    if (ownerBond >= marks[i] && !(giftMilestones & (1 << i))) {
      giftMilestones |= (1 << i);
      addCoins(5 + i * 3);
      if (i == 1) outfitOwned |= (1UL << ((birthCode % (OUTFIT_COUNT - 1)) + 1));
      if (i == 2) roomOwned |= (1UL << ((birthCode % (ROOM_STYLE_COUNT - 1)) + 1));
      if (i == 3 && medAll < 9) medAll++;
      if (!rtcCatchUpMode) popup(F("Friendship gift!"), F("A surprise for you"), 800);
    }
  }
}

void rewardBirthdayIfRemembered() {
  unsigned long day = petAgeDays();
  if (day == 0 || day % 7UL != 0 || lastBirthdayRewardDay == day) return;
  lastBirthdayRewardDay = day;
  changeBond(3); changeTrust(2); addCoins(7);
  if (!rtcCatchUpMode) popup(F("Happy birthday!"), F("You remembered"), 900);
}

byte countCareWeekBits(byte bits) {
  byte count = 0;
  for (byte i = 0; i < 7; i++) if (bits & (1 << i)) count++;
  return count;
}

void configureDailyGoal(unsigned long day) {
  dailyGoalType = (byte)((birthCode + day + temperament) % 4UL);
  dailyGoalTarget = 1;
  dailyGoalProgress = 0;
  goalDay = day;
}

void configureWeeklyGoal(unsigned long week) {
  weeklyGoalType = (byte)((birthCode + week + temperament * 3UL) % 4UL);
  weeklyGoalTarget = (weeklyGoalType == 1) ? 5 : ((weeklyGoalType == 2) ? 3 : ((weeklyGoalType == 3) ? 3 : 5));
  weeklyGoalProgress = 0;
  goalWeek = week;
}

void rewardDailyGoal() {
  addCoins(3);
  changeBond(1);
  goodSound();
  if (!rtcCatchUpMode) popup(F("Daily goal!"), F("Bond + coin"), 650);
}

void rewardWeeklyGoal() {
  addCoins(10);
  changeBond(3);
  changeTrust(2);
  winSound();
  if (!rtcCatchUpMode) popup(F("Weekly goal!"), F("Great teamwork"), 750);
}

void recordGoalEvent(byte eventType, byte amount) {
  if (dailyGoalType == eventType && dailyGoalProgress < dailyGoalTarget) {
    byte old = dailyGoalProgress;
    dailyGoalProgress += amount;
    if (dailyGoalProgress > dailyGoalTarget) dailyGoalProgress = dailyGoalTarget;
    if (old < dailyGoalTarget && dailyGoalProgress >= dailyGoalTarget) rewardDailyGoal();
  }

  bool weeklyMatch = false;
  if (weeklyGoalType == 0 && eventType == 2) weeklyMatch = true; // games
  if (weeklyGoalType == 2 && eventType == 4) weeklyMatch = true; // cooking
  if (weeklyGoalType == 3 && eventType == 5) weeklyMatch = true; // exploring
  if (weeklyMatch && weeklyGoalProgress < weeklyGoalTarget) {
    byte old = weeklyGoalProgress;
    weeklyGoalProgress += amount;
    if (weeklyGoalProgress > weeklyGoalTarget) weeklyGoalProgress = weeklyGoalTarget;
    if (old < weeklyGoalTarget && weeklyGoalProgress >= weeklyGoalTarget) rewardWeeklyGoal();
  }
  markSaveDirty();
}

void markRoutineAction(byte routineBit) {
  byte before = dailyRoutineMask;
  dailyRoutineMask |= routineBit;
  if (before != dailyRoutineMask) {
    if (routineBit == ROUTINE_CARE) lastCareTick = ageTicks;
    if (routineBit == ROUTINE_TALK) lastTalkTick = ageTicks;
    markSaveDirty();
  }
  // Routine flags describe the day; completed activities own narrative credit.
  if (routineBit == ROUTINE_BED && gameplayProgress.claimRoutine(routineBit, ageTicks, generation))
    recordGrowingAction(GTA_BED);
}

void finishRoutineDay() {
  byte completed = 0;
  if (dailyRoutineMask & ROUTINE_GREET) completed++;
  if (dailyRoutineMask & ROUTINE_CHECK) completed++;
  if (dailyRoutineMask & ROUTINE_CARE) completed++;
  if (dailyRoutineMask & ROUTINE_TALK) completed++;
  if (dailyRoutineMask & ROUTINE_PLAY) completed++;
  bool complete = completed >= 4;
  weeklyCareBits = ((weeklyCareBits << 1) | (complete ? 1 : 0)) & 0x7F;

  if (complete) {
    if (caredDayStreak < 99) caredDayStreak++;
    missedCareDays = 0;
    changeBond(2);
    changeTrust(1);
    if (weeklyGoalType == 1 && weeklyGoalProgress < weeklyGoalTarget) {
      byte old = weeklyGoalProgress++;
      if (weeklyGoalProgress > weeklyGoalTarget) weeklyGoalProgress = weeklyGoalTarget;
      if (old < weeklyGoalTarget && weeklyGoalProgress >= weeklyGoalTarget) rewardWeeklyGoal();
    }
  } else {
    caredDayStreak = 0;
    if (missedCareDays < 99) missedCareDays++;
    // One missed day is forgiven. Relationship loss starts after two.
    if (missedCareDays >= 2) changeBond(-1);
  }

  if (((routineDay + 1UL) % 7UL) == 0UL && countCareWeekBits(weeklyCareBits) >= 5) {
    changeBond(3);
    changeTrust(2);
  }

  dailyRoutineMask = 0;
}

void updateRoutineCalendar() {
  unsigned long day = ageTicks / TICKS_PER_DAY;
  if (routineDay > day) routineDay = day;
  byte guard = 0;
  while (routineDay < day && guard < 8) {
    finishRoutineDay();
    routineDay++;
    configureDailyGoal(routineDay);
    if (routineDay > 0 && routineDay % 7UL == 0UL) {
      changeBond(2);
      changeTrust(1);
      addCoins(3);
      if (!rtcCatchUpMode) popup(F("Pet birthday!"), F("A week together"), 750);
    }
    guard++;
  }
  unsigned long week = day / 7UL;
  if (goalDay != day) configureDailyGoal(day);
  if (goalWeek != week) configureWeeklyGoal(week);
}


void discoverSpecies(byte id) {
  id %= SPECIES_COUNT;
  if (id < 32) discoveredSpeciesLo |= (1UL << id);
  else discoveredSpeciesHi |= (1UL << (id - 32));
}

bool speciesDiscovered(byte id) {
  id %= SPECIES_COUNT;
  return id < 32 ? (discoveredSpeciesLo & (1UL << id)) : (discoveredSpeciesHi & (1UL << (id - 32)));
}

byte countBits16(unsigned int value) {
  byte c = 0;
  while (value) { c += value & 1U; value >>= 1; }
  return c;
}

byte countDiscoveredSpecies() {
  byte c = 0;
  for (byte i = 0; i < SPECIES_COUNT; i++) if (speciesDiscovered(i)) c++;
  return c;
}

void logCareEvent(byte type) {
  careHistoryType[careHistoryHead] = type;
  careHistoryDay[careHistoryHead] = (unsigned int)min<unsigned long>(65535UL, static_cast<unsigned long>(petAgeDays()));
  careHistoryHour[careHistoryHead] = (byte)((ageTicks % TICKS_PER_DAY) / TICKS_PER_HOUR);
  careHistoryHead = (careHistoryHead + 1) % CARE_HISTORY_COUNT;
  if (careHistoryCount < CARE_HISTORY_COUNT) careHistoryCount++;
  markSaveDirty();
}

const __FlashStringHelper* careEventName(byte type) {
  if (type == 1) return F("Hungry call");
  if (type == 2) return F("Clean call");
  if (type == 3) return F("Sick call");
  if (type == 4) return F("Wanted company");
  if (type == 5) return F("Birthday call");
  if (type == 6) return F("Call answered");
  if (type == 7) return F("Care mistake");
  if (type == 8) return F("Ran away");
  if (type == 9) return F("Came home");
  if (type == 10) return F("Fed");
  if (type == 11) return F("Medicine");
  if (type == 12) return F("Cleaned up");
  if (type == 13) return F("Affection");
  if (type == 14) return F("Playtime");
  if (type == 15) return F("Explored");
  if (type == CARE_EVENT_FOOD_BASE) return F("Ate a meal");
  if (type == CARE_EVENT_FOOD_BASE + 1) return F("Ate cooked food");
  if (type == CARE_EVENT_FOOD_BASE + 2) return F("Had a snack");
  if (type == CARE_EVENT_FOOD_BASE + 3) return F("Ate healthy food");
  if (type == CARE_EVENT_FOOD_BASE + 4) return F("Got a treat");
  if (type == CARE_EVENT_MED_BASE) return F("Took cold syrup");
  if (type == CARE_EVENT_MED_BASE + 1) return F("Took tummy drops");
  if (type == CARE_EVENT_MED_BASE + 2) return F("Used skin cream");
  if (type == CARE_EVENT_MED_BASE + 3) return F("Took energy tonic");
  if (type == CARE_EVENT_MED_BASE + 4) return F("Had comfort tea");
  if (type == CARE_EVENT_MED_BASE + 5) return F("Took all-cure");
  if (type == CARE_EVENT_COMFORT_BASE) return F("Shared a cuddle");
  if (type == CARE_EVENT_COMFORT_BASE + 1) return F("Heard kind praise");
  if (type == CARE_EVENT_COMFORT_BASE + 2) return F("Sat together");
  return F("Care event");
}

byte recentCareCount(byte eventType, byte lookbackDays) {
  const unsigned int today = static_cast<unsigned int>(
      min<unsigned long>(65535UL, static_cast<unsigned long>(petAgeDays())));
  const byte oldest = (careHistoryHead + CARE_HISTORY_COUNT - careHistoryCount) % CARE_HISTORY_COUNT;
  byte count = 0;
  for (byte i = 0; i < careHistoryCount; ++i) {
    const byte slot = (oldest + i) % CARE_HISTORY_COUNT;
    const unsigned int day = careHistoryDay[slot];
    if (careHistoryType[slot] == eventType && day <= today && today - day <= lookbackDays) {
      if (count < 255) ++count;
    }
  }
  return count;
}

byte careReplyVariant(byte count) {
  if (count == 0) return 0;
  const unsigned long seed = static_cast<unsigned long>(petAgeDays()) * 31UL +
      static_cast<unsigned long>(visualHour()) * 7UL +
      static_cast<unsigned long>(temperament) * 17UL +
      static_cast<unsigned long>(speciesType) * 13UL +
      static_cast<unsigned long>(ownerBond) * 3UL +
      static_cast<unsigned long>(careHistoryHead) * 5UL + dailyRoutineMask;
  return static_cast<byte>(seed % count);
}

const __FlashStringHelper* careFoodReply(byte pick, bool favorite, bool disliked,
                                         bool overfed, bool hungryBefore, byte repeats) {
  const byte variant = careReplyVariant(3);
  if (overfed) {
    if (variant == 0) return F("My tummy is full!");
    if (variant == 1) return F("Tiny bites next time?");
    return F("I need a little break.");
  }
  if (disliked) {
    if (variant == 0) return F("Could we try another?");
    if (variant == 1) return F("I'm tired of this taste.");
    return F("Maybe a different bite?");
  }
  if (favorite) {
    if (variant == 0) return F("You remembered my favorite!");
    if (variant == 1) return F("This one always feels special.");
    return F("You know what I like.");
  }
  if (repeats >= 3) {
    if (variant == 0) return F("This again? I know it well.");
    if (variant == 1) return F("A familiar meal today.");
    return F("Same choice, same smile.");
  }
  if (hungryBefore) {
    if (variant == 0) return F("That hit the spot!");
    if (variant == 1) return F("I really needed that.");
    return F("My tummy says thanks.");
  }

  const byte tone = temperament % 8;
  const byte choice = (tone * 2 + careReplyVariant(2) + pick) % 16;
  switch (choice) {
    case 0: return F("I'll try a little bite.");
    case 1: return F("Thanks for asking me.");
    case 2: return F("Now I have zoomies!");
    case 3: return F("Snack fuel! Let's go!");
    case 4: return F("What does this taste like?");
    case 5: return F("I like trying new things.");
    case 6: return F("Food is nicer with you.");
    case 7: return F("Can we eat together?");
    case 8: return F("I saved the last crumb!");
    case 9: return F("I may have taken two.");
    case 10: return F("A peaceful little meal.");
    case 11: return F("That was just right.");
    case 12: return F("Mmm, lovely aroma.");
    case 13: return F("I notice every flavor.");
    case 14: return F("A good choice. Thank you.");
    default: return F("I can choose for myself!");
  }
}

const __FlashStringHelper* careFoodDetail(byte pick, bool favorite, bool disliked,
                                          bool overfed, bool hungryBefore, byte repeats) {
  if (overfed) return F("I'll pace myself next time");
  if (disliked) return F("Let's change the menu");
  if (favorite) return F("You know what I enjoy");
  if (repeats >= 3) return F("A little variety sounds good");
  if (hungryBefore) return F("That helped a lot");
  if (pick == 1) return F("A warm cooked meal");
  if (pick == 2) return F("A quick little snack");
  if (pick == 3) return F("A nourishing choice");
  if (pick == 4) return F("A special treat today");
  return F("A familiar meal together");
}

const __FlashStringHelper* careCleanReply(bool needed, bool messy, byte repeats) {
  const byte variant = careReplyVariant(3);
  if (!needed) {
    if (repeats >= 2) return variant == 0 ? F("We do love this ritual.") : F("I'm already fresh, though!");
    if (temperament == 0 || temperament == 5 || temperament == 7) return F("A calm little bath sounds nice.");
    return F("Bath time can be fun too!");
  }
  if (repeats >= 3) {
    if (variant == 0) return F("We make a good clean-up team.");
    if (variant == 1) return F("Fresh again, just like that!");
    return F("You always notice the mess.");
  }
  if (messy) {
    if (variant == 0) return F("Whew, that feels better.");
    if (variant == 1) return F("Thanks for helping me reset.");
    return F("I can breathe easier now.");
  }
  switch (temperament % 8) {
    case 0: return F("Thanks for being gentle.");
    case 1: return F("Clean and ready to bounce!");
    case 2: return F("I found a bubble!");
    case 3: return F("I like caring together.");
    case 4: return F("I was totally helping!");
    case 5: return F("That was nice and quiet.");
    case 6: return F("Fresh fur, fresh feeling!");
    default: return F("I could have done it...thanks.");
  }
}

const __FlashStringHelper* careMedicineReply(byte medId, bool correct, bool recovered,
                                             byte repeats) {
  const byte variant = (careReplyVariant(3) + medId) % 3;
  if (!correct) {
    if (variant == 0) return F("That doesn't feel right.");
    if (variant == 1) return F("Could you check my symptoms?");
    return F("Let's find the right remedy.");
  }
  if (recovered) {
    if (variant == 0) return F("I feel like me again!");
    if (variant == 1) return F("You noticed just in time.");
    return F("Thank you for staying close.");
  }
  if (repeats >= 2) {
    if (variant == 0) return F("This dose is becoming familiar.");
    if (variant == 1) return F("I'll rest while it works.");
    return F("You know how to help me.");
  }
  if (temperament == 0) return F("Stay with me a moment?");
  if (temperament == 1) return F("I'll be back on my feet!");
  if (temperament == 2) return F("I wonder how it works.");
  if (temperament == 3) return F("Thank you for caring.");
  if (temperament == 4) return F("No sneaking past you!");
  if (temperament == 5) return F("I'll rest and take it easy.");
  if (temperament == 6) return F("It tastes a little odd.");
  return F("I'll handle this my way.");
}

const __FlashStringHelper* careComfortReply(byte mode, bool personalityMatch, byte repeats) {
  const byte variant = careReplyVariant(2);
  if (repeats >= 2) {
    if (mode == 0) return variant ? F("A familiar hug still helps.") : F("You know my cuddle routine.");
    if (mode == 1) return variant ? F("I remember your kind words.") : F("You always notice my effort.");
    return variant ? F("I like our quiet habit.") : F("We can sit like this again.");
  }
  if (personalityMatch) {
    if (mode == 0) return variant ? F("That hug was just right.") : F("I needed a cuddle from you.");
    if (mode == 1) return variant ? F("I'll remember that praise.") : F("You saw how hard I tried!");
    return variant ? F("This quiet feels safe.") : F("I like having you nearby.");
  }
  const byte choice = (temperament % 8) * 3 + (mode % 3);
  switch (choice) {
    case 0: return F("A slow hug is okay.");
    case 1: return F("I tried very hard!");
    case 2: return F("Let's be quiet a while.");
    case 3: return F("A quick squeeze, then play!");
    case 4: return F("I did something well?");
    case 5: return F("Stay for one more minute.");
    case 6: return F("What a warm feeling.");
    case 7: return F("I'll tell you what I think.");
    case 8: return F("I like being close to you.");
    case 9: return F("You always make me smile.");
    case 10: return F("That made my day softer.");
    case 11: return F("Let's share this moment.");
    case 12: return F("I was going to hug you!");
    case 13: return F("Good job noticing me.");
    case 14: return F("No need to fill the silence.");
    case 15: return F("A hug was my idea, really.");
    case 16: return F("I worked hard for that.");
    case 17: return F("Quiet time is underrated.");
    case 18: return F("I'll accept that cuddle.");
    case 19: return F("I can do even better!");
    case 20: return F("I like your calm company.");
    case 21: return F("Best friend, best hugs.");
    case 22: return F("Praise makes me beam.");
    case 23: return F("Let's just breathe together.");
  }
  return F("I'm glad you're here.");
}

const __FlashStringHelper* careComfortDetail(byte mode, byte repeats) {
  if (repeats >= 2) return F("A familiar kind of care");
  if (mode == 0) return F("A close, warm moment");
  if (mode == 1) return F("Praise builds confidence");
  return F("Quiet company together");
}

const __FlashStringHelper* careCallReply(byte reason) {
  const byte variant = careReplyVariant(3);
  if (reason == 1) {
    if (variant == 0) return F("My tummy's singing again.");
    if (variant == 1) return F("Could we share a little meal?");
    return F("I tried to wait patiently.");
  }
  if (reason == 2) {
    if (variant == 0) return F("I found a bit of a mess.");
    if (variant == 1) return F("Can we tidy up together?");
    return F("I might have tracked it in.");
  }
  if (reason == 3) {
    if (variant == 0) return F("I don't feel quite myself.");
    if (variant == 1) return F("Could you stay close to me?");
    return F("Something feels a little off.");
  }
  if (reason == 4) {
    if (variant == 0) return F("I missed having you nearby.");
    if (variant == 1) return F("Could we share a moment?");
    return F("I have something to show you.");
  }
  if (reason == 5) {
    if (variant == 0) return F("You remembered our day!");
    if (variant == 1) return F("This little milestone is ours.");
    return F("I'm glad we marked today.");
  }
  if (reason == 7) {
    if (variant == 0) return F("Something grew in the garden!");
    if (variant == 1) return F("Come see what we made.");
    return F("I kept checking on it.");
  }
  if (reason == 8) {
    if (variant == 0) return F("That meal is still cozy.");
    if (variant == 1) return F("I can feel the warm energy.");
    return F("I saved some for later.");
  }
  if (reason == 9) {
    if (variant == 0) return F("My eyelids feel so heavy.");
    if (variant == 1) return F("Can we slow things down?");
    return F("I need a quiet corner.");
  }
  if (reason == 10) {
    if (variant == 0) return F("Show me how to do better.");
    if (variant == 1) return F("I want to make you proud.");
    return F("I'm learning, one try at a time.");
  }
  if (reason == 11) {
    if (variant == 0) return F("I might have hidden a thing.");
    if (variant == 1) return F("I have a tiny secret.");
    return F("Can you guess what I did?");
  }
  if (reason == 12) {
    if (variant == 0) return F("Do you like my outfit?");
    if (variant == 1) return F("I picked this look for you.");
    return F("I feel like myself in this.");
  }
  if (reason == 13) {
    if (variant == 0) return F("Should we visit the market?");
    if (variant == 1) return F("I have an idea for a treat.");
    return F("Let's save up for something fun.");
  }
  if (reason == 14) {
    if (variant == 0) return F("Our friends asked about us.");
    if (variant == 1) return F("I wonder who is out today.");
    return F("Let's say hi to someone soon.");
  }
  if (reason == 15) {
    if (variant == 0) return F("The plants need our help.");
    if (variant == 1) return F("I checked the leaves for you.");
    return F("Let's see what the garden needs.");
  }
  switch (temperament % 8) {
    case 0: return F("I wanted to hear your voice.");
    case 1: return F("I have news! Guess what?");
    case 2: return F("I noticed something new today.");
    case 3: return F("I like when you check in.");
    case 4: return F("I wasn't doing anything sneaky.");
    case 5: return F("It's nice just being together.");
    case 6: return F("I have a snack idea.");
    default: return F("Just checking that you're there.");
  }
}

const __FlashStringHelper* careSleepReply(bool waking) {
  const byte variant = careReplyVariant(3);
  if (waking) {
    if (temperament == 1) return F("I'm ready to bounce!");
    if (temperament == 2) return F("I dreamed about exploring.");
    if (temperament == 5) return F("That was a peaceful rest.");
    if (variant == 0) return F("Good morning, my friend.");
    if (variant == 1) return F("I slept better knowing you.");
    return F("A new day with you!");
  }
  if (temperament == 0 || temperament == 3) return F("Stay nearby while I drift off?");
  if (temperament == 4) return F("No midnight mischief. Promise.");
  if (temperament == 5 || temperament == 7) return F("Let's make the room quiet.");
  if (variant == 0) return F("I'll tell you my dream tomorrow.");
  if (variant == 1) return F("Good night. Save me a thought.");
  return F("Rest time already? Okay, then.");
}

const __FlashStringHelper* habitatName(byte id) {
  switch (id % HABITAT_COUNT) {
    case 0: return F("Home Field");
    case 1: return F("Forest Field");
    case 2: return F("Tropical Field");
    case 3: return F("Glacier Field");
    case 4: return F("Town Field");
    case 5: return F("Garden Field");
    case 6: return F("Star Field");
    default: return F("Mountain Field");
  }
}

const __FlashStringHelper* materialName(byte id) {
  switch (id % MATERIAL_COUNT) {
    case 0: return F("Wood");
    case 1: return F("Cloth");
    case 2: return F("Metal");
    case 3: return F("Crystal");
    case 4: return F("Flower");
    default: return F("Shell");
  }
}

void addMaterial(byte id, byte amount=1) {
  id %= MATERIAL_COUNT;
  craftMaterials[id] = min((byte)99, (byte)(craftMaterials[id] + amount));
}

void updateLearnedFoodPreferences(byte food) {
  food %= FOOD_COUNT;
  if (foodExperience[food] < 99) foodExperience[food]++;
  for (byte i = 0; i < FOOD_COUNT; i++) {
    if (i == food) foodBoredom[i] = min((byte)99, (byte)(foodBoredom[i] + 2));
    else if (foodBoredom[i] > 0) foodBoredom[i]--;
  }
  byte best = 0;
  int bestScore = -999;
  for (byte i = 0; i < FOOD_COUNT; i++) {
    int score = (int)foodExperience[i] * 3 - foodBoredom[i];
    if (score > bestScore) { bestScore = score; best = i; }
  }
  if (foodExperience[best] >= 3) learnedFavoriteFood = best;
  if (foodBoredom[food] >= 12) learnedDislikedFood = food;
  else if (learnedDislikedFood == food && foodBoredom[food] < 5) learnedDislikedFood = 255;
}

void updateDevelopedHobby() {
  byte old = hobbyType;
  hobbyType = dominantHobby();
  if (gameCounts[favoriteGameId] >= 5 && (favoriteGameId == 0 || favoriteGameId >= 7)) hobbyType = 1;
  if (gardenHarvests >= 5) hobbyType = 3;
  if (musicStars >= 12) hobbyType = 2;
  if (studyStars >= 12) hobbyType = 0;
  if (old != hobbyType) changeBond(1);
}

byte currentMonthFromEpoch() {
  unsigned long epoch = currentClockEpoch();
  unsigned long days = epoch / 86400UL;
  unsigned int year = 1970;
  while (true) { unsigned int yd = rtcLeapYear(year) ? 366 : 365; if (days < yd) break; days -= yd; year++; }
  byte month = 1;
  while (month <= 12) { byte md = rtcDaysInMonth(year, month); if (days < md) break; days -= md; month++; }
  return min((byte)12, max((byte)1, month));
}

byte currentSeasonEvent() {
  byte m = currentMonthFromEpoch();
  if (m == 12 || m <= 2) return 0; // winter stars
  if (m <= 5) return 1;            // spring garden
  if (m <= 8) return 2;            // summer beach
  return 3;                         // autumn harvest
}

const __FlashStringHelper* seasonEventName(byte e) {
  if (e == 0) return F("Winter Stars");
  if (e == 1) return F("Spring Bloom");
  if (e == 2) return F("Summer Shells");
  return F("Harvest Dash");
}

byte dominantHobby() {
  byte best = skillSmart;
  byte h = 0;
  if (skillFit > best) { best = skillFit; h = 1; }
  if (skillStyle > best) { best = skillStyle; h = 2; }
  if (skillKind > best) { h = 3; }
  return h;
}

byte chooseAdultEvolution() {
  int average = (health + full + energy + happy + clean + discipline) / 6;
  int quality = average * 3 + ownerBond / 5 + ownerTrust / 5 + discipline;
  quality += nutritionScore / 10 + cleanlinessScore / 10 + min((int)exerciseScore, 30) / 6;
  quality += (skillSmart + skillFit + skillKind + skillStyle) / 20;
  quality -= (int)stageCareMistakes * 6;
  quality -= (int)careMistakes * 2 + (int)sicknessHistory + (int)overfeedCount * 2;
  if (currentEvolutionPath() != 0) quality -= 8;

  byte tier;
  if (quality >= 65) tier = 0;
  else if (quality >= 52) tier = 1;
  else if (quality >= 38) tier = 2;
  else if (quality >= 24) tier = 3;
  else tier = 4;

  hobbyType = dominantHobby();
  // One chosen hatch family has five adult silhouettes. Care tier chooses the
  // outcome, so both teen branches can reach several adults from that same
  // earlier form.
  return growthAdultSpecies(petLineage(), tier);
}

void showEvolutionMessage() {
  if (rtcCatchUpMode) return;
  winSound();
  popup(F("It evolved!"), stageName(), 900);
}

void updateGrowthStage() {
  byte wanted = STAGE_EGG;
  if (ageTicks >= growthThreshold(EGG_HATCH_TICKS)) wanted = STAGE_BABY;
  if (ageTicks >= growthThreshold(CHILD_START_TICKS)) wanted = STAGE_CHILD;
  if (ageTicks >= growthThreshold(TEEN_START_TICKS)) wanted = STAGE_TEEN;
  if (ageTicks >= growthThreshold(ADULT_START_TICKS)) wanted = STAGE_ADULT;
  if (ageTicks >= growthThreshold(ELDER_START_TICKS)) wanted = STAGE_ELDER;

  if (wanted == stage) return;
  stage = wanted;
  if (stage == STAGE_TEEN) lockTeenEvolutionPath();
  if (stage == STAGE_ADULT) {
    speciesType = deepLife.applySecretEvolution(chooseAdultEvolution());
    bool learnedGame = false;
    for (byte i = 0; i < 13; ++i) if (gameCounts[i] >= 4) learnedGame = true;
    if (!learnedGame) {
      favoriteGameId = (byte)((speciesType + temperament * 3) % 13);
      dislikedGameId = (byte)((favoriteGameId + 6) % 13);
    }
    discoverSpecies(speciesType);
  }
  stageCareMistakes = 0;
  showEvolutionMessage();
  lifeWorld.recordMemory(1, petAgeDays(), stage);
  markSaveDirty();
}

unsigned int naturalLifespanDays() {
  unsigned int days = 20 + (birthCode % 6); // 20-25 baseline
  days += ownerBond / 25;                   // up to four more days
  days += ownerTrust / 34;                  // up to two more days
  if (careMistakes <= 3) days += 2;
  if (careMistakes >= 12 && days > 4) days -= 4;
  return min((unsigned int)30, days);
}

void archiveCurrentPet() {
  if (archivedCurrent) return;
  byte index;
  if (albumCount < 8) {
    index = albumCount++;
  } else {
    for (byte i = 0; i < 7; i++) {
      albumSpecies[i] = albumSpecies[i + 1];
      albumAge[i] = albumAge[i + 1];
      albumBond[i] = albumBond[i + 1];
      albumReason[i] = albumReason[i + 1];
      albumPartner[i] = albumPartner[i + 1];
      albumGeneration[i] = albumGeneration[i + 1];
      memcpy(albumName[i], albumName[i + 1], 7);
    }
    index = 7;
  }
  albumSpecies[index] = speciesType;
  unsigned int days = petAgeDays();
  albumAge[index] = (days > 255) ? 255 : (byte)days;
  albumBond[index] = ownerBond;
  albumReason[index] = deathReason;
  albumPartner[index] = partnerId;
  albumGeneration[index] = generation;
  memcpy(albumName[index], petName, 7);

  legacySpecies = speciesType;
  legacyOutfit = outfit;
  legacySkill = dominantHobby();
  lifeWorld.archiveGeneration(speciesType, generation, temperament, favoriteGivenFood, albumAge[index]);
  deepLife.archiveGeneration(speciesType, generation, albumAge[index]);
  archivedCurrent = 1;
  markSaveDirty();
}

void endPetLife(byte reason) {
  if (dead) return;
  deathReason = reason;
  dead = true;
  sleeping = false;
  lightOff = false;
  sleepTicks = 0;
  growingTogether.recordLoss(reason, makeGrowingTogetherContext());
  archiveCurrentPet();
  if (!rtcCatchUpMode) { if (reason == 2) winSound(); else badSound(); }
}

byte bcdToDec(byte value) { return (value >> 4) * 10 + (value & 0x0F); }
byte decToBcd(byte value) { return ((value / 10) << 4) | (value % 10); }

bool rtcLeapYear(unsigned int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

byte rtcDaysInMonth(unsigned int year, byte month) {
  static const byte monthDays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (month == 2 && rtcLeapYear(year)) return 29;
  return monthDays[(month - 1) % 12];
}

unsigned long calendarToEpoch(unsigned int year, byte month, byte day, byte hour, byte minute, byte second) {
  unsigned long days = 0;
  for (unsigned int y = 2000; y < year; y++) days += rtcLeapYear(y) ? 366UL : 365UL;
  for (byte m = 1; m < month; m++) days += rtcDaysInMonth(year, m);
  days += day > 0 ? day - 1 : 0;
  return 946684800UL + days * 86400UL + (unsigned long)hour * 3600UL + (unsigned long)minute * 60UL + second;
}

byte compileMonthNumber() {
  const char *dateText = __DATE__;
  const char monthNames[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  for (byte i = 0; i < 12; i++) {
    if (dateText[0] == monthNames[i * 3] && dateText[1] == monthNames[i * 3 + 1] && dateText[2] == monthNames[i * 3 + 2]) return i + 1;
  }
  return 1;
}

bool readExternalRtcEpoch(unsigned long &epoch) {
#if ENABLE_DS3231_RTC
  Wire.beginTransmission(DS3231_ADDR);
  Wire.write((byte)0x00);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(DS3231_ADDR, 7) != 7) return false;
  byte second = bcdToDec(Wire.read() & 0x7F);
  byte minute = bcdToDec(Wire.read() & 0x7F);
  byte hour = bcdToDec(Wire.read() & 0x3F);
  Wire.read();
  byte day = bcdToDec(Wire.read() & 0x3F);
  byte month = bcdToDec(Wire.read() & 0x1F);
  unsigned int year = 2000U + bcdToDec(Wire.read());
  if (month < 1 || month > 12 || day < 1 || day > 31) return false;
  epoch = calendarToEpoch(year, month, day, hour, minute, second);
  return true;
#else
  (void)epoch;
  return false;
#endif
}

void setExternalRtcToCompileTime() {
#if ENABLE_DS3231_RTC
  const char *dateText = __DATE__;
  const char *timeText = __TIME__;
  byte day = (dateText[4] == ' ' ? 0 : dateText[4] - '0') * 10 + (dateText[5] - '0');
  unsigned int year = (dateText[7]-'0')*1000U + (dateText[8]-'0')*100U + (dateText[9]-'0')*10U + (dateText[10]-'0');
  byte hour = (timeText[0]-'0')*10 + (timeText[1]-'0');
  byte minute = (timeText[3]-'0')*10 + (timeText[4]-'0');
  byte second = (timeText[6]-'0')*10 + (timeText[7]-'0');
  Wire.beginTransmission(DS3231_ADDR);
  Wire.write((byte)0x00);
  Wire.write(decToBcd(second)); Wire.write(decToBcd(minute)); Wire.write(decToBcd(hour));
  Wire.write((byte)1); Wire.write(decToBcd(day)); Wire.write(decToBcd(compileMonthNumber())); Wire.write(decToBcd(year - 2000U));
  Wire.endTransmission();
  Wire.beginTransmission(DS3231_ADDR);
  Wire.write((byte)0x0F); Wire.write((byte)0x00);
  Wire.endTransmission();
#endif
}

void initExternalRtc() {
#if ENABLE_DS3231_RTC
  Wire.beginTransmission(DS3231_ADDR);
  Wire.write((byte)0x0F);
  if (Wire.endTransmission(false) == 0 && Wire.requestFrom(DS3231_ADDR, 1) == 1) {
    byte status = Wire.read();
    externalRtcReady = true;
    if (status & 0x80) setExternalRtcToCompileTime();
    unsigned long epoch;
    if (readExternalRtcEpoch(epoch)) rtc.setEpoch(epoch);
  }
#endif
}

unsigned long currentClockEpoch() {
  unsigned long epoch = 0;
  if (externalRtcReady && readExternalRtcEpoch(epoch)) return epoch;
  return rtc.getEpoch();
}

unsigned long compileDateEpoch() {
  const char *dateText = __DATE__;
  const char *timeText = __TIME__;
  byte day = (dateText[4] == ' ' ? 0 : dateText[4] - '0') * 10 + (dateText[5] - '0');
  unsigned int year = (dateText[7]-'0')*1000U + (dateText[8]-'0')*100U + (dateText[9]-'0')*10U + (dateText[10]-'0');
  byte hour = (timeText[0]-'0')*10 + (timeText[1]-'0');
  byte minute = (timeText[3]-'0')*10 + (timeText[4]-'0');
  byte second = (timeText[6]-'0')*10 + (timeText[7]-'0');
  return calendarToEpoch(year, compileMonthNumber(), max((byte)1, day), hour, minute, second);
}

byte clockWeekday() {
  // 0 Monday ... 6 Sunday. Unix day zero was Thursday.
  return static_cast<byte>(((currentClockEpoch() / 86400UL) + 3UL) % 7UL);
}

byte clockMinute() {
  return static_cast<byte>((currentClockEpoch() / 60UL) % 60UL);
}

const __FlashStringHelper* weekdayName(byte day) {
  switch (day % 7) {
    case 0: return F("Mon");
    case 1: return F("Tue");
    case 2: return F("Wed");
    case 3: return F("Thu");
    case 4: return F("Fri");
    case 5: return F("Sat");
    default: return F("Sun");
  }
}

void drawClock12Hour(byte hour24, byte minute, bool includeSeconds) {
  byte hour12 = hour24 % 12;
  if (hour12 == 0) hour12 = 12;
  display.print(hour12);
  display.print(':');
  if (minute < 10) display.print('0');
  display.print(minute);
  if (includeSeconds) {
    byte second = static_cast<byte>(currentClockEpoch() % 60UL);
    display.print(':');
    if (second < 10) display.print('0');
    display.print(second);
  }
  display.print(hour24 < 12 ? F(" AM") : F(" PM"));
}

unsigned long promptForManualClock(unsigned long savedEpoch) {
  unsigned long seedEpoch = savedEpoch >= 946684800UL ? savedEpoch : compileDateEpoch();
  byte selectedDay = static_cast<byte>(((seedEpoch / 86400UL) + 3UL) % 7UL);
  byte hour24 = static_cast<byte>((seedEpoch / 3600UL) % 24UL);
  byte selectedHour = hour24 % 12;
  if (selectedHour == 0) selectedHour = 12;
  byte selectedMinute = static_cast<byte>(((seedEpoch / 60UL) % 60UL) / 5UL * 5UL);
  bool pm = hour24 >= 12;
  byte step = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();

  while (step < 4) {
    bool l = digitalRead(BTN_L), sBtn = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) {
      if (step == 0) selectedDay = selectedDay == 0 ? 6 : selectedDay - 1;
      else if (step == 1) selectedHour = selectedHour <= 1 ? 12 : selectedHour - 1;
      else if (step == 2) selectedMinute = selectedMinute < 5 ? 55 : selectedMinute - 5;
      else pm = !pm;
      clickSound(); delay(105);
    }
    if (oldR == HIGH && r == LOW) {
      if (step == 0) selectedDay = (selectedDay + 1) % 7;
      else if (step == 1) selectedHour = selectedHour >= 12 ? 1 : selectedHour + 1;
      else if (step == 2) selectedMinute = selectedMinute >= 55 ? 0 : selectedMinute + 5;
      else pm = !pm;
      clickSound(); delay(105);
    }
    if (oldS == HIGH && sBtn == LOW) {
      step++;
      clickSound();
      waitButtonRelease();
      oldL = oldS = oldR = HIGH;
      continue;
    }
    oldL = l; oldS = sBtn; oldR = r;

    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(23, 0); display.print(F("SET REAL CLOCK"));
    display.drawLine(0, 10, 127, 10, WHITE);
    display.setCursor(3, 16); display.print(F("After power-off, tell"));
    display.setCursor(3, 25); display.print(F("me your real day/time."));
    display.drawRoundRect(8, 36, 112, 18, 5, WHITE);
    display.setCursor(16, 42);
    if (step == 0) { display.print(F("Day: ")); display.print(weekdayName(selectedDay)); }
    else if (step == 1) { display.print(F("Hour: ")); display.print(selectedHour); }
    else if (step == 2) { display.print(F("Minute: ")); if (selectedMinute < 10) display.print('0'); display.print(selectedMinute); }
    else { display.print(F("Period: ")); display.print(pm ? F("PM") : F("AM")); }
    display.setCursor(0, 56); display.print(F("D0 -  D2 +  D1 next"));
    display.display(); delay(20);
  }

  byte useHour = selectedHour % 12;
  if (pm) useHour += 12;
  unsigned long baseDay = seedEpoch / 86400UL;
  byte baseWeekday = static_cast<byte>((baseDay + 3UL) % 7UL);
  byte dayDelta = static_cast<byte>((selectedDay + 7 - baseWeekday) % 7);
  unsigned long candidate = (baseDay + dayDelta) * 86400UL + static_cast<unsigned long>(useHour) * 3600UL + static_cast<unsigned long>(selectedMinute) * 60UL;

  // If the selected day/time is clearly before the saved moment, treat it as
  // the following week. This keeps weekdays moving forward across power-offs.
  if (savedEpoch >= 946684800UL && candidate + 2UL * 3600UL < savedEpoch) candidate += 7UL * 86400UL;
  rtc.setEpoch(candidate);
  lastLifeClockEpoch = candidate;
  lifeSecondRemainder = 0;
  waitButtonRelease();
  return candidate;
}

void setClockMenu() {
  unsigned long previous = currentClockEpoch();
  unsigned long updated = promptForManualClock(previous);
  lastRtcEpoch = updated;
  synchronizeDailySchedule(false);
  markSaveDirty();
  saveGameNow();
  popup(F("Clock synchronized"), F("Daily schedule updated"), 850);
}

byte bedtimeHour() {
  if (stage == STAGE_BABY) return 19;
  if (stage == STAGE_CHILD) return 20;
  if (stage == STAGE_TEEN) return 22;
  if (stage == STAGE_ADULT) return 23;
  if (stage == STAGE_ELDER) return 21;
  return 21;
}

byte wakeHour() {
  if (stage == STAGE_ADULT || stage == STAGE_ELDER) return 6;
  return 7;
}

bool isScheduledSleepTime() {
  if (dead || stage == STAGE_EGG) return false;
  byte hour = visualHour();
  return hour >= bedtimeHour() || hour < wakeHour();
}

bool isScheduledSchoolTime() {
  if (dead || sleeping || clockWeekday() >= 5) return false;
  if (stage != STAGE_CHILD && stage != STAGE_TEEN) return false;
  unsigned int nowMinute = static_cast<unsigned int>(visualHour()) * 60U + clockMinute();
  return nowMinute >= 8U * 60U && nowMinute < 15U * 60U;
}

bool isScheduledWorkTime() {
  if (dead || sleeping || clockWeekday() >= 5 || stage != STAGE_ADULT) return false;
  if (lifeWorld.careerId() >= LIFE_CAREER_COUNT) return false;
  unsigned int nowMinute = static_cast<unsigned int>(visualHour()) * 60U + clockMinute();
  return nowMinute >= 9U * 60U && nowMinute < 17U * 60U;
}

bool isScheduledAwayTime() {
  return isScheduledSchoolTime() || isScheduledWorkTime();
}

const __FlashStringHelper* scheduledPlaceName() {
  if (sleeping || isScheduledSleepTime()) return F("SLEEP");
  if (isScheduledSchoolTime()) return F("SCHOOL");
  if (isScheduledWorkTime()) return F("WORK");
  return F("HOME");
}

NeedsDisplayState makeNeedsDisplayState() {
  NeedsDisplayState state{};
  state.health = health;
  state.food = full;
  state.energy = energy;
  state.fun = happy;
  state.clean = clean;
  state.poop = poop;
  state.conditionSeverity = conditionSeverity;
  state.location = lifeWorld.currentLocation();
  state.weekday = clockWeekday();
  state.hour = visualHour();
  state.minute = clockMinute();
  state.wakeHour = wakeHour();
  state.returnHour = 0;
  state.sitterHours = sitterTicksRemaining == 0 ? 0 : static_cast<byte>((sitterTicksRemaining + TICKS_PER_HOUR - 1UL) / TICKS_PER_HOUR);
  state.sick = sick || conditionType != 0;

  if (dead) state.mode = NEEDS_MODE_DEAD;
  else if (stage == STAGE_EGG) state.mode = NEEDS_MODE_EGG;
  else if (sleeping || isScheduledSleepTime()) state.mode = NEEDS_MODE_SLEEP;
  else if (sitterTicksRemaining > 0) state.mode = NEEDS_MODE_SITTER;
  else if (isScheduledSchoolTime()) { state.mode = NEEDS_MODE_SCHOOL; state.returnHour = 15; }
  else if (isScheduledWorkTime()) { state.mode = NEEDS_MODE_WORK; state.returnHour = 17; }
  else state.mode = NEEDS_MODE_LIVE;

  return state;
}

void serviceNeedsDisplay(bool force = false) {
  needsDisplay.update(makeNeedsDisplayState(), millis(), force);
}

void synchronizeDailySchedule(bool announce) {
  if (dead || stage == STAGE_EGG) return;
  bool shouldSleep = isScheduledSleepTime();
  if (shouldSleep && !sleeping) {
    sleeping = true;
    lightOff = true;
    sleepTicks = 0;
    wrongLightTicks = 0;
    lightMistakeOpen = false;
    pendingCallType = 0;
    pendingCallTicks = 0;
    markRoutineAction(ROUTINE_BED);
    if (announce && !rtcCatchUpMode) popup(careSleepReply(false), F("I'll see you in the morning"), 750);
    markSaveDirty();
  } else if (!shouldSleep && sleeping) {
    sleeping = false;
    lightOff = false;
    if (sleepTicks >= 4UL * TICKS_PER_HOUR && energy < 9) energy = 9;
    sleepTicks = 0;
    wrongLightTicks = 0;
    lightMistakeOpen = false;
    addStat(happy, 1);
    if (announce && !rtcCatchUpMode) popup(careSleepReply(true), F("A new day begins"), 750);
    markSaveDirty();
  }
}

void applyManualOfflineSchedule(unsigned long savedEpoch, unsigned long bootEpoch) {
  if (savedEpoch < 946684800UL || bootEpoch <= savedEpoch || dead || stage == STAGE_EGG) return;

  // Without the optional DS3231, the software clock resets on power loss. After the
  // player enters the real time, replay only the safe daily-schedule part of
  // the powered-off interval. This lets a real overnight power-off count as
  // sleep without inventing hours of hunger, poop, sickness, or care misses.
  unsigned long elapsed = bootEpoch - savedEpoch;
  if (elapsed > 14UL * 86400UL) elapsed = 14UL * 86400UL;
  unsigned long start = bootEpoch - elapsed;
  unsigned int sleepingHalfHours = 0;

  for (unsigned long t = start; t < bootEpoch; t += 1800UL) {
    byte hour = static_cast<byte>((t / 3600UL) % 24UL);
    if (hour >= bedtimeHour() || hour < wakeHour()) sleepingHalfHours++;
  }

  if (sleepingHalfHours >= 8) { // At least four real hours crossed bedtime.
    byte recovered = static_cast<byte>(min<unsigned int>(10U, 5U + sleepingHalfHours / 2U));
    if (energy < recovered) energy = recovered;
    if (sleepingHalfHours >= 12 && health < 10) health++;
    sleepTicks = static_cast<unsigned long>(sleepingHalfHours) * (TICKS_PER_HOUR / 2UL);
  }

  // The current entered time still decides whether the pet is asleep, home,
  // at school, or at work. No daytime quick-rest credit is granted.
  markSaveDirty();
}

void updateBatteryReading(bool force=false) {
#if BATTERY_MONITOR_ENABLED
  if (!force && millis() - lastBatteryCheckAt < 60000UL) return;
  lastBatteryCheckAt = millis();
  pinMode(BATTERY_ENABLE_PIN, OUTPUT);
  digitalWrite(BATTERY_ENABLE_PIN, HIGH);
  delay(4);
  analogReadResolution(12);
  unsigned long total = 0;
  for (byte i = 0; i < 6; i++) total += analogRead(BATTERY_SENSE_PIN);
  digitalWrite(BATTERY_ENABLE_PIN, LOW);
  unsigned long raw = total / 6UL;
  unsigned long pinMv = (raw * 3300UL) / 4095UL;
  batteryMv = (unsigned int)((pinMv * (BATTERY_DIVIDER_TOP_K + BATTERY_DIVIDER_BOTTOM_K)) / BATTERY_DIVIDER_BOTTOM_K);
#else
  batteryMv = 0;
  (void)force;
#endif
}

bool batteryLow() {
  return batteryMv > 0 && batteryMv <= BATTERY_LOW_MV;
}

bool batteryCritical() {
  return batteryMv > 0 && batteryMv <= BATTERY_CRITICAL_MV;
}

void applyBrightness() {
  // SH1107 contrast is a 7-bit value (0x00–0x7F).
  byte contrast = (brightnessLevel == 0) ? 0x20 : ((brightnessLevel == 1) ? 0x60 : 0x7F);
  display.setContrast(contrast);
}

const __FlashStringHelper* speciesName() {
  if (stage < STAGE_ADULT) {
    switch (petLineage()) {
      case 0: return F("Skyling");
      case 1: return F("Tidekin");
      case 2: return F("Pouncer");
      case 3: return F("Shadelet");
      case 4: return F("Crumbkin");
      case 5: return F("Hornling");
      case 6: return F("Pupkin");
      case 7: return F("Sproutling");
      case 8: return F("Tinker");
      default: return F("Gearling");
    }
  }
  switch (speciesType % SPECIES_COUNT) {
    case 0: return F("Bird");
    case 1: return F("Slug");
    case 2: return F("ShellCat");
    case 3: return F("Shadow");
    case 4: return F("Burger");
    case 5: return F("HornPig");
    case 6: return F("LongDino");
    case 7: return F("RoundDog");
    case 8: return F("Mushroom");
    case 9: return F("Leaf");
    case 10: return F("Hood");
    case 11: return F("Runner");
    case 12: return F("Square");
    case 13: return F("Snowman");
    case 14: return F("Hoodie");
    case 15: return F("MiniBird");
    case 16: return F("CowBell");
    case 17: return F("LongSeal");
    case 18: return F("Tower");
    case 19: return F("Frog");
    case 20: return F("BatCat");
    case 21: return F("BatBug");
    case 22: return F("Bean");
    case 23: return F("Jelly");
    case 24: return F("Devil");
    case 25: return F("Blob");
    case 26: return F("BigNose");
    case 27: return F("HatStack");
    case 28: return F("ShirtCat");
    case 29: return F("Fox");
    case 30: return F("Candle");
    case 31: return F("Potato");
    case 32: return F("Helmet");
    case 33: return F("BigMouth");
    case 34: return F("NoseMan");
    case 35: return F("Mouse");
    case 36: return F("Whale");
    case 37: return F("Sandwich");
    case 38: return F("Robot");
    case 39: return F("Octo");
    case 40: return F("UFO");
    case 41: return F("RoundPet");
    case 42: return F("Unicorn");
    case 43: return F("Duck");
    case 44: return F("Ghost");
    case 45: return F("HornOwl");
    case 46: return F("Cape");
    case 47: return F("DinoHead");
    case 48: return F("StarCat");
    case 49: return F("Bell");
    default: return F("Pet");
  }
}


bool needsCare() {
  return aquarium.needsCare() || full < 4 || energy < 3 || happy < 4 || clean < 4 || sick || poop > 0;
}

void waitButtonRelease() {
  bool sawPress = false;

  while (digitalRead(BTN_L) == LOW || digitalRead(BTN_S) == LOW || digitalRead(BTN_R) == LOW) {
    sawPress = true;
    lastInputAt = millis();
    delay(10);
  }

  if (sawPress) lastInputAt = millis();
}



bool pressed(byte pin) {
  bool down = digitalRead(pin) == LOW;
  if (down) lastInputAt = millis();
  return down;
}


bool rightHoldExitCheck() {
  static unsigned long started = 0;

  if (digitalRead(BTN_R) == LOW) {
    lastInputAt = millis();

    if (started == 0) started = millis();
    if (millis() - started >= HOLD_RIGHT_EXIT_MS) {
      started = 0;
      clickSound();
      waitButtonRelease();
      lastInputAt = millis();
      return true;
    }
  } else {
    started = 0;
  }

  return false;
}



void popup(const __FlashStringHelper *line1,const __FlashStringHelper *line2,int hold) {
  char title[96],body[160];NativeUI::flashCopy(title,line1,sizeof(title));NativeUI::flashCopy(body,line2,sizeof(body));
  NativeUI::message(display,title,body,"");delay(hold);
}

// ---------- menu icons ----------

void drawFoodIcon(int x, int y, bool inv) {
  uint16_t col = inv ? BLACK : WHITE;
  display.drawCircle(x + 8, y + 7, 4, col);
  display.drawLine(x + 14, y + 3, x + 14, y + 12, col);
  display.drawLine(x + 16, y + 3, x + 16, y + 12, col);
  display.drawPixel(x + 6, y + 6, col);
}

void drawLightIcon(int x, int y, bool inv) {
  uint16_t col = inv ? BLACK : WHITE;
  display.drawCircle(x + 9, y + 6, 4, col);
  display.drawLine(x + 6, y + 11, x + 12, y + 11, col);
  display.drawLine(x + 7, y + 13, x + 11, y + 13, col);
  display.drawPixel(x + 2, y + 5, col);
  display.drawPixel(x + 16, y + 5, col);
}

void drawGameIcon(int x, int y, bool inv) {
  uint16_t col = inv ? BLACK : WHITE;
  display.drawRect(x + 3, y + 5, 13, 7, col);
  display.drawLine(x + 1, y + 9, x + 17, y + 9, col);
  display.drawPixel(x + 7, y + 8, col);
  display.drawPixel(x + 12, y + 8, col);
}

void drawMedIcon(int x, int y, bool inv) {
  uint16_t col = inv ? BLACK : WHITE;
  display.drawRoundRect(x + 5, y + 4, 10, 10, 2, col);
  display.drawLine(x + 10, y + 5, x + 10, y + 13, col);
  display.drawLine(x + 6, y + 9, x + 14, y + 9, col);
}

void drawCleanIcon(int x, int y, bool inv) {
  uint16_t col = inv ? BLACK : WHITE;
  display.drawLine(x + 4, y + 4, x + 14, y + 13, col);
  display.drawLine(x + 9, y + 3, x + 16, y + 9, col);
  display.drawLine(x + 2, y + 13, x + 9, y + 13, col);
  display.drawPixel(x + 14, y + 4, col);
}

void drawLoveIcon(int x, int y, bool inv) {
  uint16_t col = inv ? BLACK : WHITE;
  display.fillCircle(x + 7, y + 6, 2, col);
  display.fillCircle(x + 11, y + 6, 2, col);
  display.fillTriangle(x + 5, y + 7, x + 13, y + 7, x + 9, y + 13, col);
}

void drawCallIcon(int x, int y, bool inv) {
  uint16_t col = inv ? BLACK : WHITE;
  display.drawRoundRect(x + 4, y + 3, 11, 10, 2, col);
  display.drawLine(x + 7, y + 13, x + 5, y + 15, col);
  display.drawPixel(x + 8, y + 8, col);
  display.drawPixel(x + 10, y + 8, col);
}

void drawWalkIcon(int x, int y, bool inv) {
  uint16_t col = inv ? BLACK : WHITE;
  display.drawCircle(x + 9, y + 4, 2, col);
  display.drawLine(x + 9, y + 6, x + 9, y + 10, col);
  display.drawLine(x + 9, y + 8, x + 5, y + 10, col);
  display.drawLine(x + 9, y + 8, x + 13, y + 10, col);
  display.drawLine(x + 9, y + 10, x + 5, y + 14, col);
  display.drawLine(x + 9, y + 10, x + 14, y + 14, col);
}

void drawStatsIcon(int x, int y, bool inv) {
  uint16_t col = inv ? BLACK : WHITE;
  display.drawRect(x + 3, y + 4, 13, 10, col);
  display.drawLine(x + 5, y + 11, x + 5, y + 12, col);
  display.drawLine(x + 8, y + 9, x + 8, y + 12, col);
  display.drawLine(x + 11, y + 7, x + 11, y + 12, col);
  display.drawLine(x + 14, y + 5, x + 14, y + 12, col);
}

void drawTrainIcon(int x, int y, bool inv) {
  uint16_t col = inv ? BLACK : WHITE;
  // Tiny whistle/book icon for discipline training.
  display.drawRoundRect(x + 4, y + 4, 10, 8, 2, col);
  display.drawLine(x + 7, y + 6, x + 13, y + 6, col);
  display.drawLine(x + 7, y + 9, x + 13, y + 9, col);
  display.drawPixel(x + 15, y + 11, col);
  display.drawPixel(x + 16, y + 12, col);
}

void drawStudyIcon(int x, int y, bool inv) {
  uint16_t col = inv ? BLACK : WHITE;
  display.drawRect(x + 4, y + 4, 11, 10, col);
  display.drawLine(x + 9, y + 4, x + 9, y + 14, col);
  display.drawPixel(x + 6, y + 7, col);
  display.drawPixel(x + 12, y + 7, col);
  display.drawLine(x + 5, y + 11, x + 8, y + 11, col);
  display.drawLine(x + 11, y + 11, x + 14, y + 11, col);
}

void drawGardenIcon(int x, int y, bool inv) {
  uint16_t col = inv ? BLACK : WHITE;
  display.drawRoundRect(x + 2, y + 9, 14, 7, 2, col);
  display.drawFastHLine(x + 3, y + 11, 12, col);
  display.drawLine(x + 6, y + 11, x + 6, y + 5, col);
  display.drawCircle(x + 4, y + 6, 2, col);
  display.drawCircle(x + 8, y + 5, 2, col);
  display.drawLine(x + 12, y + 11, x + 12, y + 6, col);
  display.drawTriangle(x + 10, y + 8, x + 14, y + 4, x + 14, y + 9, col);
  display.drawPixel(x + 5, y + 13, inv ? WHITE : BLACK);
  display.drawPixel(x + 12, y + 14, inv ? WHITE : BLACK);
}

void drawPlantRoomIcon(int x, int y, bool inv) {
  uint16_t col = inv ? BLACK : WHITE;
  display.drawRoundRect(x + 4, y + 11, 10, 5, 2, col);
  display.drawFastHLine(x + 5, y + 12, 8, inv ? WHITE : BLACK);
  display.drawLine(x + 9, y + 11, x + 9, y + 5, col);
  display.drawLine(x + 9, y + 8, x + 5, y + 6, col);
  display.drawLine(x + 9, y + 8, x + 13, y + 6, col);
  display.drawCircle(x + 5, y + 5, 2, col);
  display.drawCircle(x + 9, y + 3, 2, col);
  display.drawCircle(x + 13, y + 5, 2, col);
  display.drawPixel(x + 9, y + 5, inv ? WHITE : BLACK);
}

void drawMusicIcon(int x, int y, bool inv) {
  uint16_t col = inv ? BLACK : WHITE;
  display.drawLine(x + 7, y + 4, x + 7, y + 12, col);
  display.drawLine(x + 7, y + 4, x + 14, y + 5, col);
  display.drawLine(x + 14, y + 5, x + 14, y + 13, col);
  display.fillCircle(x + 5, y + 13, 2, col);
  display.fillCircle(x + 12, y + 14, 2, col);
}

void drawShopIcon(int x, int y, bool inv) {
  uint16_t col = inv ? BLACK : WHITE;
  display.drawRoundRect(x + 4, y + 5, 11, 8, 2, col);
  display.drawLine(x + 6, y + 5, x + 8, y + 2, col);
  display.drawLine(x + 13, y + 5, x + 11, y + 2, col);
  display.drawPixel(x + 8, y + 9, col);
  display.drawPixel(x + 11, y + 9, col);
}

void drawToyIcon(int x, int y, bool inv) {
  uint16_t col = inv ? BLACK : WHITE;
  display.drawCircle(x + 9, y + 9, 5, col);
  display.drawLine(x + 5, y + 9, x + 13, y + 9, col);
  display.drawLine(x + 9, y + 5, x + 9, y + 13, col);
}

void drawExploreIcon(int x, int y, bool inv) {
  uint16_t col = inv ? BLACK : WHITE;
  display.drawTriangle(x + 4, y + 13, x + 9, y + 4, x + 14, y + 13, col);
  display.drawLine(x + 9, y + 4, x + 9, y + 2, col);
  display.drawPixel(x + 15, y + 4, col);
}

void drawCookIcon(int x, int y, bool inv) {
  uint16_t col = inv ? BLACK : WHITE;
  display.drawRoundRect(x + 4, y + 7, 12, 7, 3, col);
  display.drawLine(x + 6, y + 7, x + 5, y + 4, col);
  display.drawLine(x + 10, y + 7, x + 10, y + 3, col);
  display.drawLine(x + 14, y + 7, x + 15, y + 4, col);
}

void drawJobIcon(int x, int y, bool inv) {
  uint16_t col = inv ? BLACK : WHITE;
  display.drawRect(x + 4, y + 6, 12, 8, col);
  display.drawLine(x + 7, y + 6, x + 7, y + 3, col);
  display.drawLine(x + 7, y + 3, x + 13, y + 3, col);
  display.drawLine(x + 13, y + 3, x + 13, y + 6, col);
}

void drawRoomIcon(int x, int y, bool inv) {
  uint16_t col = inv ? BLACK : WHITE;
  display.drawRect(x + 3, y + 5, 13, 9, col);
  display.drawLine(x + 3, y + 5, x + 9, y + 1, col);
  display.drawLine(x + 16, y + 5, x + 9, y + 1, col);
  display.drawRect(x + 8, y + 9, 4, 5, col);
}

void drawTalkIcon(int x, int y, bool inv) {
  uint16_t col = inv ? BLACK : WHITE;
  display.drawRoundRect(x + 3, y + 4, 13, 8, 3, col);
  display.drawLine(x + 7, y + 12, x + 5, y + 15, col);
  display.drawPixel(x + 7, y + 8, col);
  display.drawPixel(x + 10, y + 8, col);
  display.drawPixel(x + 13, y + 8, col);
}

void drawAquariumIcon(int x, int y, bool inv) {
  aquarium.drawSmallIcon(x, y, inv);
}

void drawIcon(byte id, int x, int y, bool inv) {
  if (id == M_FOOD) drawFoodIcon(x, y, inv);
  else if (id == M_LIGHT) drawLightIcon(x, y, inv);
  else if (id == M_GAME) drawGameIcon(x, y, inv);
  else if (id == M_MED) drawMedIcon(x, y, inv);
  else if (id == M_CLEAN) drawCleanIcon(x, y, inv);
  else if (id == M_LOVE) drawLoveIcon(x, y, inv);
  else if (id == M_CALL) drawCallIcon(x, y, inv);
  else if (id == M_WALK) drawWalkIcon(x, y, inv);
  else if (id == M_TRAIN) drawTrainIcon(x, y, inv);
  else if (id == M_STUDY) drawStudyIcon(x, y, inv);
  else if (id == M_GARDEN) drawGardenIcon(x, y, inv);
  else if (id == M_MUSIC) drawMusicIcon(x, y, inv);
  else if (id == M_SHOP) drawShopIcon(x, y, inv);
  else if (id == M_TOY) drawToyIcon(x, y, inv);
  else if (id == M_EXPLORE) drawExploreIcon(x, y, inv);
  else if (id == M_COOK) drawCookIcon(x, y, inv);
  else if (id == M_JOB) drawJobIcon(x, y, inv);
  else if (id == M_ROOM) drawRoomIcon(x, y, inv);
  else if (id == M_TALK || id == M_FAMILY || id == M_DIARY || id == M_NEIGHBORS) drawTalkIcon(x, y, inv);
  else if (id == M_AQUARIUM) drawAquariumIcon(x, y, inv);
  else if (id == M_PLANTROOM) drawPlantRoomIcon(x, y, inv);
  else if (id == M_CRAFT) drawCookIcon(x, y, inv);
  else if (id == M_HABITAT || id == M_EVENT) drawExploreIcon(x, y, inv);
  else if (id == M_BOOK || id == M_HISTORY || id == M_INSPECT || id == M_DIAG) drawStatsIcon(x, y, inv);
  else drawStatsIcon(x, y, inv);
}

byte categoryIcon(byte category) {
  if (category == 0) return M_FOOD;
  if (category == 1) return M_CLEAN;
  if (category == 2) return M_GAME;
  if (category == 3) return M_TALK;
  if (category == 4) return M_EXPLORE;
  return M_STATS;
}

const __FlashStringHelper* categoryShortTitle(byte category) {
  if (category == 0) return F("FOOD");
  if (category == 1) return F("CARE");
  if (category == 2) return F("PLAY");
  if (category == 3) return F("BOND");
  if (category == 4) return F("ACTIVITY");
  return F("STATUS");
}

const __FlashStringHelper* categoryDescription(byte category) {
  if (category == 0) return F("Meals/snack");
  if (category == 1) return F("Health care");
  if (category == 2) return F("Games/skill");
  if (category == 3) return F("Talk/family");
  if (category == 4) return F("Explore/job");
  return F("Stats/setup");
}

void drawBatteryIcon(int x, int y) {
  if (batteryMv == 0) return;
  display.drawRect(x, y, 12, 6, WHITE);
  display.fillRect(x + 12, y + 2, 2, 2, WHITE);
  byte bars = 0;
  if (batteryMv >= 4050) bars = 4;
  else if (batteryMv >= 3850) bars = 3;
  else if (batteryMv >= 3650) bars = 2;
  else if (batteryMv >= 3450) bars = 1;
  for (byte i = 0; i < bars; i++) display.fillRect(x + 2 + i * 2, y + 2, 1, 2, WHITE);
}









void drawMenu() {
  if(!menuVisible)return;
  NativeUI::header(display,"ACTIVITIES");
  static const char *labels[]={"FOOD","CARE","PLAY","BOND","WORLD","STATS"};
  for(byte i=0;i<CATEGORY_COUNT;++i){
    const int x=4+(i%2)*62,y=23+(i/2)*30;
    const bool active=i==mainCategory;
    if(active)display.fillRoundRect(x,y,58,26,4,WHITE);
    else display.drawRoundRect(x,y,58,26,4,WHITE);
    NativeUI::icon(display,i,x+4,y+5,active?BLACK:WHITE);
    NativeUI::text(display,labels[i],x+24,y+10,30,active?BLACK:WHITE);
  }
  NativeUI::footer(display,"PREV","OPEN","NEXT");
}

void drawMenuName() {
  display.setTextSize(1);
  display.setTextColor(WHITE);

  if (!menuVisible) {
    if (sitterTicksRemaining > 0) {
      display.setCursor(0, 0);
      display.print(F("SITTER"));
    } else if (sick || conditionType != 0) {
      display.setCursor(0, 0);
      display.print(F("SICK"));
    } else if (stage == STAGE_EGG) {
      display.setCursor(0, 0);
      display.print(F("EGG"));
    }
    if (poop > 0) {
      display.setCursor(94, 0);
      display.print(F("POOP"));
    }
    if (botany.gardenReady()) {
      display.setCursor(43, 0);
      display.print(F("GARDEN"));
    }
    if (batteryLow()) {
      display.setCursor(100, 9);
      display.print(F("LOW"));
    }
    if ((needsCare() || pendingCallType != 0) && (anim & 1)) {
      display.drawCircle(124, 8, 3, WHITE);
      display.drawLine(124, 12, 124, 14, WHITE);
    }
    if ((parentDialogue.hasPending() || (stage != STAGE_BABY && growingTogether.hasPending())) && (anim & 1)) {
      display.drawRoundRect(78, 0, 18, 8, 3, WHITE);
      display.drawLine(82, 8, 80, 11, WHITE);
      display.drawPixel(83, 4, WHITE); display.drawPixel(87, 4, WHITE); display.drawPixel(91, 4, WHITE);
    }
    drawBatteryIcon(108, 0);
    return;
  }

  // v46 menu chrome is drawn entirely by drawMenu().
  return;
}

// Old drawMenuName removed in v15.3.


// Old drawMenuName removed by v15.


// ---------- egg and pet drawing ----------
// v14: retro-inspired sprite roster and normal eggs.

// The 50-species sprite masks are shared with NativeUI via RosterSprites.h.

void drawMaskSprite(int x, int y, const uint32_t rows[32], byte scale) {
  // Compatibility wrapper. In v16.5, sprites are 32x32.
  // scale 1 draws the original 32x32 sprite.
  // scale 0 draws a 16x16 downsampled mini sprite.
  if (scale == 0) {
    for (byte row = 0; row < 16; row++) {
      uint32_t bitsA = pgm_read_dword(&rows[row * 2]);
      uint32_t bitsB = pgm_read_dword(&rows[row * 2 + 1]);
      for (byte col = 0; col < 16; col++) {
        uint32_t maskA = (0xC0000000UL >> (col * 2));
        if ((bitsA & maskA) || (bitsB & maskA)) {
          display.drawPixel(x + col, y + row, WHITE);
        }
      }
    }
    return;
  }

  for (byte row = 0; row < 32; row++) {
    uint32_t bits = pgm_read_dword(&rows[row]);
    for (byte col = 0; col < 32; col++) {
      if (bits & (0x80000000UL >> col)) {
        if (scale == 1) display.drawPixel(x + col, y + row, WHITE);
        else display.fillRect(x + col * scale, y + row * scale, scale, scale, WHITE);
      }
    }
  }
}

void drawSprite32(byte id,int x,int y){NativeUI::pet(display,x,y,id,STAGE_ADULT,0,false,2);}

void drawSprite16(byte id,int x,int y){NativeUI::pet(display,x,y,id,STAGE_ADULT,0);}


byte favoriteFoodType() {
  switch (speciesType % SPECIES_COUNT) {
    case 0: return 0;
    case 1: return 3;
    case 2: return 1;
    case 3: return 4;
    case 4: return 2;
    case 5: return 1;
    case 6: return 4;
    case 7: return 2;
    case 8: return 0;
    case 9: return 3;
    case 10: return 2;
    case 11: return 0;
    case 12: return 3;
    case 13: return 1;
    case 14: return 4;
    case 15: return 3;
    case 16: return 1;
    case 17: return 4;
    case 18: return 2;
    case 19: return 0;
    case 20: return 4;
    case 21: return 2;
    case 22: return 0;
    case 23: return 3;
    case 24: return 1;
    case 25: return 0;
    case 26: return 3;
    case 27: return 1;
    case 28: return 4;
    case 29: return 2;
    case 30: return 1;
    case 31: return 4;
    case 32: return 2;
    case 33: return 0;
    case 34: return 3;
    case 35: return 2;
    case 36: return 0;
    case 37: return 3;
    case 38: return 1;
    case 39: return 4;
    case 40: return 3;
    case 41: return 1;
    case 42: return 4;
    case 43: return 2;
    case 44: return 0;
    case 45: return 4;
    case 46: return 2;
    case 47: return 0;
    case 48: return 3;
    case 49: return 1;
    default: return 0;
  }
}

const __FlashStringHelper* favoriteFoodName() {
  switch (favoriteFoodType()) {
    case 0: return F("basic meal");
    case 1: return F("cooked meal");
    case 2: return F("snack");
    case 3: return F("healthy meal");
    default: return F("treat");
  }
}

const __FlashStringHelper* petDialogue() {
  switch (speciesType % SPECIES_COUNT) {
    case 0: return F("Tweet tweet!");
    case 1: return F("Slow day good.");
    case 2: return F("My shell cozy.");
    case 3: return F("Hehe, prank?");
    case 4: return F("Is my hat cute?");
    case 5: return F("Snack time?");
    case 6: return F("Let us walk!");
    case 7: return F("Pet me please!");
    case 8: return F("Rain feels nice.");
    case 9: return F("Check garden!");
    case 10: return F("Hum with me.");
    case 11: return F("Need a hug!");
    case 12: return F("Race you!");
    case 13: return F("Snow nap soon.");
    case 14: return F("Hoodie comfy.");
    case 15: return F("Tiny song!");
    case 16: return F("Ring ring!");
    case 17: return F("Splash time!");
    case 18: return F("Stack me up!");
    case 19: return F("Ribbit hi!");
    case 20: return F("Night is fun.");
    case 21: return F("Wings ready!");
    case 22: return F("Bean mood.");
    case 23: return F("Soft tune?");
    case 24: return F("Trick time!");
    case 25: return F("Blob hello.");
    case 26: return F("Tell me joke!");
    case 27: return F("Hat looks good.");
    case 28: return F("Clean shirt!");
    case 29: return F("Forest walk?");
    case 30: return F("Warm glow.");
    case 31: return F("Cook potatoes!");
    case 32: return F("Helmet ready.");
    case 33: return F("Big bite!");
    case 34: return F("Story time?");
    case 35: return F("Cheese please.");
    case 36: return F("Ocean dream.");
    case 37: return F("Lunch break!");
    case 38: return F("System happy.");
    case 39: return F("Drum beat!");
    case 40: return F("I saw stars.");
    case 41: return F("Roll around!");
    case 42: return F("Magic horn!");
    case 43: return F("Quack!");
    case 44: return F("Boo friend!");
    case 45: return F("Wise hoot.");
    case 46: return F("Cape hero!");
    case 47: return F("Tiny roar!");
    case 48: return F("Sparkle day!");
    case 49: return F("Chime chime!");
    default: return F("Hello!");
  }
}

const __FlashStringHelper* petDreamLine() {
  switch (speciesType % SPECIES_COUNT) {
    case 0: return F("Flying high!");
    case 1: return F("A soft bed.");
    case 2: return F("A warm shell.");
    case 3: return F("Being sneaky.");
    case 4: return F("A fashion show.");
    case 5: return F("Candy mountain.");
    case 6: return F("A long road.");
    case 7: return F("A cozy lap.");
    case 8: return F("A rain forest.");
    case 9: return F("A big harvest.");
    case 10: return F("A concert.");
    case 11: return F("A cuddle pile.");
    case 12: return F("A race track.");
    case 13: return F("Snow clouds.");
    case 14: return F("A tiny cabin.");
    case 15: return F("A bird choir.");
    case 16: return F("A bell tower.");
    case 17: return F("Blue waves.");
    case 18: return F("Tall towers.");
    case 19: return F("A lily pond.");
    case 20: return F("A moon cave.");
    case 21: return F("Huge wings.");
    case 22: return F("A bean field.");
    case 23: return F("A music cloud.");
    case 24: return F("A magic stage.");
    case 25: return F("A jelly pool.");
    case 26: return F("A comedy show.");
    case 27: return F("A hat shop.");
    case 28: return F("A shirt shop.");
    case 29: return F("A green forest.");
    case 30: return F("A warm room.");
    case 31: return F("A kitchen.");
    case 32: return F("A safe helmet.");
    case 33: return F("A feast.");
    case 34: return F("A campfire.");
    case 35: return F("A cheese moon.");
    case 36: return F("The deep sea.");
    case 37: return F("A picnic.");
    case 38: return F("A robot city.");
    case 39: return F("A drum circle.");
    case 40: return F("A star ship.");
    case 41: return F("A round hill.");
    case 42: return F("A rainbow.");
    case 43: return F("A bread trail.");
    case 44: return F("A ghost party.");
    case 45: return F("A wise tree.");
    case 46: return F("A hero cape.");
    case 47: return F("A dino valley.");
    case 48: return F("A star garden.");
    case 49: return F("A music bell.");
    default: return F("Big adventure!");
  }
}

void applySpeciesBirthBonus() {
  switch (speciesType % SPECIES_COUNT) {
    case 0: addStat(health, 2); break;
    case 1: addStat(full, 2); break;
    case 2: addStat(energy, 2); addSkill(skillFit, 1); break;
    case 3: addStat(happy, 2); addSkill(skillKind, 1); break;
    case 4: addStat(clean, 2); break;
    case 5: addStat(discipline, 2); break;
    case 6: addSkill(skillSmart, 2); break;
    case 7: addSkill(skillStyle, 2); break;
    case 8: addStat(health, 2); break;
    case 9: addStat(full, 2); break;
    case 10: addStat(energy, 2); addSkill(skillFit, 1); break;
    case 11: addStat(happy, 2); addSkill(skillKind, 1); break;
    case 12: addStat(clean, 2); break;
    case 13: addStat(discipline, 2); break;
    case 14: addSkill(skillSmart, 2); break;
    case 15: addSkill(skillStyle, 2); break;
    case 16: addStat(health, 2); break;
    case 17: addStat(full, 2); break;
    case 18: addStat(energy, 2); addSkill(skillFit, 1); break;
    case 19: addStat(happy, 2); addSkill(skillKind, 1); break;
    case 20: addStat(clean, 2); break;
    case 21: addStat(discipline, 2); break;
    case 22: addSkill(skillSmart, 2); break;
    case 23: addSkill(skillStyle, 2); break;
    case 24: addStat(health, 2); break;
    case 25: addStat(full, 2); break;
    case 26: addStat(energy, 2); addSkill(skillFit, 1); break;
    case 27: addStat(happy, 2); addSkill(skillKind, 1); break;
    case 28: addStat(clean, 2); break;
    case 29: addStat(discipline, 2); break;
    case 30: addSkill(skillSmart, 2); break;
    case 31: addSkill(skillStyle, 2); break;
    case 32: addStat(health, 2); break;
    case 33: addStat(full, 2); break;
    case 34: addStat(energy, 2); addSkill(skillFit, 1); break;
    case 35: addStat(happy, 2); addSkill(skillKind, 1); break;
    case 36: addStat(clean, 2); break;
    case 37: addStat(discipline, 2); break;
    case 38: addSkill(skillSmart, 2); break;
    case 39: addSkill(skillStyle, 2); break;
    case 40: addStat(health, 2); break;
    case 41: addStat(full, 2); break;
    case 42: addStat(energy, 2); addSkill(skillFit, 1); break;
    case 43: addStat(happy, 2); addSkill(skillKind, 1); break;
    case 44: addStat(clean, 2); break;
    case 45: addStat(discipline, 2); break;
    case 46: addSkill(skillSmart, 2); break;
    case 47: addSkill(skillStyle, 2); break;
    case 48: addStat(health, 2); break;
    case 49: addStat(full, 2); break;
  }

  if (favoriteFoodType() == 1) cookedMeals++;
  if (petLineage() == 0) berryInv++;
  if (petLineage() == 3) spiceInv++;
  if (petLineage() == 7) herbInv++;
}


const __FlashStringHelper* favoriteThing() {
  switch (speciesType % SPECIES_COUNT) {
    case 0: return F("likes chirping");
    case 1: return F("likes warm naps");
    case 2: return F("likes cozy shells");
    case 3: return F("likes mischief");
    case 4: return F("likes dress-up");
    case 5: return F("likes sweet snacks");
    case 6: return F("likes long walks");
    case 7: return F("likes attention");
    case 8: return F("likes rain");
    case 9: return F("likes gardens");
    case 10: return F("likes music");
    case 11: return F("likes hugs");
    case 12: return F("likes racing");
    case 13: return F("likes snow");
    case 14: return F("likes hoodies");
    case 15: return F("likes tiny songs");
    case 16: return F("likes bells");
    case 17: return F("likes swimming");
    case 18: return F("likes stacking");
    case 19: return F("likes ponds");
    case 20: return F("likes night games");
    case 21: return F("likes flying");
    case 22: return F("likes quiet time");
    case 23: return F("likes soft music");
    case 24: return F("likes tricks");
    case 25: return F("likes puddles");
    case 26: return F("likes jokes");
    case 27: return F("likes hats");
    case 28: return F("likes shirts");
    case 29: return F("likes forests");
    case 30: return F("likes candles");
    case 31: return F("likes cooking");
    case 32: return F("likes helmets");
    case 33: return F("likes big meals");
    case 34: return F("likes stories");
    case 35: return F("likes cheese");
    case 36: return F("likes waves");
    case 37: return F("likes lunch");
    case 38: return F("likes tinkering");
    case 39: return F("likes drums");
    case 40: return F("likes stars");
    case 41: return F("likes rolling");
    case 42: return F("likes magic");
    case 43: return F("likes crumbs");
    case 44: return F("likes moonlight");
    case 45: return F("likes wisdom");
    case 46: return F("likes capes");
    case 47: return F("likes roaring");
    case 48: return F("likes sparkles");
    case 49: return F("likes chimes");
    default: return F("likes chimes");
  }
}


const __FlashStringHelper* dislikedThing() {
  switch (speciesType % SPECIES_COUNT) {
    case 0: return F("hates thunder");
    case 1: return F("hates rushing");
    case 2: return F("hates mud");
    case 3: return F("hates losing");
    case 4: return F("hates heat");
    case 5: return F("hates bedtime");
    case 6: return F("hates waiting");
    case 7: return F("hates loneliness");
    case 8: return F("hates dry wind");
    case 9: return F("hates frost");
    case 10: return F("hates noise");
    case 11: return F("hates dirt");
    case 12: return F("hates sitting still");
    case 13: return F("hates summer");
    case 14: return F("hates cold soup");
    case 15: return F("hates loud rooms");
    case 16: return F("hates silence");
    case 17: return F("hates dry land");
    case 18: return F("hates falling");
    case 19: return F("hates dry ponds");
    case 20: return F("hates sunlight");
    case 21: return F("hates tight spaces");
    case 22: return F("hates loud beeps");
    case 23: return F("hates boredom");
    case 24: return F("hates rules");
    case 25: return F("hates sharp rocks");
    case 26: return F("hates plain food");
    case 27: return F("hates wind");
    case 28: return F("hates stains");
    case 29: return F("hates cages");
    case 30: return F("hates rain");
    case 31: return F("hates peeling");
    case 32: return F("hates bare heads");
    case 33: return F("hates small bites");
    case 34: return F("hates being ignored");
    case 35: return F("hates cats");
    case 36: return F("hates deserts");
    case 37: return F("hates crumbs");
    case 38: return F("hates water");
    case 39: return F("hates quiet");
    case 40: return F("hates gravity");
    case 41: return F("hates corners");
    case 42: return F("hates plain food");
    case 43: return F("hates cats");
    case 44: return F("hates bright lights");
    case 45: return F("hates daytime");
    case 46: return F("hates plain clothes");
    case 47: return F("hates tiny meals");
    case 48: return F("hates dull days");
    case 49: return F("hates silence");
    default: return F("hates silence");
  }
}


void drawGrowthSprite(int x, int y, byte sc, bool sleepEyes=false) {
  const byte appearance = stage < STAGE_ADULT ? petLineage() : speciesType;
  NativeUI::pet(display, x, y, appearance, stage, anim, sleepEyes, sc,
                currentEvolutionPath());
}

void drawCurrentPetMini(int x, int y, byte scale) {
  if (stage < STAGE_ADULT) {
    NativeUI::pet(display, x, y, petLineage(), stage, anim, false,
                  scale <= 1 ? 1 : 2, currentEvolutionPath());
    return;
  }
  // Adult and elder forms use the 50-creature evolution roster.
  // scale 0 or 1 are treated as mini 16x16 for games.
  if (scale <= 1) {
    drawSprite16(speciesType, x, y);
    int cx = x + 8;
    int ey = y + 6;
    display.drawPixel(cx - 3, ey, BLACK);
    display.drawPixel(cx + 3, ey, BLACK);
    display.drawLine(cx - 1, ey + 3, cx + 1, ey + 3, BLACK);
  } else {
    drawSprite32(speciesType, x, y);
    int cx = x + 16;
    int ey = y + 12;
    display.fillRect(cx - 5, ey, 2, 3, BLACK);
    display.fillRect(cx + 4, ey, 2, 3, BLACK);
    display.drawLine(cx - 2, ey + 6, cx + 2, ey + 6, BLACK);

    // Elder details make the final life stage visibly distinct without replacing
    // the evolved species art. High-bond pets also show a tiny friendship heart.
    if (stage == STAGE_ELDER) {
      display.drawLine(x + 27, y + 19, x + 29, y + 30, WHITE);
      display.drawLine(x + 29, y + 30, x + 31, y + 30, WHITE);
      display.drawPixel(cx - 7, ey + 5, BLACK);
      display.drawPixel(cx + 7, ey + 5, BLACK);
    }
    if (ownerBond >= 80 && (anim & 1) == 0) {
      display.drawPixel(x + 28, y + 2, WHITE);
      display.drawPixel(x + 30, y + 2, WHITE);
      display.drawPixel(x + 27, y + 3, WHITE);
      display.drawPixel(x + 29, y + 3, WHITE);
      display.drawPixel(x + 31, y + 3, WHITE);
      display.drawPixel(x + 29, y + 4, WHITE);
    }
  }
}


void drawCurrentPetTiny(int x,int y){
  const byte appearance = stage < STAGE_ADULT ? petLineage() : speciesType;
  NativeUI::pet(display,x,y,appearance,stage,anim,sleeping||lightOff,1,
                currentEvolutionPath());
}


void drawCurrentPetSleepMini(int x, int y, byte scale) {
  if (stage < STAGE_ADULT) {
    NativeUI::pet(display, x, y, petLineage(), stage, anim, true,
                  scale <= 1 ? 1 : 2, currentEvolutionPath());
    return;
  }
  if (scale <= 1) {
    drawSprite16(speciesType, x, y);
    int cx = x + 8;
    int ey = y + 6;
    display.drawLine(cx - 3, ey + 1, cx - 1, ey + 1, BLACK);
    display.drawLine(cx + 1, ey + 1, cx + 3, ey + 1, BLACK);
    display.drawPixel(cx, ey + 4, BLACK);
  } else {
    drawSprite32(speciesType, x, y);
    int cx = x + 16;
    int ey = y + 12;
    display.drawLine(cx - 6, ey + 2, cx - 2, ey + 2, BLACK);
    display.drawLine(cx + 2, ey + 2, cx + 6, ey + 2, BLACK);
    display.drawPixel(cx - 1, ey + 7, BLACK);
    display.drawPixel(cx, ey + 8, BLACK);
    display.drawPixel(cx + 1, ey + 7, BLACK);
  }
}


void drawCurrentPetSleepTiny(int x, int y) {
  drawCurrentPetSleepMini(x, y, 1);
}

void drawCurrentPetGameBadge(int x, int y) {
  display.fillRoundRect(x - 2, y - 2, 20, 20, 3, BLACK);
  display.drawRoundRect(x - 2, y - 2, 20, 20, 3, WHITE);
  drawCurrentPetTiny(x, y);
}


void drawBigMenuEmoji(byte id, int x, int y, bool inv) {
  uint16_t col = inv ? BLACK : WHITE;

  // 0 Food, 1 Light, 2 Games, 3 Med, 4 Clean, 5 Love,
  // 6 Call, 7 Walk, 8 Train, 9 Study, 10 Garden, 11 Music,
  // 12 Stats, 13 Clothes, 14 Toys, 15 Explore, 16 Cook, 17 Job,
  // 18 Room, 19 Talk.
  if (id == M_FOOD) { // bowl/food
    display.drawRoundRect(x + 4, y + 14, 28, 11, 5, col);
    display.drawLine(x + 8, y + 25, x + 28, y + 25, col);
    display.fillCircle(x + 18, y + 11, 4, col);
    display.drawLine(x + 28, y + 5, x + 30, y + 20, col);
  } else if (id == M_LIGHT) { // lightbulb
    display.drawCircle(x + 18, y + 11, 8, col);
    display.drawLine(x + 13, y + 20, x + 23, y + 20, col);
    display.drawLine(x + 15, y + 24, x + 21, y + 24, col);
    display.drawPixel(x + 6, y + 11, col); display.drawPixel(x + 30, y + 11, col);
  } else if (id == M_GAME) { // gamepad
    display.drawRoundRect(x + 4, y + 10, 29, 15, 5, col);
    display.drawLine(x + 10, y + 17, x + 18, y + 17, col);
    display.drawLine(x + 14, y + 13, x + 14, y + 21, col);
    display.fillCircle(x + 24, y + 15, 2, col);
    display.fillCircle(x + 29, y + 20, 2, col);
  } else if (id == M_MED) { // medicine cross
    display.drawRoundRect(x + 8, y + 5, 21, 23, 4, col);
    display.drawLine(x + 18, y + 9, x + 18, y + 24, col);
    display.drawLine(x + 11, y + 16, x + 25, y + 16, col);
  } else if (id == M_CLEAN) { // bath bubbles/brush
    display.drawLine(x + 7, y + 22, x + 27, y + 6, col);
    display.drawRoundRect(x + 4, y + 23, 15, 5, 2, col);
    display.drawCircle(x + 23, y + 22, 3, col);
    display.drawCircle(x + 29, y + 18, 2, col);
    display.drawCircle(x + 31, y + 25, 2, col);
  } else if (id == M_LOVE) { // heart
    display.fillCircle(x + 13, y + 12, 5, col);
    display.fillCircle(x + 23, y + 12, 5, col);
    display.fillTriangle(x + 8, y + 15, x + 28, y + 15, x + 18, y + 28, col);
  } else if (id == M_CALL) { // speech bubble
    display.drawRoundRect(x + 5, y + 6, 27, 19, 5, col);
    display.drawLine(x + 13, y + 25, x + 8, y + 30, col);
    display.drawPixel(x + 14, y + 15, col); display.drawPixel(x + 20, y + 15, col); display.drawPixel(x + 26, y + 15, col);
  } else if (id == M_WALK) { // shoes/walk
    display.drawCircle(x + 16, y + 6, 3, col);
    display.drawLine(x + 16, y + 9, x + 16, y + 18, col);
    display.drawLine(x + 16, y + 12, x + 9, y + 15, col);
    display.drawLine(x + 16, y + 12, x + 24, y + 15, col);
    display.drawLine(x + 16, y + 18, x + 9, y + 27, col);
    display.drawLine(x + 16, y + 18, x + 25, y + 27, col);
  } else if (id == M_TRAIN) { // target
    display.drawCircle(x + 18, y + 16, 13, col);
    display.drawCircle(x + 18, y + 16, 8, col);
    display.fillCircle(x + 18, y + 16, 3, col);
  } else if (id == M_STUDY) { // book
    display.drawRect(x + 5, y + 7, 13, 20, col);
    display.drawRect(x + 18, y + 7, 13, 20, col);
    display.drawLine(x + 18, y + 7, x + 18, y + 27, col);
    display.drawLine(x + 8, y + 12, x + 15, y + 12, col);
    display.drawLine(x + 21, y + 12, x + 28, y + 12, col);
  } else if (id == M_GARDEN) { // raised food bed with a crop and root vegetable
    display.drawRoundRect(x + 4, y + 21, 28, 8, 2, col);
    display.drawFastHLine(x + 6, y + 23, 24, col);
    display.drawLine(x + 12, y + 22, x + 12, y + 9, col);
    display.drawCircle(x + 8, y + 12, 4, col);
    display.drawCircle(x + 16, y + 10, 4, col);
    display.fillTriangle(x + 25, y + 16, x + 31, y + 16, x + 28, y + 22, col);
    display.drawLine(x + 28, y + 16, x + 28, y + 11, col);
    display.drawLine(x + 28, y + 13, x + 24, y + 10, col);
    display.drawLine(x + 28, y + 13, x + 32, y + 10, col);
    display.drawPixel(x + 10, y + 25, inv ? WHITE : BLACK);
    display.drawPixel(x + 22, y + 26, inv ? WHITE : BLACK);
  } else if (id == M_PLANTROOM) { // arranged flower in a ceramic pot
    display.drawRoundRect(x + 10, y + 22, 16, 7, 3, col);
    display.drawFastHLine(x + 12, y + 24, 12, inv ? WHITE : BLACK);
    display.drawLine(x + 18, y + 22, x + 18, y + 11, col);
    display.drawLine(x + 18, y + 17, x + 12, y + 14, col);
    display.drawLine(x + 18, y + 18, x + 24, y + 14, col);
    display.drawTriangle(x + 12, y + 14, x + 7, y + 12, x + 13, y + 11, col);
    display.drawTriangle(x + 24, y + 14, x + 23, y + 11, x + 29, y + 12, col);
    display.drawCircle(x + 18, y + 8, 3, col);
    display.drawCircle(x + 14, y + 8, 3, col);
    display.drawCircle(x + 22, y + 8, 3, col);
    display.drawCircle(x + 18, y + 4, 3, col);
    display.drawPixel(x + 18, y + 8, inv ? WHITE : BLACK);
  } else if (id == M_MUSIC || id == M_PLAYER) { // note
    display.drawLine(x + 14, y + 7, x + 14, y + 23, col);
    display.drawLine(x + 14, y + 7, x + 26, y + 10, col);
    display.fillCircle(x + 10, y + 24, 5, col);
    display.fillCircle(x + 26, y + 27, 2, col);
  } else if (id == M_STATS) { // chart
    display.drawRect(x + 5, y + 6, 27, 22, col);
    display.fillRect(x + 10, y + 20, 3, 5, col);
    display.fillRect(x + 17, y + 15, 3, 10, col);
    display.fillRect(x + 24, y + 10, 3, 15, col);
  } else if (id == M_SHOP) { // shop bag
    display.drawLine(x + 10, y + 7, x + 5, y + 14, col);
    display.drawLine(x + 26, y + 7, x + 31, y + 14, col);
    display.drawRect(x + 10, y + 7, 16, 20, col);
    display.drawLine(x + 14, y + 7, x + 18, y + 12, col);
    display.drawLine(x + 22, y + 7, x + 18, y + 12, col);
  } else if (id == M_TOY) { // ball
    display.drawCircle(x + 18, y + 17, 12, col);
    display.drawLine(x + 8, y + 17, x + 28, y + 17, col);
    display.drawLine(x + 18, y + 7, x + 18, y + 27, col);
  } else if (id == M_EXPLORE) { // map
    display.drawRect(x + 5, y + 7, 26, 20, col);
    display.drawLine(x + 13, y + 7, x + 13, y + 27, col);
    display.drawLine(x + 22, y + 7, x + 22, y + 27, col);
    display.drawPixel(x + 26, y + 12, col); display.drawPixel(x + 10, y + 22, col);
  } else if (id == M_COOK) { // pot
    display.drawRoundRect(x + 7, y + 14, 23, 12, 4, col);
    display.drawLine(x + 11, y + 12, x + 26, y + 12, col);
    display.drawLine(x + 12, y + 8, x + 12, y + 4, col);
    display.drawLine(x + 20, y + 8, x + 20, y + 3, col);
  } else if (id == M_JOB) { // briefcase
    display.drawRoundRect(x + 5, y + 12, 27, 15, 3, col);
    display.drawRect(x + 13, y + 7, 10, 5, col);
    display.drawLine(x + 5, y + 18, x + 32, y + 18, col);
  } else if (id == M_ROOM) { // house
    display.drawTriangle(x + 4, y + 15, x + 18, y + 5, x + 32, y + 15, col);
    display.drawRect(x + 8, y + 15, 21, 14, col);
    display.drawRect(x + 16, y + 20, 5, 9, col);
  } else if (id == M_AQUARIUM) {
    display.drawRoundRect(x + 3, y + 6, 30, 23, 4, col);
    display.drawLine(x + 5, y + 12, x + 31, y + 12, col);
    display.fillCircle(x + 19, y + 20, 4, col);
    display.fillTriangle(x + 15, y + 20, x + 9, y + 15, x + 9, y + 25, col);
    display.drawPixel(x + 21, y + 19, inv ? WHITE : BLACK);
    display.drawCircle(x + 28, y + 16, 2, col);
  } else if (id == M_LIFE) {
    lifeWorld.drawLifeIcon(x, y, inv);
  } else if (id == M_PARADISE) {
    display.drawCircle(x + 18, y + 17, 13, col);
    display.drawLine(x + 6, y + 14, x + 30, y + 20, col);
    display.drawLine(x + 10, y + 25, x + 24, y + 6, col);
    display.drawCircle(x + 18, y + 17, 4, col);
    display.drawPixel(x + 30, y + 4, col);
    display.drawPixel(x + 33, y + 8, col);
  } else { // talk
    display.drawRoundRect(x + 4, y + 7, 28, 17, 5, col);
    display.drawLine(x + 13, y + 24, x + 8, y + 29, col);
    display.drawLine(x + 11, y + 15, x + 25, y + 15, col);
  }
}


const __FlashStringHelper* menuShortTitle(byte id) {
  if (id == M_NEIGHBORS) return F("TOWN");
  if (id == M_FOOD) return F("FOOD");
  if (id == M_LIGHT) return sleeping ? F("SLEEP") : F("BED");
  if (id == M_GAME) return F("GAME");
  if (id == M_MED) return F("MED");
  if (id == M_CLEAN) return F("WASH");
  if (id == M_LOVE) return F("LOVE");
  if (id == M_CALL) return F("CALL");
  if (id == M_WALK) return F("WALK");
  if (id == M_TRAIN) return F("TRN");
  if (id == M_STUDY) return F("BOOK");
  if (id == M_GARDEN) return F("GROW");
  if (id == M_MUSIC) return F("MUS");
  if (id == M_PLAYER) return F("PLAYER");
  if (id == M_STATS) return F("STAT");
  if (id == M_SHOP) return F("SHOP");
  if (id == M_TOY) return F("TOY");
  if (id == M_EXPLORE) return F("MAP");
  if (id == M_COOK) return F("COOK");
  if (id == M_JOB) return F("JOB");
  if (id == M_ROOM) return F("ROOM");
  if (id == M_SETTINGS) return F("Settings");
  if (id == M_ALBUM) return F("Album");
  if (id == M_SITTER) return F("Sitter");
  if (id == M_NEW_EGG) return F("New egg");
  if (id == M_AQUARIUM) return F("FISH");
  if (id == M_PLANTROOM) return F("PLANTS");
  if (id == M_PARADISE) return F("LAB");
  if (id == M_LIFE) return F("LIFE");
  return F("TALK");
}

const __FlashStringHelper* menuTitle(byte id) {
  if (id == M_NEIGHBORS) return F("Neighbors");
  if (id == M_FOOD) return F("Food");
  if (id == M_LIGHT) return sleeping ? F("Sleeping") : F("Bedtime");
  if (id == M_GAME) return F("Games");
  if (id == M_MED) return F("Medicine");
  if (id == M_CLEAN) return F("Clean");
  if (id == M_LOVE) return F("Love");
  if (id == M_CALL) return F("Call");
  if (id == M_WALK) return F("Walk");
  if (id == M_TRAIN) return F("Train");
  if (id == M_STUDY) return F("Study");
  if (id == M_GARDEN) return F("Garden");
  if (id == M_MUSIC) return F("Music");
  if (id == M_PLAYER) return F("Music Player");
  if (id == M_STATS) return F("Stats");
  if (id == M_SHOP) return F("Shop");
  if (id == M_TOY) return F("Toys");
  if (id == M_EXPLORE) return F("Explore");
  if (id == M_COOK) return F("Cook");
  if (id == M_JOB) return F("Job");
  if (id == M_ROOM) return F("Room");
  if (id == M_SETTINGS) return F("Settings");
  if (id == M_ALBUM) return F("Album");
  if (id == M_SITTER) return F("Sitter");
  if (id == M_NEW_EGG) return F("New egg");
  if (id == M_BOOK) return F("Tama Book");
  if (id == M_CRAFT) return F("DIY Craft");
  if (id == M_HABITAT) return F("Habitat");
  if (id == M_EVENT) return F("Season Event");
  if (id == M_FAMILY) return F("Family");
  if (id == M_INSPECT) return F("Inspect");
  if (id == M_HISTORY) return F("Care Log");
  if (id == M_DIAG) return F("Diagnostics");
  if (id == M_DIARY) return F("Diary");
  if (id == M_AQUARIUM) return F("Aquarium");
  if (id == M_PLANTROOM) return F("Plant Room");
  if (id == M_PARADISE) return F("Paradise Lab");
  if (id == M_LIFE) return F("Living World");
  return F("Talk");
}

const __FlashStringHelper* menuDescription(byte id) {
  if (id == M_NEIGHBORS) return F("Friendship and dates");
  if (id == M_FOOD) return F("Meals/snack");
  if (id == M_LIGHT) return F("Sleep time");
  if (id == M_GAME) return F("All games");
  if (id == M_MED) return F("Treat sick");
  if (id == M_CLEAN) return F("Bath/clean");
  if (id == M_LOVE) return F("Bond time");
  if (id == M_CALL) return F("Answer call");
  if (id == M_WALK) return F("Go outside");
  if (id == M_TRAIN) return F("Build skill");
  if (id == M_STUDY) return F("Learn");
  if (id == M_GARDEN) return F("Grow food plants");
  if (id == M_MUSIC) return F("Play music");
  if (id == M_PLAYER) return F("MP3 from SD card");
  if (id == M_STATS) return F("Needs/info");
  if (id == M_SHOP) return F("Buy items");
  if (id == M_TOY) return F("Use toys");
  if (id == M_EXPLORE) return F("Adventure");
  if (id == M_COOK) return F("Cook meal");
  if (id == M_JOB) return F("Career");
  if (id == M_ROOM) return F("Decorate");
  if (id == M_SETTINGS) return F("Options");
  if (id == M_ALBUM) return F("Past Tamas");
  if (id == M_SITTER) return F("Care help");
  if (id == M_NEW_EGG) return F("Fresh egg");
  if (id == M_BOOK) return F("Species");
  if (id == M_CRAFT) return F("Make items");
  if (id == M_HABITAT) return F("Change area");
  if (id == M_EVENT) return F("Season day");
  if (id == M_FAMILY) return F("Family");
  if (id == M_INSPECT) return F("Profile");
  if (id == M_HISTORY) return F("Care log");
  if (id == M_DIAG) return F("System info");
  if (id == M_DIARY) return F("Memories");
  if (id == M_AQUARIUM) return F("Fish care");
  if (id == M_PLANTROOM) return F("Grow and sell decor");
  if (id == M_PARADISE) return F("Planet lab");
  if (id == M_LIFE) return F("Life world");
  return F("Talk/bond");
}

const __FlashStringHelper* menuUnlockShort(byte action) {
  if (stage == STAGE_EGG) return F("AFTER HATCH");
  if (action == M_GAME) return F("NEEDS BABY");
  if (action == M_WALK || action == M_TRAIN || action == M_MUSIC ||
      action == M_TOY || action == M_GARDEN || action == M_PLANTROOM ||
      action == M_AQUARIUM || action == M_NEIGHBORS) return F("NEEDS CHILD");
  if (action == M_JOB) return F("NEEDS ADULT");
  return F("NEEDS TEEN");
}

void drawRosterFace(int x, int y, byte sc) {
  int cx = x + 16;
  int ey = y + 12;

  if (sick || health < 3 || conditionSeverity >= 2) {
    display.drawLine(cx - 7, ey - 1, cx - 3, ey + 3, BLACK);
    display.drawLine(cx - 3, ey - 1, cx - 7, ey + 3, BLACK);
    display.drawLine(cx + 3, ey - 1, cx + 7, ey + 3, BLACK);
    display.drawLine(cx + 7, ey - 1, cx + 3, ey + 3, BLACK);
    display.drawLine(cx - 4, ey + 9, cx + 4, ey + 9, BLACK);
    display.drawPixel(cx - 5, ey + 10, BLACK);
    display.drawPixel(cx + 5, ey + 10, BLACK);
    return;
  }

  if (energy <= 2) { // sleepy half-lids
    display.drawLine(cx - 8, ey + 1, cx - 3, ey + 1, BLACK);
    display.drawLine(cx + 3, ey + 1, cx + 8, ey + 1, BLACK);
    display.drawPixel(cx + 11, ey - 4, BLACK);
    display.drawPixel(cx + 13, ey - 6, BLACK);
  } else if (clean <= 2) { // embarrassed/squinting
    display.drawLine(cx - 8, ey - 1, cx - 3, ey + 2, BLACK);
    display.drawLine(cx + 3, ey + 2, cx + 8, ey - 1, BLACK);
    display.drawPixel(cx - 12, ey + 5, BLACK);
    display.drawPixel(cx + 12, ey + 4, BLACK);
  } else if (happy >= 8 && ownerBond >= 45) { // bright sparkle eyes
    display.fillRect(cx - 7, ey - 1, 3, 4, BLACK);
    display.fillRect(cx + 4, ey - 1, 3, 4, BLACK);
    display.drawPixel(cx - 6, ey - 2, WHITE);
    display.drawPixel(cx + 5, ey - 2, WHITE);
  } else if (temperament == 4 && happy >= 5) { // mischievous eyebrows
    display.drawLine(cx - 8, ey - 2, cx - 3, ey, BLACK);
    display.drawLine(cx + 3, ey, cx + 8, ey - 2, BLACK);
    display.fillRect(cx - 6, ey + 1, 2, 2, BLACK);
    display.fillRect(cx + 4, ey + 1, 2, 2, BLACK);
  } else {
    display.fillRect(cx - 6, ey, 2, 3, BLACK);
    display.fillRect(cx + 5, ey, 2, 3, BLACK);
  }

  if (full <= 2) { // hungry worried mouth + drool
    display.drawCircle(cx, ey + 8, 2, BLACK);
    display.drawPixel(cx + 5, ey + 8, BLACK);
    display.drawPixel(cx + 6, ey + 10, BLACK);
  } else if (happy >= 8) {
    display.drawPixel(cx - 4, ey + 6, BLACK);
    display.drawLine(cx - 3, ey + 7, cx + 3, ey + 7, BLACK);
    display.drawPixel(cx + 4, ey + 6, BLACK);
  } else if (happy <= 3 || pendingCallType != 0) {
    display.drawLine(cx - 4, ey + 9, cx + 4, ey + 9, BLACK);
    display.drawPixel(cx - 5, ey + 10, BLACK);
    display.drawPixel(cx + 5, ey + 10, BLACK);
  } else {
    display.drawLine(cx - 2, ey + 7, cx + 2, ey + 7, BLACK);
  }

  if (ownerBond >= 70 && happy >= 6) {
    display.drawPixel(cx - 11, ey + 6, BLACK);
    display.drawPixel(cx + 11, ey + 6, BLACK);
  }
}



void drawAdultExtra(int x, int y, byte sc) {
  // v16.5: the uploaded 32x32 sprites already include their unique shapes.
  // Keep only tiny optional stage/detail accents so we do not ruin the sprite art.
  if ((speciesType % 12) == 8) {
    display.drawPixel(x + 12, y + 8, BLACK);
    display.drawPixel(x + 20, y + 9, BLACK);
  } else if ((speciesType % 12) == 3) {
    display.drawPixel(x + 10, y + 5, WHITE);
    display.drawPixel(x + 22, y + 5, WHITE);
  }
}


byte tinyHash(byte a, byte b) {
  byte x = a * 37 + b * 17 + 91;
  x ^= x << 3;
  x ^= x >> 5;
  return x;
}

byte statBirthRoll(byte low, byte high) {
  if (high <= low) return low;
  return low + random(0, high - low + 1);
}

void rollEggMenu() {
  // Four unique lineage eggs each hatch into one of ten original young forms.
  byte used[4] = {255, 255, 255, 255};

  for (byte i = 0; i < 4; i++) {
    byte e;
    bool unique;
    do {
      e = random(0, TAMACHIBI_FAMILY_COUNT);
      unique = true;
      for (byte j = 0; j < i; j++) {
        if (used[j] == e) unique = false;
      }
    } while (!unique);

    used[i] = e;
    eggChoice[i] = e;
  }
}


void rollPetGenes(byte chosenEgg) {
  eggType = chosenEgg % TAMACHIBI_FAMILY_COUNT;
  birthCode = ((unsigned int)random(0, 256) << 8) | (unsigned int)random(0, 256);

  // The hatch keeps its family silhouette until care selects one of its adult
  // branches. Each family still has five distinct mature roster outcomes.
  speciesType = eggType;

  // Rare surprise hatch.
  if (random(0, 50) == 0) {
    speciesType = random(0, TAMACHIBI_FAMILY_COUNT);
    eggType = speciesType;
  }

  // Kept as compact compatibility values for save data and small movement flavor.
  bodyType = speciesType % 12;
  eyeType = 0;
  mouthType = 0;
  patternType = 0;
  headTrait = 0;
  tailType = 0;
  idleStyle = speciesType % 6;
  birthMark = 0;

  geneA = speciesType % 8;
  geneB = (speciesType / 2) % 8;
  geneC = (speciesType / 3) % 8;
  personality = speciesType % 8;
  statBias = speciesType % 8;

  rareAllRounder = (random(0, 64) == 0);
}


void applyBirthStats() {
  if (rareAllRounder) {
    health = statBirthRoll(8, 10);
    full = statBirthRoll(8, 10);
    energy = statBirthRoll(8, 10);
    happy = statBirthRoll(8, 10);
    clean = statBirthRoll(8, 10);
    discipline = statBirthRoll(7, 10);
    skillSmart = statBirthRoll(1, 3);
    skillStyle = statBirthRoll(1, 3);
    skillKind = statBirthRoll(1, 3);
    skillFit = statBirthRoll(1, 3);
    coins = 12 + random(0, 9);
    return;
  }

  health = statBirthRoll(5, 8);
  full = statBirthRoll(4, 7);
  energy = statBirthRoll(4, 7);
  happy = statBirthRoll(4, 7);
  clean = statBirthRoll(4, 8);
  discipline = statBirthRoll(3, 7);
  coins = 4 + random(0, 8);

  applySpeciesBirthBonus();

  // Extra random stat bias so two pets of the same species can still feel different.
  byte extra = random(0, 8);
  if (extra == 0) addStat(health, 1);
  else if (extra == 1) addStat(full, 1);
  else if (extra == 2) addStat(energy, 1);
  else if (extra == 3) addStat(happy, 1);
  else if (extra == 4) addStat(clean, 1);
  else if (extra == 5) addStat(discipline, 1);
  else if (extra == 6) addSkill(skillSmart, 1);
  else addSkill(skillStyle, 1);

  clampAll();
}


void drawEggShape(int x, int y, byte type, bool selected) {
  if (selected) display.fillRoundRect(x - 4, y - 4, 28, 31, 5, WHITE);
  else display.drawRoundRect(x - 4, y - 4, 28, 31, 5, WHITE);

  uint16_t col = selected ? BLACK : WHITE;

  display.drawCircle(x + 10, y + 14, 10, col);
  display.drawCircle(x + 10, y + 9, 7, col);
  display.drawLine(x + 3, y + 13, x + 17, y + 13, col);

  byte pat = type % 10;
  if (pat == 0) display.drawLine(x + 5, y + 17, x + 15, y + 17, col);
  else if (pat == 1) {
    display.drawPixel(x + 6, y + 10, col); display.drawPixel(x + 14, y + 15, col); display.drawPixel(x + 9, y + 20, col);
  } else if (pat == 2) {
    display.drawTriangle(x + 10, y + 8, x + 5, y + 19, x + 15, y + 19, col);
    display.drawPixel(x + 10, y + 14, selected ? WHITE : BLACK);
  } else if (pat == 3) {
    display.drawLine(x + 4, y + 16, x + 7, y + 13, col); display.drawLine(x + 7, y + 13, x + 10, y + 16, col);
    display.drawLine(x + 10, y + 16, x + 13, y + 13, col); display.drawLine(x + 13, y + 13, x + 16, y + 16, col);
  } else if (pat == 4) display.drawCircle(x + 10, y + 15, 3, col);
  else if (pat == 5) { display.drawLine(x + 5, y + 11, x + 15, y + 11, col); display.drawLine(x + 5, y + 19, x + 15, y + 19, col); }
  else if (pat == 6) display.drawRect(x + 7, y + 12, 7, 7, col);
  else if (pat == 7) { display.drawPixel(x + 7, y + 13, col); display.drawPixel(x + 10, y + 16, col); display.drawPixel(x + 13, y + 13, col); }
  else if (pat == 8) display.drawRoundRect(x + 6, y + 12, 9, 8, 3, col);
  else { display.drawLine(x + 7, y + 10, x + 13, y + 20, col); display.drawLine(x + 13, y + 10, x + 7, y + 20, col); }
}


void chooseEgg() {
  rollEggMenu();
  byte e = random(0, 4);
  bool oldLeft = HIGH;
  bool oldSel = HIGH;
  bool oldRight = HIGH;

  while (true) {
    bool l = digitalRead(BTN_L);
    bool s = digitalRead(BTN_S);
    bool r = digitalRead(BTN_R);

    if (oldLeft == HIGH && l == LOW) {
      e = (e == 0) ? 3 : e - 1;
      clickSound();
      delay(120);
    }
    if (oldRight == HIGH && r == LOW) {
      e++;
      if (e > 3) e = 0;
      clickSound();
      delay(120);
    }
    if (oldSel == HIGH && s == LOW) {
      goodSound();
      randomSeed(micros() + millis() + e * 97 + eggChoice[e] * 31);
      rollPetGenes(eggChoice[e]);
      waitButtonRelease();
      return;
    }

    oldLeft = l;
    oldSel = s;
    oldRight = r;

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(11, 0);
    display.print(F("Choose unique egg"));
    display.setCursor(18, 10);
    display.print(F("D0  D1  D2"));

    drawEggShape(9, 28, eggChoice[0], e == 0);
    drawEggShape(39, 28, eggChoice[1], e == 1);
    drawEggShape(69, 28, eggChoice[2], e == 2);
    drawEggShape(99, 28, eggChoice[3], e == 3);

    display.setCursor(6, 55);
    byte shown = eggChoice[e];
    if (shown == 0) display.print(F("Skyline egg"));
    else if (shown == 1) display.print(F("Tidepool egg"));
    else if (shown == 2) display.print(F("Pouncer egg"));
    else if (shown == 3) display.print(F("Shade egg"));
    else if (shown == 4) display.print(F("Crumb egg"));
    else if (shown == 5) display.print(F("Horn egg"));
    else if (shown == 6) display.print(F("Pup egg"));
    else if (shown == 7) display.print(F("Sprout egg"));
    else if (shown == 8) display.print(F("Tinker egg"));
    else display.print(F("Gearling egg"));

    display.display();
    delay(25);
  }
}
void drawSparkles() {
  if (sleeping || lightOff) {
    display.drawPixel(13, 28, WHITE);
    display.drawPixel(25, 38, WHITE);
    display.drawPixel(110, 30, WHITE);
    if (anim & 1) {
      display.drawLine(15, 24, 15, 28, WHITE);
      display.drawLine(13, 26, 17, 26, WHITE);
      display.drawLine(105, 36, 105, 40, WHITE);
      display.drawLine(103, 38, 107, 38, WHITE);
    }
  } else if (happy > 7 && !sick && !dead) {
    display.drawPixel(19, 34, WHITE);
    display.drawPixel(111, 32, WHITE);
    if (anim & 1) {
      display.drawCircle(23, 39, 2, WHITE);
      display.drawCircle(106, 43, 2, WHITE);
    }
  }
}

void drawPoopPile(int x, int y) {
  display.fillRoundRect(x, y + 8, 13, 5, 2, WHITE);
  display.fillRoundRect(x + 2, y + 5, 10, 4, 2, WHITE);
  display.fillRoundRect(x + 5, y + 2, 6, 4, 2, WHITE);
  display.drawLine(x + 3, y + 8, x + 10, y + 8, BLACK);
  display.drawLine(x + 5, y + 5, x + 10, y + 5, BLACK);
  display.drawPixel(x + 4, y + 10, BLACK);
  display.drawPixel(x + 9, y + 10, BLACK);
}

void drawPoops() {
  byte visiblePoop = poop;
  if (visiblePoop == 0 && clean <= 5) visiblePoop = 1;
  if (visiblePoop > 0) drawPoopPile(5, 43);
  if (visiblePoop > 1) drawPoopPile(105, 43);
  if (visiblePoop > 2) drawPoopPile(22, 47);
}

void drawClearEyes(int cx, int y, int bob, byte style, bool onWhiteBody) {
  uint16_t eyeCol = onWhiteBody ? BLACK : WHITE;
  uint16_t shineCol = onWhiteBody ? WHITE : BLACK;

  // Always draw visible eyes. No variant is allowed to accidentally hide the face.
  if (style == 0) {
    display.fillCircle(cx - 9, y + 15 + bob, 2, eyeCol);
    display.fillCircle(cx + 9, y + 15 + bob, 2, eyeCol);
  } else if (style == 1) {
    display.drawCircle(cx - 9, y + 15 + bob, 3, eyeCol);
    display.drawCircle(cx + 9, y + 15 + bob, 3, eyeCol);
    display.drawPixel(cx - 8, y + 14 + bob, eyeCol);
    display.drawPixel(cx + 10, y + 14 + bob, eyeCol);
  } else if (style == 2) {
    display.fillRect(cx - 12, y + 13 + bob, 5, 4, eyeCol);
    display.fillRect(cx + 7, y + 13 + bob, 5, 4, eyeCol);
  } else if (style == 3) {
    display.drawLine(cx - 13, y + 13 + bob, cx - 7, y + 16 + bob, eyeCol);
    display.drawLine(cx + 7, y + 16 + bob, cx + 13, y + 13 + bob, eyeCol);
  } else if (style == 4) {
    display.drawTriangle(cx - 13, y + 17 + bob, cx - 9, y + 12 + bob, cx - 5, y + 17 + bob, eyeCol);
    display.drawTriangle(cx + 5, y + 17 + bob, cx + 9, y + 12 + bob, cx + 13, y + 17 + bob, eyeCol);
  } else {
    display.fillCircle(cx - 9, y + 15 + bob, 2, eyeCol);
    display.drawCircle(cx + 9, y + 15 + bob, 2, eyeCol);
  }

  if (!onWhiteBody && happy > 7) {
    display.drawPixel(cx - 8, y + 14 + bob, shineCol);
    display.drawPixel(cx + 10, y + 14 + bob, shineCol);
  }
}

void drawClearMouth(int cx, int y, int bob, byte style, bool onWhiteBody) {
  uint16_t col = onWhiteBody ? BLACK : WHITE;

  if (happy < 3 || full < 3 || energy < 2) {
    display.drawLine(cx - 5, y + 26 + bob, cx + 5, y + 26 + bob, col);
    display.drawPixel(cx - 6, y + 27 + bob, col);
    display.drawPixel(cx + 6, y + 27 + bob, col);
  } else if (style == 0 || happy > 8) {
    display.drawPixel(cx - 5, y + 24 + bob, col);
    display.drawPixel(cx - 4, y + 25 + bob, col);
    display.drawLine(cx - 3, y + 26 + bob, cx + 3, y + 26 + bob, col);
    display.drawPixel(cx + 4, y + 25 + bob, col);
    display.drawPixel(cx + 5, y + 24 + bob, col);
  } else if (style == 1) display.drawLine(cx - 4, y + 24 + bob, cx + 4, y + 24 + bob, col);
  else if (style == 2) display.drawCircle(cx, y + 25 + bob, 2, col);
  else if (style == 3) {
    display.drawPixel(cx - 3, y + 24 + bob, col);
    display.drawPixel(cx - 1, y + 25 + bob, col);
    display.drawPixel(cx + 1, y + 25 + bob, col);
    display.drawPixel(cx + 3, y + 24 + bob, col);
  } else if (style == 4) {
    display.drawLine(cx - 5, y + 24 + bob, cx, y + 27 + bob, col);
    display.drawLine(cx, y + 27 + bob, cx + 5, y + 24 + bob, col);
  } else {
    display.drawPixel(cx - 3, y + 24 + bob, col);
    display.drawPixel(cx - 1, y + 25 + bob, col);
    display.drawPixel(cx + 1, y + 24 + bob, col);
    display.drawPixel(cx + 3, y + 25 + bob, col);
  }
}

void drawSpeciesPattern(int x, int y, int bob, byte w, byte h, bool onWhiteBody) {
  uint16_t col = onWhiteBody ? BLACK : WHITE;
  int cx = x + w / 2;

  if (patternType == 0) return;
  else if (patternType == 1) { // spots
    display.drawPixel(x + 10, y + 18 + bob, col);
    display.drawPixel(x + w - 11, y + 22 + bob, col);
    display.drawPixel(cx, y + h - 8 + bob, col);
  } else if (patternType == 2) { // belly
    display.drawCircle(cx, y + h - 10 + bob, 4, col);
  } else if (patternType == 3) { // sensible stripes
    display.drawLine(x + 8, y + 15 + bob, x + w - 8, y + 15 + bob, col);
    display.drawLine(x + 10, y + 21 + bob, x + w - 10, y + 21 + bob, col);
  } else if (patternType == 4) { // cheek dots
    display.drawPixel(cx - 17, y + 21 + bob, col);
    display.drawPixel(cx + 17, y + 21 + bob, col);
  } else if (patternType == 5) { // chest mark
    display.drawTriangle(cx - 5, y + h - 12 + bob, cx + 5, y + h - 12 + bob, cx, y + h - 5 + bob, col);
  } else if (patternType == 6) {
    display.drawRect(cx - 4, y + h - 13 + bob, 8, 6, col);
  } else if (patternType == 7) {
    display.drawPixel(cx - 8, y + 19 + bob, col);
    display.drawPixel(cx, y + 21 + bob, col);
    display.drawPixel(cx + 8, y + 19 + bob, col);
  } else {
    display.drawLine(x + 8, y + h - 13 + bob, x + 13, y + h - 8 + bob, col);
    display.drawLine(x + w - 13, y + h - 13 + bob, x + w - 8, y + h - 8 + bob, col);
  }
}

void spriteFace(int cx, int cy, byte mood, byte style) {
  // Filled white sprites use black cut-out faces, closer to classic 1-bit pets.
  uint16_t col = BLACK;

  if (sick || health < 3 || mood == 2) {
    display.drawLine(cx - 10, cy - 4, cx - 6, cy, col);
    display.drawLine(cx - 6, cy - 4, cx - 10, cy, col);
    display.drawLine(cx + 6, cy - 4, cx + 10, cy, col);
    display.drawLine(cx + 10, cy - 4, cx + 6, cy, col);
    display.drawLine(cx - 5, cy + 8, cx + 5, cy + 8, col);
    return;
  }

  // Eyes
  if (style == 0) {
    display.fillRect(cx - 10, cy - 3, 3, 4, col);
    display.fillRect(cx + 7, cy - 3, 3, 4, col);
  } else if (style == 1) {
    display.fillCircle(cx - 9, cy - 2, 2, col);
    display.fillCircle(cx + 9, cy - 2, 2, col);
  } else if (style == 2) {
    display.drawLine(cx - 12, cy - 3, cx - 7, cy - 3, col);
    display.drawLine(cx + 7, cy - 3, cx + 12, cy - 3, col);
  } else if (style == 3) {
    display.fillRect(cx - 12, cy - 4, 5, 3, col);
    display.fillRect(cx + 7, cy - 4, 5, 3, col);
  } else {
    display.fillRect(cx - 11, cy - 4, 3, 5, col);
    display.drawRect(cx + 8, cy - 4, 4, 5, col);
  }

  // Mouth
  if (happy < 3 || full < 3 || energy < 2 || mood == 1) {
    display.drawLine(cx - 5, cy + 8, cx + 5, cy + 8, col);
    display.drawPixel(cx - 6, cy + 9, col);
    display.drawPixel(cx + 6, cy + 9, col);
  } else if (happy > 7) {
    display.drawPixel(cx - 5, cy + 5, col);
    display.drawPixel(cx - 4, cy + 6, col);
    display.drawLine(cx - 3, cy + 7, cx + 3, cy + 7, col);
    display.drawPixel(cx + 4, cy + 6, col);
    display.drawPixel(cx + 5, cy + 5, col);
  } else if (mouthType == 1) {
    display.drawLine(cx - 4, cy + 5, cx + 4, cy + 5, col);
  } else if (mouthType == 2) {
    display.drawCircle(cx, cy + 6, 2, col);
  } else {
    display.drawPixel(cx - 3, cy + 5, col);
    display.drawPixel(cx - 2, cy + 6, col);
    display.drawPixel(cx - 1, cy + 6, col);
    display.drawPixel(cx, cy + 6, col);
    display.drawPixel(cx + 1, cy + 6, col);
    display.drawPixel(cx + 2, cy + 5, col);
  }
}

void spritePattern(int x, int y, byte w, byte h) {
  // Controlled patterns that read clearly on 128x64.
  uint16_t col = BLACK;
  int cx = x + w / 2;

  if (patternType == 0) return;
  if (patternType == 1) {
    display.drawPixel(x + 10, y + h / 2, col);
    display.drawPixel(x + w - 11, y + h / 2 + 4, col);
    display.drawPixel(cx, y + h - 8, col);
  } else if (patternType == 2) {
    display.drawCircle(cx, y + h - 10, 4, col);
  } else if (patternType == 3) {
    display.drawLine(x + 8, y + h / 2, x + w - 8, y + h / 2, col);
    display.drawLine(x + 10, y + h / 2 + 6, x + w - 10, y + h / 2 + 6, col);
  } else if (patternType == 4) {
    display.drawPixel(cx - 16, y + h / 2 + 5, col);
    display.drawPixel(cx + 16, y + h / 2 + 5, col);
  } else if (patternType == 5) {
    display.drawTriangle(cx - 5, y + h - 12, cx + 5, y + h - 12, cx, y + h - 5, col);
  } else if (patternType == 6) {
    display.drawRect(cx - 4, y + h - 13, 8, 6, col);
  } else {
    display.drawLine(x + 8, y + h - 13, x + 13, y + h - 8, col);
    display.drawLine(x + w - 13, y + h - 13, x + w - 8, y + h - 8, col);
  }
}

void drawSpriteFoot(int x, int y) {
  display.fillRect(x, y, 7, 3, WHITE);
}

void spriteTinyArms(int x, int y, byte w, byte h) {
  if (anim & 1) {
    display.fillRect(x - 5, y + h / 2, 6, 3, WHITE);
    display.fillRect(x + w - 1, y + h / 2 - 2, 7, 3, WHITE);
  } else {
    display.fillRect(x - 6, y + h / 2 + 2, 6, 3, WHITE);
    display.fillRect(x + w, y + h / 2 + 2, 6, 3, WHITE);
  }
}

void drawFace(int x, int y, int bob, byte w) {
  int cx = x + w / 2;
  bool onWhiteBody = (speciesType == 7); // poo is filled white, so use black facial pixels.

  if (sick || health < 3) {
    uint16_t col = onWhiteBody ? BLACK : WHITE;
    display.drawLine(cx - 12, y + 13 + bob, cx - 7, y + 17 + bob, col);
    display.drawLine(cx - 7, y + 13 + bob, cx - 12, y + 17 + bob, col);
    display.drawLine(cx + 7, y + 13 + bob, cx + 12, y + 17 + bob, col);
    display.drawLine(cx + 12, y + 13 + bob, cx + 7, y + 17 + bob, col);
    display.drawLine(cx - 5, y + 26 + bob, cx + 5, y + 26 + bob, col);
    return;
  }

  // The old version had some random eye styles that could look missing.
  // This version always renders a readable pair of eyes.
  drawClearEyes(cx, y, bob, eyeType % 6, onWhiteBody);
  drawClearMouth(cx, y, bob, mouthType % 6, onWhiteBody);

  uint16_t markCol = onWhiteBody ? BLACK : WHITE;
  if (birthMark == 1) display.drawPixel(cx - 17, y + 21 + bob, markCol);
  else if (birthMark == 2) display.drawPixel(cx + 17, y + 21 + bob, markCol);
  else if (birthMark == 3) display.drawCircle(cx - 17, y + 21 + bob, 1, markCol);
  else if (birthMark == 4) display.drawLine(cx + 15, y + 20 + bob, cx + 18, y + 23 + bob, markCol);
  else if (birthMark == 5) display.drawPixel(cx, y + 12 + bob, markCol);
  else if (birthMark == 6) {
    display.drawPixel(cx - 14, y + 12 + bob, markCol);
    display.drawPixel(cx + 14, y + 12 + bob, markCol);
  }
}

void drawTraits(int x, int y, int bob, byte w, byte h) {
  int cx = x + w / 2;
  bool onWhiteBody = (speciesType == 7);

  // Species-specific details instead of nonsense mixed parts.
  if (speciesType == 0) { // whale: spout + fin
    if (anim & 1) {
      display.drawLine(cx, y - 4 + bob, cx, y - 9 + bob, WHITE);
      display.drawLine(cx, y - 9 + bob, cx - 4, y - 12 + bob, WHITE);
      display.drawLine(cx, y - 9 + bob, cx + 4, y - 12 + bob, WHITE);
    }
    display.drawTriangle(x + 5, y + 18 + bob, x - 5, y + 12 + bob, x - 2, y + 25 + bob, WHITE);
  } else if (speciesType == 1) { // fish: dorsal fin
    display.drawTriangle(cx - 4, y + 2 + bob, cx, y - 7 + bob, cx + 4, y + 2 + bob, WHITE);
  } else if (speciesType == 2) { // jellyfish: tentacles
    for (byte i = 0; i < 4; i++) {
      int tx = x + 8 + i * 7;
      display.drawLine(tx, y + h + bob - 1, tx - 2 + (anim & 1) * 4, y + h + 7 + bob, WHITE);
    }
  } else if (speciesType == 3) { // octopus: legs
    for (byte i = 0; i < 5; i++) {
      int tx = x + 5 + i * 7;
      display.drawLine(tx, y + h + bob - 2, tx - 2 + ((anim + i) & 1) * 4, y + h + 5 + bob, WHITE);
    }
  } else if (speciesType == 4) { // turtle: shell lines
    display.drawCircle(cx, y + h / 2 + bob, w / 4, WHITE);
    display.drawLine(cx - 8, y + h / 2 + bob, cx + 8, y + h / 2 + bob, WHITE);
  } else if (speciesType == 5) { // crab: claws
    display.drawCircle(x - 5, y + 12 + bob, 4, WHITE);
    display.drawCircle(x + w + 5, y + 12 + bob, 4, WHITE);
    display.drawLine(x - 1, y + 15 + bob, x + 4, y + 18 + bob, WHITE);
    display.drawLine(x + w + 1, y + 15 + bob, x + w - 4, y + 18 + bob, WHITE);
  } else if (speciesType == 6) { // seal: whiskers
    display.drawLine(cx - 7, y + 21 + bob, cx - 16, y + 18 + bob, WHITE);
    display.drawLine(cx - 7, y + 23 + bob, cx - 16, y + 24 + bob, WHITE);
    display.drawLine(cx + 7, y + 21 + bob, cx + 16, y + 18 + bob, WHITE);
    display.drawLine(cx + 7, y + 23 + bob, cx + 16, y + 24 + bob, WHITE);
  } else if (speciesType == 7) { // poo: shine
    display.drawPixel(cx - 7, y + 9 + bob, BLACK);
    display.drawPixel(cx - 5, y + 10 + bob, BLACK);
  } else if (speciesType == 8) { // robot: antenna
    display.drawLine(cx, y + bob, cx, y - 8 + bob, WHITE);
    display.drawCircle(cx, y - 10 + bob, 2, WHITE);
  } else if (speciesType == 9) { // ghost: wavy bottom
    display.drawTriangle(x + 4, y + h - 2 + bob, x + 10, y + h - 7 + bob, x + 16, y + h - 2 + bob, WHITE);
    display.drawTriangle(x + w - 16, y + h - 2 + bob, x + w - 10, y + h - 7 + bob, x + w - 4, y + h - 2 + bob, WHITE);
  } else if (speciesType == 10) { // dino: spikes
    display.drawTriangle(cx - 12, y + 2 + bob, cx - 8, y - 5 + bob, cx - 4, y + 2 + bob, WHITE);
    display.drawTriangle(cx, y + 2 + bob, cx + 4, y - 6 + bob, cx + 8, y + 2 + bob, WHITE);
  } else { // blob: cute nubs
    display.drawCircle(x + 6, y + 3 + bob, 4, WHITE);
    display.drawCircle(x + w - 6, y + 5 + bob, 3, WHITE);
  }

  // Tail or back detail after baby stage, appropriate to species.
  if (stage >= 1) {
    if (speciesType == 0 || speciesType == 1 || speciesType == 6) {
      display.drawTriangle(x + w, y + h - 12 + bob, x + w + 12, y + h - 18 + bob, x + w + 12, y + h - 6 + bob, WHITE);
    } else if (speciesType == 4) {
      display.drawCircle(x + w + 4, y + h - 12 + bob, 3, WHITE);
    } else if (speciesType == 10) {
      display.drawLine(x + w, y + h - 12 + bob, x + w + 12, y + h - 18 + bob, WHITE);
      display.drawLine(x + w + 12, y + h - 18 + bob, x + w + 7, y + h - 9 + bob, WHITE);
    } else if (speciesType == 11) {
      display.drawCircle(x + w + 3, y + h - 11 + bob, 2, WHITE);
    }
  }

  // Adult achievement head accessory still appears, but it is restrained.
  if (stage >= 2 && happy > 5) {
    if (headTrait == 0) {
      display.drawLine(cx - 7, y - 8 + bob, cx - 3, y - 13 + bob, WHITE);
      display.drawLine(cx - 3, y - 13 + bob, cx, y - 8 + bob, WHITE);
      display.drawLine(cx, y - 8 + bob, cx + 3, y - 13 + bob, WHITE);
      display.drawLine(cx + 3, y - 13 + bob, cx + 7, y - 8 + bob, WHITE);
    } else if (headTrait == 1) display.drawCircle(cx, y - 10 + bob, 6, WHITE);
    else if (headTrait == 2) display.drawRoundRect(cx - 8, y - 12 + bob, 16, 5, 2, WHITE);
    else if (headTrait == 3) {
      display.drawLine(cx - 5, y - 6 + bob, cx - 2, y - 11 + bob, WHITE);
      display.drawLine(cx + 4, y - 6 + bob, cx + 2, y - 11 + bob, WHITE);
    }
  }

  drawSpeciesPattern(x, y, bob, w, h, onWhiteBody);
}

void drawDeadPet() {
  display.drawRoundRect(42, 27, 44, 27, 8, WHITE);
  display.drawLine(53, 37, 59, 43, WHITE);
  display.drawLine(59, 37, 53, 43, WHITE);
  display.drawLine(69, 37, 75, 43, WHITE);
  display.drawLine(75, 37, 69, 43, WHITE);
  display.drawLine(58, 49, 71, 49, WHITE);
  display.setCursor(48, 18);
  display.print(F("X_X"));
}

void drawSleepPet() {
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(3, 3); display.print(weekdayName(clockWeekday())); display.print(' '); drawClock12Hour(visualHour(), clockMinute());
  display.setCursor(78, 3); display.print(F("Wake ")); byte wh = wakeHour() % 12; if (wh == 0) wh = 12; display.print(wh); display.print(wakeHour() < 12 ? F("AM") : F("PM"));

  display.drawRoundRect(34, 42, 62, 14, 5, WHITE);
  display.drawLine(37, 49, 92, 49, WHITE);

  drawCurrentPetSleepMini(48, 20 + ((anim & 1) ? 1 : 0), 2);

  display.setCursor(90, 23);
  display.print(F("Z"));
  display.setCursor(98, 17);
  display.print(F("z"));
  if (anim & 1) {
    display.setCursor(106, 12);
    display.print(F("z"));
  }
}


void drawRoomBack() {
  // Small room decorations inspired by furniture/custom room suggestions.
  if (roomStyle == 0) return;
  if (roomStyle == 1) {
    display.drawRoundRect(4, 43, 24, 8, 3, WHITE);   // rug
    display.drawLine(7, 47, 25, 47, WHITE);
  } else if (roomStyle == 2) {
    display.drawLine(111, 52, 111, 38, WHITE);       // plant
    display.drawCircle(106, 41, 4, WHITE);
    display.drawCircle(116, 42, 4, WHITE);
    display.drawRect(106, 52, 12, 4, WHITE);
  } else if (roomStyle == 3) {
    display.drawLine(13, 52, 13, 34, WHITE);         // rounded floor lamp
    display.drawRoundRect(7, 27, 12, 8, 3, WHITE);
    display.drawLine(9, 35, 17, 35, WHITE);
    if (!lightOff && !sleeping && (anim & 1)) display.drawCircle(13, 33, 12, WHITE);
  } else if (roomStyle == 4) {
    display.drawRect(104, 33, 18, 20, WHITE);        // shelf
    display.drawLine(104, 40, 122, 40, WHITE);
    display.drawLine(104, 47, 122, 47, WHITE);
    display.drawPixel(109, 37, WHITE);
    display.drawPixel(117, 44, WHITE);
  } else {
    display.drawRoundRect(4, 33, 22, 18, 4, WHITE);  // fancy sofa
    display.drawLine(7, 51, 7, 55, WHITE);
    display.drawLine(23, 51, 23, 55, WHITE);
  }
}



void drawTopArc(int cx, int cy, int rx, int ry, uint16_t col) {
  display.drawLine(cx - rx, cy, cx - (rx * 3) / 4, cy - ry / 2, col);
  display.drawLine(cx - (rx * 3) / 4, cy - ry / 2, cx - rx / 2, cy - (ry * 3) / 4, col);
  display.drawLine(cx - rx / 2, cy - (ry * 3) / 4, cx, cy - ry, col);
  display.drawLine(cx, cy - ry, cx + rx / 2, cy - (ry * 3) / 4, col);
  display.drawLine(cx + rx / 2, cy - (ry * 3) / 4, cx + (rx * 3) / 4, cy - ry / 2, col);
  display.drawLine(cx + (rx * 3) / 4, cy - ry / 2, cx + rx, cy, col);
}

const __FlashStringHelper* outfitName(byte id) {
  switch (id % OUTFIT_COUNT) {
    case 0: return F("No hat");
    case 1: return F("Rain Bonnet");
    case 2: return F("Leaf Crown");
    case 3: return F("Flower Sunhat");
    case 4: return F("Tech Cap");
    case 5: return F("Dragon Hat");
    case 6: return F("Moon Nightcap");
    case 7: return F("Bunny Bonnet");
    case 8: return F("Snow Hat");
    case 9: return F("Idol Bow");
    case 10: return F("Pixel Cap");
    case 11: return F("Scholar Cap");
    case 12: return F("Royal Crown");
    case 13: return F("Chef Toque");
    case 14: return F("Garden Sunhat");
    default: return F("Star Captain");
  }
}

const __FlashStringHelper* outfitRequirementText(byte id) {
  if (id <= 3) return F("starter set");
  if (id <= 6) return F("style/fit 3");
  if (id <= 9) return F("skills 5");
  if (id <= 12) return F("skills 8");
  return F("job rank 2");
}

void drawFancyHat(byte id, int cx, int brimY, uint16_t col) {
  id %= OUTFIT_COUNT;
  if (id == 0) return;
  const uint16_t cut = (col == WHITE) ? BLACK : WHITE;

  if (id == 1) {
    drawTopArc(cx, brimY, 10, 7, col); display.drawLine(cx - 12, brimY, cx + 12, brimY, col);
    display.drawPixel(cx + 7, brimY - 6, col);
  } else if (id == 2) {
    display.drawLine(cx - 11, brimY, cx + 11, brimY, col);
    for (byte i = 0; i < 5; ++i) { const int px = cx - 9 + i * 5; display.drawLine(px, brimY - 1, px - 2 + (i & 1) * 4, brimY - 7, col); display.drawCircle(px - 2 + (i & 1) * 4, brimY - 8, 2, col); }
  } else if (id == 3) {
    drawTopArc(cx, brimY - 1, 9, 6, col); display.drawLine(cx - 14, brimY, cx + 14, brimY, col);
    display.drawCircle(cx - 8, brimY - 6, 3, col); display.drawPixel(cx - 8, brimY - 10, col); display.drawPixel(cx - 12, brimY - 6, col);
  } else if (id == 4) {
    display.drawRoundRect(cx - 10, brimY - 8, 20, 8, 3, col); display.drawLine(cx - 12, brimY, cx + 12, brimY, col);
    display.drawLine(cx + 6, brimY - 8, cx + 11, brimY - 14, col); display.drawCircle(cx + 12, brimY - 15, 2, col);
  } else if (id == 5) {
    drawTopArc(cx, brimY, 10, 7, col); display.drawLine(cx - 11, brimY, cx + 11, brimY, col);
    display.drawTriangle(cx - 8, brimY - 5, cx - 4, brimY - 13, cx, brimY - 5, col); display.drawTriangle(cx, brimY - 6, cx + 4, brimY - 15, cx + 8, brimY - 5, col);
  } else if (id == 6) {
    display.fillTriangle(cx - 8, brimY, cx + 6, brimY, cx + 10, brimY - 14, col); display.fillCircle(cx + 11, brimY - 15, 2, col); display.drawPixel(cx + 2, brimY - 5, cut);
  } else if (id == 7) {
    drawTopArc(cx, brimY, 10, 6, col); display.drawLine(cx - 11, brimY, cx + 11, brimY, col);
    display.drawRoundRect(cx - 8, brimY - 16, 5, 12, 2, col); display.drawRoundRect(cx + 3, brimY - 16, 5, 12, 2, col);
  } else if (id == 8) {
    display.drawRoundRect(cx - 11, brimY - 8, 22, 8, 3, col); display.drawLine(cx - 13, brimY, cx + 13, brimY, col);
    display.fillCircle(cx, brimY - 11, 3, col); display.drawCircle(cx - 13, brimY - 1, 3, col); display.drawCircle(cx + 13, brimY - 1, 3, col);
  } else if (id == 9) {
    display.drawTriangle(cx, brimY - 3, cx - 12, brimY - 10, cx - 10, brimY - 1, col); display.drawTriangle(cx, brimY - 3, cx + 12, brimY - 10, cx + 10, brimY - 1, col);
    display.fillCircle(cx, brimY - 4, 2, col); display.drawLine(cx - 9, brimY, cx + 9, brimY, col);
  } else if (id == 10) {
    display.drawRect(cx - 10, brimY - 8, 19, 8, col); display.drawLine(cx - 12, brimY, cx + 14, brimY, col); display.fillRect(cx + 8, brimY - 3, 7, 3, col); display.drawPixel(cx - 6, brimY - 5, cut);
  } else if (id == 11) {
    display.fillTriangle(cx, brimY - 13, cx - 14, brimY - 7, cx, brimY - 2, col); display.fillTriangle(cx, brimY - 13, cx + 14, brimY - 7, cx, brimY - 2, col);
    display.drawLine(cx - 8, brimY - 2, cx + 8, brimY - 2, col); display.drawLine(cx + 11, brimY - 7, cx + 11, brimY + 1, col); display.fillCircle(cx + 11, brimY + 2, 1, col);
  } else if (id == 12) {
    display.drawLine(cx - 11, brimY, cx + 11, brimY, col); display.drawTriangle(cx - 11, brimY, cx - 7, brimY - 10, cx - 3, brimY, col);
    display.drawTriangle(cx - 4, brimY, cx, brimY - 13, cx + 4, brimY, col); display.drawTriangle(cx + 3, brimY, cx + 7, brimY - 10, cx + 11, brimY, col); display.drawPixel(cx, brimY - 7, col);
  } else if (id == 13) {
    display.drawCircle(cx - 7, brimY - 8, 5, col); display.drawCircle(cx, brimY - 11, 6, col); display.drawCircle(cx + 7, brimY - 8, 5, col); display.drawRect(cx - 11, brimY - 5, 22, 5, col);
  } else if (id == 14) {
    display.drawRoundRect(cx - 9, brimY - 8, 18, 8, 3, col); display.drawLine(cx - 15, brimY, cx + 15, brimY, col);
    display.drawLine(cx - 9, brimY - 3, cx + 9, brimY - 3, col); display.drawCircle(cx + 8, brimY - 7, 2, col);
  } else {
    display.drawRoundRect(cx - 10, brimY - 9, 20, 9, 3, col); display.drawLine(cx - 13, brimY, cx + 13, brimY, col); display.fillRect(cx + 7, brimY - 3, 7, 3, col);
    display.drawPixel(cx, brimY - 6, col); display.drawLine(cx - 2, brimY - 6, cx + 2, brimY - 6, col); display.drawLine(cx, brimY - 8, cx, brimY - 4, col);
  }
}

void drawOutfitBackdrop(byte id, int x, int y, int bob, byte w, byte h) {
  (void)id; (void)x; (void)y; (void)bob; (void)w; (void)h;
}

void drawOutfitOnPet(int x, int y, int bob, byte w, byte h) {
  (void)h;
  const byte id = outfit % OUTFIT_COUNT;
  if (id == 0) return;
  drawFancyHat(id, x + w / 2, y + bob + 2, WHITE);
}


const __FlashStringHelper* roomName(byte id) {
  switch (id % ROOM_STYLE_COUNT) {
    case 0: return F("Plain Room");
    case 1: return F("Cozy Room");
    case 2: return F("Nature Room");
    case 3: return F("Study Room");
    case 4: return F("Arcade Room");
    case 5: return F("Space Room");
    case 6: return F("Tea Room");
    case 7: return F("Carnival Room");
    case 8: return F("Aquarium Room");
    default: return F("Memory Room");
  }
}

byte roomCost(byte id) {
  if (id == 0) return 0;
  return 2 + id;
}

byte visualHour() {
  return static_cast<byte>((currentClockEpoch() / 3600UL) % 24UL);
}

byte visualWeather() {
  return (byte)((petAgeDays() + birthCode / 17UL + generation + habitat * 2UL) % 5UL);
}

void drawWeatherWindow(int x, int y, int w, int h) {
  display.drawRect(x, y, w, h, WHITE);
  display.drawLine(x + w / 2, y, x + w / 2, y + h, WHITE);
  display.drawLine(x, y + h / 2, x + w, y + h / 2, WHITE);
  bool night = visualHour() < 7 || visualHour() >= 19;
  byte weather = visualWeather();
  if (night) {
    display.drawCircle(x + 5, y + 5, 3, WHITE);
    display.fillCircle(x + 7, y + 4, 3, BLACK);
    display.drawPixel(x + w - 5, y + 4, WHITE);
  } else {
    display.drawCircle(x + 5, y + 5, 3, WHITE);
    display.drawPixel(x + 5, y, WHITE); display.drawPixel(x + 5, y + 10, WHITE);
  }
  if (weather == 1) { // rain
    display.drawLine(x + 3, y + h - 5, x + 1, y + h - 1, WHITE);
    display.drawLine(x + 9, y + h - 5, x + 7, y + h - 1, WHITE);
    display.drawLine(x + 15, y + h - 5, x + 13, y + h - 1, WHITE);
  } else if (weather == 2) { // snow
    display.drawPixel(x + 4, y + h - 4, WHITE); display.drawPixel(x + 10, y + h - 7, WHITE); display.drawPixel(x + 17, y + h - 3, WHITE);
  } else if (weather == 3) { // wind
    display.drawLine(x + 3, y + h - 4, x + 15, y + h - 4, WHITE);
    display.drawLine(x + 8, y + h - 8, x + 19, y + h - 8, WHITE);
  } else if (weather == 4) { // sparkle
    display.drawPixel(x + 7, y + h - 5, WHITE); display.drawPixel(x + 16, y + h - 8, WHITE);
  }
}

void drawHabitatAccent() {
  if (habitat == 0) return;
  if (habitat == 1) { // forest
    display.drawTriangle(2, 58, 11, 35, 20, 58, WHITE); display.drawLine(11, 47, 11, 58, WHITE);
    display.drawTriangle(107, 58, 117, 32, 127, 58, WHITE); display.drawLine(117, 45, 117, 58, WHITE);
  } else if (habitat == 2) { // tropical
    display.drawCircle(112, 18, 6, WHITE); display.drawLine(13, 58, 13, 38, WHITE);
    display.drawLine(13, 40, 4, 33, WHITE); display.drawLine(13, 40, 23, 32, WHITE);
  } else if (habitat == 3) { // glacier
    display.drawTriangle(0, 58, 18, 39, 36, 58, WHITE); display.drawTriangle(92, 58, 110, 35, 127, 58, WHITE);
    display.drawPixel(15, 20, WHITE); display.drawPixel(104, 25, WHITE);
  } else if (habitat == 4) { // town
    display.drawRect(2, 40, 18, 18, WHITE); display.drawRect(108, 35, 18, 23, WHITE);
    display.drawPixel(8, 46, WHITE); display.drawPixel(115, 42, WHITE);
  } else if (habitat == 5) { // garden
    for (byte x=5; x<128; x+=24) { display.drawLine(x,58,x,50,WHITE); display.drawCircle(x,47,3,WHITE); }
  } else if (habitat == 6) { // stars
    display.drawCircle(112,18,7,WHITE); display.fillCircle(115,16,6,BLACK);
    for(byte i=0;i<7;i++) display.drawPixel(4+i*18,16+((i*7)%18),WHITE);
  } else { // mountain
    display.drawTriangle(0,58,22,31,44,58,WHITE); display.drawTriangle(82,58,105,27,127,58,WHITE);
  }
}

void drawRoomDecor() {
  const byte style = roomStyle % ROOM_STYLE_COUNT;
  V46UI::drawPetRoomBackdrop(display, style, visualHour(), visualWeather(), anim,
                             !lightOff && !sleeping);

  // Life-stage objects live only at the room edges. The central 44x34 area is
  // always kept quiet for the character silhouette and its hat.
  if (stage == STAGE_BABY) {
    display.drawRect(36, 48, 6, 5, WHITE);
    display.drawCircle(92, 49, 3, WHITE);
  } else if (stage == STAGE_CHILD) {
    display.drawCircle(35, 49, 4, WHITE);
    display.drawLine(31, 49, 39, 49, WHITE);
  } else if (stage == STAGE_TEEN) {
    display.drawRect(7, 28, 17, 8, WHITE);
    display.drawLine(10, 34, 21, 30, WHITE);
  } else if (stage == STAGE_ADULT) {
    display.drawRoundRect(94, 45, 13, 5, 2, WHITE);
    display.drawPixel(100, 47, WHITE);
  } else if (stage == STAGE_ELDER) {
    display.drawLine(35, 39, 35, 51, WHITE);
    display.drawCircle(32, 39, 3, WHITE);
  }

  // Purchased decorations are kept to the outer thirds of the room.
  if (roomOwned & (1UL << 1)) {
    display.drawLine(17, 48, 17, 37, WHITE);
    display.drawCircle(12, 39, 3, WHITE);
    display.drawCircle(22, 38, 3, WHITE);
  }
  if (roomOwned & (1UL << 2)) {
    display.drawLine(112, 49, 112, 38, WHITE);
    display.drawRoundRect(106, 31, 12, 7, 3, WHITE);
  }
  if (roomOwned & (1UL << 4)) {
    display.drawRect(8, 18, 17, 10, WHITE);
    display.drawCircle(16, 23, 3, WHITE);
  }

  // Visible mess remains readable but never appears behind the pet's face.
  if (clean <= 6) {
    display.drawCircle(92, 50, 2, WHITE);
    display.drawLine(28, 51, 34, 48, WHITE);
  }
  if (clean <= 3) {
    display.drawLine(7, 51, 13, 48, WHITE);
    display.drawLine(110, 51, 117, 47, WHITE);
  }

  // Clear the central safe zone last so complex themes can never swallow the
  // Tamagotchi silhouette. A restrained outline gives it visual priority.
  V46UI::clearCharacterZone(display, 42, 17, 44, 38, false);
}




void drawNeedBubble() {
  if (menuVisible || dead || sleeping || lightOff || stage == STAGE_EGG) return;
  byte need = pendingCallType;
  if (need == 0) {
    if (sick || conditionType != 0) need = 3;
    else if (poop > 0 || clean <= 2) need = 2;
    else if (full <= 2) need = 1;
    else if (happy <= 2) need = 4;
    else if (energy <= 2) need = 6;
  }
  if (need == 0) return;

  int x = 5, y = 18;
  display.fillRoundRect(x, y, 24, 19, 5, BLACK);
  display.drawRoundRect(x, y, 24, 19, 5, WHITE);
  display.drawLine(x + 18, y + 18, x + 22, y + 22, WHITE);
  if (need == 1) { // bowl
    display.drawRoundRect(x + 5, y + 9, 14, 6, 3, WHITE);
    display.drawPixel(x + 9, y + 6, WHITE); display.drawPixel(x + 13, y + 5, WHITE);
  } else if (need == 2) { // bubbles/clean
    display.drawCircle(x + 8, y + 9, 3, WHITE); display.drawCircle(x + 15, y + 6, 2, WHITE); display.drawCircle(x + 17, y + 13, 2, WHITE);
  } else if (need == 3) { // medicine cross
    display.drawLine(x + 12, y + 4, x + 12, y + 15, WHITE); display.drawLine(x + 7, y + 9, x + 17, y + 9, WHITE);
  } else if (need == 4) { // heart
    display.fillCircle(x + 9, y + 8, 3, WHITE); display.fillCircle(x + 15, y + 8, 3, WHITE); display.fillTriangle(x + 6, y + 9, x + 18, y + 9, x + 12, y + 16, WHITE);
  } else if (need == 5) { // celebration
    display.drawLine(x + 7, y + 15, x + 16, y + 5, WHITE); display.drawPixel(x + 17, y + 4, WHITE); display.drawPixel(x + 8, y + 5, WHITE);
  } else { // sleep
    display.setTextSize(1); display.setCursor(x + 7, y + 6); display.print(F("Zz"));
  }
}

void drawScheduledAwayScene() {
  const bool school = isScheduledSchoolTime();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  V46UI::drawCutawayRoom(display, school ? 2 : 4, roomStyle, visualHour(), visualWeather(), anim, true, false);

  if (school) {
    display.drawRect(6, 27, 28, 15, WHITE);
    display.drawLine(10, 32, 30, 32, WHITE);
    display.drawLine(10, 37, 25, 37, WHITE);
    display.drawRect(94, 36, 26, 12, WHITE);
    display.drawLine(98, 48, 98, 52, WHITE);
    display.drawLine(116, 48, 116, 52, WHITE);
  } else {
    display.drawRect(6, 27, 29, 18, WHITE);
    display.drawRect(10, 31, 21, 9, WHITE);
    display.drawLine(13, 45, 13, 51, WHITE);
    display.drawLine(28, 45, 28, 51, WHITE);
    display.drawRect(94, 25, 26, 20, WHITE);
    display.drawLine(98, 31, 116, 31, WHITE);
    display.drawLine(98, 37, 113, 37, WHITE);
  }

  V46UI::clearCharacterZone(display, 42, 17, 45, 36, true);
  if (stage < STAGE_ADULT) {
    drawGrowthSprite(48, 21, 2, false);
    if (stage >= STAGE_CHILD) drawOutfitOnPet(50, 25, 0, 28, 28);
  } else {
    drawSprite32(speciesType, 48, 20);
    drawRosterFace(48, 20, 1);
    drawAdultExtra(48, 20, 1);
    drawOutfitOnPet(50, 24, 0, 28, 28);
  }
  V46UI::drawHeader(display, school ? F("AT SCHOOL") : F("AT WORK"), visualHour(), clockMinute(), false, school ? 1 : 2);
  V46UI::drawFooter(display, F("D0 MENU"), F("D1 GAMES"), F("D2 MENU"));
}

void drawPet() {
  if (isScheduledAwayTime() && !dead && !petAway) {
    drawScheduledAwayScene();
    return;
  }
  if (petAway && !dead) {
    drawRoomDecor();
    display.setTextSize(1); display.setTextColor(WHITE);
    display.drawRect(40, 22, 48, 28, WHITE);
    display.drawLine(40, 22, 64, 10, WHITE); display.drawLine(88, 22, 64, 10, WHITE);
    display.setCursor(50, 31); display.print(F("AWAY"));
    display.setCursor(34, 54); display.print(F("Call to search"));
    return;
  }
  if (dead) {
    drawDeadPet();
    return;
  }

  if (sleeping || lightOff) {
    drawSleepPet();
    return;
  }

  drawRoomDecor();

  // A married city resident moves into the player's room and remains visible
  // beside the current Tamagotchi. The spouse uses the same 50-creature
  // roster art as playable adult forms.
  if (cityWorld.partnerLivesAtHome(partnerId, partnerBond)) {
    cityWorld.drawPartnerAtHome(partnerId, 98, 35, (anim / 6) & 1);
  }

  if (stage < STAGE_ADULT) {
    if (stage >= STAGE_CHILD) drawOutfitBackdrop(outfit, 50, 24, (anim & 1), 28, 28);
    NativeUI::pet(display, 48, 20, petLineage(), stage, anim, false, 2,
                  currentEvolutionPath());
    if (stage >= STAGE_CHILD) drawOutfitOnPet(50, 24, (anim & 1), 28, 28);
    if (stage != STAGE_EGG) drawPoops();
    drawNeedBubble();
    return;
  }

  // Adult and elder forms use the care-selected roster creature.
  // It no longer depends on needsCare(), so no species looks frozen.
  byte style = speciesType % 6;
  int x = 48;
  int y = 20;

  if (style == 0) {
    y += (anim & 1) ? 1 : 0;              // soft bob
  } else if (style == 1) {
    x += (anim % 8 < 4) ? -1 : 1;         // side sway
  } else if (style == 2) {
    y += (anim % 4 == 0) ? -2 : 0;        // little hop
  } else if (style == 3) {
    x += (anim % 8 < 4) ? -1 : 1;
    y += (anim & 1) ? 1 : 0;              // wobble
  } else if (style == 4) {
    y += (anim & 1) ? 1 : 0;              // blink/breathe below
  } else {
    x += (anim % 6 < 3) ? 1 : -1;
    y += (anim % 6 == 0) ? -1 : 0;        // energetic twitch
  }

  drawOutfitBackdrop(outfit, x + 2, y + 4, (anim & 1), 28, 28);
  drawSprite32(speciesType, x, y);
  drawRosterFace(x, y, 1);
  drawAdultExtra(x, y, 1);

  if (stage == STAGE_ELDER) {
    // Elder accent: cane and a small wisdom sparkle.
    display.drawLine(x + 35, y + 17, x + 35, y + 33, WHITE);
    display.drawCircle(x + 32, y + 17, 3, WHITE);
    display.drawPixel(x - 3, y + 4, WHITE);
    display.drawPixel(x - 5, y + 6, WHITE);
  }

  // Blinking frame for some pets.
  if ((anim % 18) >= 15) {
    int cx = x + 16;
    int ey = y + 12;
    display.drawLine(cx - 6, ey + 1, cx - 2, ey + 1, BLACK);
    display.drawLine(cx + 2, ey + 1, cx + 6, ey + 1, BLACK);
  }

  // Tiny personality accents so motion is visible even on chunky sprites.
  if (style == 3 && (anim & 1)) {
    display.drawPixel(x + 4, y + 6, WHITE);
    display.drawPixel(x + 28, y + 8, WHITE);
  } else if (style == 5 && (anim % 6 < 3)) {
    display.drawLine(x + 1, y + 23, x - 3, y + 21, WHITE);
    display.drawLine(x + 31, y + 23, x + 35, y + 21, WHITE);
  }

  drawOutfitOnPet(x + 2, y + 4, (anim & 1), 28, 28);

  if ((speciesType % 12) != 1 && (speciesType % 12) != 5 && (speciesType % 12) != 10) {
    drawSpriteFoot(x + 9, 56);
    drawSpriteFoot(x + 22, 56);
  }

  drawPoops();
  drawNeedBubble();

  if (!sick && !dead && !needsCare()) {
    if (happy > 8 && (anim % 10 < 5)) {
      display.fillCircle(26, 37, 1, WHITE);
      display.fillCircle(30, 37, 1, WHITE);
      display.fillTriangle(24, 38, 32, 38, 28, 44, WHITE);
    }
  }
}







void petWalkPose(byte &step, bool &facingLeft) {
  const byte phase = (byte)((anim + 11U) & 63U);
  step = 0;
  facingLeft = false;
  if (phase < 22) {
    step = phase;
  } else if (phase < 31) {
    step = 22;
  } else if (phase < 54) {
    step = 53 - phase;
    facingLeft = true;
  } else {
    facingLeft = true;
  }
}

void drawSquarePet() {
  char clock[8];snprintf(clock,sizeof(clock),"%02u:%02u",visualHour(),clockMinute());
  NativeUI::header(display,petName,clock,needsCare()||sick);
  NativeUI::room(display,0,roomStyle,visualHour(),anim);
  // Draw partner/children first; the current Tamachibi is the foreground subject.
  if(!dead)NativeUI::family(display,deepLife.childCount(),cityWorld.partnerLivesAtHome(partnerId,partnerBond),partnerId,lifeWorld.hasCompanion(),anim);
  if(dead){
    display.fillRoundRect(51,54,26,31,8,BLACK);display.drawRoundRect(51,54,26,31,8,WHITE);
    display.drawLine(64,60,64,75,WHITE);display.drawLine(58,65,70,65,WHITE);
  }else if(petAway||isScheduledAwayTime()){
    NativeUI::icon(display,4,56,63);
  }else {
    if (sleeping || lightOff) {
      const byte appearance = stage < STAGE_ADULT ? petLineage() : speciesType;
      NativeUI::pet(display, 15, 36, appearance, stage, anim, true, 1,
                    currentEvolutionPath());
    } else {
      const byte appearance = stage < STAGE_ADULT ? petLineage() : speciesType;
      byte walkStep = 0;
      bool facingLeft = false;
      petWalkPose(walkStep, facingLeft);
      const int petX = stage == STAGE_EGG ? 48 + ((anim & 1U) ? 1 : -1)
                                          : 16 + walkStep * 3;
      NativeUI::pet(display, petX, 47, appearance, stage, anim, false, 2,
                    currentEvolutionPath(), facingLeft);
      if (stage >= STAGE_CHILD)
        NativeUI::accessory(display, petX, 47, outfit, 2);
    }
    if(poop>0){display.drawCircle(85,88,2,WHITE);display.drawLine(82,90,89,90,WHITE);}
  }
  const char *notice=dead?"GOODNIGHT":petAway?"AWAY":isScheduledAwayTime()?"AT SCHOOL / WORK":
    sick?"NEEDS MEDICINE":full<=2?"HUNGRY":clean<=2||poop>0?"CLEAN UP":happy<=2?"LET'S PLAY":nullptr;
  if(notice){display.fillRoundRect(10,101,108,10,2,WHITE);NativeUI::text(display,notice,16,103,96,BLACK);}
  else {
    NativeUI::text(display,"F",4,104,6);NativeUI::meter(display,13,105,27,full);
    NativeUI::text(display,"H",47,104,6);NativeUI::meter(display,56,105,27,happy);
    NativeUI::text(display,"E",90,104,6);NativeUI::meter(display,99,105,25,energy);
  }
  NativeUI::footer(display,"MENU","OPEN","MENU");
}


void drawCareAlerts() {
  display.setTextSize(1);
  display.setTextColor(WHITE);

  byte x = 2;

  if (sleeping || lightOff) {
    display.setCursor(x, 0);
    display.print(F("Zz"));
    x += 18;
  }

  if (sick && !dead) {
    display.setCursor(x, 0);
    display.print(F("SICK"));
    x += 32;
  }

  if (poop > 0 && !dead) {
    display.setCursor(x, 0);
    display.print(F("POOP"));
    display.print(poop);
    x += 42;
  }

  if (full <= 2 && !dead) {
    display.setCursor(x, 0);
    display.print(F("FOOD"));
    x += 32;
  }

  if (clean <= 2 && !dead) {
    display.setCursor(2, 9);
    display.print(F("DIRTY"));
  }

  if (dead) {
    display.setCursor(2, 0);
    display.print(F("D1 NEW EGG"));
  }
}

// ---------- status ----------

void drawBar(int x, int y, byte val, const __FlashStringHelper *lab) {
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(x, y);
  display.print(lab);
  display.drawRect(x + 8, y + 1, 16, 5, WHITE);
  byte w = val;
  if (w > 10) w = 10;
  display.fillRect(x + 10, y + 3, w + 2, 1, WHITE);
}

void drawStats() {
  // Keep the status text fully inside the 64-pixel screen.
  // Old version used y=58, which could clip the bottom of letters.
  display.drawLine(0, 54, 127, 54, WHITE);
  drawBar(1, 55, health, F("H"));
  drawBar(26, 55, full, F("F"));
  drawBar(51, 55, energy, F("E"));
  drawBar(76, 55, happy, F("M"));
  drawBar(101, 55, clean, F("C"));
}


// Duplicate function removed by compile fix.


// ---------- action animations ----------

void animFeed() {
  for (byte i = 0; i < 10; i++) {
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(46, 0); display.print(F("MEAL"));
    display.drawLine(0, 56, 127, 56, WHITE);
    display.drawRoundRect(8, 42, 34, 10, 4, WHITE);
    display.drawLine(12, 52, 12, 58, WHITE); display.drawLine(38, 52, 38, 58, WHITE);
    display.drawRoundRect(16, 36, 18, 7, 3, WHITE);
    int px = 74 + ((i & 1) ? 1 : 0);
    if (stage < STAGE_ADULT) drawGrowthSprite(px, 24, 2, false); else { drawSprite16(speciesType, px, 30); }
    int biteX = 38 + i * 4;
    if (biteX < 73) display.fillCircle(biteX, 39 - (i % 2), 3, WHITE);
    display.setCursor(52, 15); display.print(i < 7 ? F("Yum yum!") : F("All done!"));
    display.display(); delay(75);
  }
}


void animBath() {
  for (byte i = 0; i < 10; i++) {
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(43, 0); display.print(F("BATH TIME"));
    display.drawRoundRect(25, 39, 78, 17, 7, WHITE);
    display.drawLine(30, 45, 98, 45, WHITE);
    drawCurrentPetTiny(56, 28 + ((i & 1) ? 1 : 0));
    for (byte b = 0; b < 10; b++) {
      int bx = 28 + (b * 13 + i * 5) % 72;
      int by = 27 + (b * 7 + i * 3) % 25;
      display.drawCircle(bx, by, 1 + (b & 1), WHITE);
    }
    display.drawLine(14, 48, 23, 39, WHITE); display.drawCircle(12, 50, 3, WHITE);
    display.setCursor(4, 17); display.print(F("scrub"));
    display.display(); delay(85);
  }
}


void animLove() {
  for (byte i = 0; i < 9; i++) {
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(32, 0); display.print(F("FRIEND TIME"));
    display.drawLine(0, 57, 127, 57, WHITE);
    drawCurrentPetTiny(55, 36 + ((i & 1) ? 1 : 0));
    int y = 45 - i * 3;
    display.fillCircle(33, y, 2, WHITE); display.fillCircle(37, y, 2, WHITE); display.fillTriangle(31, y + 1, 39, y + 1, 35, y + 7, WHITE);
    display.fillCircle(91, y + 5, 2, WHITE); display.fillCircle(95, y + 5, 2, WHITE); display.fillTriangle(89, y + 6, 97, y + 6, 93, y + 12, WHITE);
    if (ownerBond >= 60) { display.drawPixel(16, 21, WHITE); display.drawPixel(111, 20, WHITE); }
    display.display(); delay(95);
  }
}

void careChoiceAnimation(byte mode) {
  if (mode == 0) {
    animLove();
    return;
  }

  for (byte frame = 0; frame < 8; ++frame) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(mode == 1 ? 43 : 39, 8);
    display.print(mode == 1 ? F("KIND WORDS") : F("QUIET TIME"));
    display.drawLine(10, 22, 118, 22, WHITE);
    drawCurrentPetTiny(56, 62 + ((frame & 1) ? 1 : 0));
    if (mode == 1) {
      int rise = (frame & 1) ? 3 : 0;
      display.drawLine(64, 41 - rise, 64, 52 - rise, WHITE);
      display.drawLine(59, 46 - rise, 69, 46 - rise, WHITE);
      display.drawLine(60, 42 - rise, 68, 50 - rise, WHITE);
      display.drawLine(68, 42 - rise, 60, 50 - rise, WHITE);
      display.drawPixel(34, 59, WHITE);
      display.drawPixel(94, 57 + (frame & 1), WHITE);
    } else {
      display.fillCircle(64, 41, 8, WHITE);
      display.fillCircle(69, 38, 8, BLACK);
      display.drawPixel(42, 50, WHITE);
      display.drawPixel(86, 48 + (frame & 1), WHITE);
      display.drawPixel(98, 61, WHITE);
    }
    display.setCursor(32, 104);
    display.print(mode == 1 ? F("I see you trying") : F("No words needed"));
    display.display();
    delay(100);
  }
}


void animMed() {
  for (byte i = 0; i < 9; i++) {
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(35, 0); display.print(F("CARE CLINIC"));
    display.drawRoundRect(35, 42, 60, 13, 5, WHITE);
    display.drawLine(39, 49, 91, 49, WHITE);
    drawCurrentPetSleepTiny(53, 26);
    int x = 9 + i * 6;
    display.drawRect(x, 27, 10, 15, WHITE);
    display.drawLine(x + 5, 30, x + 5, 39, WHITE); display.drawLine(x + 2, 34, x + 8, 34, WHITE);
    display.setCursor(91, 22); display.print(F("+"));
    display.display(); delay(80);
  }
}


void animLight() {
  // v26: never invert the entire SSD1306. Some low-cost modules can remain
  // white if power or I2C is interrupted between invert-on and invert-off.
  for (byte i = 0; i < 4; i++) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.drawRoundRect(30, 10, 68, 44, 7, WHITE);

    if (lightOff || sleeping) {
      // Moon and stars for lights-off.
      display.fillCircle(61, 31, 11, WHITE);
      display.fillCircle(67, 27, 10, BLACK);
      display.drawPixel(44, 23, WHITE);
      display.drawPixel(82, 20, WHITE);
      display.drawPixel(87, 37, WHITE);
      if (i & 1) display.drawPixel(39, 39, WHITE);
    } else {
      // Bulb and rays for lights-on.
      display.drawCircle(64, 28, 10, WHITE);
      display.drawLine(59, 39, 69, 39, WHITE);
      display.drawLine(60, 42, 68, 42, WHITE);
      display.drawLine(64, 12, 64, 16, WHITE);
      display.drawLine(45, 28, 50, 28, WHITE);
      display.drawLine(78, 28, 83, 28, WHITE);
      display.drawLine(50, 15, 54, 19, WHITE);
      display.drawLine(74, 19, 78, 15, WHITE);
      if (i & 1) display.fillCircle(64, 28, 4, WHITE);
    }

    display.display();
    delay(90);
  }
}

void animCallBubble(const __FlashStringHelper *msg) {
  drawScreen(false);
  display.fillRoundRect(8, 19, 112, 30, 5, BLACK);
  display.drawRoundRect(8, 19, 112, 30, 5, WHITE);
  display.drawLine(80, 49, 70, 56, WHITE);
  display.setCursor(15, 27);
  display.print(msg);
  display.setCursor(15, 39);
  display.print(F("D1 close"));
  display.display();
  waitButtonRelease();
  while (digitalRead(BTN_S) == HIGH) { lastInputAt = millis(); delay(10); }
  clickSound(); waitButtonRelease();
}

// ---------- food menu ----------

void drawFoodChoice(byte foodId, int x, int y, bool sel) {
  if (sel) display.fillRoundRect(x, y, 23, 24, 4, WHITE);
  else display.drawRoundRect(x, y, 23, 24, 4, WHITE);

  uint16_t col = sel ? BLACK : WHITE;

  if (foodId == FOOD_BERRY) {
    display.fillCircle(x + 11, y + 12, 5, col);
    display.drawLine(x + 11, y + 7, x + 14, y + 4, col);
  } else if (foodId == FOOD_RICE) {
    display.drawRoundRect(x + 5, y + 11, 13, 7, 3, col);
    display.drawPixel(x + 8, y + 9, col);
    display.drawPixel(x + 11, y + 8, col);
    display.drawPixel(x + 14, y + 9, col);
  } else if (foodId == FOOD_VEG) {
    display.drawLine(x + 11, y + 6, x + 11, y + 18, col);
    display.drawCircle(x + 8, y + 9, 3, col);
    display.drawCircle(x + 14, y + 12, 3, col);
    display.drawCircle(x + 8, y + 15, 3, col);
  } else if (foodId == FOOD_SNACK) {
    display.drawRect(x + 6, y + 8, 12, 10, col);
    display.drawLine(x + 6, y + 8, x + 18, y + 18, col);
    display.drawLine(x + 18, y + 8, x + 6, y + 18, col);
  } else {
    display.fillCircle(x + 8, y + 10, 3, col);
    display.fillCircle(x + 14, y + 10, 3, col);
    display.fillTriangle(x + 5, y + 11, x + 17, y + 11, x + 11, y + 19, col);
  }
}


// Older duplicate foodMenu removed in v13.1 compile fix.



// ---------- mini games ----------

void drawStar(int x, int y) {
  display.drawPixel(x, y - 3, WHITE);
  display.drawPixel(x, y + 3, WHITE);
  display.drawPixel(x - 3, y, WHITE);
  display.drawPixel(x + 3, y, WHITE);
  display.drawLine(x - 2, y - 2, x + 2, y + 2, WHITE);
  display.drawLine(x + 2, y - 2, x - 2, y + 2, WHITE);
}

void drawMiniPet() {
  display.fillRoundRect(0, 38, 22, 20, 3, BLACK);
  display.drawRoundRect(0, 38, 22, 20, 3, WHITE);
  drawCurrentPetTiny(3, 40);
}


void resolveArcadeSession(byte score, byte goal) {
  if (activeGameId >= 13) return;
  activeGameResolved = true;
  activeGameWon = score >= goal;
  activeGameScore = score;
}

void showGameResult(byte score, byte goal) {
  if (!activeGameResolved) resolveArcadeSession(score, goal);
  byte shown = score;
  if (shown > goal) shown = goal;

  if (score >= goal) winSound();
  else badSound();

  display.clearDisplay();
  display.drawRoundRect(14, 18, 100, 32, 5, WHITE);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(25, 25);
  display.print(F("Goal: "));
  display.print(shown);
  display.print(F("/"));
  display.print(goal);
  display.setCursor(25, 36);
  display.print(F("Score: "));
  display.print(score);
  display.setCursor(25, 46);
  if (score >= goal) display.print(F("Great job!"));
  else display.print(F("Nice try!"));
  display.display();
  delay(1000);
}



void rewardGameScore(byte score, byte goal, bool smartGame, bool fitGame) {
  const byte baseAssist = (activeGameId < 13) ? lifeWorld.gameAssist(activeGameId) : 0;
  const byte deepAssist = (activeGameId < 13) ? deepLife.extraGameAssist(activeGameId) : 0;
  const byte talentAssist = min((byte)5, (byte)(baseAssist + deepAssist));
  const byte adjustedGoal = goal > talentAssist ? goal - talentAssist : 1;
  if (score >= adjustedGoal) {
    addStat(happy, 4);
    addStat(discipline, 1);
    if (smartGame) addSkill(skillSmart, 1);
    if (fitGame) addSkill(skillFit, 1);
    winSound();
  } else {
    addStat(happy, 1 + min(score, (byte)2));
    if (random(0, 3) == 0) subStat(discipline, 1);
  }

  // Fun games usually do not drain energy.
  if (random(0, 4) == 0) subStat(energy, 1);

  resolveArcadeSession(score, adjustedGoal);
  clampAll();
}


bool gameBackCheck() {
  static unsigned long gameExitStarted = 0;
  if (digitalRead(BTN_R) == LOW) {
    lastInputAt = millis();
    if (gameExitStarted == 0) gameExitStarted = millis();
    if (millis() - gameExitStarted >= GAME_EXIT_HOLD_MS) {
      gameExitStarted = 0;
      clickSound();
      waitButtonRelease();
      drawScreen(true);
      return true;
    }
  } else {
    gameExitStarted = 0;
  }
  return false;
}



void drawRightText(const char *txt, byte y) {
  byte len = strlen(txt);
  int x = 127 - (len * 6) + 1;
  if (x < 0) x = 0;
  display.setCursor(x, y);
  display.print(txt);
}

void drawRightScore(unsigned int a, unsigned int b, byte y=0) {
  char buf[12];
  snprintf(buf, sizeof(buf), "%u/%u", a, b);
  drawRightText(buf, y);
}

void drawRightNumber(unsigned int a, byte y=0) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%u", a);
  drawRightText(buf, y);
}


void starCatchGame() {
  NativeUI::GameScope gameCanvas(display,"LEFT","EXIT","RIGHT");
  waitButtonRelease();

  int basketX = 54;
  int starX = 64;
  int starY = 18;
  byte score = 0;
  byte misses = 0;
  unsigned long start = millis();
  unsigned long lastFrame = 0;

  while ((millis() - start) < 14000UL && misses < 4) {
    if (gameBackCheck()) return;
    unsigned long now = millis();
    if (now - lastFrame < 70) {
      delay(2);
      continue;
    }
    lastFrame = now;

    if (pressed(BTN_L) && basketX > 3) basketX -= 5;
    if (pressed(BTN_R) && basketX < 103) basketX += 5;
    if (pressed(BTN_S)) break;

    // Slower fall.
    starY += 1;

    if (starY >= 54) {
      if (starX >= basketX - 2 && starX <= basketX + 25) { score++; goodSound(); }
      else { misses++; badSound(); }

      starX = 8 + ((starX * 37 + score * 19 + misses * 11 + now) % 112);
      starY = 18;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print(F("STAR CATCH"));
    drawRightScore(score, misses);
    display.drawLine(0, 12, 127, 12, WHITE);

    drawStar(starX, starY);

    display.drawRoundRect(basketX, 55, 23, 7, 2, WHITE);
    display.drawLine(basketX + 2, 55, basketX + 7, 50, WHITE);
    display.drawLine(basketX + 21, 55, basketX + 16, 50, WHITE);
    drawMiniPet();

    display.display();
  }

  addStat(happy, score > 5 ? 5 : score);
  if (random(0, 3) == 0) subStat(energy, 1);
  if (score >= 5) addStat(discipline, 1);
  else subStat(discipline, 1);
  clampAll();
  showGameResult(score, 5);
  waitButtonRelease();
}

void drawRPSChoice(byte choice, int x, int y, bool inv) {
  uint16_t col = inv ? BLACK : WHITE;

  if (inv) display.fillRoundRect(x - 2, y - 2, 32, 28, 4, WHITE);
  else display.drawRoundRect(x - 2, y - 2, 32, 28, 4, WHITE);

  if (choice == 0) {
    display.drawCircle(x + 14, y + 13, 8, col);
    display.drawPixel(x + 10, y + 9, col);
    display.drawPixel(x + 16, y + 18, col);
  } else if (choice == 1) {
    display.drawRect(x + 8, y + 5, 13, 17, col);
    display.drawLine(x + 11, y + 9, x + 18, y + 9, col);
    display.drawLine(x + 11, y + 13, x + 18, y + 13, col);
    display.drawLine(x + 11, y + 17, x + 18, y + 17, col);
  } else {
    display.drawCircle(x + 10, y + 18, 3, col);
    display.drawCircle(x + 18, y + 18, 3, col);
    display.drawLine(x + 13, y + 15, x + 22, y + 5, col);
    display.drawLine(x + 15, y + 15, x + 6, y + 5, col);
  }
}

int rpsWinner(byte player, byte cpu) {
  if (player == cpu) return 0;
  if ((player == 0 && cpu == 2) || (player == 1 && cpu == 0) || (player == 2 && cpu == 1)) return 1;
  return -1;
}

void rpsGame() {
  NativeUI::GameScope gameCanvas(display,"ROCK","PAPER","CUT");
  waitButtonRelease();

  byte pick = 0;
  byte wins = 0;
  byte rounds = 0;

  while (rounds < 3) {
    bool picked = false;
    bool oldLeft = HIGH;
    bool oldSel = HIGH;
    bool oldRight = HIGH;

    while (!picked) {
      if (gameBackCheck()) return;
      bool l = digitalRead(BTN_L);
      bool s = digitalRead(BTN_S);
      bool r = digitalRead(BTN_R);

      if (oldLeft == HIGH && l == LOW) {
        pick = (pick == 0) ? 2 : pick - 1;
        delay(120);
      }
      if (oldRight == HIGH && r == LOW) {
        pick++;
        if (pick > 2) pick = 0;
        delay(120);
      }
      if (oldSel == HIGH && s == LOW) {
        picked = true;
        waitButtonRelease();
      }

      oldLeft = l;
      oldSel = s;
      oldRight = r;

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(5, 0);
      display.print(F("Rock Paper Scissors"));
      display.setCursor(5, 10);
      display.print(F("Round "));
      display.print(rounds + 1);
      display.print(F("  Win "));
      display.print(wins);

      drawRPSChoice(0, 5, 28, pick == 0);
      drawRPSChoice(1, 48, 28, pick == 1);
      drawRPSChoice(2, 91, 28, pick == 2);

      display.display();
      delay(20);
    }

    byte cpu = random(0, 3);
    int res = rpsWinner(pick, cpu);
    if (res == 1) { wins++; goodSound(); }
    else if (res == -1) badSound();
    else clickSound();

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(16, 0);
    display.print(F("You"));
    display.setCursor(86, 0);
    display.print(F("Pet"));
    drawRPSChoice(pick, 7, 20, false);
    drawRPSChoice(cpu, 82, 20, false);
    drawCurrentPetGameBadge(103, 36);
    display.setCursor(42, 54);
    if (res == 1) display.print(F("You win!"));
    else if (res == -1) display.print(F("Pet wins"));
    else display.print(F("Tie"));
    display.display();
    delay(1050);

    rounds++;
  }

  addStat(happy, 2 + wins);
  if (random(0, 4) == 0) subStat(energy, 1);
  if (wins >= 2) addStat(discipline, 1);
  clampAll();
  showGameResult(wins, 2);
  waitButtonRelease();
}

void numberGame() {
  NativeUI::GameScope gameCanvas(display,"LESS","GUESS","MORE");
  waitButtonRelease();

  byte target = random(0, 10);
  byte guess = 5;
  byte tries = 0;
  bool won = false;
  byte hint = 0;

  while (tries < 4 && !won) {
    bool oldLeft = HIGH;
    bool oldSel = HIGH;
    bool oldRight = HIGH;
    bool selected = false;

    while (!selected) {
      if (gameBackCheck()) return;
      bool l = digitalRead(BTN_L);
      bool s = digitalRead(BTN_S);
      bool r = digitalRead(BTN_R);

      if (oldLeft == HIGH && l == LOW) {
        guess = (guess == 0) ? 9 : guess - 1;
        delay(115);
      }
      if (oldRight == HIGH && r == LOW) {
        guess++;
        if (guess > 9) guess = 0;
        delay(115);
      }
      if (oldSel == HIGH && s == LOW) {
        selected = true;
        waitButtonRelease();
      }

      oldLeft = l;
      oldSel = s;
      oldRight = r;

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(14, 0);
      display.print(F("Guess my number"));
      display.setCursor(18, 11);
      display.print(F("0 to 9  Try "));
      display.print(tries + 1);
      display.print(F("/4"));

      display.drawRoundRect(44, 27, 40, 25, 5, WHITE);
      display.setTextSize(2);
      display.setCursor(58, 32);
      display.print(guess);

      display.setTextSize(1);
      display.setCursor(5, 55);
      if (hint == 1) display.print(F("Hint: higher"));
      else if (hint == 2) display.print(F("Hint: lower"));
      else display.print(F("D0/D2 move D1 pick"));

      display.display();
      delay(20);
    }

    tries++;

    if (guess == target) { won = true; goodSound(); }
    else if (guess < target) {
      badSound();
      hint = 1;
      popup(F("Nope!"), F("Go higher"), 500);
    } else {
      badSound();
      hint = 2;
      popup(F("Nope!"), F("Go lower"), 500);
    }
  }

  resolveArcadeSession(won ? 1 : 0, 1);
  display.clearDisplay();
  display.drawRoundRect(13, 17, 102, 31, 5, WHITE);
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(24, 25);
  if (won) display.print(F("Correct!"));
  else display.print(F("The number was"));
  display.setCursor(57, 36);
  display.print(target);
  display.display();
  delay(1000);

  if (won) {
    addStat(happy, 4);
    addStat(discipline, 1);
  } else {
    addStat(happy, 1);
    subStat(discipline, 1);
  }
  if (random(0, 4) == 0) subStat(energy, 1);
  clampAll();
  waitButtonRelease();
}



const byte TB_W = 8;
const byte TB_H = 12;

// 4x4 falling block masks. 7 shapes, 4 rotations.
const uint16_t TB_SHAPES[7][4] = {
  {0x0F00, 0x4444, 0x0F00, 0x4444}, // I
  {0x6600, 0x6600, 0x6600, 0x6600}, // O
  {0x4E00, 0x4640, 0x0E40, 0x4C40}, // T
  {0x6C00, 0x4620, 0x6C00, 0x4620}, // S
  {0xC600, 0x2640, 0xC600, 0x2640}, // Z
  {0x8E00, 0x6440, 0xE200, 0x44C0}, // J
  {0x2E00, 0x4460, 0xE800, 0xC440}  // L
};

bool tbCell(byte shape, byte rot, byte cx, byte cy) {
  uint16_t mask = TB_SHAPES[shape % 7][rot & 3];
  return (mask & (0x8000 >> (cy * 4 + cx))) != 0;
}

bool tbCollision(bool board[12][8], int px, int py, byte shape, byte rot) {
  for (byte cy = 0; cy < 4; cy++) {
    for (byte cx = 0; cx < 4; cx++) {
      if (!tbCell(shape, rot, cx, cy)) continue;

      int bx = px + cx;
      int by = py + cy;

      if (bx < 0 || bx >= TB_W || by >= TB_H) return true;
      if (by >= 0 && board[by][bx]) return true;
    }
  }

  return false;
}

void tbPlace(bool board[12][8], int px, int py, byte shape, byte rot) {
  for (byte cy = 0; cy < 4; cy++) {
    for (byte cx = 0; cx < 4; cx++) {
      if (!tbCell(shape, rot, cx, cy)) continue;

      int bx = px + cx;
      int by = py + cy;

      if (bx >= 0 && bx < TB_W && by >= 0 && by < TB_H) {
        board[by][bx] = true;
      }
    }
  }
}

byte tbClearLines(bool board[12][8]) {
  byte cleared = 0;

  for (int y = TB_H - 1; y >= 0; y--) {
    bool fullRow = true;
    for (byte x = 0; x < TB_W; x++) {
      if (!board[y][x]) {
        fullRow = false;
        break;
      }
    }

    if (fullRow) {
      cleared++;

      for (int yy = y; yy > 0; yy--) {
        for (byte x = 0; x < TB_W; x++) {
          board[yy][x] = board[yy - 1][x];
        }
      }

      for (byte x = 0; x < TB_W; x++) board[0][x] = false;
      y++; // check same row again after pull-down
    }
  }

  return cleared;
}

void tbDrawBlockCell(int x, int y, bool active) {
  if (active) {
    display.fillRect(x, y, 5, 4, WHITE);
  } else {
    display.drawRect(x, y, 5, 4, WHITE);
  }
}

void tbDrawBoard(bool board[12][8], int px, int py, byte shape, byte rot, byte nextShape, unsigned int score, byte lines) {
  const byte ox = 40;
  const byte oy = 13;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(20, 0);
  display.print(F("Tiny Blocks"));
  display.setCursor(97, 0);
  display.print(lines);

  display.drawRect(ox - 2, oy - 2, TB_W * 6 + 3, TB_H * 4 + 3, WHITE);

  for (byte y = 0; y < TB_H; y++) {
    for (byte x = 0; x < TB_W; x++) {
      if (board[y][x]) tbDrawBlockCell(ox + x * 6, oy + y * 4, true);
    }
  }

  for (byte cy = 0; cy < 4; cy++) {
    for (byte cx = 0; cx < 4; cx++) {
      if (!tbCell(shape, rot, cx, cy)) continue;

      int bx = px + cx;
      int by = py + cy;

      if (by >= 0 && bx >= 0 && bx < TB_W && by < TB_H) {
        tbDrawBlockCell(ox + bx * 6, oy + by * 4, true);
      }
    }
  }

  display.setCursor(0, 14);
  display.print(F("Next"));
  for (byte cy = 0; cy < 4; cy++) {
    for (byte cx = 0; cx < 4; cx++) {
      if (tbCell(nextShape, 0, cx, cy)) {
        display.fillRect(6 + cx * 5, 25 + cy * 4, 4, 3, WHITE);
      }
    }
  }

  display.setCursor(0, 44);
  display.print(F("Score"));
  display.setCursor(0, 53);
  display.print(score);

  display.setCursor(72, 55);
  display.print(F("D1 rot"));
  display.display();
}


void tinyBlocksGame() {
  NativeUI::GameScope gameCanvas(display,"LEFT","TURN","RIGHT");
  waitButtonRelease();

  bool board[12][8];
  for (byte y = 0; y < TB_H; y++) {
    for (byte x = 0; x < TB_W; x++) {
      board[y][x] = false;
    }
  }

  byte shape = random(0, 7);
  byte nextShape = random(0, 7);
  byte rot = 0;
  int px = 2;
  int py = -1;

  unsigned int score = 0;
  byte lines = 0;
  byte pieces = 0;
  bool gameOver = false;

  unsigned long lastDrop = millis();
  bool oldLeft = HIGH;
  bool oldSel = HIGH;
  bool oldRight = HIGH;
  unsigned long selectDownAt = 0;
  bool selectHoldUsed = false;

  while (!gameOver && lines < 20) {
    if (gameBackCheck()) return;

    bool l = digitalRead(BTN_L);
    bool s = digitalRead(BTN_S);
    bool r = digitalRead(BTN_R);
    unsigned long now = millis();

    if (l == LOW || s == LOW || r == LOW) lastInputAt = now;

    // D0 move left
    if (oldLeft == HIGH && l == LOW) {
      if (!tbCollision(board, px - 1, py, shape, rot)) {
        px--;
        clickSound();
      }
    }

    // D2 move right, long hold is still global back through gameBackCheck().
    if (oldRight == HIGH && r == LOW) {
      if (!tbCollision(board, px + 1, py, shape, rot)) {
        px++;
        clickSound();
      }
    }

    // D1 short press rotates. D1 hold soft-drops.
    if (oldSel == HIGH && s == LOW) {
      selectDownAt = now;
      selectHoldUsed = false;
    }

    if (s == LOW && !selectHoldUsed && selectDownAt > 0 && now - selectDownAt > 300UL) {
      if (!tbCollision(board, px, py + 1, shape, rot)) {
        py++;
        score++;
      }
      selectHoldUsed = true;
    }

    if (oldSel == LOW && s == HIGH) {
      if (!selectHoldUsed) {
        byte newRot = (rot + 1) & 3;

        if (!tbCollision(board, px, py, shape, newRot)) {
          rot = newRot;
          clickSound();
        } else if (!tbCollision(board, px - 1, py, shape, newRot)) {
          px--;
          rot = newRot;
          clickSound();
        } else if (!tbCollision(board, px + 1, py, shape, newRot)) {
          px++;
          rot = newRot;
          clickSound();
        } else {
          badSound();
        }
      }

      selectDownAt = 0;
      selectHoldUsed = false;
    }

    oldLeft = l;
    oldSel = s;
    oldRight = r;

    unsigned int fallGap = 720;
    if (lines >= 4) fallGap = 610;
    if (lines >= 8) fallGap = 500;
    if (lines >= 12) fallGap = 410;
    if (lines >= 16) fallGap = 330;

    if (now - lastDrop >= fallGap) {
      lastDrop = now;

      if (!tbCollision(board, px, py + 1, shape, rot)) {
        py++;
      } else {
        tbPlace(board, px, py, shape, rot);

        byte cleared = tbClearLines(board);
        if (cleared > 0) {
          lines += cleared;
          score += 10 * cleared * cleared;
          winSound();
        } else {
          score += 2;
          clickSound();
        }

        pieces++;
        shape = nextShape;
        nextShape = random(0, 7);
        rot = 0;
        px = 2;
        py = -1;

        if (tbCollision(board, px, py, shape, rot)) {
          gameOver = true;
          badSound();
        }
      }
    }

    tbDrawBoard(board, px, py, shape, rot, nextShape, score, lines);
    delay(18);
  }

  if (lines >= 20) {
    winSound();
    popup(F("Block master!"), F("Board cleared"), 800);
  } else {
    popup(F("Stacked out"), F("Game over"), 800);
  }

  addStat(happy, lines >= 4 ? 5 : (1 + lines));
  if (lines >= 4) addStat(discipline, 1);
  if (lines >= 8) addSkill(skillSmart, 1);
  if (random(0, 3) == 0) subStat(energy, 1);
  clampAll();
  showGameResult(lines, 4);
  waitButtonRelease();
}



byte mineCountAround(byte mines[5], byte cx, byte cy) {
  byte count = 0;
  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      if (dx == 0 && dy == 0) continue;
      int nx = cx + dx;
      int ny = cy + dy;
      if (nx >= 0 && nx < 5 && ny >= 0 && ny < 5) {
        if (mines[ny] & (1 << nx)) count++;
      }
    }
  }
  return count;
}

bool mineProtected(byte sx, byte sy, byte x, byte y) {
  int dx = (int)x - (int)sx;
  int dy = (int)y - (int)sy;
  return dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1;
}

void revealMineOpening(byte mines[5], byte revealed[5], byte sx, byte sy) {
  revealed[sy] |= (1 << sx);

  // Give a fun starting opening when the start square is empty.
  if (mineCountAround(mines, sx, sy) == 0) {
    for (int dy = -1; dy <= 1; dy++) {
      for (int dx = -1; dx <= 1; dx++) {
        int nx = sx + dx;
        int ny = sy + dy;
        if (nx >= 0 && nx < 5 && ny >= 0 && ny < 5) {
          revealed[ny] |= (1 << nx);
        }
      }
    }
  }
}

void minesweeperGame() {
  NativeUI::GameScope gameCanvas(display,"PREV","OPEN","NEXT");
  waitButtonRelease();

  byte mines[5] = {0, 0, 0, 0, 0};
  byte revealed[5] = {0, 0, 0, 0, 0};
  byte flags[5] = {0, 0, 0, 0, 0};

  // Start with a random safe opening, protected with neighbors.
  byte startX = random(0, 5);
  byte startY = random(0, 5);
  byte cx = startX;
  byte cy = startY;

  byte placed = 0;
  while (placed < 5) {
    byte x = random(0, 5);
    byte y = random(0, 5);

    if (mineProtected(startX, startY, x, y)) continue;

    if (!(mines[y] & (1 << x))) {
      mines[y] |= (1 << x);
      placed++;
    }
  }

  revealMineOpening(mines, revealed, startX, startY);

  bool exploded = false;
  bool won = false;
  bool oldLeft = HIGH, oldSel = HIGH, oldRight = HIGH;
  unsigned long leftDown = 0, rightDown = 0, selectDown = 0;
  bool leftHoldUsed = false, rightHoldUsedLocal = false, selectHoldUsed = false;

  while (!exploded && !won) {
    bool l = digitalRead(BTN_L);
    bool s = digitalRead(BTN_S);
    bool r = digitalRead(BTN_R);
    unsigned long now = millis();

    if (l == LOW || s == LOW || r == LOW) lastInputAt = now;

    if (oldLeft == HIGH && l == LOW) { leftDown = now; leftHoldUsed = false; }
    if (oldRight == HIGH && r == LOW) { rightDown = now; rightHoldUsedLocal = false; }
    if (oldSel == HIGH && s == LOW) { selectDown = now; selectHoldUsed = false; }

    // D2 long is down. Holding D2 about 2 seconds longer exits.
    if (r == LOW && rightDown > 0 && now - rightDown > LONG_RIGHT_GAME_EXIT_MS) {
      waitButtonRelease();
      drawScreen(true);
      return;
    }

    // Minesweeper uses longer D0/D2 holds for up/down movement instead of back.
    if (l == LOW && !leftHoldUsed && leftDown > 0 && now - leftDown > LONG_RIGHT_GAME_ACTION_MS) {
      if (cy > 0) cy--;
      leftHoldUsed = true;
      clickSound();
    }

    if (r == LOW && !rightHoldUsedLocal && rightDown > 0 && now - rightDown > LONG_RIGHT_GAME_ACTION_MS) {
      if (cy < 4) cy++;
      rightHoldUsedLocal = true;
      clickSound();
    }

    if (s == LOW && !selectHoldUsed && selectDown > 0 && now - selectDown > 750UL) {
      flags[cy] ^= (1 << cx);
      selectHoldUsed = true;
      clickSound();
    }

    if (oldLeft == LOW && l == HIGH && !leftHoldUsed) {
      if (cx > 0) cx--;
      clickSound();
    }

    if (oldRight == LOW && r == HIGH && !rightHoldUsedLocal) {
      if (cx < 4) cx++;
      clickSound();
    }

    if (oldSel == LOW && s == HIGH && !selectHoldUsed) {
      if (!(flags[cy] & (1 << cx))) {
        revealed[cy] |= (1 << cx);

        if (mines[cy] & (1 << cx)) {
          exploded = true;
          badSound();
        } else {
          if (mineCountAround(mines, cx, cy) == 0) revealMineOpening(mines, revealed, cx, cy);
          goodSound();
        }
      }
    }

    oldLeft = l;
    oldSel = s;
    oldRight = r;

    byte safeRevealed = 0;
    for (byte y = 0; y < 5; y++) {
      for (byte x = 0; x < 5; x++) {
        if ((revealed[y] & (1 << x)) && !(mines[y] & (1 << x))) safeRevealed++;
      }
    }

    if (safeRevealed >= 20) won = true;

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(20, 0);
    display.print(F("Mini Mines"));
    drawRightScore(safeRevealed, 20);

    for (byte y = 0; y < 5; y++) {
      for (byte x = 0; x < 5; x++) {
        int sx = 34 + x * 12;
        int sy = 13 + y * 9;
        bool cur = (x == cx && y == cy);

        if (cur) display.fillRect(sx - 1, sy - 1, 11, 8, WHITE);
        else display.drawRect(sx - 1, sy - 1, 11, 8, WHITE);

        uint16_t col = cur ? BLACK : WHITE;
        display.setTextColor(col);
        display.setCursor(sx + 2, sy);

        if (flags[y] & (1 << x)) display.print(F("F"));
        else if (revealed[y] & (1 << x)) {
          if (mines[y] & (1 << x)) display.print(F("*"));
          else {
            byte n = mineCountAround(mines, x, y);
            if (n == 0) display.print(F("."));
            else display.print(n);
          }
        }

        display.setTextColor(WHITE);
      }
    }

    display.setCursor(0, 56);
    display.print(F("D2 hold down/exit"));
    display.display();
    delay(25);
  }

  if (won) {
    winSound();
    addStat(happy, 5);
    addSkill(skillSmart, 2);
    addStat(discipline, 1);
    showGameResult(10, 5);
  } else {
    badSound();
    addStat(happy, 1);
    showGameResult(1, 5);
  }

  if (random(0, 3) == 0) subStat(energy, 1);
  clampAll();
  waitButtonRelease();
}


void lookGuessGame() {
  NativeUI::GameScope gameCanvas(display,"LEFT","GUESS","RIGHT");
  waitButtonRelease();

  byte score = 0;
  for (byte roundNo = 0; roundNo < 5; roundNo++) {
    byte pick = 0;
    bool done = false;
    bool oldLeft = HIGH, oldSel = HIGH, oldRight = HIGH;

    while (!done) {
      if (gameBackCheck()) return;

      bool l = digitalRead(BTN_L);
      bool s = digitalRead(BTN_S);
      bool r = digitalRead(BTN_R);

      if (oldLeft == HIGH && l == LOW) { pick = 0; clickSound(); delay(100); }
      if (oldRight == HIGH && r == LOW) { pick = 1; clickSound(); delay(100); }
      if (oldSel == HIGH && s == LOW) { done = true; waitButtonRelease(); }

      oldLeft = l; oldSel = s; oldRight = r;

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(20, 0);
      display.print(F("Look Guess"));
      display.setCursor(10, 12);
      display.print(F("Which way next?"));
      drawCurrentPetGameBadge(53, 27);
      display.setCursor(0, 55);
      if (pick == 0) display.print(F("< LEFT   D1 pick"));
      else display.print(F("RIGHT >  D1 pick"));
      display.display();
      delay(20);
    }

    byte answer = random(0, 2);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(35, 0);
    display.print(F("Pet looks"));
    drawCurrentPetGameBadge(53, 25);
    if (answer == 0) display.fillTriangle(43, 35, 50, 30, 50, 40, WHITE);
    else display.fillTriangle(83, 35, 76, 30, 76, 40, WHITE);
    display.setCursor(40, 55);
    if (pick == answer) { display.print(F("Correct!")); score++; goodSound(); }
    else { display.print(F("Wrong")); badSound(); }
    display.display();
    delay(850);
  }

  rewardGameScore(score, 3, false, false);
  showGameResult(score, 3);
  waitButtonRelease();
}

void memoryGame() {
  NativeUI::GameScope gameCanvas(display,"A","B","C");
  waitButtonRelease();

  byte score = 0;
  byte seq[7];

  for (byte roundNo = 0; roundNo < 5; roundNo++) {
    byte len = 3 + roundNo;
    for (byte i = 0; i < len; i++) seq[i] = random(0, 3);

    for (byte i = 0; i < len; i++) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(31, 0);
      display.print(F("Memory"));
      display.setCursor(20, 11);
      display.print(F("Watch pattern"));
      display.drawRoundRect(45, 28, 38, 19, 4, WHITE);
      display.setCursor(58, 34);
      if (seq[i] == 0) display.print(F("<"));
      else if (seq[i] == 1) display.print(F("O"));
      else display.print(F(">"));
      display.display();
      delay(420);
      display.clearDisplay();
      display.display();
      delay(130);
    }

    bool ok = true;
    for (byte i = 0; i < len; i++) {
      bool picked = false;
      byte p = 255;
      bool oldLeft = HIGH, oldSel = HIGH, oldRight = HIGH;

      while (!picked) {
        if (gameBackCheck()) return;

        bool l = digitalRead(BTN_L);
        bool s = digitalRead(BTN_S);
        bool r = digitalRead(BTN_R);

        if (oldLeft == HIGH && l == LOW) { p = 0; picked = true; waitButtonRelease(); }
        if (oldSel == HIGH && s == LOW) { p = 1; picked = true; waitButtonRelease(); }
        if (oldRight == HIGH && r == LOW) { p = 2; picked = true; waitButtonRelease(); }

        oldLeft = l; oldSel = s; oldRight = r;

        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(WHITE);
        display.setCursor(31, 0);
        display.print(F("Repeat"));
        display.setCursor(24, 18);
        display.print(F("Step "));
        display.print(i + 1);
        display.print(F("/"));
        display.print(len);
        display.setCursor(8, 55);
        display.print(F("D0 <  D1 O  D2 >"));
        display.display();
        delay(20);
      }

      if (p != seq[i]) ok = false;
    }

    if (ok) {
      score++;
      goodSound();
      popup(F("Good memory"), F("Next round"), 450);
    } else {
      badSound();
      popup(F("Missed it"), F("Try again"), 450);
      break;
    }
  }

  rewardGameScore(score, 3, true, false);
  showGameResult(score, 3);
  waitButtonRelease();
}

void sprintGame() {
  NativeUI::GameScope gameCanvas(display,"LEFT","GO","RIGHT");
  waitButtonRelease();

  byte taps = 0;
  unsigned long start = millis();
  bool oldLeft = HIGH, oldSel = HIGH, oldRight = HIGH;

  while (millis() - start < 8000UL) {
    if (gameBackCheck()) return;

    bool l = digitalRead(BTN_L);
    bool s = digitalRead(BTN_S);
    bool r = digitalRead(BTN_R);

    if (oldLeft == HIGH && l == LOW) { taps++; clickSound(); }
    if (oldSel == HIGH && s == LOW) { taps++; clickSound(); }
    if (oldRight == HIGH && r == LOW) { taps++; clickSound(); }

    oldLeft = l; oldSel = s; oldRight = r;

    byte progress = min((int)taps, 50);
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(39, 0);
    display.print(F("Sprint"));
    display.setCursor(0, 11);
    display.print(F("Mash buttons!"));
    display.drawRect(8, 29, 112, 9, WHITE);
    display.fillRect(10, 31, progress * 108 / 50, 5, WHITE);
    drawCurrentPetTiny(55 + (taps % 18), 43);
    display.setCursor(0, 55);
    display.print(F("Taps "));
    display.print(taps);
    display.display();
    delay(15);
  }

  rewardGameScore(taps, 35, false, true);
  showGameResult(taps, 35);
  waitButtonRelease();
}

void brickBreakerGame() {
  NativeUI::GameScope gameCanvas(display,"LEFT","SERVE","RIGHT");
  waitButtonRelease();

  bool bricks[3][8];
  for (byte y = 0; y < 3; y++) for (byte x = 0; x < 8; x++) bricks[y][x] = true;

  int paddleX = 51;
  int ballX = 64;
  int ballY = 44;
  int vx = 2;
  int vy = -2;
  byte score = 0;
  byte lives = 3;
  unsigned long lastFrame = 0;

  while (lives > 0 && score < 24) {
    if (gameBackCheck()) return;
    bool lNow = digitalRead(BTN_L) == LOW;
    bool rNow = digitalRead(BTN_R) == LOW;

    if (lNow || rNow || digitalRead(BTN_S) == LOW) lastInputAt = millis();

    unsigned long now = millis();
    if (now - lastFrame < 45) {
      delay(2);
      continue;
    }
    lastFrame = now;

    if (lNow && paddleX > 1) paddleX -= 4;
    if (rNow && paddleX < 95) paddleX += 4;

    ballX += vx;
    ballY += vy;

    if (ballX <= 1 || ballX >= 126) vx = -vx;
    if (ballY <= 12) vy = -vy;

    for (byte by = 0; by < 3; by++) {
      for (byte bx = 0; bx < 8; bx++) {
        if (bricks[by][bx]) {
          int rx = 5 + bx * 15;
          int ry = 15 + by * 6;
          if (ballX >= rx && ballX <= rx + 12 && ballY >= ry && ballY <= ry + 4) {
            bricks[by][bx] = false;
            vy = -vy;
            score++;
            goodSound();
          }
        }
      }
    }

    if (ballY >= 54 && ballY <= 59 && ballX >= paddleX && ballX <= paddleX + 31) {
      vy = -abs(vy);
      if (ballX < paddleX + 10) vx = -2;
      else if (ballX > paddleX + 21) vx = 2;
      clickSound();
    }

    if (ballY > 63) {
      lives--;
      badSound();
      ballX = 64;
      ballY = 44;
      vx = random(0, 2) ? 2 : -2;
      vy = -2;
      delay(350);
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print(F("Brick "));
    display.print(score);
    display.setCursor(88, 0);
    display.print(F("L"));
    display.print(lives);

    for (byte by = 0; by < 3; by++) {
      for (byte bx = 0; bx < 8; bx++) {
        if (bricks[by][bx]) display.fillRect(5 + bx * 15, 15 + by * 6, 12, 4, WHITE);
      }
    }

    display.fillRoundRect(paddleX, 58, 31, 4, 2, WHITE);
    display.fillCircle(ballX, ballY, 2, WHITE);
    display.display();
  }

  rewardGameScore(score, 12, false, true);
  showGameResult(score, 12);
  waitButtonRelease();
}



void meteorPunchGame() {
  NativeUI::GameScope gameCanvas(display,"LEFT","PUNCH","RIGHT");
  waitButtonRelease();

  byte lane = 1;
  byte meteorLane = random(0, 3);
  int meteorY = 14;
  byte score = 0;
  byte miss = 0;
  unsigned long lastFrame = 0;

  while (miss < 4 && score < 20) {
    if (gameBackCheck()) return;
    bool lNow = digitalRead(BTN_L) == LOW;
    bool sNow = digitalRead(BTN_S) == LOW;
    bool rNow = digitalRead(BTN_R) == LOW;

    if (lNow || sNow || rNow) lastInputAt = millis();

    unsigned long now = millis();
    if (now - lastFrame < 70) {
      delay(2);
      continue;
    }
    lastFrame = now;

    if (lNow) lane = 0;
    if (sNow) lane = 1;
    if (rNow) lane = 2;

    meteorY += 2 + (score / 6);

    if (meteorY >= 48) {
      if (lane == meteorLane) {
        score++;
        goodSound();
      } else {
        miss++;
        badSound();
      }

      meteorLane = random(0, 3);
      meteorY = 14;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(15, 0);
    display.print(F("Meteor Punch"));
    drawRightScore(score, miss);

    for (byte i = 0; i < 3; i++) display.drawLine(21 + i * 42, 12, 21 + i * 42, 62, WHITE);
    display.fillCircle(21 + meteorLane * 42, meteorY, 4, WHITE);
    display.drawRoundRect(13 + lane * 42, 50, 17, 10, 3, WHITE);
    display.drawLine(21 + lane * 42, 50, 21 + lane * 42, 44, WHITE);

    display.setCursor(0, 55);
    display.print(F("D0 L D1 M D2 R"));
    display.display();
  }

  rewardGameScore(score, 10, false, true);
  showGameResult(score, 10);
  waitButtonRelease();
}



void doodleJumpGame() {
  NativeUI::GameScope gameCanvas(display,"LEFT","JUMP","RIGHT");
  waitButtonRelease();

  int px = random(20, 96);
  int py = 42;
  int vy = -5;

  int platX[6];
  int platY[6];
  byte platW[6];

  // Random but reachable starting map.
  platX[0] = px - 8;
  if (platX[0] < 0) platX[0] = 0;
  if (platX[0] > 96) platX[0] = 96;
  platY[0] = 55;
  platW[0] = 32;

  int lastX = platX[0];
  for (byte i = 1; i < 6; i++) {
    platY[i] = 55 - i * random(8, 12);
    int shift = random(-35, 36);
    lastX += shift;
    if (lastX < 0) lastX = random(0, 25);
    if (lastX > 96) lastX = random(70, 100);
    platX[i] = lastX;
    platW[i] = random(24, 34);
  }

  byte score = 0;
  byte falls = 0;
  unsigned long lastFrame = 0;

  while (falls < 3 && score < 40) {
    if (gameBackCheck()) return;
    bool lNow = digitalRead(BTN_L) == LOW;
    bool rNow = digitalRead(BTN_R) == LOW;
    if (lNow || rNow || digitalRead(BTN_S) == LOW) lastInputAt = millis();

    unsigned long now = millis();
    if (now - lastFrame < 60) {
      delay(2);
      continue;
    }
    lastFrame = now;

    if (lNow) px -= 3;
    if (rNow) px += 3;
    if (px < -8) px = 127;
    if (px > 127) px = -8;

    // Easier physics than before.
    vy++;
    if (vy > 4) vy = 4;
    py += vy;

    for (byte i = 0; i < 6; i++) {
      if (vy > 0 && py + 8 >= platY[i] && py + 8 <= platY[i] + 5 &&
          px + 6 >= platX[i] && px <= platX[i] + platW[i]) {
        vy = -7;
        score++;
        goodSound();
      }
    }

    if (py < 24) {
      int shift = 24 - py;
      py = 24;

      for (byte i = 0; i < 6; i++) {
        platY[i] += shift;
        if (platY[i] > 63) {
          platY[i] = random(10, 18);

          // New platform is near another upper platform so it is reachable,
          // but still varied every time.
          byte anchor = random(0, 6);
          int nx = platX[anchor] + random(-38, 39);
          if (nx < 0) nx = random(0, 28);
          if (nx > 102) nx = random(72, 102);
          platX[i] = nx;
          platW[i] = random(22, 34);
        }
      }
    }

    if (py > 63) {
      falls++;
      badSound();

      // Respawn on the lowest platform instead of instant impossible fall.
      byte best = 0;
      for (byte i = 1; i < 6; i++) {
        if (platY[i] > platY[best]) best = i;
      }

      px = platX[best] + platW[best] / 2;
      py = platY[best] - 10;
      vy = -6;
      delay(300);
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(23, 0);
    display.print(F("Jump Climb"));
    drawRightNumber(score);

    for (byte i = 0; i < 6; i++) {
      display.drawLine(platX[i], platY[i], platX[i] + platW[i], platY[i], WHITE);
      if (i == 0 && (score % 6 < 3)) display.drawPixel(platX[i] + 3, platY[i] - 1, WHITE);
    }

    display.drawRoundRect(px, py, 8, 8, 2, WHITE);

    display.setCursor(0, 55);
    display.print(F("D0/D2 move"));
    display.display();
  }

  rewardGameScore(score, 12, false, true);
  showGameResult(score, 12);
  waitButtonRelease();
}


void drawBurgerChoice(byte kind, int x, int y, bool selected) {
  if (selected) display.fillRoundRect(x - 4, y - 4, 32, 25, 5, WHITE);
  else display.drawRoundRect(x - 4, y - 4, 32, 25, 5, WHITE);

  uint16_t col = selected ? BLACK : WHITE;

  display.drawRoundRect(x, y, 22, 15, 5, col);
  display.drawLine(x + 2, y + 5, x + 20, y + 5, col);
  display.drawLine(x + 2, y + 10, x + 20, y + 10, col);

  display.setTextColor(col);
  display.setCursor(x + 8, y + 18);
  if (kind == 0) display.print(F("A"));
  else if (kind == 1) display.print(F("B"));
  else display.print(F("C"));

  if (kind == 1) {
    display.drawPixel(x + 5, y + 7, col);
    display.drawPixel(x + 16, y + 7, col);
  } else if (kind == 2) {
    display.drawLine(x + 5, y + 12, x + 17, y + 12, col);
  }

  display.setTextColor(WHITE);
}

void burgerGame() {
  NativeUI::GameScope gameCanvas(display,"PREV","STACK","NEXT");
  waitButtonRelease();

  byte choice = 0;
  byte order = random(0, 3);
  byte score = 0;
  byte misses = 0;
  unsigned long start = millis();
  bool oldLeft = HIGH, oldSel = HIGH, oldRight = HIGH;

  while (millis() - start < 18000UL && misses < 4 && score < 12) {
    if (gameBackCheck()) return;

    bool l = digitalRead(BTN_L);
    bool s = digitalRead(BTN_S);
    bool r = digitalRead(BTN_R);

    if (oldLeft == HIGH && l == LOW) {
      choice = (choice == 0) ? 2 : choice - 1;
      clickSound();
      delay(110);
    }

    if (oldRight == HIGH && r == LOW) {
      choice++;
      if (choice > 2) choice = 0;
      clickSound();
      delay(110);
    }

    if (oldSel == HIGH && s == LOW) {
      if (choice == order) {
        score++;
        goodSound();
      } else {
        misses++;
        badSound();
      }

      order = random(0, 3);
      waitButtonRelease();
    }

    oldLeft = l;
    oldSel = s;
    oldRight = r;

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(25, 0);
    display.print(F("Burger Bite"));
    drawRightScore(score, misses);

    display.setCursor(20, 12);
    display.print(F("Order: "));
    if (order == 0) display.print(F("A Plain"));
    else if (order == 1) display.print(F("B Seed"));
    else display.print(F("C Stripe"));

    drawBurgerChoice(0, 12, 31, choice == 0);
    drawBurgerChoice(1, 51, 31, choice == 1);
    drawBurgerChoice(2, 90, 31, choice == 2);

    display.setCursor(0, 55);
    display.print(F("Match order D1 serve"));
    display.display();
    delay(20);
  }

  addStat(happy, score > 5 ? 5 : score);
  addStat(full, score > 4 ? 2 : 1);
  if (misses >= 4) subStat(happy, 1);
  if (random(0, 4) == 0) subStat(energy, 1);
  clampAll();
  showGameResult(score, 6);
  waitButtonRelease();
}


void drawMatchCardFace(byte value, int x, int y, uint16_t col) {
  // Six simple card symbols, readable on a tiny 128x64 OLED.
  if (value == 0) {
    display.drawCircle(x + 6, y + 5, 3, col);
  } else if (value == 1) {
    display.drawRect(x + 3, y + 2, 7, 7, col);
  } else if (value == 2) {
    display.drawTriangle(x + 6, y + 1, x + 1, y + 9, x + 11, y + 9, col);
  } else if (value == 3) {
    display.drawLine(x + 2, y + 5, x + 10, y + 5, col);
    display.drawLine(x + 6, y + 1, x + 6, y + 9, col);
  } else if (value == 4) {
    display.drawLine(x + 2, y + 2, x + 10, y + 8, col);
    display.drawLine(x + 10, y + 2, x + 2, y + 8, col);
  } else {
    display.drawCircle(x + 4, y + 4, 2, col);
    display.drawCircle(x + 8, y + 4, 2, col);
    display.drawCircle(x + 6, y + 8, 2, col);
  }
}

void drawMatchBoard(byte cards[12], uint16_t matchedMask, uint16_t faceUpMask, byte cursor, byte matches, byte mistakes, bool previewMode=false) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(25, 0);
  display.print(F("Card Match"));
  drawRightScore(matches, mistakes);

  for (byte i = 0; i < 12; i++) {
    byte gx = i % 4;
    byte gy = i / 4;
    int x = 22 + gx * 23;
    int y = 15 + gy * 13;

    bool selected = i == cursor;
    bool matched = matchedMask & (1 << i);
    bool up = previewMode || matched || (faceUpMask & (1 << i));

    if (selected) display.fillRoundRect(x - 2, y - 2, 19, 12, 3, WHITE);
    else display.drawRoundRect(x - 2, y - 2, 19, 12, 3, WHITE);

    uint16_t col = selected ? BLACK : WHITE;

    if (up) {
      drawMatchCardFace(cards[i], x + 1, y, col);
      if (matched) {
        display.drawPixel(x + 14, y + 1, col);
        display.drawLine(x + 12, y + 4, x + 14, y + 6, col);
        display.drawLine(x + 14, y + 6, x + 17, y + 2, col);
      }
    } else {
      display.drawPixel(x + 4, y + 3, col);
      display.drawPixel(x + 10, y + 3, col);
      display.drawLine(x + 4, y + 7, x + 11, y + 7, col);
    }
  }

  display.setTextColor(WHITE);
  display.setCursor(0, 55);
  if (previewMode) display.print(F("Memorize pieces..."));
  else display.print(F("D2 hold down/exit"));
  display.display();
}


void slidePuzzleGame() {
  NativeUI::GameScope gameCanvas(display,"PREV","MOVE","NEXT");
  // v18.5: this is now a real matching card game instead of the confusing slide matcher.
  waitButtonRelease();

  byte cards[12] = {
    0, 0, 1, 1, 2, 2,
    3, 3, 4, 4, 5, 5
  };

  // Shuffle cards.
  for (byte i = 0; i < 12; i++) {
    byte j = random(0, 12);
    byte temp = cards[i];
    cards[i] = cards[j];
    cards[j] = temp;
  }

  byte cursor = random(0, 12);
  uint16_t matchedMask = 0;
  uint16_t faceUpMask = 0;
  byte firstPick = 255;
  byte matches = 0;
  byte mistakes = 0;

  // Show all pieces first, then hide them.
  unsigned long previewStart = millis();
  while (millis() - previewStart < 3200UL) {
    if (anyButtonDown()) lastInputAt = millis();
    drawMatchBoard(cards, matchedMask, 0x0FFF, cursor, matches, mistakes, true);
    delay(80);
  }

  bool oldLeft = HIGH;
  bool oldSel = HIGH;
  bool oldRight = HIGH;
  unsigned long leftDown = 0;
  unsigned long rightDown = 0;
  bool leftHoldUsed = false;
  bool rightHoldUsedLocal = false;

  while (matches < 6 && mistakes < 9) {
    bool l = digitalRead(BTN_L);
    bool s = digitalRead(BTN_S);
    bool r = digitalRead(BTN_R);
    unsigned long now = millis();

    if (l == LOW || s == LOW || r == LOW) lastInputAt = now;

    // Same movement style as Mini Mines:
    // D0 short = left, D2 short = right
    // D0 long = up, D2 long = down
    // D1 = select
    if (oldLeft == HIGH && l == LOW) {
      leftDown = now;
      leftHoldUsed = false;
    }

    if (oldRight == HIGH && r == LOW) {
      rightDown = now;
      rightHoldUsedLocal = false;
    }

    // D2 long is down. Holding D2 about 2 seconds longer exits.
    if (r == LOW && rightDown > 0 && now - rightDown > LONG_RIGHT_GAME_EXIT_MS) {
      waitButtonRelease();
      drawScreen(true);
      return;
    }

    if (l == LOW && !leftHoldUsed && leftDown > 0 && now - leftDown > LONG_RIGHT_GAME_ACTION_MS) {
      if (cursor >= 4) cursor -= 4;
      leftHoldUsed = true;
      clickSound();
    }

    if (r == LOW && !rightHoldUsedLocal && rightDown > 0 && now - rightDown > LONG_RIGHT_GAME_ACTION_MS) {
      if (cursor < 8) cursor += 4;
      rightHoldUsedLocal = true;
      clickSound();
    }

    if (oldLeft == LOW && l == HIGH && !leftHoldUsed) {
      if ((cursor % 4) > 0) cursor--;
      clickSound();
    }

    if (oldRight == LOW && r == HIGH && !rightHoldUsedLocal) {
      if ((cursor % 4) < 3) cursor++;
      clickSound();
    }

    if (oldSel == HIGH && s == LOW) {
      if (matchedMask & (1 << cursor)) {
        badSound();
      } else if (firstPick == 255) {
        firstPick = cursor;
        faceUpMask |= (1 << cursor);
        clickSound();
      } else if (firstPick == cursor) {
        badSound();
      } else {
        faceUpMask |= (1 << cursor);
        drawMatchBoard(cards, matchedMask, faceUpMask, cursor, matches, mistakes, false);
        delay(550);

        if (cards[firstPick] == cards[cursor]) {
          matchedMask |= (1 << firstPick);
          matchedMask |= (1 << cursor);
          matches++;
          goodSound();
        } else {
          mistakes++;
          badSound();
          faceUpMask &= ~(1 << firstPick);
          faceUpMask &= ~(1 << cursor);
        }

        firstPick = 255;
      }

      waitButtonRelease();
    }

    oldLeft = l;
    oldSel = s;
    oldRight = r;

    drawMatchBoard(cards, matchedMask, faceUpMask, cursor, matches, mistakes, false);
    delay(25);
  }

  if (matches >= 6) {
    winSound();
    popup(F("All matches!"), F("Great memory"), 900);
  } else {
    badSound();
    popup(F("Too many misses"), F("Try again"), 900);
  }

  // Wrong guesses are the score against you. Lower mistakes = better reward.
  byte reward = 1;
  if (matches >= 6) {
    if (mistakes <= 2) reward = 5;
    else if (mistakes <= 5) reward = 3;
    else reward = 2;
  }

  addStat(happy, reward);
  addSkill(skillSmart, reward >= 3 ? 2 : 1);
  if (mistakes >= 7) subStat(discipline, 1);
  if (random(0, 4) == 0) subStat(energy, 1);

  clampAll();
  resolveArcadeSession(matches, 6);
  showGameResult(matches, 6);
  waitButtonRelease();
}


const __FlashStringHelper* gameName(byte gameId) {
  switch (gameId % 13) {
    case 0: return F("Star Catch");
    case 1: return F("Rock Paper");
    case 2: return F("Number Guess");
    case 3: return F("Block Stack");
    case 4: return F("Mini Mines");
    case 5: return F("Look Guess");
    case 6: return F("Memory");
    case 7: return F("Sprint");
    case 8: return F("Brick Breaker");
    case 9: return F("Meteor Punch");
    case 10: return F("Jump Climb");
    case 11: return F("Burger Bite");
    default: return F("Card Match");
  }
}

byte availableGameCount() {
  if (stage == STAGE_EGG) return 0;
  if (stage == STAGE_BABY) return 3;
  if (stage == STAGE_CHILD) return 6;
  if (stage == STAGE_TEEN) return 10;
  return 13;
}

byte dailyFeaturedGame(byte slot) {
  const byte count = availableGameCount();
  if (count == 0) return 0;
  return (byte)((petAgeDays() * 5UL + birthCode + slot * 4UL) % count);
}

int featuredGameSlot(byte gameId) {
  for (byte i = 0; i < 3; i++) if (dailyFeaturedGame(i) == gameId) return i;
  return -1;
}

void finishGameSession(byte gameId) {
  if (gameId >= 13 || !GameplayProgress::finishedGame(activeGameResolved)) return;
  activeGameResolved = false; // consume the result exactly once
  completeGameplayAction(M_GAME);
  lifeWorld.recordGameResult(gameId, activeGameWon);
  deepLife.recordGameResult(gameId, activeGameWon, activeGameScore);

  if (gameCounts[gameId] < 255) gameCounts[gameId]++;
  logCareEvent(14);
  if (exerciseScore < 100 && (gameId == 0 || gameId == 7 || gameId == 8 || gameId == 9 || gameId == 10)) exerciseScore++;
  if (weight > 1 && (gameId == 7 || gameId == 10)) weight--;
  markRoutineAction(ROUTINE_PLAY);
  recordGoalEvent(2, 1);

  // Relationship rewards are capped to one per life tick.
  if (lastGameBondTick != ageTicks) {
    changeBond(gameId == favoriteGameId ? 2 : 1);
    changeTrust(1);
    lastGameBondTick = ageTicks;
  }

  unsigned long today = petAgeDays();
  if (featuredDay != today) {
    featuredDay = today;
    featuredPlayedMask = 0;
  }
  int slot = featuredGameSlot(gameId);
  if (slot >= 0 && !(featuredPlayedMask & (1 << slot))) {
    featuredPlayedMask |= (1 << slot);
    addCoins(2);
    addStat(happy, 1);
  }

  if (gameId == favoriteGameId) addStat(happy, 1);
  // Preferences can develop from repeated play instead of being fixed forever.
  byte mostPlayed = 0, leastPlayed = 0;
  for (byte i = 1; i < 13; i++) { if (gameCounts[i] > gameCounts[mostPlayed]) mostPlayed = i; if (gameCounts[i] < gameCounts[leastPlayed]) leastPlayed = i; }
  if (gameCounts[mostPlayed] >= 4) favoriteGameId = mostPlayed;
  if (gameCounts[leastPlayed] + 4 < gameCounts[mostPlayed]) dislikedGameId = leastPlayed;
  updateDevelopedHobby();
  markSaveDirty();
}

void showGameIntro(byte gameId);

void launchGame(byte gameId) {
  showGameIntro(gameId);
  activeGameId = gameId;
  activeGameResolved = false;
  activeGameWon = false;
  activeGameScore = 0;
  activeGameStartedAt = millis();
  if (gameId == 0) starCatchGame();
  else if (gameId == 1) rpsGame();
  else if (gameId == 2) numberGame();
  else if (gameId == 3) tinyBlocksGame();
  else if (gameId == 4) minesweeperGame();
  else if (gameId == 5) lookGuessGame();
  else if (gameId == 6) memoryGame();
  else if (gameId == 7) sprintGame();
  else if (gameId == 8) brickBreakerGame();
  else if (gameId == 9) meteorPunchGame();
  else if (gameId == 10) doodleJumpGame();
  else if (gameId == 11) burgerGame();
  else slidePuzzleGame();
  finishGameSession(gameId);
  activeGameId = 255;
}


void drawGamePreviewIcon(byte gameId, int x, int y) {
  gameId %= 13;
  if (gameId == 0) { display.drawLine(x + 8, y, x + 8, y + 16, WHITE); display.drawLine(x, y + 8, x + 16, y + 8, WHITE); display.drawPixel(x + 8, y + 8, WHITE); }
  else if (gameId == 1) { display.drawCircle(x + 6, y + 9, 5, WHITE); display.drawLine(x + 11, y + 9, x + 17, y + 5, WHITE); }
  else if (gameId == 2) { display.setTextSize(2); display.setCursor(x + 2, y); display.print(F("?")); display.setTextSize(1); }
  else if (gameId == 3) { display.drawRect(x + 1, y + 11, 15, 5, WHITE); display.drawRect(x + 4, y + 6, 10, 5, WHITE); display.drawRect(x + 7, y + 1, 6, 5, WHITE); }
  else if (gameId == 4) { display.drawRect(x, y, 17, 17, WHITE); display.drawLine(x + 5, y, x + 5, y + 17, WHITE); display.drawLine(x + 11, y, x + 11, y + 17, WHITE); display.drawLine(x, y + 6, x + 17, y + 6, WHITE); display.drawLine(x, y + 12, x + 17, y + 12, WHITE); }
  else if (gameId == 5) { display.drawCircle(x + 8, y + 8, 7, WHITE); display.fillCircle(x + 8, y + 8, 2, WHITE); }
  else if (gameId == 6 || gameId == 12) { display.drawRect(x, y + 2, 7, 10, WHITE); display.drawRect(x + 10, y + 2, 7, 10, WHITE); display.drawPixel(x + 3, y + 6, WHITE); display.drawPixel(x + 13, y + 6, WHITE); }
  else if (gameId == 7) { display.drawLine(x + 8, y + 2, x + 8, y + 10, WHITE); display.drawLine(x + 8, y + 5, x + 2, y + 8, WHITE); display.drawLine(x + 8, y + 10, x + 2, y + 16, WHITE); display.drawLine(x + 8, y + 10, x + 16, y + 15, WHITE); }
  else if (gameId == 8) { display.drawLine(x, y + 15, x + 17, y + 15, WHITE); display.drawRect(x + 5, y + 2, 8, 4, WHITE); display.fillCircle(x + 8, y + 10, 2, WHITE); }
  else if (gameId == 9) { display.drawCircle(x + 8, y + 8, 6, WHITE); display.drawLine(x + 1, y + 1, x + 5, y + 5, WHITE); display.drawLine(x + 15, y + 1, x + 11, y + 5, WHITE); }
  else if (gameId == 10) { display.drawLine(x + 2, y + 15, x + 15, y + 15, WHITE); display.drawLine(x + 4, y + 11, x + 12, y + 11, WHITE); display.drawLine(x + 7, y + 7, x + 16, y + 7, WHITE); display.drawCircle(x + 5, y + 3, 2, WHITE); }
  else { display.drawRoundRect(x, y + 4, 17, 10, 4, WHITE); display.drawLine(x + 4, y + 4, x + 4, y, WHITE); display.drawLine(x + 13, y + 4, x + 13, y, WHITE); }
}

void showGameIntro(byte gameId) {
  for (byte f = 0; f < 4; f++) {
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.drawRoundRect(5, 8, 118, 48, 7, WHITE);
    drawGamePreviewIcon(gameId, 15, 23);
    // v38: text-heavy pre-game screens intentionally contain no pet sprite.
    // The old 16x16 silhouette covered the status line on the 128x64 display.
    display.drawFastVLine(35, 17, 31, WHITE);
    display.setCursor(41, 18); display.print(gameName(gameId));
    display.setCursor(41, 31);
    if (gameId == favoriteGameId) display.print(F("Favorite +bond"));
    else if (gameId == dislikedGameId) display.print(F("Challenge game"));
    else if (featuredGameSlot(gameId) >= 0) display.print(F("Featured +2G"));
    else display.print(F("Ready to play"));
    display.setCursor(41, 43); display.print(F("D1 starts"));
    display.display(); delay(105);
  }
}

void miniGameMenu() {
  clickSound(); waitButtonRelease();
  byte gameCount = availableGameCount();
  if (gameCount == 0) { popup(F("Games locked"), F("Wait for baby")); return; }
  byte g = dailyFeaturedGame(0); bool oldLeft = HIGH, oldSel = HIGH, oldRight = HIGH;
  while (true) {
    if (anyButtonDown()) lastInputAt = millis();
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(BTN_L), sBtn = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (oldLeft == HIGH && l == LOW) { g = (g == 0) ? gameCount - 1 : g - 1; clickSound(); delay(120); }
    if (oldRight == HIGH && r == LOW) { g++; if (g >= gameCount) g = 0; clickSound(); delay(120); }
    if (oldSel == HIGH && sBtn == LOW) { waitButtonRelease(); launchGame(g); lastInputAt = millis(); return; }
    oldLeft = l; oldSel = sBtn; oldRight = r;


    {
      NativeUI::Scope frame(display);display.clearDisplay();char counter[12];snprintf(counter,sizeof(counter),"%u/%u",g+1,gameCount);
      NativeUI::header(display,"ARCADE",counter);NativeUI::icon(display,2,56,26);
      char name[96];NativeUI::flashCopy(name,gameName(g),sizeof(name));NativeUI::paragraph(display,name,8,53,112,2);
      const char *badge=g==favoriteGameId?"FAVORITE +BOND":featuredGameSlot(g)>=0?"FEATURED +2G":g==dislikedGameId?"CHALLENGE":"READY TO PLAY";
      NativeUI::text(display,badge,8,90,112);NativeUI::footer(display,"PREV","PLAY","NEXT");display.display();
    }
    delay(20);
  }
}



// ---------- v45 life-stage schedule games ----------
// Five dedicated games are available for every awake life stage.  The same
// games can be practiced at home, but child/teen school hours and adult work
// hours award a schedule bonus.  None of these games restore energy, so they
// cannot replace the enforced night-sleep schedule.

const __FlashStringHelper* stageRoutineGameName(byte lifeStage, byte gameId) {
  gameId %= 5;
  if (lifeStage == STAGE_BABY) {
    if (gameId == 0) return F("Toy Catch");
    if (gameId == 1) return F("Crawl Around");
    if (gameId == 2) return F("Clap Along");
    if (gameId == 3) return F("Peekaboo");
    return F("Shape Sort");
  }
  if (lifeStage == STAGE_CHILD) {
    if (gameId == 0) return F("Pencil Catch");
    if (gameId == 1) return F("Hallway Dodge");
    if (gameId == 2) return F("Recess Jump");
    if (gameId == 3) return F("Spelling Steps");
    return F("Lunch Sort");
  }
  if (lifeStage == STAGE_TEEN) {
    if (gameId == 0) return F("Note Catch");
    if (gameId == 1) return F("Locker Rush");
    if (gameId == 2) return F("Band Timing");
    if (gameId == 3) return F("Quiz Pattern");
    return F("Group Project");
  }
  if (lifeStage == STAGE_ADULT) {
    if (gameId == 0) return F("Supply Catch");
    if (gameId == 1) return F("Coworker Dodge");
    if (gameId == 2) return F("Meeting Timing");
    if (gameId == 3) return F("Inbox Order");
    return F("Commute Sort");
  }
  if (gameId == 0) return F("Herb Basket");
  if (gameId == 1) return F("Garden Stroll");
  if (gameId == 2) return F("Gentle Stretch");
  if (gameId == 3) return F("Story Order");
  return F("Tea Sort");
}

const __FlashStringHelper* stageRoutineTitle(byte lifeStage) {
  if (lifeStage == STAGE_BABY) return F("BABY PLAY");
  if (lifeStage == STAGE_CHILD) return F("SCHOOL PLAY");
  if (lifeStage == STAGE_TEEN) return F("TEEN SCHOOL");
  if (lifeStage == STAGE_ADULT) return F("WORK PLAY");
  return F("ELDER ROUTINE");
}

bool stageRoutineBonusTime(byte lifeStage) {
  byte hour = visualHour();
  if (lifeStage == STAGE_CHILD || lifeStage == STAGE_TEEN) return isScheduledSchoolTime();
  if (lifeStage == STAGE_ADULT) return isScheduledWorkTime();
  if (lifeStage == STAGE_BABY) return (hour >= 9 && hour < 11) || (hour >= 14 && hour < 16);
  if (lifeStage == STAGE_ELDER) return (hour >= 8 && hour < 11) || (hour >= 15 && hour < 17);
  return false;
}

void drawStageRoutineObject(byte lifeStage, byte gameId, byte kind, int x, int y) {
  // Every object is kept inside a small high-contrast footprint so the player
  // and hazards remain easy to distinguish on the 128x64 monochrome display.
  if (gameId == 0) { // catch object
    if (lifeStage == STAGE_BABY) { display.drawCircle(x + 4, y + 4, 4, WHITE); display.drawPixel(x + 2, y + 3, WHITE); display.drawPixel(x + 6, y + 3, WHITE); }
    else if (lifeStage == STAGE_CHILD) { display.drawLine(x + 1, y + 7, x + 7, y + 1, WHITE); display.drawLine(x + 2, y + 8, x + 8, y + 2, WHITE); }
    else if (lifeStage == STAGE_TEEN) { display.drawRect(x + 1, y + 1, 7, 8, WHITE); display.drawLine(x + 3, y + 3, x + 7, y + 3, WHITE); }
    else if (lifeStage == STAGE_ADULT) { display.drawRect(x, y + 2, 9, 7, WHITE); display.drawLine(x + 2, y + 2, x + 2, y, WHITE); display.drawLine(x + 7, y + 2, x + 7, y, WHITE); }
    else { display.drawLine(x + 4, y, x + 4, y + 8, WHITE); display.drawLine(x + 1, y + 3, x + 7, y + 3, WHITE); display.drawPixel(x + 2, y + 1, WHITE); }
    return;
  }
  if (gameId == 1) { // obstacle
    if (lifeStage == STAGE_BABY) display.drawRoundRect(x, y, 10, 8, 3, WHITE);
    else if (lifeStage == STAGE_CHILD) { display.drawRect(x, y + 2, 11, 6, WHITE); display.drawLine(x + 2, y + 2, x + 4, y, WHITE); }
    else if (lifeStage == STAGE_TEEN) { display.drawRect(x, y, 10, 9, WHITE); display.drawLine(x + 2, y + 3, x + 8, y + 3, WHITE); }
    else if (lifeStage == STAGE_ADULT) { display.drawCircle(x + 5, y + 4, 4, WHITE); display.drawLine(x + 1, y + 8, x + 9, y, WHITE); }
    else { display.drawLine(x, y + 8, x + 5, y, WHITE); display.drawLine(x + 5, y, x + 10, y + 8, WHITE); }
    return;
  }
  if (gameId == 2) { // timing symbol
    if (kind == 0) display.drawCircle(x + 5, y + 5, 4, WHITE);
    else { display.drawLine(x, y + 5, x + 10, y + 5, WHITE); display.drawLine(x + 5, y, x + 5, y + 10, WHITE); }
    return;
  }
  if (gameId == 3) { // memory arrow/card
    if (kind == 0) { display.drawLine(x + 9, y + 1, x + 2, y + 5, WHITE); display.drawLine(x + 2, y + 5, x + 9, y + 9, WHITE); }
    else { display.drawLine(x + 1, y + 1, x + 8, y + 5, WHITE); display.drawLine(x + 8, y + 5, x + 1, y + 9, WHITE); }
    return;
  }
  // sort object: two visually distinct categories.
  if (kind == 0) display.drawCircle(x + 5, y + 5, 4, WHITE);
  else display.drawRect(x + 1, y + 1, 9, 9, WHITE);
}

void drawStageRoutinePlayer(byte lifeStage, int x, int y) {
  display.fillRoundRect(x - 2, y - 2, 17, 13, 4, BLACK);
  display.drawRoundRect(x - 2, y - 2, 17, 13, 4, WHITE);
  if (lifeStage == STAGE_BABY) {
    display.drawCircle(x + 5, y + 4, 4, WHITE);
    display.drawPixel(x + 4, y + 3, WHITE); display.drawPixel(x + 7, y + 3, WHITE);
  } else {
    drawCurrentPetTiny(x, y);
  }
}

void stageRoutineReward(byte lifeStage, byte gameId, byte score, byte goal) {
  bool won = score >= goal;
  bool timedBonus = stageRoutineBonusTime(lifeStage);

  addStat(happy, won ? 3 : 1);
  changeBond(won ? 2 : 1);
  if (won) changeTrust(1);
  // Playing is enjoyable but never acts as sleep. Every finished routine game
  // costs a little energy, even if another system currently has a buff active.
  subStat(energy, 1);

  if (lifeStage == STAGE_BABY) {
    addSkill((gameId == 1 || gameId == 2) ? skillFit : skillKind, won ? 2 : 1);
    if (timedBonus) addStat(discipline, 1);
  } else if (lifeStage == STAGE_CHILD) {
    addSkill((gameId == 1 || gameId == 2) ? skillFit : skillSmart, won ? 2 : 1);
    if (won || timedBonus) addStat(discipline, 1);
    if (timedBonus) studyStars = min((byte)255, (byte)(studyStars + 1));
  } else if (lifeStage == STAGE_TEEN) {
    if (gameId == 1 || gameId == 2) addSkill(skillFit, won ? 2 : 1);
    else if (gameId == 4) addSkill(skillKind, won ? 2 : 1);
    else addSkill(skillSmart, won ? 2 : 1);
    if (timedBonus) { addStat(discipline, 1); deepLife.recordAction(M_STUDY, petAgeDays(), visualHour(), visualWeather()); }
  } else if (lifeStage == STAGE_ADULT) {
    addSkill((gameId == 1 || gameId == 4) ? skillFit : skillSmart, won ? 2 : 1);
    if (lifeWorld.careerId() < LIFE_CAREER_COUNT) {
      lifeWorld.eventCareerProgress(won ? (timedBonus ? 8 : 5) : 2, 2, won ? 2 : 1);
    }
    if (won) addCoins(timedBonus ? 3 : 1);
  } else {
    addSkill((gameId == 1 || gameId == 2) ? skillFit : (gameId == 3 ? skillSmart : skillKind), won ? 2 : 1);
    if (timedBonus) changeBond(1);
    if (won) deepLife.recordAction(M_FAMILY, petAgeDays(), visualHour(), visualWeather());
  }

  markRoutineAction(ROUTINE_PLAY);
  recordGoalEvent(2, 1);
  completeGameplayAction(M_GAME);
  clampAll();
  markSaveDirty();
  showGameResult(score, goal);
}

void stageCatchGame(byte lifeStage) {
  NativeUI::GameScope gameCanvas(display,"LEFT","CATCH","RIGHT");
  byte score = 0;
  int playerX = 56;
  for (byte item = 0; item < 12; ++item) {
    int itemX = random(6, 117), itemY = 15;
    unsigned long lastFall = millis();
    while (itemY < 52) {
      if (gameBackCheck()) return;
      if (digitalRead(BTN_L) == LOW) playerX = max(4, playerX - 2);
      if (digitalRead(BTN_R) == LOW) playerX = min(111, playerX + 2);
      if (millis() - lastFall >= (unsigned long)(58 - min((int)item, 8) * 3)) { itemY += 2; lastFall = millis(); }

      display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
      display.setCursor(2, 0); display.print(stageRoutineGameName(lifeStage, 0));
      display.setCursor(96, 0); display.print(score); display.print(F("/12"));
      display.drawLine(0, 11, 127, 11, WHITE);
      drawStageRoutineObject(lifeStage, 0, 0, itemX, itemY);
      drawStageRoutinePlayer(lifeStage, playerX, 50);
      display.display(); delay(18);
    }
    if (abs((playerX + 6) - (itemX + 4)) <= 10) { score++; clickSound(); }
  }
  stageRoutineReward(lifeStage, 0, score, 8);
}

void stageDodgeGame(byte lifeStage) {
  NativeUI::GameScope gameCanvas(display,"LEFT","DODGE","RIGHT");
  byte safe = 0;
  int playerX = 56;
  for (byte wave = 0; wave < 12; ++wave) {
    int obstacleX = random(4, 114), obstacleY = 14;
    bool hit = false;
    unsigned long lastFall = millis();
    while (obstacleY < 51) {
      if (gameBackCheck()) return;
      if (digitalRead(BTN_L) == LOW) playerX = max(4, playerX - 2);
      if (digitalRead(BTN_R) == LOW) playerX = min(111, playerX + 2);
      if (millis() - lastFall >= (unsigned long)(52 - min((int)wave, 8) * 2)) { obstacleY += 2; lastFall = millis(); }
      if (obstacleY >= 43 && obstacleY <= 57 && abs((playerX + 6) - (obstacleX + 5)) < 11) hit = true;

      display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
      display.setCursor(2, 0); display.print(stageRoutineGameName(lifeStage, 1));
      display.setCursor(100, 0); display.print(safe);
      display.drawLine(0, 11, 127, 11, WHITE);
      drawStageRoutineObject(lifeStage, 1, 0, obstacleX, obstacleY);
      drawStageRoutinePlayer(lifeStage, playerX, 50);
      display.display(); delay(18);
    }
    if (!hit) safe++; else badSound();
  }
  stageRoutineReward(lifeStage, 1, safe, 9);
}

void stageTimingGame(byte lifeStage) {
  NativeUI::GameScope gameCanvas(display,"LEFT","STOP","RIGHT");
  byte score = 0;
  for (byte round = 0; round < 8; ++round) {
    byte target = random(24, 83);
    bool pressed = false;
    byte landed = 0;
    bool oldSelect = HIGH;
    unsigned long started = millis();
    while (millis() - started < 1900UL) {
      if (gameBackCheck()) return;
      unsigned long phase = (millis() - started) / 13UL;
      byte cursor = static_cast<byte>(phase % 180UL);
      if (cursor > 90) cursor = 180 - cursor;
      bool nowSelect = digitalRead(BTN_S);
      if (oldSelect == HIGH && nowSelect == LOW) { pressed = true; landed = cursor; waitButtonRelease(); }
      oldSelect = nowSelect;

      display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
      display.setCursor(2, 0); display.print(stageRoutineGameName(lifeStage, 2));
      display.setCursor(98, 0); display.print(score); display.print(F("/8"));
      display.drawLine(10, 33, 117, 33, WHITE);
      display.fillRect(15 + target, 27, 18, 13, WHITE);
      display.drawFastVLine(15 + cursor, 23, 21, WHITE);
      drawStageRoutineObject(lifeStage, 2, round & 1, 58, 48);
      display.setCursor(24, 14); display.print(F("D1 in bright zone"));
      display.display(); delay(12);
      if (pressed) break;
    }
    if (pressed && landed >= target && landed <= target + 17) { score++; clickSound(); }
    else badSound();
    delay(120);
  }
  stageRoutineReward(lifeStage, 2, score, 5);
}

void stageMemoryGame(byte lifeStage) {
  NativeUI::GameScope gameCanvas(display,"A","B","C");
  byte score = 0;
  for (byte round = 0; round < 5; ++round) {
    byte sequence[6];
    byte length = min((byte)6, (byte)(2 + round));
    for (byte i = 0; i < length; ++i) sequence[i] = random(0, 2);

    for (byte i = 0; i < length; ++i) {
      display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
      display.setCursor(2, 0); display.print(stageRoutineGameName(lifeStage, 3));
      display.setCursor(38, 15); display.print(F("Remember"));
      drawStageRoutineObject(lifeStage, 3, sequence[i], 58, 31);
      display.display(); delay(360);
      display.clearDisplay(); display.display(); delay(110);
    }

    bool correct = true;
    for (byte i = 0; i < length; ++i) {
      bool chosen = false;
      byte answer = 0;
      bool oldL = HIGH, oldR = HIGH;
      unsigned long started = millis();
      while (!chosen && millis() - started < 2800UL) {
        if (gameBackCheck()) return;
        bool l = digitalRead(BTN_L), r = digitalRead(BTN_R);
        if (oldL == HIGH && l == LOW) { answer = 0; chosen = true; clickSound(); waitButtonRelease(); }
        if (oldR == HIGH && r == LOW) { answer = 1; chosen = true; clickSound(); waitButtonRelease(); }
        oldL = l; oldR = r;
        display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
        display.setCursor(2, 0); display.print(stageRoutineGameName(lifeStage, 3));
        display.setCursor(25, 14); display.print(F("Repeat step ")); display.print(i + 1); display.print('/'); display.print(length);
        drawStageRoutineObject(lifeStage, 3, 0, 25, 35);
        drawStageRoutineObject(lifeStage, 3, 1, 91, 35);
        display.setCursor(4, 54); display.print(F("D0 left        D2 right"));
        display.display(); delay(18);
      }
      if (!chosen || answer != sequence[i]) correct = false;
    }
    if (correct) { score++; winSound(); } else badSound();
  }
  stageRoutineReward(lifeStage, 3, score, 3);
}

void stageSortGame(byte lifeStage) {
  NativeUI::GameScope gameCanvas(display,"LEFT","CHOOSE","RIGHT");
  byte score = 0;
  for (byte round = 0; round < 12; ++round) {
    byte kind = random(0, 2);
    bool chosen = false;
    byte answer = 2;
    bool oldL = HIGH, oldR = HIGH;
    unsigned long started = millis();
    while (!chosen && millis() - started < 2400UL) {
      if (gameBackCheck()) return;
      bool l = digitalRead(BTN_L), r = digitalRead(BTN_R);
      if (oldL == HIGH && l == LOW) { answer = 0; chosen = true; clickSound(); waitButtonRelease(); }
      if (oldR == HIGH && r == LOW) { answer = 1; chosen = true; clickSound(); waitButtonRelease(); }
      oldL = l; oldR = r;

      display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
      display.setCursor(2, 0); display.print(stageRoutineGameName(lifeStage, 4));
      display.setCursor(96, 0); display.print(score); display.print(F("/12"));
      display.drawLine(0, 11, 127, 11, WHITE);
      drawStageRoutineObject(lifeStage, 4, kind, 58, 20);
      display.drawRoundRect(8, 43, 42, 17, 4, WHITE);
      display.drawCircle(29, 51, 4, WHITE);
      display.drawRoundRect(78, 43, 42, 17, 4, WHITE);
      display.drawRect(95, 47, 9, 9, WHITE);
      display.setCursor(13, 34); display.print(F("D0"));
      display.setCursor(101, 34); display.print(F("D2"));
      display.display(); delay(18);
    }
    if (chosen && answer == kind) { score++; clickSound(); }
    else badSound();
  }
  stageRoutineReward(lifeStage, 4, score, 8);
}

void launchStageRoutineGame(byte gameId) {
  byte lifeStage = stage;
  if (lifeStage < STAGE_BABY || lifeStage > STAGE_ELDER) return;
  activeGameId = 255; // These games use their own age-specific reward rules.
  if (gameId == 0) stageCatchGame(lifeStage);
  else if (gameId == 1) stageDodgeGame(lifeStage);
  else if (gameId == 2) stageTimingGame(lifeStage);
  else if (gameId == 3) stageMemoryGame(lifeStage);
  else stageSortGame(lifeStage);
  synchronizeDailySchedule(false); // Bedtime can arrive while a game is open.
}

void stageRoutineGameMenu() {
  if (stage == STAGE_EGG) { popup(F("Still an egg"), F("Games begin after hatch"), 800); return; }
  byte selected = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(BTN_L), sBtn = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) { selected = selected == 0 ? 4 : selected - 1; clickSound(); delay(110); }
    if (oldR == HIGH && r == LOW) { selected = (selected + 1) % 5; clickSound(); delay(110); }
    if (oldS == HIGH && sBtn == LOW) { waitButtonRelease(); launchStageRoutineGame(selected); return; }
    oldL = l; oldS = sBtn; oldR = r;


    {
      NativeUI::Scope frame(display);display.clearDisplay();char counter[12];snprintf(counter,sizeof(counter),"%u/5",selected+1);
      NativeUI::header(display,stageRoutineTitle(stage),counter);NativeUI::icon(display,selected%6,56,27);
      char name[96];NativeUI::flashCopy(name,stageRoutineGameName(stage,selected),sizeof(name));NativeUI::paragraph(display,name,8,54,112,2);
      NativeUI::text(display,stageRoutineBonusTime(stage)?"BONUS ACTIVE":"DAILY PRACTICE",8,91,112);
      NativeUI::footer(display,"PREV","PLAY","NEXT");display.display();
    }
    delay(20);
  }
}

void gameHubMenu() {
  // While the pet is physically at school or work, the stage games replace
  // the previous lockout. At home, the player may choose arcade or life-stage
  // activities. Sleeping remains the only time games are unavailable.
  if (isScheduledAwayTime()) { stageRoutineGameMenu(); return; }
  byte selected = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(BTN_L), sBtn = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if ((oldL == HIGH && l == LOW) || (oldR == HIGH && r == LOW)) { selected ^= 1; clickSound(); delay(120); }
    if (oldS == HIGH && sBtn == LOW) {
      waitButtonRelease();
      if (selected == 0) miniGameMenu(); else stageRoutineGameMenu();
      return;
    }
    oldL = l; oldS = sBtn; oldR = r;


    {
      NativeUI::Scope frame(display);display.clearDisplay();NativeUI::header(display,"PLAY TOGETHER");
      const char *names[]={"ARCADE","LIFE STAGE"};const char *details[]={"Quick games","Daily games"};
      for(byte row=0;row<2;++row){int y=27+row*42;bool active=selected==row;
        if(active)display.fillRoundRect(4,y,120,35,4,WHITE);else display.drawRoundRect(4,y,120,35,4,WHITE);
        uint16_t ink=active?BLACK:WHITE;NativeUI::icon(display,row==0?2:4,10,y+9,ink);
        NativeUI::text(display,names[row],33,y+7,84,ink);NativeUI::text(display,details[row],33,y+21,84,ink);
      }
      NativeUI::footer(display,"CHOOSE","OPEN","CHOOSE");display.display();
    }
    delay(20);
  }
}

// ---------- Crossy Walk game ----------

void drawCar(int x, int y, bool truck) {
  if (truck) {
    display.fillRect(x, y, 18, 6, WHITE);
    display.fillRect(x + 3, y - 3, 8, 3, WHITE);
  } else {
    display.fillRect(x, y, 14, 5, WHITE);
    display.drawPixel(x + 3, y + 5, WHITE);
    display.drawPixel(x + 11, y + 5, WHITE);
  }
}

bool hitCar(int petX, int petY, int carX, int laneY, int carW) {
  if (petY + 5 < laneY) return false;
  if (petY > laneY + 7) return false;
  if (petX + 5 < carX) return false;
  if (petX > carX + carW) return false;
  return true;
}

byte walkHash(unsigned int n) {
  unsigned long x = n;
  x ^= x << 7;
  x ^= x >> 9;
  x = x * 37UL + 17UL;
  return (byte)(x & 255);
}

// 0 = grass/safe, 1 = road, 2 = rest island.
// This creates mixed chunks instead of one repeated map:
// sometimes long road groups, sometimes broken-up roads with safe rows.
byte walkTerrain(unsigned int row) {
  if (row < 2) return 0;       // safe starting grass

  unsigned int block = row / 6;
  byte pos = row % 6;
  byte pat = walkHash(block) % 7;

  // Every chunk starts safe so there is always breathing room eventually.
  if (pos == 0) return 2;

  if (pat == 0) {              // broken road
    if (pos == 1 || pos == 3 || pos == 4) return 1;
    return 0;
  }
  if (pat == 1) {              // 3-road chunk
    if (pos >= 1 && pos <= 3) return 1;
    return 0;
  }
  if (pat == 2) {              // long 4-road chunk
    if (pos >= 1 && pos <= 4) return 1;
    return 0;
  }
  if (pat == 3) {              // scattered roads
    if (pos == 2 || pos == 4 || pos == 5) return 1;
    return 0;
  }
  if (pat == 4) {              // two roads, rest, road
    if (pos == 1 || pos == 2 || pos == 5) return 1;
    return 2;
  }
  if (pat == 5) {              // big scary road section
    if (pos >= 1 && pos <= 5) return 1;
    return 0;
  }

  // calmer chunk
  if (pos == 1 || pos == 4) return 1;
  return 0;
}

int walkCarX(unsigned int row, unsigned long now, byte level, byte lane) {
  byte h = walkHash(row * 13 + lane * 31);
  int gap = 132 - (level * 4);
  if (gap < 48) gap = 48;

  unsigned long t = now / gap;
  byte spd = 1 + (h % 3) + (level / 7);
  int travel = (int)((t * spd + h * 5 + lane * 57) % 178);

  if (h & 1) return 132 - travel;
  return travel - 30;
}

void drawWalkRow(unsigned int row, int y, byte level, unsigned long now) {
  if (y < 12 || y > 62) return;

  byte t = walkTerrain(row);
  byte h = walkHash(row);

  if (t == 1) {
    // Road
    display.drawLine(0, y, 127, y, WHITE);
    display.drawLine(0, y + 7, 127, y + 7, WHITE);

    if ((h & 3) == 0) {
      for (byte x = 5; x < 128; x += 18) display.drawLine(x, y + 4, x + 8, y + 4, WHITE);
    }

    bool truck = h & 2;
    int c1 = walkCarX(row, now, level, 0);
    drawCar(c1, y + 2, truck);

    // Later levels sometimes add a second vehicle, but not always.
    if (level > 3 && (h & 4)) {
      int c2 = c1 + 82;
      if (c2 > 145) c2 -= 185;
      drawCar(c2, y + 2, !truck);
    }
  } else if (t == 2) {
    // Rest island
    display.fillRect(0, y + 3, 128, 2, WHITE);
    if ((row % 12) == 0) {
      display.drawPixel(20, y + 1, WHITE);
      display.drawPixel(70, y + 6, WHITE);
      display.drawPixel(108, y + 1, WHITE);
    }
  } else {
    // Grass/safe area
    if (h & 1) display.drawPixel(12, y + 3, WHITE);
    if (h & 2) display.drawPixel(43, y + 5, WHITE);
    if (h & 4) display.drawPixel(88, y + 2, WHITE);
    if (h & 8) display.drawPixel(118, y + 6, WHITE);
  }
}

bool walkCollision(unsigned int row, int px, int py, byte level, unsigned long now) {
  if (walkTerrain(row) != 1) return false;

  byte h = walkHash(row);
  bool truck = h & 2;

  int c1 = walkCarX(row, now, level, 0);
  if (hitCar(px, py, c1, py + 2, truck ? 18 : 14)) return true;

  if (level > 3 && (h & 4)) {
    int c2 = c1 + 82;
    if (c2 > 145) c2 -= 185;
    if (hitCar(px, py, c2, py + 2, truck ? 14 : 18)) return true;
  }

  return false;
}

void walkGame() {
  NativeUI::GameScope gameCanvas(display,"LEFT","HOP","RIGHT");
  clickSound();
  if (energy < 2) {
    popup(F("Too sleepy"), F("Rest first"));
    return;
  }

  waitButtonRelease();

  const byte PLAYER_Y = 45;
  const byte ROW_H = 8;

  int px = 61;
  unsigned int playerRow = 0;
  unsigned int bestRow = 0;
  byte hits = 0;
  unsigned int score = 0;
  byte level = 1;
  unsigned long lastFrame = 0;
  unsigned long graceUntil = millis() + 1400UL;
  bool quit = false;

  // v16.3: edge-triggered buttons. Holding a button no longer repeats movement.
  bool oldLeft = HIGH;
  bool oldSel = HIGH;
  bool oldRight = HIGH;

  while (hits < 3 && !quit) {
    if (gameBackCheck()) return;
    unsigned long now = millis();

    byte frameGap = 78;
    if (level > 4) frameGap = 68;
    if (level > 9) frameGap = 58;
    if (level > 15) frameGap = 50;

    if (now - lastFrame < frameGap) {
      delay(2);
      continue;
    }
    lastFrame = now;

    byte currentTerrain = walkTerrain(playerRow);
    bool currentSafe = currentTerrain != 1;

    bool l = digitalRead(BTN_L);
    bool s = digitalRead(BTN_S);
    bool r = digitalRead(BTN_R);

    bool leftPress = (oldLeft == HIGH && l == LOW);
    bool selectPress = (oldSel == HIGH && s == LOW);
    bool rightPress = (oldRight == HIGH && r == LOW);

    if (leftPress && px > 2) {
      px -= 8;
      if (px < 2) px = 2;
      stepSound();
    }

    if (rightPress && px < 119) {
      px += 8;
      if (px > 119) px = 119;
      stepSound();
    }

    if (selectPress) {
      // One press equals one forward move. Holding will not repeat.
      playerRow++;

      if (playerRow > bestRow) {
        bestRow = playerRow;
        level = 1 + bestRow / 14;
        score += 1 + (level / 3);
        stepSound();

        if (bestRow % 12 == 0) addStat(happy, 1);
        if (bestRow % 24 == 0) addStat(health, 1);
      }
    }

    oldLeft = l;
    oldSel = s;
    oldRight = r;

    currentTerrain = walkTerrain(playerRow);
    currentSafe = currentTerrain != 1;

    if (!currentSafe && now > graceUntil) {
      if (walkCollision(playerRow, px, PLAYER_Y, level, now)) {
        hits++;
        badSound();
        graceUntil = millis() + 1500UL;

        // Knock back to the last safe row below you, not instant death.
        while (playerRow > 0 && walkTerrain(playerRow) == 1) {
          playerRow--;
        }

        if (px < 10) px = 10;
        if (px > 110) px = 110;

        // v26: use a border flash instead of full-screen inversion.
        display.drawRect(0, 0, OLED_W, LEGACY_OLED_H, WHITE);
        display.drawRect(2, 2, OLED_W - 4, LEGACY_OLED_H - 4, WHITE);
        display.display();
        delay(120);
        display.clearDisplay();
        display.display();
        delay(80);
        waitButtonRelease();

        oldLeft = HIGH;
        oldSel = HIGH;
        oldRight = HIGH;
      }
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print(F("Walk "));
    display.print(score);
    display.setCursor(65, 0);
    display.print(F("Hit "));
    display.print(hits);
    display.print(F("/3"));

    // Draw scrolling terrain rows.
    for (int screenRow = -1; screenRow < 7; screenRow++) {
      int rowY = PLAYER_Y + (screenRow * ROW_H);
      int worldRow = (int)playerRow - screenRow;
      if (worldRow < 0) continue;

      byte terrain = walkTerrain((unsigned int)worldRow);

      if (terrain == 0) {
        // grass/safe
        for (byte gx = 0; gx < 128; gx += 16) {
          display.drawPixel(gx + ((worldRow * 5) % 13), rowY + 4, WHITE);
        }
      } else if (terrain == 1) {
        // road
        display.drawLine(0, rowY + 1, 127, rowY + 1, WHITE);
        display.drawLine(0, rowY + ROW_H - 1, 127, rowY + ROW_H - 1, WHITE);

        int carX = walkCarX((unsigned int)worldRow, now, level, 0);
        display.fillRoundRect(carX, rowY + 2, 15, 5, 2, WHITE);
        int carX2 = walkCarX((unsigned int)worldRow + 7, now + 400, level + 1, 1);
        display.fillRoundRect(carX2, rowY + 2, 12, 5, 2, WHITE);
      } else {
        // rest island
        display.drawRoundRect(2, rowY + 1, 124, ROW_H - 1, 3, WHITE);
        display.setCursor(54, rowY + 1);
        display.print(F("REST"));
      }
    }

    // Current pet/player
    drawCurrentPetTiny(px - 2, PLAYER_Y - 3);

    display.setCursor(0, 55);
    if (currentSafe) display.print(F("D1 up Hold D2 exit"));
    else display.print(F("D0/D2 side D1 up"));

    display.display();
  }

  completeGameplayAction(M_WALK);
  addStat(happy, score > 8 ? 8 : score);
  addStat(health, score > 18 ? 2 : 0);
  if (score > 25 || random(0, 2) == 0) subStat(energy, 1);
  clampAll();

  showGameResult(score, 10);
  waitButtonRelease();
}



// ---------- discipline training ----------

void trainGame() {
  NativeUI::GameScope gameCanvas(display,"LEFT","TRAIN","RIGHT");
  clickSound();
  if (sleeping) {
    popup(F("Sleeping..."), F("Wake first"));
    return;
  }

  waitButtonRelease();

  byte score = 0;
  byte roundNo = 0;

  while (roundNo < 6) {
    int marker = random(5, 123);
    int markerDir = random(0, 2) ? 1 : -1;
    byte markerSpeed = 2 + random(0, 3) + (roundNo / 3);

    int boxX = random(20, 96);
    byte boxW = random(10, 17);       // smaller than before
    bool movingBox = random(0, 3) == 0 || roundNo >= 3;
    int boxDir = random(0, 2) ? 1 : -1;

    bool doneRound = false;
    bool hit = false;
    unsigned long lastFrame = 0;

    while (!doneRound) {
      if (gameBackCheck()) return;
      unsigned long now = millis();
      if (now - lastFrame < 38) {
        delay(2);
        continue;
      }
      lastFrame = now;

      marker += markerDir * markerSpeed;
      if (marker > 122) { marker = 122; markerDir = -1; }
      if (marker < 5) { marker = 5; markerDir = 1; }

      if (movingBox) {
        boxX += boxDir;
        if (boxX < 12) { boxX = 12; boxDir = 1; }
        if (boxX + boxW > 116) { boxX = 116 - boxW; boxDir = -1; }
      }

      if (pressed(BTN_S)) {
        hit = marker >= boxX && marker <= boxX + boxW;
        if (hit) { score++; goodSound(); }
        else badSound();
        doneRound = true;
        waitButtonRelease();
      }

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(2, 0);
      display.print(F("Train focus"));
      display.setCursor(84, 0);
      display.print(score); display.print(F("/")); display.print(roundNo + 1);
      display.setCursor(2, 10);
      if (movingBox) display.print(F("Moving target!"));
      else display.print(F("Stop in tiny box"));

      // Variable target zone
      display.drawRoundRect(boxX, 29, boxW, 11, 2, WHITE);
      if (movingBox && (anim & 1)) display.drawPixel(boxX + boxW / 2, 27, WHITE);

      // Track and moving marker
      display.drawLine(5, 48, 123, 48, WHITE);
      display.fillTriangle(marker, 44, marker - 3, 52, marker + 3, 52, WHITE);

      // Current pet is the coach/practice character.
      drawCurrentPetGameBadge(5, 31);
      display.drawLine(24, 38, 35, 33 + (anim & 1), WHITE);

      display.display();
    }

    display.clearDisplay();
    display.drawRoundRect(23, 20, 82, 26, 5, WHITE);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(36, 29);
    if (hit) display.print(F("Great focus!"));
    else display.print(F("Try again"));
    display.display();
    delay(420);

    roundNo++;
  }

  completeGameplayAction(M_TRAIN);
  if (score > 0 && naturalActivity() == M_TRAIN) addSkill(skillFit, 1);
  trainStars += score;
  addSkill(skillFit, score);

  if (score >= 5) {
    addStat(discipline, 4);
    addStat(happy, 2);
    popup(F("Perfect focus"), F("Disc big up"));
  } else if (score >= 3) {
    addStat(discipline, 2);
    addStat(happy, 1);
    popup(F("Good training"), F("Discipline up"));
  } else if (score >= 1) {
    addStat(discipline, 1);
    popup(F("Some practice"), F("Keep trying"));
  } else {
    subStat(happy, 1);
    popup(F("Hard lesson"), F("Try later"));
  }

  // Highly disciplined pets stay cleaner and happier over time.
  if (discipline >= 8 && poop > 0 && random(0, 4) == 0) { poop--; addStat(clean, 1); }

  subStat(energy, 1);
  clampAll();
  waitButtonRelease();
}



// ---------- extra activities ----------

void studyGame() {
  NativeUI::GameScope gameCanvas(display,"LESS","ANSWER","MORE");
  clickSound();
  if (sleeping) {
    popup(F("Sleeping..."), F("Wake first"));
    return;
  }
  waitButtonRelease();
  byte correct = 0;
  for (byte q = 0; q < 5; q++) {
    byte a = random(0, 5);
    byte b = random(0, 5);
    byte ans = a + b;
    byte guess = 0;
    bool done = false;
    bool oldLeft = HIGH;
    bool oldSel = HIGH;
    bool oldRight = HIGH;
    while (!done) {
      if (gameBackCheck()) return;
      bool l = digitalRead(BTN_L);
      bool s = digitalRead(BTN_S);
      bool r = digitalRead(BTN_R);
      if (oldLeft == HIGH && l == LOW) { guess = (guess == 0) ? 9 : guess - 1; delay(115); }
      if (oldRight == HIGH && r == LOW) { guess++; if (guess > 9) guess = 0; delay(115); }
      if (oldSel == HIGH && s == LOW) { done = true; waitButtonRelease(); }
      oldLeft = l; oldSel = s; oldRight = r;

      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(10, 0);
      display.print(F("Study time"));
      display.setCursor(82, 0);
      display.print(correct); display.print(F("/")); display.print(q + 1);
      drawCurrentPetGameBadge(3, 18);
      display.drawRoundRect(28, 16, 90, 20, 5, WHITE);
      display.setCursor(43, 23);
      display.print(a); display.print(F(" + ")); display.print(b); display.print(F(" = ?"));
      display.drawRoundRect(44, 40, 40, 18, 4, WHITE);
      display.setTextSize(2);
      display.setCursor(58, 42);
      display.print(guess);
      display.display();
      delay(20);
    }
    if (guess == ans) {
      correct++;
      goodSound();
      display.clearDisplay();
      display.drawRoundRect(25, 22, 78, 22, 5, WHITE);
      display.setTextSize(1); display.setTextColor(WHITE); display.setCursor(42, 31);
      display.print(F("Correct!"));
      display.display(); delay(450);
    } else {
      badSound();
      display.clearDisplay();
      display.drawRoundRect(20, 22, 88, 22, 5, WHITE);
      display.setTextSize(1); display.setTextColor(WHITE); display.setCursor(32, 31);
      display.print(F("Ans was ")); display.print(ans);
      display.display(); delay(600);
    }
  }
  if (correct < 5 && parentDialogue.schoolConfidence() >= 70 &&
      random(0, 100) < static_cast<long>(parentDialogue.schoolConfidence() - 55)) {
    correct++;
    popup(F("Used our study plan"), F("Calm focus bonus +1"), 650);
  }
  completeGameplayAction(M_STUDY);
  if (correct > 0 && lifeWorld.projectLevel(0) > 0) addSkill(skillSmart, lifeWorld.projectLevel(0));
  studyStars += correct;
  addSkill(skillSmart, correct);
  addCoins(correct);
  if (correct >= 4) { addStat(discipline, 3); addStat(happy, 2); }
  else if (correct >= 2) { addStat(discipline, 1); addStat(happy, 1); }
  else subStat(happy, 1);
  subStat(energy, 1);
  parentDialogue.recordSchoolResult(correct >= 3);
  clampAll();
  showGameResult(correct, 4);
}

void gardenActivity() {
  clickSound();
  if (sleeping) { popup(F("Sleeping..."), F("Wake first")); return; }
  waitButtonRelease();

  if (gardenPlant == 255) {
    byte pick = 0; bool oldLeft = HIGH, oldSel = HIGH, oldRight = HIGH;
    while (true) {
      if (rightHoldExitCheck()) return;
      bool l = digitalRead(BTN_L), s = digitalRead(BTN_S), r = digitalRead(BTN_R);
      if (oldLeft == HIGH && l == LOW) { pick = (pick == 0) ? PLANT_COUNT - 1 : pick - 1; clickSound(); delay(120); }
      if (oldRight == HIGH && r == LOW) { pick++; if (pick >= PLANT_COUNT) pick = 0; clickSound(); delay(120); }
      if (oldSel == HIGH && s == LOW) break;
      oldLeft = l; oldSel = s; oldRight = r;

      display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
      display.setCursor(25, 0); display.print(F("Seed catalog")); drawRightScore(pick + 1, PLANT_COUNT);
      display.drawRoundRect(4, 13, 120, 39, 5, WHITE);
      drawPlantSprite(pick, 27, 47, 4, false);
      display.setCursor(48, 20); display.print(plantName(pick));
      display.setCursor(48, 30); display.print(plantBenefitText(pick));
      display.setCursor(48, 40); display.print(F("Grow ")); display.print(gardenGrowTime(pick) * (LIFE_TICK_MS / 60000UL)); display.print(F("m"));
      display.setCursor(0, 55); display.print(F("D0/D2 pick  D1 plant"));
      display.display(); delay(20);
    }
    waitButtonRelease();
    gardenPlant = pick; gardenStartTick = ageTicks; gardenGrowTicks = adjustedGardenGrowTime(gardenGrowTime(pick)); discoveredPlants |= (1U << pick);
    gardenWater = 0; gardenSun = 0; gardenCare = 0; gardenQuality = 0; gardenProblem = random(0, 5);
    subStat(energy, 1); goodSound(); popup(F("Seed planted"), plantName(pick), 700); clampAll(); markSaveDirty(); return;
  }

  byte prog = gardenProgress();
  bool wilted = (gardenProblem == 4 && gardenCare == 0 && prog >= gardenGrowTicks / 2) || (prog > 2 && gardenWater == 0);

  if (gardenReady()) {
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(17, 0); display.print(wilted ? F("Rescue harvest") : F("Harvest ready!"));
    drawPlantSprite(gardenPlant, 38, 53, 4, wilted);
    display.setCursor(65, 18); display.print(plantName(gardenPlant));
    display.setCursor(65, 28); display.print(F("Quality ")); display.print(gardenQuality + gardenWater + gardenSun + gardenCare);
    display.setCursor(65, 38); display.print(F("D1 collect"));
    display.setCursor(0, 56); display.print(F("Hold D2 exit")); display.display();
    while (digitalRead(BTN_S) == HIGH) { if (rightHoldExitCheck()) return; delay(20); }
    waitButtonRelease();

    byte q = gardenQuality + (gardenWater > 0) + (gardenSun > 0) + (gardenCare > 0);
    if (wilted && q > 0) q--;
    byte yield = q >= 6 ? 3 : (q >= 3 ? 2 : 1);
    byte ingredient = plantIngredientType(gardenPlant);
    if (ingredient == 0) berryInv += yield;
    else if (ingredient == 1) herbInv += yield;
    else if (ingredient == 2) vegInv += yield;
    else if (ingredient == 3) spiceInv += yield;
    else { berryInv++; herbInv++; addSkill(skillStyle, 1); }

    if (gardenPlant == 5) spiceInv++;
    else if (gardenPlant == 6) addCoins(1 + yield);
    else if (gardenPlant == 7) { addSkill(skillStyle, 2); changeBond(1); }
    else if (gardenPlant == 8) { addStat(clean, 2); addStat(happy, 1); }
    else if (gardenPlant == 9) { addStat(health, 2); vegInv++; }
    else if (gardenPlant == 10) { addCoins(3); spiceInv++; }
    else if (gardenPlant == 11) { berryInv++; herbInv++; vegInv++; spiceInv++; changeBond(2); }

    gardenHarvests++; gardenLevel++; addSkill(skillKind, 1); addStat(happy, 1);
    if (habitat == 1 || habitat == 5) { addStat(clean, 1); addMaterial(4, 1); }
    if (habitat == 2) addMaterial(5, 1);
    updateDevelopedHobby();
    if (q >= 5) { addStat(clean, 1); addStat(health, 1); }
    gardenPlant = 255; winSound(); popup(F("Harvested!"), F("New pantry items"), 750); clampAll(); markSaveDirty(); return;
  }

  bool oldLeft = HIGH, oldSel = HIGH, oldRight = HIGH;
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(BTN_L), s = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (oldLeft == HIGH && l == LOW) { if (gardenWater < 5) gardenWater++; if (gardenQuality < 250) gardenQuality++; clickSound(); delay(120); }
    if (oldRight == HIGH && r == LOW) { if (gardenSun < 5) gardenSun++; if (gardenQuality < 250) gardenQuality++; clickSound(); delay(120); }
    if (oldSel == HIGH && s == LOW) { if (gardenCare < 3) { gardenCare++; gardenQuality = min((byte)250, (byte)(gardenQuality + 2)); goodSound(); } else popup(F("Already tended"), F("Come back later")); delay(120); }
    oldLeft = l; oldSel = s; oldRight = r;

    prog = gardenProgress(); wilted = (gardenProblem == 4 && gardenCare == 0 && prog >= gardenGrowTicks / 2) || (prog > 2 && gardenWater == 0);
    byte vstage = plantVisualStage(gardenPlant, prog, gardenGrowTicks);
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(0, 0); display.print(plantName(gardenPlant)); display.print(F(" ")); display.print(prog); display.print(F("/")); display.print(gardenGrowTicks);
    display.setCursor(82, 0); display.print(F("W")); display.print(gardenWater); display.print(F(" S")); display.print(gardenSun);
    display.drawRoundRect(4, 12, 82, 40, 5, WHITE);
    drawPlantSprite(gardenPlant, 45, 49, vstage, wilted);
    drawPlantProblemIcon(gardenProblem, 70, 18);
    drawCurrentPetGameBadge(101, 25);
    display.setCursor(90, 46); display.print(F("Care ")); display.print(gardenCare);
    display.setCursor(0, 55); display.print(F("D0 water D1 tend D2 sun"));
    display.display(); delay(20);
  }
}




void musicGame() {
  NativeUI::GameScope gameCanvas(display,"LEFT","HIT","RIGHT");
  clickSound();
  if (sleeping) {
    popup(F("Sleeping..."), F("Wake first"));
    return;
  }

  waitButtonRelease();

  byte score = 0;
  byte misses = 0;
  byte combo = 0;
  byte lane = random(0, 3);
  int noteX = 124;
  byte speed = random(2, 5);
  unsigned long start = millis();
  unsigned long lastFrame = 0;
  bool oldLeft = HIGH;
  bool oldSel = HIGH;
  bool oldRight = HIGH;

  while (millis() - start < 16000UL && misses < 6) {
    if (gameBackCheck()) return;
    unsigned long now = millis();
    if (now - lastFrame < 42) {
      delay(2);
      continue;
    }
    lastFrame = now;

    noteX -= speed;

    bool l = digitalRead(BTN_L);
    bool s = digitalRead(BTN_S);
    bool r = digitalRead(BTN_R);
    byte pressedLane = 255;

    if (oldLeft == HIGH && l == LOW) pressedLane = 0;
    if (oldSel == HIGH && s == LOW) pressedLane = 1;
    if (oldRight == HIGH && r == LOW) pressedLane = 2;

    oldLeft = l;
    oldSel = s;
    oldRight = r;

    if (pressedLane != 255) {
      bool timing = noteX >= 18 && noteX <= 36;
      if (timing && pressedLane == lane) {
        score++;
        petTone(900 + lane * 250, 35);
        combo++;
        if (combo >= 4) { score++; combo = 0; }
      } else {
        misses++;
        petTone(220, 45);
        combo = 0;
        if (happy > 0) subStat(happy, 1);
      }

      noteX = 124;
      lane = (lane + random(1, 3) + score + misses) % 3;
      speed = random(2, 5) + (score / 7);
      if (speed > 6) speed = 6;
      waitButtonRelease();
    }

    if (noteX < 2) {
      misses++;
      combo = 0;
      noteX = 124;
      lane = random(0, 3);
      speed = random(2, 5) + (score / 8);
      if (speed > 6) speed = 6;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print(F("Music lanes"));
    display.setCursor(82, 0);
    display.print(score); display.print(F("/")); display.print(misses);

    display.drawRoundRect(17, 17, 20, 35, 4, WHITE);
    display.setCursor(1, 55); display.print(F("D0"));
    display.setCursor(56, 55); display.print(F("D1"));
    display.setCursor(110, 55); display.print(F("D2"));

    for (byte i = 0; i < 3; i++) {
      int y = 22 + i * 10;
      display.drawLine(10, y, 118, y, WHITE);
      if (i == 0) display.setCursor(2, y - 3), display.print(F("L"));
      if (i == 1) display.setCursor(2, y - 3), display.print(F("M"));
      if (i == 2) display.setCursor(2, y - 3), display.print(F("R"));
    }

    int ny = 22 + lane * 10;
    if ((score + misses + lane) % 3 == 0) {
      display.fillCircle(noteX, ny, 4, WHITE);
    } else if ((score + lane) % 3 == 1) {
      display.drawCircle(noteX, ny, 5, WHITE);
      display.drawLine(noteX + 5, ny, noteX + 5, ny - 10, WHITE);
    } else {
      display.fillTriangle(noteX - 4, ny + 4, noteX + 4, ny + 4, noteX, ny - 5, WHITE);
    }

    // Current creature performs in the music game.
    drawCurrentPetGameBadge(82, 37);
    if (anim & 1) {
      display.drawLine(80, 42, 74, 37, WHITE);
      display.drawLine(100, 42, 108, 48, WHITE);
    } else {
      display.drawLine(80, 42, 74, 48, WHITE);
      display.drawLine(100, 42, 108, 37, WHITE);
    }
    display.drawPixel(108, 20, WHITE);
    display.drawLine(109, 20, 109, 12, WHITE);
    display.fillCircle(106, 22, 2, WHITE);

    if (combo > 0) {
      display.setCursor(44, 10);
      display.print(F("Combo ")); display.print(combo);
    }

    display.display();
  }

  completeGameplayAction(M_MUSIC);
  if (score > 0 && naturalActivity() == M_MUSIC) addSkill(skillStyle, 1);
  musicStars += score;
  addSkill(skillStyle, score);
  addStat(happy, score > 7 ? 7 : score);
  if (score >= 8) addStat(discipline, 2);
  else if (score >= 4) addStat(discipline, 1);
  if (score > 12 || random(0, 3) == 0) subStat(energy, 1);
  clampAll();
  showGameResult(score, 8);
  waitButtonRelease();
}



// ---------- v7 fan-suggestion activities ----------

void drawShopItem(byte id, int x, int y, bool sel) {
  if (sel) display.fillRoundRect(x, y, 34, 24, 4, WHITE);
  else display.drawRoundRect(x, y, 34, 24, 4, WHITE);
  const uint16_t col = sel ? BLACK : WHITE;
  id %= OUTFIT_COUNT;
  if (id == 0) {
    display.drawLine(x + 10, y + 7, x + 24, y + 19, col);
    display.drawLine(x + 24, y + 7, x + 10, y + 19, col);
  } else drawFancyHat(id, x + 17, y + 18, col);
}


byte outfitCost(byte id) {
  if (id == 0) return 0;
  if (id <= 3) return 3 + id;
  if (id <= 6) return 5 + id;
  if (id <= 9) return 7 + id;
  if (id <= 12) return 9 + id;
  return 12 + id;
}


bool outfitSkillOK(byte id) {
  if (id <= 3) return true;
  if (id <= 6) return skillStyle >= 3 || skillFit >= 3;
  if (id <= 9) return skillStyle >= 5 && (skillKind >= 3 || skillFit >= 3);
  if (id <= 12) return skillStyle >= 7 && skillSmart >= 5;
  return jobRank >= 2 || (skillStyle >= 9 && ownerBond >= 55);
}


void shopClothes() {
  clickSound(); waitButtonRelease();
  byte pick = outfit % OUTFIT_COUNT;
  bool ol = HIGH, os = HIGH, orr = HIGH;
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(BTN_L), s = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (ol == HIGH && l == LOW) { pick = (pick == 0) ? OUTFIT_COUNT - 1 : pick - 1; clickSound(); delay(120); }
    if (orr == HIGH && r == LOW) { pick++; if (pick >= OUTFIT_COUNT) pick = 0; clickSound(); delay(120); }
    if (os == HIGH && s == LOW) {
      if (!outfitSkillOK(pick)) { badSound(); popup(F("Need more skill"), outfitRequirementText(pick), 750); }
      else if (!(outfitOwned & (1UL << pick))) {
        byte cost = outfitCost(pick);
        const byte saleDay = (byte)(petAgeDays() % 30UL) + 1;
        if (saleDay == 10 || saleDay == 20 || saleDay == 30) cost = max((byte)1, (byte)((cost + 1) / 2));
        if (!spendCoins(cost)) { badSound(); popup(F("Need coins"), F("Work or explore"), 750); }
        else { outfitOwned |= (1UL << pick); outfit = pick; completeGameplayAction(M_SHOP); goodSound(); addSkill(skillStyle, 1); changeBond(1); popup(F("Hat purchased!"), outfitName(pick), 800); break; }
      } else { if (outfit != pick) completeGameplayAction(M_SHOP); outfit = pick; goodSound(); popup(F("Hat equipped"), outfitName(pick), 700); break; }
      waitButtonRelease();
    }
    ol = l; os = s; orr = r;

    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(0, 0); display.print(F("Fancy hats $")); display.print(coins); drawRightScore(pick + 1, OUTFIT_COUNT);
    drawShopItem((pick + OUTFIT_COUNT - 1) % OUTFIT_COUNT, 2, 18, false);
    drawShopItem(pick, 47, 18, true);
    drawShopItem((pick + 1) % OUTFIT_COUNT, 92, 18, false);
    display.setCursor(0, 45); display.print(outfitName(pick));
    display.setCursor(0, 54);
    if (outfitOwned & (1UL << pick)) display.print(F("Owned: D1 wear"));
    else {
      byte shownCost = outfitCost(pick);
      const byte saleDay = (byte)(petAgeDays() % 30UL) + 1;
      const bool sale = saleDay == 10 || saleDay == 20 || saleDay == 30;
      if (sale) shownCost = max((byte)1, (byte)((shownCost + 1) / 2));
      display.print(F("$")); display.print(shownCost); if (sale) display.print(F(" SALE ")); else display.print(F(" ")); display.print(outfitRequirementText(pick));
    }
    display.display(); delay(20);
  }
  markSaveDirty(); waitButtonRelease();
}


void toyActivity() {
  clickSound();
  if (sleeping) { popup(F("Sleeping..."), F("Wake first")); return; }
  waitButtonRelease();
  byte pick = toyItem;
  bool ol = HIGH, os = HIGH, orr = HIGH;
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(BTN_L), s = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (ol == HIGH && l == LOW) { pick = (pick == 0) ? 4 : pick - 1; clickSound(); delay(120); }
    if (orr == HIGH && r == LOW) { pick++; if (pick > 4) pick = 0; clickSound(); delay(120); }
    if (os == HIGH && s == LOW) { toyItem = pick; break; }
    ol = l; os = s; orr = r;
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(25, 0); display.print(F("Toy box"));
    for (byte i=0;i<5;i++) {
      int x=5+i*24; if (i==pick) display.fillRoundRect(x,20,20,22,4,WHITE); else display.drawRoundRect(x,20,20,22,4,WHITE);
      uint16_t col=(i==pick)?BLACK:WHITE;
      if (i==0) display.drawCircle(x+10,31,6,col);
      else if (i==1) { display.drawLine(x+8,26,x+8,36,col); display.fillCircle(x+7,37,2,col); }
      else if (i==2) { display.drawRect(x+5,25,10,13,col); display.drawLine(x+10,25,x+10,38,col); }
      else if (i==3) { display.drawTriangle(x+5,36,x+15,36,x+10,25,col); display.drawPixel(x+10,30,col); }
      else { display.drawCircle(x+10,28,4,col); display.drawLine(x+10,32,x+5,39,col); display.drawLine(x+10,32,x+15,39,col); }
    }
    display.setCursor(0,52);
    if (pick==0) display.print(F("Ball: fit + mood"));
    else if (pick==1) display.print(F("Flute: style + mood"));
    else if (pick==2) display.print(F("Book: smart + calm"));
    else if (pick==3) display.print(F("Kite: fit + kind"));
    else display.print(F("Doll: kind + mood"));
    display.display(); delay(20);
  }
  for (byte i=0;i<10;i++) {
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(30,0); display.print(F("Playing!"));
    drawPet();
    int x=20+(i*9)%85; int y=30+((i+pick)%4)*4;
    if (pick==0) display.drawCircle(x,y,4,WHITE);
    else if (pick==1) { display.drawLine(x,y-6,x,y+8,WHITE); display.fillCircle(x-2,y+8,2,WHITE); petTone(900+i*40, 25); }
    else if (pick==2) display.drawRect(x,y,12,9,WHITE);
    else if (pick==3) display.drawTriangle(x,y-8,x-7,y+3,x+7,y+3,WHITE);
    else { display.drawCircle(x,y,4,WHITE); display.drawLine(x,y+4,x,y+10,WHITE); }
    display.display(); delay(95);
  }
  if (!completeGameplayAction(M_TOY)) { popup(F("Already played together"), F("Try another activity")); return; }
  if (pick==0) { addStat(happy,2); addSkill(skillFit,2); }
  else if (pick==1) { addStat(happy,3); addSkill(skillStyle,2); }
  else if (pick==2) { addStat(happy,1); addSkill(skillSmart,2); addStat(discipline,1); }
  else if (pick==3) { addStat(happy,2); addSkill(skillFit,1); addSkill(skillKind,1); }
  else { addStat(happy,3); addSkill(skillKind,2); }
  subStat(energy,1); clampAll(); goodSound(); waitButtonRelease();
}


const __FlashStringHelper* biomeName(byte loc) {
  switch (loc % 9) {
    case 0: return F("Park");
    case 1: return F("Tropical Beach");
    case 2: return F("Forest Trail");
    case 3: return F("Town Plaza");
    case 4: return F("Crystal Cave");
    case 5: return F("Night Market");
    case 6: return F("Glacier Field");
    case 7: return F("Rain Garden");
    default: return F("Star Garden");
  }
}

void drawBiomeScene(byte loc, byte frame) {
  loc %= 9;
  display.drawLine(0, 56, 127, 56, WHITE);
  if (loc == 0) { // park
    display.drawLine(18, 56, 18, 32, WHITE); display.drawCircle(10, 34, 8, WHITE); display.drawCircle(25, 31, 9, WHITE);
    display.drawRoundRect(91, 46, 25, 8, 3, WHITE); display.drawLine(96, 54, 96, 58, WHITE); display.drawLine(111, 54, 111, 58, WHITE);
  } else if (loc == 1) { // beach
    display.drawCircle(106, 19, 7, WHITE); display.drawLine(0, 42, 127, 42, WHITE);
    for (byte x = 0; x < 128; x += 18) drawTopArc(x + (frame % 3), 47, 8, 4, WHITE);
    display.drawLine(17, 56, 17, 31, WHITE); display.drawLine(17, 33, 7, 27, WHITE); display.drawLine(17, 35, 29, 29, WHITE);
  } else if (loc == 2) { // forest
    for (byte x = 6; x < 128; x += 27) { display.drawTriangle(x, 56, x + 10, 27 + (x % 6), x + 20, 56, WHITE); display.drawLine(x + 10, 45, x + 10, 56, WHITE); }
    display.drawRoundRect(75, 49, 15, 5, 2, WHITE);
  } else if (loc == 3) { // town
    display.drawRect(5, 31, 25, 25, WHITE); display.drawTriangle(2, 31, 17, 20, 33, 31, WHITE);
    display.drawRect(96, 27, 27, 29, WHITE); display.drawLine(101, 38, 118, 38, WHITE);
    display.drawLine(0, 49, 127, 49, WHITE);
  } else if (loc == 4) { // cave
    drawTopArc(64, 56, 54, 40, WHITE);
    display.drawTriangle(24, 20, 30, 34, 36, 20, WHITE); display.drawTriangle(87, 17, 94, 32, 101, 17, WHITE);
    display.drawPixel(52, 31, WHITE); display.drawPixel(78, 26, WHITE);
  } else if (loc == 5) { // market
    for (byte x = 3; x < 128; x += 31) { display.drawRect(x, 38, 25, 18, WHITE); display.drawTriangle(x, 38, x + 12, 28, x + 25, 38, WHITE); }
    for (byte x = 8; x < 125; x += 14) display.drawPixel(x, 20 + ((x / 14) & 1) * 3, WHITE);
  } else if (loc == 6) { // ice
    display.drawTriangle(5, 56, 23, 29, 41, 56, WHITE); display.drawTriangle(72, 56, 94, 24, 118, 56, WHITE);
    display.drawPixel(15, 17, WHITE); display.drawPixel(52, 29, WHITE); display.drawPixel(111, 15, WHITE);
    display.drawLine(3, 50, 124, 47, WHITE);
  } else if (loc == 7) { // rain garden
    for (byte x = 4; x < 128; x += 14) display.drawLine(x, 15 + (x % 5), x - 3, 23 + (x % 5), WHITE);
    display.drawCircle(18, 49, 5, WHITE); display.drawCircle(30, 47, 6, WHITE); display.drawCircle(103, 49, 6, WHITE);
    display.drawLine(65, 56, 65, 37, WHITE); display.drawCircle(65, 34, 7, WHITE);
  } else { // star garden
    display.drawCircle(106, 20, 8, WHITE); display.fillCircle(110, 18, 7, BLACK);
    for (byte i = 0; i < 10; i++) display.drawPixel(7 + i * 12, 16 + ((i * 7) % 20), WHITE);
    display.drawLine(18, 56, 18, 41, WHITE); display.drawCircle(18, 37, 5, WHITE);
    display.drawLine(108, 56, 108, 39, WHITE); display.drawCircle(108, 35, 5, WHITE);
  }
}

void exploreTrip() {
  clickSound();
  if (energy < 2) { popup(F("Too tired"), F("Rest first")); return; }
  waitButtonRelease();
  byte loc = (random(0, 3) == 0) ? (habitat == 1 ? 2 : habitat == 2 ? 1 : habitat == 3 ? 6 : habitat == 5 ? 7 : habitat == 6 ? 8 : random(0, 9)) : random(0, 9); byte event = random(0, 9);
  for (byte i = 0; i < 15; i++) {
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(0, 0); display.print(biomeName(loc));
    drawBiomeScene(loc, i);
    int px = 7 + i * 7; if (px > 105) px = 105;
    drawCurrentPetTiny(px, 39 + ((i & 1) ? 1 : 0));
    if (outfit > 0 && i % 3 == 0) display.drawPixel(px + 8, 36, WHITE);
    display.display(); delay(90);
  }

  if (event == 0) { berryInv += 2; popup(F("Found berries"), F("Fresh ingredients")); }
  else if (event == 1) { herbInv++; spiceInv++; popup(F("Found herbs"), F("A fragrant bundle")); }
  else if (event == 2) { vegInv += 2; popup(F("Foraged veg"), F("Healthy pantry")); }
  else if (event == 3) { friendCount++; addSkill(skillKind, 1); popup(F("Met a friend!"), F("Kindness up")); }
  else if (event == 4) { addCoins(3 + random(0, 5)); popup(F("Treasure found"), F("Coins collected")); }
  else if (event == 5) { tripBadges++; addStat(happy, 2); popup(F("Postcard saved"), biomeName(loc)); }
  else if (event == 6) { outfitOwned |= (1UL << (1 + (loc % (OUTFIT_COUNT - 1)))); popup(F("Hat souvenir"), F("A hat was unlocked")); }
  else { byte mat = (loc + event) % MATERIAL_COUNT; addMaterial(mat, 1 + (event == 8)); popup(F("Craft material"), materialName(mat)); }

  visitedLocations |= (1U << loc);
  if (gameCounts[loc % 13] + tripBadges >= 3) favoritePlace = loc;
  if (loc == 1) addSkill(skillStyle, 1);
  else if (loc == 2 || loc == 7) addSkill(skillKind, 1);
  else if (loc == 4 || loc == 8) addSkill(skillSmart, 1);
  else if (loc == 6) addSkill(skillFit, 1);
  addStat(happy, 1); subStat(energy, lifeWorld.projectLevel(1) >= 2 ? 1 : 2); subStat(full, 1);
  completeGameplayAction(M_EXPLORE);
  lastTripEvent = loc; logCareEvent(15); markRoutineAction(ROUTINE_PLAY); recordGoalEvent(5, 1); changeBond(1);
  clampAll(); goodSound(); markSaveDirty();
}


bool useIngredient(byte recipe) {
  if (recipe == 0 && berryInv >= 1) { berryInv--; return true; }
  if (recipe == 1 && herbInv >= 1 && vegInv >= 1) { herbInv--; vegInv--; return true; }
  if (recipe == 2 && vegInv >= 2) { vegInv -= 2; return true; }
  if (recipe == 3 && spiceInv >= 1 && berryInv >= 1) { spiceInv--; berryInv--; return true; }
  if (recipe == 4 && berryInv >= 1 && herbInv >= 1 && vegInv >= 1 && spiceInv >= 1) { berryInv--; herbInv--; vegInv--; spiceInv--; return true; }
  return false;
}


void cookActivity() {
  clickSound();
  waitButtonRelease();

  byte pick = 0;
  bool oldLeft = HIGH, oldSel = HIGH, oldRight = HIGH;

  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(BTN_L), s = digitalRead(BTN_S), r = digitalRead(BTN_R);

    if (oldLeft == HIGH && l == LOW) { pick = (pick == 0) ? 4 : pick - 1; clickSound(); delay(120); }
    if (oldRight == HIGH && r == LOW) { pick++; if (pick > 4) pick = 0; clickSound(); delay(120); }
    if (oldSel == HIGH && s == LOW) break;

    oldLeft = l; oldSel = s; oldRight = r;

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.print(F("Cook B")); display.print(berryInv);
    display.print(F(" H")); display.print(herbInv);
    display.print(F(" V")); display.print(vegInv);
    display.print(F(" S")); display.print(spiceInv);

    display.drawRoundRect(12, 17, 104, 30, 5, WHITE);
    display.setCursor(24, 27);
    display.print(mealName(pick));

    display.setCursor(0, 49);
    if (pick == 0) display.print(F("Need: berry"));
    else if (pick == 1) display.print(F("Need: herb+veg"));
    else if (pick == 2) display.print(F("Need: 2 veg"));
    else if (pick == 3) display.print(F("Need: spice+berry"));
    else display.print(F("Need: all four"));

    display.setCursor(0, 57);
    display.print(F("D1 cook -> pantry"));
    display.display();
    delay(20);
  }

  waitButtonRelease();
  if (!useIngredient(pick)) {
    badSound();
    popup(F("Missing items"), F("Grow/explore"));
    return;
  }

  for (byte i = 0; i < 8; i++) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(34, 0);
    display.print(F("Cooking"));
    display.drawRoundRect(46, 36, 34, 12, 5, WHITE);
    display.drawLine(50, 36, 48, 28, WHITE);
    display.drawLine(62, 36, 62, 26, WHITE);
    display.drawLine(76, 36, 78, 28, WHITE);
    display.fillCircle(52 + i * 3, 33 - (i % 3), 2, WHITE);
    display.display();
    petTone(700 + i * 60, 30);
    delay(120);
  }

  addMeal(pick);
  if (naturalActivity() == M_COOK) addSkill(skillKind, 1);
  completeGameplayAction(M_COOK);
  cookedMeals++;
  addSkill(skillStyle, 1);
  if (pick == 1) addSkill(skillKind, 1);       // soup is care-focused
  if (pick == 4) addSkill(skillSmart, 1);      // royal stew is advanced

  markRoutineAction(ROUTINE_CARE);
  recordGoalEvent(4, 1);
  changeBond(1);
  winSound();
  popup(F("Meal stored"), F("Eat in Food"));
}


void jobActivity() {
  clickSound();
  lifeWorld.workCareer();
}

void aquariumActivity() {
  if (sleeping) { popup(F("Sleeping..."), F("Wake first")); return; }
  aquarium.runMenu();
}

void roomActivity() {
  clickSound(); waitButtonRelease();
  byte pick = roomStyle % ROOM_STYLE_COUNT; bool ol = HIGH, os = HIGH, orr = HIGH;
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(BTN_L), s = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (ol == HIGH && l == LOW) { pick = (pick == 0) ? ROOM_STYLE_COUNT - 1 : pick - 1; clickSound(); delay(120); }
    if (orr == HIGH && r == LOW) { pick++; if (pick >= ROOM_STYLE_COUNT) pick = 0; clickSound(); delay(120); }
    if (os == HIGH && s == LOW) {
      if (pick == roomStyle) { popup(F("This is already home"), roomName(pick)); return; }
      byte cost = roomCost(pick);
      if (!(roomOwned & (1UL << pick)) && !spendCoins(cost)) { badSound(); popup(F("Need coins"), F("Work first")); waitButtonRelease(); }
      else { roomOwned |= (1UL << pick); roomStyle = pick; goodSound(); if (completeGameplayAction(M_ROOM)) { addStat(happy, 1); changeBond(1); } popup(F("Room changed"), roomName(pick), 750); break; }
    }
    ol = l; os = s; orr = r;
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(0, 0); display.print(F("Room themes $")); display.print(coins); drawRightScore(pick + 1, ROOM_STYLE_COUNT);
    display.drawRoundRect(4, 14, 120, 37, 5, WHITE);
    byte oldRoom = roomStyle; roomStyle = pick; drawRoomDecor(); roomStyle = oldRoom;
    display.fillRect(0, 0, 128, 13, BLACK); display.setCursor(14, 3); display.print(roomName(pick));
    display.setCursor(0, 55);
    if (roomOwned & (1UL << pick)) display.print(F("Owned D1 set"));
    else { display.print(F("D1 buy $")); display.print(roomCost(pick)); }
    display.display(); delay(20);
  }
  markSaveDirty(); waitButtonRelease();
}



byte speciesPersonalityType() {
  // Temperament is rolled independently at birth, so two pets with the same
  // adult species can still speak and behave differently.
  switch (temperament % 8) {
    case 0: return 0; // shy/calm
    case 1: return 8; // energetic/adventurous
    case 2: return 9; // curious/thoughtful
    case 3: return 3; // affectionate
    case 4: return 4; // mischievous
    case 5: return 5; // calm/nature
    case 6: return 7; // foodie
    default: return 1; // independent/cozy
  }
}

const __FlashStringHelper* personalityName() {
  return temperamentName();
}

// A high-bond pet gives the player a temperament-based nickname.
// It is derived instead of stored, so it never consumes extra save space.
const __FlashStringHelper* ownerNickname() {
  if (ownerBond < 70) return F("Friend");
  switch (temperament % 8) {
    case 0: return F("Quiet Star");
    case 1: return F("Coach");
    case 2: return F("Explorer");
    case 3: return F("Bestie");
    case 4: return F("Partner");
    case 5: return F("Safe Harbor");
    case 6: return F("Snack Hero");
    default: return F("Home Pal");
  }
}

const __FlashStringHelper* speciesSecretLine() {
  switch (speciesType % SPECIES_COUNT) {
    case 0: return F("I count clouds.");
    case 1: return F("I hide snacks.");
    case 2: return F("Shell hums.");
    case 3: return F("I like pranks.");
    case 4: return F("Hat is lucky.");
    case 5: return F("I smell cake.");
    case 6: return F("Roads call me.");
    case 7: return F("I chase dreams.");
    case 8: return F("Rain sings.");
    case 9: return F("Seeds talk.");
    case 10: return F("Hood hides songs.");
    case 11: return F("Hugs charge me.");
    case 12: return F("I race shadows.");
    case 13: return F("Snow whispers.");
    case 14: return F("Cozy is power.");
    case 15: return F("Small wings big.");
    case 16: return F("Bell knows time.");
    case 17: return F("Waves know me.");
    case 18: return F("I stack hopes.");
    case 19: return F("Ponds sparkle.");
    case 20: return F("Moon is home.");
    case 21: return F("Wings get itchy.");
    case 22: return F("Beans are wise.");
    case 23: return F("Clouds are soft.");
    case 24: return F("Tricks need art.");
    case 25: return F("Blob has plans.");
    case 26: return F("Nose finds fun.");
    case 27: return F("Hats choose me.");
    case 28: return F("Shirt is armor.");
    case 29: return F("Forest knows me.");
    case 30: return F("Flame is tiny.");
    case 31: return F("Potato power.");
    case 32: return F("Helmet brave.");
    case 33: return F("Bite careful.");
    case 34: return F("Stories are maps.");
    case 35: return F("Cheese moon real.");
    case 36: return F("Sea is huge.");
    case 37: return F("Picnics heal.");
    case 38: return F("Beep means love.");
    case 39: return F("Eight hugs please.");
    case 40: return F("Stars waved back.");
    case 41: return F("Round is best.");
    case 42: return F("Magic is quiet.");
    case 43: return F("Crumbs guide me.");
    case 44: return F("Boo means hi.");
    case 45: return F("Old trees listen.");
    case 46: return F("Cape feels fast.");
    case 47: return F("Roars are tiny.");
    case 48: return F("Stars grow slow.");
    case 49: return F("Chimes remember.");
    default: return F("I like you.");
  }
}

const __FlashStringHelper* petMoodTreeLine() {
  if (stage >= STAGE_CHILD && parentDialogue.openness() < 24) return F("I need time to open up.");
  if (stage >= STAGE_CHILD && parentDialogue.schoolConfidence() < 24 && visualHour() < 17) return F("School feels hard today.");
  if (stage >= STAGE_TEEN && parentDialogue.socialConfidence() < 24) return F("I am unsure around others.");
  if (parentDialogue.attachmentSecurity() >= 80 && ownerTrust >= 70) return F("I know you will listen.");
  if (parentDialogue.confidence() >= 85 && happy >= 6) return F("I think I can handle today!");
  if (full <= 2) return F("My tummy is empty.");
  if (clean <= 2 || poop > 0) return F("I feel messy.");
  if (energy <= 2) return F("Sleepy... zzz.");
  if (happy <= 2) return F("Stay with me?");
  if (sick || conditionType != 0) return F("I feel strange.");
  if (happy >= 8) return F("I feel sparkly!");
  switch (speciesPersonalityType()) {
    case 0: return F("Peaceful today.");
    case 1: return F("Cozy and safe.");
    case 2: return F("I have a song.");
    case 3: return F("Happy with you.");
    case 4: return F("Maybe prank?");
    case 5: return F("Fresh air please.");
    case 6: return F("Looking cute!");
    case 7: return F("Could eat more.");
    case 8: return F("Adventure soon?");
    default: return F("Thinking deeply.");
  }
}

const __FlashStringHelper* petJokeReply() {
  if (stage == STAGE_BABY) return F("Hee! Ba-ba!");
  switch (speciesPersonalityType()) {
    case 0: return F("Heh. Soft laugh.");
    case 1: return F("That was warm.");
    case 2: return F("Ba-dum chime!");
    case 3: return F("You are funny!");
    case 4: return F("I know worse.");
    case 5: return F("Trees laughed.");
    case 6: return F("Cute joke!");
    case 7: return F("Food pun next?");
    case 8: return F("Again while walking!");
    default: return F("Clever thought.");
  }
}

const __FlashStringHelper* petPraiseReply() {
  if (stage == STAGE_BABY) return F("Ba! Happy!");
  switch (speciesPersonalityType()) {
    case 0: return F("Thank you kindly.");
    case 1: return F("I feel cozy.");
    case 2: return F("My heart sings.");
    case 3: return F("Aww, hug?");
    case 4: return F("Obviously!");
    case 5: return F("I will bloom.");
    case 6: return F("I know, right?");
    case 7: return F("Reward snack?");
    case 8: return F("Now I feel brave!");
    default: return F("I will remember.");
  }
}

const __FlashStringHelper* petAdviceReply() {
  if (stage == STAGE_BABY) return F("Da-da. Gentle hands.");
  if (mischief > 2) return F("I will behave.");
  if (discipline <= 3) return F("Teach me again.");
  switch (speciesPersonalityType()) {
    case 0: return F("I will be patient.");
    case 1: return F("I will rest too.");
    case 2: return F("Practice rhythm.");
    case 3: return F("I will be kind.");
    case 4: return F("No prank. Maybe.");
    case 5: return F("Water the plants.");
    case 6: return F("I will stay neat.");
    case 7: return F("Eat slowly. Got it.");
    case 8: return F("Train and explore!");
    default: return F("I will study.");
  }
}

namespace {
const char *const PERSONAL_TALK_BABY_OPENERS[12] = {
  "Ba!", "Da-da!", "Mmm!", "Look!", "I know you!", "You came!",
  "Baba!", "Hee!", "My friend!", "There you are!", "I remember!", "Listen!"
};

const char *const PERSONAL_TALK_RELATIONSHIPS[3][8] = {
  {
    "I open up a little at a time.", "I am still learning what feels safe to share.",
    "I notice when you come back.", "I am getting to know you, too.",
    "Small, steady moments help me feel sure.", "I am not ready to tell you everything yet.",
    "I like when you give me time to find my words.", "I am learning our little rhythms."
  },
  {
    "I notice how you show up on ordinary days.", "I am learning what I can trust you with.",
    "Our routines are becoming a language between us.", "You give me room to be myself.",
    "I can feel our history in the little things.", "We are getting better at understanding each other.",
    "You have learned some of my little tells.", "I like that we can grow and still be ourselves."
  },
  {
    "I feel safe enough to be completely myself with you.", "I trust the care we have built.",
    "You know the little details that make me feel understood.", "I can bring you a hard feeling and still feel loved.",
    "Our history feels like a place I can come home to.", "You know my rhythms, and I know you will show up.",
    "I feel braver with you nearby.", "I feel like your Tamachibi, not just another little face."
  }
};

const char *const PERSONAL_TALK_EGG_LINES[8] = {
  "Your voice reaches me through the shell.", "I feel the gentle warmth outside.",
  "I am listening from my little egg.", "The world sounds kind out there.",
  "I am not ready to hatch, but I know you are near.", "The shell rocks when you visit.",
  "I am keeping this first hello.", "I wonder what our home will be like."
};

}

void dialoguePopupText(const char *youLine, const char *petLine, byte talkTopic) {
  const uint32_t sequence = parentDialogue.lifetimeConversations();
  // Mix the pet's birth, generation, topic and saved conversation count. The
  // topic decides which kind of real memory to recall; the mix only varies how
  // that memory is phrased and which related event is selected.
  const uint32_t variant = sequence * 1009UL + birthCode +
      static_cast<uint32_t>(talkTopic) * 131UL +
      static_cast<uint32_t>(generation) * 17UL;
  const uint8_t relationship = static_cast<uint8_t>(variant % 8U);
  const uint8_t opener = static_cast<uint8_t>((variant / 8U) % 12U);

  char memory[170];
  TamachibiMemoryContext memoryContext = makeTamachibiMemoryContext();
  if (stage == STAGE_BABY)
    buildTamachibiBabyMemory(memoryContext, sequence + variant / 17U, memory, sizeof(memory));
  else if (stage >= STAGE_CHILD) {
    uint8_t memoryKind = tamachibiMemoryForDirectTalk(talkTopic);
    if (talkTopic == 12 && memoryContext.fishCount == 0) memoryKind = TAMA_MEMORY_CARE;
    buildTamachibiPersonalMemory(memoryContext, memoryKind,
                                 sequence + variant / 13U, memory, sizeof(memory));
  }
  else memory[0] = '\0';

  const uint8_t relationshipTier = ownerTrust < 40 || ownerBond < 35 ? 0 :
                                  (ownerTrust >= 70 && ownerBond >= 70 ? 2 : 1);
  const char *baseReply = petLine ? petLine : "I like that you asked me.";
  if (stage == STAGE_EGG) {
    baseReply = PERSONAL_TALK_EGG_LINES[relationship];
  }

  char openingLine[112];
  const char *nickname = reinterpret_cast<const char *>(ownerNickname());
  if (stage >= STAGE_CHILD && (opener % 4U) == 0 && nickname && nickname[0])
    snprintf(openingLine, sizeof(openingLine), "Hey, %s. %s",
             nickname, PERSONAL_TALK_RELATIONSHIPS[relationshipTier][relationship]);
  else if (stage >= STAGE_CHILD)
    snprintf(openingLine, sizeof(openingLine), "%s",
             PERSONAL_TALK_RELATIONSHIPS[relationshipTier][relationship]);
  else if (stage == STAGE_BABY)
    snprintf(openingLine, sizeof(openingLine), "%s", PERSONAL_TALK_BABY_OPENERS[opener]);
  else openingLine[0] = '\0';

  char reply[540];
  if (stage == STAGE_EGG) snprintf(reply, sizeof(reply), "%s", baseReply);
  else if (memory[0]) snprintf(reply, sizeof(reply), "%s %s\n%s", openingLine, baseReply, memory);
  else snprintf(reply, sizeof(reply), "%s %s", openingLine, baseReply);
  char text[640];
  snprintf(text, sizeof(text), "%s\n%s", youLine ? youLine : "You listen.", reply);
  char title[24];
  snprintf(title, sizeof(title), "%s REMEMBERS", petName[0] ? petName : "TAMA");
  growingTogether.showReadableMessage(title, text, "D1 close");
  parentDialogue.recordDirectTalk(stage);
  lastNarrativeInteractionAtMs = millis();
}

void dialoguePopup(const __FlashStringHelper* youLine,
                   const __FlashStringHelper* petLine, byte talkTopic) {
  // ESP32 flash strings are memory mapped and can be passed to the same
  // scrollable viewer as generated, relationship-aware replies.
  dialoguePopupText(reinterpret_cast<const char *>(youLine),
                    reinterpret_cast<const char *>(petLine), talkTopic);
}

TamachibiMemoryContext makeTamachibiMemoryContext() {
  TamachibiMemoryContext m{};
  m.ownerNickname = reinterpret_cast<const char *>(ownerNickname());
  m.lastTripName = lastTripEvent < 9
      ? reinterpret_cast<const char *>(lifeWorld.locationName(lastTripEvent)) : nullptr;
  m.favoriteGameName = reinterpret_cast<const char *>(gameName(favoriteGameId % 13));
  m.companionName = lifeWorld.companionName();
  memcpy(m.careTypes, careHistoryType, sizeof(m.careTypes));
  memcpy(m.careDays, careHistoryDay, sizeof(m.careDays));
  memcpy(m.careHours, careHistoryHour, sizeof(m.careHours));
  m.careHead = careHistoryHead;
  m.careCount = careHistoryCount;
  m.caredDayStreak = caredDayStreak;
  m.missedCareDays = missedCareDays;
  m.rescuedFromSickness = rescuedFromSickness;
  m.ownerBond = ownerBond;
  m.ownerTrust = ownerTrust;
  m.lastFood = lastFood;
  m.learnedFavoriteFood = learnedFavoriteFood;
  m.learnedDislikedFood = learnedDislikedFood;
  m.favoriteGivenFood = favoriteGivenFood;
  m.favoriteGamePlays = gameCounts[favoriteGameId % 13];
  m.lastTripEvent = lastTripEvent;
  m.gardenCount = botany.gardenCount();
  m.gardenHealth = botany.gardenAverageHealth();
  m.companionType = lifeWorld.companionType();
  m.companionBond = lifeWorld.companionBond();
  m.companionMood = lifeWorld.companionMood();
  m.careerId = lifeWorld.careerId();
  m.careerRank = lifeWorld.careerRank();
  m.careerName = reinterpret_cast<const char *>(lifeWorld.careerName(m.careerId));
  m.fishCount = aquarium.fishCount();
  m.temperament = temperament;
  m.emotion = deepLife.emotionId();
  m.emotionIntensity = deepLife.emotionIntensity();
  m.emotionName = reinterpret_cast<const char *>(deepLife.emotionName());
  m.familyMood = deepLife.familyMood();
  m.childCount = deepLife.childCount();
  m.siblingBond = deepLife.averageSiblingBond();
  m.familyPresent = (partnerId < PARTNER_COUNT || m.childCount > 0 || m.siblingBond > 0) ? 1 : 0;
  return m;
}

ParentDialogueContext makeParentDialogueContext() {
  ParentDialogueContext c{};
  c.ageTicks = ageTicks;
  c.ageDays = static_cast<uint16_t>(min<unsigned long>(petAgeDays(), 65535UL));
  c.stage = stage; c.hour = visualHour(); c.weather = visualWeather();
  c.temperament = temperament; c.species = speciesType;
  c.health = health; c.full = full; c.energy = energy; c.happy = happy; c.clean = clean;
  c.discipline = discipline; c.poop = poop; c.sick = (sick || conditionType != 0) ? 1 : 0;
  c.ownerBond = ownerBond; c.ownerTrust = ownerTrust; c.careMistakes = careMistakes; c.mischief = mischief;
  c.smart = skillSmart; c.style = skillStyle; c.kindness = skillKind; c.fitness = skillFit;
  c.favoriteGame = favoriteGameId; c.favoriteFood = favoriteGivenFood;
  c.location = lifeWorld.currentLocation(); c.careerId = lifeWorld.careerId(); c.careerRank = lifeWorld.careerRank();
  c.companionType = lifeWorld.companionType(); c.companionBond = lifeWorld.companionBond();
  c.companionMood = lifeWorld.companionMood(); c.companionEnergy = lifeWorld.companionEnergy();
  c.gardenCount = botany.gardenCount(); c.gardenMature = botany.gardenMatureCount();
  c.gardenStage = botany.gardenAverageStage(); c.gardenHealth = botany.gardenAverageHealth();
  c.gardenWater = botany.gardenAverageWater(); c.indoorCount = botany.indoorCount();
  c.indoorHealth = botany.indoorAverageHealth();
  c.emotion = deepLife.emotionId(); c.emotionIntensity = deepLife.emotionIntensity();
  c.rivalBond = deepLife.rivalBond(); c.rivalRespect = deepLife.rivalRespect();
  c.partnerHarmony = deepLife.partnerHarmony(); c.familyMood = deepLife.familyMood();
  c.childCount = deepLife.childCount(); c.lowestChildBond = deepLife.lowestChildBond();
  c.siblingBond = deepLife.averageSiblingBond(); c.pendingCallType = pendingCallType;
  c.memory = makeTamachibiMemoryContext();
  return c;
}

GrowingTogetherContext makeGrowingTogetherContext() {
  GrowingTogetherContext c{};
  c.ageTicks = ageTicks;
  c.ageDays = static_cast<uint16_t>(min<unsigned long>(petAgeDays(), 65535UL));
  c.generation = generation;
  c.stage = stage; c.hour = visualHour(); c.weather = visualWeather();
  c.temperament = temperament; c.species = speciesType; c.location = lifeWorld.currentLocation();
  c.health = health; c.full = full; c.energy = energy; c.happy = happy; c.clean = clean;
  c.discipline = discipline; c.sick = (sick || conditionType != 0) ? 1 : 0;
  c.ownerBond = ownerBond; c.ownerTrust = ownerTrust; c.mischief = mischief;
  c.emotion = deepLife.emotionId(); c.emotionIntensity = deepLife.emotionIntensity();
  c.hasPartner = partnerId < PARTNER_COUNT ? 1 : 0;
  c.partnerHarmony = deepLife.partnerHarmony(); c.familyMood = deepLife.familyMood();
  c.childCount = deepLife.childCount(); c.lowestChildBond = deepLife.lowestChildBond();
  c.siblingBond = deepLife.averageSiblingBond(); c.rivalBond = deepLife.rivalBond(); c.rivalRespect = deepLife.rivalRespect();
  c.careerId = lifeWorld.careerId(); c.careerRank = lifeWorld.careerRank();
  c.companionType = lifeWorld.companionType(); c.companionBond = lifeWorld.companionBond();
  c.companionMood = lifeWorld.companionMood(); c.companionEnergy = lifeWorld.companionEnergy();
  c.gardenCount = botany.gardenCount(); c.gardenMature = botany.gardenMatureCount();
  c.gardenStage = botany.gardenAverageStage(); c.gardenHealth = botany.gardenAverageHealth();
  c.gardenWater = botany.gardenAverageWater(); c.indoorCount = botany.indoorCount(); c.indoorHealth = botany.indoorAverageHealth();
  c.openness = parentDialogue.openness(); c.confidence = parentDialogue.confidence(); c.empathy = parentDialogue.empathy();
  c.responsibility = parentDialogue.responsibility(); c.resilience = parentDialogue.resilience(); c.honesty = parentDialogue.honesty();
  c.independence = parentDialogue.independence(); c.schoolConfidence = parentDialogue.schoolConfidence();
  c.socialConfidence = parentDialogue.socialConfidence(); c.attachmentSecurity = parentDialogue.attachmentSecurity();
  c.memory = makeTamachibiMemoryContext();
  return c;
}

void applyGrowingTogetherOutcome(const GrowingTogetherOutcome &o) {
  if (!o.resolved && !o.postponed) return;
  if (o.resolved) completeGameplayAction(M_TALK);
  changeBond(o.bond); changeTrust(o.trust);
  if (o.happy > 0) addStat(happy, static_cast<byte>(o.happy));
  else if (o.happy < 0) subStat(happy, static_cast<byte>(-o.happy));
  if (o.discipline > 0) addStat(discipline, static_cast<byte>(o.discipline));
  else if (o.discipline < 0) subStat(discipline, static_cast<byte>(-o.discipline));
  if (o.kindness > 0) addSkill(skillKind, static_cast<byte>(o.kindness));
  if (o.smart > 0) addSkill(skillSmart, static_cast<byte>(o.smart));
  if (o.mischief != 0) mischief = static_cast<byte>(constrain(static_cast<int>(mischief) + o.mischief, 0, 20));
  parentDialogue.eventAdjustDevelopment(o.parentOpenness, o.parentConfidence, o.parentEmpathy,
                                        o.parentResponsibility, o.parentResilience, o.parentHonesty,
                                        o.parentIndependence, o.parentEmotionWords,
                                        o.parentSchoolConfidence, o.parentSocialConfidence,
                                        o.parentSafetyConfidence, o.parentAttachment);
  if (lifeWorld.hasCompanion() && (o.companionBond || o.companionMood || o.companionEnergy)) {
    lifeWorld.eventAdjustCompanion(o.companionBond, o.companionMood, o.companionEnergy,
                                   o.companionBond > 0 ? 3 : 0);
  }
  if (o.gardenWater > 0) botany.eventWaterGarden(static_cast<uint8_t>(o.gardenWater));
  if (o.gardenCare > 0) botany.eventTendGarden(static_cast<uint8_t>(o.gardenCare));
  if (o.gardenDamage > 0) botany.eventDamageGarden(static_cast<uint8_t>(o.gardenDamage));
  deepLife.eventParentConversation(o.partnerHarmony, o.familyMood, o.rivalBond, o.childBond,
                                   o.emotion, o.emotionIntensity, o.feature,
                                   static_cast<uint16_t>(min<unsigned long>(petAgeDays(), 65535UL)));
  markRoutineAction(ROUTINE_GREET);
  markRoutineAction(ROUTINE_TALK);
  recordGoalEvent(1, 1);
  clampAll(); markSaveDirty();
}

void runGrowingTogetherCheckIn(bool manual) {
  if (sitterTicksRemaining > 0 || petAway || sleeping) return;
  GrowingTogetherContext context = makeGrowingTogetherContext();
  if (manual) growingTogether.scheduleManual(context);
  if (!growingTogether.hasPending()) return;
  GrowingTogetherOutcome outcome = growingTogether.runPending(context);
  applyGrowingTogetherOutcome(outcome);
  lastInputAt = millis(); oldL = HIGH; oldS = HIGH; oldR = HIGH;
  lastNarrativeInteractionAtMs = lastInputAt;
}

void recordGrowingAction(uint8_t action) {
  growingTogether.recordAction(action, makeGrowingTogetherContext());
}

void applyParentDialogueOutcome(const ParentDialogueOutcome &o) {
  if (!o.resolved && !o.dismissed) { markSaveDirty(); return; }
  changeBond(o.bond); changeTrust(o.trust);
  if (o.happy > 0) addStat(happy, static_cast<byte>(o.happy));
  else if (o.happy < 0) subStat(happy, static_cast<byte>(-o.happy));
  if (o.discipline > 0) addStat(discipline, static_cast<byte>(o.discipline));
  else if (o.discipline < 0) subStat(discipline, static_cast<byte>(-o.discipline));
  if (o.smart > 0) addSkill(skillSmart, static_cast<byte>(o.smart));
  if (o.kindness > 0) addSkill(skillKind, static_cast<byte>(o.kindness));
  if (o.mischief != 0) mischief = static_cast<byte>(constrain(static_cast<int>(mischief) + o.mischief, 0, 20));
  if (o.dismissed) { clampAll(); markSaveDirty(); return; }
  completeGameplayAction(M_TALK);
  if (lifeWorld.hasCompanion() && (o.companionBond || o.companionMood || o.companionEnergy)) {
    lifeWorld.eventAdjustCompanion(o.companionBond, o.companionMood, o.companionEnergy, o.companionBond > 0 ? 2 : 0);
  }
  if (o.gardenWater > 0) botany.eventWaterGarden(static_cast<uint8_t>(o.gardenWater));
  if (o.gardenCare > 0) botany.eventTendGarden(static_cast<uint8_t>(o.gardenCare));
  deepLife.eventParentConversation(o.partnerHarmony, o.familyMood, o.rivalBond,
                                   o.familyMood > 0 ? 1 : (o.familyMood < 0 ? -1 : 0),
                                   o.emotion, o.emotionIntensity, o.topic, static_cast<uint16_t>(min<unsigned long>(petAgeDays(), 65535UL)));
  markRoutineAction(ROUTINE_GREET);
  markRoutineAction(ROUTINE_TALK);
  recordGoalEvent(1, 1);
  addSkill(skillKind, 1);
  clampAll(); markSaveDirty();
}

void runParentDialogueCheckIn(bool manual) {
  if (sitterTicksRemaining > 0 || petAway || sleeping) return;
  ParentDialogueContext context = makeParentDialogueContext();
  if (manual) parentDialogue.scheduleManual(context);
  if (!parentDialogue.hasPending()) return;
  ParentDialogueOutcome outcome = parentDialogue.runPending(context);
  applyParentDialogueOutcome(outcome);
  lastInputAt = millis(); oldL = HIGH; oldS = HIGH; oldR = HIGH;
  lastNarrativeInteractionAtMs = lastInputAt;
}

void talkActivity() {
  clickSound();
  waitButtonRelease();

  const byte TALK_COUNT = 13;
  byte q = 0;
  bool ol = HIGH, os = HIGH, orr = HIGH;

  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(BTN_L), s = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (l == LOW || s == LOW || r == LOW) lastInputAt = millis();
    if (ol == HIGH && l == LOW) { q = q == 0 ? TALK_COUNT - 1 : q - 1; clickSound(); delay(120); }
    if (orr == HIGH && r == LOW) { q = (q + 1) % TALK_COUNT; clickSound(); delay(120); }
    if (os == HIGH && s == LOW) break;
    ol = l; os = s; orr = r;

    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(18, 0); display.print(F("Talk with ")); display.print(petName);
    drawRightScore(q + 1, TALK_COUNT);
    display.drawRoundRect(8, 18, 112, 30, 5, WHITE); display.setCursor(18, 28);
    const bool babyTalk = stage <= STAGE_BABY;
    const bool eggTalk = stage == STAGE_EGG;
    if (q == 0) display.print(eggTalk ? F("Talk to the egg") :
        (parentDialogue.hasPending() || (!babyTalk && growingTogether.hasPending()) ? F("Answer waiting talk") : F("Parent check-in")));
    else if (q == 1) display.print(F("Say hello"));
    else if (q == 2) display.print(babyTalk ? F("Check baby's mood") : F("Ask mood"));
    else if (q == 3) display.print(babyTalk ? F("Show favorite things") : F("Ask likes"));
    else if (q == 4) display.print(babyTalk ? F("Make silly sound") : F("Tell joke"));
    else if (q == 5) display.print(babyTalk ? F("Cheer baby") : F("Praise them"));
    else if (q == 6) display.print(babyTalk ? F("Ask sleepy dream") : F("Ask dream"));
    else if (q == 7) display.print(babyTalk ? F("Play peekaboo") : F("Ask secret"));
    else if (q == 8) display.print(babyTalk ? F("Teach gentle hands") : F("Give advice"));
    else if (q == 9) display.print(babyTalk ? F("Baby temperament") : F("Personality"));
    else if (q == 10) display.print(F("Parent bond journal"));
    else if (q == 11) display.print(babyTalk ? F("Unlocks at child") : F("Growing Together"));
    else display.print(babyTalk ? F("Look at fish") : F("Ask about fish"));
    display.setCursor(10, 56); display.print(F("D0/D2 browse  D1 choose"));
    display.display(); delay(20);
  }

  waitButtonRelease();
  bool parentConversation = false;
  if (q == 0) {
    parentConversation = true;
    if (stage == STAGE_EGG) dialoguePopup(F("You speak softly"), F("Your voice reaches me through the shell."), q);
    else if (stage == STAGE_BABY) runParentDialogueCheckIn(true);
    else if (parentDialogue.hasPending()) runParentDialogueCheckIn(false);
    else if (growingTogether.hasPending()) runGrowingTogetherCheckIn(false);
    else runParentDialogueCheckIn(true);
  } else if (q == 1) {
    dialoguePopup(F("You: hello!"), stage == STAGE_BABY ? F("Ba-ba! I know your voice.") : petDialogue(), q);
    addStat(happy, 1);
  } else if (q == 2) {
    dialoguePopup(F("You: how feel?"), petMoodTreeLine(), q);
  } else if (q == 3) {
    const char *likes = reinterpret_cast<const char *>(favoriteThing());
    const char *dislikes = reinterpret_cast<const char *>(dislikedThing());
    if (strncmp(likes, "likes ", 6) == 0) likes += 6;
    if (strncmp(dislikes, "hates ", 6) == 0) dislikes += 6;
    const uint8_t likedFood = learnedFavoriteFood < FOOD_COUNT ? learnedFavoriteFood :
                              (favoriteGivenFood < FOOD_COUNT ? favoriteGivenFood : favoriteFoodType());
    const uint8_t game = favoriteGameId % 13;
    char reply[250];
    if (stage == STAGE_BABY) {
      snprintf(reply, sizeof(reply), "I like %s. I am still tasting different foods.", likes);
    } else {
      snprintf(reply, sizeof(reply), "Things I like: %s. Food that feels special is %s. I try to avoid %s. My favorite game is %s, and we have played it %u times.",
               likes, tamachibiFoodMemoryName(likedFood), dislikes,
               reinterpret_cast<const char *>(gameName(game)), gameCounts[game]);
    }
    dialoguePopupText("You ask what I like", reply, q);
  } else if (q == 4) {
    dialoguePopup(F("You tell joke"), petJokeReply(), q); addStat(happy, 2); if (speciesPersonalityType() == 4) mischief++;
  } else if (q == 5) {
    dialoguePopup(F("You praise pet"), petPraiseReply(), q); addStat(happy, 2); addSkill(skillKind, 1); if (praiseCount < 255) praiseCount++; changeBond(1);
  } else if (q == 6) {
    if (stage == STAGE_BABY) dialoguePopup(F("You: sleepy dream?"), F("Mmm... warm light, you."), q);
    else dialoguePopup(F("You: dream?"), petDreamLine(), q);
  } else if (q == 7) {
    if (stage == STAGE_BABY) dialoguePopup(F("Peekaboo!"), F("Ba! There you are!"), q);
    else if (ownerBond < 60) dialoguePopup(F("Secret?"), F("Maybe when we are closer."), q);
    else { dialoguePopup(F("Secret?"), speciesSecretLine(), q); addSkill(skillSmart, 1); }
  } else if (q == 8) {
    dialoguePopup(stage == STAGE_BABY ? F("You: gentle hands") : F("You give advice"), petAdviceReply(), q);
    addStat(discipline, 1); if (mischief > 0) mischief--;
  } else if (q == 9) {
    const char *traits[8] = {
      "I take my time and notice the quiet details.", "I want to see what is beyond the next corner.",
      "I keep asking questions because the world is interesting.", "I feel best when we make time for each other.",
      "I like the playful parts of life and the occasional harmless prank.", "I feel connected to slow mornings and growing things.",
      "I notice when you remember what makes me happy.", "I like trying things on my own while knowing you are nearby."
    };
    char reply[240];
    if (stage == STAGE_BABY) {
      snprintf(reply, sizeof(reply), "I am a little %s. Ba-ba! I like when you are near.",
               reinterpret_cast<const char *>(personalityName()));
    } else {
      snprintf(reply, sizeof(reply), "I am %s. %s One thing that feels very me is: %s",
               reinterpret_cast<const char *>(personalityName()), traits[temperament % 8],
               reinterpret_cast<const char *>(petDialogue()));
    }
    dialoguePopupText("You ask about my personality", reply, q);
  } else if (q == 10) {
    parentDialogue.runJournal(makeParentDialogueContext());
  } else if (q == 11) {
    parentConversation = true;
    if (stage <= STAGE_BABY) dialoguePopup(F("Growing Together"),
        stage == STAGE_EGG ? F("I will have more stories after I hatch.") : F("Unlocks when baby becomes a child."), q);
    else {
      growingTogether.runHub(makeGrowingTogetherContext());
      if (growingTogether.hasPending()) runGrowingTogetherCheckIn(false);
    }
  } else {
    if (stage <= STAGE_BABY) dialoguePopup(F("You show the fish"),
        stage == STAGE_EGG ? F("I can hear the water move outside my shell.") : F("Bub-bub! Fish!"), q);
    else if (!aquarium.hasFish()) dialoguePopup(F("You ask about the aquarium"), F("Maybe we will meet a fish together one day."), q);
    else {
      char reply[250];
      aquarium.composeSelectedFishComment(reply, sizeof(reply));
      dialoguePopupText("You ask about the aquarium", reply, q);
      changeBond(1);
    }
  }

  bool firstTalkToday = (dailyRoutineMask & ROUTINE_TALK) == 0;
  lastTalkChoice = q;
  completeGameplayAction(M_TALK);
  markRoutineAction(ROUTINE_GREET); markRoutineAction(ROUTINE_TALK);
  rewardBirthdayIfRemembered(); recordGoalEvent(1, 1);
  if (!parentConversation) {
    addSkill(skillKind, 1); addStat(happy, 1);
    if (firstTalkToday) { changeBond(2); changeTrust(1); }
  }
  clampAll(); markSaveDirty(); goodSound();
}


// ---------- status page ----------

void statusPage() {
  clickSound();
  updateBatteryReading(true);
  markRoutineAction(ROUTINE_CHECK);
  waitButtonRelease();
  byte page = 0;
  bool oldLeft = HIGH, oldSel = HIGH, oldRight = HIGH;

  while (true) {
    if (rightHoldExitCheck()) break;
    const bool l = digitalRead(BTN_L);
    const bool sBtn = digitalRead(BTN_S);
    const bool r = digitalRead(BTN_R);
    if (oldLeft == HIGH && l == LOW) { page = page == 0 ? 7 : page - 1; clickSound(); delay(100); }
    if (oldRight == HIGH && r == LOW) { page = (page + 1) % 8; clickSound(); delay(100); }
    if (oldSel == HIGH && sBtn == LOW) break;
    oldLeft = l; oldSel = sBtn; oldRight = r;

    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    V46UI::drawHeader(display, F("STATUS"), visualHour(), clockMinute(), false, 0);
    display.setCursor(105, 10); display.print(page + 1); display.print(F("/8"));

    if (page == 0) {
      const byte values[5] = {health, full, energy, happy, clean};
      const __FlashStringHelper *labels[5] = {F("Health"), F("Food"), F("Energy"), F("Mood"), F("Clean")};
      const char icons[5] = {'H','F','E','M','C'};
      for (byte i = 0; i < 5; ++i) {
        const int y = 12 + i * 10;
        display.setCursor(0, y); display.print(icons[i]);
        display.drawRect(9, y, 43, 7, WHITE);
        display.fillRect(11, y + 2, min(39, (int)values[i] * 4), 3, WHITE);
        display.setCursor(57, y); display.print(labels[i]);
        display.setCursor(116, y); display.print(values[i]);
      }
    } else if (page == 1) {
      display.setCursor(0, 11); display.print(F("Name")); display.setCursor(48, 11); display.print(petName);
      display.setCursor(0, 20); display.print(F("Stage")); display.setCursor(48, 20); display.print(stageName());
      display.setCursor(0, 29); display.print(F("Age days")); display.setCursor(84, 29); display.print(petAgeDays());
      display.setCursor(0, 38); display.print(F("Generation")); display.setCursor(84, 38); display.print(generation);
      display.setCursor(0, 47); display.print(F("Bond")); display.setCursor(42, 47); display.print(ownerBond);
      display.setCursor(70, 47); display.print(F("Trust")); display.setCursor(112, 47); display.print(ownerTrust);
      display.setCursor(0, 56); display.print(F("Care mistakes")); display.setCursor(104, 56); display.print(careMistakes);
    } else if (page == 2) {
      const __FlashStringHelper *labels[5] = {F("Smart"), F("Style"), F("Kind"), F("Fitness"), F("Weight")};
      const unsigned int values[5] = {skillSmart, skillStyle, skillKind, skillFit, weight};
      for (byte i = 0; i < 5; ++i) {
        const int y = 12 + i * 10;
        display.setCursor(0, y); display.print(labels[i]);
        display.setCursor(52, y); display.print(values[i]);
      }
      display.setCursor(72, 12); display.print(F("Best")); display.setCursor(108, 12); display.print(crossyBest);
      display.setCursor(72, 22); display.print(F("Cook")); display.setCursor(108, 22); display.print(cookedMeals);
      display.setCursor(72, 32); display.print(F("Trips")); display.setCursor(108, 32); display.print(tripBadges);
      display.setCursor(72, 42); display.print(F("Friends")); display.setCursor(114, 42); display.print(friendCount);
      display.setCursor(72, 52); display.print(F("Mis")); display.setCursor(108, 52); display.print(mischief);
    } else if (page == 3) {
      const __FlashStringHelper *labels[5] = {F("Nutrition"), F("Clean habit"), F("Exercise"), F("Illnesses"), F("Overfed")};
      const unsigned int values[5] = {nutritionScore, cleanlinessScore, exerciseScore, sicknessHistory, overfeedCount};
      for (byte i = 0; i < 5; ++i) {
        const int y = 12 + i * 10;
        display.setCursor(0, y); display.print(labels[i]);
        display.setCursor(76, y); display.print(values[i]);
      }
      display.setCursor(96, 12); display.print(F("H")); display.print(hobbyType);
      display.setCursor(96, 22); display.print(F("G")); display.print(favoriteGameId + 1);
    } else if (page == 4) {
      display.setCursor(0, 12); display.print(F("Routine "));
      display.print((dailyRoutineMask & ROUTINE_GREET) ? 'G' : '-');
      display.print((dailyRoutineMask & ROUTINE_CHECK) ? 'S' : '-');
      display.print((dailyRoutineMask & ROUTINE_CARE) ? 'C' : '-');
      display.print((dailyRoutineMask & ROUTINE_TALK) ? 'T' : '-');
      display.print((dailyRoutineMask & ROUTINE_PLAY) ? 'P' : '-');
      display.setCursor(88, 12); display.print(F("St")); display.print(caredDayStreak);
      display.setCursor(0, 23); display.print(F("Care days")); display.setCursor(72, 23); display.print(countCareWeekBits(weeklyCareBits)); display.print(F("/8"));
      display.setCursor(0, 34); display.print(F("Daily goal")); display.setCursor(72, 34); display.print(dailyGoalProgress); display.print(F("/")); display.print(dailyGoalTarget);
      display.setCursor(0, 45); display.print(F("Weekly goal")); display.setCursor(72, 45); display.print(weeklyGoalProgress); display.print(F("/")); display.print(weeklyGoalTarget);
      display.setCursor(0, 56); display.print(F("Good day: 4 of 5 care"));
    } else if (page == 5) {
      display.setCursor(0, 11); display.print(F("Garden")); display.setCursor(66, 11); display.print(botany.gardenCount());
      if (botany.gardenReady()) { display.setCursor(88, 11); display.print(F("READY")); }
      display.setCursor(0, 20); display.print(F("House plants")); display.setCursor(90, 20); display.print(botany.indoorCount());
      display.setCursor(0, 29); display.print(F("Aquarium")); display.setCursor(90, 29); display.print(aquarium.fishCount());
      display.setCursor(0, 38); display.print(F("Sitter")); display.setCursor(60, 38);
      if (sitterTicksRemaining > 0) { display.print((sitterTicksRemaining + TICKS_PER_HOUR - 1) / TICKS_PER_HOUR); display.print(F("h")); }
      else display.print(F("off"));
      display.setCursor(0, 47); display.print(F("Battery")); display.setCursor(60, 47);
      if (batteryMv > 0) { display.print(batteryMv); display.print(F("mV")); } else display.print(F("not wired"));
      display.setCursor(0, 56); display.print(F("Next")); display.setCursor(42, 56);
      if (stage == STAGE_EGG) display.print(F("baby"));
      else if (stage == STAGE_BABY) display.print(F("child day 1"));
      else if (stage == STAGE_CHILD) display.print(F("teen day 3"));
      else if (stage == STAGE_TEEN) display.print(F("adult day 6"));
      else if (stage == STAGE_ADULT) display.print(F("elder day 20"));
      else display.print(F("legacy"));
    } else if (page == 7) {
      display.setCursor(2, 12); display.print(temperamentName());
      display.setCursor(2, 24); display.print(F("Natural talent:"));
      display.setCursor(2, 35); display.print(menuTitle(naturalActivity()));
      display.setCursor(2, 47); display.print(F("Practice earns more."));
      display.setCursor(2, 57); display.print(F("Likes grow with care."));
    } else {
      display.setCursor(2, 12); display.print(weekdayName(clockWeekday())); display.print(' '); drawClock12Hour(visualHour(), clockMinute());
      display.setCursor(70, 12); display.print(scheduledPlaceName());
      const byte activeStart = stage == STAGE_ADULT ? 9 : 8;
      const byte activeEnd = stage == STAGE_ADULT ? 17 : 15;
      V46UI::drawTimeline(display, wakeHour(), activeStart, activeEnd, bedtimeHour(), visualHour(),
                          isScheduledSchoolTime() || isScheduledWorkTime());
    }
    V46UI::drawFooter(display, F("D0 PAGE"), F("D1 CLOSE"), F("D2 PAGE"));
    display.display();
    delay(20);
  }
  waitButtonRelease();
}

byte categoryActionCount(byte category) {
  if (category == 0) return 2;
  if (category == 1) return 5;
  if (category == 2) return 6;
  if (category == 3) return 4;
  if (category == 4) return 13;
  return 7;
}

byte categoryActionAt(byte category, byte index) {
  static const byte foodActions[2] = {M_FOOD, M_COOK};
  static const byte careActions[5] = {M_LIGHT, M_MED, M_CLEAN, M_LOVE, M_CALL};
  static const byte playActions[6] = {M_GAME, M_WALK, M_TRAIN, M_MUSIC, M_TOY, M_PLAYER};
  static const byte talkActions[4] = {M_TALK, M_DIARY, M_NEIGHBORS, M_FAMILY};
  static const byte activityActions[13] = {M_LIFE, M_PARADISE, M_STUDY, M_GARDEN, M_PLANTROOM, M_AQUARIUM, M_JOB, M_EXPLORE, M_CRAFT, M_HABITAT, M_EVENT, M_SHOP, M_ROOM};
  static const byte statusActions[7] = {M_STATS, M_INSPECT, M_BOOK, M_HISTORY, M_SETTINGS, M_ALBUM, M_SITTER};
  if (category == 0) return foodActions[index % 2];
  if (category == 1) return careActions[index % 5];
  if (category == 2) return playActions[index % 6];
  if (category == 3) return talkActions[index % 4];
  if (category == 4) return activityActions[index % 13];
  return statusActions[index % 7];
}

bool actionUnlocked(byte action) {
  if (action == M_PLAYER || action == M_LIGHT || action == M_TALK || action == M_STATS || action == M_SETTINGS || action == M_ALBUM || action == M_SITTER || action == M_BOOK || action == M_INSPECT || action == M_HISTORY || action == M_DIARY || action == M_PARADISE || action == M_LIFE) return true;
  if (stage == STAGE_EGG) return false;
  if (action == M_NEIGHBORS) return stage >= STAGE_CHILD;
  if (action == M_FOOD || action == M_MED || action == M_CLEAN || action == M_LOVE || action == M_CALL) return true;
  if (stage >= STAGE_BABY && action == M_GAME) return true;
  if (stage >= STAGE_CHILD && (action == M_WALK || action == M_TRAIN || action == M_MUSIC || action == M_TOY || action == M_GARDEN || action == M_PLANTROOM || action == M_AQUARIUM)) return true;
  if (stage >= STAGE_TEEN && (action == M_STUDY || action == M_COOK || action == M_EXPLORE || action == M_CRAFT || action == M_HABITAT || action == M_EVENT || action == M_SHOP || action == M_ROOM)) return true;
  if (stage >= STAGE_ADULT && (action == M_JOB || action == M_FAMILY)) return true;
  return false;
}

const __FlashStringHelper* unlockHint(byte action) {
  if (stage == STAGE_EGG) return F("Hatches soon");
  if (action == M_NEIGHBORS) return F("Unlocks: child");
  if (action == M_GAME) return F("Unlocks: baby");
  if (action == M_WALK || action == M_TRAIN || action == M_MUSIC || action == M_TOY || action == M_GARDEN || action == M_PLANTROOM || action == M_AQUARIUM) return F("Unlocks: child");
  if (action == M_JOB) return F("Unlocks: adult");
  return F("Unlocks: teen");
}

byte characterSetIndex(char c) {
  const char chars[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  for (byte i = 0; i < sizeof(chars) - 1; i++) if (chars[i] == c) return i;
  return 1;
}

void namePetActivity() {
  const char chars[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  const byte charCount = sizeof(chars) - 1;
  byte pos = 0;
  byte selected = characterSetIndex(petName[0]);
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();

  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(BTN_L), sBtn = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (l == LOW || sBtn == LOW || r == LOW) lastInputAt = millis();

    if (oldL == HIGH && l == LOW) {
      selected = (selected == 0) ? charCount - 1 : selected - 1;
      petName[pos] = chars[selected]; clickSound(); delay(110);
    }
    if (oldR == HIGH && r == LOW) {
      selected++; if (selected >= charCount) selected = 0;
      petName[pos] = chars[selected]; clickSound(); delay(110);
    }
    if (oldS == HIGH && sBtn == LOW) {
      petName[pos] = chars[selected];
      if (pos >= 5) break;
      pos++;
      if (petName[pos] == '\0') petName[pos] = 'A';
      selected = characterSetIndex(petName[pos]);
      clickSound(); delay(140);
    }
    oldL = l; oldS = sBtn; oldR = r;

    display.clearDisplay();
    display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(32, 0); display.print(F("Name your pet"));
    display.drawRoundRect(13, 18, 102, 25, 5, WHITE);
    display.setTextSize(2); display.setCursor(27, 23);
    for (byte i = 0; i < 6; i++) display.print(petName[i] ? petName[i] : ' ');
    display.drawLine(27 + pos * 12, 42, 36 + pos * 12, 42, WHITE);
    display.setTextSize(1); display.setCursor(0, 55); display.print(F("D0/D2 letter D1 next"));
    display.display(); delay(20);
  }

  // Trim trailing spaces while preserving at least one character.
  for (int i = 5; i > 0; i--) {
    if (petName[i] == ' ') petName[i] = '\0';
    else break;
  }
  petName[6] = '\0';
  if (petName[0] == ' ' || petName[0] == '\0') strcpy(petName, "CHIBI");
  markSaveDirty(); goodSound(); popup(F("Nice name!"), F("Saved"), 650);
  waitButtonRelease();
}

void albumPage() {
  if (albumCount == 0) {
    popup(F("Family album"), F("No past pets yet"), 900);
    return;
  }
  byte page = albumCount - 1;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) break;
    bool l = digitalRead(BTN_L), sBtn = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) { page = (page == 0) ? albumCount - 1 : page - 1; clickSound(); delay(120); }
    if (oldR == HIGH && r == LOW) { page++; if (page >= albumCount) page = 0; clickSound(); delay(120); }
    if (oldS == HIGH && sBtn == LOW) break;
    oldL = l; oldS = sBtn; oldR = r;

    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(0, 0); display.print(F("Family album ")); display.print(page + 1); display.print(F("/")); display.print(albumCount);
    drawSprite16(albumSpecies[page], 8, 18);
    display.setCursor(32, 16); display.print(albumName[page]); display.print(F(" G")); display.print(albumGeneration[page]);
    display.setCursor(32, 27); display.print(F("Age ")); display.print(albumAge[page]); display.print(F("d Bond ")); display.print(albumBond[page]);
    display.setCursor(32, 38); display.print(F("Partner ")); if (albumPartner[page] < PARTNER_COUNT) display.print(partnerName(albumPartner[page])); else display.print(F("none"));
    display.setCursor(32, 50); if (albumReason[page] == 2) display.print(F("Peaceful elder")); else if (albumReason[page] == 3) display.print(F("Moved on")); else if (albumReason[page] == 4) display.print(F("Raised family")); else display.print(F("Life remembered"));
    display.display(); delay(20);
  }
  waitButtonRelease();
}

void batteryPage() {
  updateBatteryReading(true);
  display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
  display.setCursor(28, 0); display.print(F("Battery status"));
#if BATTERY_MONITOR_ENABLED
  display.setCursor(8, 18); display.print(F("Voltage: ")); display.print(batteryMv); display.print(F(" mV"));
  display.setCursor(8, 31); display.print(batteryCritical() ? F("CRITICAL") : (batteryLow() ? F("LOW") : F("Battery OK")));
  display.setCursor(8, 45); display.print(F("A6 + switched divider"));
#else
  display.setCursor(8, 17); display.print(F("Meter disabled in code"));
  display.setCursor(8, 29); display.print(F("Set BATTERY_MONITOR"));
  display.setCursor(8, 39); display.print(F("_ENABLED to 1, then"));
  display.setCursor(8, 49); display.print(F("wire A6/D7 divider"));
#endif
  display.display(); delay(1500); waitButtonRelease();
}

bool confirmChoice(const __FlashStringHelper *title, const __FlashStringHelper *warning) {
  byte pick = 1;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return false;
    bool l = digitalRead(BTN_L), sBtn = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if ((oldL == HIGH && l == LOW) || (oldR == HIGH && r == LOW)) { pick = 1 - pick; clickSound(); delay(120); }
    if (oldS == HIGH && sBtn == LOW) { waitButtonRelease(); return pick == 0; }
    oldL = l; oldS = sBtn; oldR = r;
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(12, 3); display.print(title);
    display.setCursor(6, 18); display.print(warning);
    display.fillRoundRect(13, 39, 42, 17, 4, pick == 0 ? WHITE : BLACK); display.drawRoundRect(13, 39, 42, 17, 4, WHITE);
    display.fillRoundRect(73, 39, 42, 17, 4, pick == 1 ? WHITE : BLACK); display.drawRoundRect(73, 39, 42, 17, 4, WHITE);
    display.setTextColor(pick == 0 ? BLACK : WHITE); display.setCursor(24, 44); display.print(F("YES"));
    display.setTextColor(pick == 1 ? BLACK : WHITE); display.setCursor(87, 44); display.print(F("NO"));
    display.display(); delay(20);
  }
}

void sitterModePage() {
  if (sitterTicksRemaining > 0) {
    if (confirmChoice(F("End sitter mode?"), F("Care and aging resume"))) sitterTicksRemaining = 0;
  } else if (sitterCooldownTicks > 0) {
    popup(F("Sitter resting"), F("Try again tomorrow"), 900);
  } else {
    byte hours = 24;
    bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
    waitButtonRelease();
    while (true) {
      if (rightHoldExitCheck()) return;
      bool l = digitalRead(BTN_L), sBtn = digitalRead(BTN_S), r = digitalRead(BTN_R);
      if ((oldL == HIGH && l == LOW) || (oldR == HIGH && r == LOW)) { hours = hours == 24 ? 48 : (hours == 48 ? 72 : 24); clickSound(); delay(120); }
      if (oldS == HIGH && sBtn == LOW) break;
      oldL = l; oldS = sBtn; oldR = r;
      display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
      display.setCursor(27, 3); display.print(F("Sitter mode"));
      display.setCursor(10, 20); display.print(F("Protect for ")); display.print(hours); display.print(F(" hours"));
      display.setCursor(10, 32); display.print(F("No aging or rewards"));
      display.setCursor(0, 55); display.print(F("D0/D2 time D1 start")); display.display(); delay(20);
    }
    if (confirmChoice(F("Start sitter?"), F("Limited to selected time"))) sitterTicksRemaining = (unsigned long)hours * TICKS_PER_HOUR;
  }
  markSaveDirty(); waitButtonRelease();
}

void startReplacementEgg() {
  if (!confirmChoice(F("Start a new egg?"), F("Current pet is archived"))) return;
  if (!dead) { deathReason = 3; archiveCurrentPet(); }
  chooseEgg(); startNewPet(); saveGameNow();
}


const __FlashStringHelper* evolutionHint(byte id) {
  byte tier = growthCareTierForAdult(id);
  if(tier==0) return F("Excellent care + hobby");
  if(tier==1) return F("Good care + chosen field");
  if(tier==2) return F("Balanced daily routine");
  if(tier==3) return F("Needs an unusual path");
  return F("Secret or difficult path");
}

const __FlashStringHelper* speciesNameById(byte id) {
  byte oldSpecies = speciesType;
  byte oldStage = stage;
  speciesType = id % SPECIES_COUNT;
  stage = STAGE_ADULT;
  const __FlashStringHelper* result = speciesName();
  speciesType = oldSpecies;
  stage = oldStage;
  return result;
}

const __FlashStringHelper* craftRecipeName(byte id) {
  static const char *unused = 0; (void)unused;
  switch (id % CRAFT_RECIPE_COUNT) {
    case 0: return F("Rain Set"); case 1: return F("Forest Set"); case 2: return F("Hula Set");
    case 3: return F("Mecha Set"); case 4: return F("Pajama Set"); case 5: return F("Winter Set");
    case 6: return F("Nature Room"); case 7: return F("Study Room"); case 8: return F("Arcade Room");
    case 9: return F("Space Room"); case 10: return F("Medicine Kit"); default: return F("Royal Meal");
  }
}

byte craftMaterialCost(byte id) {
  return 1 + id / 6 - ((naturalActivity() == M_CRAFT && id >= 6) ? 1 : 0);
}

bool craftCanMake(byte id) {
  id %= CRAFT_RECIPE_COUNT;
  byte a = id % MATERIAL_COUNT, b = (id + 2) % MATERIAL_COUNT;
  byte needA = craftMaterialCost(id), needB = 1;
  return craftMaterials[a] >= needA && craftMaterials[b] >= needB;
}

void applyCraftReward(byte id) {
  if (id <= 5) { byte outfitId = 1 + id; outfitOwned |= (1UL << outfitId); outfit = outfitId; }
  else if (id <= 9) { byte roomId = 2 + (id - 6); roomOwned |= (1UL << roomId); roomStyle = roomId; }
  else if (id == 10) { medCold++; medTummy++; medSkin++; medEnergy++; medMood++; }
  else { mealRoyalStew++; cookedMeals++; }
  recipesCrafted |= (1U << id);
  completeGameplayAction(M_CRAFT);
  addSkill(skillStyle, 1); changeBond(1); addStat(happy, 1);
}

void craftPage() {
  byte pick = 0; bool ol=HIGH, os=HIGH, orr=HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l=digitalRead(BTN_L), sb=digitalRead(BTN_S), r=digitalRead(BTN_R);
    if (ol==HIGH && l==LOW) { pick=(pick==0)?CRAFT_RECIPE_COUNT-1:pick-1; clickSound(); delay(120); }
    if (orr==HIGH && r==LOW) { pick=(pick+1)%CRAFT_RECIPE_COUNT; clickSound(); delay(120); }
    if (os==HIGH && sb==LOW) {
      byte a=pick%MATERIAL_COUNT, b=(pick+2)%MATERIAL_COUNT, needA=craftMaterialCost(pick);
      if (!craftCanMake(pick)) { badSound(); popup(F("Need materials"), F("Explore more"), 700); }
      else { craftMaterials[a]-=needA; craftMaterials[b]--; applyCraftReward(pick); winSound(); popup(F("DIY complete!"), craftRecipeName(pick), 750); markSaveDirty(); return; }
      waitButtonRelease();
    }
    ol=l; os=sb; orr=r;
    byte a=pick%MATERIAL_COUNT, b=(pick+2)%MATERIAL_COUNT, needA=craftMaterialCost(pick);
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(25,0); display.print(F("DIY Workshop")); drawRightScore(pick+1,CRAFT_RECIPE_COUNT);
    display.drawRoundRect(5,14,118,34,5,WHITE); display.setCursor(13,21); display.print(craftRecipeName(pick));
    display.setCursor(13,32); display.print(materialName(a)); display.print(F(" ")); display.print(craftMaterials[a]); display.print(F("/")); display.print(needA);
    display.setCursor(68,32); display.print(materialName(b)); display.print(F(" ")); display.print(craftMaterials[b]); display.print(F("/1"));
    display.setCursor(13,42); display.print(recipesCrafted&(1U<<pick)?F("Crafted before"):F("New recipe"));
    display.setCursor(0,56); display.print(F("D1 craft  Hold D2 exit")); display.display(); delay(20);
  }
}

void habitatPage() {
  byte pick=habitat; bool ol=HIGH, os=HIGH, orr=HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l=digitalRead(BTN_L), sb=digitalRead(BTN_S), r=digitalRead(BTN_R);
    if (ol==HIGH && l==LOW) { pick=(pick==0)?HABITAT_COUNT-1:pick-1; clickSound(); delay(120); }
    if (orr==HIGH && r==LOW) { pick=(pick+1)%HABITAT_COUNT; clickSound(); delay(120); }
    if (os==HIGH && sb==LOW) {
      if (!(habitatOwned&(1U<<pick))) { byte cost=4+pick; if (!spendCoins(cost)) { badSound(); popup(F("Need coins"),F("Explore/work")); } else { habitatOwned|=(1U<<pick); habitat=pick; completeGameplayAction(M_HABITAT); winSound(); popup(F("Field unlocked"),habitatName(pick)); } }
      else { if (habitat != pick) completeGameplayAction(M_HABITAT); habitat=pick; goodSound(); popup(F("Moved habitat"),habitatName(pick)); }
      markSaveDirty(); return;
    }
    ol=l; os=sb; orr=r;
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(29,0); display.print(F("Tama Fields")); drawRightScore(pick+1,HABITAT_COUNT);
    byte scene = 3;
    if (pick == 1) scene = 2; else if (pick == 2) scene = 1; else if (pick == 3) scene = 6;
    else if (pick == 5) scene = 7; else if (pick == 6) scene = 8; else if (pick == 7) scene = 4;
    drawBiomeScene(scene, anim);
    display.fillRect(0,0,128,14,BLACK); display.setCursor(15,3); display.print(habitatName(pick));
    display.setCursor(0,56); if(habitatOwned&(1U<<pick))display.print(F("D1 move"));else{display.print(F("D1 unlock $"));display.print(4+pick);} display.display(); delay(20);
  }
}

byte runSeasonEvent(byte event, bool practice) {
  byte score=0;
  for(byte i=0;i<20;i++){
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(0,0); display.print(practice ? F("Practice ") : F("Event ")); display.print(seasonEventName(event));
    display.setCursor(104,0); display.print(score);
    int target=8+((i*23+birthCode+event*11)%108);
    display.drawCircle(target,30+(i%3)*8,3,WHITE); drawCurrentPetTiny(54,42);
    display.setCursor(0,56); display.print(F("Tap any button")); display.display();
    unsigned long started=millis(); bool hit=false;
    while(millis()-started<500){ if(anyButtonDown()){hit=true;break;} delay(5); }
    waitButtonRelease();
    if(hit){score++;goodSound();} else stepSound();
  }
  completeGameplayAction(M_EVENT);
  if(score>seasonalEventBest) seasonalEventBest=score;
  return score;
}

void seasonalEventPage() {
  byte event=currentSeasonEvent(); unsigned long day=petAgeDays();
  waitButtonRelease();
  while(true){
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(18,0); display.print(seasonEventName(event));
    display.setCursor(8,15); display.print(F("Best: ")); display.print(seasonalEventBest);
    display.setCursor(8,27); display.print(F("D0 practice"));
    display.setCursor(8,39); display.print(seasonalEventPlayedDay==day?F("Daily already played"):F("D1 daily challenge"));
    display.setCursor(0,56); display.print(F("Hold D2 exit")); display.display();
    if(rightHoldExitCheck()) return;
    if(digitalRead(BTN_L)==LOW){waitButtonRelease();runSeasonEvent(event,true);popup(F("Practice done"),F("No daily reward"),650);markSaveDirty();}
    if(digitalRead(BTN_S)==LOW && seasonalEventPlayedDay!=day){waitButtonRelease();byte score=runSeasonEvent(event,false);seasonalEventPlayedDay=day;addCoins(1+score/4);addStat(happy,2);changeBond(1);if(score>=14&&!(seasonalRewardMask&(1U<<event))){seasonalRewardMask|=(1U<<event);outfitOwned|=(1UL<<(7+event));discoveredOutfits|=(1UL<<(7+event));popup(F("Event reward!"),F("Season hat"),750);}else popup(F("Event complete"),F("Daily reward saved"),700);markSaveDirty();}
    delay(20);
  }
}

const __FlashStringHelper* partnerName(byte id) {
  switch(id%PARTNER_COUNT){case 0:return F("Mochi");case 1:return F("Lumi");case 2:return F("Piko");case 3:return F("Nori");case 4:return F("Kumo");case 5:return F("Bibi");case 6:return F("Toto");default:return F("Mimi");}
}

byte partnerCompatibility(byte id) {
  int score=40+((birthCode+id*17+temperament*9+hobbyType*7+habitat*5)%36); score+=ownerBond/10; score+=skillKind/10; score-=careMistakes/3;
  if (score < 0) score = 0;
  if (score > 100) score = 100;
  return (byte)score;
}

void preparePartnerInheritance() {
  if (partnerId >= PARTNER_COUNT) return;
  const byte partnerSpecies = cityWorld.residentSpecies(partnerId);
  const byte partnerBody = partnerSpecies % 12;
  const byte partnerTemper = (partnerSpecies + partnerId * 3) % 8;
  const byte partnerGame = (partnerSpecies + partnerId * 2) % 13;

  // Each generation takes some traits directly from a parent and blends
  // others. The result is deterministic for this family line but changes
  // across generations.
  inheritedBodyType = ((birthCode + generation) & 1U) ? bodyType : partnerBody;
  inheritedTemperament = (byte)((temperament + partnerTemper + generation) % 8);
  inheritedFavoriteGame = (byte)((favoriteGameId + partnerGame + generation) % 13);
  inheritedTrait = (byte)((dominantHobby() + partnerSpecies + generation) % 4);
}

void familyPage() {
  if (stage < STAGE_ADULT) {
    popup(F("Grow up first"), F("Family unlocks as adult"));
    return;
  }

  if (partnerId == 255) {
    popup(F("Meet someone in town"), F("Friendship 60, then date"));
    cityWorld.visitResidents();
    return;
  }
  const byte match = partnerCompatibility(partnerId);
  familyReady = GameplayProgress::familyReady(stage, partnerBond) ? 1 : 0;

  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;

    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(39, 0); display.print(F("FAMILY"));
    display.setCursor(4, 13); display.print(partnerName(partnerId));
    if (cityWorld.partnerLivesAtHome(partnerId, partnerBond)) {
      display.setCursor(73, 13); display.print(F("MARRIED"));
    } else {
      display.setCursor(79, 13); display.print(F("DATING"));
    }

    drawCurrentPetTiny(28, 29 + ((anim / 5) & 1));
    cityWorld.drawPartnerAtHome(partnerId, 83, 29 + ((anim / 6) & 1), (anim / 7) & 1);
    display.drawPixel(62, 34, WHITE);
    display.drawPixel(65, 34, WHITE);
    display.drawPixel(61, 35, WHITE);
    display.drawPixel(63, 35, WHITE);
    display.drawPixel(64, 35, WHITE);
    display.drawPixel(66, 35, WHITE);
    display.drawPixel(64, 37, WHITE);

    char leftStatus[12];
    char rightStatus[12];
    snprintf(leftStatus, sizeof(leftStatus), "BOND %u", partnerBond);
    snprintf(rightStatus, sizeof(rightStatus), "MATCH %u", match);
    display.setCursor(1, 56); display.print(leftStatus);
    display.setCursor(127 - (int)strlen(rightStatus) * 6, 56);
    display.print(rightStatus);

    if (familyReady) {
      display.fillRoundRect(34, 44, 60, 9, 3, BLACK);
      display.drawRoundRect(34, 44, 60, 9, 3, WHITE);
      display.setCursor(42, 45); display.print(F("NEW EGG"));
    }
    display.display();

    if (familyReady && digitalRead(BTN_S) == LOW) {
      waitButtonRelease();
      if (confirmChoice(F("Raise a new egg?"), F("Parents enter album"))) {
        preparePartnerInheritance();
        deathReason = 4;
        archiveCurrentPet();
        chooseEgg();
        startNewPet();
        saveGameNow();
      }
      return;
    }
    delay(30);
  }
}

void diaryPage() {
  byte page=0; bool ol=HIGH,orr=HIGH; waitButtonRelease();
  while(true){
    if(rightHoldExitCheck()) return;
    bool l=digitalRead(BTN_L),r=digitalRead(BTN_R);
    if(ol==HIGH&&l==LOW){page=(page==0)?5:page-1;clickSound();delay(120);}
    if(orr==HIGH&&r==LOW){page=(page+1)%6;clickSound();delay(120);}
    ol=l;orr=r;
    display.clearDisplay();display.setTextSize(1);display.setTextColor(WHITE);
    display.setCursor(34,0);display.print(F("Pet Diary"));drawRightScore(page+1,6);
    if(page==0){display.setCursor(4,16);display.print(F("Day "));display.print(petAgeDays());display.print(F(": I live in"));display.setCursor(4,29);display.print(habitatName(habitat));display.setCursor(4,42);display.print(F("with my favorite human."));}
    else if(page==1){display.setCursor(4,16);display.print(F("Food memory:"));display.setCursor(4,29);if(learnedFavoriteFood<FOOD_COUNT){display.print(F("I learned to love food #"));display.print(learnedFavoriteFood+1);}else display.print(F("I am still tasting things."));display.setCursor(4,42);if(learnedDislikedFood<FOOD_COUNT)display.print(F("I want more variety."));else display.print(F("Meals feel special."));}
    else if(page==2){display.setCursor(4,16);display.print(F("Play memory:"));display.setCursor(4,29);display.print(F("My favorite is "));display.print(gameName(favoriteGameId));display.setCursor(4,42);display.print(F("We played it "));display.print(gameCounts[favoriteGameId]);display.print(F(" times."));}
    else if(page==3){display.setCursor(4,16);display.print(F("Travel memory:"));display.setCursor(4,29);if(visitedLocations) {display.print(F("I remember "));display.print(biomeName(favoritePlace));} else display.print(F("I dream of an adventure."));display.setCursor(4,42);display.print(F("Trips made us closer."));}
    else if(page==4){display.setCursor(4,16);display.print(F("Care memory:"));display.setCursor(4,29);if(rescuedFromSickness)display.print(F("You helped me get well."));else if(runawayCount)display.print(F("I came back home."));else display.print(F("You check on me daily."));display.setCursor(4,42);display.print(F("Trust "));display.print(ownerTrust);display.print(F("  Bond "));display.print(ownerBond);}
    else{display.setCursor(4,16);display.print(F("Family memory:"));display.setCursor(4,29);if(partnerId<PARTNER_COUNT){display.print(F("I met "));display.print(partnerName(partnerId));}else display.print(F("My story is still growing."));display.setCursor(4,42);display.print(F("Generation "));display.print(generation);}
    display.setCursor(0,56);display.print(F("D0/D2 pages Hold exit"));display.display();delay(20);
  }
}

void careHistoryPage() {
  byte page=0; waitButtonRelease();
  while(true){if(rightHoldExitCheck())return;bool l=digitalRead(BTN_L),r=digitalRead(BTN_R);if(l==LOW&&careHistoryCount>0){page=(page==0)?careHistoryCount-1:page-1;clickSound();waitButtonRelease();}if(r==LOW){if(page+1<careHistoryCount)page++;clickSound();waitButtonRelease();}display.clearDisplay();display.setTextSize(1);display.setTextColor(WHITE);display.setCursor(30,0);display.print(F("Care History"));if(careHistoryCount==0){display.setCursor(14,28);display.print(F("No care events yet"));}else{byte oldest=(careHistoryHead+CARE_HISTORY_COUNT-careHistoryCount)%CARE_HISTORY_COUNT;byte idx=(oldest+page)%CARE_HISTORY_COUNT;display.setCursor(8,18);display.print(careEventName(careHistoryType[idx]));display.setCursor(8,31);display.print(F("Day "));display.print(careHistoryDay[idx]);display.print(F("  Hour "));display.print(careHistoryHour[idx]);display.setCursor(8,44);display.print(page+1);display.print(F("/"));display.print(careHistoryCount);}display.setCursor(0,56);display.print(F("D0/D2 browse Hold exit"));display.display();delay(20);}
}

void inspectPage() {
  byte page = 0;
  bool oldL = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(BTN_L);
    const bool r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) { page = page == 0 ? 3 : page - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { page = (page + 1) % 4; clickSound(); delay(100); }
    oldL = l; oldR = r;

    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(38, 0); display.print(F("INSPECT"));
    drawRightScore(page + 1, 4);

    if (page == 0) {
      display.setCursor(0, 15); display.print(F("World"));
      display.setCursor(42, 15); display.print(habitatName(habitat));
      display.setCursor(0, 29); display.print(F("Event"));
      display.setCursor(42, 29); display.print(seasonEventName(currentSeasonEvent()));
      display.setCursor(0, 43); display.print(F("Weather"));
      display.setCursor(54, 43); display.print(visualWeather());
    } else if (page == 1) {
      display.setCursor(0, 15); display.print(F("Room"));
      display.setCursor(42, 15); display.print(roomName(roomStyle));
      display.setCursor(0, 29); display.print(F("Garden"));
      display.setCursor(48, 29);
      if (gardenPlant == 255) display.print(F("empty")); else display.print(plantName(gardenPlant));
      display.setCursor(0, 43); display.print(F("Mess"));
      display.setCursor(48, 43); display.print((10 - clean) + poop);
    } else if (page == 2) {
      display.setCursor(0, 15); display.print(F("Pet"));
      display.setCursor(36, 15); display.print(petName);
      display.setCursor(0, 29); display.print(F("Mood"));
      display.setCursor(42, 29); display.print(happy);
      display.setCursor(72, 29); display.print(F("Hobby"));
      display.setCursor(114, 29); display.print(hobbyType);
      display.setCursor(0, 43); display.print(F("Favorite food"));
      display.setCursor(90, 43);
      if (learnedFavoriteFood < FOOD_COUNT) display.print(learnedFavoriteFood + 1);
      else display.print(F("?"));
    } else {
      display.setCursor(0, 15); display.print(F("Health")); display.setCursor(48, 15); display.print(health);
      display.setCursor(72, 15); display.print(F("Food")); display.setCursor(108, 15); display.print(full);
      display.setCursor(0, 29); display.print(F("Energy")); display.setCursor(48, 29); display.print(energy);
      display.setCursor(72, 29); display.print(F("Clean")); display.setCursor(108, 29); display.print(clean);
      display.setCursor(0, 43); display.print(F("Condition")); display.setCursor(66, 43); display.print(conditionShort());
    }
    display.display();
    delay(20);
  }
}

void tamaBookPage() {
  byte section = 0;
  byte item = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();

  while (true) {
    if (rightHoldExitCheck()) return;
    const byte maxItem = section == 0 ? SPECIES_COUNT
                         : section == 1 ? BOTANY_CROP_COUNT
                         : section == 2 ? BOTANY_INDOOR_COUNT
                         : section == 3 ? OUTFIT_COUNT
                         : section == 4 ? ROOM_STYLE_COUNT
                         : section == 5 ? 9
                         : CRAFT_RECIPE_COUNT;
    const bool l = digitalRead(BTN_L);
    const bool sBtn = digitalRead(BTN_S);
    const bool r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) {
      item = item == 0 ? maxItem - 1 : item - 1;
      clickSound(); delay(100);
    }
    if (oldR == HIGH && r == LOW) {
      item = (item + 1) % maxItem;
      clickSound(); delay(100);
    }
    if (oldS == HIGH && sBtn == LOW) {
      section = (section + 1) % 7;
      item = 0;
      clickSound(); delay(100);
    }
    oldL = l; oldS = sBtn; oldR = r;

    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0); display.print(F("TAMA BOOK"));
    display.setCursor(104, 0); display.print(item + 1); display.print(F("/")); display.print(maxItem);

    if (section == 0) {
      const bool known = speciesDiscovered(item);
      display.setCursor(0, 12); display.print(F("PETS"));
      display.setCursor(88, 12); display.print(countDiscoveredSpecies()); display.print(F("/")); display.print(SPECIES_COUNT);
      if (known) drawSprite16(item, 56, 24);
      else { display.drawRoundRect(54, 22, 20, 24, 4, WHITE); display.setCursor(61, 30); display.print(F("?")); }
      display.setCursor(0, 53); display.print(known ? speciesNameById(item) : evolutionHint(item));
    } else if (section == 1) {
      const bool known = botany.cropDiscovered(item);
      display.setCursor(0, 12); display.print(F("GARDEN CROPS"));
      display.setCursor(88, 12); display.print(botany.discoveredCropCount()); display.print(F("/")); display.print(BOTANY_CROP_COUNT);
      if (known) botany.drawCropCatalogSprite(item, 64, 48, 4);
      else { display.drawCircle(64, 37, 7, WHITE); display.setCursor(61, 33); display.print(F("?")); }
      display.setCursor(0, 53); display.print(known ? botany.cropDisplayName(item) : F("Undiscovered seed"));
    } else if (section == 2) {
      const bool known = botany.indoorDiscovered(item);
      display.setCursor(0, 12); display.print(F("HOUSE PLANTS"));
      display.setCursor(88, 12); display.print(botany.discoveredIndoorCount()); display.print(F("/")); display.print(BOTANY_INDOOR_COUNT);
      if (known) botany.drawIndoorCatalogSprite(item, 64, 48, 4);
      else { display.drawRoundRect(58, 40, 12, 7, 2, WHITE); display.setCursor(61, 27); display.print(F("?")); }
      display.setCursor(0, 53); display.print(known ? botany.indoorDisplayName(item) : F("Unknown house plant"));
    } else if (section == 3) {
      const bool known = ((discoveredOutfits | outfitOwned) & (1UL << item)) != 0;
      display.setCursor(0, 12); display.print(F("OUTFITS"));
      display.setCursor(88, 12); display.print(countBits16((unsigned int)(discoveredOutfits | outfitOwned))); display.print(F("/")); display.print(OUTFIT_COUNT);
      display.drawRoundRect(45, 23, 38, 24, 5, WHITE);
      display.setCursor(0, 53); display.print(known ? outfitName(item) : F("Unknown hat"));
    } else if (section == 4) {
      const bool known = ((discoveredRooms | roomOwned) & (1UL << item)) != 0;
      display.setCursor(0, 12); display.print(F("ROOMS"));
      display.setCursor(88, 12); display.print(countBits16((unsigned int)(discoveredRooms | roomOwned))); display.print(F("/")); display.print(ROOM_STYLE_COUNT);
      display.drawRect(39, 23, 50, 25, WHITE);
      display.drawLine(39, 23, 64, 15, WHITE); display.drawLine(89, 23, 64, 15, WHITE);
      display.setCursor(0, 53); display.print(known ? roomName(item) : F("Unknown room"));
    } else if (section == 5) {
      const bool known = (visitedLocations & (1U << item)) != 0;
      display.setCursor(0, 12); display.print(F("PLACES"));
      display.setCursor(88, 12); display.print(countBits16(visitedLocations)); display.print(F("/9"));
      display.drawCircle(64, 34, 10, WHITE); display.drawLine(54, 34, 74, 34, WHITE);
      display.setCursor(0, 53); display.print(known ? biomeName(item) : F("Unvisited place"));
    } else {
      const bool known = (recipesCrafted & (1U << item)) != 0;
      display.setCursor(0, 12); display.print(F("RECIPES"));
      display.setCursor(88, 12); display.print(countBits16(recipesCrafted)); display.print(F("/")); display.print(CRAFT_RECIPE_COUNT);
      display.drawRect(53, 21, 22, 27, WHITE); display.drawLine(57, 27, 71, 27, WHITE);
      display.setCursor(0, 53); display.print(known ? craftRecipeName(item) : F("Unknown recipe"));
    }
    display.display();
    delay(20);
  }
}

// ---------- Paradise-inspired world systems ----------

byte planetLevel() {
  byte level = 0;
  if (stage >= STAGE_CHILD) level = 1;
  if (stage >= STAGE_TEEN) level = 2;
  if (stage >= STAGE_ADULT) level = 3;
  if (generation >= 2) level = max(level, (byte)4);
  if (countBits16(habitatOwned) >= 2) level = max(level, (byte)5);
  const byte raised = min((byte)6, (byte)(albumCount + ((stage >= STAGE_ADULT && !dead) ? 1 : 0)));
  if (raised >= 2) level = max(level, (byte)6);
  if (raised >= 3) level = max(level, (byte)7);
  if (raised >= 4) level = max(level, (byte)8);
  if (raised >= 5) level = max(level, (byte)9);
  if (raised >= 6) level = 10;
  return level;
}

const __FlashStringHelper *weatherName(byte weather) {
  if (weather == 1) return F("Rain");
  if (weather == 2) return F("Snow");
  if (weather == 3) return F("Wind");
  if (weather == 4) return F("Sparkles");
  return F("Clear");
}

const __FlashStringHelper *outdoorToyName(byte toy) {
  switch (toy % PARADISE_TOY_COUNT) {
    case 0: return F("Play Ball");
    case 1: return F("Balance Toy");
    case 2: return F("Parasol");
    case 3: return F("Flower Stand");
    case 4: return F("Telescope");
    case 5: return F("Water Sprayer");
    case 6: return F("Music Stage");
    default: return F("Planet Statue");
  }
}

byte outdoorToyCost(byte toy) {
  static const byte costs[PARADISE_TOY_COUNT] = {0, 8, 10, 12, 15, 14, 18, 24};
  byte cost = costs[toy % PARADISE_TOY_COUNT];
  const byte saleDay = (byte)(petAgeDays() % 30UL) + 1;
  if (saleDay == 10 || saleDay == 20 || saleDay == 30) cost = max((byte)1, (byte)((cost + 1) / 2));
  return cost;
}

void drawOutdoorToy(byte toy, int x, int y) {
  switch (toy % PARADISE_TOY_COUNT) {
    case 0:
      display.drawCircle(x + 7, y + 7, 6, WHITE);
      display.drawLine(x + 2, y + 7, x + 12, y + 7, WHITE);
      display.drawLine(x + 7, y + 2, x + 7, y + 12, WHITE);
      break;
    case 1:
      display.drawLine(x + 1, y + 12, x + 13, y + 12, WHITE);
      display.drawLine(x + 7, y + 12, x + 7, y + 3, WHITE);
      display.drawLine(x + 2, y + 6, x + 12, y + 6, WHITE);
      display.drawCircle(x + 2, y + 6, 2, WHITE);
      display.drawCircle(x + 12, y + 6, 2, WHITE);
      break;
    case 2:
      display.drawTriangle(x, y + 7, x + 14, y + 7, x + 7, y, WHITE);
      display.drawLine(x + 7, y + 7, x + 7, y + 14, WHITE);
      break;
    case 3:
      display.drawRect(x + 2, y + 9, 11, 5, WHITE);
      display.drawLine(x + 7, y + 9, x + 7, y + 3, WHITE);
      display.drawCircle(x + 7, y + 2, 3, WHITE);
      display.drawCircle(x + 3, y + 5, 3, WHITE);
      display.drawCircle(x + 11, y + 5, 3, WHITE);
      break;
    case 4:
      display.drawLine(x + 2, y + 12, x + 12, y + 2, WHITE);
      display.drawRoundRect(x + 7, y, 7, 4, 2, WHITE);
      display.drawLine(x + 7, y + 7, x + 4, y + 14, WHITE);
      display.drawLine(x + 7, y + 7, x + 11, y + 14, WHITE);
      break;
    case 5:
      display.drawRoundRect(x + 2, y + 7, 10, 6, 2, WHITE);
      display.drawLine(x + 12, y + 8, x + 15, y + 5, WHITE);
      display.drawPixel(x + 15, y + 2, WHITE);
      display.drawPixel(x + 12, y + 1, WHITE);
      break;
    case 6:
      display.drawRect(x + 1, y + 8, 14, 6, WHITE);
      display.drawLine(x + 5, y + 8, x + 5, y + 2, WHITE);
      display.fillCircle(x + 3, y + 2, 2, WHITE);
      display.drawLine(x + 11, y + 8, x + 11, y, WHITE);
      display.fillCircle(x + 9, y + 1, 2, WHITE);
      break;
    default:
      display.drawCircle(x + 7, y + 7, 6, WHITE);
      display.drawLine(x + 7, y + 1, x + 7, y + 13, WHITE);
      display.drawLine(x + 1, y + 7, x + 13, y + 7, WHITE);
      display.drawPixel(x + 7, y - 1, WHITE);
      break;
  }
}

const __FlashStringHelper *missionName(byte mission) {
  switch (mission % PARADISE_MISSION_COUNT) {
    case 0: return F("Raise a Child");
    case 1: return F("Raise an Adult");
    case 2: return F("Start Gen 2");
    case 3: return F("Find 5 Species");
    case 4: return F("Unlock 3 Fields");
    case 5: return F("Visit 6 Places");
    case 6: return F("Reach Bond 75");
    case 7: return F("Harvest 10 Crops");
    case 8: return F("Cook 8 Meals");
    case 9: return F("Keep 3 Fish");
    case 10: return F("Own 8 Outfits");
    default: return F("Play 20 Games");
  }
}

unsigned int missionTarget(byte mission) {
  static const unsigned int targets[PARADISE_MISSION_COUNT] = {1,1,2,5,3,6,75,10,8,3,8,20};
  return targets[mission % PARADISE_MISSION_COUNT];
}

unsigned int missionProgress(byte mission) {
  mission %= PARADISE_MISSION_COUNT;
  if (mission == 0) return stage >= STAGE_CHILD ? 1 : 0;
  if (mission == 1) return stage >= STAGE_ADULT ? 1 : 0;
  if (mission == 2) return generation;
  if (mission == 3) return countDiscoveredSpecies();
  if (mission == 4) return countBits16(habitatOwned);
  if (mission == 5) return countBits16(visitedLocations);
  if (mission == 6) return ownerBond;
  if (mission == 7) return gardenHarvests;
  if (mission == 8) return cookedMeals;
  if (mission == 9) return aquarium.fishCount();
  if (mission == 10) return countBits16((unsigned int)outfitOwned);
  unsigned int total = 0;
  for (byte i = 0; i < 13; ++i) total += gameCounts[i];
  return total;
}

bool missionComplete(byte mission) {
  return missionProgress(mission) >= missionTarget(mission);
}

void planetReportPage() {
  byte page = 0;
  bool oldL = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(BTN_L);
    const bool r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) { page = page == 0 ? 3 : page - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { page = (page + 1) % 4; clickSound(); delay(100); }
    oldL = l; oldR = r;

    display.clearDisplay(); display.setTextWrap(false); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(22, 0); display.print(F("PLANET REPORT")); drawRightScore(page + 1, 4);
    if (page == 0) {
      display.drawCircle(64, 33, 17, WHITE);
      display.drawLine(49, 29, 77, 38, WHITE);
      display.drawLine(52, 42, 69, 17, WHITE);
      display.setCursor(0, 14); display.print(F("LEVEL")); display.setCursor(104, 14); display.print(planetLevel());
      display.setCursor(0, 54); display.print(F("Research ")); display.print(paradise.researchPoints());
    } else if (page == 1) {
      display.setCursor(0, 14); display.print(F("Fields")); display.setCursor(88, 14); display.print(countBits16(habitatOwned)); display.print(F("/8"));
      display.setCursor(0, 26); display.print(F("Species")); display.setCursor(88, 26); display.print(countDiscoveredSpecies()); display.print(F("/50"));
      display.setCursor(0, 38); display.print(F("Generations")); display.setCursor(88, 38); display.print(generation);
      display.setCursor(0, 50); display.print(F("Past Tamas")); display.setCursor(88, 50); display.print(albumCount);
    } else if (page == 2) {
      display.setCursor(0, 14); display.print(F("Missions")); display.setCursor(90, 14); display.print(paradise.claimedMissionCount()); display.print(F("/12"));
      display.setCursor(0, 26); display.print(F("Space worlds")); display.setCursor(90, 26); display.print(paradise.visitedDestinationCount()); display.print(F("/8"));
      display.setCursor(0, 38); display.print(F("Emergency W/L")); display.setCursor(90, 38); display.print(paradise.emergencyWins()); display.print(F("/")); display.print(paradise.emergencyLosses());
      display.setCursor(0, 50); display.print(F("Biofuel")); display.setCursor(90, 50); display.print(paradise.biofuel());
    } else {
      display.setCursor(0, 14); display.print(F("Current field"));
      display.setCursor(0, 26); display.print(habitatName(habitat));
      display.setCursor(0, 39); display.print(F("Weather ")); display.print(weatherName(visualWeather()));
      display.setCursor(0, 52); display.print(F("Outside toy "));
      byte toy = paradise.placedToy(habitat);
      display.print(toy < PARADISE_TOY_COUNT ? outdoorToyName(toy) : F("None"));
    }
    display.display(); delay(20);
  }
}

void missionsPage() {
  byte pick = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(BTN_L), sBtn = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? PARADISE_MISSION_COUNT - 1 : pick - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { pick = (pick + 1) % PARADISE_MISSION_COUNT; clickSound(); delay(100); }
    if (oldS == HIGH && sBtn == LOW) {
      waitButtonRelease();
      if (paradise.missionClaimed(pick)) popup(F("Already claimed"), F("Mission complete"));
      else if (!missionComplete(pick)) popup(F("Still in progress"), missionName(pick));
      else {
        paradise.claimMission(pick);
        paradise.addResearch(25);
        addCoins(15);
        winSound(); popup(F("Mission cleared!"), F("15G + research"), 800);
        markSaveDirty();
      }
    }
    oldL = l; oldS = sBtn; oldR = r;

    const unsigned int progress = min(missionProgress(pick), missionTarget(pick));
    const unsigned int target = missionTarget(pick);
    display.clearDisplay(); display.setTextWrap(false); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(32, 0); display.print(F("MISSIONS")); drawRightScore(pick + 1, PARADISE_MISSION_COUNT);
    display.setCursor(4, 16); display.print(missionName(pick));
    display.drawRect(5, 31, 118, 8, WHITE);
    display.fillRect(7, 33, target ? (int)((unsigned long)progress * 114UL / target) : 0, 4, WHITE);
    display.setCursor(5, 44); display.print(progress); display.print(F("/")); display.print(target);
    display.setCursor(60, 44);
    if (paradise.missionClaimed(pick)) display.print(F("CLAIMED"));
    else if (missionComplete(pick)) display.print(F("D1 CLAIM"));
    else display.print(F("IN PROGRESS"));
    display.setCursor(5, 56); display.print(F("Reward 15G + 25 research"));
    display.display(); delay(20);
  }
}

void chooseOutdoorToy() {
  byte pick = paradise.placedToy(habitat);
  if (pick >= PARADISE_TOY_COUNT) pick = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(BTN_L), sBtn = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? PARADISE_TOY_COUNT - 1 : pick - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { pick = (pick + 1) % PARADISE_TOY_COUNT; clickSound(); delay(100); }
    if (oldS == HIGH && sBtn == LOW) {
      waitButtonRelease();
      if (!paradise.toyOwned(pick)) {
        const byte cost = outdoorToyCost(pick);
        if (!spendCoins(cost)) { badSound(); popup(F("Need more coins"), F("Work or play")); }
        else { paradise.unlockToy(pick); paradise.placeToy(habitat, pick); winSound(); popup(F("Toy purchased"), outdoorToyName(pick)); markSaveDirty(); return; }
      } else { paradise.placeToy(habitat, pick); goodSound(); popup(F("Placed outside"), outdoorToyName(pick)); markSaveDirty(); return; }
    }
    oldL = l; oldS = sBtn; oldR = r;

    display.clearDisplay(); display.setTextWrap(false); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(21, 0); display.print(F("OUTSIDE TOYS")); drawRightScore(pick + 1, PARADISE_TOY_COUNT);
    display.drawRoundRect(5, 15, 118, 35, 5, WHITE);
    drawOutdoorToy(pick, 15, 24);
    display.setCursor(42, 22); display.print(outdoorToyName(pick));
    display.setCursor(42, 36);
    if (paradise.toyOwned(pick)) display.print(F("Owned - D1 place"));
    else { display.print(F("Buy ")); display.print(outdoorToyCost(pick)); display.print(F("G")); }
    display.setCursor(0, 56); display.print(F("Field: ")); display.print(habitatName(habitat));
    display.display(); delay(20);
  }
}

void fieldLifePage() {
  byte pick = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(BTN_L), sBtn = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? 2 : pick - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { pick = (pick + 1) % 3; clickSound(); delay(100); }
    if (oldS == HIGH && sBtn == LOW) {
      waitButtonRelease();
      if (pick == 0) habitatPage();
      else if (pick == 1) chooseOutdoorToy();
      else if (paradise.fieldExpanded(habitat)) popup(F("Already expanded"), habitatName(habitat));
      else if (planetLevel() < 10) popup(F("Planet Lv.10 needed"), F("Raise more generations"));
      else if (coins < 50 || paradise.researchPoints() < 50) popup(F("Need 50G + 50 research"), F("Missions fund research"));
      else { spendCoins(50); paradise.spendResearch(50); paradise.expandField(habitat); winSound(); popup(F("Field expanded!"), F("Toy effects now doubled")); markSaveDirty(); }
    }
    oldL = l; oldS = sBtn; oldR = r;

    display.clearDisplay(); display.setTextWrap(false); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(30, 0); display.print(F("FIELD LIFE")); drawRightScore(pick + 1, 3);
    display.setCursor(4, 15); display.print(habitatName(habitat));
    display.setCursor(4, 27); display.print(F("Weather: ")); display.print(weatherName(visualWeather()));
    byte toy = paradise.placedToy(habitat);
    display.setCursor(4, 39); display.print(F("Toy: ")); display.print(toy < PARADISE_TOY_COUNT ? outdoorToyName(toy) : F("None"));
    display.setCursor(4, 53);
    if (pick == 0) display.print(F("D1 change field"));
    else if (pick == 1) display.print(F("D1 place outside toy"));
    else display.print(paradise.fieldExpanded(habitat) ? F("Expanded permanently") : F("Expand: 50G + 50 R"));
    display.display(); delay(20);
  }
}

void eggHuntPage() {
  const unsigned long day = petAgeDays();
  if (paradise.lastEggHuntDay() == day) { popup(F("Egg hunt used today"), F("Try again next day")); return; }
  byte area = habitat == 3 ? 1 : 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(BTN_L), sBtn = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) { area ^= 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { area ^= 1; clickSound(); delay(100); }
    if (oldS == HIGH && sBtn == LOW) break;
    oldL = l; oldS = sBtn; oldR = r;

    const byte team = min((byte)3, (byte)(1 + albumCount));
    display.clearDisplay(); display.setTextWrap(false); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(34, 0); display.print(F("EGG HUNT"));
    display.setCursor(6, 15); display.print(area == 0 ? F("Volcano launch") : F("Crystal spring"));
    display.setCursor(6, 28); display.print(F("Team size ")); display.print(team); display.print(F("/3"));
    display.setCursor(6, 41); display.print(area == 0 ? F("Food + rare spice") : F("Food + shells"));
    display.setCursor(6, 55); display.print(F("D1 begin daily hunt"));
    display.display(); delay(20);
  }
  waitButtonRelease();

  const byte team = min((byte)3, (byte)(1 + albumCount));
  byte eggs = 0;
  for (byte frame = 0; frame < 24; ++frame) {
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(0, 0); display.print(area == 0 ? F("VOLCANO HUNT") : F("SPRING HUNT"));
    if (area == 0) {
      display.drawTriangle(25, 55, 62, 17, 99, 55, WHITE);
      display.drawLine(50, 35, 75, 35, WHITE);
    } else {
      display.drawCircle(64, 40, 20, WHITE);
      display.drawLine(44, 42, 84, 42, WHITE);
    }
    for (byte t = 0; t < team; ++t) drawCurrentPetTiny(6 + t * 18, 39 + ((frame + t) & 1));
    const int ex = 103 - (frame * 4 % 96);
    const int ey = 17 + ((frame * 7) % 25);
    display.drawCircle(ex, ey, 3, WHITE);
    if ((frame % 6) == 0) ++eggs;
    display.setCursor(103, 0); display.print(min((byte)5, eggs));
    display.display(); delay(75);
  }

  eggs = min((byte)5, (byte)(2 + team + random(0, 2)));
  if (area == 0) {
    berryInv = min((byte)99, (byte)(berryInv + eggs));
    if (eggs >= 4) spiceInv = min((byte)99, (byte)(spiceInv + 1));
  } else {
    herbInv = min((byte)99, (byte)(herbInv + eggs));
    addMaterial(5, max((byte)1, (byte)(eggs / 2)));
  }
  paradise.setLastEggHuntDay(day);
  paradise.addResearch(5 + eggs);
  addStat(happy, 2); subStat(energy, 1); changeBond(1);
  winSound(); popup(F("Eggs opened!"), area == 0 ? F("Fresh foods found") : F("Herbs and shells"), 850);
  markSaveDirty();
}

const __FlashStringHelper *spaceDestinationName(byte destination) {
  switch (destination % PARADISE_DESTINATION_COUNT) {
    case 0: return F("Sweet Planet");
    case 1: return F("Relax Planet");
    case 2: return F("Foodie Planet");
    case 3: return F("Art Planet");
    case 4: return F("Ninja Planet");
    case 5: return F("Eco Planet");
    case 6: return F("Music Planet");
    default: return F("Ice Planet");
  }
}

byte spaceTravelCost(byte destination) { return 8 + (destination % PARADISE_DESTINATION_COUNT) * 2 - lifeWorld.projectLevel(5) * 2; }

void spaceTravelPage() {
  byte pick = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(BTN_L), sBtn = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? PARADISE_DESTINATION_COUNT - 1 : pick - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { pick = (pick + 1) % PARADISE_DESTINATION_COUNT; clickSound(); delay(100); }
    if (oldS == HIGH && sBtn == LOW) {
      waitButtonRelease();
      const byte cost = spaceTravelCost(pick);
      if (!paradise.spendBiofuel(cost)) { badSound(); popup(F("Not enough biofuel"), F("Clean poop to refill")); }
      else {
        for (byte frame = 0; frame < 24; ++frame) {
          display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
          for (byte i = 0; i < 18; ++i) display.drawPixel((i * 37 + frame * 3) % 128, (i * 19 + frame) % 54, WHITE);
          const int rx = 5 + frame * 5;
          display.drawTriangle(rx, 30, rx + 15, 24, rx + 15, 36, WHITE);
          display.drawRect(rx + 15, 26, 9, 8, WHITE);
          display.drawLine(rx, 30, max(0, rx - 8), 30, WHITE);
          display.setCursor(0, 56); display.print(spaceDestinationName(pick));
          display.display(); delay(65);
        }
        const bool first = !paradise.destinationVisited(pick);
        paradise.markDestinationVisited(pick);
        paradise.addResearch(first ? 20 : 5);
        if (first) {
          if ((pick & 1) == 0) {
            const byte unlock = 1 + pick;
            outfitOwned |= (1UL << min((byte)(OUTFIT_COUNT - 1), unlock));
            discoveredOutfits |= (1UL << min((byte)(OUTFIT_COUNT - 1), unlock));
          } else {
            const byte unlock = 1 + (pick % (ROOM_STYLE_COUNT - 1));
            roomOwned |= (1UL << unlock);
            discoveredRooms |= (1UL << unlock);
          }
          popup(F("New planet gift!"), spaceDestinationName(pick), 850);
        } else {
          addCoins(3 + pick / 2); addMaterial(pick % MATERIAL_COUNT, 1);
          popup(F("Return-trip gift"), F("Coins + material"), 750);
        }
        if (tripBadges < 255) tripBadges++;
        addStat(happy, 3); subStat(energy, 1); changeBond(1); winSound(); markSaveDirty();
      }
    }
    oldL = l; oldS = sBtn; oldR = r;

    display.clearDisplay(); display.setTextWrap(false); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(24, 0); display.print(F("SPACE TRAVEL")); drawRightScore(pick + 1, PARADISE_DESTINATION_COUNT);
    display.drawCircle(30, 33, 13, WHITE); display.drawLine(17, 33, 43, 33, WHITE);
    display.setCursor(52, 20); display.print(spaceDestinationName(pick));
    display.setCursor(52, 33); display.print(F("Fuel ")); display.print(spaceTravelCost(pick));
    display.setCursor(52, 46); display.print(paradise.destinationVisited(pick) ? F("Visited") : F("New gift"));
    display.setCursor(0, 56); display.print(F("Tank ")); display.print(paradise.biofuel()); display.print(F("  D1 launch"));
    display.display(); delay(20);
  }
}

bool meteorEmergencyGame() {
  byte score = 0;
  waitButtonRelease();
  for (byte round = 0; round < 12; ++round) {
    const byte lane = random(0, 3);
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(21, 0); display.print(F("METEOR ALERT"));
    display.drawCircle(64, 40, 15, WHITE);
    int mx = lane == 0 ? 9 : lane == 1 ? 61 : 113;
    display.fillCircle(mx, 18, 4, WHITE);
    display.drawLine(mx, 12, mx + (lane == 0 ? 8 : lane == 2 ? -8 : 0), 4, WHITE);
    display.setCursor(0, 56); display.print(F("D0 left D1 mid D2 right")); display.display();
    unsigned long started = millis(); bool hit = false;
    while (millis() - started < max(280UL, 720UL - (unsigned long)round * 25UL)) {
      if ((lane == 0 && digitalRead(BTN_L) == LOW) ||
          (lane == 1 && digitalRead(BTN_S) == LOW) ||
          (lane == 2 && digitalRead(BTN_R) == LOW)) { hit = true; break; }
      delay(3);
    }
    waitButtonRelease();
    if (hit) { ++score; goodSound(); } else badSound();
  }
  return score >= 8;
}

bool stampedeEmergencyGame() {
  byte score = 0;
  waitButtonRelease();
  for (byte round = 0; round < 10; ++round) {
    const bool hide = random(0, 3) != 0;
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(18, 0); display.print(F("STAMPEDE ALERT"));
    for (byte i = 0; i < 5; ++i) {
      const int x = 6 + i * 24;
      display.drawCircle(x + 6, 31, 5, WHITE);
      display.drawLine(x + 2, 36, x, 44, WHITE);
      display.drawLine(x + 10, 36, x + 12, 44, WHITE);
    }
    display.setCursor(42, 50); display.print(hide ? F("HIDE! D1") : F("WAIT!")); display.display();
    unsigned long started = millis(); bool pressed = false;
    while (millis() - started < 650UL) { if (digitalRead(BTN_S) == LOW) { pressed = true; break; } delay(3); }
    waitButtonRelease();
    if (pressed == hide) { ++score; goodSound(); } else badSound();
  }
  return score >= 7;
}

void emergencyPage() {
  if (!paradise.emergencyPending()) {
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(24, 0); display.print(F("PLANET SAFETY"));
    display.drawCircle(64, 30, 15, WHITE); display.drawLine(53, 20, 75, 41, WHITE);
    display.setCursor(12, 51); display.print(F("No emergency right now")); display.display();
    waitButtonRelease();
    unsigned long start = millis();
    while (millis() - start < 1200UL && !anyButtonDown()) delay(10);
    waitButtonRelease();
    return;
  }

  popup(F("Emergency incoming!"), paradise.emergencyType() == 0 ? F("Meteorites") : F("Stampede"), 700);
  const bool success = paradise.emergencyType() == 0 ? meteorEmergencyGame() : stampedeEmergencyGame();
  paradise.resolveEmergency(success);
  if (success) {
    addCoins(12); paradise.addResearch(20); addStat(happy, 2); changeBond(2);
    winSound(); popup(F("Planet protected!"), F("12G + research"), 850);
  } else {
    paradise.placeToy(habitat, 255);
    subStat(health, 2); subStat(happy, 2); subStat(clean, 2);
    if (conditionType == 0) setCondition(paradise.emergencyType() == 0 ? 4 : 3, 1);
    badSound(); popup(F("Planet was damaged"), F("Toy stored; tend pet"), 900);
  }
  clampAll(); markSaveDirty();
}

void maybeScheduleParadiseEmergency() {
  const unsigned long day = petAgeDays();
  if (planetLevel() < 3 || paradise.emergencyPending() || paradise.emergencyDay() == day) return;
  const byte dayInMonth = (byte)(day % 30UL) + 1;
  if (dayInMonth == 9 || dayInMonth == 19 || dayInMonth == 29) paradise.scheduleEmergency(day, 0);
  else if (dayInMonth == 13 && planetLevel() >= 6) paradise.scheduleEmergency(day, 1);
  else if (random(0, 18) == 0) paradise.scheduleEmergency(day, random(0, 2));
}

void applyParadiseDailyEffects() {
  const byte toy = paradise.placedToy(habitat);
  const byte repeats = paradise.fieldExpanded(habitat) ? 2 : 1;
  for (byte i = 0; i < repeats; ++i) {
    if (toy == 0 || toy == 1) { addStat(happy, 1); addSkill(skillFit, 1); }
    else if (toy == 2 || toy == 5) { addStat(clean, 1); addStat(happy, 1); }
    else if (toy == 3) { addSkill(skillKind, 1); addSkill(skillStyle, 1); }
    else if (toy == 4) addSkill(skillSmart, 1);
    else if (toy == 6) { addSkill(skillStyle, 1); addStat(happy, 1); }
    else if (toy == 7) { addStat(happy, 1); paradise.addResearch(1); }
  }
}

void connectionLabPage() {
  const unsigned long day = petAgeDays();
  paradise.beginConnectionDay(day);
  byte pick = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(BTN_L), sBtn = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? CITY_RESIDENT_COUNT - 1 : pick - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { pick = (pick + 1) % CITY_RESIDENT_COUNT; clickSound(); delay(100); }
    if (oldS == HIGH && sBtn == LOW) {
      waitButtonRelease();
      if (!paradise.useConnection()) { popup(F("3 links used today"), F("Try tomorrow")); }
      else {
        for (byte frame = 0; frame < 18; ++frame) {
          display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
          display.drawCircle(64, 34, 24, WHITE);
          drawCurrentPetTiny(31 + min((byte)18, frame), 30);
          cityWorld.drawResidentTiny(pick, 81 - min((byte)18, frame), 30, frame & 1);
          display.setCursor(31, 0); display.print(F("MOON LINK")); display.display(); delay(70);
        }
        const int chance = 35 + cityWorld.relation(pick) / 2 + ownerBond / 4 + happy * 2 - (10 - full);
        const bool success = random(0, 100) < constrain(chance, 15, 95);
        if (success) {
          if (friendCount < 255) friendCount++;
          addCoins(4); addStat(happy, 2); changeBond(1); paradise.addResearch(4);
          if (partnerId == pick) partnerBond = min((byte)100, (byte)(partnerBond + 4));
          goodSound(); popup(F("Great connection!"), F("Friendship grew"));
        } else {
          mischief = min((byte)99, (byte)(mischief + 1)); subStat(happy, 1);
          badSound(); popup(F("A strange prank!"), F("Try again later"));
        }
        markSaveDirty();
      }
    }
    oldL = l; oldS = sBtn; oldR = r;

    display.clearDisplay(); display.setTextWrap(false); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(26, 0); display.print(F("CONNECTION LAB")); drawRightScore(pick + 1, CITY_RESIDENT_COUNT);
    drawCurrentPetTiny(19, 25); cityWorld.drawResidentTiny(pick, 90, 25, (anim / 4) & 1);
    display.drawLine(48, 33, 80, 33, WHITE); display.drawPixel(62, 31, WHITE); display.drawPixel(66, 35, WHITE);
    display.setCursor(4, 49); display.print(cityWorld.residentName(pick));
    display.setCursor(4, 57); display.print(F("Links left ")); display.print(3 - paradise.connectionUses()); display.print(F("  D1 dock"));
    display.display(); delay(20);
  }
}

bool cellTreatmentGame() {
  byte cursor = 1;
  byte hits = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  for (byte round = 0; round < 12; ++round) {
    const byte virus = random(0, 3);
    const unsigned long started = millis();
    bool shot = false;
    while (millis() - started < 900UL) {
      const bool l = digitalRead(BTN_L), sBtn = digitalRead(BTN_S), r = digitalRead(BTN_R);
      if (oldL == HIGH && l == LOW && cursor > 0) { --cursor; clickSound(); }
      if (oldR == HIGH && r == LOW && cursor < 2) { ++cursor; clickSound(); }
      if (oldS == HIGH && sBtn == LOW) { shot = true; break; }
      oldL = l; oldS = sBtn; oldR = r;
      display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
      display.setCursor(26, 0); display.print(F("CELL TREATMENT"));
      for (byte lane = 0; lane < 3; ++lane) {
        const int x = 16 + lane * 48;
        display.drawLine(x, 12, x, 52, WHITE);
        if (lane == virus) { display.drawCircle(x, 24, 5, WHITE); display.drawPixel(x - 2, 22, WHITE); display.drawPixel(x + 2, 22, WHITE); }
        if (lane == cursor) display.fillTriangle(x - 5, 50, x + 5, 50, x, 41, WHITE);
      }
      display.setCursor(0, 56); display.print(F("D0/D2 aim D1 blast")); display.display(); delay(15);
    }
    waitButtonRelease(); oldL = oldS = oldR = HIGH;
    if (shot && cursor == virus) { ++hits; goodSound(); } else badSound();
  }
  return hits >= 7;
}

void cellLabPage() {
  byte page = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(BTN_L), sBtn = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) { page = page == 0 ? 2 : page - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { page = (page + 1) % 3; clickSound(); delay(100); }
    if (oldS == HIGH && sBtn == LOW && page == 2) {
      waitButtonRelease();
      if (conditionType == 0) popup(F("Cells are healthy"), F("No treatment needed"));
      else if (cellTreatmentGame()) {
        reduceCondition();
        if (conditionSeverity <= 1) clearCondition();
        addStat(health, 2); paradise.addResearch(8); winSound(); popup(F("Cells stabilized!"), F("Condition improved"));
      } else { subStat(energy, 1); popup(F("Treatment incomplete"), F("Medicine can still help")); }
      clampAll(); markSaveDirty();
    }
    oldL = l; oldS = sBtn; oldR = r;

    display.clearDisplay(); display.setTextWrap(false); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(35, 0); display.print(F("TAMA CELLS")); drawRightScore(page + 1, 3);
    if (page == 0) {
      display.setCursor(0, 14); display.print(F("Body family ")); display.print(bodyType);
      display.setCursor(0, 26); display.print(F("Eyes ")); display.print(eyeType); display.setCursor(66, 26); display.print(F("Pattern ")); display.print(patternType);
      display.setCursor(0, 38); display.print(F("Inherited ")); display.print(inheritedTrait < 4 ? inheritedTrait : 9);
      display.setCursor(0, 50); display.print(F("Birth code ")); display.print(birthCode);
    } else if (page == 1) {
      display.setCursor(0, 14); display.print(F("Field cell ")); display.print(habitatName(habitat));
      display.setCursor(0, 26); display.print(F("Food cell "));
      if (learnedFavoriteFood < FOOD_COUNT) display.print(learnedFavoriteFood + 1); else display.print(F("unknown"));
      display.setCursor(0, 38); display.print(F("Care quality ")); display.print((health + happy + clean + discipline) / 4);
      display.setCursor(0, 50); display.print(F("Growth hint ")); display.print(evolutionHint(speciesType));
    } else {
      display.setCursor(0, 14); display.print(F("Condition ")); display.print(conditionName());
      display.setCursor(0, 28); display.print(F("Severity ")); display.print(conditionSeverity);
      display.setCursor(0, 42); display.print(F("Health ")); display.print(health); display.print(F("/10"));
      display.setCursor(0, 56); display.print(conditionType == 0 ? F("Cells stable") : F("D1 start cell treatment"));
    }
    display.display(); delay(20);
  }
}

void enterParadiseCode() {
  const char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  char code[9] = "00000000";
  byte pos = 0;
  byte value[8] = {0,0,0,0,0,0,0,0};
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (pos < 8) {
    const bool l = digitalRead(BTN_L), sBtn = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) { value[pos] = value[pos] == 0 ? 35 : value[pos] - 1; code[pos] = alphabet[value[pos]]; clickSound(); delay(85); }
    if (oldR == HIGH && r == LOW) { value[pos] = (value[pos] + 1) % 36; code[pos] = alphabet[value[pos]]; clickSound(); delay(85); }
    if (oldS == HIGH && sBtn == LOW) { ++pos; clickSound(); delay(100); }
    oldL = l; oldS = sBtn; oldR = r;
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(28, 0); display.print(F("LAB CODE"));
    for (byte i = 0; i < 8; ++i) {
      const int x = 5 + i * 15;
      if (i == pos) display.fillRect(x - 1, 22, 12, 13, WHITE);
      display.setTextColor(i == pos ? BLACK : WHITE);
      display.setCursor(x + 2, 25); display.print(code[i]);
      display.setTextColor(WHITE);
    }
    display.setCursor(3, 48); display.print(F("D0/D2 letter D1 next")); display.display(); delay(15);
  }
  code[8] = 0; waitButtonRelease();
  if (strcmp(code, "PICO2026") == 0) {
    paradise.unlockToy(4); addCoins(20); popup(F("Code accepted"), F("Telescope + 20G"));
  } else if (strcmp(code, "GARDEN88") == 0) {
    paradise.unlockToy(3); addMaterial(4, 5); popup(F("Code accepted"), F("Flower stand + petals"));
  } else if (strcmp(code, "STARLAB1") == 0) {
    paradise.addResearch(50); roomOwned |= (1UL << 5); discoveredRooms |= (1UL << 5); popup(F("Code accepted"), F("Research + space room"));
  } else if (strcmp(code, "AQUA2026") == 0) {
    paradise.unlockToy(5); addMaterial(5, 5); popup(F("Code accepted"), F("Sprayer + shells"));
  } else {
    badSound(); popup(F("Unknown lab code"), F("Check all 8 symbols"));
  }
  markSaveDirty();
}

void researchProjectsPage() {
  if (stage < STAGE_CHILD) { popup(F("Research opens at child"), F("Grow a little first")); return; }
  byte pick = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(BTN_L), sb = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) { pick = (pick + 3) % 4; clickSound(); }
    if (oldR == HIGH && r == LOW) { pick = (pick + 1) % 4; clickSound(); }
    if (oldS == HIGH && sb == LOW) {
      waitButtonRelease();
      if (energy < 1) { popup(F("Research needs energy"), F("Rest first")); return; }
      if (pick == 0 && (!botany.gardenCount() || botany.gardenAverageWater() >= 6)) {
        popup(F("Garden needs no water"), F("Grow food crops first")); return;
      }
      if (pick == 1 && !botany.indoorNeedsTending()) { popup(F("No plants need tending"), F("Adopt or grow decor")); return; }
      if (pick == 2 && (stage < STAGE_ADULT || lifeWorld.careerId() >= LIFE_CAREER_COUNT)) {
        popup(F("Choose an adult career"), F("Research builds its XP")); return;
      }
      const byte project = petAgeDays() % LIFE_PROJECT_COUNT;
      if (pick == 3 && lifeWorld.projectLevel(project) >= 3) { popup(F("Project is complete"), F("Tomorrow: next district")); return; }
      if (paradise.researchPoints() < 10) { popup(F("Need 10 research"), F("Missions and space trips")); return; }
      if (!completeGameplayAction(M_RESEARCH)) { popup(F("Project already studied"), F("Try another activity")); return; }
      paradise.spendResearch(10);
      subStat(energy, 1);
      if (pick == 0) { botany.eventWaterGarden(3); botany.eventTendGarden(2); }
      else if (pick == 1) botany.eventTendIndoor(3);
      else if (pick == 2) lifeWorld.eventCareerProgress(10, 2, 1);
      else lifeWorld.eventCommunityProgress(project, 25, 0, 2);
      lifeWorld.recordMemory(21, petAgeDays(), pick);
      markSaveDirty(); popup(F("Research put to work"), F("A real project improved")); return;
    }
    oldL = l; oldS = sb; oldR = r;
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(10, 0); display.print(F("APPLIED RESEARCH"));
    display.setCursor(5, 18);
    if (pick == 0) display.print(F("Food crop irrigation"));
    else if (pick == 1) display.print(F("Florist plant care"));
    else if (pick == 2) display.print(F("Career study +10 XP"));
    else display.print(F("City project +25%"));
    display.setCursor(5, 33); display.print(F("10 research + 1 energy"));
    display.setCursor(5, 45); display.print(F("Research: ")); display.print(paradise.researchPoints());
    display.setCursor(0, 57); display.print(F("D0/D2 pick  D1 apply"));
    display.display(); delay(25);
  }
}

void paradiseLabPage() {
  static const byte count = 10;
  byte pick = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(BTN_L), sBtn = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? count - 1 : pick - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { pick = (pick + 1) % count; clickSound(); delay(100); }
    if (oldS == HIGH && sBtn == LOW) {
      waitButtonRelease();
      if (pick == 0) planetReportPage();
      else if (pick == 1) missionsPage();
      else if (pick == 2) fieldLifePage();
      else if (pick == 3) eggHuntPage();
      else if (pick == 4) spaceTravelPage();
      else if (pick == 5) emergencyPage();
      else if (pick == 6) connectionLabPage();
      else if (pick == 7) cellLabPage();
      else if (pick == 8) researchProjectsPage();
      else enterParadiseCode();
    }
    oldL = l; oldS = sBtn; oldR = r;

    display.clearDisplay(); display.setTextWrap(false); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(28, 0); display.print(F("PARADISE LAB")); drawRightScore(pick + 1, count);
    display.drawRoundRect(5, 15, 118, 36, 5, WHITE);
    display.setCursor(18, 24);
    if (pick == 0) display.print(F("Planet report"));
    else if (pick == 1) display.print(F("Missions and trophies"));
    else if (pick == 2) display.print(F("Tama Fields + toys"));
    else if (pick == 3) display.print(F("Daily egg hunt"));
    else if (pick == 4) display.print(F("Biofuel space travel"));
    else if (pick == 5) display.print(F("Planet emergencies"));
    else if (pick == 6) display.print(F("Moon connection lab"));
    else if (pick == 7) display.print(F("Tama cell research"));
    else if (pick == 8) display.print(F("Apply research to life"));
    else display.print(F("Enter an 8-char code"));
    display.setCursor(8, 39);
    if (paradise.emergencyPending()) display.print(F("! EMERGENCY WAITING !"));
    else { display.print(F("Planet Lv.")); display.print(planetLevel()); display.print(F("  R:")); display.print(paradise.researchPoints()); }
    display.setCursor(0, 56); display.print(F("Hold D2 to leave lab"));
    display.display(); delay(20);
  }
}


void diagnosticsPage() {
  byte page = 0;
  bool oldL = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(BTN_L);
    const bool r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) { page = page == 0 ? 3 : page - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { page = (page + 1) % 4; clickSound(); delay(100); }
    oldL = l; oldR = r;

    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(27, 0); display.print(F("DIAGNOSTICS"));
    drawRightScore(page + 1, 4);

    if (page == 0) {
      display.setCursor(0, 14); display.print(F("RTC")); display.setCursor(54, 14); display.print(currentClockEpoch());
      display.setCursor(0, 26); display.print(F("Age ticks")); display.setCursor(66, 26); display.print(ageTicks);
      display.setCursor(0, 38); display.print(F("Remainder")); display.setCursor(66, 38); display.print(lifeSecondRemainder);
      display.setCursor(0, 50); display.print(F("Stage")); display.setCursor(54, 50); display.print(stage);
    } else if (page == 1) {
      display.setCursor(0, 14); display.print(F("Food zero")); display.setCursor(66, 14); display.print(zeroFoodTicks);
      display.setCursor(0, 26); display.print(F("Clean zero")); display.setCursor(66, 26); display.print(zeroCleanTicks);
      display.setCursor(0, 38); display.print(F("Mood zero")); display.setCursor(66, 38); display.print(zeroHappyTicks);
      display.setCursor(0, 50); display.print(F("Critical")); display.setCursor(66, 50); display.print(criticalNeglectTicks);
    } else if (page == 2) {
      display.setCursor(0, 14); display.print(F("Call type")); display.setCursor(72, 14); display.print(pendingCallType);
      display.setCursor(0, 26); display.print(F("Call ticks")); display.setCursor(72, 26); display.print(pendingCallTicks);
      display.setCursor(0, 38); display.print(F("Away")); display.setCursor(48, 38); display.print(petAway);
      display.setCursor(72, 38); display.print(F("Wake")); display.setCursor(108, 38); display.print(lastWakeReason);
      display.setCursor(0, 50); display.print(F("Save seq")); display.setCursor(60, 50); display.print(saveSequence);
    } else {
      display.setCursor(0, 14); display.print(F("Difficulty")); display.setCursor(66, 14); display.print(difficultyName());
      display.setCursor(0, 26); display.print(F("Habitat")); display.setCursor(60, 26); display.print(habitat);
      display.setCursor(0, 38); display.print(F("Species")); display.setCursor(60, 38); display.print(speciesType);
      display.setCursor(0, 50); display.print(F("Next day")); display.setCursor(60, 50); display.print(TICKS_PER_DAY - (ageTicks % TICKS_PER_DAY));
    }
    display.display();
    delay(20);
  }
}


void developerCommit(bool saveNow) {
#if ALWAYS_AWAKE_MODE
  sleeping = false;
  lightOff = false;
  sleepTicks = 0;
  wrongLightTicks = 0;
  lightMistakeOpen = false;
  screenSleeping = false;
  if (oledIsOff) forceDisplayOn();
#endif
  dead = false;
  archivedCurrent = 0;
  clampAll();
  markSaveDirty();
  if (saveNow) saveGameNow();
}

bool developerEditNumber(const __FlashStringHelper *title, long &value,
                         long minimum, long maximum, long step) {
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return false;
    bool l = digitalRead(BTN_L), s = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) {
      value -= step;
      if (value < minimum) value = maximum;
      clickSound(); delay(105);
    }
    if (oldR == HIGH && r == LOW) {
      value += step;
      if (value > maximum) value = minimum;
      clickSound(); delay(105);
    }
    if (oldS == HIGH && s == LOW) { waitButtonRelease(); goodSound(); return true; }
    oldL = l; oldS = s; oldR = r;
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(5, 4); display.print(title);
    display.drawRoundRect(8, 19, 112, 31, 5, WHITE);
    display.setTextSize(2); display.setCursor(25, 27); display.print(value);
    display.setTextSize(1); display.display(); delay(20);
  }
}

void developerApplyStage(byte newStage) {
  newStage = min((byte)STAGE_ELDER, newStage);
  stage = newStage;
  if (stage == STAGE_EGG) ageTicks = 0;
  else if (stage == STAGE_BABY) ageTicks = growthThreshold(EGG_HATCH_TICKS);
  else if (stage == STAGE_CHILD) ageTicks = growthThreshold(CHILD_START_TICKS);
  else if (stage == STAGE_TEEN) ageTicks = growthThreshold(TEEN_START_TICKS);
  else if (stage == STAGE_ADULT) ageTicks = growthThreshold(ADULT_START_TICKS);
  else ageTicks = growthThreshold(ELDER_START_TICKS);
  if (stage == STAGE_TEEN) lockTeenEvolutionPath();
  stageCareMistakes = 0;
  petAway = false; awayTicks = 0;
  if (stage >= STAGE_ADULT) discoverSpecies(speciesType);
  developerCommit();
}

void developerSelectStage() {
  byte pick = stage;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l=digitalRead(BTN_L), s=digitalRead(BTN_S), r=digitalRead(BTN_R);
    if (oldL==HIGH && l==LOW) { pick = pick==0 ? STAGE_ELDER : pick-1; clickSound(); delay(110); }
    if (oldR==HIGH && r==LOW) { pick = (pick+1) % (STAGE_ELDER+1); clickSound(); delay(110); }
    if (oldS==HIGH && s==LOW) { developerApplyStage(pick); goodSound(); popup(F("Stage changed"), stageName()); return; }
    oldL=l; oldS=s; oldR=r;
    byte saved=stage; stage=pick;
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(25,3); display.print(F("Choose stage"));
    display.drawRoundRect(8,18,112,32,5,WHITE); display.setTextSize(2);
    display.setCursor(22,28); display.print(stageName()); display.setTextSize(1);
    display.display(); stage=saved; delay(20);
  }
}

void developerSelectSpecies() {
  byte pick = speciesType % SPECIES_COUNT;
  bool oldL=HIGH, oldS=HIGH, oldR=HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l=digitalRead(BTN_L), s=digitalRead(BTN_S), r=digitalRead(BTN_R);
    if (oldL==HIGH && l==LOW) { pick = pick==0 ? SPECIES_COUNT-1 : pick-1; clickSound(); delay(90); }
    if (oldR==HIGH && r==LOW) { pick=(pick+1)%SPECIES_COUNT; clickSound(); delay(90); }
    if (oldS==HIGH && s==LOW) {
      speciesType=pick; eggType=growthFamilyForAdult(pick); bodyType=pick%12; idleStyle=pick%6;
      geneA=pick%8; geneB=(pick/2)%8; geneC=(pick/3)%8;
      personality=pick%8; statBias=pick%8;
      if (stage < STAGE_ADULT) developerApplyStage(STAGE_ADULT);
      discoverSpecies(pick); developerCommit(); goodSound();
      popup(F("Species changed"), speciesNameById(pick)); return;
    }
    oldL=l; oldS=s; oldR=r;
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(18,2); display.print(F("Choose creature")); drawRightScore(pick+1,SPECIES_COUNT);
    display.drawRoundRect(4,17,120,35,5,WHITE);
    display.setCursor(10,25); display.print(F("#")); display.print(pick+1);
    display.setCursor(10,38); display.print(speciesNameById(pick));
    display.display(); delay(20);
  }
}

void developerSelectTemperament() {
  byte pick=temperament%8; bool oldL=HIGH,oldS=HIGH,oldR=HIGH; waitButtonRelease();
  while(true){
    if(rightHoldExitCheck())return;
    bool l=digitalRead(BTN_L),s=digitalRead(BTN_S),r=digitalRead(BTN_R);
    if(oldL==HIGH&&l==LOW){pick=pick==0?7:pick-1;clickSound();delay(100);}
    if(oldR==HIGH&&r==LOW){pick=(pick+1)%8;clickSound();delay(100);}
    if(oldS==HIGH&&s==LOW){temperament=pick;developerCommit();goodSound();popup(F("Temperament set"),temperamentName());return;}
    oldL=l;oldS=s;oldR=r;
    byte saved=temperament;temperament=pick;display.clearDisplay();display.setTextSize(1);display.setTextColor(WHITE);
    display.setCursor(17,4);display.print(F("Temperament"));display.drawRoundRect(8,19,112,31,5,WHITE);
    display.setCursor(20,31);display.print(temperamentName());display.display();temperament=saved;delay(20);
  }
}

void developerPetMenu() {
  const byte count=7; byte pick=0; bool oldL=HIGH,oldS=HIGH,oldR=HIGH; waitButtonRelease();
  while(true){
    if(rightHoldExitCheck())return;
    bool l=digitalRead(BTN_L),s=digitalRead(BTN_S),r=digitalRead(BTN_R);
    if(oldL==HIGH&&l==LOW){pick=pick==0?count-1:pick-1;clickSound();delay(100);}
    if(oldR==HIGH&&r==LOW){pick=(pick+1)%count;clickSound();delay(100);}
    if(oldS==HIGH&&s==LOW){waitButtonRelease();
      if(pick==0)developerSelectStage();
      else if(pick==1){long v=petAgeDays();if(developerEditNumber(F("Age in days"),v,0,999,1)){ageTicks=(unsigned long)v*TICKS_PER_DAY;updateGrowthStage();developerCommit();}}
      else if(pick==2)developerSelectSpecies();
      else if(pick==3)developerSelectTemperament();
      else if(pick==4){developerApplyStage(stage>=STAGE_ELDER?STAGE_EGG:stage+1);popup(F("Advanced stage"),stageName());}
      else if(pick==5){ageTicks=0;stage=STAGE_EGG;developerCommit();popup(F("Egg restored"),F("Ready to hatch"));}
      else{health=10;dead=false;deathReason=0;petAway=false;clearCondition();developerCommit();popup(F("Pet revived"),F("Health restored"));}
    }
    oldL=l;oldS=s;oldR=r;
    display.clearDisplay();display.setTextSize(1);display.setTextColor(WHITE);display.setCursor(31,1);display.print(F("Pet / Age"));drawRightScore(pick+1,count);
    display.drawRoundRect(5,17,118,36,5,WHITE);display.setCursor(13,25);
    if(pick==0){display.print(F("Stage: "));display.print(stageName());}
    else if(pick==1){display.print(F("Age days: "));display.print(petAgeDays());}
    else if(pick==2){display.print(F("Species: "));display.print(speciesName());}
    else if(pick==3){display.print(F("Temper: "));display.print(temperamentName());}
    else if(pick==4)display.print(F("Advance one stage"));
    else if(pick==5)display.print(F("Return to egg"));
    else display.print(F("Revive / restore"));
    display.display();delay(20);
  }
}

void developerNeedsMenu() {
  const byte count=13; byte pick=0; bool oldL=HIGH,oldS=HIGH,oldR=HIGH; waitButtonRelease();
  while(true){
    if(rightHoldExitCheck())return;
    bool l=digitalRead(BTN_L),s=digitalRead(BTN_S),r=digitalRead(BTN_R);
    if(oldL==HIGH&&l==LOW){pick=pick==0?count-1:pick-1;clickSound();delay(90);}
    if(oldR==HIGH&&r==LOW){pick=(pick+1)%count;clickSound();delay(90);}
    if(oldS==HIGH&&s==LOW){waitButtonRelease();
      if(pick==0){health=full=energy=happy=clean=discipline=10;ownerBond=ownerTrust=100;clearCondition();poop=0;}
      else if(pick==1){health=full=energy=happy=clean=discipline=5;ownerBond=ownerTrust=50;}
      else if(pick==2){full=energy=happy=clean=0;health=1;ownerBond=ownerTrust=0;}
      else if(pick>=3&&pick<=8){long v=0;byte *target=nullptr;if(pick==3){v=health;target=&health;}if(pick==4){v=full;target=&full;}if(pick==5){v=energy;target=&energy;}if(pick==6){v=happy;target=&happy;}if(pick==7){v=clean;target=&clean;}if(pick==8){v=discipline;target=&discipline;}if(developerEditNumber(F("Set need 0-10"),v,0,10,1))*target=(byte)v;}
      else if(pick==9){long v=ownerBond;if(developerEditNumber(F("Owner bond"),v,0,100,5))ownerBond=(byte)v;}
      else if(pick==10){long v=ownerTrust;if(developerEditNumber(F("Owner trust"),v,0,100,5))ownerTrust=(byte)v;}
      else if(pick==11){long v=careMistakes;if(developerEditNumber(F("Care mistakes"),v,0,100,1))careMistakes=(unsigned int)v;}
      else{zeroFoodTicks=zeroCleanTicks=zeroHappyTicks=zeroSickTicks=criticalNeglectTicks=0;foodMistakeOpen=cleanMistakeOpen=happyMistakeOpen=sickMistakeOpen=callMistakeOpen=false;pendingCallType=0;pendingCallTicks=0;}
      developerCommit();goodSound();
    }
    oldL=l;oldS=s;oldR=r;
    display.clearDisplay();display.setTextSize(1);display.setTextColor(WHITE);display.setCursor(31,1);display.print(F("Needs / Care"));drawRightScore(pick+1,count);
    display.drawRoundRect(4,17,120,36,5,WHITE);display.setCursor(12,25);
    if(pick==0)display.print(F("Max all needs"));else if(pick==1)display.print(F("Set balanced 5"));else if(pick==2)display.print(F("Set critical test"));
    else if(pick==3){display.print(F("Health: "));display.print(health);}else if(pick==4){display.print(F("Hunger: "));display.print(full);}else if(pick==5){display.print(F("Energy: "));display.print(energy);}else if(pick==6){display.print(F("Happy: "));display.print(happy);}else if(pick==7){display.print(F("Clean: "));display.print(clean);}else if(pick==8){display.print(F("Discipline: "));display.print(discipline);}else if(pick==9){display.print(F("Bond: "));display.print(ownerBond);}else if(pick==10){display.print(F("Trust: "));display.print(ownerTrust);}else if(pick==11){display.print(F("Mistakes: "));display.print(careMistakes);}else display.print(F("Clear neglect timers"));
    display.display();delay(20);
  }
}

void developerSelectSpeed() {
  bool oldL=HIGH,oldS=HIGH,oldR=HIGH;waitButtonRelease();
  while(true){if(rightHoldExitCheck())return;bool l=digitalRead(BTN_L),s=digitalRead(BTN_S),r=digitalRead(BTN_R);
    if(oldL==HIGH&&l==LOW){developerSpeedMode=developerSpeedMode==0?4:developerSpeedMode-1;clickSound();delay(100);}
    if(oldR==HIGH&&r==LOW){developerSpeedMode=(developerSpeedMode+1)%5;clickSound();delay(100);}
    if(oldS==HIGH&&s==LOW){lifeSecondRemainder=0;lastLifeClockEpoch=currentClockEpoch();goodSound();waitButtonRelease();return;}
    oldL=l;oldS=s;oldR=r;display.clearDisplay();display.setTextSize(1);display.setTextColor(WHITE);display.setCursor(28,4);display.print(F("Life speed"));display.drawRoundRect(8,19,112,31,5,WHITE);display.setTextSize(2);display.setCursor(30,28);display.print(developerSpeedName());display.setTextSize(1);display.display();delay(20);}
}

void developerTimeMenu() {
  const byte count=7;byte pick=0;bool oldL=HIGH,oldS=HIGH,oldR=HIGH;waitButtonRelease();
  while(true){if(rightHoldExitCheck())return;bool l=digitalRead(BTN_L),s=digitalRead(BTN_S),r=digitalRead(BTN_R);
    if(oldL==HIGH&&l==LOW){pick=pick==0?count-1:pick-1;clickSound();delay(90);}if(oldR==HIGH&&r==LOW){pick=(pick+1)%count;clickSound();delay(90);}
    if(oldS==HIGH&&s==LOW){waitButtonRelease();if(pick==0)developerSelectSpeed();else if(pick==1)gameTick();else if(pick==2){for(byte i=0;i<TICKS_PER_HOUR;i++)gameTick();}else if(pick==3){for(unsigned int i=0;i<TICKS_PER_DAY&&!dead;i++)gameTick();}else if(pick==4){ageTicks+=TICKS_PER_DAY;updateGrowthStage();developerCommit();}else if(pick==5){difficultyMode=(difficultyMode+1)%4;goodSound();}else{developerSpeedMode=0;difficultyMode=1;lifeSecondRemainder=0;lastLifeClockEpoch=currentClockEpoch();goodSound();}developerCommit();}
    oldL=l;oldS=s;oldR=r;display.clearDisplay();display.setTextSize(1);display.setTextColor(WHITE);display.setCursor(30,1);display.print(F("Time / Speed"));drawRightScore(pick+1,count);display.drawRoundRect(4,17,120,36,5,WHITE);display.setCursor(12,25);
    if(pick==0){display.print(F("Life speed: "));display.print(developerSpeedName());}else if(pick==1)display.print(F("Run 1 life tick"));else if(pick==2)display.print(F("Run 1 care hour"));else if(pick==3)display.print(F("Run 1 care day"));else if(pick==4)display.print(F("Age +1 day only"));else if(pick==5){display.print(F("Difficulty: "));display.print(difficultyName());}else display.print(F("Reset normal timing"));display.display();delay(20);}
}

void developerUnlockEverything() {
  coins=65000;skillSmart=skillStyle=skillKind=skillFit=100;jobRank=10;friendCount=20;tripBadges=255;gardenLevel=10;musicStars=trainStars=studyStars=100;
  outfitOwned=0xFFFFFFFFUL;roomOwned=0xFFFFFFFFUL;discoveredOutfits=0xFFFFFFFFUL;discoveredRooms=0xFFFFFFFFUL;habitatOwned=0xFFFFU;
  discoveredSpeciesLo=0xFFFFFFFFUL;discoveredSpeciesHi=(1UL<<(SPECIES_COUNT-32))-1UL;discoveredPlants=(1U<<PLANT_COUNT)-1U;visitedLocations=0xFFFFU;recipesCrafted=(1U<<CRAFT_RECIPE_COUNT)-1U;seasonalRewardMask=0xFFFFU;
  berryInv=herbInv=vegInv=spiceInv=99;mealBerryBite=mealHerbSoup=mealVegPlate=mealSpicyJam=mealRoyalStew=99;medCold=medTummy=medSkin=medEnergy=medMood=medAll=99;
  for(byte i=0;i<MATERIAL_COUNT;i++)craftMaterials[i]=99;
  aquarium.developerUnlockAll();lifeWorld.developerMax();deepLife.developerMax();parentDialogue.developerMax();growingTogether.developerMax();developerCommit();
}

void developerUnlockMenu() {
  const byte count=7;byte pick=0;bool oldL=HIGH,oldS=HIGH,oldR=HIGH;waitButtonRelease();
  while(true){if(rightHoldExitCheck())return;bool l=digitalRead(BTN_L),s=digitalRead(BTN_S),r=digitalRead(BTN_R);if(oldL==HIGH&&l==LOW){pick=pick==0?count-1:pick-1;clickSound();delay(90);}if(oldR==HIGH&&r==LOW){pick=(pick+1)%count;clickSound();delay(90);}if(oldS==HIGH&&s==LOW){waitButtonRelease();if(pick==0){coins=min((unsigned int)65000,(unsigned int)(coins+100));}else if(pick==1){long v=coins;if(developerEditNumber(F("Set coins"),v,0,65000,100))coins=(unsigned int)v;}else if(pick==2){skillSmart=skillStyle=skillKind=skillFit=100;parentDialogue.developerMax();growingTogether.developerMax();}else if(pick==3){berryInv=herbInv=vegInv=spiceInv=99;mealBerryBite=mealHerbSoup=mealVegPlate=mealSpicyJam=mealRoyalStew=99;medCold=medTummy=medSkin=medEnergy=medMood=medAll=99;for(byte i=0;i<MATERIAL_COUNT;i++)craftMaterials[i]=99;}else if(pick==4)developerUnlockEverything();else if(pick==5){outfitOwned=discoveredOutfits=0xFFFFFFFFUL;roomOwned=discoveredRooms=0xFFFFFFFFUL;habitatOwned=0xFFFFU;}else{discoveredSpeciesLo=0xFFFFFFFFUL;discoveredSpeciesHi=(1UL<<(SPECIES_COUNT-32))-1UL;discoveredPlants=(1U<<PLANT_COUNT)-1U;visitedLocations=0xFFFFU;recipesCrafted=(1U<<CRAFT_RECIPE_COUNT)-1U;}developerCommit();goodSound();}
    oldL=l;oldS=s;oldR=r;display.clearDisplay();display.setTextSize(1);display.setTextColor(WHITE);display.setCursor(25,1);display.print(F("Items / Unlocks"));drawRightScore(pick+1,count);display.drawRoundRect(4,17,120,36,5,WHITE);display.setCursor(12,25);if(pick==0)display.print(F("Add 100 coins"));else if(pick==1){display.print(F("Coins: "));display.print(coins);}else if(pick==2)display.print(F("Max all skills"));else if(pick==3)display.print(F("Max supplies"));else if(pick==4)display.print(F("Unlock everything"));else if(pick==5)display.print(F("Unlock rooms/items"));else display.print(F("Unlock discoveries"));display.display();delay(20);}
}

void developerSelectPartner() {
  byte pick = partnerId < PARTNER_COUNT ? partnerId : PARTNER_COUNT;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(BTN_L);
    const bool s = digitalRead(BTN_S);
    const bool r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? PARTNER_COUNT : pick - 1; clickSound(); delay(90); }
    if (oldR == HIGH && r == LOW) { pick = (pick + 1) % (PARTNER_COUNT + 1); clickSound(); delay(90); }
    if (oldS == HIGH && s == LOW) {
      partnerId = pick < PARTNER_COUNT ? pick : 255;
      if (partnerId == 255) { partnerBond = 0; familyReady = 0; }
      developerCommit(); goodSound(); return;
    }
    oldL = l; oldS = s; oldR = r;

    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(34, 2); display.print(F("PARTNER"));
    display.drawRoundRect(7, 17, 114, 35, 5, WHITE);
    if (pick < PARTNER_COUNT) {
      cityWorld.drawResidentTiny(pick, 18, 27, (anim / 5) & 1);
      display.setCursor(52, 25); display.print(partnerName(pick));
      display.setCursor(52, 38); display.print(F("SPECIES "));
      display.print(cityWorld.residentSpecies(pick));
    } else {
      display.setCursor(51, 31); display.print(F("NONE"));
    }
    display.display();
    delay(20);
  }
}

void developerStartNextGeneration() {
  if (!confirmChoice(F("Start next generation?"), F("Current pet is archived"))) return;
  if (partnerId >= PARTNER_COUNT) partnerId = 0;
  partnerBond = 100;
  familyReady = 1;
  preparePartnerInheritance();
  deathReason = 4;
  archiveCurrentPet();
  chooseEgg();
  startNewPet();
  developerCommit(true);
}

void developerFamilyMenu() {
  const byte count = 10;
  byte pick = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(BTN_L);
    const bool s = digitalRead(BTN_S);
    const bool r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? count - 1 : pick - 1; clickSound(); delay(90); }
    if (oldR == HIGH && r == LOW) { pick = (pick + 1) % count; clickSound(); delay(90); }
    if (oldS == HIGH && s == LOW) {
      waitButtonRelease();
      if (pick == 0) developerSelectPartner();
      else if (pick == 1) { long v = partnerBond; if (developerEditNumber(F("Partner bond"), v, 0, 100, 5)) partnerBond = (byte)v; }
      else if (pick == 2) familyReady = !familyReady;
      else if (pick == 3) { long v = generation; if (developerEditNumber(F("Generation"), v, 1, 65000, 1)) generation = (unsigned int)v; }
      else if (pick == 4) { long v = inheritedTrait < 4 ? inheritedTrait : 0; if (developerEditNumber(F("Inherited hobby"), v, 0, 3, 1)) inheritedTrait = (byte)v; }
      else if (pick == 5) { long v = inheritedTemperament < 8 ? inheritedTemperament : 0; if (developerEditNumber(F("Inherited temper"), v, 0, 7, 1)) inheritedTemperament = (byte)v; }
      else if (pick == 6) { long v = inheritedFavoriteGame < 13 ? inheritedFavoriteGame : 0; if (developerEditNumber(F("Inherited game"), v, 0, 12, 1)) inheritedFavoriteGame = (byte)v; }
      else if (pick == 7) { long v = inheritedBodyType < 12 ? inheritedBodyType : 0; if (developerEditNumber(F("Inherited body"), v, 0, 11, 1)) inheritedBodyType = (byte)v; }
      else if (pick == 8) { if (partnerId >= PARTNER_COUNT) partnerId = 0; partnerBond = 100; familyReady = 1; preparePartnerInheritance(); }
      else { developerStartNextGeneration(); return; }
      developerCommit(); goodSound();
    }
    oldL = l; oldS = s; oldR = r;

    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(25, 1); display.print(F("FAMILY / GENES"));
    drawRightScore(pick + 1, count);
    display.drawRoundRect(5, 17, 118, 36, 5, WHITE);
    display.setCursor(13, 24);
    if (pick == 0) {
      display.print(F("Partner"));
      display.setCursor(13, 38);
      if (partnerId < PARTNER_COUNT) display.print(partnerName(partnerId)); else display.print(F("None"));
    } else if (pick == 1) {
      display.print(F("Partner bond")); display.setCursor(90, 38); display.print(partnerBond);
    } else if (pick == 2) {
      display.print(F("Family ready")); display.setCursor(82, 38); display.print(familyReady ? F("YES") : F("NO"));
    } else if (pick == 3) {
      display.print(F("Generation")); display.setCursor(78, 38); display.print(generation);
    } else if (pick == 4) {
      display.print(F("Child hobby")); display.setCursor(90, 38); display.print(inheritedTrait);
    } else if (pick == 5) {
      display.print(F("Child temper")); display.setCursor(90, 38); display.print(inheritedTemperament);
    } else if (pick == 6) {
      display.print(F("Child game")); display.setCursor(90, 38); display.print(inheritedFavoriteGame);
    } else if (pick == 7) {
      display.print(F("Child body")); display.setCursor(90, 38); display.print(inheritedBodyType);
    } else if (pick == 8) {
      display.print(F("Prepare family test")); display.setCursor(13, 38); display.print(F("Bond 100 + mixed genes"));
    } else {
      display.print(F("Start next generation")); display.setCursor(13, 38); display.print(F("Archive current pet"));
    }
    display.display();
    delay(20);
  }
}

void developerConditionMenu() {
  const byte count=12;byte pick=0;bool oldL=HIGH,oldS=HIGH,oldR=HIGH;waitButtonRelease();
  while(true){if(rightHoldExitCheck())return;bool l=digitalRead(BTN_L),s=digitalRead(BTN_S),r=digitalRead(BTN_R);if(oldL==HIGH&&l==LOW){pick=pick==0?count-1:pick-1;clickSound();delay(90);}if(oldR==HIGH&&r==LOW){pick=(pick+1)%count;clickSound();delay(90);}if(oldS==HIGH&&s==LOW){waitButtonRelease();if(pick==0)clearCondition();else if(pick>=1&&pick<=6)setCondition(pick,3);else if(pick==7){long v=poop;if(developerEditNumber(F("Poop count"),v,0,3,1))poop=(byte)v;}else if(pick==8){long v=pendingCallType;if(developerEditNumber(F("Call type 0-6"),v,0,6,1)){pendingCallType=(byte)v;pendingCallTicks=0;}}else if(pick==9){petAway=!petAway;if(!petAway)awayTicks=0;}else if(pick==10){full=0;pendingCallType=1;pendingCallTicks=0;}else{dead=false;deathReason=0;health=10;petAway=false;clearCondition();}developerCommit();goodSound();}
    oldL=l;oldS=s;oldR=r;display.clearDisplay();display.setTextSize(1);display.setTextColor(WHITE);display.setCursor(23,1);display.print(F("Events / Health"));drawRightScore(pick+1,count);display.drawRoundRect(4,17,120,36,5,WHITE);display.setCursor(12,25);if(pick==0)display.print(F("Clear illness"));else if(pick==1)display.print(F("Set cold"));else if(pick==2)display.print(F("Set tummy ache"));else if(pick==3)display.print(F("Set rash"));else if(pick==4)display.print(F("Set exhaustion"));else if(pick==5)display.print(F("Set gloom"));else if(pick==6)display.print(F("Set fever"));else if(pick==7){display.print(F("Poop: "));display.print(poop);}else if(pick==8){display.print(F("Call type: "));display.print(pendingCallType);}else if(pick==9){display.print(F("Pet away: "));display.print(petAway?F("yes"):F("no"));}else if(pick==10)display.print(F("Trigger hunger call"));else display.print(F("Revive + healthy"));display.display();delay(20);}
}

void developerAquariumMenu() {
  const byte count=8;byte pick=0;bool oldL=HIGH,oldS=HIGH,oldR=HIGH;waitButtonRelease();
  while(true){if(rightHoldExitCheck())return;bool l=digitalRead(BTN_L),s=digitalRead(BTN_S),r=digitalRead(BTN_R);if(oldL==HIGH&&l==LOW){pick=pick==0?count-1:pick-1;clickSound();delay(90);}if(oldR==HIGH&&r==LOW){pick=(pick+1)%count;clickSound();delay(90);}if(oldS==HIGH&&s==LOW){waitButtonRelease();if(pick==0)aquarium.developerFillTank();else if(pick==1)aquarium.developerAddFish(10);else if(pick==2)aquarium.developerAddFish(11);else if(pick==3)aquarium.developerMaxCare();else if(pick==4)aquarium.developerNeglectTank();else if(pick==5)aquarium.developerUnlockAll();else if(pick==6){if(confirmChoice(F("Reset aquarium?"),F("All fish are removed")))aquarium.reset();}else aquarium.saveNow();developerCommit();goodSound();}
    oldL=l;oldS=s;oldR=r;display.clearDisplay();display.setTextSize(1);display.setTextColor(WHITE);display.setCursor(31,1);display.print(F("Aquarium Lab"));drawRightScore(pick+1,count);display.drawRoundRect(4,17,120,36,5,WHITE);display.setCursor(12,25);if(pick==0)display.print(F("Fill test tank"));else if(pick==1)display.print(F("Add Moonfin"));else if(pick==2)display.print(F("Add Starfin"));else if(pick==3)display.print(F("Max fish care"));else if(pick==4)display.print(F("Create care alert"));else if(pick==5)display.print(F("Unlock tank/items"));else if(pick==6)display.print(F("Reset aquarium"));else display.print(F("Save aquarium now"));display.display();delay(20);}
}


void developerBotanyMenu() {
  const byte count = 6; byte pick = 0; bool oldL = HIGH, oldS = HIGH, oldR = HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(BTN_L), sBtn = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? count - 1 : pick - 1; clickSound(); delay(90); }
    if (oldR == HIGH && r == LOW) { pick = (pick + 1) % count; clickSound(); delay(90); }
    if (oldS == HIGH && sBtn == LOW) {
      waitButtonRelease();
      if (pick == 0) botany.developerFillGarden();
      else if (pick == 1) botany.developerFillPlantRoom();
      else if (pick == 2) botany.developerMaxCare();
      else if (pick == 3) botany.developerNeglect();
      else if (pick == 4) botany.developerUnlockAll();
      else botany.saveNow();
      developerCommit(); goodSound();
    }
    oldL = l; oldS = sBtn; oldR = r;
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(35, 1); display.print(F("BOTANY LAB")); drawRightScore(pick + 1, count);
    display.drawRoundRect(4, 17, 120, 36, 5, WHITE); display.setCursor(12, 25);
    if (pick == 0) display.print(F("Fill ready garden"));
    else if (pick == 1) display.print(F("Fill plant room"));
    else if (pick == 2) display.print(F("Max all plant care"));
    else if (pick == 3) display.print(F("Create plant alert"));
    else if (pick == 4) display.print(F("Unlock all plants"));
    else display.print(F("Save botany now"));
    display.display(); delay(20);
  }
}

void developerCityMenu() {
  const byte count = 3; byte pick = 0; bool oldL = HIGH, oldS = HIGH, oldR = HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(BTN_L), sBtn = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? count - 1 : pick - 1; clickSound(); delay(90); }
    if (oldR == HIGH && r == LOW) { pick = (pick + 1) % count; clickSound(); delay(90); }
    if (oldS == HIGH && sBtn == LOW) {
      waitButtonRelease();
      if (pick == 0) cityWorld.developerMaxRelations();
      else if (pick == 1) cityWorld.developerResetRelations();
      else cityWorld.saveNow();
      developerCommit(); goodSound();
    }
    oldL = l; oldS = sBtn; oldR = r;
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(39, 1); display.print(F("CITY LAB")); drawRightScore(pick + 1, count);
    display.drawRoundRect(4, 17, 120, 36, 5, WHITE); display.setCursor(12, 25);
    if (pick == 0) display.print(F("Max all relations"));
    else if (pick == 1) display.print(F("Reset relations"));
    else display.print(F("Save city now"));
    display.display(); delay(20);
  }
}

void developerGenesMenu() {
  const byte count=10;byte pick=0;bool oldL=HIGH,oldS=HIGH,oldR=HIGH;waitButtonRelease();
  while(true){if(rightHoldExitCheck())return;bool l=digitalRead(BTN_L),s=digitalRead(BTN_S),r=digitalRead(BTN_R);if(oldL==HIGH&&l==LOW){pick=pick==0?count-1:pick-1;clickSound();delay(90);}if(oldR==HIGH&&r==LOW){pick=(pick+1)%count;clickSound();delay(90);}if(oldS==HIGH&&s==LOW){waitButtonRelease();long v=0;if(pick==0){v=bodyType;if(developerEditNumber(F("Body type"),v,0,11,1))bodyType=(byte)v;}else if(pick==1){v=eyeType;if(developerEditNumber(F("Eye type"),v,0,5,1))eyeType=(byte)v;}else if(pick==2){v=mouthType;if(developerEditNumber(F("Mouth type"),v,0,5,1))mouthType=(byte)v;}else if(pick==3){v=patternType;if(developerEditNumber(F("Pattern"),v,0,7,1))patternType=(byte)v;}else if(pick==4){v=headTrait;if(developerEditNumber(F("Head trait"),v,0,3,1))headTrait=(byte)v;}else if(pick==5){v=tailType;if(developerEditNumber(F("Tail type"),v,0,7,1))tailType=(byte)v;}else if(pick==6){v=idleStyle;if(developerEditNumber(F("Idle style"),v,0,5,1))idleStyle=(byte)v;}else if(pick==7){v=birthMark;if(developerEditNumber(F("Birth mark"),v,0,6,1))birthMark=(byte)v;}else if(pick==8){rareAllRounder=!rareAllRounder;}else{rollPetGenes(speciesType);if(stage>=STAGE_ADULT)discoverSpecies(speciesType);}developerCommit();goodSound();}
    oldL=l;oldS=s;oldR=r;display.clearDisplay();display.setTextSize(1);display.setTextColor(WHITE);display.setCursor(31,1);display.print(F("Appearance Lab"));drawRightScore(pick+1,count);display.drawRoundRect(4,17,120,36,5,WHITE);display.setCursor(12,25);if(pick==0){display.print(F("Body: "));display.print(bodyType);}else if(pick==1){display.print(F("Eyes: "));display.print(eyeType);}else if(pick==2){display.print(F("Mouth: "));display.print(mouthType);}else if(pick==3){display.print(F("Pattern: "));display.print(patternType);}else if(pick==4){display.print(F("Head trait: "));display.print(headTrait);}else if(pick==5){display.print(F("Tail: "));display.print(tailType);}else if(pick==6){display.print(F("Idle style: "));display.print(idleStyle);}else if(pick==7){display.print(F("Birth mark: "));display.print(birthMark);}else if(pick==8){display.print(F("Rare genes: "));display.print(rareAllRounder?F("on"):F("off"));}else display.print(F("Reroll current genes"));display.display();delay(20);}
}

void developerFullSaveReset() {
  stopSoftBuzzer();
  for (size_t i = 0; i < TAMACHIBI_EEPROM_BYTES; ++i) EEPROM.write(i, 0xFF);
  EEPROM.commit();
  saveSequence = 0; extrasSequence = 0; saveDirty = false; lastSaveAt = 0;
  developerSpeedMode = 0; difficultyMode = 1; rescueMode = 1;
  habitat = 0; habitatOwned = 1U;
  discoveredSpeciesLo = 0; discoveredSpeciesHi = 0; discoveredPlants = 0;
  visitedLocations = 0; recipesCrafted = 0;
  discoveredOutfits = 1UL; discoveredRooms = 1UL;
  craftMaterials[0] = 1; craftMaterials[1] = 1;
  for (byte i = 2; i < MATERIAL_COUNT; ++i) craftMaterials[i] = 0;
  memset(foodExperience, 0, sizeof(foodExperience)); memset(foodBoredom, 0, sizeof(foodBoredom));
  learnedFavoriteFood = 255; learnedDislikedFood = 255; favoritePlace = 0;
  partnerId = 255; partnerBond = 0; familyReady = 0;
  inheritedTrait = 255; inheritedTemperament = 255; inheritedFavoriteGame = 255; inheritedBodyType = 255;
  petAway = false; awayTicks = 0; runawayCount = 0;
  memset(careHistoryType, 0, sizeof(careHistoryType)); memset(careHistoryDay, 0, sizeof(careHistoryDay)); memset(careHistoryHour, 0, sizeof(careHistoryHour));
  careHistoryHead = 0; careHistoryCount = 0;
  seasonalEventBest = 0; seasonalEventPlayedDay = 999999UL; seasonalRewardMask = 0;
  generation = 1; albumCount = 0;
  memset(albumSpecies, 0, sizeof(albumSpecies)); memset(albumAge, 0, sizeof(albumAge)); memset(albumBond, 0, sizeof(albumBond)); memset(albumReason, 0, sizeof(albumReason)); memset(albumName, 0, sizeof(albumName));
  for (byte i = 0; i < 8; ++i) { albumPartner[i] = 255; albumGeneration[i] = 0; }
  legacySpecies = 255; legacySkill = 255; legacyOutfit = 0; archivedCurrent = 0;
  soundMode = 2; brightnessLevel = 1; dialWorldView = VIEW_PET;
  aquarium.reset(); botany.reset(); cityWorld.reset(); paradise.reset(); lifeWorld.developerReset(); deepLife.developerReset(); parentDialogue.developerReset(); growingTogether.developerReset(); saveProfiles.eraseAll();
  waitButtonRelease(); chooseEgg(); archivedCurrent = 0; startNewPet();
  lastRtcEpoch = currentClockEpoch(); lastLifeClockEpoch = lastRtcEpoch; lifeSecondRemainder = 0;
  applyBrightness(); forceDisplayOn(); saveGameNow();
  popup(F("FULL RESET DONE"), F("All progress erased"), 1000);
}

void developerSaveMenu() {
  const byte count = 7; byte pick = 0; bool oldL = HIGH, oldS = HIGH, oldR = HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(BTN_L), sBtn = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? count - 1 : pick - 1; clickSound(); delay(90); }
    if (oldR == HIGH && r == LOW) { pick = (pick + 1) % count; clickSound(); delay(90); }
    if (oldS == HIGH && sBtn == LOW) {
      waitButtonRelease();
      if (pick == 0) { saveGameNow(); popup(F("Saved now"), F("All test data kept")); }
      else if (pick == 1) { if (loadGameSave()) popup(F("Save reloaded"), F("Stored state restored")); else popup(F("No valid save"), F("State unchanged")); }
      else if (pick == 2) diagnosticsPage();
      else if (pick == 3) { developerSpeedMode = 0; difficultyMode = 1; screenSleeping = false; oledIsOff = false; sleeping = false; lightOff = false; forceDisplayOn(); popup(F("Safe defaults"), F("Always awake")); }
      else if (pick == 4) { openSaveProfiles(); }
      else if (pick == 5) { if (confirmChoice(F("New clean pet?"), F("Current pet is archived"))) { startReplacementEgg(); return; } }
      else if (confirmChoice(F("FULL SAVE RESET?"), F("Erases every system")) && confirmChoice(F("Really erase all?"), F("This cannot be undone"))) { developerFullSaveReset(); return; }
      developerCommit();
    }
    oldL = l; oldS = sBtn; oldR = r;
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE); display.setCursor(35, 1); display.print(F("Save Tools")); drawRightScore(pick + 1, count);
    display.drawRoundRect(4, 17, 120, 36, 5, WHITE); display.setCursor(12, 25);
    if (pick == 0) display.print(F("Save immediately")); else if (pick == 1) display.print(F("Reload last save")); else if (pick == 2) display.print(F("Open diagnostics"));
    else if (pick == 3) display.print(F("Restore safe defaults")); else if (pick == 4) display.print(F("Safe save profiles")); else if (pick == 5) display.print(F("Start replacement egg"));
    else { display.print(F("FULL SAVE RESET")); display.setCursor(12, 38); display.print(F("Erases ALL progress")); }
    display.display(); delay(20);
  }
}

void developerLab() {
  const byte count=11;byte pick=0;bool oldL=HIGH,oldS=HIGH,oldR=HIGH;waitButtonRelease();
  while(true){if(rightHoldExitCheck()){developerCommit();return;}bool l=digitalRead(BTN_L),s=digitalRead(BTN_S),r=digitalRead(BTN_R);if(oldL==HIGH&&l==LOW){pick=pick==0?count-1:pick-1;clickSound();delay(90);}if(oldR==HIGH&&r==LOW){pick=(pick+1)%count;clickSound();delay(90);}if(oldS==HIGH&&s==LOW){waitButtonRelease();if(pick==0)developerPetMenu();else if(pick==1)developerNeedsMenu();else if(pick==2)developerTimeMenu();else if(pick==3)developerUnlockMenu();else if(pick==4)developerFamilyMenu();else if(pick==5)developerConditionMenu();else if(pick==6)developerAquariumMenu();else if(pick==7)developerBotanyMenu();else if(pick==8)developerCityMenu();else if(pick==9)developerGenesMenu();else developerSaveMenu();}
    oldL=l;oldS=s;oldR=r;display.clearDisplay();display.setTextSize(1);display.setTextColor(WHITE);display.setCursor(27,0);display.print(F("DEVELOPER LAB"));drawRightScore(pick+1,count);display.drawRoundRect(5,17,118,37,5,WHITE);display.setCursor(14,27);if(pick==0)display.print(F("Pet, age, species"));else if(pick==1)display.print(F("Needs and care"));else if(pick==2)display.print(F("Time and growth"));else if(pick==3)display.print(F("Items and unlocks"));else if(pick==4)display.print(F("Family and genes"));else if(pick==5)display.print(F("Events and health"));else if(pick==6)display.print(F("Aquarium testing"));else if(pick==7)display.print(F("Garden / plant room"));else if(pick==8)display.print(F("City relationships"));else if(pick==9)display.print(F("Appearance genes"));else display.print(F("Save and diagnostics"));display.setCursor(10,44);display.print(F("Speed: "));display.print(developerSpeedName());display.display();delay(20);}
}

void settingsMenu() {
  const byte settingCount = 12;
  byte pick = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(BTN_L), sBtn = digitalRead(BTN_S), r = digitalRead(BTN_R);
    if (oldL == HIGH && l == LOW) { pick = (pick == 0) ? settingCount - 1 : pick - 1; clickSound(); delay(120); }
    if (oldR == HIGH && r == LOW) { pick++; if (pick >= settingCount) pick = 0; clickSound(); delay(120); }
    if (oldS == HIGH && sBtn == LOW) {
      waitButtonRelease();
      if (pick == 0) { soundMode++; if (soundMode > 2) soundMode = 0; goodSound(); }
      else if (pick == 1) { brightnessLevel++; if (brightnessLevel > 2) brightnessLevel = 0; applyBrightness(); }
      else if (pick == 2) setClockMenu();
      else if (pick == 3) sitterModePage();
      else if (pick == 4) namePetActivity();
      else if (pick == 5) albumPage();
      else if (pick == 6) batteryPage();
      else if (pick == 7) { difficultyMode = (difficultyMode + 1) % 4; goodSound(); }
      else if (pick == 8) { rescueMode = !rescueMode; goodSound(); }
      else if (pick == 9) developerLab();
      else if (pick == 10) diagnosticsPage();
      else { startReplacementEgg(); return; }
      markSaveDirty();
    }
    oldL = l; oldS = sBtn; oldR = r;

    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    V46UI::drawHeader(display, F("SETTINGS"), visualHour(), clockMinute(), false, 0);
    display.drawRoundRect(8, 15, 112, 34, 6, WHITE);
    display.drawLine(14, 22, 114, 22, WHITE);
    display.setCursor(18, 28);
    if (pick == 0) { display.print(F("Sound: ")); display.print(soundMode == 0 ? F("off") : (soundMode == 1 ? F("quiet") : F("normal"))); }
    else if (pick == 1) { display.print(F("Brightness: ")); display.print(brightnessLevel == 0 ? F("low") : (brightnessLevel == 1 ? F("medium") : F("high"))); }
    else if (pick == 2) { display.print(F("Set real clock: ")); drawClock12Hour(visualHour(), clockMinute()); }
    else if (pick == 3) { display.print(F("Sitter: ")); if (sitterTicksRemaining == 0) display.print(F("off")); else { display.print((sitterTicksRemaining + TICKS_PER_HOUR - 1) / TICKS_PER_HOUR); display.print(F("h")); } }
    else if (pick == 4) { display.print(F("Rename: ")); display.print(petName); }
    else if (pick == 5) display.print(F("Family album"));
    else if (pick == 6) display.print(F("Battery meter"));
    else if (pick == 7) { display.print(F("Difficulty: ")); display.print(difficultyName()); }
    else if (pick == 8) { display.print(F("Rescue mode: ")); display.print(rescueMode ? F("on") : F("off")); }
    else if (pick == 9) display.print(F("Developer Lab"));
    else if (pick == 10) display.print(F("Diagnostics"));
    else display.print(F("Start a new egg"));
    display.setCursor(94, 17); display.print(pick + 1); display.print(F("/")); display.print(settingCount);
    V46UI::drawFooter(display, F("D0 PREV"), F("D1 CHANGE"), F("D2 NEXT"));
    display.display(); delay(20);
  }
}


void categoryMenu(byte category) {
  if(dead){startReplacementEgg();return;}
  byte count=categoryActionCount(category),pick=0;bool oldL=HIGH,oldS=HIGH,oldR=HIGH;
  waitButtonRelease();
  while(true){
    if(rightHoldExitCheck())return;
    bool l=digitalRead(BTN_L),sel=digitalRead(BTN_S),r=digitalRead(BTN_R);
    if(oldL==HIGH&&l==LOW){pick=pick==0?count-1:pick-1;clickSound();delay(120);}
    if(oldR==HIGH&&r==LOW){pick=(pick+1)%count;clickSound();delay(120);}
    if(oldS==HIGH&&sel==LOW){
      byte action=categoryActionAt(category,pick);waitButtonRelease();
      if(!actionUnlocked(action)){badSound();popup(F("Not unlocked"),unlockHint(action),700);}
      else{executeAction(action);return;}
    }
    oldL=l;oldS=sel;oldR=r;
    {
      NativeUI::Scope frame(display);display.clearDisplay();
      char counter[10];snprintf(counter,sizeof(counter),"%u/%u",pick+1,count);
      NativeUI::header(display,categoryShortTitle(category),counter);
      for(byte row=0;row<3;++row){
        byte index=(pick+count+row-1)%count,action=categoryActionAt(category,index);
        bool unlocked=actionUnlocked(action),active=row==1;int y=23+row*29;
        if(active)display.fillRoundRect(4,y,120,26,3,WHITE);
        uint16_t ink=active?BLACK:WHITE;
        NativeUI::text(display,menuTitle(action),9,y+4,108,ink);
        NativeUI::text(display,unlocked?menuDescription(action):menuUnlockShort(action),9,y+15,108,ink);
      }
      NativeUI::footer(display,"PREV","OPEN","NEXT");display.display();
    }
    delay(20);
  }
}

// ---------- actions ----------

void startNewPet() {
  gameplayProgress.reset();
  bool nextGeneration = archivedCurrent != 0;
  const byte familyPartnerId = partnerId;
  const byte familyPartnerSpecies =
      (nextGeneration && familyPartnerId < PARTNER_COUNT)
          ? cityWorld.residentSpecies(familyPartnerId)
          : legacySpecies;
  if (nextGeneration && generation < 65000) generation++;

  // Generous egg/baby starting values. The pet is easy to begin caring for.
  health = 9;
  full = 8;
  energy = 8;
  happy = 8;
  clean = 10;
  discipline = 5;
  sleeping = false;
  sick = false;
  dead = false;
  lightOff = false;
  conditionType = 0;
  conditionSeverity = 0;
  conditionTicks = 0;
  medCold = 1; medTummy = 1; medSkin = 1; medEnergy = 1; medMood = 1; medAll = 0;
  poop = 0;
  ageTicks = 0;
  stage = STAGE_EGG;
  careMistakes = 0;
  stageCareMistakes = 0;
  deathReason = 0;
  archivedCurrent = 0;
  menu = M_LIGHT;
  mainCategory = 0;
  menuVisible = true;
  sleepTicks = 0;
  lastFood = 255;
  varietyStreak = 0;
  walkWins = 0;
  crossyBest = 0;
  studyStars = 0;
  gardenLevel = 0;
  musicStars = 0;
  trainStars = 0;
  gardenHarvests = 0;
  coins = 8;
  skillSmart = 0; skillStyle = 0; skillKind = 0; skillFit = 0;
  jobRank = 0;
  outfit = 0;
  roomStyle = 0;
  toyItem = 0;
  mischief = 0;
  friendCount = 0;
  tripBadges = 0;
  cookedMeals = 0;
  berryInv = 1; herbInv = 0; vegInv = 0; spiceInv = 0;
  mealBerryBite = 0; mealHerbSoup = 0; mealVegPlate = 0; mealSpicyJam = 0; mealRoyalStew = 0;
  mealBuffType = 0; mealBuffTicks = 0; foodGuardTicks = 0;
  gardenPlant = 255; gardenStartTick = 0; gardenGrowTicks = 0;
  gardenWater = 0; gardenSun = 0; gardenCare = 0; gardenProblem = 0; gardenQuality = 0;
  outfitOwned = 1UL;
  roomOwned = 1UL;

  memset(foodExperience, 0, sizeof(foodExperience)); memset(foodBoredom, 0, sizeof(foodBoredom));
  learnedFavoriteFood = 255; learnedDislikedFood = 255; favoritePlace = habitat == 1 ? 2 : habitat == 2 ? 1 : habitat == 3 ? 6 : 0;
  partnerId = 255; partnerBond = 0; familyReady = 0; petAway = false; awayTicks = 0;
  memset(careHistoryType, 0, sizeof(careHistoryType)); memset(careHistoryDay, 0, sizeof(careHistoryDay)); memset(careHistoryHour, 0, sizeof(careHistoryHour)); careHistoryHead = 0; careHistoryCount = 0;
  seasonalEventPlayedDay = 999999UL;

  ownerBond = 20;
  ownerTrust = 20;
  temperament = random(0, 8);
  if (nextGeneration && inheritedTemperament < 8) temperament = inheritedTemperament;
  if (nextGeneration && inheritedBodyType < 12) {
    bodyType = inheritedBodyType;
    headTrait = (byte)((inheritedBodyType + familyPartnerSpecies) % 4);
  }
  if (nextGeneration) {
    // Mix visible genes from the archived parent and the resident spouse.
    // These traits influence the child's intermediate appearance and later
    // evolution without replacing care-based evolution.
    geneA = (byte)((geneA + familyPartnerSpecies) % 8);
    geneB = (byte)((geneB + familyPartnerSpecies / 2 + generation) % 8);
    geneC = (byte)((geneC + familyPartnerSpecies / 3 + legacySpecies) % 8);
    earType = (byte)((earType + familyPartnerSpecies) % 6);
    tailType = (byte)((tailType + familyPartnerSpecies / 2) % 8);
    patternType = (byte)((patternType + familyPartnerSpecies + generation) % 8);
    birthMark = (byte)((birthMark + familyPartnerSpecies) % 7);
  }
  strcpy(petName, "CHIBI");
  lastCareTick = 0;
  lastTalkTick = 0;
  dailyRoutineMask = 0;
  weeklyCareBits = 0;
  caredDayStreak = 0;
  missedCareDays = 0;
  routineDay = 0;
  lastTalkChoice = 255;
  lastEventId = 0;
  favoriteGivenFood = 255;
  ignoredCallCount = 0;
  rescuedFromSickness = 0;
  lastTripEvent = 255;
  hobbyType = 0;
  favoriteGameId = (byte)((birthCode + temperament * 3) % 13);
  if (nextGeneration && inheritedFavoriteGame < 13) favoriteGameId = inheritedFavoriteGame;
  dislikedGameId = (byte)((favoriteGameId + 6) % 13);
  activeGameId = 255;
  activeGameStartedAt = 0;
  lastGameBondTick = 0;
  featuredPlayedMask = 0;
  featuredDay = 0;
  memset(gameCounts, 0, sizeof(gameCounts));
  nutritionScore = 50; cleanlinessScore = 50; exerciseScore = 0; sicknessHistory = 0;
  overfeedCount = 0; praiseCount = 0; giftMilestones = 0; weight = 5;
  pendingCallType = 0; pendingCallTicks = 0; lastBirthdayRewardDay = 0;
  goalDay = 999999UL;
  goalWeek = 999999UL;
  configureDailyGoal(0);
  configureWeeklyGoal(0);
  sitterTicksRemaining = 0;
  sitterCooldownTicks = 0;

  foodMistakeOpen = false;
  cleanMistakeOpen = false;
  happyMistakeOpen = false;
  sickMistakeOpen = false;
  lightMistakeOpen = false;
  callMistakeOpen = false;
  zeroFoodTicks = 0;
  zeroCleanTicks = 0;
  zeroHappyTicks = 0;
  zeroSickTicks = 0;
  wrongLightTicks = 0;
  criticalNeglectTicks = 0;

  applyBirthStats();

  // Exactly one legacy passes forward: an outfit, a room item, or one skill point.
  if (nextGeneration) {
    byte inheritance = (byte)((birthCode + generation + legacySpecies) % 3U);
    if (inheritance == 0 && legacyOutfit > 0 && legacyOutfit < 31) {
      outfitOwned |= (1UL << legacyOutfit);
    } else if (inheritance == 1) {
      byte inheritedRoom = (byte)(1 + ((legacySpecies + generation) % (ROOM_STYLE_COUNT - 1)));
      roomOwned |= (1UL << inheritedRoom);
    } else {
      if (legacySkill == 0) addSkill(skillSmart, 1);
      else if (legacySkill == 1) addSkill(skillFit, 1);
      else if (legacySkill == 2) addSkill(skillStyle, 1);
      else addSkill(skillKind, 1);
    }
    if (inheritedTrait == 0) addSkill(skillSmart, 2);
    else if (inheritedTrait == 1) addSkill(skillFit, 2);
    else if (inheritedTrait == 2) addSkill(skillStyle, 2);
    else if (inheritedTrait == 3) addSkill(skillKind, 2);
    changeTrust(2);
  }

  lifeWorld.startGeneration(speciesType, generation, birthCode, temperament, favoriteGivenFood);
  deepLife.startGeneration(speciesType, generation, birthCode, temperament);
  parentDialogue.startGeneration(generation, ageTicks);
  growingTogether.startGeneration(makeGrowingTogetherContext());
  clampAll();
  markSaveDirty();
  if (rareAllRounder) popup(F("Rare egg!"), F("All-rounder genes"), 1000);
  else if (nextGeneration) popup(F("New generation"), F("A legacy remains"), 900);
  else popup(F("New egg"), F("Hatches in 5 min"), 900);
}


void drawFoodMenuIcon(byte pick, int x, int y) {
  // Big simple food icons inspired by the reference sheet style.
  if (pick == 0) { // basic meal bowl
    display.drawRoundRect(x + 2, y + 16, 28, 10, 5, WHITE);
    display.drawLine(x + 6, y + 26, x + 26, y + 26, WHITE);
    display.fillCircle(x + 11, y + 13, 3, WHITE);
    display.fillCircle(x + 18, y + 11, 3, WHITE);
    display.fillCircle(x + 24, y + 14, 3, WHITE);
  } else if (pick == 1) { // cooked meal pot
    display.drawRoundRect(x + 4, y + 14, 26, 13, 4, WHITE);
    display.drawLine(x + 8, y + 12, x + 26, y + 12, WHITE);
    display.drawLine(x + 11, y + 8, x + 11, y + 4, WHITE);
    display.drawLine(x + 18, y + 8, x + 18, y + 3, WHITE);
    display.drawLine(x + 25, y + 8, x + 25, y + 5, WHITE);
  } else if (pick == 2) { // snack heart/cookie
    display.drawCircle(x + 16, y + 16, 10, WHITE);
    display.drawPixel(x + 11, y + 12, WHITE);
    display.drawPixel(x + 20, y + 13, WHITE);
    display.drawPixel(x + 16, y + 21, WHITE);
  } else if (pick == 3) { // healthy leaf
    display.drawLine(x + 16, y + 27, x + 16, y + 7, WHITE);
    display.drawCircle(x + 10, y + 12, 6, WHITE);
    display.drawCircle(x + 23, y + 15, 6, WHITE);
    display.drawLine(x + 8, y + 12, x + 16, y + 17, WHITE);
    display.drawLine(x + 23, y + 15, x + 16, y + 20, WHITE);
  } else { // treat
    display.fillCircle(x + 12, y + 12, 5, WHITE);
    display.fillCircle(x + 22, y + 12, 5, WHITE);
    display.fillTriangle(x + 7, y + 15, x + 27, y + 15, x + 17, y + 28, WHITE);
  }
}

void foodMenu() {
  clickSound();
  waitButtonRelease();

  byte pick = 0;
  bool oldLeft = HIGH, oldSel = HIGH, oldRight = HIGH;

  while (true) {
    bool l = digitalRead(BTN_L);
    bool s = digitalRead(BTN_S);
    bool r = digitalRead(BTN_R);

    // D0 cycles food. D1 eats/selects. D2 exits.
    if (oldLeft == HIGH && l == LOW) {
      pick++;
      if (pick > 4) pick = 0;
      clickSound();
      delay(120);
    }

    if (oldRight == HIGH && r == LOW) {
      clickSound();
      waitButtonRelease();
      return;
    }

    if (oldSel == HIGH && s == LOW) {
      waitButtonRelease();
      break;
    }

    oldLeft = l;
    oldSel = s;
    oldRight = r;

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);

    // Left-side food selector. It does not conflict with pet art style.
    display.drawRoundRect(0, 8, 39, 45, 4, WHITE);
    drawFoodMenuIcon(pick, 3, 14);

    display.setCursor(4, 55);
    display.print(F("D2 back"));

    // Details on the right.
    display.setCursor(45, 0);
    display.print(F("Food"));
    display.setCursor(45, 12);

    if (pick == 0) {
      display.print(F("Basic meal"));
      display.setCursor(45, 24); display.print(F("+Food"));
      display.setCursor(45, 36); display.print(F("+Mood small"));
    } else if (pick == 1) {
      display.print(F("Cooked meal"));
      display.setCursor(45, 24); display.print(F("Meals: "));
      display.print(cookedMeals);
      display.setCursor(45, 36); display.print(F("Long buff"));
    } else if (pick == 2) {
      display.print(F("Light snack"));
      display.setCursor(45, 24); display.print(F("+Mood"));
      display.setCursor(45, 36); display.print(F("-Clean tiny"));
    } else if (pick == 3) {
      display.print(F("Healthy meal"));
      display.setCursor(45, 24); display.print(F("+Food"));
      display.setCursor(45, 36); display.print(F("+Health"));
    } else {
      display.print(F("Treat"));
      display.setCursor(45, 24); display.print(F("+Big mood"));
      display.setCursor(45, 36); display.print(F("-Discipline"));
    }

    display.setCursor(91, 0);
    if (learnedDislikedFood == pick || foodBoredom[pick] >= 12) display.print(F("TIRED"));
    else if (pick == favoriteFoodType() || pick == learnedFavoriteFood) display.print(F("FAV"));
    else if (recentCareCount(CARE_EVENT_FOOD_BASE + pick, 1) >= 2) display.print(F("AGAIN"));
    else if (foodExperience[pick] == 0) display.print(F("NEW"));

    display.setCursor(45, 55);
    display.print(F("D0 next D1 eat"));
    display.display();
    delay(20);
  }

  byte fullnessBefore = full;
  bool hungryBefore = fullnessBefore <= 2;
  bool overfed = fullnessBefore >= 9;
  byte recentMeals = recentCareCount(CARE_EVENT_FOOD_BASE + pick, 1);
  if (overfed && overfeedCount < 255) overfeedCount++;
  if (weight < 20) weight++;

  if (pick == 0) {
    addStat(full, 3);
    addStat(happy, 1);
  } else if (pick == 1) {
    if (cookedMeals == 0) {
      badSound();
      popup(F("No cooked food"), F("Cook first"), 700);
      return;
    }

    cookedMeals--;
    addStat(full, 5);
    addStat(health, 2);
    addStat(happy, 2);
    addStat(clean, 1);
    mealBuffTicks = 18;

  } else if (pick == 2) {
    addStat(full, 1);
    addStat(happy, 2);
    if (clean > 0) subStat(clean, 1);
  } else if (pick == 3) {
    addStat(full, 3);
    addStat(health, 1);
    addStat(clean, 1);
  } else {
    addStat(full, 1);
    addStat(happy, 4);
    if (discipline > 0) subStat(discipline, 1);
  }

  updateLearnedFoodPreferences(pick);
  logCareEvent(CARE_EVENT_FOOD_BASE + pick);
  bool learnedFavorite = (learnedFavoriteFood == pick && learnedDislikedFood != pick);
  bool disliked = learnedDislikedFood == pick;
  bool favorite = (pick == favoriteFoodType() || learnedFavorite) && !disliked;
  if (favorite) {
    favoriteGivenFood = pick;
    addStat(happy, 2);
    addStat(discipline, 1);
    changeBond(1);
  }

  if (lastFood != pick) {
    if (varietyStreak < 9) varietyStreak++;
  } else {
    varietyStreak = 0;
  }
  lastFood = pick;
  completeGameplayAction(M_FOOD);

  if (disliked) {
    subStat(happy, 1);
  }

  if (overfed) {
    subStat(health, 1);
    subStat(clean, 1);
    if (poop < 3) poop++;
  } else if (full >= 10 && random(0, 8) == 0 && poop < 3) {
    poop++;
  }

  if (pick == 0 || pick == 1 || pick == 3) nutritionScore = min((byte)100, (byte)(nutritionScore + 2));
  else nutritionScore = nutritionScore > 1 ? nutritionScore - 2 : 0;
  markRoutineAction(ROUTINE_CARE);
  recordGoalEvent(0, 1);
  changeTrust(1);
  goodSound();
  clampAll();
  markSaveDirty();
  popup(careFoodReply(pick, favorite, disliked, overfed, hungryBefore, recentMeals),
        careFoodDetail(pick, favorite, disliked, overfed, hungryBefore, recentMeals), 800);
}


void doFood() {
  clickSound();
  if (sleeping) {
    popup(F("Sleeping..."), F("Wake first"));
    return;
  }
  foodMenu();
}

void doLight() {
  clickSound();
  synchronizeDailySchedule(false);

  if (stage == STAGE_EGG) {
    popup(F("The egg is resting"), F("Clock starts after hatch"), 700);
    return;
  }

  if (isScheduledSleepTime()) {
    if (!sleeping) synchronizeDailySchedule(true);
    else {
      display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
      display.setCursor(29, 4); display.print(F("SLEEP SCHEDULE"));
      display.drawRoundRect(8, 16, 112, 31, 6, WHITE);
      display.setCursor(18, 23); display.print(F("Sleeping until "));
      display.setCursor(34, 35); drawClock12Hour(wakeHour(), 0);
      display.setCursor(0, 56); display.print(F("Night sleep cannot skip"));
      display.display();
      waitButtonRelease();
      while (digitalRead(BTN_S) == HIGH && digitalRead(BTN_R) == HIGH && digitalRead(BTN_L) == HIGH) delay(20);
      waitButtonRelease();
    }
  } else {
    display.clearDisplay(); display.setTextSize(1); display.setTextColor(WHITE);
    display.setCursor(31, 3); display.print(F("NOT BEDTIME"));
    display.drawRoundRect(8, 15, 112, 34, 6, WHITE);
    display.setCursor(17, 22); display.print(F("Real bedtime: "));
    display.setCursor(39, 34); drawClock12Hour(bedtimeHour(), 0);
    display.setCursor(4, 56); display.print(F("No quick-rest energy gain"));
    display.display();
    waitButtonRelease();
    while (digitalRead(BTN_S) == HIGH && digitalRead(BTN_R) == HIGH && digitalRead(BTN_L) == HIGH) delay(20);
    waitButtonRelease();
  }
  markSaveDirty();
}

void doMedicine() {
  clickSound();
  waitButtonRelease();

  byte pick = 0;
  bool oldLeft = HIGH;
  bool oldSel = HIGH;
  bool oldRight = HIGH;

  while (true) {
    if (rightHoldExitCheck()) return;

    bool l = digitalRead(BTN_L);
    bool s = digitalRead(BTN_S);
    bool r = digitalRead(BTN_R);

    if (oldLeft == HIGH && l == LOW) {
      pick = (pick == 0) ? 5 : pick - 1;
      clickSound();
      delay(120);
    }
    if (oldRight == HIGH && r == LOW) {
      pick++;
      if (pick > 5) pick = 0;
      clickSound();
      delay(120);
    }
    if (oldSel == HIGH && s == LOW) {
      waitButtonRelease();
      applyMedicine(pick);
      markSaveDirty();
      return;
    }

    oldLeft = l;
    oldSel = s;
    oldRight = r;

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);

    display.setCursor(0, 0);
    display.print(F("Medicine"));
    display.setCursor(78, 0);
    display.print(F("Cond:"));
    display.print(conditionShort());

    display.drawRoundRect(6, 14, 116, 33, 5, WHITE);
    display.setCursor(14, 22);
    display.print(medName(pick));
    display.setCursor(14, 32);
    display.print(medUseText(pick));

    byte stock = *medCountPtr(pick);
    display.setCursor(14, 42);
    display.print(F("Stock "));
    display.print(stock);
    display.print(F("  $"));
    display.print(coins);
    display.print(F(" Dose ")); display.print(MEDICINE_PRICE - lifeWorld.projectLevel(2));

    display.setCursor(0, 55);
    if (stock == 0) display.print(F("D1 buy/use  Hold D2 exit"));
    else display.print(F("D1 use     Hold D2 exit"));

    display.display();
    delay(20);
  }
}


void doClean() {
  clickSound();
  const byte cleanedPoop = poop;
  const byte repeats = recentCareCount(12, 1);
  const bool messy = cleanedPoop > 0 || clean <= 3;
  const bool needed = cleanedPoop > 0 || clean <= 7 || conditionType == 3;
  animBath();
  poop = 0;
  if (cleanedPoop > 0) paradise.addBiofuel((uint16_t)cleanedPoop * 6U);
  zeroCleanTicks = 0; cleanMistakeOpen = false;

  if (needed) {
    completeGameplayAction(M_CLEAN);
    cleanlinessScore = min((byte)100, (byte)(cleanlinessScore + 3));
    addStat(clean, 5);
    addStat(happy, 1);
    if (messy) addStat(health, 1);
    changeTrust(1);
    if (repeats == 0) changeBond(1);
    recordGoalEvent(3, 1);
  } else {
    // A pleasant bath can still be a shared moment, but clean-up is no longer
    // an easy way to grind health, trust, or cleanliness milestones.
    if (repeats == 0) addStat(clean, 1);
    if (repeats < 2) addStat(happy, 1);
    if (repeats == 0) changeBond(1);
  }

  if (conditionType == 3 && conditionSeverity <= 1) clearCondition();
  markRoutineAction(ROUTINE_CARE);
  logCareEvent(12);
  clampAll();
  popup(careCleanReply(needed, messy, repeats),
        needed ? (messy ? F("I feel fresh again") : F("That helped me reset"))
               : F("A little splash for fun"), 750);
}

void doLove() {
  clickSound();
  if (sleeping) {
    popup(F("Shhh..."), F("It is asleep"));
    return;
  }

  byte pick = 0;
  bool oldLeft = HIGH, oldSelect = HIGH, oldRight = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool left = digitalRead(BTN_L);
    const bool select = digitalRead(BTN_S);
    const bool right = digitalRead(BTN_R);
    if (oldLeft == HIGH && left == LOW) {
      pick = pick == 0 ? 2 : pick - 1;
      clickSound();
      delay(100);
    }
    if (oldRight == HIGH && right == LOW) {
      pick = (pick + 1) % 3;
      clickSound();
      delay(100);
    }
    if (oldSelect == HIGH && select == LOW) {
      waitButtonRelease();
      break;
    }
    oldLeft = left; oldSelect = select; oldRight = right;

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(35, 8); display.print(F("CARE MOMENT"));
    display.drawLine(8, 20, 120, 20, WHITE);
    display.drawRoundRect(9, 28, 110, 60, 8, WHITE);
    drawCurrentPetTiny(56, 43);
    display.setCursor(22, 72);
    if (pick == 0) display.print(F("Cuddle"));
    else if (pick == 1) display.print(F("Praise"));
    else display.print(F("Quiet company"));
    display.setCursor(13, 96);
    if (pick == 0) display.print(F("Warm reassurance"));
    else if (pick == 1) display.print(F("Notice their effort"));
    else display.print(F("Stay close, no rush"));
    display.setCursor(4, 117); display.print(F("D0/D2 pick  D1 select"));
    display.display();
    delay(20);
  }

  const byte repeats = recentCareCount(CARE_EVENT_COMFORT_BASE + pick, 1);
  const bool personalityMatch =
      (pick == 0 && (temperament == 0 || temperament == 3)) ||
      (pick == 1 && (temperament == 1 || temperament == 2 || temperament == 4 || temperament == 6)) ||
      (pick == 2 && (temperament == 0 || temperament == 5 || temperament == 7));

  careChoiceAnimation(pick);
  if (!completeGameplayAction(M_LOVE)) { popup(F("Still happy you are here"), F("A quiet moment together")); return; }
  if (pick == 0) {
    addStat(happy, happy <= 4 ? 3 : 1);
    if (repeats < 2) changeBond(personalityMatch ? 2 : 1);
    if (personalityMatch && repeats == 0) changeTrust(1);
  } else if (pick == 1) {
    if (repeats < 2) {
      addStat(happy, 1);
      addStat(discipline, 1);
      addSkill(skillKind, 1);
    }
    if (repeats < 2) changeBond(1);
    if (repeats == 0) changeTrust(1);
  } else {
    addStat(happy, (happy <= 3 || (temperament == 5 && repeats == 0)) ? 2 : 1);
    if (repeats < 2) changeBond(1);
    if (personalityMatch && repeats == 0) changeTrust(1);
  }

  markRoutineAction(ROUTINE_TALK);
  logCareEvent(CARE_EVENT_COMFORT_BASE + pick);
  clampAll();
  popup(careComfortReply(pick, personalityMatch, repeats), careComfortDetail(pick, repeats), 800);
}

void doCall() {
  careSound();
  markRoutineAction(ROUTINE_GREET);
  byte answeredCall = pendingCallType;
  rewardBirthdayIfRemembered();
  if (pendingCallType != 0) {
    pendingCallType = 0; pendingCallTicks = 0; callMistakeOpen = false;
    changeBond(2); changeTrust(2); logCareEvent(6);
  }
  if (petAway) {
    byte chance = min((byte)95, (byte)(35 + ownerTrust / 2 + ownerBond / 3));
    if (random(0, 100) < chance) { petAway = false; awayTicks = 0; health = max((byte)2, health); full = max((byte)2, full); happy = max((byte)2, happy); changeTrust(3); logCareEvent(9); animCallBubble(F("I came home!")); }
    else animCallBubble(F("Keep searching..."));
    markSaveDirty(); return;
  }
  if (dead) {
    popup(F("Press select"), F("to restart"));
    return;
  }

  if (answeredCall == 6) aquarium.showPetComment();
  else if (answeredCall >= 1 && answeredCall <= 5) animCallBubble(careCallReply(answeredCall));
  else if (conditionType != 0) animCallBubble(careCallReply(3));
  else if (sick) animCallBubble(careCallReply(3));
  else if (aquarium.needsCare()) aquarium.showPetComment();
  else if (botany.gardenReady()) animCallBubble(careCallReply(7));
  else if (mealBuffTicks > 0) animCallBubble(careCallReply(8));
  else if (poop > 0 || clean < 4) animCallBubble(careCallReply(2));
  else if (full < 4) animCallBubble(careCallReply(1));
  else if (energy < 3) animCallBubble(careCallReply(9));
  else if (happy < 4) animCallBubble(careCallReply(4));
  else if (discipline < 3) animCallBubble(careCallReply(10));
  else if (mischief > 0) animCallBubble(careCallReply(11));
  else if (outfit > 0 && skillStyle >= 4) animCallBubble(careCallReply(12));
  else if (coins >= 20) animCallBubble(careCallReply(13));
  else if (friendCount > 1) animCallBubble(careCallReply(14));
  else if (botany.needsCare()) animCallBubble(careCallReply(15));
  else if (gardenHarvests >= 3) animCallBubble(F("Garden snacks!"));
  else if (gardenLevel >= 4) animCallBubble(F("Garden is nice!"));
  else if (trainStars >= 8) animCallBubble(F("I am focused!"));
  else if (musicStars >= 8) animCallBubble(F("I love music!"));
  else if (stage == STAGE_EGG) animCallBubble(F("I am tiny!"));
  else if (stage == STAGE_BABY) animCallBubble(F("I am growing!"));
  else if (ownerBond >= 80) animCallBubble(F("You are my best friend"));
  else animCallBubble(lifeWorld.personalityThought());
  deepLife.drawEmotionBadge(108, 12, anim);
  const bool credited = completeGameplayAction(M_CALL);
  markRoutineAction(ROUTINE_TALK);
  if (credited) changeBond(1);
}

bool completeGameplayAction(uint8_t action) {
  if (action > M_ADVENTURE || dead || sleeping || sitterTicksRemaining > 0 ||
      !gameplayProgress.claim(action, ageTicks, generation)) return false;
  lifeWorld.recordAction(action, petAgeDays(), visualHour(), visualWeather());
  deepLife.recordAction(action, petAgeDays(), visualHour(), visualWeather());
  if (action == M_MED || action == M_CLEAN || action == M_FOOD) recordGrowingAction(GTA_CARE);
  if (action == M_GAME || action == M_WALK || action == M_TRAIN || action == M_MUSIC || action == M_TOY || action == M_EVENT || action == M_FESTIVAL) recordGrowingAction(GTA_PLAY);
  else if (action == M_STUDY) recordGrowingAction(GTA_SCHOOL);
  else if (action == M_GARDEN) recordGrowingAction(GTA_GARDEN);
  else if (action == M_PLANTROOM || action == M_RESEARCH || action == M_CRAFT || action == M_ROOM || action == M_SHOP) recordGrowingAction(GTA_HOBBY);
  else if (action == M_AQUARIUM || action == M_COMPANION) recordGrowingAction(GTA_COMPANION);
  else if (action == M_EXPLORE || action == M_ADVENTURE || action == M_HABITAT) recordGrowingAction(GTA_EXPLORE);
  else if (action == M_COOK || action == M_FOOD) recordGrowingAction(GTA_COOK);
  else if (action == M_JOB) recordGrowingAction(GTA_WORK);
  else if (action == M_CLEAN) recordGrowingAction(GTA_CLEAN);
  else if (action == M_FAMILY) recordGrowingAction(GTA_FAMILY);
  else if (action == M_TALK || action == M_LOVE || action == M_CALL) recordGrowingAction(GTA_TALK);
  if (!sleeping && (action == M_WALK || action == M_TRAIN || action == M_MUSIC || action == M_TOY || action == M_GARDEN || action == M_PLANTROOM || action == M_STUDY || action == M_AQUARIUM)) {
    dailyRoutineMask |= ROUTINE_PLAY;
  }
  if (!sleeping && (action == M_WALK || action == M_TRAIN)) {
    if (exerciseScore < 100) exerciseScore++;
    if (weight > 1) weight--;
  }
  // Strong friendships slightly improve trips and job earnings, without becoming grindable.
  if (action == M_EXPLORE && ownerBond >= 60) addCoins(1);
  if (action == M_JOB && ownerBond >= 70) addCoins(1);

  markSaveDirty();
  return true;
}

void drawMusicPlayerScene(bool playing) {
  drawMusicRoom(display, stage < STAGE_ADULT ? petLineage() : speciesType,
                stage, currentEvolutionPath(), playing, millis());
}

void drawMusicPlayerInfo(const String &title, const String &status) {
  needsDisplay.showMusic(title, status);
}

void executeAction(byte action) {
  menu = action;
  if (dead) { startReplacementEgg(); drawScreen(true); return; }
  if (!actionUnlocked(action)) { popup(F("Keep growing"), unlockHint(action)); return; }
  if (sitterTicksRemaining > 0 && action != M_SITTER && action != M_SETTINGS &&
      action != M_STATS && action != M_INSPECT && action != M_HISTORY &&
      action != M_DIARY && action != M_ALBUM && action != M_BOOK && action != M_PLAYER) {
    popup(F("Sitter is caring for us"), F("End sitter to play")); return;
  }
  if (petAway && action != M_CALL && action != M_STATS && action != M_INSPECT && action != M_HISTORY && action != M_SETTINGS && action != M_PLAYER) {
    badSound(); popup(F("Pet is away"), F("Use Call to search"), 750); drawScreen(true); return;
  }
  if (sleeping && action != M_LIGHT && action != M_STATS && action != M_INSPECT && action != M_HISTORY &&
      action != M_SETTINGS && action != M_DIARY && action != M_ALBUM && action != M_BOOK && action != M_PLAYER) {
    badSound();
    popup(F("Still sleeping"), F("Night sleep cannot skip"), 800);
    drawScreen(true);
    return;
  }
  if (isScheduledAwayTime() && action != M_GAME && action != M_STATS && action != M_INSPECT && action != M_HISTORY &&
      action != M_SETTINGS && action != M_DIARY && action != M_ALBUM && action != M_BOOK && action != M_TALK && action != M_PLAYER) {
    badSound();
    popup(isScheduledSchoolTime() ? F("Currently at school") : F("Currently at work"),
          isScheduledSchoolTime() ? F("Returns after 3 PM") : F("Returns after 5 PM"), 850);
    drawScreen(true);
    return;
  }
  if (ownerTrust < 15 && (action == M_WALK || action == M_TRAIN || action == M_MUSIC || action == M_TOY) && random(0,3)==0) {
    badSound(); popup(F("Not ready to play"), F("Rebuild trust first"), 700); drawScreen(true); return;
  }

  if (menu == M_FOOD) doFood();
  else if (menu == M_LIGHT) doLight();
  else if (menu == M_GAME) {
    if (sleeping) popup(F("Sleeping..."), F("Wake first"));
    else gameHubMenu();
  }
  else if (menu == M_MED) doMedicine();
  else if (menu == M_CLEAN) doClean();
  else if (menu == M_LOVE) doLove();
  else if (menu == M_CALL) doCall();
  else if (menu == M_WALK) walkGame();
  else if (menu == M_TRAIN) trainGame();
  else if (menu == M_STUDY) studyGame();
  else if (menu == M_GARDEN) botany.runGarden();
  else if (menu == M_PLANTROOM) botany.runPlantRoom();
  else if (menu == M_PARADISE) paradiseLabPage();
  else if (menu == M_LIFE) lifeWorld.runHub();
  else if (menu == M_NEIGHBORS) cityWorld.visitResidents();
  else if (menu == M_MUSIC) musicGame();
  else if (menu == M_PLAYER) {
    musicPlayer.setViews(drawMusicPlayerScene, drawMusicPlayerInfo);
    musicPlayer.run();
    needsDisplay.update(makeNeedsDisplayState(), millis(), true);
    updateWorldViewFromEncoder(true);  // Re-sync after the player used the encoder.
    drawScreen(true);
    return;
  }
  else if (menu == M_SHOP) shopClothes();
  else if (menu == M_TOY) toyActivity();
  else if (menu == M_EXPLORE) exploreTrip();
  else if (menu == M_COOK) cookActivity();
  else if (menu == M_JOB) jobActivity();
  else if (menu == M_ROOM) roomActivity();
  else if (menu == M_AQUARIUM) aquariumActivity();
  else if (menu == M_TALK) talkActivity();
  else if (menu == M_STATS) statusPage();
  else if (menu == M_SETTINGS) settingsMenu();
  else if (menu == M_ALBUM) albumPage();
  else if (menu == M_SITTER) sitterModePage();
  else if (menu == M_BOOK) tamaBookPage();
  else if (menu == M_CRAFT) craftPage();
  else if (menu == M_HABITAT) habitatPage();
  else if (menu == M_EVENT) seasonalEventPage();
  else if (menu == M_FAMILY) familyPage();
  else if (menu == M_INSPECT) inspectPage();
  else if (menu == M_HISTORY) careHistoryPage();
  else if (menu == M_DIARY) diaryPage();
  else if (menu == M_DIAG) diagnosticsPage();
  else if (menu == M_NEW_EGG) startReplacementEgg();


  markSaveDirty();
  drawScreen(true);
}

// ---------- timed life ----------

void recordCareMistake() {
  if (careMistakes < 65000) careMistakes++;
  if (stageCareMistakes < 65000) stageCareMistakes++;
  changeTrust(-2);
  changeBond(-1);
  logCareEvent(7);
  markSaveDirty();
}

void createPetCallIfNeeded() {
  if (pendingCallType != 0 || sleeping || isScheduledAwayTime() || stage == STAGE_EGG || sitterTicksRemaining > 0) return;
  if (conditionType != 0 || sick) pendingCallType = 3;
  else if (poop > 0 || clean <= 2) pendingCallType = 2;
  else if (full <= 2) pendingCallType = 1;
  else if (happy <= 2) pendingCallType = 4;
  else if (aquarium.needsCare()) pendingCallType = 6;
  else if (botany.needsCare()) pendingCallType = 4;
  else if (petAgeDays() > 0 && petAgeDays() % 7 == 0 && lastBirthdayRewardDay != petAgeDays()) pendingCallType = 5;
  else if (ageTicks % (6UL * TICKS_PER_HOUR) == 0 && random(0, 8) == 0) pendingCallType = 4;
  if (pendingCallType != 0) { pendingCallTicks = 0; logCareEvent(pendingCallType); }
}

void gameTick() {
  if (dead) return;
  if (sitterCooldownTicks > 0) sitterCooldownTicks--;
  if (sitterTicksRemaining > 0) {
    --sitterTicksRemaining;
    if (sitterTicksRemaining == 0) sitterCooldownTicks = TICKS_PER_DAY;
    markSaveDirty();
    return;
  }

  synchronizeDailySchedule(false);

  const bool scheduledAwayNow = isScheduledAwayTime();
  if (scheduledAwayNow) {
    // School and work handle ordinary daytime needs off-screen. Do not create
    // an impossible care call while the player is locked out of care actions.
    pendingCallType = 0;
    pendingCallTicks = 0;
    callMistakeOpen = false;
  }

  aquarium.tick(rtcCatchUpMode);
  botany.tick(rtcCatchUpMode);
  lifeWorld.tick(petAgeDays(), visualHour(), stage, visualWeather(), clockWeekday() < 5);
  deepLife.tick(petAgeDays(), visualHour(), stage, visualWeather());
  parentDialogue.tick(makeParentDialogueContext(), rtcCatchUpMode);
  growingTogether.tick(makeGrowingTogetherContext(), rtcCatchUpMode);

  if (petAway) {
    awayTicks++;
    if (awayTicks % difficultyTicks(TICKS_PER_HOUR) == 0) changeTrust(-1);
    if (friendCount > 0 && awayTicks >= difficultyTicks(TICKS_PER_DAY) && random(0, max((byte)2,(byte)(12-friendCount))) == 0) {
      petAway=false; awayTicks=0; health=max((byte)2,health); happy=2; logCareEvent(9); changeTrust(2); markSaveDirty(); return;
    }
    if (awayTicks >= difficultyTicks(2UL * TICKS_PER_DAY) && !rescueMode) { endPetLife(3); return; }
    markSaveDirty(); return;
  }


  ageTicks++;
  updateRoutineCalendar();
  updateGrowthStage();
  synchronizeDailySchedule(false);
  if (ageTicks % TICKS_PER_DAY == 0) {
    maybeScheduleParadiseEmergency();
    applyParadiseDailyEffects();
  }

  if (stage == STAGE_EGG) { markSaveDirty(); return; }

  tickMealBuff();
  tickConditionEffects();
  maybeStartRandomCondition();

  if (sleeping) {
    sleepTicks++;
    if (sleepTicks % SLEEP_ENERGY_GAIN_EVERY_TICKS == 0) addStat(energy, 1);
    if (sleepTicks % SLEEP_FOOD_DROP_EVERY_TICKS == 0 && foodGuardTicks == 0) subStat(full, 1);

    if (!lightOff) {
      wrongLightTicks++;
      if (wrongLightTicks >= AUTO_WAKE_AFTER_TICKS && !lightMistakeOpen) {
        lightMistakeOpen = true;
        recordCareMistake();
      }
    } else {
      wrongLightTicks = 0;
      lightMistakeOpen = false;
    }

    if (WAKE_AUTOMATICALLY && sleepTicks >= AUTO_WAKE_AFTER_TICKS) {
      sleeping = false; lightOff = false; sleepTicks = 0; wrongLightTicks = 0;
      addStat(happy, 1);
    }
  } else {
    sleepTicks = 0; wrongLightTicks = 0; lightMistakeOpen = false;
    if (ageTicks % difficultyTicks(HUNGER_DROP_EVERY_TICKS) == 0 && foodGuardTicks == 0) subStat(full, 1);
    if (ageTicks % difficultyTicks(ENERGY_DROP_EVERY_TICKS) == 0) subStat(energy, 1);
    if (ageTicks % difficultyTicks(MOOD_DROP_EVERY_TICKS) == 0) subStat(happy, 1);

    byte poopChance = BASE_POOP_CHANCE;
    if (discipline >= 7) poopChance = GOOD_DISCIPLINE_POOP_CHANCE;
    if (discipline <= 2) poopChance = BAD_DISCIPLINE_POOP_CHANCE;
    if (!scheduledAwayNow && random(0, poopChance) == 0 && poop < 3) poop++;

    // School/work provides a simple lunch at noon, but never sleep energy.
    // This prevents unavoidable hunger mistakes while the pet is away.
    if (scheduledAwayNow && visualHour() == 12 && clockMinute() < 5) addStat(full, stage <= STAGE_TEEN ? 2 : 1);

    // Energy can reach zero, but a daytime quick nap no longer replaces real
    // overnight sleep. The pet remains tired until its scheduled bedtime.
  }

  if (full == 0) zeroFoodTicks++; else { zeroFoodTicks = 0; foodMistakeOpen = false; }
  if (clean == 0) zeroCleanTicks++; else { zeroCleanTicks = 0; cleanMistakeOpen = false; }
  if (happy == 0) zeroHappyTicks++; else { zeroHappyTicks = 0; happyMistakeOpen = false; }
  if (sick || conditionType != 0) zeroSickTicks++; else { zeroSickTicks = 0; sickMistakeOpen = false; }

  if (zeroFoodTicks >= difficultyTicks(FOOD_MISTAKE_AFTER_TICKS) && !foodMistakeOpen) { foodMistakeOpen = true; recordCareMistake(); }
  if (zeroCleanTicks >= difficultyTicks(CLEAN_MISTAKE_AFTER_TICKS) && !cleanMistakeOpen) { cleanMistakeOpen = true; recordCareMistake(); }
  if (zeroHappyTicks >= difficultyTicks(HAPPY_MISTAKE_AFTER_TICKS) && !happyMistakeOpen) { happyMistakeOpen = true; recordCareMistake(); }
  if (zeroSickTicks >= difficultyTicks(SICK_MISTAKE_AFTER_TICKS) && !sickMistakeOpen) { sickMistakeOpen = true; recordCareMistake(); }

  // Normal digestion now creates visible poop even when the pet was not overfed.
  const unsigned long digestionTicks = difficultyTicks(8UL * TICKS_PER_HOUR);
  if (stage >= STAGE_BABY && !sleeping && !scheduledAwayNow && full >= 3 && ageTicks > 0 && digestionTicks > 0 && ageTicks % digestionTicks == 0 && poop < 3) {
    poop++;
    pendingCallType = 2;
    pendingCallTicks = 0;
  }
  if (poop > 0 && ageTicks % difficultyTicks(POOP_CLEAN_DROP_EVERY_TICKS) == 0) subStat(clean, 1);
  if (zeroCleanTicks >= difficultyTicks(DIRTY_SICK_TICKS) && conditionType == 0) setCondition(3, 2 + random(0, 2));
  else if (!sleeping && clean <= 2 && ageTicks % TICKS_PER_HOUR == 0 && random(0, SICK_FROM_DIRT_CHANCE) == 0) setCondition(3, 1);

  bool critical = full == 0 || clean == 0 || happy == 0 || conditionSeverity >= 3;
  if (critical) criticalNeglectTicks++; else criticalNeglectTicks = 0;

  if (criticalNeglectTicks >= difficultyTicks(MILD_NEGLECT_TICKS) && criticalNeglectTicks % TICKS_PER_DAY == 0 && conditionType == 0) setCondition(5, 1);
  if (criticalNeglectTicks >= difficultyTicks(CRITICAL_GRACE_TICKS) && criticalNeglectTicks % difficultyTicks(CRITICAL_DAMAGE_EVERY_TICKS) == 0) subStat(health, 1);
  if (criticalNeglectTicks >= difficultyTicks(SEVERE_NEGLECT_TICKS)) {
    if (rescueMode && !petAway) { petAway = true; awayTicks = 0; runawayCount++; health = 1; changeBond(-5); changeTrust(-8); logCareEvent(8); criticalNeglectTicks = 0; return; }
    if (ownerBond < 35 || careMistakes >= 10) { endPetLife(3); return; }
    health = 1; setCondition(6, 3); changeBond(-5); changeTrust(-5);
    criticalNeglectTicks = difficultyTicks(CRITICAL_GRACE_TICKS);
  }

  // Real-time attendance connects progression to the player's day. School
  // slowly builds learning/discipline; a selected adult career earns modest
  // hourly pay and experience while the Tama is visibly at work.
  if (isScheduledSchoolTime() && ageTicks % (2UL * TICKS_PER_HOUR) == 0) {
    addSkill(skillSmart, 1);
    addStat(discipline, 1);
    recordGrowingAction(GTA_SCHOOL);
  }
  if (isScheduledWorkTime() && ageTicks % TICKS_PER_HOUR == 0) {
    addCoins(1);
    lifeWorld.eventCareerProgress(1, 2, 1);
    recordGrowingAction(GTA_WORK);
  }

  createPetCallIfNeeded();
  if (pendingCallType != 0) {
    pendingCallTicks++;
    if (pendingCallTicks >= difficultyTicks(6UL * TICKS_PER_HOUR) && !callMistakeOpen) {
      callMistakeOpen = true;
      if (ignoredCallCount < 255) ignoredCallCount++;
      recordCareMistake();
    }
  }

  if (discipline >= 8 && ageTicks % 72UL == 0) addStat(happy, 1);
  if ((discipline <= 3 || temperament == 4) && !sleeping && ageTicks % 48UL == 0 && random(0, 4) == 0) {
    if (mischief < 6) mischief++;
    subStat(clean, 1);
  }
  if (ageTicks % 72UL == 0) {
    if (roomStyle == 1) addStat(happy, 1);
    else if (roomStyle == 2) { addStat(happy, 1); addStat(clean, 1); }
    else if (roomStyle == 3) addSkill(skillSmart, 1);
    else if (roomStyle == 4) addSkill(skillStyle, 1);
    else if (roomStyle == 5) addSkill(skillSmart, 1);
    else if (roomStyle == 6) { addStat(happy, 1); addSkill(skillKind, 1); }
    else if (roomStyle == 7) addStat(happy, 2);
    else if (roomStyle == 8) { addStat(happy, 1); addStat(health, 1); }
    else if (roomStyle == 9) { addStat(happy, 1); changeBond(1); }
  }
  if (ageTicks % TICKS_PER_DAY == 0) {
    if (habitat == 1) { addStat(health,1); addSkill(skillKind,1); }
    else if (habitat == 2) { addStat(happy,1); addSkill(skillStyle,1); }
    else if (habitat == 3) { addStat(energy,1); addSkill(skillFit,1); }
    else if (habitat == 4) addCoins(1);
    else if (habitat == 5) { addStat(clean,1); addSkill(skillKind,1); }
    else if (habitat == 6) addSkill(skillSmart,1);
    else if (habitat == 7) addSkill(skillFit,1);
  }
  if (outfit > 0 && ageTicks % TICKS_PER_DAY == 0) addSkill(skillStyle, 1);
  if (studyStars >= 10 && ageTicks % TICKS_PER_DAY == 0) addStat(discipline, 1);
  if (gardenLevel >= 3 && ageTicks % TICKS_PER_DAY == 0) addStat(clean, 1);
  if (musicStars >= 10 && ageTicks % TICKS_PER_DAY == 0) addStat(happy, 1);

  if (health == 0) endPetLife(1);
  else if (stage == STAGE_ELDER && petAgeDays() >= naturalLifespanDays()) endPetLife(2);

  clampAll();
  markSaveDirty();
}

void buttonWakeISR() {
  deepWakeFlag = true;
}

void processElapsedSeconds(unsigned long elapsedSeconds) {
  unsigned long tickSeconds = activeLifeTickSeconds();
  if (tickSeconds == 0) return;

  // Preserve partial life ticks across OLED-off periods, standby cycles and
  // frequent button checks. The old code discarded every interval shorter
  // than one full five-minute tick, which could leave an egg stuck forever.
  unsigned long totalSeconds = lifeSecondRemainder + elapsedSeconds;
  unsigned long requestedTicks = totalSeconds / tickSeconds;
  lifeSecondRemainder = totalSeconds % tickSeconds;
  if (requestedTicks == 0) return;

  unsigned long ticks = min(requestedTicks, 3UL * TICKS_PER_DAY);
  rtcCatchUpMode = true;
  for (unsigned long i = 0; i < ticks && !dead; i++) gameTick();
  rtcCatchUpMode = false;

  // Do not silently treat a week as only two days. Anything beyond the 72-hour
  // simulation window receives one clear severe-neglect outcome.
  if (requestedTicks > 3UL * TICKS_PER_DAY && !dead && sitterTicksRemaining == 0) {
    if (rescueMode) { petAway = true; awayTicks = 0; runawayCount++; logCareEvent(8); }
    else if (ownerBond < 45 || careMistakes >= 8) endPetLife(3);
    else {
      health = 1; full = 0; happy = 0; clean = 0;
      setCondition(6, 3);
      criticalNeglectTicks = difficultyTicks(CRITICAL_GRACE_TICKS);
    }
  }
  if (dead) { sleeping = false; lightOff = false; }
  markSaveDirty();
}

void serviceLifeClock() {
  unsigned long nowEpoch = currentClockEpoch();

  // The software clock advances while the board remains powered.
  // If the clock ever moves backward, resync without applying a huge age jump.
  if (lastLifeClockEpoch == 0 || nowEpoch < lastLifeClockEpoch) {
    lastLifeClockEpoch = nowEpoch;
    return;
  }

  if (nowEpoch > lastLifeClockEpoch) {
    unsigned long elapsed = nowEpoch - lastLifeClockEpoch;
    lastLifeClockEpoch = nowEpoch;
    processElapsedSeconds(elapsed);
  }
}

void forceDisplayOn() {
  // v26: do not call display.begin() again here. Reinitializing the SH1107
  // during a wake can leave inexpensive modules black, white, or peppered.
  display.oled_command(SH110X_NORMALDISPLAY);
  display.oled_command(SH110X_DISPLAYON);
  display.setTextWrap(false);
  display.clearDisplay();
  display.display();

  applyBrightness();
  oledIsOff = false;
}



void turnOledOffIdle() {
#if ALWAYS_AWAKE_MODE
  // Hard safety: even an accidental call cannot turn off the OLED in v34.
  screenSleeping = false;
  if (oledIsOff) forceDisplayOn();
  return;
#else
  if (screenSleeping || oledIsOff) return;
  display.clearDisplay();
  display.display();
  display.oled_command(SH110X_DISPLAYOFF);
  oledIsOff = true;
  menuVisible = false;
#endif
}

void enterScreenSleep() {
  // Reliable default: turn off only the main OLED. The ESP32 continues
  // polling all three buttons, so a press always restores the screen.
  turnOledOffIdle();
}



bool readRightButtonDebounced() {
  const bool raw = digitalRead(BTN_R);
  const unsigned long now = millis();
  if (!rightButtonDebounceReady) {
    rightButtonRawLast = raw;
    rightButtonStable = raw;
    rightButtonRawChangedAt = now;
    rightButtonDebounceReady = true;
    return rightButtonStable;
  }
  if (raw != rightButtonRawLast) {
    rightButtonRawLast = raw;
    rightButtonRawChangedAt = now;
  }
  if (now - rightButtonRawChangedAt >= 25UL) {
    rightButtonStable = rightButtonRawLast;
  }
  return rightButtonStable;
}

void syncRightButtonDebouncer() {
  rightButtonRawLast = digitalRead(BTN_R);
  rightButtonStable = rightButtonRawLast;
  rightButtonRawChangedAt = millis();
  rightButtonDebounceReady = true;
}

void wakeScreen() {
  // Wake must work even if the OLED is off but screenSleeping got out of sync.
  if (!screenSleeping && !oledIsOff) return;

  forceDisplayOn();
  screenSleeping = false;
  oledIsOff = false;
  lastWakeReason = 1;

  serviceLifeClock();
  lastInputAt = millis();
  lastGameTick = millis();
  lastAnimTick = millis();
  oldL = HIGH;
  oldS = HIGH;
  oldR = HIGH;

  waitButtonRelease();
  syncRightButtonDebouncer();
  oldR = HIGH;
  drawScreen(true);
}



bool anyButtonDown() {
  return digitalRead(BTN_L) == LOW || digitalRead(BTN_S) == LOW || digitalRead(BTN_R) == LOW;
}

void noteInput() {
  lastInputAt = millis();
}



void updateWorldViewFromEncoder(bool forceRead) {
  const unsigned long now = millis();
  if (forceRead) {
    resetWorldViewEncoder();
    return;
  }
  if (now - lastViewDialReadAt < 2UL) return;
  lastViewDialReadAt = now;

  const int8_t step = readWorldViewEncoderStep();
  if (step == 0) return;
  byte nextView = dialWorldView;
  if (step > 0) nextView = (dialWorldView + 1) % 3;
  else nextView = (dialWorldView + 2) % 3;

  if (nextView != dialWorldView) {
    dialWorldView = nextView;
    menuVisible = false;
    lastInputAt = now;
    if (!forceRead) clickSound();
    drawScreen(true);
  }
}

void resetWorldViewEncoder() {
  worldEncoderPreviousState =
      (static_cast<uint8_t>(digitalRead(VOLUME_ENCODER_CLK_PIN)) << 1) |
      static_cast<uint8_t>(digitalRead(VOLUME_ENCODER_DT_PIN));
  worldEncoderTransitionCount = 0;
  worldEncoderInitialized = true;
  lastViewDialReadAt = millis();
}

int8_t readWorldViewEncoderStep() {
  static const int8_t transitions[16] = {
      0, -1, 1, 0, 1, 0, 0, -1,
      -1, 0, 0, 1, 0, 1, -1, 0};
  if (!worldEncoderInitialized) {
    resetWorldViewEncoder();
    return 0;
  }

  const uint8_t currentState =
      (static_cast<uint8_t>(digitalRead(VOLUME_ENCODER_CLK_PIN)) << 1) |
      static_cast<uint8_t>(digitalRead(VOLUME_ENCODER_DT_PIN));
  const uint8_t transition = (worldEncoderPreviousState << 2) | currentState;
  worldEncoderTransitionCount += transitions[transition];
  worldEncoderPreviousState = currentState;

  if (worldEncoderTransitionCount >= 4) {
    worldEncoderTransitionCount = 0;
    return 1;
  }
  if (worldEncoderTransitionCount <= -4) {
    worldEncoderTransitionCount = 0;
    return -1;
  }
  return 0;
}

void drawHouseWorldView() {
  byte room=cityWorld.houseRoom();char count[8];snprintf(count,sizeof(count),"%u/%u",room+1,HOUSE_ROOM_COUNT);
  NativeUI::header(display,cityWorld.houseRoomName(room),count,botany.needsCare()||aquarium.needsCare());
  NativeUI::room(display,room,roomStyle,visualHour(),anim);
  if(!dead&&room!=1&&room!=2&&room!=3) {
    const byte appearance = stage < STAGE_ADULT ? petLineage() : speciesType;
    byte walkStep = 0;
    bool facingLeft = false;
    int petX = 56;
    if (sleeping || lightOff) {
      facingLeft = false;
    } else if (stage == STAGE_EGG) {
      petX = 56 + ((anim & 1U) ? 1 : -1);
    } else {
      petWalkPose(walkStep, facingLeft);
      petX = 12 + walkStep * 4;
    }
    NativeUI::pet(display,petX,62,appearance,stage,anim,sleeping,1,
                  currentEvolutionPath(),facingLeft);
  }
  if(room==0&&!dead)NativeUI::family(display,deepLife.childCount(),cityWorld.partnerLivesAtHome(partnerId,partnerBond),partnerId,lifeWorld.hasCompanion(),anim);
  NativeUI::text(display,"TURN DIAL: WORLD",16,104,102);
  NativeUI::footer(display,"ROOM","ENTER","ROOM");
}

void drawCityWorldView() {
  byte place=cityWorld.cityPlace();char count[8];snprintf(count,sizeof(count),"%u/%u",place+1,CITY_PLACE_COUNT);
  NativeUI::header(display,cityWorld.cityPlaceName(place),count);
  NativeUI::town(display,place,anim);
  NativeUI::text(display,"YOUR NEIGHBORHOOD",13,104,108);
  NativeUI::footer(display,"PLACE","ENTER","PLACE");
}

void enterHouseSelection() {
  static const byte actions[HOUSE_ROOM_COUNT] = {M_ROOM, M_AQUARIUM, M_PLANTROOM, M_GARDEN, M_COOK};
  executeAction(actions[cityWorld.houseRoom() % HOUSE_ROOM_COUNT]);
  waitButtonRelease();
}

void enterCitySelection() {
  static const byte actions[CITY_PLACE_COUNT] = {M_JOB, M_SHOP, M_COOK, M_STUDY, M_EXPLORE, M_NEIGHBORS};
  executeAction(actions[cityWorld.cityPlace() % CITY_PLACE_COUNT]);
  waitButtonRelease();
}

void updateButtons() {
  bool l = digitalRead(BTN_L);
  bool s = digitalRead(BTN_S);
  bool r = readRightButtonDebounced();
  unsigned long now = millis();

  // If the OLED is off, the first button press only wakes the screen.
  // It must not also click/select something behind the dark screen.
  if (screenSleeping || oledIsOff) {
    if (l == LOW || s == LOW || r == LOW) {
      wakeScreen();
    }
    oldL = HIGH;
    oldS = HIGH;
    oldR = HIGH;
    return;
  }

  if (l == LOW || s == LOW || r == LOW) {
    noteInput();
  }

  // In House and City views the buttons navigate the visible world. Rotate
  // the living-world selector back to Pet view whenever the normal category menu is needed.
  if (!menuVisible && dialWorldView != VIEW_PET) {
    if (now - lastButtonTick > 130 && oldL == HIGH && l == LOW) {
      if (dialWorldView == VIEW_HOUSE) cityWorld.previousHouseRoom();
      else cityWorld.previousCityPlace();
      clickSound(); lastButtonTick = now; drawScreen(true);
    }
    if (now - lastButtonTick > 130 && oldS == HIGH && s == LOW) {
      lastButtonTick = now;
      if (dialWorldView == VIEW_HOUSE) enterHouseSelection();
      else enterCitySelection();
      drawScreen(true);
    }
    if (now - lastButtonTick > 60 && oldR == HIGH && r == LOW) {
      if (dialWorldView == VIEW_HOUSE) cityWorld.nextHouseRoom();
      else cityWorld.nextCityPlace();
      clickSound(); lastButtonTick = now; drawScreen(true);
    }
    oldL = l; oldS = s; oldR = r;
    return;
  }

  // D0: previous menu option. If menu is hidden, D0 opens it.
  if (now - lastButtonTick > 130 && oldL == HIGH && l == LOW) {
    if (!menuVisible) {
      menuVisible = true;
    } else {
      if (mainCategory == 0) mainCategory = CATEGORY_COUNT - 1;
      else mainCategory--;
    }

    clickSound();
    lastButtonTick = now;
    drawScreen(true);
  }

  // D1: select. If menu is hidden, D1 opens it instead of doing a random action.
  if (now - lastButtonTick > 130 && oldS == HIGH && s == LOW) {
    lastButtonTick = now;

    if (!menuVisible) {
      menuVisible = true;
      clickSound();
      drawScreen(true);
    } else {
      categoryMenu(mainCategory);
      lastInputAt = millis();
    }
  }

  // D2: next menu option on tap. Hold D2 to hide/exit menu.
  if (oldR == HIGH && r == LOW) {
    rightMenuDownAt = now;
    rightMenuHoldUsed = false;
  }

  if (r == LOW && !rightMenuHoldUsed && rightMenuDownAt > 0 && now - rightMenuDownAt > 550UL) {
    menuVisible = false;
    rightMenuHoldUsed = true;
    clickSound();
    lastButtonTick = now;
    drawScreen(true);
  }

  if (now - lastButtonTick > 60 && oldR == LOW && r == HIGH) {
    if (!rightMenuHoldUsed) {
      if (!menuVisible) {
        menuVisible = true;
      } else {
        mainCategory++;
        if (mainCategory >= CATEGORY_COUNT) mainCategory = 0;
      }

      clickSound();
      lastButtonTick = now;
      drawScreen(true);
    }

    rightMenuDownAt = 0;
    rightMenuHoldUsed = false;
  }

  oldL = l;
  oldS = s;
  oldR = r;
}


// ---------- setup and main ----------

void splash() {
  NativeUI::Scope frame(display);display.clearDisplay();
  NativeUI::room(display,0,0,12,0);NativeUI::pet(display,56,63,0,4,0);
  NativeUI::header(display,"TAMACHIBI");
  NativeUI::text(display,"A LITTLE WORLD",22,104,90);NativeUI::footer(display,"","HELLO!","");
  display.display();delay(900);
}


void openSaveProfiles() { saveProfiles.runMenu(); }
void openDeepLife() { deepLife.runHub(); }
bool lifeResidentAvailable(byte resident, byte hour) { return lifeWorld.residentAvailable(resident, hour); }
const __FlashStringHelper* lifeResidentLocation(byte resident, byte hour) { return lifeWorld.residentLocationName(resident, hour); }
void lifeRecordNeighborInteraction(byte resident, byte interaction, byte relation) {
  lifeWorld.recordNeighborInteraction(resident, interaction, relation);
  deepLife.recordNeighborInteraction(resident, interaction, relation);
  parentDialogue.recordSocialResult(
      relation >= 45 || NeighborStories::actionFromRecord(interaction) ==
                            NeighborStories::CHAT);
}
byte lifeResidentStoryChapter(byte resident) { return lifeWorld.neighborStoryChapter(resident); }
byte lifeResidentInteractionCount(byte resident) { return lifeWorld.neighborInteractionCount(resident); }
bool lifeResidentInvitePending(byte resident) { return lifeWorld.neighborInvitePending(resident); }
void lifeSetResidentInvitePending(byte resident, bool pending) {
  lifeWorld.setNeighborInvitePending(resident, pending);
}

uint8_t detectMainOledAddress() {
  const uint8_t candidates[] = {OLED_ADDR, 0x3C, 0x3D};
  for (uint8_t i = 0; i < sizeof(candidates); ++i) {
    bool duplicate = false;
    for (uint8_t j = 0; j < i; ++j) {
      if (candidates[j] == candidates[i]) duplicate = true;
    }
    if (duplicate) continue;
    Wire.beginTransmission(candidates[i]);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("Main 128x128 OLED at I2C 0x"));
      Serial.println(candidates[i], HEX);
      return candidates[i];
    }
  }
  Serial.println(F("No OLED at 0x3C/0x3D; trying 0x3C (check SDA/SCL/controller)"));
  return OLED_ADDR;
}

void setup() {
  Serial.begin(115200);
  Serial.println(F("Tamachibi v47 ESP32-WROOM-32U starting"));
  Serial.printf("ESP reset reason code: %d; free heap: %u bytes\n",
                static_cast<int>(esp_reset_reason()),
                static_cast<unsigned int>(ESP.getFreeHeap()));

  // Allow USB power and the OLED charge pump to settle cleanly.
  delay(750);

  pinMode(BTN_L, INPUT_PULLUP);
  pinMode(BTN_S, INPUT_PULLUP);
  pinMode(BTN_R, INPUT_PULLUP);
  syncRightButtonDebouncer();
  pinMode(BUZZER_PIN, OUTPUT);
  esp32BuzzerBegin(BUZZER_PIN);
  esp32BuzzerStop(BUZZER_PIN);
  // The encoder module is powered at 3V3. Its push switch is the right/D2
  // button on GPIO25; the existing standalone button remains select on GPIO33.
  // GPIO35/39 are input-only and have no internal pull-ups, so use the
  // module's pull-ups or add 10k resistors for CLK and DT.
  pinMode(VOLUME_ENCODER_CLK_PIN, INPUT);
  pinMode(VOLUME_ENCODER_DT_PIN, INPUT);
  resetWorldViewEncoder();

  // ESP32 supports assigning I2C pins at begin(). The displays can share
  // address 0x3C because they use separate I2C controllers. The main panel
  // probes both common SH1107 addresses so either 0x3C or 0x3D modules work.
  Wire.begin(MAIN_OLED_SDA, MAIN_OLED_SCL);
  // The 128x128 SH1107 frame takes about 90 ms at 100 kHz. Run the main bus
  // at 400 kHz so encoder and button polling are not held up by each redraw.
  Wire.setClock(400000UL);
  Wire1.begin(NEEDS_OLED_SDA, NEEDS_OLED_SCL);
  Wire1.setClock(100000UL);

  EEPROM.begin(TAMACHIBI_EEPROM_BYTES);
  saveProfiles.begin();
  rtc.begin(false);
#if BATTERY_MONITOR_ENABLED
  pinMode(BATTERY_ENABLE_PIN, OUTPUT);
  digitalWrite(BATTERY_ENABLE_PIN, LOW);
  pinMode(BATTERY_SENSE_PIN, INPUT);
#endif
  initExternalRtc();

  const uint8_t mainOledAddress = detectMainOledAddress();
  if (!display.begin(mainOledAddress, true)) {
    Serial.println(F("SH1107 init failed; check 3V3, GND, SDA/SCL, and controller"));
    while (1) { delay(100); }
  }

  display.oled_command(SH110X_NORMALDISPLAY);
  display.oled_command(SH110X_DISPLAYON);
  display.clearDisplay();
  display.display();

  // The second OLED is optional. If it is absent or miswired, the main game
  // still starts normally instead of freezing.
  needsDisplay.begin(NEEDS_OLED_ADDR);
  needsDisplay.showStartup();

  applyBrightness();
  updateBatteryReading(true);
  oledIsOff = false;
  screenSleeping = false;

  randomSeed(micros());

  splash();
  bool loadedSave = loadGameSave();
  unsigned long savedEpoch = loadedSave ? lastRtcEpoch : 0;
  unsigned long bootEpoch = 0;

  if (externalRtcReady) {
    bootEpoch = currentClockEpoch();
  } else {
#if ASK_FOR_CLOCK_ON_BOOT
    bootEpoch = promptForManualClock(savedEpoch);
#else
    bootEpoch = compileDateEpoch();
    rtc.setEpoch(bootEpoch);
#endif
  }

  if (loadedSave) {
    // A hardware RTC can calculate true powered-off elapsed time. With the
    // manual clock, only safe schedule recovery is applied: an overnight
    // power-off counts as sleep, but does not invent neglect or care misses.
    if (externalRtcReady && bootEpoch >= savedEpoch && savedEpoch > 0 && bootEpoch - savedEpoch <= 31UL * 86400UL) {
      rtcWasPetSleeping = (sleeping || lightOff);
      processElapsedSeconds(bootEpoch - savedEpoch);
    } else if (!externalRtcReady) {
      applyManualOfflineSchedule(savedEpoch, bootEpoch);
    }
    lastRtcEpoch = bootEpoch;
    synchronizeDailySchedule(false);
    popup(F("Save loaded"), externalRtcReady ? F("Clock restored") : F("Real time synchronized"), 750);
  } else {
    waitButtonRelease();
    delay(120);
    aquarium.reset();
    botany.reset();
    cityWorld.reset();
    paradise.reset();
    lifeWorld.reset();
    deepLife.reset();
    parentDialogue.reset();
    growingTogether.reset();
    chooseEgg();
    startNewPet();
    lastRtcEpoch = bootEpoch;
    synchronizeDailySchedule(false);
    saveGameNow();
  }

  saveProfiles.createBootBackup();

  // Repair any older save that already accumulated enough age ticks but
  // still stored the Egg stage because its timer was repeatedly reset.
  if (!dead) updateGrowthStage();
  if (stage >= STAGE_ADULT) discoverSpecies(speciesType);
  if (gardenPlant < PLANT_COUNT) discoveredPlants |= (1U << gardenPlant);

  lastLifeClockEpoch = currentClockEpoch();
  lifeSecondRemainder = 0;
  lastInputAt = millis();
  lastGameTick = millis();
  lastAnimTick = millis();
  updateWorldViewFromEncoder(true);
  drawScreen(true);
  serviceNeedsDisplay(true);
}

void loop() {
  updateWorldViewFromEncoder(false);
  updateButtons();

  unsigned long now = millis();
  serviceSoftBuzzer();
  updateBatteryReading(false);

  if (anyButtonDown()) {
    lastInputAt = now;
  }

  // The OLED may turn off when idle; the pet clock and schedule continue running.
#if !ALWAYS_AWAKE_MODE
  if (!screenSleeping && !oledIsOff && now - lastInputAt >= OLED_OFF_TIMEOUT_MS) {
    turnOledOffIdle();
  }
#else
  screenSleeping = false;
  if (oledIsOff) forceDisplayOn();
#endif
  unsigned long ageBeforeClockService = ageTicks;
  serviceLifeClock();
  synchronizeDailySchedule(false);
  bool lifeAdvanced = (ageTicks != ageBeforeClockService);

  // The secondary OLED remains active even when the main OLED is idle.
  // It redraws quickly enough to show slow and urgent blink rates.
  serviceNeedsDisplay(lifeAdvanced);

  if (!screenSleeping && !oledIsOff && lifeAdvanced) drawScreen(true);

  // Spontaneous parent-child conversations only open from the safe idle main
  // screen. They never interrupt a minigame, shop, city action, emergency,
  // urgent care call, sleep, or offline catch-up simulation.
  const bool narrativeCooldownDone = lastNarrativeInteractionAtMs == 0 ||
      now - lastNarrativeInteractionAtMs >= NARRATIVE_COOLDOWN_MS;
  if (narrativeCooldownDone && !dead && !sleeping && !lightOff && !screenSleeping && !oledIsOff &&
      !rtcCatchUpMode && dialWorldView == VIEW_PET && pendingCallType == 0 &&
      !paradise.emergencyPending() && !anyButtonDown()) {
    GrowingTogetherContext growingContext = makeGrowingTogetherContext();
    if (growingTogether.shouldAutoOpen(growingContext, now - lastInputAt)) {
      runGrowingTogetherCheckIn(false);
      drawScreen(true);
      return;
    }
    ParentDialogueContext context = makeParentDialogueContext();
    if (parentDialogue.shouldAutoOpen(context, now - lastInputAt)) {
      runParentDialogueCheckIn(false);
      drawScreen(true);
      return;
    }
  }

  const bool petIsWalking = !menuVisible &&
      (dialWorldView == VIEW_PET || dialWorldView == VIEW_HOUSE) &&
      !dead && !sleeping && !lightOff && !petAway &&
      !isScheduledAwayTime() && !rtcCatchUpMode;
  const bool recentUiActivity = now - lastInputAt < ACTIVE_ANIMATION_WINDOW_MS;
  if (!screenSleeping && !oledIsOff && (petIsWalking || recentUiActivity) &&
      now - lastAnimTick >= ANIM_TICK_MS) {
    lastAnimTick = now;
    anim++;
    drawScreen(true);
  }

  maybeAutoSave(false);
}


