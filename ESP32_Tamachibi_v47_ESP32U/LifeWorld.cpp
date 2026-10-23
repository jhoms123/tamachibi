#include "LifeWorld.h"
#include <stddef.h>
#include <string.h>

extern byte health, full, energy, happy, clean, discipline;
extern byte conditionType, conditionSeverity;
extern unsigned int conditionTicks;
extern byte medCold, medTummy, medSkin, medEnergy, medMood, medAll;
extern byte skillSmart, skillStyle, skillKind, skillFit;
extern byte stage, temperament, roomStyle, habitat, favoriteGameId;
extern byte craftMaterials[];
extern unsigned int coins, generation;
extern unsigned long ageTicks;
extern bool sleeping, sick;
extern byte visualHour();
extern byte visualWeather();
extern unsigned int petAgeDays();
extern bool spendCoins(byte amount);
extern void addCoins(byte amount);
extern void addStat(byte &value, byte amount);
extern void subStat(byte &value, byte amount);
extern void addSkill(byte &value, byte amount);
extern void changeBond(int amount);
extern void changeTrust(int amount);
extern void markSaveDirty();
extern void clampAll();
extern void waitButtonRelease();
extern bool rightHoldExitCheck();
extern void clickSound();
extern void goodSound();
extern void badSound();
extern void winSound();
extern void drawCurrentPetTiny(int x, int y);
extern void drawSprite16(byte id, int x, int y);
extern void openSaveProfiles();
extern void openDeepLife();

namespace {
constexpr uint8_t STAGE_EGG_L = 0;
constexpr uint8_t STAGE_BABY_L = 1;
constexpr uint8_t STAGE_CHILD_L = 2;
constexpr uint8_t STAGE_TEEN_L = 3;
constexpr uint8_t STAGE_ADULT_L = 4;
constexpr uint8_t STAGE_ELDER_L = 5;

const char *const RESIDENT_NAMES[LIFE_RESIDENT_COUNT] = {
  "MOCHI", "LUMI", "PIKO", "NORI", "KUMO", "BIBI", "TOTO", "MIMI"
};

const char *const SKILL_NAMES[LIFE_SKILL_COUNT] = {
  "Cooking", "Gardening", "Music", "Medicine",
  "Exploring", "Social", "Creative", "Technology"
};

const char *const DECOR_NAMES[LIFE_DECOR_COUNT] = {
  "Cushion", "Plant", "Lamp", "Books", "Radio", "Trophy",
  "Clock", "Tea Set", "Robot", "Photo", "Globe", "Toy Chest"
};

const char *const EXPEDITION_NAMES[LIFE_EXPEDITION_COUNT] = {
  "Whisper Woods", "Crystal Cave", "Shell Beach",
  "Cloud Peak", "Old Ruins", "Moon Station"
};

const char *const PROJECT_NAMES[LIFE_PROJECT_COUNT] = {
  "School", "Park", "Clinic", "Market", "Museum", "Spaceport"
};

const char *const FESTIVAL_NAMES[LIFE_FESTIVAL_COUNT] = {
  "Cook-Off", "Garden Show", "Music Fest", "Sports Day",
  "Hat Gala", "Science Fair", "Memory Cup", "Space Race"
};

const char *const COMPANION_NAMES[8] = {
  "Mewbit", "Chirplet", "Blobbo", "RoboPup",
  "Finfin", "Sprout", "Boolet", "Moonbug"
};

const char *const HUB_NAMES[16] = {
  "Personality", "Skills & Careers", "Daily Routine", "Neighbor Stories",
  "Game Talents", "Decor Placement", "Expeditions", "Companion Pets",
  "Family Legacy", "Scrapbook", "Health Clinic", "Community",
  "Festivals", "Reputation", "Deep Life World", "Save Profiles"
};

const uint8_t ACTION_FOOD = 0;
const uint8_t ACTION_GAME = 2;
const uint8_t ACTION_MED = 3;
const uint8_t ACTION_CLEAN = 4;
const uint8_t ACTION_LOVE = 5;
const uint8_t ACTION_CALL = 6;
const uint8_t ACTION_WALK = 7;
const uint8_t ACTION_TRAIN = 8;
const uint8_t ACTION_STUDY = 9;
const uint8_t ACTION_GARDEN = 10;
const uint8_t ACTION_MUSIC = 11;
const uint8_t ACTION_TOY = 14;
const uint8_t ACTION_EXPLORE = 15;
const uint8_t ACTION_COOK = 16;
const uint8_t ACTION_JOB = 17;
const uint8_t ACTION_ROOM = 18;
const uint8_t ACTION_TALK = 19;
const uint8_t ACTION_CRAFT = 25;
const uint8_t ACTION_FAMILY = 28;

void rightText(GameDisplay &d, int right, int y, const char *text) {
  int w = static_cast<int>(strlen(text)) * 6;
  d.setCursor(max(0, right - w), y);
  d.print(text);
}
}

LifeWorldSystem::LifeWorldSystem(size_t storageBase, GameDisplay &display,
                                 uint8_t buttonLeft, uint8_t buttonSelect,
                                 uint8_t buttonRight)
    : storeA_(storageBase), storeB_(storageBase + sizeof(LifeWorldSave)),
      display_(display), btnL_(buttonLeft), btnS_(buttonSelect), btnR_(buttonRight) {
  reset();
}

void LifeWorldSystem::reset() {
  sequence_ = 0;
  for (uint8_t &v : traits_) v = 50;
  memset(extraSkills_, 0, sizeof(extraSkills_));
  careerId_ = 255; careerRank_ = 0; careerXp_ = 0;
  currentLocation_ = 0; routineScore_ = 50; routineFlags_ = 0; routineDay_ = 0; lastRoutineHour_ = 255;
  favoriteRoom_ = 0; favoriteWeather_ = 0; favoriteNeighbor_ = 0; fearType_ = 0; importantMemoryMask_ = 0;
  memset(roomVisits_, 0, sizeof(roomVisits_));
  memset(residentQuest_, 0, sizeof(residentQuest_));
  memset(residentQuestProgress_, 0, sizeof(residentQuestProgress_));
  decorOwnedMask_ = 1; decorPlaced_[0] = 0; decorPlaced_[1] = 255; decorPlaced_[2] = 255;
  expeditionVisitedMask_ = 0; memset(expeditionMastery_, 0, sizeof(expeditionMastery_));
  companionType_ = 255; companionBond_ = 0; companionEnergy_ = 8; companionMood_ = 8; companionLevel_ = 0; companionXp_ = 0;
  familyCount_ = familyHead_ = 0;
  memset(familySpecies_, 0, sizeof(familySpecies_)); memset(familyCareer_, 255, sizeof(familyCareer_));
  memset(familyPersonality_, 0, sizeof(familyPersonality_)); memset(familyAchievement_, 0, sizeof(familyAchievement_));
  memset(familyHeirloom_, 0, sizeof(familyHeirloom_)); memset(familyGeneration_, 0, sizeof(familyGeneration_));
  scrapCount_ = scrapHead_ = 0; memset(scrapType_, 0, sizeof(scrapType_)); memset(scrapDay_, 0, sizeof(scrapDay_)); memset(scrapValue_, 0, sizeof(scrapValue_));
  memset(projectLevel_, 0, sizeof(projectLevel_)); memset(projectProgress_, 0, sizeof(projectProgress_));
  memset(festivalWins_, 0, sizeof(festivalWins_)); memset(festivalBest_, 0, sizeof(festivalBest_)); festivalTrophyMask_ = 0;
  reputation_[0] = 30; reputation_[1] = 30; reputation_[2] = 30; reputation_[3] = 0; reputation_[4] = 20;
  lastDailyUpdate_ = 0;
}

uint32_t LifeWorldSystem::checksum(const LifeWorldSave &save) const {
  const uint8_t *raw = reinterpret_cast<const uint8_t *>(&save);
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < offsetof(LifeWorldSave, checksum); ++i) {
    hash ^= raw[i]; hash *= 16777619UL;
  }
  return hash;
}

bool LifeWorldSystem::valid(const LifeWorldSave &save) const {
  return save.magic == LIFEWORLD_SAVE_MAGIC && save.version == LIFEWORLD_SAVE_VERSION && save.checksum == checksum(save);
}

void LifeWorldSystem::fillSave(LifeWorldSave &s) const {
  memset(&s, 0, sizeof(s)); s.magic = LIFEWORLD_SAVE_MAGIC; s.version = LIFEWORLD_SAVE_VERSION; s.sequence = sequence_ + 1;
  memcpy(s.traits, traits_, sizeof(traits_)); memcpy(s.extraSkills, extraSkills_, sizeof(extraSkills_));
  s.careerId = careerId_; s.careerRank = careerRank_; s.careerXp = careerXp_;
  s.currentLocation = currentLocation_; s.routineScore = routineScore_; s.routineFlags = routineFlags_; s.routineDay = routineDay_; s.lastRoutineHour = lastRoutineHour_;
  s.favoriteRoom = favoriteRoom_; s.favoriteWeather = favoriteWeather_; s.favoriteNeighbor = favoriteNeighbor_; s.fearType = fearType_; s.importantMemoryMask = importantMemoryMask_;
  memcpy(s.roomVisits, roomVisits_, sizeof(roomVisits_)); memcpy(s.residentQuest, residentQuest_, sizeof(residentQuest_)); memcpy(s.residentQuestProgress, residentQuestProgress_, sizeof(residentQuestProgress_));
  s.decorOwnedMask = decorOwnedMask_; memcpy(s.decorPlaced, decorPlaced_, sizeof(decorPlaced_));
  s.expeditionVisitedMask = expeditionVisitedMask_; memcpy(s.expeditionMastery, expeditionMastery_, sizeof(expeditionMastery_));
  s.companionType = companionType_; s.companionBond = companionBond_; s.companionEnergy = companionEnergy_; s.companionMood = companionMood_; s.companionLevel = companionLevel_; s.companionXp = companionXp_;
  s.familyCount = familyCount_; s.familyHead = familyHead_; memcpy(s.familySpecies, familySpecies_, sizeof(familySpecies_)); memcpy(s.familyCareer, familyCareer_, sizeof(familyCareer_));
  memcpy(s.familyPersonality, familyPersonality_, sizeof(familyPersonality_)); memcpy(s.familyAchievement, familyAchievement_, sizeof(familyAchievement_)); memcpy(s.familyHeirloom, familyHeirloom_, sizeof(familyHeirloom_)); memcpy(s.familyGeneration, familyGeneration_, sizeof(familyGeneration_));
  s.scrapCount = scrapCount_; s.scrapHead = scrapHead_; memcpy(s.scrapType, scrapType_, sizeof(scrapType_)); memcpy(s.scrapDay, scrapDay_, sizeof(scrapDay_)); memcpy(s.scrapValue, scrapValue_, sizeof(scrapValue_));
  memcpy(s.projectLevel, projectLevel_, sizeof(projectLevel_)); memcpy(s.projectProgress, projectProgress_, sizeof(projectProgress_)); memcpy(s.festivalWins, festivalWins_, sizeof(festivalWins_)); memcpy(s.festivalBest, festivalBest_, sizeof(festivalBest_)); s.festivalTrophyMask = festivalTrophyMask_;
  memcpy(s.reputation, reputation_, sizeof(reputation_)); s.lastDailyUpdate = lastDailyUpdate_; s.checksum = checksum(s);
}

void LifeWorldSystem::applySave(const LifeWorldSave &s) {
  sequence_ = s.sequence; memcpy(traits_, s.traits, sizeof(traits_)); memcpy(extraSkills_, s.extraSkills, sizeof(extraSkills_));
  careerId_ = s.careerId < LIFE_CAREER_COUNT ? s.careerId : 255; careerRank_ = min<uint8_t>(s.careerRank, 9); careerXp_ = s.careerXp;
  currentLocation_ = s.currentLocation % 8; routineScore_ = min<uint8_t>(s.routineScore, 100); routineFlags_ = s.routineFlags; routineDay_ = s.routineDay; lastRoutineHour_ = s.lastRoutineHour;
  favoriteRoom_ = s.favoriteRoom % 5; favoriteWeather_ = s.favoriteWeather % 5; favoriteNeighbor_ = s.favoriteNeighbor % LIFE_RESIDENT_COUNT; fearType_ = s.fearType % 5; importantMemoryMask_ = s.importantMemoryMask;
  memcpy(roomVisits_, s.roomVisits, sizeof(roomVisits_)); memcpy(residentQuest_, s.residentQuest, sizeof(residentQuest_)); memcpy(residentQuestProgress_, s.residentQuestProgress, sizeof(residentQuestProgress_));
  for (uint8_t &v : residentQuest_) v = min<uint8_t>(v, 3);
  decorOwnedMask_ = s.decorOwnedMask | 1U; memcpy(decorPlaced_, s.decorPlaced, sizeof(decorPlaced_)); for (uint8_t &v : decorPlaced_) if (v >= LIFE_DECOR_COUNT) v = 255;
  expeditionVisitedMask_ = s.expeditionVisitedMask; memcpy(expeditionMastery_, s.expeditionMastery, sizeof(expeditionMastery_));
  companionType_ = s.companionType < 8 ? s.companionType : 255; companionBond_ = min<uint8_t>(s.companionBond, 100); companionEnergy_ = min<uint8_t>(s.companionEnergy, 10); companionMood_ = min<uint8_t>(s.companionMood, 10); companionLevel_ = min<uint8_t>(s.companionLevel, 9); companionXp_ = s.companionXp;
  familyCount_ = min<uint8_t>(s.familyCount, LIFE_FAMILY_COUNT); familyHead_ = s.familyHead % LIFE_FAMILY_COUNT; memcpy(familySpecies_, s.familySpecies, sizeof(familySpecies_)); memcpy(familyCareer_, s.familyCareer, sizeof(familyCareer_));
  memcpy(familyPersonality_, s.familyPersonality, sizeof(familyPersonality_)); memcpy(familyAchievement_, s.familyAchievement, sizeof(familyAchievement_)); memcpy(familyHeirloom_, s.familyHeirloom, sizeof(familyHeirloom_)); memcpy(familyGeneration_, s.familyGeneration, sizeof(familyGeneration_));
  scrapCount_ = min<uint8_t>(s.scrapCount, LIFE_SCRAP_COUNT); scrapHead_ = s.scrapHead % LIFE_SCRAP_COUNT; memcpy(scrapType_, s.scrapType, sizeof(scrapType_)); memcpy(scrapDay_, s.scrapDay, sizeof(scrapDay_)); memcpy(scrapValue_, s.scrapValue, sizeof(scrapValue_));
  memcpy(projectLevel_, s.projectLevel, sizeof(projectLevel_)); memcpy(projectProgress_, s.projectProgress, sizeof(projectProgress_)); memcpy(festivalWins_, s.festivalWins, sizeof(festivalWins_)); memcpy(festivalBest_, s.festivalBest, sizeof(festivalBest_)); festivalTrophyMask_ = s.festivalTrophyMask;
  memcpy(reputation_, s.reputation, sizeof(reputation_)); for (uint8_t &v : reputation_) v = min<uint8_t>(v, 100); lastDailyUpdate_ = s.lastDailyUpdate;
  for (uint8_t &v : traits_) v = min<uint8_t>(v, 100); for (uint8_t &v : extraSkills_) v = min<uint8_t>(v, 100);
}

bool LifeWorldSystem::load() {
  LifeWorldSave a = storeA_.read(), b = storeB_.read(); bool va = valid(a), vb = valid(b);
  if (!va && !vb) { reset(); return false; }
  applySave(va && vb ? (a.sequence >= b.sequence ? a : b) : (va ? a : b)); return true;
}

void LifeWorldSystem::saveNow() {
  LifeWorldSave s; fillSave(s); if (s.sequence & 1UL) storeA_.write(s); else storeB_.write(s); sequence_ = s.sequence;
}

void LifeWorldSystem::changeByte(uint8_t &v, int amount, uint8_t maximum) {
  int n = static_cast<int>(v) + amount; v = static_cast<uint8_t>(constrain(n, 0, maximum));
}

uint8_t LifeWorldSystem::skillValue(uint8_t skill) const {
  if (skill < LIFE_SKILL_COUNT) return extraSkills_[skill];
  if (skill == 8) return min<uint8_t>(100, skillSmart * 5);
  if (skill == 9) return min<uint8_t>(100, skillFit * 5);
  if (skill == 10) return min<uint8_t>(100, skillStyle * 5);
  return min<uint8_t>(100, skillKind * 5);
}

uint8_t LifeWorldSystem::dominantAdvancedSkill() const {
  uint8_t best = 0; for (uint8_t i = 1; i < LIFE_SKILL_COUNT; ++i) if (extraSkills_[i] > extraSkills_[best]) best = i; return best;
}

void LifeWorldSystem::startGeneration(uint8_t species, uint16_t gen, uint16_t code, uint8_t temp, uint8_t favoriteFood) {
  const uint8_t legacySkill = dominantAdvancedSkill();
  const uint8_t legacyValue = extraSkills_[legacySkill];
  memset(extraSkills_, 0, sizeof(extraSkills_));
  if (gen > 1 && legacyValue >= 20) extraSkills_[legacySkill] = 5; // one parent-taught head start
  careerId_ = 255; careerRank_ = 0; careerXp_ = 0;
  traits_[0] = static_cast<uint8_t>(35 + (code + species * 7) % 36);
  traits_[1] = static_cast<uint8_t>(35 + (code / 3 + temp * 11) % 36);
  traits_[2] = static_cast<uint8_t>(35 + (code / 7 + species * 3) % 36);
  traits_[3] = static_cast<uint8_t>(40 + (code / 11 + gen) % 31);
  currentLocation_ = 0; routineFlags_ = 0; routineScore_ = 50; lastRoutineHour_ = 255;
  favoriteRoom_ = static_cast<uint8_t>((favoriteFood + species) % 5);
  favoriteWeather_ = static_cast<uint8_t>((species + temp) % 5); fearType_ = static_cast<uint8_t>((code + temp) % 5);
  importantMemoryMask_ = 1; recordMemory(0, 0, species); markSaveDirty();
}

void LifeWorldSystem::archiveGeneration(uint8_t species, uint16_t gen, uint8_t temp, uint8_t favoriteFood, uint8_t ageDays) {
  uint8_t idx = familyHead_ % LIFE_FAMILY_COUNT;
  familySpecies_[idx] = species; familyCareer_[idx] = careerId_; familyPersonality_[idx] = temp;
  uint8_t bestSkill = dominantAdvancedSkill();
  familyAchievement_[idx] = festivalTrophyMask_ ? 3 : (careerRank_ >= 3 ? 2 : (expeditionVisitedMask_ ? 1 : 0));
  familyHeirloom_[idx] = static_cast<uint8_t>((bestSkill + favoriteFood + ageDays) % LIFE_DECOR_COUNT);
  familyGeneration_[idx] = gen;
  familyHead_ = static_cast<uint8_t>((familyHead_ + 1) % LIFE_FAMILY_COUNT); if (familyCount_ < LIFE_FAMILY_COUNT) ++familyCount_;
  decorOwnedMask_ |= static_cast<uint16_t>(1U << familyHeirloom_[idx]);
  recordMemory(9, ageDays, species); markSaveDirty();
}

void LifeWorldSystem::recordMemory(uint8_t type, uint32_t day, uint8_t value) {
  uint8_t idx = scrapHead_ % LIFE_SCRAP_COUNT; scrapType_[idx] = type; scrapDay_[idx] = static_cast<uint16_t>(min<uint32_t>(day, 65535)); scrapValue_[idx] = value;
  scrapHead_ = static_cast<uint8_t>((scrapHead_ + 1) % LIFE_SCRAP_COUNT); if (scrapCount_ < LIFE_SCRAP_COUNT) ++scrapCount_;
  importantMemoryMask_ |= static_cast<uint16_t>(1U << (type % 16));
}

void LifeWorldSystem::recordAction(uint8_t action, uint32_t day, uint8_t hour, uint8_t weather) {
  routineDay_ = day; lastRoutineHour_ = hour; routineFlags_ |= static_cast<uint8_t>(1U << (action % 8));
  if (routineScore_ < 100) ++routineScore_;
  favoriteWeather_ = weather % 5;
  if (action == ACTION_COOK || action == ACTION_FOOD) { changeByte(extraSkills_[0], 2); changeByte(traits_[3], 1); currentLocation_ = 1; }
  else if (action == ACTION_GARDEN) { changeByte(extraSkills_[1], 2); changeByte(traits_[3], 1); currentLocation_ = 5; }
  else if (action == ACTION_MUSIC) { changeByte(extraSkills_[2], 2); changeByte(extraSkills_[6], 1); }
  else if (action == ACTION_MED || action == ACTION_CLEAN) { changeByte(extraSkills_[3], 2); changeByte(traits_[3], 2); changeByte(reputation_[2], 1); }
  else if (action == ACTION_EXPLORE || action == ACTION_WALK) { changeByte(extraSkills_[4], 2); changeByte(traits_[0], 1); changeByte(traits_[2], 1); changeByte(reputation_[4], 1); }
  else if (action == ACTION_TALK || action == ACTION_LOVE || action == ACTION_CALL || action == ACTION_FAMILY) { changeByte(extraSkills_[5], 2); changeByte(traits_[1], 2); changeByte(reputation_[0], 1); }
  else if (action == ACTION_ROOM || action == ACTION_CRAFT || action == ACTION_TOY) { changeByte(extraSkills_[6], 2); changeByte(traits_[0], 1); favoriteRoom_ = roomStyle % 5; if (roomVisits_[favoriteRoom_] < 255) ++roomVisits_[favoriteRoom_]; }
  else if (action == ACTION_STUDY) { changeByte(extraSkills_[7], 2); changeByte(traits_[0], 2); }
  else if (action == ACTION_TRAIN) { changeByte(traits_[2], 2); changeByte(reputation_[2], 1); }
  else if (action == ACTION_JOB) { changeByte(traits_[3], 2); changeByte(reputation_[2], 1); }
  else if (action == ACTION_GAME) { changeByte(traits_[1], 1); changeByte(reputation_[1], 1); }
  markSaveDirty();
}

void LifeWorldSystem::recordGameResult(uint8_t gameId, bool won) {
  static const uint8_t skillMap[13] = {4,5,7,7,4,6,7,4,7,4,4,0,7};
  uint8_t s = skillMap[gameId % 13]; changeByte(extraSkills_[s], won ? 3 : 1);
  changeByte(reputation_[1], won ? 2 : 1); if (won) changeByte(reputation_[3], 1);
  if (companionType_ < 8) { changeByte(companionMood_, 1, 10); changeByte(companionBond_, 1); companionXp_ += won ? 3 : 1; }
}

void LifeWorldSystem::awardQuest(uint8_t resident, uint8_t qstage) {
  addCoins(3 + qstage * 2); changeByte(reputation_[0], 3); changeByte(reputation_[3], 1);
  decorOwnedMask_ |= static_cast<uint16_t>(1U << ((resident + qstage * 3) % LIFE_DECOR_COUNT));
  recordMemory(3, petAgeDays(), resident); winSound();
}

void LifeWorldSystem::recordNeighborInteraction(uint8_t resident, uint8_t interaction, uint8_t relation) {
  resident %= LIFE_RESIDENT_COUNT; favoriteNeighbor_ = resident; changeByte(extraSkills_[5], interaction == 0 ? 2 : 1); changeByte(traits_[1], 1);
  uint8_t q = residentQuest_[resident]; bool complete = false;
  if (q == 0 && interaction == 0 && relation >= 10) complete = true;
  else if (q == 1 && interaction == 1 && relation >= 30) complete = true;
  else if (q == 2 && interaction == 2 && relation >= 55) complete = true;
  if (complete) { residentQuest_[resident] = q + 1; awardQuest(resident, q + 1); }
  else if (residentQuestProgress_[resident] < 100) residentQuestProgress_[resident]++;
  markSaveDirty();
}

uint8_t LifeWorldSystem::calculateLocation(uint8_t st, uint8_t hour, bool weekday) const {
  uint8_t bedtime = 21;
  uint8_t wake = 7;
  if (st == STAGE_BABY_L) bedtime = 19;
  else if (st == STAGE_CHILD_L) bedtime = 20;
  else if (st == STAGE_TEEN_L) bedtime = 22;
  else if (st == STAGE_ADULT_L) { bedtime = 23; wake = 6; }
  else if (st == STAGE_ELDER_L) { bedtime = 21; wake = 6; }
  if (hour >= bedtime || hour < wake) return 7;
  if (hour == 7 || hour == 12 || hour == 18) return 1;
  if (st <= STAGE_BABY_L) return hour >= 15 && hour < 18 ? 4 : 0;
  if (st == STAGE_CHILD_L) { if (weekday && hour >= 8 && hour < 15) return 2; if (hour >= 15 && hour < 18) return 4; return 0; }
  if (st == STAGE_TEEN_L) { if (weekday && hour >= 8 && hour < 15) return 2; if (hour >= 15 && hour < 18) return 3; return 0; }
  if (st == STAGE_ELDER_L) return hour >= 9 && hour < 12 ? 5 : 0;
  return weekday && hour >= 9 && hour < 17 && careerId_ < LIFE_CAREER_COUNT ? 3 : (hour >= 17 && hour < 19 ? 4 : 0);
}

void LifeWorldSystem::tick(uint32_t day, uint8_t hour, uint8_t st, uint8_t weather, bool weekday) {
  currentLocation_ = calculateLocation(st, hour, weekday);
  if (lastDailyUpdate_ != day) {
    lastDailyUpdate_ = day;
    if (routineFlags_ >= 3) changeByte(routineScore_, 2); else changeByte(routineScore_, -2);
    routineFlags_ = 0;
    if (companionType_ < 8) {
      changeByte(companionEnergy_, -1, 10); changeByte(companionMood_, -1, 10);
      if (companionBond_ > 0 && companionMood_ == 0) changeByte(companionBond_, -1);
    }
    if (favoriteWeather_ == weather) changeByte(traits_[0], 1);
    markSaveDirty();
  }
  lastRoutineHour_ = hour;
}

uint8_t LifeWorldSystem::gameAssist(uint8_t gameId) const {
  static const uint8_t skillMap[13] = {4,5,7,7,4,6,7,4,7,4,4,0,7};
  uint8_t value = extraSkills_[skillMap[gameId % 13]];
  if (gameId == favoriteGameId) value = min<uint8_t>(100, value + 15);
  if (companionType_ < 8 && companionBond_ >= 60) value = min<uint8_t>(100, value + 10);
  return value >= 75 ? 2 : (value >= 35 ? 1 : 0);
}

void LifeWorldSystem::eventAdjustCompanion(int bond, int mood, int energyDelta, uint16_t xp) {
  if (companionType_ >= 8) return;
  changeByte(companionBond_, bond);
  changeByte(companionMood_, mood, 10);
  changeByte(companionEnergy_, energyDelta, 10);
  companionXp_ = static_cast<uint16_t>(min<uint32_t>(65535UL, static_cast<uint32_t>(companionXp_) + xp));
  while (companionLevel_ < 9 && companionXp_ >= static_cast<uint16_t>((companionLevel_ + 1) * 25U)) ++companionLevel_;
  markSaveDirty();
}

void LifeWorldSystem::eventCareerProgress(uint16_t xp, uint8_t reputationKind, int reputationDelta) {
  addCareerXp(xp);
  changeByte(reputation_[reputationKind % 5], reputationDelta);
  markSaveDirty();
}

void LifeWorldSystem::eventCommunityProgress(uint8_t project, uint8_t progress, uint8_t reputationKind, int reputationDelta) {
  project %= LIFE_PROJECT_COUNT;
  if (projectLevel_[project] < 3) {
    uint16_t total = static_cast<uint16_t>(projectProgress_[project]) + progress;
    while (total >= 10 && projectLevel_[project] < 3) { total -= 10; ++projectLevel_[project]; recordMemory(8, petAgeDays(), project); }
    projectProgress_[project] = static_cast<uint8_t>(min<uint16_t>(9, total));
  }
  changeByte(reputation_[reputationKind % 5], reputationDelta);
  markSaveDirty();
}

bool LifeWorldSystem::residentAvailable(uint8_t resident, uint8_t hour) const {
  resident %= LIFE_RESIDENT_COUNT;
  if (hour < 8 || hour >= 20) return true;
  uint8_t awayStart = static_cast<uint8_t>(8 + (resident % 3));
  uint8_t awayEnd = static_cast<uint8_t>(14 + (resident % 4));
  return !(hour >= awayStart && hour < awayEnd);
}

const __FlashStringHelper *LifeWorldSystem::residentLocationName(uint8_t resident, uint8_t hour) const {
  if (residentAvailable(resident, hour)) return F("Home");
  switch (resident % LIFE_RESIDENT_COUNT) {
    case 0: return F("Cafe"); case 1: return F("School"); case 2: return F("Market"); case 3: return F("Garden");
    case 4: return F("Observatory"); case 5: return F("Kitchen"); case 6: return F("Music Hall"); default: return F("Workshop");
  }
}

const __FlashStringHelper *LifeWorldSystem::locationName(uint8_t l) const {
  switch (l % 8) { case 0:return F("Pet Room"); case 1:return F("Kitchen"); case 2:return F("School"); case 3:return F("Work"); case 4:return F("Park"); case 5:return F("Garden"); case 6:return F("Market"); default:return F("Sleeping"); }
}

const char *LifeWorldSystem::companionName() const {
  return companionType_ < 8 ? COMPANION_NAMES[companionType_] : nullptr;
}

const __FlashStringHelper *LifeWorldSystem::personalityThought() const {
  uint8_t best = 0;
  for (uint8_t i = 1; i < 4; ++i) if (traits_[i] > traits_[best]) best = i;
  if (currentLocation_ == 1) return F("Can we cook together?");
  if (currentLocation_ == 2) return F("School taught me lots!");
  if (currentLocation_ == 3) return careerId_ < LIFE_CAREER_COUNT ? F("I worked hard today!") : F("What job fits me?");
  if (currentLocation_ == 4) return F("The park feels lively!");
  if (currentLocation_ == 5) return F("Let's check the garden!");
  if (best == 0) return F("What can we discover?");
  if (best == 1) return F("Let's visit a friend!");
  if (best == 2) return F("I want an adventure!");
  return F("I finished my routine!");
}

const __FlashStringHelper *LifeWorldSystem::careerName(uint8_t c) const {
  switch (c % LIFE_CAREER_COUNT) { case 0:return F("Chef"); case 1:return F("Musician"); case 2:return F("Scientist"); case 3:return F("Gardener"); case 4:return F("Doctor"); case 5:return F("Teacher"); case 6:return F("Artist"); case 7:return F("Astronaut"); case 8:return F("Shopkeeper"); default:return F("Athlete"); }
}

uint8_t LifeWorldSystem::careerPayBonus() const { return careerId_ < LIFE_CAREER_COUNT ? static_cast<uint8_t>(careerRank_ + extraSkills_[careerId_ % LIFE_SKILL_COUNT] / 20) : 0; }

void LifeWorldSystem::addCareerXp(uint16_t amount) {
  careerXp_ = min<uint16_t>(65000, careerXp_ + amount); uint16_t need = static_cast<uint16_t>((careerRank_ + 1) * 25);
  if (careerRank_ < 9 && careerXp_ >= need) { careerXp_ -= need; ++careerRank_; recordMemory(6, petAgeDays(), careerId_); winSound(); }
}

void LifeWorldSystem::workCareer() {
  if (stage < STAGE_ADULT_L) { popup(F("Adult career only"), F("Study while growing")); return; }
  uint8_t pick = careerId_ < LIFE_CAREER_COUNT ? careerId_ : 0; bool ol=HIGH,os=HIGH,orr=HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return; bool l=digitalRead(btnL_),s=digitalRead(btnS_),r=digitalRead(btnR_);
    if (ol==HIGH&&l==LOW){pick=pick?pick-1:LIFE_CAREER_COUNT-1;clickSound();delay(100);} if(orr==HIGH&&r==LOW){pick=(pick+1)%LIFE_CAREER_COUNT;clickSound();delay(100);}
    if(os==HIGH&&s==LOW){waitButtonRelease(); if(careerId_!=pick){careerId_=pick;careerRank_=0;careerXp_=0;goodSound();popup(F("Career selected"),careerName(pick));return;} if(energy<2){badSound();popup(F("Too tired"),F("Rest first"));return;} uint8_t skill=extraSkills_[pick%LIFE_SKILL_COUNT]; uint8_t pay=static_cast<uint8_t>(3+careerRank_+skill/18); addCoins(pay); addCareerXp(static_cast<uint16_t>(5+skill/20)); subStat(energy,2); subStat(full,1); changeByte(reputation_[2],2); changeByte(reputation_[3],1); recordMemory(5,petAgeDays(),pick); markSaveDirty(); goodSound(); popup(F("Work complete"),careerName(pick));return;}
    ol=l;os=s;orr=r; display_.clearDisplay();drawHeader(F("CAREERS"),pick+1,LIFE_CAREER_COUNT);display_.drawRoundRect(4,15,120,38,5,SSD1306_WHITE);drawCurrentPetTiny(10,31);display_.setCursor(40,22);display_.print(careerName(pick));display_.setCursor(40,34);display_.print(F("Rank "));display_.print(careerId_==pick?careerRank_:0);display_.setCursor(40,44);display_.print(F("Skill "));display_.print(extraSkills_[pick%LIFE_SKILL_COUNT]);display_.setCursor(2,56);display_.print(careerId_==pick?F("D1 work"):F("D1 choose career"));display_.display();delay(25);
  }
}

void LifeWorldSystem::drawHeader(const __FlashStringHelper *title, uint8_t page, uint8_t count) const {
  display_.setTextSize(1); display_.setTextColor(SSD1306_WHITE); display_.setCursor(2,1); display_.print(title); display_.setCursor(104,1); display_.print(page); display_.print(F("/")); display_.print(count); display_.drawLine(0,10,127,10,SSD1306_WHITE);
}

void LifeWorldSystem::drawLifeIcon(int x,int y,bool inverted) const {
  uint16_t c=inverted?SSD1306_BLACK:SSD1306_WHITE; display_.drawCircle(x+16,y+15,11,c);display_.drawCircle(x+12,y+13,2,c);display_.drawCircle(x+20,y+13,2,c);display_.drawLine(x+11,y+20,x+16,y+23,c);display_.drawLine(x+16,y+23,x+21,y+20,c);display_.drawPixel(x+5,y+3,c);display_.drawPixel(x+27,y+5,c);
}

void LifeWorldSystem::drawHubIcon(uint8_t id,int x,int y) const {
  switch(id%15){
    case 0: display_.drawCircle(x+14,y+14,11,SSD1306_WHITE);display_.drawCircle(x+10,y+12,2,SSD1306_WHITE);display_.drawCircle(x+18,y+12,2,SSD1306_WHITE);display_.drawLine(x+9,y+19,x+19,y+19,SSD1306_WHITE);break;
    case 1: display_.drawRect(x+5,y+7,20,18,SSD1306_WHITE);display_.drawLine(x+9,y+11,x+21,y+11,SSD1306_WHITE);display_.drawLine(x+9,y+16,x+19,y+16,SSD1306_WHITE);break;
    case 2: display_.drawCircle(x+15,y+15,11,SSD1306_WHITE);display_.drawLine(x+15,y+15,x+15,y+7,SSD1306_WHITE);display_.drawLine(x+15,y+15,x+22,y+18,SSD1306_WHITE);break;
    case 3: display_.drawCircle(x+10,y+13,6,SSD1306_WHITE);display_.drawCircle(x+21,y+13,6,SSD1306_WHITE);display_.drawLine(x+6,y+24,x+25,y+24,SSD1306_WHITE);break;
    case 4: display_.drawRoundRect(x+3,y+10,25,14,5,SSD1306_WHITE);display_.drawLine(x+9,y+17,x+15,y+17,SSD1306_WHITE);display_.drawLine(x+12,y+14,x+12,y+20,SSD1306_WHITE);display_.drawCircle(x+22,y+17,2,SSD1306_WHITE);break;
    case 5: display_.drawRect(x+4,y+8,23,17,SSD1306_WHITE);display_.drawRoundRect(x+8,y+17,8,7,2,SSD1306_WHITE);display_.drawCircle(x+22,y+13,3,SSD1306_WHITE);break;
    case 6: display_.drawTriangle(x+15,y+4,x+4,y+25,x+26,y+25,SSD1306_WHITE);display_.drawLine(x+10,y+18,x+15,y+13,SSD1306_WHITE);display_.drawLine(x+15,y+13,x+20,y+18,SSD1306_WHITE);break;
    case 7: drawCompanionSprite(0,x+8,y+9,0);break;
    case 8: display_.drawLine(x+15,y+4,x+15,y+26,SSD1306_WHITE);display_.drawLine(x+15,y+10,x+7,y+16,SSD1306_WHITE);display_.drawLine(x+15,y+10,x+23,y+16,SSD1306_WHITE);display_.drawCircle(x+7,y+19,4,SSD1306_WHITE);display_.drawCircle(x+23,y+19,4,SSD1306_WHITE);break;
    case 9: display_.drawRect(x+5,y+5,21,22,SSD1306_WHITE);display_.drawLine(x+9,y+10,x+22,y+10,SSD1306_WHITE);display_.drawLine(x+9,y+15,x+20,y+15,SSD1306_WHITE);display_.drawLine(x+9,y+20,x+18,y+20,SSD1306_WHITE);break;
    case 10: display_.drawCircle(x+15,y+15,11,SSD1306_WHITE);display_.drawLine(x+15,y+8,x+15,y+22,SSD1306_WHITE);display_.drawLine(x+8,y+15,x+22,y+15,SSD1306_WHITE);break;
    case 11: display_.drawRect(x+4,y+14,23,12,SSD1306_WHITE);display_.drawTriangle(x+4,y+14,x+15,y+5,x+27,y+14,SSD1306_WHITE);display_.drawRect(x+13,y+18,5,8,SSD1306_WHITE);break;
    case 12: display_.drawCircle(x+15,y+15,10,SSD1306_WHITE);display_.drawLine(x+15,y+2,x+15,y+28,SSD1306_WHITE);display_.drawLine(x+2,y+15,x+28,y+15,SSD1306_WHITE);break;
    case 13: display_.drawCircle(x+15,y+15,11,SSD1306_WHITE);display_.drawLine(x+9,y+19,x+21,y+19,SSD1306_WHITE);display_.drawPixel(x+8,y+8,SSD1306_WHITE);display_.drawPixel(x+22,y+8,SSD1306_WHITE);break;
    default: display_.drawRect(x+5,y+6,21,20,SSD1306_WHITE);display_.drawLine(x+9,y+10,x+22,y+10,SSD1306_WHITE);display_.drawLine(x+9,y+15,x+22,y+15,SSD1306_WHITE);display_.drawLine(x+9,y+20,x+18,y+20,SSD1306_WHITE);break;
  }
}

void LifeWorldSystem::drawCompanionSprite(uint8_t id,int x,int y,uint8_t frame) const {
  id%=8; int b=(frame&1); display_.fillRoundRect(x,y+b,14,12,4,SSD1306_BLACK);display_.drawRoundRect(x,y+b,14,12,4,SSD1306_WHITE);
  if(id==0){display_.drawTriangle(x+2,y+2+b,x+4,y-3+b,x+7,y+2+b,SSD1306_WHITE);display_.drawTriangle(x+7,y+2+b,x+10,y-3+b,x+12,y+2+b,SSD1306_WHITE);}
  else if(id==1){display_.drawLine(x+2,y+3+b,x-2,y-1+b,SSD1306_WHITE);display_.drawLine(x+12,y+3+b,x+16,y-1+b,SSD1306_WHITE);}
  else if(id==2){display_.drawCircle(x+3,y+1+b,2,SSD1306_WHITE);display_.drawCircle(x+11,y+1+b,2,SSD1306_WHITE);}
  else if(id==3){display_.drawRect(x+2,y-2+b,10,5,SSD1306_WHITE);display_.drawPixel(x+5,y+b,SSD1306_BLACK);display_.drawPixel(x+9,y+b,SSD1306_BLACK);}
  else if(id==4){display_.drawLine(x-3,y+6+b,x,y+4+b,SSD1306_WHITE);display_.drawLine(x-3,y+6+b,x,y+8+b,SSD1306_WHITE);}
  else if(id==5){display_.drawLine(x+7,y+b,x+7,y-5+b,SSD1306_WHITE);display_.drawCircle(x+3,y-4+b,3,SSD1306_WHITE);display_.drawCircle(x+11,y-4+b,3,SSD1306_WHITE);}
  else if(id==6){display_.drawLine(x+2,y+11+b,x-1,y+15+b,SSD1306_WHITE);display_.drawLine(x+12,y+11+b,x+15,y+15+b,SSD1306_WHITE);}
  else{display_.drawLine(x+4,y+b,x+2,y-4+b,SSD1306_WHITE);display_.drawLine(x+10,y+b,x+12,y-4+b,SSD1306_WHITE);display_.drawCircle(x+2,y-5+b,1,SSD1306_WHITE);display_.drawCircle(x+12,y-5+b,1,SSD1306_WHITE);}
  display_.drawPixel(x+4,y+5+b,SSD1306_WHITE);display_.drawPixel(x+9,y+5+b,SSD1306_WHITE);display_.drawLine(x+5,y+9+b,x+8,y+9+b,SSD1306_WHITE);
}

void LifeWorldSystem::drawCompanion(int x,int y,uint8_t frame) const { if(companionType_<8) drawCompanionSprite(companionType_,x,y,frame); }

void LifeWorldSystem::drawDecorSprite(uint8_t id,int x,int y) const {
  id%=LIFE_DECOR_COUNT;
  if(id==0){display_.drawRoundRect(x,y+5,18,9,4,SSD1306_WHITE);display_.drawLine(x+3,y+9,x+15,y+9,SSD1306_WHITE);} else if(id==1){display_.drawRect(x+5,y+10,9,5,SSD1306_WHITE);display_.drawLine(x+9,y+10,x+9,y+1,SSD1306_WHITE);display_.drawCircle(x+4,y+4,4,SSD1306_WHITE);display_.drawCircle(x+14,y+5,4,SSD1306_WHITE);} else if(id==2){display_.drawLine(x+9,y+15,x+9,y+4,SSD1306_WHITE);display_.drawRoundRect(x+3,y,12,6,2,SSD1306_WHITE);display_.drawLine(x+4,y+15,x+14,y+15,SSD1306_WHITE);} else if(id==3){for(int i=0;i<3;i++)display_.drawRect(x+2+i*5,y+4-i*2,4,11+i*2,SSD1306_WHITE);} else if(id==4){display_.drawRoundRect(x+1,y+3,17,12,3,SSD1306_WHITE);display_.drawCircle(x+5,y+9,3,SSD1306_WHITE);display_.drawLine(x+11,y+7,x+16,y+7,SSD1306_WHITE);display_.drawLine(x+11,y+11,x+16,y+11,SSD1306_WHITE);} else if(id==5){display_.drawTriangle(x+9,y,x+3,y+10,x+15,y+10,SSD1306_WHITE);display_.drawRect(x+7,y+10,4,5,SSD1306_WHITE);} else if(id==6){display_.drawCircle(x+9,y+7,7,SSD1306_WHITE);display_.drawLine(x+9,y+7,x+9,y+2,SSD1306_WHITE);display_.drawLine(x+9,y+7,x+13,y+9,SSD1306_WHITE);} else if(id==7){display_.drawCircle(x+7,y+10,5,SSD1306_WHITE);display_.drawRect(x+11,y+7,6,6,SSD1306_WHITE);display_.drawLine(x+2,y+15,x+18,y+15,SSD1306_WHITE);} else if(id==8){display_.drawRect(x+3,y+4,13,11,SSD1306_WHITE);display_.drawPixel(x+7,y+8,SSD1306_WHITE);display_.drawPixel(x+12,y+8,SSD1306_WHITE);display_.drawLine(x+7,y+12,x+12,y+12,SSD1306_WHITE);display_.drawLine(x+6,y+3,x+4,y,SSD1306_WHITE);display_.drawLine(x+13,y+3,x+15,y,SSD1306_WHITE);} else if(id==9){display_.drawRect(x+2,y+2,16,13,SSD1306_WHITE);display_.drawCircle(x+10,y+8,4,SSD1306_WHITE);display_.drawPixel(x+10,y+7,SSD1306_WHITE);} else if(id==10){display_.drawCircle(x+9,y+7,7,SSD1306_WHITE);display_.drawLine(x+9,y+14,x+9,y+16,SSD1306_WHITE);display_.drawLine(x+4,y+16,x+14,y+16,SSD1306_WHITE);display_.drawLine(x+3,y+7,x+15,y+7,SSD1306_WHITE);} else {display_.drawRect(x+1,y+6,17,9,SSD1306_WHITE);display_.drawLine(x+1,y+6,x+9,y+1,SSD1306_WHITE);display_.drawLine(x+18,y+6,x+9,y+1,SSD1306_WHITE);display_.drawRect(x+7,y+9,4,3,SSD1306_WHITE);}
}

void LifeWorldSystem::drawHomeDecor(uint8_t room,uint8_t frame) const {
  (void)room;(void)frame;
  if(decorPlaced_[0]<LIFE_DECOR_COUNT){display_.fillRect(1,35,22,20,SSD1306_BLACK);drawDecorSprite(decorPlaced_[0],3,38);} if(decorPlaced_[1]<LIFE_DECOR_COUNT){display_.fillRect(104,35,23,20,SSD1306_BLACK);drawDecorSprite(decorPlaced_[1],106,38);} if(decorPlaced_[2]<LIFE_DECOR_COUNT){display_.fillRect(104,12,22,19,SSD1306_BLACK);drawDecorSprite(decorPlaced_[2],106,14);}
}

void LifeWorldSystem::drawCommunityUpgrades(uint8_t cityPlace,uint8_t frame) const {
  (void)frame;
  // Tiny edge details show permanent city progress while preserving the central
  // 28x24 character pad and the top title/footer bands.
  if(cityPlace==3 && projectLevel_[0]){ // school
    display_.drawLine(112,13,112,29,SSD1306_WHITE);display_.drawTriangle(112,13,123,17,112,21,SSD1306_WHITE);
    if(projectLevel_[0]>=2)display_.drawRect(99,34,22,15,SSD1306_WHITE);
  }
  if(cityPlace==4 && projectLevel_[1]){ // park
    display_.drawLine(10,36,10,50,SSD1306_WHITE);display_.drawCircle(10,31,7,SSD1306_WHITE);
    if(projectLevel_[1]>=2){display_.drawLine(101,47,119,47,SSD1306_WHITE);display_.drawLine(104,47,104,51,SSD1306_WHITE);display_.drawLine(116,47,116,51,SSD1306_WHITE);}
  }
  if(cityPlace==0 && projectLevel_[2]){ // clinic sign
    display_.drawRect(4,14,17,17,SSD1306_WHITE);display_.drawLine(12,17,12,28,SSD1306_WHITE);display_.drawLine(7,22,17,22,SSD1306_WHITE);
  }
  if(cityPlace==1 && projectLevel_[3]){ // market awning
    display_.drawLine(3,15,25,15,SSD1306_WHITE);for(int x=4;x<25;x+=6)display_.drawLine(x,15,x+3,20,SSD1306_WHITE);
  }
  if(cityPlace==5 && projectLevel_[4]){ // museum banner
    display_.drawRect(4,13,22,15,SSD1306_WHITE);display_.drawTriangle(4,13,15,8,26,13,SSD1306_WHITE);display_.drawLine(9,16,9,26,SSD1306_WHITE);display_.drawLine(15,16,15,26,SSD1306_WHITE);display_.drawLine(21,16,21,26,SSD1306_WHITE);
  }
  if(cityPlace==0 && projectLevel_[5]){ // spaceport dish
    display_.drawLine(111,43,120,30,SSD1306_WHITE);display_.drawCircle(121,28,5,SSD1306_WHITE);display_.drawLine(107,48,122,48,SSD1306_WHITE);
  }
}

void LifeWorldSystem::popup(const __FlashStringHelper *a,const __FlashStringHelper *b,uint16_t holdMs) const {display_.clearDisplay();display_.setTextSize(1);display_.setTextColor(SSD1306_WHITE);display_.drawRoundRect(3,13,122,39,5,SSD1306_WHITE);display_.setCursor(10,23);display_.print(a);display_.setCursor(10,36);display_.print(b);display_.display();delay(holdMs);}
void LifeWorldSystem::popup(const __FlashStringHelper *a,const char *b,uint16_t holdMs) const {display_.clearDisplay();display_.setTextSize(1);display_.setTextColor(SSD1306_WHITE);display_.drawRoundRect(3,13,122,39,5,SSD1306_WHITE);display_.setCursor(10,23);display_.print(a);display_.setCursor(10,36);display_.print(b);display_.display();delay(holdMs);}

void LifeWorldSystem::runHub() {
  uint8_t pick=0;bool ol=HIGH,os=HIGH,orr=HIGH;waitButtonRelease();
  while(true){if(rightHoldExitCheck())return;bool l=digitalRead(btnL_),s=digitalRead(btnS_),r=digitalRead(btnR_);if(ol==HIGH&&l==LOW){pick=pick?pick-1:15;clickSound();delay(100);}if(orr==HIGH&&r==LOW){pick=(pick+1)%16;clickSound();delay(100);}if(os==HIGH&&s==LOW){waitButtonRelease();switch(pick){case 0:personalityPage();break;case 1:skillsPage();break;case 2:routinePage();break;case 3:residentQuestPage();break;case 4:gameTalentPage();break;case 5:decorPage();break;case 6:expeditionPage();break;case 7:companionPage();break;case 8:familyTreePage();break;case 9:scrapbookPage();break;case 10:clinicPage();break;case 11:communityPage();break;case 12:festivalPage();break;case 13:reputationPage();break;case 14:openDeepLife();break;default:openSaveProfiles();break;}ol=os=orr=HIGH;waitButtonRelease();}ol=l;os=s;orr=r;display_.clearDisplay();drawHeader(F("LIVING WORLD"),pick+1,16);display_.drawRoundRect(4,15,120,37,5,SSD1306_WHITE);drawHubIcon(pick,10,19);display_.setCursor(46,23);display_.print(HUB_NAMES[pick]);display_.setCursor(46,36);if(pick==0)display_.print(F("Memories shape behavior"));else if(pick==1)display_.print(F("12 skills, 10 careers"));else if(pick==2)display_.print(locationName(currentLocation_));else if(pick==3)display_.print(F("8 resident questlines"));else if(pick==4)display_.print(F("Practice changes play"));else if(pick==5)display_.print(F("3 safe decor slots"));else if(pick==6)display_.print(F("6 branching regions"));else if(pick==7)display_.print(companionType_<8?COMPANION_NAMES[companionType_]:"Adopt a small friend");else if(pick==8)display_.print(F("Heirlooms and ancestors"));else if(pick==9)display_.print(F("Automatic memories"));else if(pick==10)display_.print(F("Symptoms and treatment"));else if(pick==11)display_.print(F("Build the whole city"));else if(pick==12)display_.print(F("8 recurring contests"));else if(pick==13)display_.print(F("Choices change reputation"));else if(pick==14)display_.print(F("Stories, rivals, museum"));else display_.print(F("Main / test / backup"));display_.setCursor(1,56);display_.print(F("D0/D2 browse  D1 open"));display_.display();delay(25);}
}

void LifeWorldSystem::personalityPage(){uint8_t p=0;bool ol=HIGH,os=HIGH,orr=HIGH;waitButtonRelease();while(true){if(rightHoldExitCheck())return;bool l=digitalRead(btnL_),s=digitalRead(btnS_),r=digitalRead(btnR_);if(ol==HIGH&&l==LOW){p=p? p-1:2;clickSound();delay(100);}if(orr==HIGH&&r==LOW){p=(p+1)%3;clickSound();delay(100);}if(os==HIGH&&s==LOW){changeByte(traits_[p%4],1);changeBond(1);goodSound();popup(F("Quiet reflection"),F("A memory grew"));markSaveDirty();}ol=l;os=s;orr=r;display_.clearDisplay();drawHeader(F("PERSONALITY"),p+1,3);if(p==0){display_.setCursor(3,16);display_.print(F("Curious "));display_.print(traits_[0]);display_.setCursor(3,27);display_.print(F("Social  "));display_.print(traits_[1]);display_.setCursor(3,38);display_.print(F("Brave   "));display_.print(traits_[2]);display_.setCursor(3,49);display_.print(F("Reliable "));display_.print(traits_[3]);}else if(p==1){display_.setCursor(3,16);display_.print(F("Favorite room: "));display_.print(favoriteRoom_);display_.setCursor(3,28);display_.print(F("Weather: "));display_.print(favoriteWeather_);display_.setCursor(3,40);display_.print(F("Best friend: "));display_.print(RESIDENT_NAMES[favoriteNeighbor_]);display_.setCursor(3,52);display_.print(F("Fear type: "));display_.print(fearType_);}else{display_.setCursor(3,16);display_.print(F("Memory types found"));display_.setCursor(3,29);display_.print(F("Mask 0x"));display_.print(importantMemoryMask_);display_.setCursor(3,42);display_.print(F("Routine shapes traits"));display_.setCursor(3,54);display_.print(F("D1 reflect together"));}display_.display();delay(25);}}

void LifeWorldSystem::skillsPage(){uint8_t p=0;bool ol=HIGH,os=HIGH,orr=HIGH;waitButtonRelease();while(true){if(rightHoldExitCheck())return;bool l=digitalRead(btnL_),s=digitalRead(btnS_),r=digitalRead(btnR_);if(ol==HIGH&&l==LOW){p=p? p-1:3;clickSound();delay(100);}if(orr==HIGH&&r==LOW){p=(p+1)%4;clickSound();delay(100);}if(os==HIGH&&s==LOW){workCareer();return;}ol=l;os=s;orr=r;display_.clearDisplay();drawHeader(F("SKILLS"),p+1,4);if(p<2){uint8_t start=p*4;for(uint8_t i=0;i<4;i++){display_.setCursor(2,15+i*11);display_.print(SKILL_NAMES[start+i]);display_.setCursor(104,15+i*11);display_.print(extraSkills_[start+i]);}}else if(p==2){display_.setCursor(2,16);display_.print(F("Smart "));display_.print(skillSmart);display_.setCursor(2,28);display_.print(F("Fitness "));display_.print(skillFit);display_.setCursor(2,40);display_.print(F("Style "));display_.print(skillStyle);display_.setCursor(2,52);display_.print(F("Kindness "));display_.print(skillKind);}else{display_.setCursor(2,16);display_.print(F("Career: "));display_.print(careerId_<LIFE_CAREER_COUNT?careerName(careerId_):F("None"));display_.setCursor(2,29);display_.print(F("Rank "));display_.print(careerRank_);display_.print(F(" XP "));display_.print(careerXp_);display_.setCursor(2,42);display_.print(F("Pay bonus +"));display_.print(careerPayBonus());display_.setCursor(2,55);display_.print(F("D1 choose/work"));}display_.display();delay(25);}}

void LifeWorldSystem::routinePage(){bool ol=HIGH,os=HIGH,orr=HIGH;waitButtonRelease();while(true){if(rightHoldExitCheck())return;bool l=digitalRead(btnL_),s=digitalRead(btnS_),r=digitalRead(btnR_);if(os==HIGH&&s==LOW){routineFlags_=0;routineScore_=50;goodSound();popup(F("Routine refreshed"),F("Today starts clean"));markSaveDirty();}ol=l;os=s;orr=r;display_.clearDisplay();drawHeader(F("DAILY ROUTINE"),1,1);display_.setCursor(2,15);display_.print(F("Now: "));display_.print(locationName(currentLocation_));display_.setCursor(2,27);display_.print(F("Hour "));display_.print(visualHour());display_.print(F("  Score "));display_.print(routineScore_);display_.setCursor(2,39);display_.print(F("7/12/18 Kitchen"));display_.setCursor(2,49);display_.print(F("School/work by age"));display_.setCursor(2,58);display_.print(F("D1 reset today"));display_.display();delay(40);}}

void LifeWorldSystem::residentQuestPage(){uint8_t p=0;bool ol=HIGH,os=HIGH,orr=HIGH;waitButtonRelease();while(true){if(rightHoldExitCheck())return;bool l=digitalRead(btnL_),s=digitalRead(btnS_),r=digitalRead(btnR_);if(ol==HIGH&&l==LOW){p=p? p-1:7;clickSound();delay(100);}if(orr==HIGH&&r==LOW){p=(p+1)%8;clickSound();delay(100);}ol=l;os=s;orr=r;display_.clearDisplay();drawHeader(F("NEIGHBOR STORY"),p+1,8);drawSprite16((p*7+2)%50,6,22);display_.setCursor(30,16);display_.print(RESIDENT_NAMES[p]);display_.setCursor(30,27);display_.print(F("At: "));display_.print(residentLocationName(p,visualHour()));display_.setCursor(30,38);display_.print(F("Chapter "));display_.print(residentQuest_[p]);display_.print(F("/3"));display_.setCursor(30,49);if(residentQuest_[p]==0)display_.print(F("Chat at relation 10"));else if(residentQuest_[p]==1)display_.print(F("Play at relation 30"));else if(residentQuest_[p]==2)display_.print(F("Gift at relation 55"));else display_.print(F("Story complete"));display_.display();delay(30);}}

void LifeWorldSystem::gameTalentPage(){uint8_t g=0;bool ol=HIGH,os=HIGH,orr=HIGH;waitButtonRelease();while(true){if(rightHoldExitCheck())return;bool l=digitalRead(btnL_),s=digitalRead(btnS_),r=digitalRead(btnR_);if(ol==HIGH&&l==LOW){g=g?g-1:12;clickSound();delay(100);}if(orr==HIGH&&r==LOW){g=(g+1)%13;clickSound();delay(100);}ol=l;os=s;orr=r;display_.clearDisplay();drawHeader(F("GAME TALENTS"),g+1,13);drawHubIcon(4,6,18);display_.setCursor(43,18);display_.print(F("Game "));display_.print(g+1);display_.setCursor(43,30);display_.print(F("Assist +"));display_.print(gameAssist(g));display_.setCursor(43,42);display_.print(F("Practice skill "));static const uint8_t m[13]={4,5,7,7,4,6,7,4,7,4,4,0,7};display_.print(extraSkills_[m[g]]);display_.setCursor(2,56);display_.print(F("High skill widens rewards"));display_.display();delay(30);}}

void LifeWorldSystem::decorPage(){uint8_t item=0,slot=0;bool ol=HIGH,os=HIGH,orr=HIGH;waitButtonRelease();while(true){if(rightHoldExitCheck())return;bool l=digitalRead(btnL_),s=digitalRead(btnS_),r=digitalRead(btnR_);if(ol==HIGH&&l==LOW){item=item?item-1:LIFE_DECOR_COUNT-1;clickSound();delay(100);}if(orr==HIGH&&r==LOW){item=(item+1)%LIFE_DECOR_COUNT;clickSound();delay(100);}if(os==HIGH&&s==LOW){waitButtonRelease();if(!(decorOwnedMask_&(1U<<item))){uint8_t cost=2+item/3;if(!spendCoins(cost)){badSound();popup(F("Need more coins"),F("Work or explore"));}else{decorOwnedMask_|=(1U<<item);goodSound();popup(F("Decor purchased"),F("Select again to place"));}}else{decorPlaced_[slot]=item;slot=(slot+1)%3;favoriteRoom_=roomStyle%5;changeByte(extraSkills_[6],2);goodSound();popup(F("Placed safely"),slot==0?F("Next: left floor"):(slot==1?F("Next: right floor"):F("Next: wall")));}markSaveDirty();}ol=l;os=s;orr=r;display_.clearDisplay();drawHeader(F("DECOR"),item+1,LIFE_DECOR_COUNT);display_.fillRect(2,15,34,35,SSD1306_BLACK);display_.drawRoundRect(2,15,34,35,5,SSD1306_WHITE);drawDecorSprite(item,10,25);display_.setCursor(43,19);display_.print(DECOR_NAMES[item]);display_.setCursor(43,31);display_.print((decorOwnedMask_&(1U<<item))?F("Owned"):F("Not owned"));display_.setCursor(43,43);display_.print(F("Place slot "));display_.print(slot+1);display_.setCursor(2,56);display_.print(F("Side slots keep pet clear"));display_.display();delay(25);}}

void LifeWorldSystem::expeditionPage(){uint8_t p=0;bool ol=HIGH,os=HIGH,orr=HIGH;waitButtonRelease();while(true){if(rightHoldExitCheck())return;bool l=digitalRead(btnL_),s=digitalRead(btnS_),r=digitalRead(btnR_);if(ol==HIGH&&l==LOW){p=p?p-1:5;clickSound();delay(100);}if(orr==HIGH&&r==LOW){p=(p+1)%6;clickSound();delay(100);}if(os==HIGH&&s==LOW){waitButtonRelease();if(energy<2){badSound();popup(F("Too tired"),F("Rest first"));return;}uint8_t challenge=20+p*10;uint8_t power=extraSkills_[4]+traits_[2]/2+(companionType_<8?companionLevel_*4:0);subStat(energy,2);expeditionVisitedMask_|=(1U<<p);if(power+random(0,31)>=challenge){changeByte(expeditionMastery_[p],1,10);changeByte(extraSkills_[4],3);changeByte(reputation_[4],3);uint8_t mat=p%6;if(craftMaterials[mat]<250)craftMaterials[mat]++;addCoins(2+p);recordMemory(4,petAgeDays(),p);winSound();popup(F("Expedition success"),F("Material discovered"));}else{changeByte(extraSkills_[4],1);goodSound();popup(F("Trail mapped"),F("Try again stronger"));}markSaveDirty();return;}ol=l;os=s;orr=r;display_.clearDisplay();drawHeader(F("EXPEDITION"),p+1,6);drawHubIcon(6,5,19);display_.setCursor(40,17);display_.print(EXPEDITION_NAMES[p]);display_.setCursor(40,29);display_.print(F("Mastery "));display_.print(expeditionMastery_[p]);display_.setCursor(40,41);display_.print(F("Need explore "));display_.print(20+p*10);display_.setCursor(2,56);display_.print(F("D1 choose path and search"));display_.display();delay(25);}}

void LifeWorldSystem::companionPage(){uint8_t p=companionType_<8?companionType_:0;uint8_t act=0;bool ol=HIGH,os=HIGH,orr=HIGH;waitButtonRelease();while(true){if(rightHoldExitCheck())return;bool l=digitalRead(btnL_),s=digitalRead(btnS_),r=digitalRead(btnR_);if(companionType_>=8){if(ol==HIGH&&l==LOW){p=p?p-1:7;clickSound();delay(100);}if(orr==HIGH&&r==LOW){p=(p+1)%8;clickSound();delay(100);}}else{if(ol==HIGH&&l==LOW){act=act?act-1:2;clickSound();delay(100);}if(orr==HIGH&&r==LOW){act=(act+1)%3;clickSound();delay(100);}}if(os==HIGH&&s==LOW){waitButtonRelease();if(companionType_>=8){if(!spendCoins(8)){badSound();popup(F("Need 8 coins"),F("Adoption fee"));}else{companionType_=p;companionBond_=20;companionEnergy_=8;companionMood_=8;recordMemory(7,petAgeDays(),p);winSound();popup(F("Companion adopted"),F("Lives beside your Tama"));}}else if(act==0){if(!spendCoins(1)){badSound();popup(F("Need 1 coin"),F("Companion snack"));}else{changeByte(companionEnergy_,3,10);changeByte(companionBond_,2);goodSound();popup(F("Snack shared"),COMPANION_NAMES[companionType_]);}}else if(act==1){changeByte(companionMood_,3,10);changeByte(companionBond_,2);companionXp_+=3;goodSound();popup(F("Played together"),COMPANION_NAMES[companionType_]);}else{if(companionEnergy_<2){badSound();popup(F("Companion tired"),F("Feed first"));}else{changeByte(companionEnergy_,-2,10);companionXp_+=5;changeByte(companionBond_,1);if(companionXp_>=(companionLevel_+1)*20&&companionLevel_<9){companionXp_=0;companionLevel_++;winSound();popup(F("Companion leveled"),COMPANION_NAMES[companionType_]);}else goodSound();}}markSaveDirty();}ol=l;os=s;orr=r;display_.clearDisplay();drawHeader(F("COMPANION"),companionType_<8?act+1:p+1,companionType_<8?3:8);drawCompanionSprite(companionType_<8?companionType_:p,9,24,0);display_.setCursor(40,17);display_.print(COMPANION_NAMES[companionType_<8?companionType_:p]);if(companionType_>=8){display_.setCursor(40,30);display_.print(F("Adopt for 8 coins"));display_.setCursor(40,43);display_.print(F("D1 adopt"));}else{display_.setCursor(40,29);display_.print(F("Bond "));display_.print(companionBond_);display_.print(F(" Lv "));display_.print(companionLevel_);display_.setCursor(40,41);display_.print(F("E"));display_.print(companionEnergy_);display_.print(F(" M"));display_.print(companionMood_);display_.setCursor(2,56);display_.print(act==0?F("D1 feed"):act==1?F("D1 play"):F("D1 train"));}display_.display();delay(25);}}

void LifeWorldSystem::familyTreePage(){if(familyCount_==0){popup(F("No ancestors yet"),F("Complete a generation"));return;}uint8_t p=0;bool ol=HIGH,os=HIGH,orr=HIGH;waitButtonRelease();while(true){if(rightHoldExitCheck())return;bool l=digitalRead(btnL_),s=digitalRead(btnS_),r=digitalRead(btnR_);if(ol==HIGH&&l==LOW){p=p?p-1:familyCount_-1;clickSound();delay(100);}if(orr==HIGH&&r==LOW){p=(p+1)%familyCount_;clickSound();delay(100);}ol=l;os=s;orr=r;uint8_t idx=(familyHead_+LIFE_FAMILY_COUNT-familyCount_+p)%LIFE_FAMILY_COUNT;display_.clearDisplay();drawHeader(F("FAMILY LEGACY"),p+1,familyCount_);drawSprite16(familySpecies_[idx]%50,6,23);display_.setCursor(30,16);display_.print(F("Generation "));display_.print(familyGeneration_[idx]);display_.setCursor(30,28);display_.print(F("Career "));display_.print(familyCareer_[idx]<LIFE_CAREER_COUNT?careerName(familyCareer_[idx]):F("None"));display_.setCursor(30,40);display_.print(F("Achievement "));display_.print(familyAchievement_[idx]);display_.setCursor(30,52);display_.print(F("Heirloom "));display_.print(DECOR_NAMES[familyHeirloom_[idx]%LIFE_DECOR_COUNT]);display_.display();delay(25);}}

void LifeWorldSystem::scrapbookPage(){if(scrapCount_==0){popup(F("Scrapbook empty"),F("Live more adventures"));return;}uint8_t p=0;bool ol=HIGH,os=HIGH,orr=HIGH;waitButtonRelease();while(true){if(rightHoldExitCheck())return;bool l=digitalRead(btnL_),s=digitalRead(btnS_),r=digitalRead(btnR_);if(ol==HIGH&&l==LOW){p=p?p-1:scrapCount_-1;clickSound();delay(100);}if(orr==HIGH&&r==LOW){p=(p+1)%scrapCount_;clickSound();delay(100);}ol=l;os=s;orr=r;uint8_t idx=(scrapHead_+LIFE_SCRAP_COUNT-scrapCount_+p)%LIFE_SCRAP_COUNT;display_.clearDisplay();drawHeader(F("SCRAPBOOK"),p+1,scrapCount_);drawHubIcon(scrapType_[idx]%14,5,19);display_.setCursor(40,17);display_.print(F("Day "));display_.print(scrapDay_[idx]);display_.setCursor(40,30);switch(scrapType_[idx]){case 0:display_.print(F("Hatched"));break;case 3:display_.print(F("Friend story"));break;case 4:display_.print(F("Expedition"));break;case 5:display_.print(F("Career day"));break;case 6:display_.print(F("Promotion"));break;case 7:display_.print(F("Adoption"));break;case 8:display_.print(F("Festival win"));break;case 9:display_.print(F("Legacy archived"));break;default:display_.print(F("Special memory"));break;}display_.setCursor(40,43);display_.print(F("Memory #"));display_.print(scrapValue_[idx]);display_.setCursor(2,56);display_.print(F("Stored automatically"));display_.display();delay(25);}}

void LifeWorldSystem::clinicPage(){uint8_t pick=0;bool ol=HIGH,os=HIGH,orr=HIGH;waitButtonRelease();while(true){if(rightHoldExitCheck())return;bool l=digitalRead(btnL_),s=digitalRead(btnS_),r=digitalRead(btnR_);if(ol==HIGH&&l==LOW){pick=pick?pick-1:2;clickSound();delay(100);}if(orr==HIGH&&r==LOW){pick=(pick+1)%3;clickSound();delay(100);}if(os==HIGH&&s==LOW){waitButtonRelease();if(pick==0){changeByte(extraSkills_[3],1);goodSound();popup(F("Diagnosis recorded"),conditionType?F("Symptoms identified"):F("Healthy today"));}else if(pick==1){if(conditionType==0){popup(F("No treatment needed"),F("Healthy"));}else if(!spendCoins(2)){badSound();popup(F("Need 2 coins"),F("Clinic supplies"));}else{conditionSeverity=conditionSeverity>0?conditionSeverity-1:0;conditionTicks=0;if(conditionSeverity==0){conditionType=0;sick=false;}changeByte(extraSkills_[3],3);changeByte(reputation_[0],2);winSound();popup(F("Treatment complete"),F("Rest and recover"));}}else{addStat(energy,2);addStat(health,1);sleeping=false;changeByte(traits_[3],1);goodSound();popup(F("Recovery plan"),F("Energy restored"));}markSaveDirty();return;}ol=l;os=s;orr=r;display_.clearDisplay();drawHeader(F("HEALTH CLINIC"),pick+1,3);drawHubIcon(10,5,19);display_.setCursor(40,17);display_.print(conditionType?F("Condition active"):F("No condition"));display_.setCursor(40,29);display_.print(F("Severity "));display_.print(conditionSeverity);display_.setCursor(40,41);display_.print(pick==0?F("Inspect symptoms"):pick==1?F("Target treatment"):F("Rest plan"));display_.setCursor(2,56);display_.print(F("D1 select"));display_.display();delay(25);}}

void LifeWorldSystem::communityPage(){uint8_t p=0;bool ol=HIGH,os=HIGH,orr=HIGH;waitButtonRelease();while(true){if(rightHoldExitCheck())return;bool l=digitalRead(btnL_),s=digitalRead(btnS_),r=digitalRead(btnR_);if(ol==HIGH&&l==LOW){p=p?p-1:5;clickSound();delay(100);}if(orr==HIGH&&r==LOW){p=(p+1)%6;clickSound();delay(100);}if(os==HIGH&&s==LOW){waitButtonRelease();if(projectLevel_[p]>=3){popup(F("Project complete"),PROJECT_NAMES[p]);}else if(!spendCoins(3)){badSound();popup(F("Need 3 coins"),F("Community fund"));}else{projectProgress_[p]+=25;if(projectProgress_[p]>=100){projectProgress_[p]=0;projectLevel_[p]++;changeByte(reputation_[0],5);changeByte(reputation_[3],2);recordMemory(10,petAgeDays(),p);winSound();popup(F("City upgraded"),PROJECT_NAMES[p]);}else{goodSound();popup(F("Contribution added"),PROJECT_NAMES[p]);}markSaveDirty();}}ol=l;os=s;orr=r;display_.clearDisplay();drawHeader(F("COMMUNITY"),p+1,6);drawHubIcon(11,5,19);display_.setCursor(40,17);display_.print(PROJECT_NAMES[p]);display_.setCursor(40,29);display_.print(F("Level "));display_.print(projectLevel_[p]);display_.print(F("/3"));display_.setCursor(40,41);display_.print(F("Progress "));display_.print(projectProgress_[p]);display_.print(F("%"));display_.setCursor(2,56);display_.print(F("D1 donate 3 coins"));display_.display();delay(25);}}

void LifeWorldSystem::festivalPage(){uint8_t p=0;bool ol=HIGH,os=HIGH,orr=HIGH;waitButtonRelease();while(true){if(rightHoldExitCheck())return;bool l=digitalRead(btnL_),s=digitalRead(btnS_),r=digitalRead(btnR_);if(ol==HIGH&&l==LOW){p=p?p-1:7;clickSound();delay(100);}if(orr==HIGH&&r==LOW){p=(p+1)%8;clickSound();delay(100);}if(os==HIGH&&s==LOW){waitButtonRelease();uint8_t today=petAgeDays()%8;if(p!=today){badSound();popup(F("Festival not today"),F("Return on its event day"));return;}uint8_t skillMap[8]={0,1,2,4,6,7,7,4};uint8_t score=static_cast<uint8_t>(min<int>(99,extraSkills_[skillMap[p]]/2+traits_[p%4]/3+random(0,31)));if(score>festivalBest_[p])festivalBest_[p]=score;if(score>=55){festivalWins_[p]++;festivalTrophyMask_|=(1U<<p);addCoins(8);changeByte(reputation_[3],5);recordMemory(8,petAgeDays(),p);winSound();popup(F("Festival victory!"),F("Trophy and 8 coins"));}else{changeByte(extraSkills_[skillMap[p]],2);goodSound();popup(F("Good performance"),F("Practice for next time"));}markSaveDirty();return;}ol=l;os=s;orr=r;display_.clearDisplay();drawHeader(F("FESTIVAL"),p+1,8);drawHubIcon(12,5,19);display_.setCursor(40,17);display_.print(FESTIVAL_NAMES[p]);display_.setCursor(40,29);display_.print(F("Wins "));display_.print(festivalWins_[p]);display_.setCursor(40,41);display_.print(F("Best "));display_.print(festivalBest_[p]);display_.setCursor(2,53);display_.print(p==petAgeDays()%8?F("TODAY - D1 enter"):F("Scheduled another day"));display_.display();delay(25);}}

void LifeWorldSystem::reputationPage(){uint8_t p=0;bool ol=HIGH,os=HIGH,orr=HIGH;waitButtonRelease();while(true){if(rightHoldExitCheck())return;bool l=digitalRead(btnL_),s=digitalRead(btnS_),r=digitalRead(btnR_);if(ol==HIGH&&l==LOW){p=p?p-1:2;clickSound();delay(100);}if(orr==HIGH&&r==LOW){p=(p+1)%3;clickSound();delay(100);}if(os==HIGH&&s==LOW){waitButtonRelease();if(p==0){changeByte(reputation_[0],3);changeByte(reputation_[2],2);changeByte(traits_[3],2);popup(F("Returned lost item"),F("Helpful + Reliable"));}else if(p==1){changeByte(reputation_[1],4);changeByte(traits_[2],2);if(coins<65000)coins++;popup(F("Played a harmless prank"),F("Playful + Brave"));}else{changeByte(reputation_[4],4);changeByte(traits_[0],2);subStat(energy,1);popup(F("Took the risky trail"),F("Adventurous + Curious"));}goodSound();markSaveDirty();return;}ol=l;os=s;orr=r;display_.clearDisplay();drawHeader(F("REPUTATION"),p+1,3);display_.setCursor(2,14);display_.print(F("Helpful "));display_.print(reputation_[0]);display_.setCursor(66,14);display_.print(F("Play "));display_.print(reputation_[1]);display_.setCursor(2,26);display_.print(F("Reliable "));display_.print(reputation_[2]);display_.setCursor(66,26);display_.print(F("Fame "));display_.print(reputation_[3]);display_.setCursor(2,38);display_.print(F("Adventure "));display_.print(reputation_[4]);display_.setCursor(2,50);display_.print(p==0?F("Choice: return item"):p==1?F("Choice: playful prank"):F("Choice: risky trail"));display_.display();delay(25);}}

void LifeWorldSystem::developerMax(){for(uint8_t &v:traits_)v=100;for(uint8_t &v:extraSkills_)v=100;careerId_=0;careerRank_=9;careerXp_=0;for(uint8_t i=0;i<LIFE_RESIDENT_COUNT;i++)residentQuest_[i]=3;decorOwnedMask_=0x0FFF;expeditionVisitedMask_=0x003F;for(uint8_t &v:expeditionMastery_)v=10;companionType_=0;companionBond_=100;companionEnergy_=10;companionMood_=10;companionLevel_=9;for(uint8_t &v:projectLevel_)v=3;for(uint8_t &v:festivalWins_)v=9;festivalTrophyMask_=0x00FF;for(uint8_t &v:reputation_)v=100;saveNow();}
void LifeWorldSystem::developerReset(){reset();saveNow();}
