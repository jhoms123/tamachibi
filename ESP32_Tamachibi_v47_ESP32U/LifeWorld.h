#pragma once

#include <Arduino.h>
#include "GameDisplay.h"
#include "Esp32Platform.h"

constexpr uint32_t LIFEWORLD_SAVE_MAGIC = 0x4C465731UL; // LFW1
constexpr uint8_t LIFEWORLD_SAVE_VERSION = 1;
constexpr uint8_t LIFE_SKILL_COUNT = 8;
constexpr uint8_t LIFE_CAREER_COUNT = 10;
constexpr uint8_t LIFE_RESIDENT_COUNT = 8;
constexpr uint8_t LIFE_DECOR_COUNT = 12;
constexpr uint8_t LIFE_EXPEDITION_COUNT = 6;
constexpr uint8_t LIFE_PROJECT_COUNT = 6;
constexpr uint8_t LIFE_FESTIVAL_COUNT = 8;
constexpr uint8_t LIFE_FAMILY_COUNT = 8;
constexpr uint8_t LIFE_SCRAP_COUNT = 12;

struct LifeWorldSave {
  uint32_t magic;
  uint8_t version;
  uint32_t sequence;

  uint8_t traits[4];                 // curious, social, brave, responsible
  uint8_t extraSkills[LIFE_SKILL_COUNT];
  uint8_t careerId;
  uint8_t careerRank;
  uint16_t careerXp;

  uint8_t currentLocation;
  uint8_t routineScore;
  uint8_t routineFlags;
  uint32_t routineDay;
  uint8_t lastRoutineHour;

  uint8_t favoriteRoom;
  uint8_t favoriteWeather;
  uint8_t favoriteNeighbor;
  uint8_t fearType;
  uint16_t importantMemoryMask;
  uint8_t roomVisits[5];

  uint8_t residentQuest[LIFE_RESIDENT_COUNT];
  uint8_t residentQuestProgress[LIFE_RESIDENT_COUNT];

  uint16_t decorOwnedMask;
  uint8_t decorPlaced[3];            // left floor, right floor, wall

  uint16_t expeditionVisitedMask;
  uint8_t expeditionMastery[LIFE_EXPEDITION_COUNT];

  uint8_t companionType;             // 255 = none
  uint8_t companionBond;
  uint8_t companionEnergy;
  uint8_t companionMood;
  uint8_t companionLevel;
  uint16_t companionXp;

  uint8_t familyCount;
  uint8_t familyHead;
  uint8_t familySpecies[LIFE_FAMILY_COUNT];
  uint8_t familyCareer[LIFE_FAMILY_COUNT];
  uint8_t familyPersonality[LIFE_FAMILY_COUNT];
  uint8_t familyAchievement[LIFE_FAMILY_COUNT];
  uint8_t familyHeirloom[LIFE_FAMILY_COUNT];
  uint16_t familyGeneration[LIFE_FAMILY_COUNT];

  uint8_t scrapCount;
  uint8_t scrapHead;
  uint8_t scrapType[LIFE_SCRAP_COUNT];
  uint16_t scrapDay[LIFE_SCRAP_COUNT];
  uint8_t scrapValue[LIFE_SCRAP_COUNT];

  uint8_t projectLevel[LIFE_PROJECT_COUNT];
  uint8_t projectProgress[LIFE_PROJECT_COUNT];
  uint8_t festivalWins[LIFE_FESTIVAL_COUNT];
  uint8_t festivalBest[LIFE_FESTIVAL_COUNT];
  uint16_t festivalTrophyMask;

  uint8_t reputation[5];             // helpful, playful, reliable, famous, adventurous
  uint32_t lastDailyUpdate;
  uint32_t checksum;
};

class LifeWorldSystem {
public:
  LifeWorldSystem(size_t storageBase, GameDisplay &display,
                  uint8_t buttonLeft, uint8_t buttonSelect, uint8_t buttonRight);

  void reset();
  bool load();
  void saveNow();
  void runHub();

  void startGeneration(uint8_t species, uint16_t generation, uint16_t birthCode,
                       uint8_t temperament, uint8_t favoriteFood);
  void archiveGeneration(uint8_t species, uint16_t generation, uint8_t temperament,
                         uint8_t favoriteFood, uint8_t ageDays);
  void recordAction(uint8_t action, uint32_t day, uint8_t hour, uint8_t weather);
  void recordPlantSale(uint8_t species);
  void recordFloristOrder(uint8_t species, uint32_t day);
  void recordGameResult(uint8_t gameId, bool won);
  void recordNeighborInteraction(uint8_t resident, uint8_t interaction, uint8_t relation);
  uint8_t neighborStoryChapter(uint8_t resident) const;
  uint8_t neighborInteractionCount(uint8_t resident) const;
  bool neighborInvitePending(uint8_t resident) const;
  void setNeighborInvitePending(uint8_t resident, bool pending);
  void recordMemory(uint8_t type, uint32_t day, uint8_t value = 0);
  void tick(uint32_t day, uint8_t hour, uint8_t stage, uint8_t weather, bool weekday);

  uint8_t gameAssist(uint8_t gameId) const;
  uint8_t currentLocation() const { return currentLocation_; }
  uint8_t careerId() const { return careerId_; }
  uint8_t careerRank() const { return careerRank_; }
  uint8_t companionType() const { return companionType_; }
  const char *companionName() const;
  uint8_t companionBond() const { return companionBond_; }
  uint8_t companionMood() const { return companionMood_; }
  uint8_t companionEnergy() const { return companionEnergy_; }
  uint8_t companionLevel() const { return companionLevel_; }
  uint8_t trait(uint8_t id) const { return traits_[id % 4]; }
  uint8_t reputation(uint8_t id) const { return reputation_[id % 5]; }
  uint8_t projectLevel(uint8_t id) const { return projectLevel_[id % LIFE_PROJECT_COUNT]; }
  void eventAdjustCompanion(int bond, int mood, int energy, uint16_t xp);
  void eventCareerProgress(uint16_t xp, uint8_t reputationKind, int reputationDelta);
  void eventCommunityProgress(uint8_t project, uint8_t progress, uint8_t reputationKind, int reputationDelta);
  bool residentAvailable(uint8_t resident, uint8_t hour) const;
  const __FlashStringHelper *residentLocationName(uint8_t resident, uint8_t hour) const;
  const __FlashStringHelper *locationName(uint8_t location) const;
  const __FlashStringHelper *careerName(uint8_t career) const;
  uint8_t careerPayBonus() const;
  void workCareer();

  bool hasCompanion() const { return companionType_ < 8; }
  void drawCompanion(int x, int y, uint8_t frame) const;
  void drawHomeDecor(uint8_t room, uint8_t frame) const;
  void drawCommunityUpgrades(uint8_t cityPlace, uint8_t frame) const;
  const __FlashStringHelper *personalityThought() const;
  void drawLifeIcon(int x, int y, bool inverted) const;

  void developerMax();
  void developerReset();

private:
  Esp32EEPROMStorage<LifeWorldSave> storeA_;
  Esp32EEPROMStorage<LifeWorldSave> storeB_;
  GameDisplay &display_;
  uint8_t btnL_, btnS_, btnR_;

  uint32_t sequence_;
  uint8_t traits_[4];
  uint8_t extraSkills_[LIFE_SKILL_COUNT];
  uint8_t careerId_, careerRank_;
  uint16_t careerXp_;
  uint8_t currentLocation_, routineScore_, routineFlags_, lastRoutineHour_;
  uint32_t routineDay_;
  uint8_t favoriteRoom_, favoriteWeather_, favoriteNeighbor_, fearType_;
  uint16_t importantMemoryMask_;
  uint8_t roomVisits_[5];
  uint8_t residentQuest_[LIFE_RESIDENT_COUNT];
  uint8_t residentQuestProgress_[LIFE_RESIDENT_COUNT];
  uint16_t decorOwnedMask_;
  uint8_t decorPlaced_[3];
  uint16_t expeditionVisitedMask_;
  uint8_t expeditionMastery_[LIFE_EXPEDITION_COUNT];
  uint8_t companionType_, companionBond_, companionEnergy_, companionMood_, companionLevel_;
  uint16_t companionXp_;
  uint8_t familyCount_, familyHead_;
  uint8_t familySpecies_[LIFE_FAMILY_COUNT], familyCareer_[LIFE_FAMILY_COUNT];
  uint8_t familyPersonality_[LIFE_FAMILY_COUNT], familyAchievement_[LIFE_FAMILY_COUNT];
  uint8_t familyHeirloom_[LIFE_FAMILY_COUNT];
  uint16_t familyGeneration_[LIFE_FAMILY_COUNT];
  uint8_t scrapCount_, scrapHead_;
  uint8_t scrapType_[LIFE_SCRAP_COUNT], scrapValue_[LIFE_SCRAP_COUNT];
  uint16_t scrapDay_[LIFE_SCRAP_COUNT];
  uint8_t projectLevel_[LIFE_PROJECT_COUNT], projectProgress_[LIFE_PROJECT_COUNT];
  uint8_t festivalWins_[LIFE_FESTIVAL_COUNT], festivalBest_[LIFE_FESTIVAL_COUNT];
  uint16_t festivalTrophyMask_;
  uint8_t reputation_[5];
  uint32_t lastDailyUpdate_;

  uint32_t checksum(const LifeWorldSave &save) const;
  bool valid(const LifeWorldSave &save) const;
  void fillSave(LifeWorldSave &save) const;
  void applySave(const LifeWorldSave &save);
  void changeByte(uint8_t &value, int amount, uint8_t maximum = 100);
  uint8_t skillValue(uint8_t skill) const;
  uint8_t dominantAdvancedSkill() const;
  uint8_t calculateLocation(uint8_t stage, uint8_t hour, bool weekday) const;
  void addCareerXp(uint16_t amount);
  void awardQuest(uint8_t resident, uint8_t stage);

  void drawHeader(const __FlashStringHelper *title, uint8_t page, uint8_t count) const;
  void drawHubIcon(uint8_t id, int x, int y) const;
  void drawDecorSprite(uint8_t id, int x, int y) const;
  void drawCompanionSprite(uint8_t id, int x, int y, uint8_t frame) const;
  void popup(const __FlashStringHelper *line1, const __FlashStringHelper *line2,
             uint16_t holdMs = 850) const;
  void popup(const __FlashStringHelper *line1, const char *line2,
             uint16_t holdMs = 850) const;

  void personalityPage();
  void skillsPage();
  void routinePage();
  void residentQuestPage();
  void gameTalentPage();
  void decorPage();
  void expeditionPage();
  void companionPage();
  void familyTreePage();
  void scrapbookPage();
  void clinicPage();
  void communityPage();
  void festivalPage();
  void reputationPage();
};
