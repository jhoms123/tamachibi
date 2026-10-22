#pragma once

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "Esp32Platform.h"

constexpr uint32_t DEEPLIFE_SAVE_MAGIC = 0x444C4631UL; // DLF1
constexpr uint8_t DEEPLIFE_SAVE_VERSION = 2;
constexpr uint8_t DEEP_CHILD_COUNT = 3;
constexpr uint8_t DEEP_EVENT_LOG_COUNT = 12;
constexpr uint8_t DEEP_FISH_COUNT = 12;
constexpr uint8_t DEEP_FOSSIL_COUNT = 8;
constexpr uint8_t DEEP_WILD_COUNT = 16;
constexpr uint8_t DEEP_TITLE_COUNT = 16;
constexpr uint8_t DEEP_LEGACY_SIBLING_COUNT = 2;

struct DeepLifeSave {
  uint32_t magic;
  uint8_t version;
  uint32_t sequence;

  uint8_t emotion;
  uint8_t emotionIntensity;
  uint16_t emotionMinutes;
  uint8_t lastEmotionCause;

  uint8_t rivalSpecies;
  uint8_t rivalPersonality;
  uint8_t rivalBond;
  uint8_t rivalRespect;
  uint8_t rivalRank;
  uint8_t rivalWins;
  uint8_t rivalLosses;
  uint8_t rivalStory;

  uint8_t groupId;
  uint8_t groupLevel;
  uint8_t groupMeetings;
  uint8_t groupQuest;

  uint8_t partnerHarmony;
  uint8_t familyMood;
  uint8_t argumentState;
  uint16_t anniversaryDay;

  uint8_t childCount;
  uint8_t chosenHeir;
  uint8_t childSpecies[DEEP_CHILD_COUNT];
  uint8_t childPersonality[DEEP_CHILD_COUNT];
  uint8_t childTalent[DEEP_CHILD_COUNT];
  uint8_t childBond[DEEP_CHILD_COUNT];
  uint8_t childAge[DEEP_CHILD_COUNT];
  uint8_t siblingBond[DEEP_CHILD_COUNT];
  uint8_t mentorSkill;
  uint8_t mentorProgress;

  uint8_t legacySiblingCount;
  uint8_t legacySiblingSpecies[DEEP_LEGACY_SIBLING_COUNT];
  uint8_t legacySiblingBond[DEEP_LEGACY_SIBLING_COUNT];
  uint8_t grandparentSpecies;
  uint8_t grandparentBond;

  uint8_t pendingEvent;
  uint8_t pendingEventChoice;
  uint16_t pendingEventDay;
  uint32_t storyFlags;
  uint8_t eventLogHead;
  uint8_t eventLogCount;
  uint8_t eventLogType[DEEP_EVENT_LOG_COUNT];
  uint8_t eventLogValue[DEEP_EVENT_LOG_COUNT];
  uint16_t eventLogDay[DEEP_EVENT_LOG_COUNT];

  uint32_t secretHintMask;
  uint16_t secretUnlockMask;
  uint32_t wildSeenMask;
  uint32_t wildFriendMask;

  uint8_t fossilFragments[DEEP_FOSSIL_COUNT];
  uint8_t fossilCompleteMask;
  uint8_t fossilDonatedMask;

  uint16_t fishSeenMask;
  uint8_t fishBest[DEEP_FISH_COUNT];
  uint8_t fishingLevel;
  uint8_t fishingXp;

  uint8_t expeditionSeed;
  uint8_t expeditionWins;
  uint8_t expeditionLosses;
  uint8_t expeditionDepth;

  uint8_t season;
  uint8_t rareWeather;
  uint16_t weatherSeenMask;
  uint16_t seasonalCollectionMask;

  uint32_t achievementsLo;
  uint32_t achievementsHi;
  uint16_t badgeMask;
  uint8_t titleId;
  uint8_t museumLevel;
  uint8_t museumDonations;
  uint8_t newGamePlus;

  uint8_t accessibilityFlags;
  uint8_t tutorialStep;
  uint8_t tutorialDone;
  uint8_t infoReason;

  uint8_t dailyGoal;
  uint8_t dailyProgress;
  uint8_t dailyTarget;
  uint16_t dailyGoalDay;
  uint8_t weeklyGoal;
  uint8_t weeklyProgress;
  uint8_t weeklyTarget;
  uint16_t weeklyGoalWeek;

  uint32_t economyEarned;
  uint32_t economySpent;
  uint16_t economyTransactions;
  uint16_t economyDayEarned;
  uint16_t economyDaySpent;
  uint16_t economyDay;

  uint16_t saveHealthChecks;
  uint8_t migrationCount;
  uint8_t lastSaveHealth;
  uint32_t checksum;
};

class DeepLifeSystem {
public:
  DeepLifeSystem(size_t storageBase, Adafruit_SSD1306 &display,
                 uint8_t buttonLeft, uint8_t buttonSelect, uint8_t buttonRight);

  void reset();
  bool load();
  void saveNow();
  void runHub();

  void startGeneration(uint8_t species, uint16_t generation, uint16_t birthCode,
                       uint8_t temperamentValue);
  void archiveGeneration(uint8_t species, uint16_t generation, uint8_t ageDays);
  void tick(uint32_t day, uint8_t hour, uint8_t stageValue, uint8_t weather);
  void recordAction(uint8_t action, uint32_t day, uint8_t hour, uint8_t weather);
  void recordGameResult(uint8_t gameId, bool won, uint8_t score);
  void recordNeighborInteraction(uint8_t resident, uint8_t interaction, uint8_t relation);
  void recordEconomy(uint16_t amount, bool earned);

  uint8_t applySecretEvolution(uint8_t normalSpecies);
  uint8_t extraGameAssist(uint8_t gameId) const;
  bool reducedMotion() const { return (accessibilityFlags_ & 0x02U) != 0; }
  bool highContrast() const { return (accessibilityFlags_ & 0x01U) != 0; }
  bool confirmations() const { return (accessibilityFlags_ & 0x20U) != 0; }
  uint8_t emotionId() const { return emotion_; }
  uint8_t emotionIntensity() const { return emotionIntensity_; }
  uint8_t rivalBond() const { return rivalBond_; }
  uint8_t rivalRespect() const { return rivalRespect_; }
  uint8_t partnerHarmony() const { return partnerHarmony_; }
  uint8_t familyMood() const { return familyMood_; }
  uint8_t childCount() const { return childCount_; }
  uint8_t lowestChildBond() const;
  uint8_t averageSiblingBond() const;
  void eventParentConversation(int partnerHarmonyDelta, int familyMoodDelta,
                               int rivalBondDelta, int childBondDelta,
                               uint8_t emotion, uint8_t intensity, uint8_t topic, uint16_t day);

  void drawEmotionBadge(int x, int y, uint8_t frame) const;
  void drawFamilyActors(uint8_t room, uint8_t frame) const;
  void drawSeasonOverlay(bool cityScene, uint8_t sceneId, uint8_t frame) const;
  void drawContextPose(int x, int y, uint8_t frame) const;
  void playTransition(uint8_t type) const;

  const __FlashStringHelper *emotionName() const;
  const __FlashStringHelper *titleName() const;
  const __FlashStringHelper *stageBenefit(uint8_t stageValue) const;

  void developerMax();
  void developerReset();

private:
  Esp32EEPROMStorage<DeepLifeSave> storeA_;
  Esp32EEPROMStorage<DeepLifeSave> storeB_;
  Adafruit_SSD1306 &display_;
  uint8_t btnL_, btnS_, btnR_;

  uint32_t sequence_;
  uint8_t emotion_, emotionIntensity_, lastEmotionCause_;
  uint16_t emotionMinutes_;
  uint8_t rivalSpecies_, rivalPersonality_, rivalBond_, rivalRespect_, rivalRank_;
  uint8_t rivalWins_, rivalLosses_, rivalStory_;
  uint8_t groupId_, groupLevel_, groupMeetings_, groupQuest_;
  uint8_t partnerHarmony_, familyMood_, argumentState_;
  uint16_t anniversaryDay_;
  uint8_t childCount_, chosenHeir_;
  uint8_t childSpecies_[DEEP_CHILD_COUNT], childPersonality_[DEEP_CHILD_COUNT];
  uint8_t childTalent_[DEEP_CHILD_COUNT], childBond_[DEEP_CHILD_COUNT];
  uint8_t childAge_[DEEP_CHILD_COUNT], siblingBond_[DEEP_CHILD_COUNT];
  uint8_t mentorSkill_, mentorProgress_;
  uint8_t legacySiblingCount_;
  uint8_t legacySiblingSpecies_[DEEP_LEGACY_SIBLING_COUNT];
  uint8_t legacySiblingBond_[DEEP_LEGACY_SIBLING_COUNT];
  uint8_t grandparentSpecies_, grandparentBond_;
  uint8_t pendingEvent_, pendingEventChoice_;
  uint16_t pendingEventDay_;
  uint32_t storyFlags_;
  uint8_t eventLogHead_, eventLogCount_;
  uint8_t eventLogType_[DEEP_EVENT_LOG_COUNT], eventLogValue_[DEEP_EVENT_LOG_COUNT];
  uint16_t eventLogDay_[DEEP_EVENT_LOG_COUNT];
  uint32_t secretHintMask_, wildSeenMask_, wildFriendMask_;
  uint16_t secretUnlockMask_;
  uint8_t fossilFragments_[DEEP_FOSSIL_COUNT], fossilCompleteMask_, fossilDonatedMask_;
  uint16_t fishSeenMask_;
  uint8_t fishBest_[DEEP_FISH_COUNT], fishingLevel_, fishingXp_;
  uint8_t expeditionSeed_, expeditionWins_, expeditionLosses_, expeditionDepth_;
  uint8_t season_, rareWeather_;
  uint16_t weatherSeenMask_, seasonalCollectionMask_;
  uint32_t achievementsLo_, achievementsHi_;
  uint16_t badgeMask_;
  uint8_t titleId_, museumLevel_, museumDonations_, newGamePlus_;
  uint8_t accessibilityFlags_, tutorialStep_, tutorialDone_, infoReason_;
  uint8_t dailyGoal_, dailyProgress_, dailyTarget_;
  uint16_t dailyGoalDay_;
  uint8_t weeklyGoal_, weeklyProgress_, weeklyTarget_;
  uint16_t weeklyGoalWeek_;
  uint32_t economyEarned_, economySpent_;
  uint16_t economyTransactions_, economyDayEarned_, economyDaySpent_, economyDay_;
  uint16_t saveHealthChecks_;
  uint8_t migrationCount_, lastSaveHealth_;

  uint32_t checksum(const DeepLifeSave &save) const;
  bool valid(const DeepLifeSave &save) const;
  void fillSave(DeepLifeSave &save) const;
  void applySave(const DeepLifeSave &save);
  void changeByte(uint8_t &value, int amount, uint8_t maximum = 100);
  void setEmotion(uint8_t emotion, uint8_t intensity, uint8_t cause);
  void addEventLog(uint8_t type, uint8_t value, uint16_t day);
  void setAchievement(uint8_t achievement);
  bool hasAchievement(uint8_t achievement) const;
  uint8_t achievementCount() const;
  void evaluateAchievements();
  void updateGoals(uint8_t action);
  void configureGoals(uint16_t day);
  void generateDailyEvent(uint16_t day, uint8_t stageValue, uint8_t weather);
  void resolveEvent(uint8_t choice);
  void resolveProceduralEvent(uint8_t event, uint8_t choice, bool &positive);
  const char *eventTitle(uint8_t event) const;
  const char *eventCategoryName(uint8_t event) const;
  const char *eventPrompt(uint8_t event) const;
  const char *eventChoiceLabel(uint8_t event, uint8_t choice) const;
  void printEventContext(uint8_t event) const;
  void updateSeason(uint16_t day, uint8_t weather);
  uint8_t effectiveDifficulty() const;

  void drawHeader(const __FlashStringHelper *title, uint8_t page, uint8_t count) const;
  void drawHubIcon(uint8_t id, int x, int y) const;
  void drawEmotionIcon(uint8_t id, int x, int y, bool filled) const;
  void drawRival(int x, int y, uint8_t frame) const;
  void drawChild(uint8_t id, int x, int y, uint8_t frame) const;
  void drawWild(uint8_t id, int x, int y, uint8_t frame) const;
  void drawFish(uint8_t id, int x, int y, uint8_t frame) const;
  void drawFossil(uint8_t id, int x, int y) const;
  void drawMuseumScene(uint8_t page, uint8_t frame) const;
  void popup(const __FlashStringHelper *line1, const __FlashStringHelper *line2,
             uint16_t holdMs = 850) const;
  void popup(const __FlashStringHelper *line1, const char *line2,
             uint16_t holdMs = 850) const;
  bool confirm(const __FlashStringHelper *title, const __FlashStringHelper *detail) const;

  void todayPage();
  void storyPage();
  void rivalPage();
  void groupsPage();
  void familyPage();
  void mentoringPage();
  void legacyRelativesPage();
  void adventurePage();
  void creaturesPage();
  void fossilsPage();
  void fishingPage();
  void seasonsPage();
  void achievementsPage();
  void museumPage();
  void newGamePlusPage();
  void settingsPage();
  void tutorialPage();
  void economyPage();
  void diagnosticsPage();
  void expeditionMapGame();
  void fishingGame();
};
