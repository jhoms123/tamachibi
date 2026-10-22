#pragma once

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include "Esp32Platform.h"

constexpr uint32_t GROWING_TOGETHER_SAVE_MAGIC = 0x47545731UL; // GTW1
constexpr uint8_t GROWING_TOGETHER_SAVE_VERSION = 1;
constexpr uint8_t GT_FEATURE_COUNT = 27;
constexpr uint8_t GT_SCENARIOS_PER_FEATURE = 12;
constexpr uint8_t GT_LEADS_PER_FEATURE = 5;
constexpr uint8_t GT_ENDINGS_PER_FEATURE = 5;
constexpr uint16_t GT_PROMPTS_PER_FEATURE =
    GT_SCENARIOS_PER_FEATURE * GT_LEADS_PER_FEATURE * GT_ENDINGS_PER_FEATURE; // 300
constexpr uint8_t GT_PROMISE_SLOTS = 4;
constexpr uint8_t GT_VALUE_COUNT = 12;
constexpr uint8_t GT_RITUAL_COUNT = 8;
constexpr uint8_t GT_KEEP_COUNT = 12;
constexpr uint8_t GT_STYLE_COUNT = 8;
constexpr uint8_t GT_HOBBY_COUNT = 10;
constexpr uint8_t GT_TRADITION_COUNT = 8;
constexpr uint8_t GT_MEMORY_COUNT = 8;

// Feature families. Each narrative feature has 12 authored scenario roots,
// five openings and five endings, producing exactly 300 base prompts before
// live context and follow-up state are appended.
enum GrowingFeature : uint8_t {
  GT_MULTI_DAY_ARC = 0,
  GT_FAMILY_VALUES,
  GT_RUPTURE_REPAIR,
  GT_PROMISES,
  GT_BEDTIME_RITUALS,
  GT_AFFECTION_CONSENT,
  GT_DRAWINGS_LETTERS,
  GT_KEEPSAKE_BOX,
  GT_LANGUAGE_GROWTH,
  GT_PARENT_STYLE,
  GT_SCHOOL_REPORTS,
  GT_CHORES,
  GT_FAMILY_MEETINGS,
  GT_COPARENTING,
  GT_SECRETS_PRIVACY,
  GT_MEANINGFUL_APOLOGY,
  GT_SHARED_HOBBIES,
  GT_MILESTONES,
  GT_GRIEF_REMEMBRANCE,
  GT_INTERGENERATIONAL,
  GT_FALSE_BELIEFS,
  GT_TEMPERAMENT,
  GT_RELATIONSHIP_MEMORY,
  GT_FAMILY_TRADITIONS,
  GT_HOUSEHOLD_RULES,
  GT_UNSOLICITED_BEHAVIOR,
  GT_JUST_LISTEN
};

enum GrowingAction : uint8_t {
  GTA_TALK = 0,
  GTA_CARE,
  GTA_PLAY,
  GTA_BED,
  GTA_GARDEN,
  GTA_COMPANION,
  GTA_SCHOOL,
  GTA_WORK,
  GTA_EXPLORE,
  GTA_COOK,
  GTA_CLEAN,
  GTA_FAMILY,
  GTA_APOLOGY,
  GTA_HOBBY
};

struct GrowingTogetherContext {
  uint32_t ageTicks;
  uint16_t ageDays;
  uint16_t generation;
  uint8_t stage;
  uint8_t hour;
  uint8_t weather;
  uint8_t temperament;
  uint8_t species;
  uint8_t location;

  uint8_t health;
  uint8_t full;
  uint8_t energy;
  uint8_t happy;
  uint8_t clean;
  uint8_t discipline;
  uint8_t sick;
  uint8_t ownerBond;
  uint8_t ownerTrust;
  uint8_t mischief;

  uint8_t emotion;
  uint8_t emotionIntensity;
  uint8_t hasPartner;
  uint8_t partnerHarmony;
  uint8_t familyMood;
  uint8_t childCount;
  uint8_t lowestChildBond;
  uint8_t siblingBond;
  uint8_t rivalBond;
  uint8_t rivalRespect;

  uint8_t careerId;
  uint8_t careerRank;
  uint8_t companionType;
  uint8_t companionBond;
  uint8_t companionMood;
  uint8_t companionEnergy;

  uint8_t gardenCount;
  uint8_t gardenMature;
  uint8_t gardenStage;
  uint8_t gardenHealth;
  uint8_t gardenWater;
  uint8_t indoorCount;
  uint8_t indoorHealth;

  uint8_t openness;
  uint8_t confidence;
  uint8_t empathy;
  uint8_t responsibility;
  uint8_t resilience;
  uint8_t honesty;
  uint8_t independence;
  uint8_t schoolConfidence;
  uint8_t socialConfidence;
  uint8_t attachmentSecurity;
};

struct GrowingTogetherOutcome {
  bool resolved;
  bool postponed;
  uint8_t feature;
  uint8_t choice;
  int8_t bond;
  int8_t trust;
  int8_t happy;
  int8_t discipline;
  int8_t kindness;
  int8_t smart;
  int8_t mischief;
  int8_t partnerHarmony;
  int8_t familyMood;
  int8_t childBond;
  int8_t rivalBond;
  int8_t companionBond;
  int8_t companionMood;
  int8_t companionEnergy;
  int8_t gardenWater;
  int8_t gardenCare;
  int8_t gardenDamage;
  int8_t parentOpenness;
  int8_t parentConfidence;
  int8_t parentEmpathy;
  int8_t parentResponsibility;
  int8_t parentResilience;
  int8_t parentHonesty;
  int8_t parentIndependence;
  int8_t parentEmotionWords;
  int8_t parentSchoolConfidence;
  int8_t parentSocialConfidence;
  int8_t parentSafetyConfidence;
  int8_t parentAttachment;
  uint8_t emotion;
  uint8_t emotionIntensity;
};

#pragma pack(push, 1)
struct GrowingTogetherSave {
  uint32_t magic;
  uint8_t version;
  uint32_t sequence;

  uint32_t nextPromptTick;
  uint32_t lastPromptTick;
  uint8_t pendingFeature;
  uint8_t pendingScenario;
  uint8_t pendingLead;
  uint8_t pendingEnding;
  uint8_t pendingFlags;
  uint16_t dailyDay;
  uint8_t dailyCount;

  uint8_t activeArcType;
  uint8_t activeArcStep;
  uint8_t activeArcBranch;
  uint16_t activeArcDueDay;
  uint16_t activeArcFlags;

  uint8_t promiseType[GT_PROMISE_SLOTS];
  uint16_t promiseDueDay[GT_PROMISE_SLOTS];
  uint8_t promiseStatus[GT_PROMISE_SLOTS];

  uint8_t values[GT_VALUE_COUNT];
  uint8_t valueConsistency[GT_VALUE_COUNT];
  uint8_t relationshipState;
  uint8_t ruptureScore;
  uint8_t repairProgress;

  uint8_t ritualStreak[GT_RITUAL_COUNT];
  uint16_t affectionPreferenceMask;
  uint8_t consentRespect;
  uint8_t affectionBond;

  uint32_t keepsakeMask;
  uint8_t keepsakeCount[GT_KEEP_COUNT];
  uint8_t languageLevel;
  uint8_t vocabularyScore;
  uint8_t speechPattern;
  uint8_t communicationStyleCount[GT_STYLE_COUNT];

  uint8_t schoolEffort;
  uint8_t schoolCooperation;
  uint8_t schoolConfidence;
  uint16_t schoolLastReportDay;

  uint16_t choreAssignedMask;
  uint16_t choreDoneMask;
  uint16_t choreMissedMask;
  uint8_t choreFairness;
  uint16_t familyMeetingDay;
  uint8_t familyDecision;

  uint8_t coparentStyle;
  uint8_t coparentAgreement;
  uint8_t coparentDisagreement;
  uint8_t secretId;
  uint8_t privacyTrust;
  uint8_t secretSafety;
  uint8_t apologySkill;
  uint8_t repairSkill;

  uint8_t sharedHobby;
  uint8_t hobbyBond[GT_HOBBY_COUNT];
  uint32_t milestoneMask;
  uint8_t griefType;
  uint8_t griefIntensity;
  uint8_t griefStage;
  uint8_t intergenerationalPattern[8];
  uint8_t falseBeliefId;
  uint8_t falseBeliefIntensity;
  int8_t relationshipMemory[GT_MEMORY_COUNT];

  uint16_t traditionMask;
  uint8_t traditionStreak[GT_TRADITION_COUNT];
  uint16_t householdRulesMask;
  uint8_t ruleConsistency;
  uint8_t unsolicitedAction;
  uint16_t unsolicitedDay;
  uint8_t justListenCount;
  uint8_t justListenRespected;

  uint8_t reflectionCount[GT_STYLE_COUNT];
  uint16_t featureTalkCount[GT_FEATURE_COUNT];
  uint16_t generationConversations;
  uint32_t lifetimeConversations;
  uint8_t lastFeature;
  uint8_t lastWeight;
  uint32_t lastRejectedMask;
  uint32_t checksum;
};
#pragma pack(pop)

class GrowingTogetherSystem {
public:
  GrowingTogetherSystem(size_t storageBase, Adafruit_SSD1306 &display,
                        uint8_t buttonLeft, uint8_t buttonSelect, uint8_t buttonRight);

  void reset();
  bool load();
  void saveNow();
  void startGeneration(const GrowingTogetherContext &ctx);
  void tick(const GrowingTogetherContext &ctx, bool catchUpMode);
  void recordAction(uint8_t action, const GrowingTogetherContext &ctx);
  // Carries a real family loss into the next generation's grief, keepsake,
  // remembrance, and intergenerational story systems.
  void recordLoss(uint8_t lossType, const GrowingTogetherContext &ctx);

  bool hasPending() const { return pendingFeature_ < GT_FEATURE_COUNT; }
  bool shouldAutoOpen(const GrowingTogetherContext &ctx, unsigned long idleMs) const;
  void scheduleManual(const GrowingTogetherContext &ctx);
  GrowingTogetherOutcome runPending(const GrowingTogetherContext &ctx);
  void runHub(const GrowingTogetherContext &ctx);
  // Reusable persistent text viewer for older dialogue paths. It word-wraps
  // every message, supports D0/D2 scrolling, and never closes until D1.
  void showReadableMessage(const char *title, const char *text,
                           const char *footer = "D1 close") const;

  uint8_t relationshipState() const { return relationshipState_; }
  uint8_t languageLevel() const { return languageLevel_; }
  uint8_t promiseCount() const;
  uint32_t promptDatabaseSize() const {
    return static_cast<uint32_t>(GT_FEATURE_COUNT) * GT_PROMPTS_PER_FEATURE;
  }
  uint16_t promptsPerFeature() const { return GT_PROMPTS_PER_FEATURE; }
  uint32_t lifetimeConversations() const { return lifetimeConversations_; }

  void developerMax();
  void developerReset();

private:
  Esp32EEPROMStorage<GrowingTogetherSave> storeA_;
  Esp32EEPROMStorage<GrowingTogetherSave> storeB_;
  Adafruit_SSD1306 &display_;
  uint8_t btnL_, btnS_, btnR_;

  uint32_t sequence_, nextPromptTick_, lastPromptTick_;
  uint8_t pendingFeature_, pendingScenario_, pendingLead_, pendingEnding_, pendingFlags_;
  uint16_t dailyDay_;
  uint8_t dailyCount_;

  uint8_t activeArcType_, activeArcStep_, activeArcBranch_;
  uint16_t activeArcDueDay_, activeArcFlags_;
  uint8_t promiseType_[GT_PROMISE_SLOTS], promiseStatus_[GT_PROMISE_SLOTS];
  uint16_t promiseDueDay_[GT_PROMISE_SLOTS];
  uint8_t values_[GT_VALUE_COUNT], valueConsistency_[GT_VALUE_COUNT];
  uint8_t relationshipState_, ruptureScore_, repairProgress_;
  uint8_t ritualStreak_[GT_RITUAL_COUNT];
  uint16_t affectionPreferenceMask_;
  uint8_t consentRespect_, affectionBond_;
  uint32_t keepsakeMask_;
  uint8_t keepsakeCount_[GT_KEEP_COUNT];
  uint8_t languageLevel_, vocabularyScore_, speechPattern_;
  uint8_t communicationStyleCount_[GT_STYLE_COUNT];
  uint8_t schoolEffort_, schoolCooperation_, schoolConfidence_;
  uint16_t schoolLastReportDay_;
  uint16_t choreAssignedMask_, choreDoneMask_, choreMissedMask_;
  uint8_t choreFairness_;
  uint16_t familyMeetingDay_;
  uint8_t familyDecision_;
  uint8_t coparentStyle_, coparentAgreement_, coparentDisagreement_;
  uint8_t secretId_, privacyTrust_, secretSafety_, apologySkill_, repairSkill_;
  uint8_t sharedHobby_, hobbyBond_[GT_HOBBY_COUNT];
  uint32_t milestoneMask_;
  uint8_t griefType_, griefIntensity_, griefStage_;
  uint8_t intergenerationalPattern_[8];
  uint8_t falseBeliefId_, falseBeliefIntensity_;
  int8_t relationshipMemory_[GT_MEMORY_COUNT];
  uint16_t traditionMask_, householdRulesMask_;
  uint8_t traditionStreak_[GT_TRADITION_COUNT], ruleConsistency_;
  uint8_t unsolicitedAction_;
  uint16_t unsolicitedDay_;
  uint8_t justListenCount_, justListenRespected_;
  uint8_t reflectionCount_[GT_STYLE_COUNT];
  uint16_t featureTalkCount_[GT_FEATURE_COUNT];
  uint16_t generationConversations_;
  uint32_t lifetimeConversations_;
  uint8_t lastFeature_, lastWeight_;
  uint32_t lastRejectedMask_;

  uint32_t checksum(const GrowingTogetherSave &save) const;
  bool valid(const GrowingTogetherSave &save) const;
  void fillSave(GrowingTogetherSave &save) const;
  void applySave(const GrowingTogetherSave &save);
  void changeByte(uint8_t &value, int delta, uint8_t maximum = 100);
  void scheduleNext(const GrowingTogetherContext &ctx, bool soon = false);
  bool featureAllowedForStage(uint8_t feature, const GrowingTogetherContext &ctx) const;
  bool scenarioAllowedForStage(uint8_t feature, uint8_t scenario,
                               const GrowingTogetherContext &ctx) const;
  uint8_t selectScenarioForFeature(uint8_t feature, const GrowingTogetherContext &ctx) const;
  const char *stageLead(const GrowingTogetherContext &ctx, uint8_t lead) const;
  const char *featureQuestion(uint8_t feature) const;
  void selectFeature(const GrowingTogetherContext &ctx, bool manual);
  uint8_t featureWeight(uint8_t feature, const GrowingTogetherContext &ctx) const;
  void updateDerivedState(const GrowingTogetherContext &ctx);
  void evaluatePromises(const GrowingTogetherContext &ctx);
  void evaluateMilestones(const GrowingTogetherContext &ctx);
  void advanceArc(const GrowingTogetherContext &ctx, uint8_t feature, uint8_t choice);
  void applyFeatureChoice(const GrowingTogetherContext &ctx, uint8_t feature,
                          uint8_t choice, GrowingTogetherOutcome &outcome);

  const char *featureName(uint8_t feature) const;
  const char *scenarioLine(uint8_t feature, uint8_t scenario) const;
  const char *scenarioLineForContext(uint8_t feature, uint8_t scenario,
                                     const GrowingTogetherContext &ctx) const;
  const char *choiceLabel(uint8_t feature, uint8_t choice) const;
  const char *resultLine(uint8_t feature, uint8_t choice) const;
  void buildPrompt(const GrowingTogetherContext &ctx, char *out, size_t outSize) const;
  void appendContextLine(const GrowingTogetherContext &ctx, uint8_t feature,
                         char *out, size_t outSize) const;

  uint8_t wrapText(const char *text, char lines[][22], uint8_t maxLines) const;
  void showScrollableText(const char *title, const char *text, const char *footer,
                          bool allowBack = false) const;
  uint8_t chooseResponse(const GrowingTogetherContext &ctx, uint8_t feature) const;
  void drawHubPage(const GrowingTogetherContext &ctx, uint8_t page, uint8_t pages) const;
  void showReflection(const GrowingTogetherContext &ctx) const;
  void showDebug(const GrowingTogetherContext &ctx) const;
};
