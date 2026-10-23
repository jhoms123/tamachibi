#pragma once

#include <Arduino.h>
#include "GameDisplay.h"
#include "Esp32Platform.h"
#include "ConversationMemory.h"

constexpr uint32_t PARENT_DIALOGUE_SAVE_MAGIC = 0x50444231UL; // PDB1
constexpr uint8_t PARENT_DIALOGUE_SAVE_VERSION = 1;
constexpr uint8_t PARENT_DIALOGUE_RECENT_COUNT = 8;
constexpr uint8_t PARENT_DIALOGUE_STAGE_COUNT = 5;
constexpr uint8_t PARENT_DIALOGUE_STYLE_COUNT = 8;
constexpr uint8_t PARENT_DIALOGUE_TOPIC_COUNT = 65;

struct ParentDialogueContext {
  uint32_t ageTicks;
  uint16_t ageDays;
  uint8_t stage;
  uint8_t hour;
  uint8_t weather;
  uint8_t temperament;
  uint8_t species;

  uint8_t health;
  uint8_t full;
  uint8_t energy;
  uint8_t happy;
  uint8_t clean;
  uint8_t discipline;
  uint8_t poop;
  uint8_t sick;
  uint8_t ownerBond;
  uint8_t ownerTrust;
  uint16_t careMistakes;
  uint8_t mischief;

  uint8_t smart;
  uint8_t style;
  uint8_t kindness;
  uint8_t fitness;
  uint8_t favoriteGame;
  uint8_t favoriteFood;

  uint8_t location;
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

  uint8_t emotion;
  uint8_t emotionIntensity;
  uint8_t rivalBond;
  uint8_t rivalRespect;
  uint8_t partnerHarmony;
  uint8_t familyMood;
  uint8_t childCount;
  uint8_t lowestChildBond;
  uint8_t siblingBond;
  uint8_t pendingCallType;

  TamachibiMemoryContext memory;
};

struct ParentDialogueOutcome {
  bool resolved;
  bool dismissed;
  uint8_t topic;
  uint8_t responseStyle;
  int8_t bond;
  int8_t trust;
  int8_t happy;
  int8_t discipline;
  int8_t smart;
  int8_t kindness;
  int8_t mischief;
  int8_t companionBond;
  int8_t companionMood;
  int8_t companionEnergy;
  int8_t gardenWater;
  int8_t gardenCare;
  int8_t familyMood;
  int8_t partnerHarmony;
  int8_t rivalBond;
  uint8_t emotion;
  uint8_t emotionIntensity;
};

struct ParentDialogueSave {
  uint32_t magic;
  uint8_t version;
  uint32_t sequence;

  uint32_t nextPromptTick;
  uint32_t lastPromptTick;
  uint16_t pendingSeed;
  uint8_t pendingTopic;
  uint8_t pendingVariant;
  uint8_t promptMode;                 // 0 quiet, 1 normal, 2 chatty
  uint8_t dailyCount;
  uint16_t dailyDay;
  uint8_t dismissedCount;

  uint16_t generationConversations;
  uint32_t lifetimeConversations;
  uint16_t stageConversations[PARENT_DIALOGUE_STAGE_COUNT];
  uint16_t lastDialogueCode;
  uint8_t recentTopics[PARENT_DIALOGUE_RECENT_COUNT];
  uint8_t recentHead;

  uint8_t openness;
  uint8_t confidence;
  uint8_t empathy;
  uint8_t responsibility;
  uint8_t resilience;
  uint8_t honesty;
  uint8_t independence;
  uint8_t emotionWords;
  uint8_t schoolConfidence;
  uint8_t socialConfidence;
  uint8_t safetyConfidence;
  uint8_t attachmentSecurity;
  uint8_t responseStyleCount[PARENT_DIALOGUE_STYLE_COUNT];

  uint16_t schoolTalks;
  uint16_t familyTalks;
  uint16_t petTalks;
  uint16_t gardenTalks;
  uint16_t difficultTalks;
  uint32_t checksum;
};

class ParentDialogueSystem {
public:
  ParentDialogueSystem(size_t storageBase, GameDisplay &display,
                       uint8_t buttonLeft, uint8_t buttonSelect, uint8_t buttonRight);

  void reset();
  bool load();
  void saveNow();
  void startGeneration(uint16_t generation, uint32_t ageTicks);
  void tick(const ParentDialogueContext &ctx, bool catchUpMode);

  bool hasPending() const { return pendingTopic_ < PARENT_DIALOGUE_TOPIC_COUNT; }
  bool shouldAutoOpen(const ParentDialogueContext &ctx, unsigned long idleMs) const;
  void scheduleManual(const ParentDialogueContext &ctx);
  ParentDialogueOutcome runPending(const ParentDialogueContext &ctx);
  void runJournal(const ParentDialogueContext &ctx);
  void recordSchoolResult(bool successful);
  void recordSocialResult(bool successful);
  void recordDirectTalk(uint8_t stage);
  void eventAdjustDevelopment(int openness, int confidence, int empathy,
                              int responsibility, int resilience, int honesty,
                              int independence, int emotionWords,
                              int schoolConfidence, int socialConfidence,
                              int safetyConfidence, int attachmentSecurity);

  uint8_t openness() const { return openness_; }
  uint8_t confidence() const { return confidence_; }
  uint8_t empathy() const { return empathy_; }
  uint8_t responsibility() const { return responsibility_; }
  uint8_t honesty() const { return honesty_; }
  uint8_t independence() const { return independence_; }
  uint8_t emotionWords() const { return emotionWords_; }
  uint8_t resilience() const { return resilience_; }
  uint8_t schoolConfidence() const { return schoolConfidence_; }
  uint8_t socialConfidence() const { return socialConfidence_; }
  uint8_t attachmentSecurity() const { return attachmentSecurity_; }
  uint32_t lifetimeConversations() const { return lifetimeConversations_; }
  uint32_t estimatedLinePool() const {
    return static_cast<uint32_t>(PARENT_DIALOGUE_TOPIC_COUNT) * 4UL * 8UL * 3UL;
  }

  void developerMax();
  void developerReset();

private:
  Esp32EEPROMStorage<ParentDialogueSave> storeA_;
  Esp32EEPROMStorage<ParentDialogueSave> storeB_;
  GameDisplay &display_;
  uint8_t btnL_, btnS_, btnR_;

  uint32_t sequence_;
  uint32_t nextPromptTick_, lastPromptTick_;
  uint16_t pendingSeed_;
  uint8_t pendingTopic_, pendingVariant_, promptMode_, dailyCount_, dismissedCount_;
  uint16_t dailyDay_;
  uint16_t generationConversations_, stageConversations_[PARENT_DIALOGUE_STAGE_COUNT];
  uint32_t lifetimeConversations_;
  uint16_t lastDialogueCode_;
  uint8_t recentTopics_[PARENT_DIALOGUE_RECENT_COUNT], recentHead_;

  uint8_t openness_, confidence_, empathy_, responsibility_, resilience_, honesty_;
  uint8_t independence_, emotionWords_, schoolConfidence_, socialConfidence_;
  uint8_t safetyConfidence_, attachmentSecurity_;
  uint8_t responseStyleCount_[PARENT_DIALOGUE_STYLE_COUNT];
  uint16_t schoolTalks_, familyTalks_, petTalks_, gardenTalks_, difficultTalks_;

  uint32_t checksum(const ParentDialogueSave &save) const;
  bool valid(const ParentDialogueSave &save) const;
  void fillSave(ParentDialogueSave &save) const;
  void applySave(const ParentDialogueSave &save);
  void changeByte(uint8_t &value, int amount, uint8_t maximum = 100);
  void scheduleNext(const ParentDialogueContext &ctx, bool afterDismissal = false);
  void selectTopic(const ParentDialogueContext &ctx, bool manual);
  bool recentTopic(uint8_t topic) const;
  uint8_t responseSetForTopic(uint8_t topic) const;
  uint8_t topicCategory(uint8_t topic) const;

  void buildPrompt(const ParentDialogueContext &ctx, char *page1, size_t page1Size,
                   char *page2, size_t page2Size) const;
  void buildBabyPrompt(const ParentDialogueContext &ctx, char *page1, size_t page1Size,
                       char *page2, size_t page2Size) const;
  const char *topicLine(uint8_t topic, uint8_t variant) const;
  const char *topicQuestion(uint8_t topic, uint8_t variant) const;
  const char *choiceLabel(uint8_t responseSet, uint8_t choice) const;
  uint8_t choiceStyle(uint8_t responseSet, uint8_t choice) const;
  const char *resultReply(uint8_t stage, uint8_t responseSet, uint8_t choice) const;
  void applyChoice(uint8_t topic, uint8_t style, ParentDialogueOutcome &outcome);

  void drawDialoguePage(const ParentDialogueContext &ctx, const char *text,
                        uint8_t page, uint8_t pages) const;
  void drawChoicePage(const ParentDialogueContext &ctx, uint8_t responseSet,
                      uint8_t selected) const;
  void drawResultPage(const ParentDialogueContext &ctx, const char *reply,
                      const ParentDialogueOutcome &outcome) const;
  void printWrapped(const char *text, int x, int y, uint8_t maxChars,
                    uint8_t maxLines) const;
  bool waitForContinue(bool allowDismiss) const;
  uint8_t wrapScrollable(const char *text, char lines[][22], uint8_t maxLines) const;
  void showScrollableText(const ParentDialogueContext &ctx, const char *title,
                          const char *text, const char *footer) const;
};
