#include "Aquarium.h"
#include "NativeUI.h"
#include "AquariumArt.h"
#include <stddef.h>
#include <string.h>

// Main-game state and helpers used to integrate aquarium care with the pet.
extern byte happy, energy, health, clean, discipline;
extern byte ownerBond, ownerTrust, skillKind, skillSmart, skillStyle, cleanlinessScore;
extern byte stage, habitat;
extern unsigned int coins;
extern unsigned long ageTicks;
extern bool dead, sleeping, rtcCatchUpMode;
extern char petName[7];

extern void addStat(byte &value, byte amount);
extern void subStat(byte &value, byte amount);
extern void addSkill(byte &value, byte amount);
extern void addCoins(byte amount);
extern bool spendCoins(byte amount);
extern void changeBond(int amount);
extern void changeTrust(int amount);
extern void markRoutineAction(byte routineBit);
extern void recordGoalEvent(byte eventType, byte amount);
extern void markSaveDirty();
extern void clampAll();
extern void waitButtonRelease();
extern bool rightHoldExitCheck();
extern void clickSound();
extern void goodSound();
extern void badSound();
extern void winSound();
extern void careSound();
extern void drawCurrentPetTiny(int x, int y);
extern unsigned long difficultyTicks(unsigned long normalTicks);

namespace {
constexpr uint8_t EMPTY_FISH = 255;
constexpr uint32_t TICKS_PER_HOUR_AQ = 12UL;
constexpr uint32_t TICKS_PER_DAY_AQ = 288UL;
constexpr uint8_t ROUTINE_CARE_AQ = 4;
constexpr uint8_t ROUTINE_TALK_AQ = 8;
constexpr uint8_t ROUTINE_PLAY_AQ = 16;
constexpr uint8_t AQUARIUM_METADATA_MARKER = 0xA0;

const char *personalityName(uint8_t personality) {
  static const char *const names[4] = {"BOLD", "SHY", "CURIOUS", "SOCIABLE"};
  return names[personality & 3U];
}

const char *quirkName(uint8_t quirk) {
  static const char *const names[4] = {
    "BUBBLE CHASER", "PLANT NAPPER", "GLASS GREETER", "LOOP SWIMMER"
  };
  return names[quirk & 3U];
}

const char *foodName(uint8_t food) {
  static const char *const names[3] = {"flakes", "tiny pellets", "green bites"};
  return names[food % 3U];
}

const char *favoriteDecorName(uint8_t decor) {
  static const char *const names[5] = {
    "plants", "castle", "bubble arch", "coral garden", "moon grotto"
  };
  return names[(decor == 0 ? 1 : decor) - 1U];
}

const char *memoryName(uint8_t memory) {
  switch (memory & 15U) {
    case AQUA_MEMORY_FIRST_GREET: return "our first hello";
    case AQUA_MEMORY_FIRST_MEAL: return "the first snack you gave me";
    case AQUA_MEMORY_FAVORITE_MEAL: return "the favorite meal you found";
    case AQUA_MEMORY_BUBBLE_PLAY: return "our bubble-chasing game";
    case AQUA_MEMORY_GENTLE_TOUCH: return "the time you waited for me";
    case AQUA_MEMORY_RECOVERY: return "how you helped me feel better";
    case AQUA_MEMORY_CLEAN_WATER: return "the day our water sparkled again";
    case AQUA_MEMORY_FAVORITE_DECOR: return "finding my favorite place to hide";
    case AQUA_MEMORY_GROWTH_YOUNG: return "when I first grew bigger";
    case AQUA_MEMORY_GROWTH_JUVENILE: return "the day I became a young fish";
    case AQUA_MEMORY_GROWTH_ADULT: return "the day I became grown-up";
    case AQUA_MEMORY_RETURN_VISIT: return "you coming back to see me";
    case AQUA_MEMORY_TREASURE: return "finding a little tank treasure";
    case AQUA_MEMORY_COMFORT: return "settling in beside my friends";
    case AQUA_MEMORY_WATCH_TANK: return "watching the water together";
    default: return "our quiet time by the tank";
  }
}

const char *memoryPhrase(uint8_t memory, uint8_t variant, const char *name,
                         const char *food, const char *decor) {
  static const char *const lines[16][4] = {
    {"I like having you here with us.", "The tank feels calmer when we watch it together.", "I have a small story about our fish.", "We can tell the fish feel safe when you are near."},
    {"%s remembers the first time you said hello.", "%s was shy at first, then swam closer to your voice.", "%s always looks for you after we visit.", "That first hello still makes %s swim closer."},
    {"%s remembers the first snack you shared.", "The first meal made %s trust us a little more.", "I remember when %s tried food here for the first time.", "That first little meal was a good day for %s."},
    {"You found out %s loves %s.", "%s wiggles when its favorite snack arrives.", "That favorite %s makes %s come right to the glass.", "%s perks up when you bring its favorite snack."},
    {"%s darts after bubbles like it is a grand adventure.", "%s never gives up on the last bubble.", "I still laugh when %s spins around to catch one.", "The bubbles always give %s something to chase."},
    {"You waited gently, and %s chose to come closer.", "%s trusts the quiet moments you share.", "I think %s recognizes your patient visits.", "I can tell %s trusts the quiet waits."},
    {"You helped %s get well, and it stayed close afterward.", "%s seemed brighter when you cared for it.", "I remember watching %s feel better day by day.", "It was a lovely thing watching %s recover."},
    {"The clear water made %s explore every corner.", "%s swam right up after you cleaned the tank.", "That fresh-water day made the whole tank feel new.", "The fresh water made the tank feel bigger."},
    {"%s always swims back to the %s.", "That %s nook is %s's favorite place to rest.", "The %s hideaway feels made for %s.", "%s likes this tank best with its %s nook."},
    {"I remember when %s outgrew its tiny-fry shape.", "%s got a little braver as it grew.", "You were here when %s started looking bigger.", "I liked seeing %s grow out of its first tiny shape."},
    {"%s started swimming with a young fish's confidence.", "I noticed %s exploring farther as it grew.", "Our little fish kept changing right before us.", "Every week, %s seems a little more sure of itself."},
    {"You watched %s become a grown-up fish.", "%s still has the same little quirks from when it was small.", "I am glad we got to see %s grow up here.", "Grown-up %s still carries its own little habits."},
    {"%s is happiest when you come back to the tank.", "Even a short visit seems to brighten %s's swim.", "%s knows your visits are part of our routine.", "You are becoming part of %s's daily routine."},
    {"We found a tiny surprise together, and %s investigated first.", "%s inspected our little tank treasure from every side.", "That surprise gave %s a new thing to explore.", "We still talk about the day we found it."},
    {"%s settles in when the tank feels peaceful.", "%s likes having familiar friends nearby.", "I think %s feels at home in the place we made.", "The whole group seems calmer with %s around."},
    {"Watching %s drift through the tank is one of my favorite quiet things.", "%s always finds something new when we stop and look.", "I like the little routine of watching %s with you.", "The water always has a new story when we pause."}
  };
  static char line[120];
  const char *source = lines[memory & 15U][variant % 4U];
  if (memory == AQUA_MEMORY_FAVORITE_MEAL && variant == 0)
    snprintf(line, sizeof(line), source, name, food);
  else if (memory == AQUA_MEMORY_FAVORITE_MEAL && variant == 2)
    snprintf(line, sizeof(line), source, food, name);
  else if (memory == AQUA_MEMORY_FAVORITE_DECOR) {
    if (variant == 1 || variant == 2)
      snprintf(line, sizeof(line), source, decor, name);
    else
      snprintf(line, sizeof(line), source, name, decor);
  }
  else
    snprintf(line, sizeof(line), source, name);
  return line;
}

uint8_t boundedAdd(uint8_t value, uint8_t amount, uint8_t maximum = 10) {
  uint16_t next = static_cast<uint16_t>(value) + amount;
  return static_cast<uint8_t>(next > maximum ? maximum : next);
}

uint8_t boundedSub(uint8_t value, uint8_t amount) {
  return value > amount ? static_cast<uint8_t>(value - amount) : 0;
}

}

AquariumSystem::AquariumSystem(size_t storageBase, GameDisplay &display,
                               uint8_t buttonLeft, uint8_t buttonSelect,
                               uint8_t buttonRight)
    : storeA_(storageBase), storeB_(storageBase + sizeof(AquariumSave)),
      display_(display), btnL_(buttonLeft), btnS_(buttonSelect), btnR_(buttonRight) {
  reset();
}

void AquariumSystem::reset() {
  sequence_ = 0;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
    species_[i] = EMPTY_FISH;
    names_[i][0] = '\0';
    hunger_[i] = 8;
    mood_[i] = 8;
    health_[i] = 10;
    friendship_[i] = 0;
    ageTicks_[i] = 0;
    personalitySeed_[i] = 0;
    lastMemory_[i] = AQUA_MEMORY_NONE;
    favoriteMemory_[i] = AQUA_MEMORY_NONE;
    growthBranch_[i] = 1;
    actionPose_[i] = 0;
    actionFrames_[i] = 0;
  }
  tankClean_ = 10;
  waterQuality_ = 10;
  tankDecor_ = 0;
  tankLevel_ = 1;
  foodStock_ = 5;
  remedyStock_ = 1;
  selectedSlot_ = 0;
  aquariumVisits_ = 0;
  careStreak_ = 0;
  surpriseType_ = 0;
  discoveredMask_ = 0;
  lastRewardDay_ = 0;
  lastInteractionTick_ = 0;
  firstFedMask_ = 0;
}

uint32_t AquariumSystem::checksum(const AquariumSave &save) const {
  const uint8_t *raw = reinterpret_cast<const uint8_t *>(&save);
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < offsetof(AquariumSave, checksum); ++i) {
    hash ^= raw[i];
    hash *= 16777619UL;
  }
  return hash;
}

bool AquariumSystem::valid(const AquariumSave &save) const {
  return save.magic == AQUARIUM_SAVE_MAGIC &&
         save.version == AQUARIUM_SAVE_VERSION &&
         save.checksum == checksum(save);
}

void AquariumSystem::fillSave(AquariumSave &save) const {
  memset(&save, 0, sizeof(save));
  save.magic = AQUARIUM_SAVE_MAGIC;
  save.version = AQUARIUM_SAVE_VERSION;
  save.sequence = sequence_ + 1UL;
  memcpy(save.species, species_, sizeof(species_));
  memcpy(save.names, names_, sizeof(names_));
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
    save.hunger[i] = static_cast<uint8_t>((personalitySeed_[i] << 4) | (hunger_[i] & 15U));
    save.mood[i] = static_cast<uint8_t>((lastMemory_[i] << 4) | (mood_[i] & 15U));
    save.health[i] = static_cast<uint8_t>((favoriteMemory_[i] << 4) | (health_[i] & 15U));
  }
  save.discoveredMask = discoveredMask_ & 0x0FFFU;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
    save.friendship[i] = static_cast<uint8_t>((friendship_[i] & 0x7FU) |
                                             ((growthBranch_[i] & 1U) << 7));
    save.discoveredMask |= static_cast<uint16_t>(((growthBranch_[i] >> 1) & 1U) << (12U + i));
  }
  memcpy(save.ageTicks, ageTicks_, sizeof(ageTicks_));
  save.tankClean = tankClean_;
  save.waterQuality = waterQuality_;
  save.tankDecor = tankDecor_;
  save.tankLevel = tankLevel_;
  save.foodStock = foodStock_;
  save.remedyStock = remedyStock_;
  save.selectedSlot = static_cast<uint8_t>((selectedSlot_ & 3U) |
                                           ((firstFedMask_ & 15U) << 4U));
  save.aquariumVisits = aquariumVisits_;
  save.careStreak = careStreak_;
  save.surpriseType = static_cast<uint8_t>(AQUARIUM_METADATA_MARKER | (surpriseType_ & 15U));
  save.lastRewardDay = lastRewardDay_;
  save.lastInteractionTick = lastInteractionTick_;
  save.checksum = checksum(save);
}

void AquariumSystem::applySave(const AquariumSave &save) {
  sequence_ = save.sequence;
  memcpy(species_, save.species, sizeof(species_));
  memcpy(names_, save.names, sizeof(names_));
  memcpy(friendship_, save.friendship, sizeof(friendship_));
  memcpy(ageTicks_, save.ageTicks, sizeof(ageTicks_));
  const bool hasPackedMetadata = (save.surpriseType & 0xF0U) == AQUARIUM_METADATA_MARKER;
  firstFedMask_ = hasPackedMetadata
      ? static_cast<uint8_t>((save.selectedSlot >> 4U) & 15U) : 0;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
    hunger_[i] = save.hunger[i] & 15U;
    mood_[i] = save.mood[i] & 15U;
    health_[i] = save.health[i] & 15U;
    personalitySeed_[i] = save.hunger[i] >> 4;
    lastMemory_[i] = save.mood[i] >> 4;
    favoriteMemory_[i] = save.health[i] >> 4;
    growthBranch_[i] = hasPackedMetadata
        ? static_cast<uint8_t>(((save.friendship[i] >> 7U) & 1U) |
              (((save.discoveredMask >> (12U + i)) & 1U) << 1U))
        : 1;
    friendship_[i] = save.friendship[i] & 0x7FU;
    actionPose_[i] = 0;
    actionFrames_[i] = 0;
    if (slotOccupied(i) && personalitySeed_[i] == 0) {
      uint8_t seed = static_cast<uint8_t>((species_[i] * 7U + i * 5U +
                         ageTicks_[i] + static_cast<uint8_t>(names_[i][0])) % 15U + 1U);
      initializeFishProfile(i, seed);
    }
    if (!slotOccupied(i)) firstFedMask_ &= static_cast<uint8_t>(~(1U << i));
  }
  tankClean_ = save.tankClean;
  waterQuality_ = save.waterQuality;
  tankDecor_ = save.tankDecor;
  tankLevel_ = save.tankLevel;
  foodStock_ = save.foodStock;
  remedyStock_ = save.remedyStock;
  selectedSlot_ = save.selectedSlot & 3U;
  aquariumVisits_ = save.aquariumVisits;
  careStreak_ = save.careStreak;
  surpriseType_ = hasPackedMetadata ? save.surpriseType & 15U : save.surpriseType;
  discoveredMask_ = save.discoveredMask & 0x0FFFU;
  lastRewardDay_ = save.lastRewardDay;
  lastInteractionTick_ = save.lastInteractionTick;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) names_[i][6] = '\0';
  clampState();
}

bool AquariumSystem::load() {
  AquariumSave a = storeA_.read();
  AquariumSave b = storeB_.read();
  const bool validA = valid(a);
  const bool validB = valid(b);
  if (!validA && !validB) {
    reset();
    return false;
  }
  if (validA && validB) applySave(a.sequence >= b.sequence ? a : b);
  else applySave(validA ? a : b);
  return true;
}

void AquariumSystem::saveNow() {
  AquariumSave save;
  fillSave(save);
  if (save.sequence & 1UL) storeA_.write(save);
  else storeB_.write(save);
  sequence_ = save.sequence;
}

void AquariumSystem::clampState() {
  tankClean_ = min((uint8_t)10, tankClean_);
  waterQuality_ = min((uint8_t)10, waterQuality_);
  tankDecor_ = min((uint8_t)5, tankDecor_);
  tankLevel_ = constrain(tankLevel_, (uint8_t)1, (uint8_t)3);
  selectedSlot_ %= AQUARIUM_FISH_SLOTS;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
    if (species_[i] >= AQUARIUM_SPECIES_COUNT) species_[i] = EMPTY_FISH;
    hunger_[i] = min((uint8_t)10, hunger_[i]);
    mood_[i] = min((uint8_t)10, mood_[i]);
    health_[i] = constrain(health_[i], (uint8_t)1, (uint8_t)10);
    friendship_[i] = min((uint8_t)100, friendship_[i]);
  }
}

bool AquariumSystem::slotOccupied(uint8_t slot) const {
  return slot < AQUARIUM_FISH_SLOTS && species_[slot] < AQUARIUM_SPECIES_COUNT;
}

uint8_t AquariumSystem::fishCount() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) if (slotOccupied(i)) ++count;
  return count;
}

uint8_t AquariumSystem::capacity() const {
  return min((uint8_t)AQUARIUM_FISH_SLOTS, (uint8_t)(tankLevel_ + 1));
}

bool AquariumSystem::hasFish() const { return fishCount() > 0; }

void AquariumSystem::developerUnlockAll() {
  tankLevel_ = 3;
  tankDecor_ = 5;
  foodStock_ = 99;
  remedyStock_ = 99;
  discoveredMask_ = static_cast<uint16_t>((1U << AQUARIUM_SPECIES_COUNT) - 1U);
  tankClean_ = 10;
  waterQuality_ = 10;
  markSaveDirty();
}

void AquariumSystem::developerAddFish(uint8_t species) {
  species %= AQUARIUM_SPECIES_COUNT;
  tankLevel_ = 3;
  uint8_t slot = firstEmptySlot();
  if (slot == EMPTY_FISH) slot = selectedSlot_ % AQUARIUM_FISH_SLOTS;
  firstFedMask_ &= static_cast<uint8_t>(~(1U << slot));
  species_[slot] = species;
  strncpy(names_[slot], defaultFishName(species), 7);
  names_[slot][6] = '\0';
  hunger_[slot] = 10;
  mood_[slot] = 10;
  health_[slot] = 10;
  friendship_[slot] = 100;
  ageTicks_[slot] = static_cast<uint16_t>(3UL * TICKS_PER_DAY_AQ);
  lastMemory_[slot] = AQUA_MEMORY_NONE;
  favoriteMemory_[slot] = AQUA_MEMORY_NONE;
  growthBranch_[slot] = 1;
  initializeFishProfile(slot, static_cast<uint8_t>(random(1, 16)));
  selectedSlot_ = slot;
  discoveredMask_ |= static_cast<uint16_t>(1U << species);
  foodStock_ = max((uint8_t)20, foodStock_);
  remedyStock_ = max((uint8_t)10, remedyStock_);
  tankClean_ = 10;
  waterQuality_ = 10;
  markSaveDirty();
}

void AquariumSystem::developerFillTank() {
  developerUnlockAll();
  firstFedMask_ = 0;
  const uint8_t testSpecies[AQUARIUM_FISH_SLOTS] = {0, 7, 10, 11};
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
    species_[i] = testSpecies[i];
    strncpy(names_[i], defaultFishName(testSpecies[i]), 7);
    names_[i][6] = '\0';
    hunger_[i] = 10;
    mood_[i] = 10;
    health_[i] = 10;
    friendship_[i] = 100;
    ageTicks_[i] = static_cast<uint16_t>(3UL * TICKS_PER_DAY_AQ);
    lastMemory_[i] = AQUA_MEMORY_NONE;
    favoriteMemory_[i] = AQUA_MEMORY_NONE;
    growthBranch_[i] = 1;
    initializeFishProfile(i, static_cast<uint8_t>(i * 3U + 1U));
  }
  selectedSlot_ = 0;
  markSaveDirty();
}

void AquariumSystem::developerMaxCare() {
  tankClean_ = 10;
  waterQuality_ = 10;
  foodStock_ = max((uint8_t)20, foodStock_);
  remedyStock_ = max((uint8_t)10, remedyStock_);
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
    if (!slotOccupied(i)) continue;
    hunger_[i] = 10;
    mood_[i] = 10;
    health_[i] = 10;
    friendship_[i] = 100;
  }
  markSaveDirty();
}

void AquariumSystem::developerNeglectTank() {
  if (!hasFish()) developerAddFish(0);
  tankClean_ = 1;
  waterQuality_ = 1;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
    if (!slotOccupied(i)) continue;
    hunger_[i] = 1;
    mood_[i] = 2;
    health_[i] = 3;
  }
  markSaveDirty();
}

bool AquariumSystem::healthy() const {
  if (!hasFish() || tankClean_ < 6 || waterQuality_ < 6) return false;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
    if (slotOccupied(i) && (hunger_[i] < 4 || health_[i] < 5 || mood_[i] < 4)) return false;
  }
  return true;
}

bool AquariumSystem::needsCare() const {
  if (!hasFish()) return false;
  if (tankClean_ <= 3 || waterQuality_ <= 3) return true;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
    if (slotOccupied(i) && (hunger_[i] <= 2 || health_[i] <= 3)) return true;
  }
  return false;
}

uint8_t AquariumSystem::firstEmptySlot() const {
  const uint8_t limit = capacity();
  for (uint8_t i = 0; i < limit; ++i) if (!slotOccupied(i)) return i;
  return EMPTY_FISH;
}

uint8_t AquariumSystem::nextOccupiedSlot(uint8_t from, int8_t direction) const {
  if (!hasFish()) return 0;
  uint8_t slot = from % AQUARIUM_FISH_SLOTS;
  for (uint8_t step = 0; step < AQUARIUM_FISH_SLOTS; ++step) {
    slot = direction < 0 ? (slot == 0 ? AQUARIUM_FISH_SLOTS - 1 : slot - 1)
                         : static_cast<uint8_t>((slot + 1) % AQUARIUM_FISH_SLOTS);
    if (slotOccupied(slot)) return slot;
  }
  return from;
}

const char *AquariumSystem::selectedFishName() const {
  if (!slotOccupied(selectedSlot_)) return "NO FISH";
  return names_[selectedSlot_][0] ? names_[selectedSlot_] : defaultFishName(species_[selectedSlot_]);
}

uint8_t AquariumSystem::selectedFishPersonality() const {
  return personalitySeed_[selectedSlot_ % AQUARIUM_FISH_SLOTS] & 3U;
}

uint8_t AquariumSystem::selectedFishQuirk() const {
  return (personalitySeed_[selectedSlot_ % AQUARIUM_FISH_SLOTS] >> 2) & 3U;
}

uint8_t AquariumSystem::selectedFishFavoriteFood() const {
  return fishFavoriteFood(selectedSlot_);
}

uint8_t AquariumSystem::fishFavoriteFood(uint8_t slot) const {
  return (personalitySeed_[slot % AQUARIUM_FISH_SLOTS] + species_[slot % AQUARIUM_FISH_SLOTS]) % 3U;
}

uint8_t AquariumSystem::fishFavoriteDecor(uint8_t slot) const {
  slot %= AQUARIUM_FISH_SLOTS;
  return AquariumArt::preferredDecor(species_[slot], personalitySeed_[slot]);
}

void AquariumSystem::initializeFishProfile(uint8_t slot, uint8_t seed) {
  if (slot >= AQUARIUM_FISH_SLOTS) return;
  seed &= 15U;
  if (seed == 0) seed = static_cast<uint8_t>((species_[slot] * 7U + slot * 5U + 1U) % 15U + 1U);
  personalitySeed_[slot] = seed;
}

void AquariumSystem::setMemory(uint8_t slot, uint8_t memory) {
  if (!slotOccupied(slot)) return;
  memory &= 15U;
  if (lastMemory_[slot] == memory && memory != AQUA_MEMORY_NONE)
    favoriteMemory_[slot] = memory;
  else if (favoriteMemory_[slot] == AQUA_MEMORY_NONE && friendship_[slot] >= 24)
    favoriteMemory_[slot] = memory;
  lastMemory_[slot] = memory;
}

void AquariumSystem::setActionPose(uint8_t slot, uint8_t pose, uint8_t frames) {
  if (slot >= AQUARIUM_FISH_SLOTS) return;
  actionPose_[slot] = pose;
  actionFrames_[slot] = frames;
}

const __FlashStringHelper *AquariumSystem::speciesName(uint8_t species) const {
  switch (species % AQUARIUM_SPECIES_COUNT) {
    case 0: return F("Guppy");
    case 1: return F("Goldfish");
    case 2: return F("Betta");
    case 3: return F("Neon Tetra");
    case 4: return F("Molly");
    case 5: return F("Cory");
    case 6: return F("Angelfish");
    case 7: return F("Koi");
    case 8: return F("Puffer");
    case 9: return F("Clownfish");
    case 10: return F("Moonfin");
    default: return F("Starfin");
  }
}

const char *AquariumSystem::defaultFishName(uint8_t species) const {
  static const char names[AQUARIUM_SPECIES_COUNT][7] = {
    "GUPPY", "GOLDIE", "BETTA", "NEON", "MOLLY", "CORY",
    "ANGEL", "KOI", "PUFF", "CLOWN", "MOON", "STAR"
  };
  return names[species % AQUARIUM_SPECIES_COUNT];
}

uint8_t AquariumSystem::speciesCost(uint8_t species) const {
  static const uint8_t costs[AQUARIUM_SPECIES_COUNT] = {3, 5, 6, 5, 6, 7, 10, 12, 14, 12, 18, 22};
  return costs[species % AQUARIUM_SPECIES_COUNT];
}

bool AquariumSystem::speciesUnlocked(uint8_t species) const {
  if (species < 6) return true;
  if (species < 9) return tankLevel_ >= 2;
  return tankLevel_ >= 3;
}

const __FlashStringHelper *AquariumSystem::decorName(uint8_t decor) const {
  switch (decor) {
    case 0: return F("Plain gravel");
    case 1: return F("Water plants");
    case 2: return F("Tiny castle");
    case 3: return F("Bubble arch");
    case 4: return F("Coral garden");
    default: return F("Moon grotto");
  }
}

uint8_t AquariumSystem::fishLifeStage(uint8_t slot) const {
  if (!slotOccupied(slot)) return 0;
  if (ageTicks_[slot] < TICKS_PER_DAY_AQ) return 0;
  if (ageTicks_[slot] < 3UL * TICKS_PER_DAY_AQ) return 1;
  if (ageTicks_[slot] < 7UL * TICKS_PER_DAY_AQ) return 2;
  return 3;
}

bool AquariumSystem::allFishComfortable() const {
  if (!hasFish()) return false;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
    if (slotOccupied(i) && (hunger_[i] < 5 || mood_[i] < 5 || health_[i] < 6)) return false;
  }
  return tankClean_ >= 6 && waterQuality_ >= 6;
}

void AquariumSystem::showTextPopup(const char *line1, const char *line2, uint16_t holdMs) {
  display_.clearDisplay();
  display_.setTextWrap(false);
  display_.setTextSize(1);
  display_.setTextColor(SSD1306_WHITE);
  display_.drawRoundRect(3, 13, 122, 39, 5, SSD1306_WHITE);
  display_.setCursor(10, 23);
  display_.print(line1 ? line1 : "");
  display_.setCursor(10, 36);
  display_.print(line2 ? line2 : "");
  display_.display();
  delay(holdMs);
}

void AquariumSystem::showFlashPopup(const __FlashStringHelper *line1,
                                    const __FlashStringHelper *line2,
                                    uint16_t holdMs) {
  display_.clearDisplay();
  display_.setTextWrap(false);
  display_.setTextSize(1);
  display_.setTextColor(SSD1306_WHITE);
  display_.drawRoundRect(3, 13, 122, 39, 5, SSD1306_WHITE);
  display_.setCursor(10, 23);
  display_.print(line1);
  display_.setCursor(10, 36);
  display_.print(line2);
  display_.display();
  delay(holdMs);
}

void AquariumSystem::drawSmallIcon(int x, int y, bool inverse) {
  const uint16_t color = inverse ? SSD1306_BLACK : SSD1306_WHITE;
  display_.drawRect(x + 2, y + 3, 16, 12, color);
  display_.drawLine(x + 3, y + 7, x + 17, y + 7, color);
  display_.fillCircle(x + 10, y + 11, 2, color);
  display_.fillTriangle(x + 7, y + 11, x + 4, y + 8, x + 4, y + 14, color);
  display_.drawPixel(x + 11, y + 10, inverse ? SSD1306_WHITE : SSD1306_BLACK);
}

void AquariumSystem::drawFishSprite(uint8_t species,int x,int y,bool facingRight,uint8_t lifeStage,bool selected) {
  const uint8_t slot = selectedSlot_ % AQUARIUM_FISH_SLOTS;
  const uint8_t quirk = slotOccupied(slot) ? (personalitySeed_[slot] >> 2) & 3U : 0;
  AquariumArt::fish(display_, species, x, y, lifeStage, facingRight,
                    static_cast<uint8_t>(millis() / 180UL), quirk, selected,
                    slotOccupied(slot) ? actionPose_[slot] : 0,
                    slotOccupied(slot) ? growthBranch_[slot] : 1);
}

void AquariumSystem::drawTankScene(uint8_t selected,uint16_t frame,bool includePet) {
  NativeUI::Scope native(display_);
  display_.clearDisplay();
  const bool hasSelected = slotOccupied(selected);
  char count[10]; snprintf(count, sizeof(count), "%u/%u", fishCount(), capacity());
  NativeUI::header(display_, hasSelected ? names_[selected] : "AQUARIUM", count, needsCare());

  // The tank has depth and open swimming lanes; every plant and bubble is
  // drawn first so the foreground fish remain crisp and readable.
  display_.drawRoundRect(3, 20, 122, 91, 5, SSD1306_WHITE);
  display_.drawFastHLine(8, 27, 112, SSD1306_WHITE);
  const uint8_t ripple = static_cast<uint8_t>((frame >> 2) % 8U);
  display_.drawLine(10 + ripple, 29, 24 + ripple, 29, SSD1306_WHITE);
  display_.drawLine(42 + (7 - ripple), 30, 54 + (7 - ripple), 30, SSD1306_WHITE);
  display_.drawLine(79 + ripple, 29, 94 + ripple, 29, SSD1306_WHITE);

  // Rockwork, plants, and the floor become a small habitat rather than a
  // single decoration number. The tier grows the scene cumulatively.
  display_.drawFastHLine(7, 101, 114, SSD1306_WHITE);
  for (uint8_t i = 0; i < 12; ++i) {
    const int x = 9 + i * 9;
    const int lift = ((i + frame / 5) & 1U) ? 1 : 0;
    display_.drawPixel(x, 105 + lift, SSD1306_WHITE);
  }
  if (tankDecor_ >= 1) {
    NativeUI::plant(display_, 9, 76, 0, 3, false);
    NativeUI::plant(display_, 101, 76, 1, 3, false);
  }
  if (tankDecor_ >= 2) {
    display_.drawRoundRect(48, 85, 29, 16, 3, SSD1306_WHITE);
    display_.drawRoundRect(54, 77, 17, 10, 2, SSD1306_WHITE);
    display_.drawRect(60, 90, 5, 11, SSD1306_WHITE);
  }
  if (tankDecor_ >= 3) {
    display_.drawCircle(91, 88, 8, SSD1306_WHITE);
    display_.drawCircle(91, 88, 5, SSD1306_WHITE);
  }
  if (tankDecor_ >= 4) {
    NativeUI::plant(display_, 26, 79, 3, 3, false);
    display_.drawLine(31, 88, 28, 82, SSD1306_WHITE);
    display_.drawLine(31, 88, 35, 82, SSD1306_WHITE);
  }
  if (tankDecor_ >= 5) {
    display_.drawCircle(31, 47, 8, SSD1306_WHITE);
    display_.drawCircle(31, 47, 4, SSD1306_WHITE);
    display_.drawPixel(31, 47, SSD1306_WHITE);
  }
  for (uint8_t i = 0; i < 3; ++i) {
    const int bx = 21 + i * 39;
    const int by = 36 + ((frame / 3 + i * 7) % 21);
    display_.drawCircle(bx, by, i == 1 ? 2 : 1, SSD1306_WHITE);
  }

  // Individual route speed, depth, and swimming habit come from each fish's
  // persistent seed. The selected fish is drawn last with its interaction mark.
  for (uint8_t pass = 0; pass < 2; ++pass) {
    for (uint8_t slot = 0; slot < AQUARIUM_FISH_SLOTS; ++slot) {
    const bool isSelected = hasSelected && slot == selected;
    if ((pass == 0 && isSelected) || (pass == 1 && !isSelected)) continue;
    if (!slotOccupied(slot)) continue;
    const uint8_t seed = personalitySeed_[slot];
    const uint8_t personality = seed & 3U;
    const uint8_t quirk = (seed >> 2) & 3U;
    uint8_t rank = 0;
    for (uint8_t earlier = 0; earlier < slot; ++earlier)
      if (slotOccupied(earlier)) ++rank;
    const uint8_t total = fishCount();
    const AquariumArt::SwimPoint path = AquariumArt::swimPoint(seed, rank, total, frame);
    const bool right = path.facingRight;
    int px = path.x;
    int py = path.y;
    const int vertical = path.y - (rank < 2 ? 40 + rank * 3 : 64 + (rank - 2) * 3);
    if (quirk == 1) py += 4 + vertical / 2;
    else if (quirk == 2) py -= 3;
    else if (quirk == 3) {
      const uint8_t orbit = static_cast<uint8_t>((frame / 9U + seed) % 4U);
      px += orbit == 0 ? 0 : orbit == 1 ? 4 : orbit == 2 ? 0 : -4;
      py += orbit == 1 ? 5 : orbit == 3 ? -5 : 0;
    }
    if (personality == 3 && total > 1) {
      const int groupX = (rank & 1U) ? 35 : 91;
      const uint8_t schoolPhase = static_cast<uint8_t>((frame + slot * 23U) % 96U);
      const int schoolPull = schoolPhase < 48U ? schoolPhase : 96U - schoolPhase;
      px = (px * (96 - schoolPull) + groupX * schoolPull) / 96;
      py -= schoolPull / 24;
    }
    const uint8_t favoriteDecor = AquariumArt::preferredDecor(species_[slot], seed);
    if (tankDecor_ >= favoriteDecor) {
      int targetX = 63, targetY = 68;
      switch (favoriteDecor) {
        case 1: targetX = (rank & 1U) ? 104 : 12; targetY = 68; break;
        case 2: targetX = 63; targetY = 72; break;
        case 3: targetX = 91; targetY = 67; break;
        case 4: targetX = 27; targetY = 69; break;
        default: targetX = 31; targetY = 47; break;
      }
      const uint8_t decorPhase = static_cast<uint8_t>((frame + slot * 17U) % 144U);
      const int decorPull = decorPhase < 72U ? decorPhase : 144U - decorPhase;
      px = (px * (144 - decorPull) + targetX * decorPull) / 144;
      py = (py * (144 - decorPull) + targetY * decorPull) / 144;
    }
    if (actionFrames_[slot] > 0) {
      py -= (actionPose_[slot] == 2 && (frame & 3U) < 2U) ? 4 : 0;
      --actionFrames_[slot];
      if (actionFrames_[slot] == 0) actionPose_[slot] = 0;
    }
    const int minX = slot == selected ? 25 : 18;
    const int maxX = slot == selected ? 103 : 110;
    px = px < minX ? minX : px > maxX ? maxX : px;
    py = py < 36 ? 36 : py > 72 ? 72 : py;
    AquariumArt::fish(display_, species_[slot], px, py, fishLifeStage(slot),
                      right, static_cast<uint8_t>(frame), quirk,
                      slot == selected, actionPose_[slot], growthBranch_[slot]);
    }
  }

  // Tamachibi watches from the front edge of the tank. The swimmer lanes stay
  // high enough that the companion never hides the selected fish.
  if (includePet) drawCurrentPetTiny(104, 84);
  NativeUI::footer(display_, "PREV", "GREET", "NEXT");
}

void AquariumSystem::interactWithFish(uint8_t slot) {
  if (!slotOccupied(slot)) return;
  const uint32_t now = static_cast<uint32_t>(ageTicks);
  const bool rewardInteraction = lastInteractionTick_ == 0 ||
      static_cast<uint32_t>(now - lastInteractionTick_) >= TICKS_PER_HOUR_AQ;
  const uint8_t personality = personalitySeed_[slot] & 3U;
  mood_[slot] = boundedAdd(mood_[slot], personality == 1 ? 1 : 2);
  const uint8_t pose = personality == 1 ? 1 : personality == 2 ? 3 : 2;
  setActionPose(slot, pose, 34);
  if (personality == 3) {
    for (uint8_t other = 0; other < AQUARIUM_FISH_SLOTS; ++other) {
      if (other == slot || !slotOccupied(other)) continue;
      mood_[other] = boundedAdd(mood_[other], 1);
      setActionPose(other, 1, 20);
    }
  }
  if (rewardInteraction) {
    lastInteractionTick_ = now;
    friendship_[slot] = boundedAdd(friendship_[slot], 2, 100);
    setMemory(slot, lastMemory_[slot] == AQUA_MEMORY_NONE
                        ? AQUA_MEMORY_FIRST_GREET : AQUA_MEMORY_GENTLE_TOUCH);
    addStat(happy, 2);
    addStat(energy, 1);
    addSkill(skillKind, 1);
    changeBond(1);
    markRoutineAction(ROUTINE_PLAY_AQ);
    recordGoalEvent(2, 1);
    goodSound();
  }
  markSaveDirty();
}

void AquariumSystem::watchTank() {
  if (!hasFish()) {
    showFlashPopup(F("The tank is empty"), F("Buy your first fish"));
    return;
  }
  selectedSlot_ = slotOccupied(selectedSlot_) ? selectedSlot_ : nextOccupiedSlot(0, 1);
  if (aquariumVisits_ && aquariumVisits_ % 3U == 0) {
    setMemory(selectedSlot_, AQUA_MEMORY_WATCH_TANK);
    markSaveDirty();
  }
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  uint16_t frame = 0;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_);
    const bool s = digitalRead(btnS_);
    const bool r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { selectedSlot_ = nextOccupiedSlot(selectedSlot_, -1); clickSound(); delay(110); }
    if (oldR == HIGH && r == LOW) { selectedSlot_ = nextOccupiedSlot(selectedSlot_, 1); clickSound(); delay(110); }
    if (oldS == HIGH && s == LOW) { interactWithFish(selectedSlot_); delay(120); }
    oldL = l; oldS = s; oldR = r;
    drawTankScene(selectedSlot_, frame++, true);
    display_.display();
    delay(110);
  }
}

void AquariumSystem::fishStatusPage() {
  if (!hasFish()) {
    showFlashPopup(F("No fish yet"), F("Visit the fish shop"));
    return;
  }
  selectedSlot_ = slotOccupied(selectedSlot_) ? selectedSlot_ : nextOccupiedSlot(0, 1);
  bool oldL = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_);
    const bool r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { selectedSlot_ = nextOccupiedSlot(selectedSlot_, -1); clickSound(); delay(110); }
    if (oldR == HIGH && r == LOW) { selectedSlot_ = nextOccupiedSlot(selectedSlot_, 1); clickSound(); delay(110); }
    oldL = l; oldR = r;

    NativeUI::Scope native(display_);
    display_.clearDisplay();
    char count[10]; snprintf(count, sizeof(count), "%u/%u", selectedSlot_ + 1U, fishCount());
    NativeUI::header(display_, names_[selectedSlot_], count, false);
    display_.setTextSize(1); display_.setTextColor(SSD1306_WHITE); display_.setTextWrap(false);
    display_.setCursor(3, 22); display_.print(speciesName(species_[selectedSlot_]));
    display_.print(F("  "));
    switch (fishLifeStage(selectedSlot_)) {
      case 0: display_.print(F("FRY")); break;
      case 1: display_.print(F("YOUNG")); break;
      case 2: display_.print(F("JUVENILE")); break;
      default: display_.print(F("ADULT")); break;
    }
    drawFishSprite(species_[selectedSlot_], 21, 43, true,
                   fishLifeStage(selectedSlot_), true);
    display_.setCursor(43, 31); display_.print(personalityName(personalitySeed_[selectedSlot_]));
    display_.setCursor(43, 42); display_.print(quirkName(personalitySeed_[selectedSlot_] >> 2));
    display_.setCursor(43, 53); display_.print(F("Likes ")); display_.print(foodName(fishFavoriteFood(selectedSlot_)));
    display_.setCursor(3, 64); display_.print(F("FOOD"));
    NativeUI::meter(display_, 37, 64, 76, hunger_[selectedSlot_]);
    display_.setCursor(3, 76); display_.print(F("MOOD"));
    NativeUI::meter(display_, 37, 76, 76, mood_[selectedSlot_]);
    display_.setCursor(3, 88); display_.print(F("HEALTH"));
    NativeUI::meter(display_, 37, 88, 76, health_[selectedSlot_]);
    char bond[18]; snprintf(bond, sizeof(bond), "BOND %u/100", friendship_[selectedSlot_]);
    NativeUI::text(display_, bond, 3, 98, 72);
    char story[48];
    snprintf(story, sizeof(story), "REM: %s", memoryName(lastMemory_[selectedSlot_]));
    NativeUI::text(display_, story, 3, 107, 121);
    NativeUI::footer(display_, "PREV", "PROFILE", "NEXT");
    display_.display();
    delay(25);
  }
}

void AquariumSystem::feedFish() {
  if (!hasFish()) { showFlashPopup(F("No fish to feed"), F("Buy a fish first")); return; }
  if (foodStock_ == 0) { showFlashPopup(F("Fish food is empty"), F("Buy supplies")); return; }
  selectedSlot_ = slotOccupied(selectedSlot_) ? selectedSlot_ : nextOccupiedSlot(0, 1);
  uint8_t food = fishFavoriteFood(selectedSlot_);
  bool oldL = HIGH, oldR = HIGH, oldS = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_), r = digitalRead(btnR_), s = digitalRead(btnS_);
    if (oldL == HIGH && l == LOW) { food = food == 0 ? 2 : food - 1; clickSound(); }
    if (oldR == HIGH && r == LOW) { food = static_cast<uint8_t>((food + 1) % 3); clickSound(); }
    if (oldS == HIGH && s == LOW) {
      if (hunger_[selectedSlot_] >= 9) {
        showTextPopup(names_[selectedSlot_], "is already full");
      } else {
        const uint8_t fedBit = static_cast<uint8_t>(1U << selectedSlot_);
        const bool firstMeal = (firstFedMask_ & fedBit) == 0;
        firstFedMask_ |= fedBit;
        --foodStock_;
        hunger_[selectedSlot_] = boundedAdd(hunger_[selectedSlot_], 4);
        const uint8_t personality = personalitySeed_[selectedSlot_] & 3U;
        if (food == fishFavoriteFood(selectedSlot_)) {
          mood_[selectedSlot_] = boundedAdd(mood_[selectedSlot_], 2);
          friendship_[selectedSlot_] = boundedAdd(friendship_[selectedSlot_], 3, 100);
          if (firstMeal) {
            setMemory(selectedSlot_, AQUA_MEMORY_FIRST_MEAL);
            if (favoriteMemory_[selectedSlot_] == AQUA_MEMORY_NONE)
              favoriteMemory_[selectedSlot_] = AQUA_MEMORY_FAVORITE_MEAL;
          } else {
            setMemory(selectedSlot_, AQUA_MEMORY_FAVORITE_MEAL);
          }
          setActionPose(selectedSlot_, 2, 36);
          static const char *const reactions[4] = {
            "dashes in for it!", "nibbles a tiny bite.",
            "inspects, then eats!", "calls friends to watch!"
          };
          showTextPopup(names_[selectedSlot_], reactions[personality]);
          if (personality == 3) {
            for (uint8_t other = 0; other < AQUARIUM_FISH_SLOTS; ++other)
              if (other != selectedSlot_ && slotOccupied(other))
                mood_[other] = boundedAdd(mood_[other], 1);
          }
        } else {
          mood_[selectedSlot_] = boundedAdd(mood_[selectedSlot_], 1);
          friendship_[selectedSlot_] = boundedAdd(friendship_[selectedSlot_], 1, 100);
          if (firstMeal) setMemory(selectedSlot_, AQUA_MEMORY_FIRST_MEAL);
          setActionPose(selectedSlot_, 1, 26);
          static const char *const reactions[4] = {
            "grabs a quick snack.", "takes a shy nibble.",
            "sniffs, then nibbles.", "shares the excitement!"
          };
          showTextPopup(names_[selectedSlot_], reactions[personality]);
        }
        addStat(happy, 1); addSkill(skillKind, 1); changeBond(1);
        markRoutineAction(ROUTINE_CARE_AQ); careSound(); markSaveDirty();
      }
      return;
    }
    oldL = l; oldR = r; oldS = s;
    NativeUI::Scope native(display_); display_.clearDisplay();
    NativeUI::header(display_, names_[selectedSlot_], "meal time", false);
    drawFishSprite(species_[selectedSlot_], 37, 50, true, fishLifeStage(selectedSlot_), true);
    display_.setTextSize(1); display_.setTextColor(SSD1306_WHITE);
    display_.setCursor(61, 40); display_.print(F("HUNGER ")); display_.print(hunger_[selectedSlot_]); display_.print(F("/10"));
    display_.setCursor(61, 53); display_.print(F("BOND ")); display_.print(friendship_[selectedSlot_]);
    display_.drawRoundRect(23, 76, 82, 20, 4, SSD1306_WHITE);
    display_.setCursor(33, 82); display_.print(foodName(food));
    if (food == fishFavoriteFood(selectedSlot_)) {
      display_.setCursor(45, 101); display_.print(F("FAVORITE!"));
    }
    NativeUI::footer(display_, "FOOD -", "GIVE", "FOOD +");
    display_.display(); delay(30);
  }
}

void AquariumSystem::cleanTankAction() {
  if (!hasFish()) { showFlashPopup(F("The tank is empty"), F("Nothing to clean")); return; }
  selectedSlot_ = slotOccupied(selectedSlot_) ? selectedSlot_ : nextOccupiedSlot(0, 1);
  if (tankClean_ >= 9 && waterQuality_ >= 9) { showFlashPopup(F("Tank already clean"), F("Water looks clear")); return; }

  for (uint8_t frame = 0; frame < 9; ++frame) {
    drawTankScene(selectedSlot_, frame, true);
    display_.drawLine(17 + frame * 10, 33, 12 + frame * 10, 86, SSD1306_WHITE);
    display_.display();
    delay(90);
  }
  tankClean_ = 10;
  waterQuality_ = 10;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) if (slotOccupied(i)) mood_[i] = boundedAdd(mood_[i], 1);
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
    if (!slotOccupied(i)) continue;
    setMemory(i, AQUA_MEMORY_CLEAN_WATER);
    if (i == selectedSlot_) {
      mood_[i] = boundedAdd(mood_[i], 1);
      friendship_[i] = boundedAdd(friendship_[i], 1, 100);
      setActionPose(i, 3, 34);
    } else {
      setActionPose(i, 2, 26);
    }
  }
  subStat(energy, 1);
  addStat(happy, 1);
  addSkill(skillKind, 1);
  cleanlinessScore = boundedAdd(cleanlinessScore, 2, 100);
  changeBond(1);
  changeTrust(1);
  markRoutineAction(ROUTINE_CARE_AQ);
  recordGoalEvent(3, 1);
  careSound();
  showFlashPopup(F("Tank cleaned"), F("The fish look calm"));
  markSaveDirty();
}

void AquariumSystem::renameFish(uint8_t slot) {
  if (!slotOccupied(slot)) return;
  static const char characters[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  const uint8_t characterCount = sizeof(characters) - 1;
  uint8_t position = 0;
  uint8_t selected = 1;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();

  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_);
    const bool s = digitalRead(btnS_);
    const bool r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) {
      selected = selected == 0 ? characterCount - 1 : selected - 1;
      names_[slot][position] = characters[selected]; clickSound(); delay(100);
    }
    if (oldR == HIGH && r == LOW) {
      selected = static_cast<uint8_t>((selected + 1) % characterCount);
      names_[slot][position] = characters[selected]; clickSound(); delay(100);
    }
    if (oldS == HIGH && s == LOW) {
      ++position;
      if (position >= 6) break;
      selected = 1;
      for (uint8_t i = 0; i < characterCount; ++i) if (characters[i] == names_[slot][position]) selected = i;
      clickSound(); delay(100);
    }
    oldL = l; oldS = s; oldR = r;

    display_.clearDisplay();
    display_.setTextWrap(false);
    display_.setTextSize(1);
    display_.setTextColor(SSD1306_WHITE);
    display_.setCursor(35, 4); display_.print(F("Fish name"));
    for (uint8_t i = 0; i < 6; ++i) {
      const int x = 25 + i * 13;
      display_.drawRect(x, 23, 11, 17, SSD1306_WHITE);
      display_.setCursor(x + 3, 28); display_.print(names_[slot][i] ? names_[slot][i] : ' ');
      if (i == position) display_.drawLine(x, 44, x + 10, 44, SSD1306_WHITE);
    }
    drawFishSprite(species_[slot], 64, 55, true, fishLifeStage(slot), false);
    display_.display();
    delay(20);
  }

  names_[slot][6] = '\0';
  for (int8_t i = 5; i >= 0 && names_[slot][i] == ' '; --i) names_[slot][i] = '\0';
  if (names_[slot][0] == '\0') strncpy(names_[slot], defaultFishName(species_[slot]), 7);
  goodSound();
  showTextPopup(names_[slot], "name saved");
  markSaveDirty();
}

void AquariumSystem::buyFish() {
  const uint8_t slot = firstEmptySlot();
  if (slot == EMPTY_FISH) { showFlashPopup(F("Tank is full"), F("Upgrade for space")); return; }

  uint8_t pick = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_);
    const bool s = digitalRead(btnS_);
    const bool r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? AQUARIUM_SPECIES_COUNT - 1 : pick - 1; clickSound(); delay(110); }
    if (oldR == HIGH && r == LOW) { pick = static_cast<uint8_t>((pick + 1) % AQUARIUM_SPECIES_COUNT); clickSound(); delay(110); }
    if (oldS == HIGH && s == LOW) {
      if (!speciesUnlocked(pick)) { badSound(); showFlashPopup(F("Fish is locked"), F("Upgrade the tank")); }
      else if (!spendCoins(speciesCost(pick))) { badSound(); showFlashPopup(F("Not enough coins"), F("Work or explore")); }
      else {
        firstFedMask_ &= static_cast<uint8_t>(~(1U << slot));
        species_[slot] = pick;
        strncpy(names_[slot], defaultFishName(pick), 7);
        hunger_[slot] = 8; mood_[slot] = 8; health_[slot] = 10; friendship_[slot] = 1; ageTicks_[slot] = 0;
        lastMemory_[slot] = AQUA_MEMORY_NONE; favoriteMemory_[slot] = AQUA_MEMORY_NONE;
        growthBranch_[slot] = 1;
        initializeFishProfile(slot, static_cast<uint8_t>(random(1, 16)));
        setActionPose(slot, 1, 30);
        selectedSlot_ = slot;
        discoveredMask_ |= static_cast<uint16_t>(1U << pick);
        waterQuality_ = boundedSub(waterQuality_, 1);
        winSound();
        showFlashPopup(F("New fish!"), speciesName(pick));
        renameFish(slot);
        markSaveDirty();
        return;
      }
      waitButtonRelease();
    }
    oldL = l; oldS = s; oldR = r;

    display_.clearDisplay();
    display_.setTextWrap(false);
    display_.setTextSize(1);
    display_.setTextColor(SSD1306_WHITE);
    display_.setCursor(34, 0); display_.print(F("Fish Shop"));
    drawFishSprite(pick, 64, 28, true, pick < 6 ? 1 : 2, false);
    display_.setCursor(10, 45); display_.print(speciesName(pick));
    display_.setCursor(90, 45); display_.print(F("$")); display_.print(speciesCost(pick));
    display_.setCursor(10, 56); display_.print(speciesUnlocked(pick) ? F("Available") : F("Tank upgrade needed"));
    display_.display();
    delay(25);
  }
}

void AquariumSystem::renameSelectedFish() {
  if (!hasFish()) { showFlashPopup(F("No fish to name"), F("Buy a fish first")); return; }
  selectedSlot_ = slotOccupied(selectedSlot_) ? selectedSlot_ : nextOccupiedSlot(0, 1);
  renameFish(selectedSlot_);
}

void AquariumSystem::decorateTank() {
  if (tankDecor_ >= 5) { showFlashPopup(F("Decor complete"), F("Moon grotto active")); return; }
  const uint8_t next = tankDecor_ + 1;
  const uint8_t cost = static_cast<uint8_t>(3 + next * 2);
  if (!spendCoins(cost)) { badSound(); showFlashPopup(F("Need more coins"), F("Decor stays the same")); return; }
  tankDecor_ = next;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) if (slotOccupied(i)) {
    mood_[i] = boundedAdd(mood_[i], 2);
    if (fishFavoriteDecor(i) == next) {
      mood_[i] = boundedAdd(mood_[i], 1);
      friendship_[i] = boundedAdd(friendship_[i], 2, 100);
      setMemory(i, AQUA_MEMORY_FAVORITE_DECOR);
      setActionPose(i, 3, 36);
    } else {
      setActionPose(i, 2, 30);
    }
  }
  addStat(happy, 1);
  addSkill(skillStyle, 1);
  winSound();
  showFlashPopup(F("Tank decorated"), decorName(tankDecor_));
  markSaveDirty();
}

void AquariumSystem::upgradeTank() {
  if (tankLevel_ >= 3) { showFlashPopup(F("Largest tank owned"), F("Four fish can live here")); return; }
  const uint8_t cost = tankLevel_ == 1 ? 12 : 24;
  if (!spendCoins(cost)) { badSound(); showFlashPopup(F("Need more coins"), F("Upgrade later")); return; }
  ++tankLevel_;
  tankClean_ = 10;
  waterQuality_ = 10;
  winSound();
  showFlashPopup(F("Tank upgraded"), tankLevel_ == 2 ? F("Three fish capacity") : F("Four fish capacity"));
  markSaveDirty();
}

void AquariumSystem::suppliesShop() {
  uint8_t pick = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_);
    const bool s = digitalRead(btnS_);
    const bool r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { pick ^= 1; clickSound(); delay(110); }
    if (oldR == HIGH && r == LOW) { pick ^= 1; clickSound(); delay(110); }
    if (oldS == HIGH && s == LOW) {
      const uint8_t cost = pick == 0 ? 2 : 4;
      if (!spendCoins(cost)) { badSound(); showFlashPopup(F("Need more coins"), F("Try again later")); }
      else if (pick == 0) { foodStock_ = boundedAdd(foodStock_, 5, 99); goodSound(); showFlashPopup(F("Fish food +5"), F("Supplies stocked")); }
      else { remedyStock_ = boundedAdd(remedyStock_, 1, 20); goodSound(); showFlashPopup(F("Remedy +1"), F("Medicine stocked")); }
      markSaveDirty();
      return;
    }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay();
    display_.setTextWrap(false);
    display_.setTextSize(1);
    display_.setTextColor(SSD1306_WHITE);
    display_.setCursor(29, 2); display_.print(F("Tank Supplies"));
    display_.drawRoundRect(12, 18, 104, 30, 5, SSD1306_WHITE);
    display_.setCursor(25, 28);
    if (pick == 0) { display_.print(F("Food x5  $2")); display_.setCursor(25, 39); display_.print(F("Stock ")); display_.print(foodStock_); }
    else { display_.print(F("Remedy x1 $4")); display_.setCursor(25, 39); display_.print(F("Stock ")); display_.print(remedyStock_); }
    display_.display();
    delay(25);
  }
}

void AquariumSystem::treatFish() {
  if (!hasFish()) { showFlashPopup(F("No fish to treat"), F("The tank is empty")); return; }
  selectedSlot_ = slotOccupied(selectedSlot_) ? selectedSlot_ : nextOccupiedSlot(0, 1);
  if (health_[selectedSlot_] >= 9) { showTextPopup(names_[selectedSlot_], "already feels well"); return; }
  if (remedyStock_ == 0) { showFlashPopup(F("No fish remedy"), F("Buy supplies")); return; }
  --remedyStock_;
  health_[selectedSlot_] = boundedAdd(health_[selectedSlot_], 5);
  mood_[selectedSlot_] = boundedAdd(mood_[selectedSlot_], 2);
  friendship_[selectedSlot_] = boundedAdd(friendship_[selectedSlot_], 3, 100);
  setMemory(selectedSlot_, AQUA_MEMORY_RECOVERY);
  setActionPose(selectedSlot_, 3, 34);
  addSkill(skillKind, 1);
  addStat(happy, 1);
  changeBond(1);
  markRoutineAction(ROUTINE_CARE_AQ);
  careSound();
  static const char *const recoveryReplies[4] = {
    "paddles back bravely!", "peeks out, then relaxes.",
    "inspects the remedy.", "looks for tankmates."
  };
  showTextPopup(names_[selectedSlot_], recoveryReplies[personalitySeed_[selectedSlot_] & 3U]);
  markSaveDirty();
}

void AquariumSystem::playBubbles() {
  if (!hasFish()) { showFlashPopup(F("No fish to play"), F("Meet a fish first")); return; }
  selectedSlot_ = slotOccupied(selectedSlot_) ? selectedSlot_ : nextOccupiedSlot(0, 1);
  uint8_t attempts = 0, catches = 0;
  uint16_t frame = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (attempts < 4 && frame < 360) {
    if (rightHoldExitCheck()) break;
    const bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { selectedSlot_ = nextOccupiedSlot(selectedSlot_, -1); clickSound(); }
    if (oldR == HIGH && r == LOW) { selectedSlot_ = nextOccupiedSlot(selectedSlot_, 1); clickSound(); }
    const uint8_t seed = personalitySeed_[selectedSlot_];
    const uint8_t personality = seed & 3U;
    uint8_t rank = 0;
    for (uint8_t earlier = 0; earlier < selectedSlot_; ++earlier)
      if (slotOccupied(earlier)) ++rank;
    const uint8_t total = fishCount();
    const AquariumArt::SwimPoint path = AquariumArt::swimPoint(seed, rank, total, frame);
    int fishX = path.x;
    int fishY = path.y;
    const uint8_t quirk = (seed >> 2) & 3U;
    const int vertical = path.y - (rank < 2 ? 40 + rank * 3 : 64 + (rank - 2) * 3);
    if (quirk == 1) fishY += 4 + vertical / 2;
    else if (quirk == 2) fishY -= 3;
    else if (quirk == 3) {
      const uint8_t orbit = static_cast<uint8_t>((frame / 9U + seed) % 4U);
      fishX += orbit == 0 ? 0 : orbit == 1 ? 4 : orbit == 2 ? 0 : -4;
      fishY += orbit == 1 ? 5 : orbit == 3 ? -5 : 0;
    }
    if (personality == 3 && total > 1) {
      const int groupX = (rank & 1U) ? 35 : 91;
      const uint8_t schoolPhase = static_cast<uint8_t>((frame + selectedSlot_ * 23U) % 96U);
      const int schoolPull = schoolPhase < 48U ? schoolPhase : 96U - schoolPhase;
      fishX = (fishX * (96 - schoolPull) + groupX * schoolPull) / 96;
      fishY -= schoolPull / 24;
    }
    const uint8_t favoriteDecor = AquariumArt::preferredDecor(species_[selectedSlot_], seed);
    if (tankDecor_ >= favoriteDecor) {
      int targetX = 63, targetY = 68;
      switch (favoriteDecor) {
        case 1: targetX = (rank & 1U) ? 104 : 12; targetY = 68; break;
        case 2: targetX = 63; targetY = 72; break;
        case 3: targetX = 91; targetY = 67; break;
        case 4: targetX = 27; targetY = 69; break;
        default: targetX = 31; targetY = 47; break;
      }
      const uint8_t decorPhase = static_cast<uint8_t>((frame + selectedSlot_ * 17U) % 144U);
      const int decorPull = decorPhase < 72U ? decorPhase : 144U - decorPhase;
      fishX = (fishX * (144 - decorPull) + targetX * decorPull) / 144;
      fishY = (fishY * (144 - decorPull) + targetY * decorPull) / 144;
    }
    fishX = fishX < 25 ? 25 : fishX > 103 ? 103 : fishX;
    fishY = fishY < 36 ? 36 : fishY > 72 ? 72 : fishY;
    const int bubbleX = 16 + static_cast<int>((frame * 2U + 18U) % 100U);
    const int bubbleY = 34 + static_cast<int>((frame * 2U + seed * 11U) % 54U);
    if (oldS == HIGH && s == LOW) {
      ++attempts;
      if (abs(fishX - bubbleX) <= 12 && abs(fishY - bubbleY) <= 9) {
        ++catches; mood_[selectedSlot_] = boundedAdd(mood_[selectedSlot_], 1);
        setActionPose(selectedSlot_, 2, 28); goodSound();
      } else badSound();
    }
    oldL = l; oldS = s; oldR = r;
    drawTankScene(selectedSlot_, frame, true);
    display_.drawCircle(bubbleX, bubbleY, 3, SSD1306_WHITE);
    display_.drawPixel(bubbleX, bubbleY - 1, SSD1306_WHITE);
    char score[18]; snprintf(score, sizeof(score), "CATCH %u/4", catches);
    NativeUI::text(display_, score, 37, 106, 55);
    display_.display();
    ++frame;
    delay(70);
  }

  if (attempts > 0) {
    const uint8_t reward = catches ? catches : 1;
    mood_[selectedSlot_] = boundedAdd(mood_[selectedSlot_], reward);
    friendship_[selectedSlot_] = boundedAdd(friendship_[selectedSlot_], catches ? reward * 2U : 1U, 100);
    setMemory(selectedSlot_, AQUA_MEMORY_BUBBLE_PLAY);
    setActionPose(selectedSlot_, catches ? 2 : 1, 36);
    addStat(happy, catches ? 2 : 1); addSkill(skillKind, 1); changeBond(1);
    markRoutineAction(ROUTINE_PLAY_AQ); recordGoalEvent(2, 1); markSaveDirty();
    if (catches >= 3) showTextPopup(names_[selectedSlot_], "bubble champion!");
    else if (catches) showTextPopup(names_[selectedSlot_], "loved chasing bubbles");
    else showTextPopup(names_[selectedSlot_], "had fun swimming with you");
  }
}

void AquariumSystem::fishMemoryPage() {
  if (!hasFish()) { showFlashPopup(F("No stories yet"), F("Meet your first fish")); return; }
  selectedSlot_ = slotOccupied(selectedSlot_) ? selectedSlot_ : nextOccupiedSlot(0, 1);
  bool oldL = HIGH, oldR = HIGH;
  uint16_t frame = 0;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { selectedSlot_ = nextOccupiedSlot(selectedSlot_, -1); clickSound(); }
    if (oldR == HIGH && r == LOW) { selectedSlot_ = nextOccupiedSlot(selectedSlot_, 1); clickSound(); }
    oldL = l; oldR = r;
    NativeUI::Scope native(display_); display_.clearDisplay();
    char count[10]; snprintf(count, sizeof(count), "%u/%u", selectedSlot_ + 1U, fishCount());
    NativeUI::header(display_, names_[selectedSlot_], count, false);
    display_.setTextSize(1); display_.setTextColor(SSD1306_WHITE);
    char speciesStage[32];
    snprintf(speciesStage, sizeof(speciesStage), "%s  %s",
             reinterpret_cast<const char *>(speciesName(species_[selectedSlot_])),
             fishLifeStage(selectedSlot_) == 0 ? "FRY" :
             fishLifeStage(selectedSlot_) == 1 ? "YOUNG" :
             fishLifeStage(selectedSlot_) == 2 ? "JUVENILE" : "ADULT");
    NativeUI::text(display_, speciesStage, 6, 22, 84);
    AquariumArt::fish(display_, species_[selectedSlot_], 107, 44,
                      fishLifeStage(selectedSlot_), true,
                      static_cast<uint8_t>(frame), selectedFishQuirk(), true,
                      actionPose_[selectedSlot_], growthBranch_[selectedSlot_]);
    NativeUI::text(display_, personalityName(selectedFishPersonality()), 6, 34, 84);
    NativeUI::text(display_, quirkName(selectedFishQuirk()), 6, 45, 84);
    char preference[40];
    snprintf(preference, sizeof(preference), "Food: %s", foodName(fishFavoriteFood(selectedSlot_)));
    NativeUI::text(display_, preference, 6, 56, 84);
    snprintf(preference, sizeof(preference), "Room: %s", favoriteDecorName(fishFavoriteDecor(selectedSlot_)));
    NativeUI::text(display_, preference, 6, 67, 84);
    char last[64], keepsake[64];
    snprintf(last, sizeof(last), "Last: %s", memoryName(lastMemory_[selectedSlot_]));
    snprintf(keepsake, sizeof(keepsake), "Keepsake: %s", memoryName(favoriteMemory_[selectedSlot_]));
    NativeUI::text(display_, last, 6, 80, 116);
    NativeUI::text(display_, keepsake, 6, 91, 116);
    display_.setCursor(6, 102); display_.print(F("Trust ")); display_.print(friendship_[selectedSlot_]); display_.print(F("/100"));
    NativeUI::footer(display_, "PREV", "STORIES", "NEXT");
    display_.display(); delay(100); ++frame;
  }
}

void AquariumSystem::maybeCreateFry(bool catchUpMode) {
  if (tankLevel_ < 3 || fishCount() < 2 || firstEmptySlot() == EMPTY_FISH || careStreak_ < 5) return;
  if (random(0, 14) != 0) return;
  const uint8_t slot = firstEmptySlot();
  uint8_t first = EMPTY_FISH, second = EMPTY_FISH;
  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
    if (!slotOccupied(i)) continue;
    if (first == EMPTY_FISH) first = i;
    else { second = i; break; }
  }
  const uint8_t parent = random(0, 2) == 0 ? first : second;
  const uint8_t newSpecies = species_[parent];
  firstFedMask_ &= static_cast<uint8_t>(~(1U << slot));
  species_[slot] = newSpecies;
  strncpy(names_[slot], "FRY", 7);
  hunger_[slot] = 8; mood_[slot] = 9; health_[slot] = 10; friendship_[slot] = 2; ageTicks_[slot] = 0;
  lastMemory_[slot] = AQUA_MEMORY_NONE; favoriteMemory_[slot] = AQUA_MEMORY_NONE;
  growthBranch_[slot] = 1;
  initializeFishProfile(slot, static_cast<uint8_t>((personalitySeed_[first] + personalitySeed_[second] + random(1, 5)) & 15U));
  setMemory(slot, AQUA_MEMORY_FIRST_GREET);
  setMemory(first, AQUA_MEMORY_COMFORT);
  setMemory(second, AQUA_MEMORY_COMFORT);
  setActionPose(slot, 1, 36);
  discoveredMask_ |= static_cast<uint16_t>(1U << newSpecies);
  surpriseType_ = 2;
  if (!catchUpMode) selectedSlot_ = slot;
}

void AquariumSystem::applyDailyTankEffects(bool catchUpMode) {
  const uint32_t day = ageTicks / TICKS_PER_DAY_AQ;
  if (lastRewardDay_ == day) return;
  lastRewardDay_ = day;

  if (allFishComfortable()) {
    careStreak_ = min((uint8_t)99, (uint8_t)(careStreak_ + 1));
    addStat(happy, 1);
    addSkill(skillKind, 1);
    cleanlinessScore = boundedAdd(cleanlinessScore, 1, 100);
    if ((day & 1U) == 0) changeBond(1);

    bool hasKoi = false, hasCory = false, hasMoon = false, hasStar = false;
    for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
      if (!slotOccupied(i)) continue;
      hasCory |= species_[i] == 5;
      hasKoi |= species_[i] == 7;
      hasMoon |= species_[i] == 10;
      hasStar |= species_[i] == 11;
    }
    if (hasCory) tankClean_ = boundedAdd(tankClean_, 1);
    if (hasKoi && random(0, 3) == 0) { addCoins(1); surpriseType_ = 1; }
    if (hasMoon) addStat(energy, 1);
    if (hasStar) addSkill(skillSmart, 1);
    maybeCreateFry(catchUpMode);
    if (random(0, 6) == 0) {
      for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
        if (!slotOccupied(i)) continue;
        mood_[i] = boundedAdd(mood_[i], 1);
        setMemory(i, AQUA_MEMORY_COMFORT);
        if ((personalitySeed_[i] & 3U) == 2) setActionPose(i, 2, 28);
      }
      surpriseType_ = 4;
    }
  } else {
    careStreak_ = 0;
    if (needsCare()) subStat(happy, 1);
  }
}

void AquariumSystem::tick(bool catchUpMode) {
  if (!hasFish()) return;

  for (uint8_t i = 0; i < AQUARIUM_FISH_SLOTS; ++i) {
    if (!slotOccupied(i)) continue;
    const uint8_t oldStage = fishLifeStage(i);
    if (ageTicks_[i] < 65535) ++ageTicks_[i];
    if (ageTicks_[i] % difficultyTicks(6UL * TICKS_PER_HOUR_AQ) == 0) hunger_[i] = boundedSub(hunger_[i], 1);
    if (ageTicks_[i] % difficultyTicks(12UL * TICKS_PER_HOUR_AQ) == 0 && (tankClean_ < 6 || waterQuality_ < 6)) mood_[i] = boundedSub(mood_[i], 1);
    if (ageTicks_[i] % difficultyTicks(TICKS_PER_DAY_AQ) == 0) {
      if (hunger_[i] <= 2 || tankClean_ <= 3 || waterQuality_ <= 3) {
        health_[i] = max((uint8_t)1, boundedSub(health_[i], 1));
        mood_[i] = boundedSub(mood_[i], 1);
      } else {
        health_[i] = boundedAdd(health_[i], 1);
        const uint8_t comfort = static_cast<uint8_t>((tankDecor_ >= 2 ? 2 : 1) +
            (tankDecor_ >= fishFavoriteDecor(i) ? 1 : 0));
        mood_[i] = boundedAdd(mood_[i], comfort);
        friendship_[i] = boundedAdd(friendship_[i], 1, 100);
      }
    }
    const uint8_t newStage = fishLifeStage(i);
    if (newStage != oldStage) {
      const uint8_t memory = newStage == 1 ? AQUA_MEMORY_GROWTH_YOUNG :
                             newStage == 2 ? AQUA_MEMORY_GROWTH_JUVENILE :
                                             AQUA_MEMORY_GROWTH_ADULT;
      setMemory(i, memory);
      setActionPose(i, 3, 36);
      if (newStage == 3) {
        uint8_t careScore = 0;
        if (health_[i] >= 7) ++careScore;
        if (mood_[i] >= 7) ++careScore;
        if (friendship_[i] >= 25) ++careScore;
        if (tankClean_ >= 7 && waterQuality_ >= 7) ++careScore;
        growthBranch_[i] = min((uint8_t)3, careScore);
      }
      if (!catchUpMode && i == selectedSlot_) surpriseType_ = 3;
    }
  }

  const uint16_t totalFish = fishCount();
  if (ageTicks % difficultyTicks(12UL * TICKS_PER_HOUR_AQ) == 0) tankClean_ = boundedSub(tankClean_, totalFish >= 3 ? 2 : 1);
  if (ageTicks % difficultyTicks(18UL * TICKS_PER_HOUR_AQ) == 0) waterQuality_ = boundedSub(waterQuality_, 1);
  if (tankDecor_ >= 1 && ageTicks % difficultyTicks(TICKS_PER_DAY_AQ) == 0) waterQuality_ = boundedAdd(waterQuality_, 1);
  if (ageTicks % TICKS_PER_DAY_AQ == 0) applyDailyTankEffects(catchUpMode);

  clampState();
  markSaveDirty();
}

void AquariumSystem::composeSelectedFishComment(char *out, size_t outSize) {
  if (!out || outSize == 0) return;
  if (!hasFish()) { snprintf(out, outSize, "%s wants to meet a fish with you.", petName); return; }
  selectedSlot_ = slotOccupied(selectedSlot_) ? selectedSlot_ : nextOccupiedSlot(0, 1);
  const uint8_t slot = selectedSlot_;
  const uint8_t visit = ++aquariumVisits_;
  const uint8_t temperament = personalitySeed_[slot] & 3U;
  const uint8_t quirk = (personalitySeed_[slot] >> 2) & 3U;
  static const char *const openings[4][8] = {
    {"I have a new little story about %s.", "%s always finds a way to surprise me.", "I know what %s is going to do next. Probably.", "I have been watching %s explore.", "Something made me think of %s today.", "%s has been busy making its own plans.", "I noticed a new little habit from %s.", "There is always something happening with %s."},
    {"I have been thinking about quiet little %s.", "%s is brave in a gentle way.", "I like that %s takes its time with me.", "%s and I understand each other slowly.", "I have been giving %s room to choose.", "%s has a soft way of saying hello.", "I think %s notices when we are patient.", "A quiet visit with %s feels just right."},
    {"You would not believe what %s noticed today.", "%s is curious about everything we bring home.", "I think %s has a new question for us.", "%s always finds the interesting corner.", "I wonder what %s is investigating now.", "%s has been exploring a new route.", "There is something new for %s to inspect.", "I think %s has spotted something."},
    {"%s always wants to see what the others are doing.", "%s swims over whenever a friend comes by.", "I think %s likes being part of the group.", "%s brings the whole tank together.", "The other fish make %s feel right at home.", "%s is happiest when everyone is nearby.", "I have seen %s invite the others along.", "%s makes a little gathering out of every visit."}
  };
  static const char *const closings[4][8] = {
    {"We should see what it investigates next.", "I bet it will be ready for another adventure soon.", "It keeps the whole tank lively.", "It is fun learning its little routines.", "I wonder what it will try tomorrow.", "It has a talent for making me smile.", "There is never just one way to know it.", "It is always itself, in the best way."},
    {"I like waiting until it feels safe.", "It always chooses the calmest corner first.", "I think it trusts us a little more now.", "It notices when we give it room.", "It comes closer in its own time.", "We should keep our voices gentle.", "Its courage grows in small steps.", "I am glad we let it choose."},
    {"It never misses a new detail.", "I wonder what it will discover next.", "It makes every visit feel a little different.", "I like seeing where its curiosity leads.", "There is always another thing to inspect.", "It would make a fine little explorer.", "Its next idea could be anything.", "The tank is full of tiny mysteries."},
    {"It always goes looking for its fish friends.", "A shared swim seems to make it happy.", "It brings the group together in its own way.", "The other fish brighten when it comes by.", "It likes having familiar company nearby.", "Our little school feels closer with it here.", "It is always ready to welcome a tankmate.", "I hope the whole group keeps growing close."}
  };
  const uint8_t rememberedEvent = favoriteMemory_[slot] != AQUA_MEMORY_NONE && (visit % 3U == 0)
                                      ? favoriteMemory_[slot] : lastMemory_[slot];
  const char *memory = memoryPhrase(rememberedEvent,
                                   static_cast<uint8_t>((visit >> 3U) & 3U),
                                   names_[slot], foodName(fishFavoriteFood(slot)),
                                   favoriteDecorName(fishFavoriteDecor(slot)));
  char present[88];
  if (health_[slot] <= 3) snprintf(present, sizeof(present), "%s could use some medicine today.", names_[slot]);
  else if (hunger_[slot] <= 2) snprintf(present, sizeof(present), "%s is looking for a little food now.", names_[slot]);
  else if (tankClean_ <= 3 || waterQuality_ <= 3) snprintf(present, sizeof(present), "The cloudy water is making %s quieter today.", names_[slot]);
  else if (mood_[slot] <= 3) snprintf(present, sizeof(present), "%s seems quiet, so I would give it a soft hello.", names_[slot]);
  else if (tankDecor_ >= fishFavoriteDecor(slot) && (visit & 1U) == 0)
    snprintf(present, sizeof(present), "%s keeps returning to its %s nook.", names_[slot], favoriteDecorName(fishFavoriteDecor(slot)));
  else if (friendship_[slot] >= 60) snprintf(present, sizeof(present), "%s recognizes us when we come by.", names_[slot]);
  else snprintf(present, sizeof(present), "%s is doing well and exploring at its own pace.", names_[slot]);
  snprintf(out, outSize, openings[temperament][visit & 7U], names_[slot]);
  size_t used = strlen(out);
  if (used + 2 < outSize) snprintf(out + used, outSize - used, " %s", memory);
  used = strlen(out);
  if (used + 2 < outSize) snprintf(out + used, outSize - used, " %s", present);
  used = strlen(out);
  if (used + 2 < outSize)
    snprintf(out + used, outSize - used, " %s", closings[quirk][(visit >> 5U) & 7U]);
  markSaveDirty();
}

void AquariumSystem::showPetComment() {
  if (!hasFish()) { showTextPopup(petName, "wants a fish"); return; }
  selectedSlot_ = slotOccupied(selectedSlot_) ? selectedSlot_ : nextOccupiedSlot(0, 1);
  if (surpriseType_ == 2) {
    surpriseType_ = 0;
    showTextPopup(petName, "found a tiny fry!");
    markSaveDirty();
    return;
  }
  if (surpriseType_ == 1) {
    surpriseType_ = 0;
    showTextPopup(petName, "found a lucky coin");
    markSaveDirty();
    return;
  }
  if (surpriseType_ == 3) {
    const uint8_t growthMemory = fishLifeStage(selectedSlot_) == 1 ? AQUA_MEMORY_GROWTH_YOUNG :
                                fishLifeStage(selectedSlot_) == 2 ? AQUA_MEMORY_GROWTH_JUVENILE :
                                                                    AQUA_MEMORY_GROWTH_ADULT;
    setMemory(selectedSlot_, growthMemory);
  }
  if (surpriseType_ == 4) setMemory(selectedSlot_, AQUA_MEMORY_COMFORT);
  surpriseType_ = 0;
  char line[250];
  composeSelectedFishComment(line, sizeof(line));
  showTextPopup(petName, line, 1600);
  markRoutineAction(ROUTINE_TALK_AQ);
  changeBond(1);
}

void AquariumSystem::runMenu() {
  static const uint8_t OPTION_COUNT = 12;
  uint8_t option = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  ++aquariumVisits_;
  markSaveDirty();
  if (hasFish() && aquariumVisits_ % 5U == 0)
    setMemory(slotOccupied(selectedSlot_) ? selectedSlot_ : nextOccupiedSlot(0, 1),
              AQUA_MEMORY_RETURN_VISIT);

  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_);
    const bool s = digitalRead(btnS_);
    const bool r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { option = option == 0 ? OPTION_COUNT - 1 : option - 1; clickSound(); delay(110); }
    if (oldR == HIGH && r == LOW) { option = static_cast<uint8_t>((option + 1) % OPTION_COUNT); clickSound(); delay(110); }
    if (oldS == HIGH && s == LOW) {
      waitButtonRelease();
      switch (option) {
        case 0: watchTank(); break;
        case 1: fishStatusPage(); break;
        case 2: fishMemoryPage(); break;
        case 3: playBubbles(); break;
        case 4: feedFish(); break;
        case 5: cleanTankAction(); break;
        case 6: treatFish(); break;
        case 7: renameSelectedFish(); break;
        case 8: buyFish(); break;
        case 9: decorateTank(); break;
        case 10: upgradeTank(); break;
        default: suppliesShop(); break;
      }
      oldL = oldS = oldR = HIGH;
      waitButtonRelease();
    }
    oldL = l; oldS = s; oldR = r;

    NativeUI::Scope native(display_);
    const uint8_t selected = slotOccupied(selectedSlot_) ? selectedSlot_ : 0;
    drawTankScene(selected, static_cast<uint16_t>(millis() / 110UL), false);
    static const char *const options[OPTION_COUNT] = {
      "Watch the tank", "Fish profile", "Fish stories", "Play bubbles",
      "Feed a fish", "Clean the tank", "Treat a fish", "Name a fish",
      "Meet a fish", "Change decor", "Upgrade tank", "Buy supplies"
    };
    display_.fillRoundRect(4, 85, 120, 26, 4, SSD1306_BLACK);
    display_.drawRoundRect(4, 85, 120, 26, 4, SSD1306_WHITE);
    NativeUI::text(display_, options[option], 9, 90, 91);
    char stock[30];
    snprintf(stock, sizeof(stock), "W%u C%u FOOD%u", waterQuality_, tankClean_, foodStock_);
    NativeUI::text(display_, stock, 23, 101, 77);
    drawCurrentPetTiny(106, 87);
    NativeUI::footer(display_, "PREV", "OPEN", "NEXT");
    if (needsCare() && (millis() / 350UL) % 2 == 0) {
      display_.setCursor(117, 4); display_.print(F("!"));
    }
    display_.display();
    delay(25);
  }
}
