#include "GrowingTogether.h"
#include <cstring>
#include <cstdio>
#include <cstddef>

extern void clickSound();
extern void goodSound();
extern void badSound();
extern void waitButtonRelease();
extern bool rightHoldExitCheck();
extern void markSaveDirty();
extern unsigned long lastInputAt;

namespace {
constexpr int clampInt(const int value, const int minimum, const int maximum) {
  return value < minimum ? minimum : (value > maximum ? maximum : value);
}

#include "GrowingTogetherData.h"

constexpr uint8_t REL_SECURE = 0;
constexpr uint8_t REL_DISTANT = 1;
constexpr uint8_t REL_CLINGY = 2;
constexpr uint8_t REL_GUARDED = 3;
constexpr uint8_t REL_REBELLIOUS = 4;
constexpr uint8_t REL_RESENTFUL = 5;
constexpr uint8_t REL_RECONNECTING = 6;
constexpr uint8_t REL_DEEP_TRUST = 7;

const char *const RELATIONSHIP_NAMES[] = {
  "Secure", "Distant", "Clingy", "Guarded",
  "Rebellious", "Resentful", "Reconnecting", "Deep trust"
};

const char *const VALUE_NAMES[GT_VALUE_COUNT] = {
  "Kindness", "Honesty", "Courage", "Responsibility",
  "Curiosity", "Loyalty", "Independence", "Patience",
  "Fairness", "Tradition", "Achievement", "Creativity"
};

const char *const RITUAL_NAMES[GT_RITUAL_COUNT] = {
  "Story", "Song", "Best part", "Hard part",
  "Goodnight", "Night light", "Family tale", "Quiet sit"
};

const char *const HOBBY_NAMES[GT_HOBBY_COUNT] = {
  "Cooking", "Gardening", "Fishing", "Music", "Reading",
  "Stargazing", "Drawing", "Exercise", "Exploring", "Pet care"
};

const char *const PROMISE_NAMES[] = {
  "Talk", "Care", "Play", "Bedtime", "Garden", "Companion",
  "School", "Work", "Explore", "Cook", "Clean", "Family",
  "Apology", "Hobby"
};

// Age-specific openings keep a child from speaking like an adult professional.
const char *const STAGE_TALK_LEADS[4][GT_LEADS_PER_FEATURE] = {
  {"Can I tell you?", "I need help.", "Something happened.", "Can we talk?", "Please listen."},
  {"Can we talk?", "This is hard to say.", "I need your advice.", "Something happened today.", "Please hear me out."},
  {"Can we talk this through?", "I need another view.", "Something is weighing on me.", "I want to handle this well.", "I have a decision to make."},
  {"Sit with me a moment.", "I remembered something.", "I still value your thoughts.", "An old feeling returned.", "Let us talk slowly."}
};

// One direct question per feature. Each question points at the exact three
// choices shown on the next screen instead of using a generic ending.
const char *const FEATURE_QUESTIONS[GT_FEATURE_COUNT] = {
  "Should you listen, plan with me, or leave it for now?",
  "Will you explain the value, discuss the conflict, or ignore it?",
  "Can we name the hurt, repair it, or stay apart?",
  "Should we promise, make a real plan, or be honest that we cannot?",
  "Can we share a ritual, talk about today, or skip it?",
  "Will you ask, offer another kind of comfort, or ignore my no?",
  "Will you celebrate it, ask what it means, or dismiss it?",
  "Should we save it, share the memory, or let it go?",
  "Will you teach the word, explore it with me, or correct me harshly?",
  "Can you ask what I need, guide me calmly, or take over?",
  "Will you praise effort, practice with me, or focus only on the score?",
  "Will you teach the steps, use a fair consequence, or do it for me?",
  "Should we hear everyone, compromise, or have one person decide?",
  "Will the adults talk privately, agree on a plan, or argue here?",
  "Will you respect privacy, check safety gently, or invade it?",
  "Should we name the harm, repair it, or demand forgiveness?",
  "Will we practice, change the hobby, or quit it?",
  "Should the milestone feel personal, include family, or pass quickly?",
  "Will we remember together, make a memorial, or stop talking about it?",
  "Should we keep the good pattern, change it, or repeat it without thought?",
  "Will we check evidence, consider another view, or accept the fear as fact?",
  "Can you adapt support, offer choices, or use the same approach every time?",
  "Will we remember details, talk to the person, or dismiss the memory?",
  "Should we continue the tradition, update it, or end it?",
  "Will you explain the rule, allow a fair exception, or punish first?",
  "Will you notice my intention, address the result, or ignore it?",
  "Can you listen, reflect the feeling, or give advice before I am ready?"
};

uint8_t stageIndex(uint8_t stage) {
  if (stage <= 1) return 0;
  if (stage == 2) return 1;
  if (stage == 3) return 2;
  if (stage == 4) return 3;
  return 4;
}

void appendText(char *out, size_t outSize, const char *text) {
  if (!out || outSize == 0 || !text) return;
  const size_t used = strlen(out);
  if (used + 1 >= outSize) return;
  strncat(out, text, outSize - used - 1);
}

void appendNumber(char *out, size_t outSize, int value) {
  char number[16];
  snprintf(number, sizeof(number), "%d", value);
  appendText(out, outSize, number);
}
} // namespace

GrowingTogetherSystem::GrowingTogetherSystem(size_t storageBase,
                                             Adafruit_SSD1306 &display,
                                             uint8_t buttonLeft,
                                             uint8_t buttonSelect,
                                             uint8_t buttonRight)
    : storeA_(storageBase),
      storeB_(storageBase + sizeof(GrowingTogetherSave)),
      display_(display), btnL_(buttonLeft), btnS_(buttonSelect), btnR_(buttonRight) {
  reset();
}

void GrowingTogetherSystem::reset() {
  sequence_ = 0;
  nextPromptTick_ = 0;
  lastPromptTick_ = 0;
  pendingFeature_ = 255;
  pendingScenario_ = pendingLead_ = pendingEnding_ = pendingFlags_ = 0;
  dailyDay_ = 65535;
  dailyCount_ = 0;
  activeArcType_ = 255;
  activeArcStep_ = activeArcBranch_ = 0;
  activeArcDueDay_ = activeArcFlags_ = 0;
  memset(promiseType_, 255, sizeof(promiseType_));
  memset(promiseDueDay_, 0, sizeof(promiseDueDay_));
  memset(promiseStatus_, 0, sizeof(promiseStatus_));
  for (uint8_t i = 0; i < GT_VALUE_COUNT; ++i) {
    values_[i] = 35;
    valueConsistency_[i] = 50;
  }
  relationshipState_ = REL_SECURE;
  ruptureScore_ = 0;
  repairProgress_ = 0;
  memset(ritualStreak_, 0, sizeof(ritualStreak_));
  affectionPreferenceMask_ = 0x0055U;
  consentRespect_ = 50;
  affectionBond_ = 35;
  keepsakeMask_ = 0;
  memset(keepsakeCount_, 0, sizeof(keepsakeCount_));
  languageLevel_ = 0;
  vocabularyScore_ = 4;
  speechPattern_ = 0;
  memset(communicationStyleCount_, 0, sizeof(communicationStyleCount_));
  schoolEffort_ = 45;
  schoolCooperation_ = 45;
  schoolConfidence_ = 40;
  schoolLastReportDay_ = 65535;
  choreAssignedMask_ = choreDoneMask_ = choreMissedMask_ = 0;
  choreFairness_ = 60;
  familyMeetingDay_ = 65535;
  familyDecision_ = 255;
  coparentStyle_ = 0;
  coparentAgreement_ = 55;
  coparentDisagreement_ = 0;
  secretId_ = 255;
  privacyTrust_ = 50;
  secretSafety_ = 70;
  apologySkill_ = 25;
  repairSkill_ = 25;
  sharedHobby_ = 255;
  memset(hobbyBond_, 0, sizeof(hobbyBond_));
  milestoneMask_ = 0;
  griefType_ = 255;
  griefIntensity_ = griefStage_ = 0;
  memset(intergenerationalPattern_, 50, sizeof(intergenerationalPattern_));
  falseBeliefId_ = 255;
  falseBeliefIntensity_ = 0;
  memset(relationshipMemory_, 0, sizeof(relationshipMemory_));
  traditionMask_ = householdRulesMask_ = 0;
  memset(traditionStreak_, 0, sizeof(traditionStreak_));
  ruleConsistency_ = 55;
  unsolicitedAction_ = 255;
  unsolicitedDay_ = 65535;
  justListenCount_ = justListenRespected_ = 0;
  memset(reflectionCount_, 0, sizeof(reflectionCount_));
  memset(featureTalkCount_, 0, sizeof(featureTalkCount_));
  generationConversations_ = 0;
  lifetimeConversations_ = 0;
  lastFeature_ = 255;
  lastWeight_ = 0;
  lastRejectedMask_ = 0;
}

uint32_t GrowingTogetherSystem::checksum(const GrowingTogetherSave &save) const {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&save);
  const size_t count = offsetof(GrowingTogetherSave, checksum);
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < count; ++i) {
    hash ^= bytes[i];
    hash *= 16777619UL;
  }
  return hash;
}

bool GrowingTogetherSystem::valid(const GrowingTogetherSave &save) const {
  return save.magic == GROWING_TOGETHER_SAVE_MAGIC &&
         save.version == GROWING_TOGETHER_SAVE_VERSION &&
         save.checksum == checksum(save);
}

void GrowingTogetherSystem::fillSave(GrowingTogetherSave &s) const {
  memset(&s, 0, sizeof(s));
  s.magic = GROWING_TOGETHER_SAVE_MAGIC;
  s.version = GROWING_TOGETHER_SAVE_VERSION;
  s.sequence = sequence_ + 1;
  s.nextPromptTick = nextPromptTick_;
  s.lastPromptTick = lastPromptTick_;
  s.pendingFeature = pendingFeature_;
  s.pendingScenario = pendingScenario_;
  s.pendingLead = pendingLead_;
  s.pendingEnding = pendingEnding_;
  s.pendingFlags = pendingFlags_;
  s.dailyDay = dailyDay_;
  s.dailyCount = dailyCount_;
  s.activeArcType = activeArcType_;
  s.activeArcStep = activeArcStep_;
  s.activeArcBranch = activeArcBranch_;
  s.activeArcDueDay = activeArcDueDay_;
  s.activeArcFlags = activeArcFlags_;
  memcpy(s.promiseType, promiseType_, sizeof(promiseType_));
  memcpy(s.promiseDueDay, promiseDueDay_, sizeof(promiseDueDay_));
  memcpy(s.promiseStatus, promiseStatus_, sizeof(promiseStatus_));
  memcpy(s.values, values_, sizeof(values_));
  memcpy(s.valueConsistency, valueConsistency_, sizeof(valueConsistency_));
  s.relationshipState = relationshipState_;
  s.ruptureScore = ruptureScore_;
  s.repairProgress = repairProgress_;
  memcpy(s.ritualStreak, ritualStreak_, sizeof(ritualStreak_));
  s.affectionPreferenceMask = affectionPreferenceMask_;
  s.consentRespect = consentRespect_;
  s.affectionBond = affectionBond_;
  s.keepsakeMask = keepsakeMask_;
  memcpy(s.keepsakeCount, keepsakeCount_, sizeof(keepsakeCount_));
  s.languageLevel = languageLevel_;
  s.vocabularyScore = vocabularyScore_;
  s.speechPattern = speechPattern_;
  memcpy(s.communicationStyleCount, communicationStyleCount_, sizeof(communicationStyleCount_));
  s.schoolEffort = schoolEffort_;
  s.schoolCooperation = schoolCooperation_;
  s.schoolConfidence = schoolConfidence_;
  s.schoolLastReportDay = schoolLastReportDay_;
  s.choreAssignedMask = choreAssignedMask_;
  s.choreDoneMask = choreDoneMask_;
  s.choreMissedMask = choreMissedMask_;
  s.choreFairness = choreFairness_;
  s.familyMeetingDay = familyMeetingDay_;
  s.familyDecision = familyDecision_;
  s.coparentStyle = coparentStyle_;
  s.coparentAgreement = coparentAgreement_;
  s.coparentDisagreement = coparentDisagreement_;
  s.secretId = secretId_;
  s.privacyTrust = privacyTrust_;
  s.secretSafety = secretSafety_;
  s.apologySkill = apologySkill_;
  s.repairSkill = repairSkill_;
  s.sharedHobby = sharedHobby_;
  memcpy(s.hobbyBond, hobbyBond_, sizeof(hobbyBond_));
  s.milestoneMask = milestoneMask_;
  s.griefType = griefType_;
  s.griefIntensity = griefIntensity_;
  s.griefStage = griefStage_;
  memcpy(s.intergenerationalPattern, intergenerationalPattern_, sizeof(intergenerationalPattern_));
  s.falseBeliefId = falseBeliefId_;
  s.falseBeliefIntensity = falseBeliefIntensity_;
  memcpy(s.relationshipMemory, relationshipMemory_, sizeof(relationshipMemory_));
  s.traditionMask = traditionMask_;
  memcpy(s.traditionStreak, traditionStreak_, sizeof(traditionStreak_));
  s.householdRulesMask = householdRulesMask_;
  s.ruleConsistency = ruleConsistency_;
  s.unsolicitedAction = unsolicitedAction_;
  s.unsolicitedDay = unsolicitedDay_;
  s.justListenCount = justListenCount_;
  s.justListenRespected = justListenRespected_;
  memcpy(s.reflectionCount, reflectionCount_, sizeof(reflectionCount_));
  memcpy(s.featureTalkCount, featureTalkCount_, sizeof(featureTalkCount_));
  s.generationConversations = generationConversations_;
  s.lifetimeConversations = lifetimeConversations_;
  s.lastFeature = lastFeature_;
  s.lastWeight = lastWeight_;
  s.lastRejectedMask = lastRejectedMask_;
  s.checksum = checksum(s);
}

void GrowingTogetherSystem::applySave(const GrowingTogetherSave &s) {
  sequence_ = s.sequence;
  nextPromptTick_ = s.nextPromptTick;
  lastPromptTick_ = s.lastPromptTick;
  pendingFeature_ = s.pendingFeature < GT_FEATURE_COUNT ? s.pendingFeature : 255;
  pendingScenario_ = s.pendingScenario % GT_SCENARIOS_PER_FEATURE;
  pendingLead_ = s.pendingLead % GT_LEADS_PER_FEATURE;
  pendingEnding_ = s.pendingEnding % GT_ENDINGS_PER_FEATURE;
  pendingFlags_ = s.pendingFlags;
  dailyDay_ = s.dailyDay;
  dailyCount_ = s.dailyCount;
  activeArcType_ = s.activeArcType < GT_FEATURE_COUNT ? s.activeArcType : 255;
  activeArcStep_ = min<uint8_t>(s.activeArcStep, 6);
  activeArcBranch_ = min<uint8_t>(s.activeArcBranch, 2);
  activeArcDueDay_ = s.activeArcDueDay;
  activeArcFlags_ = s.activeArcFlags;
  memcpy(promiseType_, s.promiseType, sizeof(promiseType_));
  memcpy(promiseDueDay_, s.promiseDueDay, sizeof(promiseDueDay_));
  memcpy(promiseStatus_, s.promiseStatus, sizeof(promiseStatus_));
  memcpy(values_, s.values, sizeof(values_));
  memcpy(valueConsistency_, s.valueConsistency, sizeof(valueConsistency_));
  relationshipState_ = min<uint8_t>(s.relationshipState, REL_DEEP_TRUST);
  ruptureScore_ = min<uint8_t>(s.ruptureScore, 100);
  repairProgress_ = min<uint8_t>(s.repairProgress, 100);
  memcpy(ritualStreak_, s.ritualStreak, sizeof(ritualStreak_));
  affectionPreferenceMask_ = s.affectionPreferenceMask;
  consentRespect_ = min<uint8_t>(s.consentRespect, 100);
  affectionBond_ = min<uint8_t>(s.affectionBond, 100);
  keepsakeMask_ = s.keepsakeMask;
  memcpy(keepsakeCount_, s.keepsakeCount, sizeof(keepsakeCount_));
  languageLevel_ = min<uint8_t>(s.languageLevel, 5);
  vocabularyScore_ = min<uint8_t>(s.vocabularyScore, 100);
  speechPattern_ = s.speechPattern % 8;
  memcpy(communicationStyleCount_, s.communicationStyleCount, sizeof(communicationStyleCount_));
  schoolEffort_ = min<uint8_t>(s.schoolEffort, 100);
  schoolCooperation_ = min<uint8_t>(s.schoolCooperation, 100);
  schoolConfidence_ = min<uint8_t>(s.schoolConfidence, 100);
  schoolLastReportDay_ = s.schoolLastReportDay;
  choreAssignedMask_ = s.choreAssignedMask;
  choreDoneMask_ = s.choreDoneMask;
  choreMissedMask_ = s.choreMissedMask;
  choreFairness_ = min<uint8_t>(s.choreFairness, 100);
  familyMeetingDay_ = s.familyMeetingDay;
  familyDecision_ = s.familyDecision;
  coparentStyle_ = s.coparentStyle % 8;
  coparentAgreement_ = min<uint8_t>(s.coparentAgreement, 100);
  coparentDisagreement_ = min<uint8_t>(s.coparentDisagreement, 100);
  secretId_ = s.secretId;
  privacyTrust_ = min<uint8_t>(s.privacyTrust, 100);
  secretSafety_ = min<uint8_t>(s.secretSafety, 100);
  apologySkill_ = min<uint8_t>(s.apologySkill, 100);
  repairSkill_ = min<uint8_t>(s.repairSkill, 100);
  sharedHobby_ = s.sharedHobby < GT_HOBBY_COUNT ? s.sharedHobby : 255;
  memcpy(hobbyBond_, s.hobbyBond, sizeof(hobbyBond_));
  milestoneMask_ = s.milestoneMask;
  griefType_ = s.griefType;
  griefIntensity_ = min<uint8_t>(s.griefIntensity, 100);
  griefStage_ = min<uint8_t>(s.griefStage, 5);
  memcpy(intergenerationalPattern_, s.intergenerationalPattern, sizeof(intergenerationalPattern_));
  falseBeliefId_ = s.falseBeliefId;
  falseBeliefIntensity_ = min<uint8_t>(s.falseBeliefIntensity, 100);
  memcpy(relationshipMemory_, s.relationshipMemory, sizeof(relationshipMemory_));
  traditionMask_ = s.traditionMask;
  memcpy(traditionStreak_, s.traditionStreak, sizeof(traditionStreak_));
  householdRulesMask_ = s.householdRulesMask;
  ruleConsistency_ = min<uint8_t>(s.ruleConsistency, 100);
  unsolicitedAction_ = s.unsolicitedAction;
  unsolicitedDay_ = s.unsolicitedDay;
  justListenCount_ = s.justListenCount;
  justListenRespected_ = s.justListenRespected;
  memcpy(reflectionCount_, s.reflectionCount, sizeof(reflectionCount_));
  memcpy(featureTalkCount_, s.featureTalkCount, sizeof(featureTalkCount_));
  generationConversations_ = s.generationConversations;
  lifetimeConversations_ = s.lifetimeConversations;
  lastFeature_ = s.lastFeature;
  lastWeight_ = s.lastWeight;
  lastRejectedMask_ = s.lastRejectedMask;
}

bool GrowingTogetherSystem::load() {
  const GrowingTogetherSave a = storeA_.read();
  const GrowingTogetherSave b = storeB_.read();
  const bool va = valid(a), vb = valid(b);
  if (!va && !vb) {
    reset();
    return false;
  }
  applySave(va && vb ? (a.sequence >= b.sequence ? a : b) : (va ? a : b));
  return true;
}

void GrowingTogetherSystem::saveNow() {
  GrowingTogetherSave s;
  fillSave(s);
  if (s.sequence & 1UL) storeA_.write(s);
  else storeB_.write(s);
  sequence_ = s.sequence;
}

void GrowingTogetherSystem::changeByte(uint8_t &value, int delta, uint8_t maximum) {
  value = static_cast<uint8_t>(clampInt(static_cast<int>(value) + delta, 0, static_cast<int>(maximum)));
}

void GrowingTogetherSystem::startGeneration(const GrowingTogetherContext &ctx) {
  generationConversations_ = 0;
  pendingFeature_ = 255;
  dailyDay_ = ctx.ageDays;
  dailyCount_ = 0;
  activeArcType_ = 255;
  activeArcStep_ = activeArcBranch_ = 0;
  activeArcDueDay_ = ctx.ageDays;
  choreDoneMask_ = choreMissedMask_ = 0;
  schoolLastReportDay_ = 65535;
  unsolicitedAction_ = 255;
  unsolicitedDay_ = 65535;
  languageLevel_ = stageIndex(ctx.stage);
  vocabularyScore_ = static_cast<uint8_t>(clampInt(8 + ctx.generation * 2 + ctx.ownerTrust / 8, 5, 45));
  relationshipState_ = ctx.ownerTrust >= 70 ? REL_SECURE : REL_RECONNECTING;
  ruptureScore_ = 0;
  repairProgress_ = 10;
  nextPromptTick_ = ctx.ageTicks + 3;
  lastPromptTick_ = ctx.ageTicks;
  // Intergenerational patterns persist and influence the next child, while the
  // new relationship still begins with room to become its own story.
  for (uint8_t i = 0; i < 8; ++i) {
    const uint8_t inherited = intergenerationalPattern_[i];
    communicationStyleCount_[i] = static_cast<uint8_t>(inherited / 10);
  }
  markSaveDirty();
}

void GrowingTogetherSystem::scheduleNext(const GrowingTogetherContext &ctx, bool soon) {
  uint16_t low = 8, high = 22;
  if (ctx.stage <= 1) { low = 3; high = 9; }
  else if (ctx.stage == 2) { low = 6; high = 15; }
  else if (ctx.stage == 3) { low = 8; high = 19; }
  else if (ctx.stage >= 5) { low = 12; high = 30; }
  if (ctx.ownerBond >= 75 && ctx.ownerTrust >= 70) {
    low = max<uint16_t>(2, low * 3 / 4);
    high = max<uint16_t>(static_cast<uint16_t>(low + 1), high * 3 / 4);
  }
  if (relationshipState_ == REL_GUARDED || relationshipState_ == REL_DISTANT) {
    low = low * 5 / 4;
    high = high * 5 / 4;
  }
  if (soon) { low = 2; high = 5; }
  nextPromptTick_ = ctx.ageTicks + static_cast<uint32_t>(random(low, high + 1));
}

bool GrowingTogetherSystem::featureAllowedForStage(uint8_t feature,
                                                        const GrowingTogetherContext &ctx) const {
  if (ctx.stage <= 1 || feature >= GT_FEATURE_COUNT) return false;
  if (feature == GT_MULTI_DAY_ARC) {
    return activeArcType_ < GT_FEATURE_COUNT &&
           featureAllowedForStage(activeArcType_, ctx);
  }
  if (feature == GT_SCHOOL_REPORTS) return ctx.stage == 2 || ctx.stage == 3;
  if (feature == GT_COPARENTING || feature == GT_INTERGENERATIONAL)
    return ctx.stage >= 4;
  if (feature == GT_SECRETS_PRIVACY) return ctx.stage >= 2 && ctx.stage <= 4;
  return ctx.stage >= 2;
}

bool GrowingTogetherSystem::scenarioAllowedForStage(uint8_t feature,
                                                     uint8_t scenario,
                                                     const GrowingTogetherContext &ctx) const {
  if (!featureAllowedForStage(feature, ctx) || scenario >= GT_SCENARIOS_PER_FEATURE)
    return false;
  switch (feature) {
    case GT_DRAWINGS_LETTERS:
      if (scenario == 6) return ctx.stage == 2 || ctx.stage == 3; // school project
      if (scenario == 8) return ctx.stage >= 3;                  // private future dream
      return true;
    case GT_LANGUAGE_GROWTH:
      if (scenario == 1) return ctx.stage == 2;                  // first recognizable word
      if (scenario == 3) return ctx.stage == 2 || ctx.stage == 3;
      if (scenario == 4) return ctx.stage >= 3;                  // sarcasm
      if (scenario == 7) return ctx.stage >= 3;                  // advanced hobby/career words
      return true;
    case GT_CHORES:
      if (scenario == 11) return ctx.stage >= 4;                 // pass skill down
      return true;
    case GT_SECRETS_PRIVACY:
      if (scenario == 3 || scenario == 9) return ctx.stage >= 3; // crush / leave after argument
      if (scenario == 8) return ctx.stage == 2 || ctx.stage == 3;// teacher or older student
      return true;
    case GT_MILESTONES:
      if (ctx.stage == 2) return scenario == 0 || scenario == 1 || scenario == 10 || scenario == 11;
      if (ctx.stage == 3) return scenario == 0 || scenario == 2 || scenario == 10 || scenario == 11;
      if (ctx.stage == 4) return scenario == 0 || scenario == 2 || scenario == 3 || scenario == 4 ||
                                  scenario == 5 || scenario == 6 || scenario == 10 || scenario == 11;
      return scenario == 0 || scenario == 7 || scenario == 8 || scenario == 9 ||
             scenario == 10 || scenario == 11;
    case GT_FALSE_BELIEFS:
      if (scenario == 6) return ctx.stage == 2 || ctx.stage == 3; // teacher
      if (scenario == 8) return ctx.stage >= 4;                  // partner
      return true;
    case GT_RELATIONSHIP_MEMORY:
      if (scenario == 5) return ctx.stage >= 4;                  // partner/career
      if (scenario == 7) return ctx.stage == 2 || ctx.stage == 3 || ctx.stage >= 5;
      return true;
    case GT_FAMILY_TRADITIONS:
      if (scenario == 4) return ctx.stage == 2 || ctx.stage == 3;
      if (scenario == 6) return ctx.stage >= 4;
      return true;
    case GT_UNSOLICITED_BEHAVIOR:
      if (scenario == 7) return ctx.stage == 2 || ctx.stage == 3; // allowance
      if (scenario == 9) return ctx.stage >= 3;                  // cook alone
      return true;
    default:
      return true;
  }
}

uint8_t GrowingTogetherSystem::selectScenarioForFeature(uint8_t feature,
                                                         const GrowingTogetherContext &ctx) const {
  uint8_t eligible[GT_SCENARIOS_PER_FEATURE];
  uint8_t count = 0;
  for (uint8_t scenario = 0; scenario < GT_SCENARIOS_PER_FEATURE; ++scenario) {
    if (scenarioAllowedForStage(feature, scenario, ctx)) eligible[count++] = scenario;
  }
  return count ? eligible[random(0, count)] : 0;
}

const char *GrowingTogetherSystem::stageLead(const GrowingTogetherContext &ctx,
                                             uint8_t lead) const {
  const uint8_t row = ctx.stage == 2 ? 0 : (ctx.stage == 3 ? 1 : (ctx.stage == 4 ? 2 : 3));
  return STAGE_TALK_LEADS[row][lead % GT_LEADS_PER_FEATURE];
}

const char *GrowingTogetherSystem::featureQuestion(uint8_t feature) const {
  return FEATURE_QUESTIONS[feature % GT_FEATURE_COUNT];
}

uint8_t GrowingTogetherSystem::featureWeight(uint8_t f, const GrowingTogetherContext &c) const {
  if (!featureAllowedForStage(f, c)) return 0;
  const bool child = c.stage >= 2;
  const bool teen = c.stage >= 3;
  const bool adult = c.stage >= 4;
  const bool family = c.childCount > 0 || c.hasPartner;
  switch (f) {
    case GT_MULTI_DAY_ARC: return activeArcType_ < GT_FEATURE_COUNT && c.ageDays >= activeArcDueDay_ ? 25 : 0;
    case GT_FAMILY_VALUES: return child ? static_cast<uint8_t>(4 + (c.discipline < 5 ? 4 : 0)) : 1;
    case GT_RUPTURE_REPAIR: return ruptureScore_ >= 20 || c.ownerTrust < 45 || c.familyMood < 45 ? 15 : 2;
    case GT_PROMISES: return child ? static_cast<uint8_t>(promiseCount() ? 10 : 4) : 1;
    case GT_BEDTIME_RITUALS: return c.hour >= 19 || c.energy <= 3 ? 16 : 1;
    case GT_AFFECTION_CONSENT: return c.stage >= 1 ? static_cast<uint8_t>(c.emotionIntensity >= 4 ? 9 : 3) : 0;
    case GT_DRAWINGS_LETTERS: return child ? static_cast<uint8_t>(4 + c.stage) : 1;
    case GT_KEEPSAKE_BOX: return c.generation > 1 || keepsakeMask_ != 0 ? 6 : 2;
    case GT_LANGUAGE_GROWTH: return c.stage >= 1 ? static_cast<uint8_t>(7 + (c.stage <= 2 ? 4 : 0)) : 0;
    case GT_PARENT_STYLE: return child ? 6 : 2;
    case GT_SCHOOL_REPORTS: return c.stage == 2 || c.stage == 3 ? static_cast<uint8_t>(c.ageDays != schoolLastReportDay_ ? 12 : 2) : 0;
    case GT_CHORES: return child ? static_cast<uint8_t>((c.gardenCount || c.companionType < 8) ? 9 : 4) : 1;
    case GT_FAMILY_MEETINGS: return family ? static_cast<uint8_t>(c.familyMood < 60 ? 12 : 5) : 0;
    case GT_COPARENTING: return c.childCount > 0 && c.hasPartner ? static_cast<uint8_t>(c.partnerHarmony < 65 ? 15 : 5) : 0;
    case GT_SECRETS_PRIVACY: return teen ? static_cast<uint8_t>(privacyTrust_ < 50 ? 12 : 6) : (child ? 2 : 0);
    case GT_MEANINGFUL_APOLOGY: return child ? static_cast<uint8_t>(ruptureScore_ > 10 || c.mischief > 2 ? 11 : 4) : 1;
    case GT_SHARED_HOBBIES: return child ? static_cast<uint8_t>(sharedHobby_ < GT_HOBBY_COUNT ? 8 : 5) : 2;
    case GT_MILESTONES: return (milestoneMask_ & (1UL << min<uint8_t>(31, c.stage))) == 0 ? 13 : 2;
    case GT_GRIEF_REMEMBRANCE: return griefIntensity_ > 0 || (c.stage >= 5 && c.generation > 1) ? 12 : 1;
    case GT_INTERGENERATIONAL: return adult && (c.generation > 1 || c.childCount > 0) ? 10 : 0;
    case GT_FALSE_BELIEFS: return child ? static_cast<uint8_t>(c.emotionIntensity >= 5 ? 12 : 5) : 1;
    case GT_TEMPERAMENT: return c.stage >= 1 ? 6 : 0;
    case GT_RELATIONSHIP_MEMORY: return c.generation > 1 || c.rivalBond > 0 || c.childCount > 0 ? 7 : 2;
    case GT_FAMILY_TRADITIONS: return c.generation > 1 || traditionMask_ != 0 ? 8 : 3;
    case GT_HOUSEHOLD_RULES: return child ? static_cast<uint8_t>(c.discipline < 5 ? 10 : 5) : 1;
    case GT_UNSOLICITED_BEHAVIOR: return unsolicitedDay_ == c.ageDays ? 14 : (child ? 3 : 0);
    case GT_JUST_LISTEN: return child ? static_cast<uint8_t>(c.emotionIntensity >= 4 ? 14 : 5) : 1;
    default: return 0;
  }
}

void GrowingTogetherSystem::selectFeature(const GrowingTogetherContext &ctx, bool manual) {
  uint8_t pool[255];
  uint16_t count = 0;
  lastRejectedMask_ = 0;
  uint8_t bestWeight = 0;
  for (uint8_t f = 0; f < GT_FEATURE_COUNT; ++f) {
    uint8_t w = featureWeight(f, ctx);
    if (f == lastFeature_ && w > 1) w /= 2;
    // Manual check-ins still respect age and live context. They slightly
    // boost eligible topics instead of forcing school, marriage, privacy, or
    // grief content into life stages where it does not belong.
    if (manual && w > 0) w = min<uint8_t>(20, static_cast<uint8_t>(w + 2));
    if (w == 0) {
      if (f < 32) lastRejectedMask_ |= (1UL << f);
      continue;
    }
    bestWeight = max(bestWeight, w);
    for (uint8_t n = 0; n < w && count < sizeof(pool); ++n) pool[count++] = f;
  }
  if (count == 0) {
    pendingFeature_ = GT_PARENT_STYLE;
    lastWeight_ = 1;
  } else {
    pendingFeature_ = pool[random(0, count)];
    lastWeight_ = featureWeight(pendingFeature_, ctx);
  }
  const uint8_t contentFeature = pendingFeature_ == GT_MULTI_DAY_ARC && activeArcType_ < GT_FEATURE_COUNT
                                     ? activeArcType_ : pendingFeature_;
  pendingScenario_ = selectScenarioForFeature(contentFeature, ctx);
  pendingLead_ = static_cast<uint8_t>(random(0, GT_LEADS_PER_FEATURE));
  pendingEnding_ = 0;
  pendingFlags_ = 0;
  if (pendingFeature_ == GT_MULTI_DAY_ARC && activeArcType_ < GT_FEATURE_COUNT) {
    pendingScenario_ = selectScenarioForFeature(activeArcType_, ctx);
  }
}

void GrowingTogetherSystem::evaluatePromises(const GrowingTogetherContext &ctx) {
  for (uint8_t i = 0; i < GT_PROMISE_SLOTS; ++i) {
    if (promiseStatus_[i] != 1) continue;
    if (ctx.ageDays > promiseDueDay_[i]) {
      promiseStatus_[i] = 3; // broken
      changeByte(ruptureScore_, 10);
      changeByte(repairProgress_, -5);
      changeByte(ruleConsistency_, -4);
      if (!hasPending()) {
        pendingFeature_ = GT_PROMISES;
        pendingScenario_ = 11;
        pendingLead_ = 3;
        pendingEnding_ = 2;
        pendingFlags_ = 0x01;
      }
    }
  }
}

void GrowingTogetherSystem::evaluateMilestones(const GrowingTogetherContext &ctx) {
  const uint8_t stageBit = min<uint8_t>(ctx.stage, 5);
  if ((milestoneMask_ & (1UL << stageBit)) == 0) {
    milestoneMask_ |= (1UL << stageBit);
    keepsakeMask_ |= (1UL << min<uint8_t>(31, stageBit));
    if (keepsakeCount_[stageBit % GT_KEEP_COUNT] < 255) ++keepsakeCount_[stageBit % GT_KEEP_COUNT];
    if (!hasPending() && ctx.stage >= 2) {
      pendingFeature_ = GT_MILESTONES;
      pendingScenario_ = ctx.stage == 2 ? 1 : (ctx.stage == 3 ? 2 : (ctx.stage == 4 ? 3 : 8));
      pendingLead_ = stageBit % GT_LEADS_PER_FEATURE;
      pendingEnding_ = 0;
    }
  }
  languageLevel_ = max<uint8_t>(languageLevel_, stageIndex(ctx.stage));
  if (ctx.stage <= 1) speechPattern_ = 0;
  else if (ctx.stage == 2) speechPattern_ = vocabularyScore_ >= 45 ? 2 : 1;
  else if (ctx.stage == 3) speechPattern_ = 3 + (ctx.temperament % 2);
  else if (ctx.stage == 4) speechPattern_ = 5;
  else speechPattern_ = 6 + (ctx.temperament % 2);
}

void GrowingTogetherSystem::updateDerivedState(const GrowingTogetherContext &ctx) {
  if (ctx.ownerTrust >= 85 && ctx.ownerBond >= 85 && ruptureScore_ < 15) relationshipState_ = REL_DEEP_TRUST;
  else if (repairProgress_ >= 60 && ruptureScore_ >= 15) relationshipState_ = REL_RECONNECTING;
  else if (ruptureScore_ >= 75) relationshipState_ = REL_RESENTFUL;
  else if (ruptureScore_ >= 55 && ctx.stage >= 3) relationshipState_ = REL_REBELLIOUS;
  else if (ctx.ownerTrust < 30) relationshipState_ = REL_GUARDED;
  else if (ctx.ownerBond < 35) relationshipState_ = REL_DISTANT;
  else if (ctx.attachmentSecurity < 35 && ctx.ownerBond >= 55) relationshipState_ = REL_CLINGY;
  else relationshipState_ = REL_SECURE;

  if (griefIntensity_ > 0 && ctx.ageTicks % 24UL == 0) {
    --griefIntensity_;
    if (griefIntensity_ < 65) griefStage_ = 2;
    if (griefIntensity_ < 35) griefStage_ = 3;
    if (griefIntensity_ == 0) griefStage_ = 4;
  }
}

void GrowingTogetherSystem::tick(const GrowingTogetherContext &ctx, bool catchUpMode) {
  if (ctx.stage == 0) return;
  // Babies use ParentDialogue's babble system exclusively. This engine starts
  // at the child stage, preventing paragraphs about school, work or marriage.
  if (ctx.stage <= 1) {
    pendingFeature_ = 255;
    nextPromptTick_ = max<uint32_t>(nextPromptTick_, ctx.ageTicks + 3);
    updateDerivedState(ctx);
    return;
  }
  if (hasPending()) {
    const uint8_t content = pendingFeature_ == GT_MULTI_DAY_ARC && activeArcType_ < GT_FEATURE_COUNT
                                ? activeArcType_ : pendingFeature_;
    if (!featureAllowedForStage(content, ctx) || !scenarioAllowedForStage(content, pendingScenario_, ctx))
      pendingFeature_ = 255;
  }
  if (dailyDay_ != ctx.ageDays) {
    if (dailyDay_ != 65535) {
      choreMissedMask_ |= static_cast<uint16_t>(choreAssignedMask_ & ~choreDoneMask_);
      if (choreMissedMask_) changeByte(choreFairness_, -1);
    }
    dailyDay_ = ctx.ageDays;
    dailyCount_ = 0;
    choreDoneMask_ = 0;
    unsolicitedDay_ = ctx.ageDays;
    unsolicitedAction_ = static_cast<uint8_t>(random(0, 12));
    if (ctx.childCount || ctx.stage >= 2) choreAssignedMask_ = static_cast<uint16_t>(1U << random(0, 8));
    if (ctx.ageDays % 7 == 0) familyMeetingDay_ = ctx.ageDays;
  }
  evaluatePromises(ctx);
  evaluateMilestones(ctx);
  updateDerivedState(ctx);
  if (nextPromptTick_ == 0) scheduleNext(ctx);
  if (hasPending()) return;
  if (activeArcType_ < GT_FEATURE_COUNT && ctx.ageDays >= activeArcDueDay_) {
    pendingFeature_ = GT_MULTI_DAY_ARC;
    pendingScenario_ = selectScenarioForFeature(activeArcType_, ctx);
    pendingLead_ = activeArcStep_ % GT_LEADS_PER_FEATURE;
    pendingEnding_ = 0;
    return;
  }
  if (ctx.ageTicks < nextPromptTick_) return;
  const uint8_t dailyMax = ctx.stage <= 1 ? 5 : (ctx.stage == 2 ? 4 : 3);
  if (dailyCount_ >= dailyMax) {
    nextPromptTick_ = ctx.ageTicks + 10;
    return;
  }
  selectFeature(ctx, false);
  if (catchUpMode) nextPromptTick_ = ctx.ageTicks + 6;
}

bool GrowingTogetherSystem::shouldAutoOpen(const GrowingTogetherContext &ctx,
                                           unsigned long idleMs) const {
  if (!hasPending() || ctx.stage <= 1 || ctx.sick || ctx.ageTicks < nextPromptTick_) return false;
  const uint8_t content = pendingFeature_ == GT_MULTI_DAY_ARC && activeArcType_ < GT_FEATURE_COUNT
                              ? activeArcType_ : pendingFeature_;
  if (!featureAllowedForStage(content, ctx) || !scenarioAllowedForStage(content, pendingScenario_, ctx))
    return false;
  return idleMs >= 4200UL;
}

void GrowingTogetherSystem::scheduleManual(const GrowingTogetherContext &ctx) {
  if (ctx.stage <= 1) return;
  if (!hasPending()) selectFeature(ctx, true);
}

uint8_t GrowingTogetherSystem::promiseCount() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < GT_PROMISE_SLOTS; ++i) if (promiseStatus_[i] == 1) ++count;
  return count;
}

void GrowingTogetherSystem::recordAction(uint8_t action, const GrowingTogetherContext &ctx) {
  action %= 14;
  for (uint8_t i = 0; i < GT_PROMISE_SLOTS; ++i) {
    if (promiseStatus_[i] == 1 && promiseType_[i] == action && ctx.ageDays <= promiseDueDay_[i]) {
      promiseStatus_[i] = 2;
      changeByte(repairProgress_, 5);
      changeByte(ruleConsistency_, 3);
      relationshipMemory_[0] = static_cast<int8_t>(clampInt(static_cast<int>(relationshipMemory_[0]) + 2, -100, 100));
    }
  }
  if (action == GTA_BED) {
    const uint8_t ritual = static_cast<uint8_t>((ctx.ageDays + ctx.temperament) % GT_RITUAL_COUNT);
    changeByte(ritualStreak_[ritual], 1, 99);
    if (ritualStreak_[ritual] >= 3) traditionMask_ |= (1U << (ritual % GT_TRADITION_COUNT));
  }
  if (action == GTA_GARDEN || action == GTA_COMPANION || action == GTA_CLEAN || action == GTA_CARE) {
    choreDoneMask_ |= choreAssignedMask_;
  }
  if (action == GTA_PLAY || action == GTA_GARDEN || action == GTA_COOK ||
      action == GTA_EXPLORE || action == GTA_COMPANION) {
    uint8_t hobby = 0;
    if (action == GTA_GARDEN) hobby = 1;
    else if (action == GTA_PLAY) hobby = 3;
    else if (action == GTA_COOK) hobby = 0;
    else if (action == GTA_EXPLORE) hobby = 8;
    else hobby = 9;
    sharedHobby_ = hobby;
    changeByte(hobbyBond_[hobby], 2);
    if (hobbyBond_[hobby] >= 12) {
      traditionMask_ |= (1U << (hobby % GT_TRADITION_COUNT));
      changeByte(traditionStreak_[hobby % GT_TRADITION_COUNT], 1, 99);
    }
  }
  if (action == GTA_SCHOOL) {
    changeByte(schoolEffort_, 2);
    changeByte(vocabularyScore_, 1);
  }
  if (action == GTA_TALK) changeByte(vocabularyScore_, 1);
  markSaveDirty();
}

void GrowingTogetherSystem::recordLoss(uint8_t lossType,
                                       const GrowingTogetherContext &ctx) {
  griefType_ = static_cast<uint8_t>(lossType % 8);
  griefIntensity_ = max<uint8_t>(griefIntensity_, 78);
  griefStage_ = 1;
  keepsakeMask_ |= (1UL << 21);
  if (keepsakeCount_[11] < 255) ++keepsakeCount_[11];
  relationshipMemory_[7] = static_cast<int8_t>(clampInt(static_cast<int>(relationshipMemory_[7]) + 5, -100, 100));
  // The loss is remembered after the next egg begins. The current pet can
  // finish its archive animation without being interrupted by dialogue.
  activeArcType_ = GT_GRIEF_REMEMBRANCE;
  activeArcStep_ = 0;
  activeArcBranch_ = 0;
  activeArcDueDay_ = static_cast<uint16_t>(ctx.ageDays + 1);
  pendingFeature_ = 255;
  scheduleNext(ctx, true);
  markSaveDirty();
}

const char *GrowingTogetherSystem::featureName(uint8_t feature) const {
  return GT_FEATURE_NAMES[feature % GT_FEATURE_COUNT];
}

const char *GrowingTogetherSystem::scenarioLine(uint8_t feature, uint8_t scenario) const {
  return GT_SCENARIOS[feature % GT_FEATURE_COUNT][scenario % GT_SCENARIOS_PER_FEATURE];
}

const char *GrowingTogetherSystem::scenarioLineForContext(uint8_t feature,
                                                         uint8_t scenario,
                                                         const GrowingTogetherContext &ctx) const {
  // Replace the few deliberately broad database roots with stage-specific
  // wording. This prevents a child from saying "coworker" or an adult from
  // describing their own current school report.
  if (feature == GT_MULTI_DAY_ARC && scenario == 5) {
    if (ctx.stage == 2 || ctx.stage == 3) return "A teacher noticed that the problem is still going on.";
    if (ctx.stage == 4) return "A coworker noticed that the problem is still going on.";
    return "A family member noticed that the problem is still going on.";
  }
  if (feature == GT_PROMISES && scenario == 3) {
    if (ctx.stage == 2 || ctx.stage == 3) return "A school event needs your support at a certain time.";
    if (ctx.stage == 4) return "A work or family event needs support at a certain time.";
    return "A family event needs us to remember a certain time.";
  }
  if (feature == GT_LANGUAGE_GROWTH && scenario == 7) {
    if (ctx.stage == 3) return "My hobby and school interests are giving me new words.";
    if (ctx.stage == 4) return "My career or hobby is giving me specialized words.";
    return "A skill I practiced for years still gives me special words.";
  }
  if (feature == GT_CHORES && scenario == 9) {
    if (ctx.stage == 2 || ctx.stage == 3) return "I am tired after school and need a fair chore adjustment.";
    if (ctx.stage == 4) return "I am tired after work and need a fair chore adjustment.";
    return "My energy is lower now, and I need a fair chore adjustment.";
  }
  if (feature == GT_MILESTONES && scenario == 2 && ctx.stage == 3)
    return "Finishing one school stage changes how the family sees me.";
  if (feature == GT_FALSE_BELIEFS && scenario == 7 && ctx.stage == 2)
    return "Another child won, so I started thinking effort does not matter.";
  if (feature == GT_RELATIONSHIP_MEMORY && scenario == 5 && ctx.stage >= 5)
    return "My partner stayed beside me during a difficult adult year.";
  return scenarioLine(feature, scenario);
}

const char *GrowingTogetherSystem::choiceLabel(uint8_t feature, uint8_t choice) const {
  return GT_CHOICES[feature % GT_FEATURE_COUNT][choice % 3];
}

const char *GrowingTogetherSystem::resultLine(uint8_t feature, uint8_t choice) const {
  return GT_RESULTS[feature % GT_FEATURE_COUNT][choice % 3];
}

void GrowingTogetherSystem::appendContextLine(const GrowingTogetherContext &c,
                                              uint8_t feature,
                                              char *out, size_t outSize) const {
  appendText(out, outSize, "\nRight now, ");
  if (c.stage == 2) {
    switch (feature) {
      case GT_SCHOOL_REPORTS: appendText(out, outSize, c.schoolConfidence < 45 ? "school feels hard." : "I am getting braver at school."); return;
      case GT_CHORES: appendText(out, outSize, choreMissedMask_ ? "I missed a job I was meant to do." : "I can practice one job at a time."); return;
      case GT_GRIEF_REMEMBRANCE: appendText(out, outSize, griefIntensity_ > 60 ? "I miss them very much." : "the memory still matters to me."); return;
      case GT_FALSE_BELIEFS: appendText(out, outSize, "a big feeling may be making one thought seem true."); return;
      case GT_PROMISES: appendText(out, outSize, promiseCount() ? "I am waiting on a promise." : "I want our words to be clear."); return;
      case GT_AFFECTION_CONSENT: appendText(out, outSize, consentRespect_ >= 70 ? "I trust you to hear no." : "I am learning how to say what touch I want."); return;
      default: appendText(out, outSize, c.ownerTrust >= 70 ? "I think I can tell you the truth." : "I am not sure how much to say yet."); return;
    }
  }
  switch (feature) {
    case GT_BEDTIME_RITUALS:
      appendText(out, outSize, c.energy <= 3 ? "I am very tired." : "the day is finally slowing down.");
      break;
    case GT_SCHOOL_REPORTS:
      appendText(out, outSize, c.schoolConfidence < 45 ? "school confidence is fragile." : "school confidence is growing.");
      break;
    case GT_CHORES:
      appendText(out, outSize, choreMissedMask_ ? "a responsibility was missed." : "the routine can still be practiced.");
      break;
    case GT_COPARENTING:
      appendText(out, outSize, c.partnerHarmony < 55 ? "the adults are not in agreement." : "the adults can plan calmly.");
      break;
    case GT_GRIEF_REMEMBRANCE:
      appendText(out, outSize, griefIntensity_ > 60 ? "the loss still feels close." : "the memory is becoming easier to carry.");
      break;
    case GT_FALSE_BELIEFS:
      appendText(out, outSize, c.emotionIntensity >= 5 ? "the feeling is making the thought seem certain." : "there may be more than one explanation.");
      break;
    case GT_AFFECTION_CONSENT:
      appendText(out, outSize, consentRespect_ >= 70 ? "I expect my boundaries to be respected." : "I am still learning whether no will be heard.");
      break;
    case GT_PROMISES:
      appendText(out, outSize, promiseCount() ? "there is an active promise." : "our words could become a real commitment.");
      break;
    case GT_MULTI_DAY_ARC:
      appendText(out, outSize, "this is day ");
      appendNumber(out, outSize, activeArcStep_ + 1);
      appendText(out, outSize, " of an ongoing story about ");
      appendText(out, outSize, activeArcType_ < GT_FEATURE_COUNT ? featureName(activeArcType_) : "growing up");
      appendText(out, outSize, ". Your earlier response was to ");
      appendText(out, outSize, activeArcBranch_ == 0 ? "listen and connect" :
                                      (activeArcBranch_ == 1 ? "make a practical plan" : "pull away from the issue"));
      appendText(out, outSize, ", and the next part reflects that choice.");
      break;
    default:
      appendText(out, outSize, c.ownerTrust >= 70 ? "I believe I can be honest with you." : "I am deciding how much feels safe to share.");
      break;
  }
}

void GrowingTogetherSystem::buildPrompt(const GrowingTogetherContext &ctx,
                                        char *out, size_t outSize) const {
  if (!out || outSize == 0) return;
  out[0] = 0;
  appendText(out, outSize, stageLead(ctx, pendingLead_));
  appendText(out, outSize, "\n");
  const uint8_t contentFeature =
      pendingFeature_ == GT_MULTI_DAY_ARC && activeArcType_ < GT_FEATURE_COUNT
          ? activeArcType_ : pendingFeature_;
  appendText(out, outSize, scenarioLineForContext(contentFeature, pendingScenario_, ctx));
  appendContextLine(ctx, pendingFeature_, out, outSize);
  appendText(out, outSize, "\n");
  appendText(out, outSize, featureQuestion(contentFeature));
}

uint8_t GrowingTogetherSystem::wrapText(const char *text, char lines[][22],
                                        uint8_t maxLines) const {
  if (!text || maxLines == 0) return 0;
  uint8_t line = 0;
  uint8_t col = 0;
  memset(lines, 0, static_cast<size_t>(maxLines) * 22U);
  const char *p = text;
  while (*p && line < maxLines) {
    if (*p == '\n') {
      ++line;
      col = 0;
      ++p;
      continue;
    }
    while (*p == ' ') ++p;
    if (!*p) break;
    const char *word = p;
    uint8_t length = 0;
    while (p[length] && p[length] != ' ' && p[length] != '\n') ++length;
    if (col > 0 && col + 1 + length > 21) {
      ++line;
      col = 0;
      if (line >= maxLines) break;
    }
    if (col > 0) lines[line][col++] = ' ';
    uint8_t copied = 0;
    while (copied < length && line < maxLines) {
      if (col >= 21) {
        ++line;
        col = 0;
        if (line >= maxLines) break;
      }
      lines[line][col++] = word[copied++];
    }
    if (line < maxLines) lines[line][col] = 0;
    p += length;
  }
  if (line >= maxLines) return maxLines;
  return static_cast<uint8_t>(line + 1);
}

void GrowingTogetherSystem::showScrollableText(const char *title, const char *text,
                                               const char *footer,
                                               bool allowBack) const {
  char lines[48][22];
  const uint8_t count = wrapText(text, lines, 48);
  uint8_t top = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    const bool l = digitalRead(btnL_);
    const bool s = digitalRead(btnS_);
    const bool r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) {
      if (top > 0) --top;
      clickSound(); delay(90);
    }
    if (oldR == HIGH && r == LOW) {
      if (top + 5 < count) ++top;
      clickSound(); delay(90);
    }
    if (oldS == HIGH && s == LOW) {
      clickSound(); waitButtonRelease(); return;
    }
    if (allowBack && rightHoldExitCheck()) return;
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay();
    display_.setTextSize(1);
    display_.setTextColor(SSD1306_WHITE);
    display_.setTextWrap(false);
    display_.drawLine(0, 8, 127, 8, SSD1306_WHITE);
    display_.setCursor(1, 0);
    // A 128-pixel display fits 21 characters at the default 6-pixel font.
    // Keep the whole feature title visible and move the page counter below.
    for (uint8_t i = 0; title && title[i] && i < 21; ++i) display_.print(title[i]);
    for (uint8_t row = 0; row < 5; ++row) {
      const uint8_t idx = static_cast<uint8_t>(top + row);
      if (idx >= count) break;
      display_.setCursor(1, 11 + row * 9);
      display_.print(lines[idx]);
    }
    display_.drawLine(0, 55, 127, 55, SSD1306_WHITE);
    display_.setCursor(1, 57);
    if (count > 5) {
      display_.print(F("D0/D2 "));
      display_.print(top + 1); display_.print('/');
      display_.print(max<uint8_t>(1, static_cast<uint8_t>(count - 4)));
      display_.print(F(" D1 next"));
    } else display_.print(footer ? footer : "D1 continue");
    display_.display();
    lastInputAt = millis();
    delay(18);
  }
}

void GrowingTogetherSystem::showReadableMessage(const char *title, const char *text,
                                                const char *footer) const {
  showScrollableText(title, text, footer, false);
}

uint8_t GrowingTogetherSystem::chooseResponse(const GrowingTogetherContext &ctx,
                                              uint8_t feature) const {
  (void)ctx;
  uint8_t selected = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    const bool l = digitalRead(btnL_);
    const bool s = digitalRead(btnS_);
    const bool r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) {
      selected = selected == 0 ? 2 : selected - 1;
      clickSound(); delay(100);
    }
    if (oldR == HIGH && r == LOW) {
      selected = static_cast<uint8_t>((selected + 1) % 3);
      clickSound(); delay(100);
    }
    if (oldS == HIGH && s == LOW) {
      clickSound(); waitButtonRelease(); return selected;
    }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay();
    display_.setTextSize(1);
    display_.setTextColor(SSD1306_WHITE);
    display_.setTextWrap(false);
    display_.setCursor(1, 1); display_.print(F("HOW DO YOU RESPOND?"));
    for (uint8_t i = 0; i < 3; ++i) {
      const int y = 13 + i * 15;
      if (i == selected) display_.fillRoundRect(1, y, 126, 13, 3, SSD1306_WHITE);
      else display_.drawRoundRect(1, y, 126, 13, 3, SSD1306_WHITE);
      display_.setTextColor(i == selected ? SSD1306_BLACK : SSD1306_WHITE);
      display_.setCursor(5, y + 3);
      const char *label = choiceLabel(feature, i);
      // Twenty characters fit from x=5 through x=124.
      for (uint8_t c = 0; label[c] && c < 20; ++c) display_.print(label[c]);
    }
    display_.setTextColor(SSD1306_WHITE);
    display_.setCursor(1, 59); display_.print(F("D0/D2 choose  D1 answer"));
    display_.display();
    lastInputAt = millis();
    delay(18);
  }
}

void GrowingTogetherSystem::advanceArc(const GrowingTogetherContext &ctx,
                                       uint8_t feature, uint8_t choice) {
  if (feature == GT_MULTI_DAY_ARC) {
    activeArcBranch_ = choice;
    if (++activeArcStep_ >= 4) {
      activeArcType_ = 255;
      activeArcStep_ = 0;
      activeArcDueDay_ = 0;
      activeArcFlags_ = 0;
      keepsakeMask_ |= (1UL << 20);
      if (keepsakeCount_[10] < 255) ++keepsakeCount_[10];
    } else {
      activeArcDueDay_ = static_cast<uint16_t>(ctx.ageDays + 1);
    }
    return;
  }
  const bool arcFeature = feature == GT_RUPTURE_REPAIR || feature == GT_SCHOOL_REPORTS ||
                          feature == GT_COPARENTING || feature == GT_SECRETS_PRIVACY ||
                          feature == GT_GRIEF_REMEMBRANCE || feature == GT_FALSE_BELIEFS ||
                          feature == GT_MEANINGFUL_APOLOGY;
  if (arcFeature && activeArcType_ >= GT_FEATURE_COUNT) {
    activeArcType_ = feature;
    activeArcStep_ = 1;
    activeArcBranch_ = choice;
    activeArcDueDay_ = static_cast<uint16_t>(ctx.ageDays + 1);
    activeArcFlags_ = static_cast<uint16_t>(1U << (pendingScenario_ % 16));
  }
}

void GrowingTogetherSystem::applyFeatureChoice(const GrowingTogetherContext &ctx,
                                               uint8_t feature, uint8_t choice,
                                               GrowingTogetherOutcome &o) {
  const bool supportive = choice < 2;
  o.feature = feature;
  o.choice = choice;
  o.resolved = true;
  o.emotion = supportive ? 1 : 3;
  o.emotionIntensity = supportive ? 3 : 5;
  if (choice == 0) {
    o.bond = 2; o.trust = 2; o.happy = 1; o.kindness = 1;
    o.parentOpenness = 2; o.parentEmpathy = 2; o.parentAttachment = 2;
    changeByte(communicationStyleCount_[0], 1, 255);
    changeByte(reflectionCount_[0], 1, 255);
  } else if (choice == 1) {
    o.bond = 1; o.trust = 1; o.discipline = 1; o.smart = 1;
    o.parentConfidence = 1; o.parentResponsibility = 2; o.parentResilience = 1;
    changeByte(communicationStyleCount_[2], 1, 255);
    changeByte(reflectionCount_[2], 1, 255);
  } else {
    o.bond = -2; o.trust = -2; o.happy = -1;
    o.parentOpenness = -2; o.parentAttachment = -2;
    changeByte(communicationStyleCount_[6], 1, 255);
    changeByte(reflectionCount_[6], 1, 255);
    changeByte(ruptureScore_, 6);
  }

  switch (feature) {
    case GT_FAMILY_VALUES: {
      const uint8_t value = pendingScenario_ % GT_VALUE_COUNT;
      changeByte(values_[value], choice == 0 ? 4 : (choice == 1 ? 2 : -3));
      changeByte(valueConsistency_[value], choice == 0 ? 3 : (choice == 1 ? 1 : -5));
      o.parentHonesty += choice < 2 ? 1 : -1;
      break;
    }
    case GT_RUPTURE_REPAIR:
      if (choice < 2) { changeByte(ruptureScore_, -10); changeByte(repairProgress_, 10); o.familyMood += 2; }
      else { changeByte(ruptureScore_, 10); changeByte(repairProgress_, -6); o.familyMood -= 2; }
      break;
    case GT_PROMISES: {
      if (choice < 2) {
        uint8_t slot = 255;
        for (uint8_t i = 0; i < GT_PROMISE_SLOTS; ++i) if (promiseStatus_[i] == 0 || promiseStatus_[i] >= 2) { slot = i; break; }
        if (slot < GT_PROMISE_SLOTS) {
          promiseType_[slot] = static_cast<uint8_t>((pendingScenario_ + ctx.stage) % 14);
          promiseDueDay_[slot] = static_cast<uint16_t>(ctx.ageDays + (choice == 0 ? 1 : 2));
          promiseStatus_[slot] = 1;
        }
        changeByte(ruleConsistency_, 2);
      } else {
        // Honest refusal is less damaging than a broken promise.
        o.bond = -1; o.trust = 0; changeByte(ruleConsistency_, 1);
      }
      break;
    }
    case GT_BEDTIME_RITUALS: {
      const uint8_t ritual = pendingScenario_ % GT_RITUAL_COUNT;
      changeByte(ritualStreak_[ritual], choice < 2 ? 2 : -1, 99);
      if (ritualStreak_[ritual] >= 3) traditionMask_ |= (1U << (ritual % GT_TRADITION_COUNT));
      o.parentAttachment += choice < 2 ? 2 : -1;
      break;
    }
    case GT_AFFECTION_CONSENT:
      if (choice < 2) { changeByte(consentRespect_, 4); changeByte(affectionBond_, 3); affectionPreferenceMask_ ^= (1U << (pendingScenario_ % 12)); }
      else { changeByte(consentRespect_, -8); changeByte(affectionBond_, -5); }
      break;
    case GT_DRAWINGS_LETTERS:
      if (choice < 2) { const uint8_t k = pendingScenario_ % GT_KEEP_COUNT; keepsakeMask_ |= (1UL << k); changeByte(keepsakeCount_[k], 1, 255); o.parentEmpathy += 1; }
      break;
    case GT_KEEPSAKE_BOX:
      if (choice < 2) { const uint8_t k = pendingScenario_ % GT_KEEP_COUNT; keepsakeMask_ |= (1UL << k); changeByte(keepsakeCount_[k], 1, 255); }
      else { keepsakeMask_ &= ~(1UL << (pendingScenario_ % GT_KEEP_COUNT)); }
      break;
    case GT_LANGUAGE_GROWTH:
      changeByte(vocabularyScore_, choice == 0 ? 4 : (choice == 1 ? 3 : -3));
      languageLevel_ = max<uint8_t>(languageLevel_, stageIndex(ctx.stage));
      o.parentEmotionWords += choice < 2 ? 2 : -1;
      break;
    case GT_PARENT_STYLE:
      o.parentOpenness += choice == 0 ? 2 : (choice == 1 ? 1 : -2);
      o.parentIndependence += choice == 1 ? 2 : (choice == 2 ? -1 : 0);
      break;
    case GT_SCHOOL_REPORTS:
      schoolLastReportDay_ = ctx.ageDays;
      changeByte(schoolEffort_, choice == 0 ? 4 : (choice == 1 ? 3 : -4));
      changeByte(schoolConfidence_, choice < 2 ? 4 : -6);
      o.parentSchoolConfidence += choice < 2 ? 3 : -3;
      break;
    case GT_CHORES:
      if (choice == 0) { choreDoneMask_ |= choreAssignedMask_; changeByte(choreFairness_, 3); o.parentResponsibility += 2; }
      else if (choice == 1) { changeByte(choreFairness_, 2); o.discipline += 1; o.parentResponsibility += 1; }
      else { changeByte(choreFairness_, -4); o.parentResponsibility -= 2; }
      break;
    case GT_FAMILY_MEETINGS:
      familyMeetingDay_ = ctx.ageDays; familyDecision_ = choice;
      o.familyMood += choice == 0 ? 3 : (choice == 1 ? 2 : -3);
      break;
    case GT_COPARENTING:
      if (choice < 2) { changeByte(coparentAgreement_, 4); changeByte(coparentDisagreement_, -3); o.partnerHarmony += 3; o.familyMood += 2; }
      else { changeByte(coparentAgreement_, -5); changeByte(coparentDisagreement_, 6); o.partnerHarmony -= 4; o.familyMood -= 3; }
      break;
    case GT_SECRETS_PRIVACY:
      secretId_ = pendingScenario_;
      if (choice == 0) { changeByte(privacyTrust_, 5); o.parentOpenness += 2; }
      else if (choice == 1) { changeByte(privacyTrust_, 3); changeByte(secretSafety_, 4); o.parentSafetyConfidence += 2; }
      else { changeByte(privacyTrust_, -8); o.parentOpenness -= 3; }
      break;
    case GT_MEANINGFUL_APOLOGY:
      if (choice == 0) { changeByte(apologySkill_, 5); changeByte(ruptureScore_, -5); o.parentHonesty += 2; }
      else if (choice == 1) { changeByte(repairSkill_, 6); changeByte(repairProgress_, 6); }
      else { changeByte(ruptureScore_, 7); changeByte(repairSkill_, -3); }
      break;
    case GT_SHARED_HOBBIES: {
      const uint8_t hobby = pendingScenario_ % GT_HOBBY_COUNT;
      if (choice < 2) { sharedHobby_ = hobby; changeByte(hobbyBond_[hobby], choice == 0 ? 4 : 3); if (hobbyBond_[hobby] >= 12) traditionMask_ |= (1U << (hobby % GT_TRADITION_COUNT)); }
      else changeByte(hobbyBond_[hobby], -3);
      break;
    }
    case GT_MILESTONES:
      if (choice < 2) { keepsakeMask_ |= (1UL << (12 + pendingScenario_ % 12)); changeByte(keepsakeCount_[pendingScenario_ % GT_KEEP_COUNT], 1, 255); o.familyMood += 2; }
      break;
    case GT_GRIEF_REMEMBRANCE:
      griefType_ = pendingScenario_ % 8;
      if (griefIntensity_ == 0) griefIntensity_ = 70;
      if (choice < 2) { changeByte(griefIntensity_, -8); griefStage_ = max<uint8_t>(1, griefStage_); o.familyMood += 1; }
      else { changeByte(griefIntensity_, 5); o.familyMood -= 1; }
      break;
    case GT_INTERGENERATIONAL: {
      const uint8_t p = pendingScenario_ % 8;
      changeByte(intergenerationalPattern_[p], choice == 0 ? 3 : (choice == 1 ? -3 : 4));
      o.parentAttachment += choice == 1 ? 2 : 0;
      break;
    }
    case GT_FALSE_BELIEFS:
      falseBeliefId_ = pendingScenario_;
      if (choice < 2) { changeByte(falseBeliefIntensity_, -12); o.parentResilience += 2; o.parentConfidence += 1; }
      else { changeByte(falseBeliefIntensity_, 10); o.parentConfidence -= 2; }
      break;
    case GT_TEMPERAMENT:
      if (choice < 2) { o.parentConfidence += 1; o.parentIndependence += 1; }
      else { o.parentOpenness -= 2; }
      break;
    case GT_RELATIONSHIP_MEMORY: {
      const uint8_t m = pendingScenario_ % GT_MEMORY_COUNT;
      const int delta = choice == 0 ? 4 : (choice == 1 ? 3 : -3);
      relationshipMemory_[m] = static_cast<int8_t>(clampInt(static_cast<int>(relationshipMemory_[m]) + delta, -100, 100));
      break;
    }
    case GT_FAMILY_TRADITIONS: {
      const uint8_t t = pendingScenario_ % GT_TRADITION_COUNT;
      if (choice < 2) { traditionMask_ |= (1U << t); changeByte(traditionStreak_[t], 2, 99); }
      else { traditionMask_ &= ~(1U << t); traditionStreak_[t] = 0; }
      break;
    }
    case GT_HOUSEHOLD_RULES: {
      const uint8_t r = pendingScenario_ % 12;
      if (choice < 2) { householdRulesMask_ |= (1U << r); changeByte(ruleConsistency_, choice == 0 ? 3 : 2); }
      else changeByte(ruleConsistency_, -6);
      o.parentResponsibility += choice < 2 ? 1 : -1;
      break;
    }
    case GT_UNSOLICITED_BEHAVIOR:
      unsolicitedAction_ = pendingScenario_;
      if (choice == 0) { o.parentConfidence += 1; o.bond += 1; }
      else if (choice == 1) { o.parentResponsibility += 2; o.discipline += 1; }
      else { o.parentResponsibility -= 1; }
      break;
    case GT_JUST_LISTEN:
      changeByte(justListenCount_, 1, 255);
      if (choice < 2) { changeByte(justListenRespected_, 1, 255); o.parentOpenness += 3; o.parentEmpathy += 2; }
      else { o.parentOpenness -= 3; o.parentEmpathy -= 2; }
      break;
    default: break;
  }
  advanceArc(ctx, feature, choice);
}

GrowingTogetherOutcome GrowingTogetherSystem::runPending(const GrowingTogetherContext &ctx) {
  GrowingTogetherOutcome out{};
  if (!hasPending() || ctx.stage <= 1) return out;
  uint8_t validationFeature = pendingFeature_ == GT_MULTI_DAY_ARC && activeArcType_ < GT_FEATURE_COUNT
                                  ? activeArcType_ : pendingFeature_;
  if (!featureAllowedForStage(validationFeature, ctx) ||
      !scenarioAllowedForStage(validationFeature, pendingScenario_, ctx)) {
    pendingFeature_ = 255;
    selectFeature(ctx, false);
    validationFeature = pendingFeature_ == GT_MULTI_DAY_ARC && activeArcType_ < GT_FEATURE_COUNT
                            ? activeArcType_ : pendingFeature_;
    if (!hasPending() || !featureAllowedForStage(validationFeature, ctx)) return out;
  }
  char prompt[640];
  buildPrompt(ctx, prompt, sizeof(prompt));
  const bool followUp = pendingFeature_ == GT_MULTI_DAY_ARC && activeArcType_ < GT_FEATURE_COUNT;
  const uint8_t contentFeature = followUp ? activeArcType_ : pendingFeature_;
  showScrollableText(followUp ? "STORY FOLLOW-UP" : featureName(contentFeature),
                     prompt, "D1 response");
  const uint8_t choice = chooseResponse(ctx, contentFeature);
  applyFeatureChoice(ctx, contentFeature, choice, out);
  // Applying the underlying topic changes the real school/family/privacy/etc.
  // system; this separate step advances the four-day narrative itself.
  if (followUp) advanceArc(ctx, GT_MULTI_DAY_ARC, choice);
  showScrollableText("YOUR TAMA REPLIES", resultLine(contentFeature, choice), "D1 close");
  choice < 2 ? goodSound() : badSound();

  if (featureTalkCount_[contentFeature] < 65535) ++featureTalkCount_[contentFeature];
  if (generationConversations_ < 65535) ++generationConversations_;
  ++lifetimeConversations_;
  if (dailyCount_ < 255) ++dailyCount_;
  lastFeature_ = contentFeature;
  lastPromptTick_ = ctx.ageTicks;
  pendingFeature_ = 255;
  pendingFlags_ = 0;
  scheduleNext(ctx);
  updateDerivedState(ctx);
  markSaveDirty();
  return out;
}

void GrowingTogetherSystem::drawHubPage(const GrowingTogetherContext &ctx,
                                        uint8_t page, uint8_t pages) const {
  display_.clearDisplay();
  display_.setTextSize(1);
  display_.setTextColor(SSD1306_WHITE);
  display_.setTextWrap(false);
  display_.setCursor(1, 0); display_.print(F("GROWING TOGETHER"));
  display_.setCursor(108, 0); display_.print(page + 1); display_.print('/'); display_.print(pages);
  display_.drawLine(0, 9, 127, 9, SSD1306_WHITE);
  if (page == 0) {
    display_.setCursor(2, 14); display_.print(hasPending() ? F("A talk is waiting") : F("Start parent check-in"));
    display_.setCursor(2, 27); display_.print(F("Relationship: ")); display_.print(RELATIONSHIP_NAMES[relationshipState_]);
    display_.setCursor(2, 40); display_.print(F("This life talks ")); display_.print(generationConversations_);
    display_.setCursor(2, 53); display_.print(F("D1 talk now"));
  } else if (page == 1) {
    display_.setCursor(2, 14); display_.print(F("Rupture ")); display_.print(ruptureScore_);
    display_.setCursor(66, 14); display_.print(F("Repair ")); display_.print(repairProgress_);
    display_.setCursor(2, 27); display_.print(F("Consent ")); display_.print(consentRespect_);
    display_.setCursor(66, 27); display_.print(F("Privacy ")); display_.print(privacyTrust_);
    display_.setCursor(2, 40); display_.print(F("Apology ")); display_.print(apologySkill_);
    display_.setCursor(66, 40); display_.print(F("Rules ")); display_.print(ruleConsistency_);
    display_.setCursor(2, 55); display_.print(F("D1 reflection"));
  } else if (page == 2) {
    display_.setCursor(2, 14); display_.print(F("Active promises ")); display_.print(promiseCount());
    for (uint8_t i = 0; i < GT_PROMISE_SLOTS; ++i) {
      if (promiseStatus_[i] == 1) {
        display_.setCursor(2, 25 + i * 9); display_.print(PROMISE_NAMES[promiseType_[i] % 14]);
        display_.print(F(" by day ")); display_.print(promiseDueDay_[i]);
      }
    }
    display_.setCursor(2, 58); display_.print(F("D1 detailed list"));
  } else if (page == 3) {
    display_.setCursor(2, 14); display_.print(F("Language level ")); display_.print(languageLevel_);
    display_.setCursor(2, 26); display_.print(F("Vocabulary ")); display_.print(vocabularyScore_);
    display_.setCursor(2, 38); display_.print(F("School effort ")); display_.print(schoolEffort_);
    display_.setCursor(2, 50); display_.print(F("School confidence ")); display_.print(schoolConfidence_);
  } else if (page == 4) {
    display_.setCursor(2, 14); display_.print(F("Keepsake types "));
    uint8_t keepTypes = 0; for (uint8_t i = 0; i < 24; ++i) if (keepsakeMask_ & (1UL << i)) ++keepTypes;
    display_.print(keepTypes);
    display_.setCursor(2, 27); display_.print(F("Traditions ")); uint8_t tr = 0; for (uint8_t i=0;i<GT_TRADITION_COUNT;++i) if(traditionMask_&(1U<<i))++tr; display_.print(tr);
    display_.setCursor(2, 40); display_.print(F("Shared hobby ")); display_.print(sharedHobby_ < GT_HOBBY_COUNT ? HOBBY_NAMES[sharedHobby_] : "None yet");
    display_.setCursor(2, 53); display_.print(F("Milestones saved ")); display_.print(__builtin_popcount(milestoneMask_));
  } else if (page == 5) {
    display_.setCursor(2, 14); display_.print(F("Family meeting day ")); display_.print(familyMeetingDay_);
    display_.setCursor(2, 27); display_.print(F("Co-parent agree ")); display_.print(coparentAgreement_);
    display_.setCursor(2, 40); display_.print(F("Chore fairness ")); display_.print(choreFairness_);
    display_.setCursor(2, 53); display_.print(F("Family mood ")); display_.print(ctx.familyMood);
  } else if (page == 6) {
    display_.setCursor(2, 14); display_.print(F("Active story: "));
    display_.print(activeArcType_ < GT_FEATURE_COUNT ? featureName(activeArcType_) : "None");
    display_.setCursor(2, 27); display_.print(F("Story step ")); display_.print(activeArcStep_);
    display_.setCursor(2, 40); display_.print(F("Due day ")); display_.print(activeArcDueDay_);
    display_.setCursor(2, 53); display_.print(F("Grief intensity ")); display_.print(griefIntensity_);
  } else if (page == 7) {
    display_.setCursor(2, 14); display_.print(F("27 feature banks"));
    display_.setCursor(2, 27); display_.print(F("300 prompts each"));
    display_.setCursor(2, 40); display_.print(F("Total base prompts ")); display_.print(promptDatabaseSize());
    display_.setCursor(2, 53); display_.print(F("D1 narrative debug"));
  }
  display_.display();
}

void GrowingTogetherSystem::showReflection(const GrowingTogetherContext &ctx) const {
  uint8_t dominant = 0;
  for (uint8_t i = 1; i < GT_STYLE_COUNT; ++i) if (reflectionCount_[i] > reflectionCount_[dominant]) dominant = i;
  char text[420] = "Your parenting pattern is currently ";
  static const char *const styles[GT_STYLE_COUNT] = {
    "warm listening", "comfort", "calm coaching", "clear boundaries",
    "encouragement", "supporting independence", "dismissal", "mischief"
  };
  appendText(text, sizeof(text), styles[dominant]);
  appendText(text, sizeof(text), ". The relationship is ");
  appendText(text, sizeof(text), RELATIONSHIP_NAMES[relationshipState_]);
  appendText(text, sizeof(text), ". Trust is "); appendNumber(text, sizeof(text), ctx.ownerTrust);
  appendText(text, sizeof(text), ", consent respect is "); appendNumber(text, sizeof(text), consentRespect_);
  appendText(text, sizeof(text), ", privacy trust is "); appendNumber(text, sizeof(text), privacyTrust_);
  appendText(text, sizeof(text), ", and promise consistency is "); appendNumber(text, sizeof(text), ruleConsistency_);
  appendText(text, sizeof(text), ". This page describes patterns, not a permanent label. Future choices can change it.");
  showScrollableText("PARENT REFLECTION", text, "D1 close");
}

void GrowingTogetherSystem::showDebug(const GrowingTogetherContext &ctx) const {
  char text[700] = "Last selected feature: ";
  appendText(text, sizeof(text), lastFeature_ < GT_FEATURE_COUNT ? featureName(lastFeature_) : "none");
  appendText(text, sizeof(text), ". Weight: "); appendNumber(text, sizeof(text), lastWeight_);
  appendText(text, sizeof(text), ". Rejected feature mask: "); appendNumber(text, sizeof(text), static_cast<int>(lastRejectedMask_));
  appendText(text, sizeof(text), ". Active arc: "); appendText(text, sizeof(text), activeArcType_ < GT_FEATURE_COUNT ? featureName(activeArcType_) : "none");
  appendText(text, sizeof(text), ", step "); appendNumber(text, sizeof(text), activeArcStep_);
  appendText(text, sizeof(text), ". Current stage "); appendNumber(text, sizeof(text), ctx.stage);
  appendText(text, sizeof(text), ", bond "); appendNumber(text, sizeof(text), ctx.ownerBond);
  appendText(text, sizeof(text), ", trust "); appendNumber(text, sizeof(text), ctx.ownerTrust);
  appendText(text, sizeof(text), ", family mood "); appendNumber(text, sizeof(text), ctx.familyMood);
  appendText(text, sizeof(text), ", garden health "); appendNumber(text, sizeof(text), ctx.gardenHealth);
  appendText(text, sizeof(text), ", companion bond "); appendNumber(text, sizeof(text), ctx.companionBond);
  appendText(text, sizeof(text), ". Every feature bank has 12 scenario roots x 5 openings x 5 endings = 300 base prompts. Live context and multi-day state add more variations.");
  showScrollableText("NARRATIVE DEBUG", text, "D1 close");
}

void GrowingTogetherSystem::runHub(const GrowingTogetherContext &ctx) {
  const uint8_t pages = 8;
  uint8_t page = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_);
    const bool s = digitalRead(btnS_);
    const bool r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { page = page == 0 ? pages - 1 : page - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { page = static_cast<uint8_t>((page + 1) % pages); clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) {
      clickSound(); waitButtonRelease();
      if (page == 0) {
        scheduleManual(ctx);
        return;
      }
      if (page == 1) showReflection(ctx);
      else if (page == 2) {
        char text[520] = "";
        if (!promiseCount()) appendText(text, sizeof(text), "No active promises. Conversations can create specific promises with deadlines.");
        for (uint8_t i = 0; i < GT_PROMISE_SLOTS; ++i) if (promiseStatus_[i] == 1) {
          appendText(text, sizeof(text), PROMISE_NAMES[promiseType_[i] % 14]);
          appendText(text, sizeof(text), " due day "); appendNumber(text, sizeof(text), promiseDueDay_[i]); appendText(text, sizeof(text), ".\n");
        }
        showScrollableText("PROMISES", text, "D1 close");
      } else if (page == 7) showDebug(ctx);
      else {
        char text[500] = "This page updates automatically through conversations and daily life. Use D0 and D2 to browse the hub. The detailed narrative appears when the relevant situation occurs.";
        showScrollableText("LIFE JOURNAL", text, "D1 close");
      }
    }
    oldL = l; oldS = s; oldR = r;
    drawHubPage(ctx, page, pages);
    lastInputAt = millis();
    delay(20);
  }
}

void GrowingTogetherSystem::developerMax() {
  for (uint8_t &v : values_) v = 100;
  for (uint8_t &v : valueConsistency_) v = 100;
  relationshipState_ = REL_DEEP_TRUST;
  ruptureScore_ = 0;
  repairProgress_ = 100;
  for (uint8_t &v : ritualStreak_) v = 99;
  consentRespect_ = affectionBond_ = 100;
  keepsakeMask_ = 0x00FFFFFFUL;
  for (uint8_t &v : keepsakeCount_) v = 9;
  languageLevel_ = 5; vocabularyScore_ = 100;
  schoolEffort_ = schoolCooperation_ = schoolConfidence_ = 100;
  choreFairness_ = coparentAgreement_ = privacyTrust_ = secretSafety_ = 100;
  apologySkill_ = repairSkill_ = 100;
  traditionMask_ = 0x00FFU;
  householdRulesMask_ = 0x0FFFU;
  ruleConsistency_ = 100;
  for (uint8_t &v : hobbyBond_) v = 100;
  milestoneMask_ = 0x00FFFFFFUL;
  nextPromptTick_ = 1;
  markSaveDirty();
}

void GrowingTogetherSystem::developerReset() { reset(); }
