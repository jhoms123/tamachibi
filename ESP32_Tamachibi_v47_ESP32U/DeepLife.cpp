#include "DeepLife.h"
#include "LifeWorld.h"
#include "Botany.h"
#include "DeepLifeDialogueData.h"
#include <stddef.h>
#include <string.h>

extern byte health, full, energy, happy, clean, discipline, stage, temperament;
extern byte ownerBond, ownerTrust;
extern byte gardenLevel, gardenHarvests;
extern LifeWorldSystem lifeWorld;
extern BotanySystem botany;
extern byte speciesType, partnerId, partnerBond, outfit, roomStyle, habitat;
extern byte favoriteGameId, difficultyMode, rescueMode, soundMode, brightnessLevel;
extern byte skillSmart, skillStyle, skillKind, skillFit;
extern byte craftMaterials[];
extern unsigned int coins, generation;
extern unsigned long ageTicks;
extern bool sleeping, sick, dead;
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
extern byte inheritedTemperament, inheritedFavoriteGame, inheritedBodyType, inheritedTrait;

namespace {
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

const char *const HUB_NAMES[18] = {
  "Today", "Life Stories", "Rival", "Friend Groups",
  "Family Life", "Children", "Adventures", "Creatures",
  "Fossils", "Fishing", "Seasons", "Achievements",
  "Museum", "New Game Plus", "Accessibility", "Tutorial",
  "Economy", "Diagnostics"
};

const char *const EMOTION_NAMES[10] = {
  "Calm", "Excited", "Nervous", "Proud", "Lonely",
  "Jealous", "Embarrassed", "Curious", "Grumpy", "Inspired"
};

const char *const GROUP_NAMES[8] = {
  "School Friends", "Work Friends", "Garden Club", "Music Group",
  "Sports Team", "Science Club", "Mischief Crew", "Cooking Club"
};

const char *const WILD_NAMES[DEEP_WILD_COUNT] = {
  "Moss Mite", "Pebble Pup", "Moon Moth", "Shell Sprite",
  "Rain Wisp", "Cave Nibbler", "Snow Puff", "Sun Hopper",
  "Leafling", "Star Minnow", "Clock Beetle", "Cloud Kit",
  "Glow Worm", "Pond Blob", "Dune Finch", "Tiny Comet"
};

const char *const FOSSIL_NAMES[DEEP_FOSSIL_COUNT] = {
  "Spiral Shell", "Ancient Paw", "Sky Feather", "Moon Tooth",
  "Root Crown", "Crystal Spine", "Tiny Horn", "Star Egg"
};

const char *const FISH_NAMES[DEEP_FISH_COUNT] = {
  "Puddle Guppy", "Ribbon Fish", "Moon Carp", "Bubble Eel",
  "Glass Minnow", "Leaf Koi", "Snowfin", "Sun Perch",
  "Cave Tetra", "Star Ray", "Royal Betta", "Dream Whale"
};

const char *const TITLE_NAMES[DEEP_TITLE_COUNT] = {
  "New Caretaker", "Kind Neighbor", "Game Learner", "City Helper",
  "Rival Friend", "Trail Scout", "Wild Whisperer", "Fossil Keeper",
  "Master Angler", "Season Watcher", "Family Mentor", "Museum Curator",
  "Festival Star", "City Legend", "Legacy Keeper", "World Guardian"
};

const char *const EVENT_TITLES[16] = {
  "Lost Package", "Sudden Storm", "Rival Challenge", "Sick Neighbor",
  "Mystery Egg", "Garden Trouble", "Club Invitation", "Family Picnic",
  "Stray Companion", "Work Emergency", "Old Map", "Festival Practice",
  "Sibling Dispute", "Rare Visitor", "Broken Toy", "Ancestor Letter"
};

constexpr uint8_t ORIGINAL_EVENT_COUNT = 16;
constexpr uint8_t PROCEDURAL_EVENT_CATEGORY_COUNT = 14;
constexpr uint8_t PROCEDURAL_EVENTS_PER_CATEGORY = 16;
constexpr uint8_t TOTAL_LIFE_EVENT_COUNT = ORIGINAL_EVENT_COUNT + PROCEDURAL_EVENT_CATEGORY_COUNT * PROCEDURAL_EVENTS_PER_CATEGORY;

static bool classicEventAllowed(uint8_t event, uint8_t stageValue, uint8_t childCount) {
  if (stageValue <= 1 || event >= ORIGINAL_EVENT_COUNT) return false;
  if (event == 9 && stageValue < 4) return false;       // Work Emergency: adult/elder only.
  if (event == 12 && (stageValue < 4 || childCount < 2)) return false; // Parent mediation.
  return true;
}

const char *const PROCEDURAL_CATEGORY_NAMES[14] = {
  "BABY MOMENTS", "CHILD DAYS", "TEEN LIFE", "ADULT PURPOSE", "ELDER LEGACY", "CARETAKER BOND", "PARTNER LIFE", "FAMILY MOMENT", "COMPANION PET", "GARDEN STORY", "PLANT ROOM", "WEATHER TALE", "HEALTH & MOOD", "COMMUNITY LIFE"
};

const char *const PROCEDURAL_EVENT_TITLES[PROCEDURAL_EVENT_CATEGORY_COUNT][PROCEDURAL_EVENTS_PER_CATEGORY] = {
  { "First Laugh", "Blanket Fort", "Bottle Refusal", "Nap Trouble", "Tiny Footsteps", "Bath Splash", "New Sound", "Lost Pacifier", "Night Fright", "Mirror Friend", "Soft Toy Choice", "First Crawl", "High Chair Mess", "Doorway Wonder", "Clap Along", "Sleepy Cuddle" },
  { "School Project", "Playground Dare", "Lost Crayon", "Class Pet", "Library Card", "Show and Tell", "Rainy Recess", "Secret Club", "Lunch Trade", "Science Seed", "Art Contest", "New Classmate", "Homework Help", "Treehouse Plan", "Music Lesson", "Field Trip" },
  { "Style Debate", "Rival Rumor", "Club Election", "Late Curfew", "First Crush", "Big Audition", "Team Tryout", "Identity Journal", "Friend Fallout", "Career Shadow", "Solo Journey", "Public Speech", "Secret Talent", "Peer Pressure", "Future Choice", "Graduation Plan" },
  { "Busy Shift", "New Contract", "Work Mistake", "Team Conflict", "Promotion Talk", "Client Request", "Night Shift", "Career Change", "Mentor Request", "Big Deadline", "Office Party", "Skill Course", "Side Project", "Work Travel", "Ethics Choice", "Dream Venture" },
  { "Old Photograph", "Family Recipe", "Garden Lesson", "Legacy Speech", "Quiet Morning", "Heirloom Repair", "Story Circle", "Final Project", "Old Rival Visit", "City Tribute", "Wisdom Letter", "Memory Walk", "Grandchild Ask", "Ancient Song", "Retirement Plan", "Starry Farewell" },
  { "Forgotten Hello", "Long Cuddle", "Quiet Distance", "Shared Snack", "Care Promise", "Favorite Game", "Trust Test", "Bedtime Talk", "Unexpected Gift", "Missed Routine", "Comfort Needed", "Proud Moment", "Hard Goodbye", "Welcome Home", "Private Joke", "Loyal Choice" },
  { "Anniversary Plan", "Chore Dispute", "Dinner Date", "Money Talk", "Old Friend", "Parenting Plan", "Career Balance", "Quiet Evening", "Jealous Moment", "Shared Hobby", "Family Visit", "Big Decision", "Home Repair", "Travel Dream", "Apology Note", "Promise Renewed" },
  { "Sibling Toy", "Bedtime Story", "School Worry", "Shared Chore", "Talent Show", "Broken Keepsake", "Secret Snack", "Parent Choice", "Sibling Praise", "Family Game", "First Lesson", "Rainy Tantrum", "Lost Homework", "Big Question", "Heirloom Turn", "Group Hug" },
  { "Pet Won't Eat", "Mud Tracks", "New Trick", "Hidden Treasure", "Vet Scare", "Jealous Pet", "Runaway Scare", "Favorite Spot", "Pet Birthday", "Strange Bark", "Night Watch", "Garden Dig", "Companion Race", "Cozy Nap", "Wild Friend", "Loyal Rescue" },
  { "Dry Seedlings", "First Bud", "Garden Pests", "Overgrown Vines", "Rare Blossom", "Heavy Harvest", "Broken Trellis", "Mystery Sprout", "Compost Heat", "Bird Visitors", "Moon Bloom", "Flooded Bed", "Prize Vegetable", "Weed Invasion", "Seed Exchange", "Autumn Harvest" },
  { "Wilting Leaves", "New Cutting", "Pot Cracked", "Window Light", "Root Bound", "Tiny Flowers", "Moldy Soil", "Rare Fragrance", "Fallen Shelf", "Plant Gift", "Leaf Pattern", "Warm Draft", "Water Leak", "New Planter", "Night Bloom", "Indoor Jungle" },
  { "Sudden Rainbow", "Thick Fog", "Heat Wave", "First Snow", "Meteor Shower", "Thunder Night", "Windy Morning", "Spring Rain", "Dry Spell", "Frost Warning", "Cloud Shapes", "Long Sunset", "Hail Surprise", "Moon Halo", "Storm Cleanup", "Season Turn" },
  { "Scratchy Throat", "Tired Eyes", "Stomach Flutter", "Restless Night", "Sore Muscles", "Low Appetite", "Happy Burst", "Lonely Morning", "Nervous Habit", "Proud Glow", "Jealous Mood", "Grumpy Spell", "Inspired Rush", "Embarrassed Fall", "Quiet Recovery", "Health Check" },
  { "Park Cleanup", "Missing Poster", "Cafe Rush", "School Fund", "Clinic Queue", "Market Dispute", "Museum Find", "Festival Booth", "Lost Tourist", "Neighbor Move", "Street Garden", "Power Outage", "Town Meeting", "Charity Drive", "Public Mural", "City Award" },
};

const char *const DAILY_GOALS[8] = {
  "Care twice", "Play a game", "Visit or talk", "Study or train",
  "Cook or feed", "Clean or heal", "Explore", "Family time"
};
const char *const WEEKLY_GOALS[8] = {
  "Win 3 games", "Help 4 times", "Explore 3 times", "Earn 20G",
  "Meet club twice", "Catch 3 fish", "Find 2 relics", "Mentor 3 times"
};

const char *const ACHIEVEMENT_NAMES[64] = {
  "Triple Rival Win", "Rival Best Friend", "Club Regular", "Golden Marriage",
  "Growing Family", "Patient Mentor", "Trail Veteran", "Field Observer",
  "Creature Friend", "Fossil Scholar", "Fish Collector", "Weather Watcher",
  "Museum Builder", "Earned 250G", "Five Generations", "Legacy Ascended",
  "Secret Evolution", "Full Event Journal", "Twenty Awards", "Thirty-Two Awards",
  "Daily Goal", "Weekly Goal", "Returned Package", "Honored Elder",
  "Lasting Marriage", "Anniversary", "First Child", "First Donation",
  "Tutorial Graduate", "All Creatures Seen", "All Creatures Friend", "All Fossils Built",
  "All Fossils Donated", "All Fish Found", "Master Angler", "Deep Explorer",
  "Flawless Trail", "Top Rival Rank", "Rival Respect", "Club Leader",
  "Club Socialite", "Perfect Harmony", "Happy Household", "Three Children",
  "Beloved Parent", "Close Siblings", "Master Teacher", "Season Collector",
  "Rare Weather Expert", "Earned 1000G", "Hundred Trades", "Spent 500G",
  "Grand Museum", "Ten Generations", "Legacy Tier Three", "Four Secret Forms",
  "All Secret Forms", "World Guardian", "Badge Collector", "Story Collector",
  "Many Life Choices", "Help Master", "Balanced Life", "Complete Caretaker"
};

const uint8_t SECRET_SPECIES[8] = {42, 43, 44, 45, 46, 47, 48, 49};

uint8_t popcount32(uint32_t v) {
  uint8_t count = 0;
  while (v) { count += static_cast<uint8_t>(v & 1U); v >>= 1U; }
  return count;
}

uint8_t popcount16(uint16_t v) {
  uint8_t count = 0;
  while (v) { count += static_cast<uint8_t>(v & 1U); v >>= 1U; }
  return count;
}
}

DeepLifeSystem::DeepLifeSystem(size_t storageBase, GameDisplay &display,
                               uint8_t buttonLeft, uint8_t buttonSelect,
                               uint8_t buttonRight)
    : storeA_(storageBase), storeB_(storageBase + sizeof(DeepLifeSave)),
      display_(display), btnL_(buttonLeft), btnS_(buttonSelect), btnR_(buttonRight) {
  reset();
}

void DeepLifeSystem::reset() {
  sequence_ = 0;
  emotion_ = 0; emotionIntensity_ = 1; emotionMinutes_ = 0; lastEmotionCause_ = 0;
  rivalSpecies_ = 0; rivalPersonality_ = 0; rivalBond_ = 5; rivalRespect_ = 5;
  rivalRank_ = 0; rivalWins_ = 0; rivalLosses_ = 0; rivalStory_ = 0;
  groupId_ = 255; groupLevel_ = 0; groupMeetings_ = 0; groupQuest_ = 0;
  partnerHarmony_ = 50; familyMood_ = 50; argumentState_ = 0; anniversaryDay_ = 0;
  childCount_ = 0; chosenHeir_ = 255; mentorSkill_ = 255; mentorProgress_ = 0;
  memset(childSpecies_, 255, sizeof(childSpecies_));
  memset(childPersonality_, 0, sizeof(childPersonality_));
  memset(childTalent_, 0, sizeof(childTalent_));
  memset(childBond_, 0, sizeof(childBond_));
  memset(childAge_, 0, sizeof(childAge_));
  memset(siblingBond_, 50, sizeof(siblingBond_));
  legacySiblingCount_ = 0; memset(legacySiblingSpecies_, 255, sizeof(legacySiblingSpecies_));
  memset(legacySiblingBond_, 0, sizeof(legacySiblingBond_)); grandparentSpecies_ = 255; grandparentBond_ = 0;
  pendingEvent_ = 255; pendingEventChoice_ = 255; pendingEventDay_ = 65535;
  storyFlags_ = 0; eventLogHead_ = 0; eventLogCount_ = 0;
  memset(eventLogType_, 0, sizeof(eventLogType_));
  memset(eventLogValue_, 0, sizeof(eventLogValue_));
  memset(eventLogDay_, 0, sizeof(eventLogDay_));
  secretHintMask_ = 0; secretUnlockMask_ = 0; wildSeenMask_ = 0; wildFriendMask_ = 0;
  memset(fossilFragments_, 0, sizeof(fossilFragments_));
  fossilCompleteMask_ = 0; fossilDonatedMask_ = 0;
  fishSeenMask_ = 0; memset(fishBest_, 0, sizeof(fishBest_)); fishingLevel_ = 0; fishingXp_ = 0;
  expeditionSeed_ = 1; expeditionWins_ = 0; expeditionLosses_ = 0; expeditionDepth_ = 0;
  season_ = 0; rareWeather_ = 0; weatherSeenMask_ = 0; seasonalCollectionMask_ = 0;
  achievementsLo_ = 0; achievementsHi_ = 0; badgeMask_ = 0; titleId_ = 0;
  museumLevel_ = 0; museumDonations_ = 0; newGamePlus_ = 0;
  accessibilityFlags_ = 0x20U; tutorialStep_ = 0; tutorialDone_ = 0; infoReason_ = 0;
  dailyGoal_ = 0; dailyProgress_ = 0; dailyTarget_ = 2; dailyGoalDay_ = 65535;
  weeklyGoal_ = 0; weeklyProgress_ = 0; weeklyTarget_ = 3; weeklyGoalWeek_ = 65535;
  economyEarned_ = 0; economySpent_ = 0; economyTransactions_ = 0;
  economyDayEarned_ = 0; economyDaySpent_ = 0; economyDay_ = 65535;
  saveHealthChecks_ = 0; migrationCount_ = 0; lastSaveHealth_ = 100;
}

uint32_t DeepLifeSystem::checksum(const DeepLifeSave &save) const {
  const uint8_t *p = reinterpret_cast<const uint8_t *>(&save);
  const size_t n = offsetof(DeepLifeSave, checksum);
  uint32_t value = 2166136261UL;
  for (size_t i = 0; i < n; ++i) { value ^= p[i]; value *= 16777619UL; }
  return value;
}

bool DeepLifeSystem::valid(const DeepLifeSave &save) const {
  return save.magic == DEEPLIFE_SAVE_MAGIC && save.version == DEEPLIFE_SAVE_VERSION &&
         save.checksum == checksum(save);
}

void DeepLifeSystem::fillSave(DeepLifeSave &s) const {
  memset(&s, 0, sizeof(s));
  s.magic = DEEPLIFE_SAVE_MAGIC; s.version = DEEPLIFE_SAVE_VERSION; s.sequence = sequence_ + 1;
  s.emotion = emotion_; s.emotionIntensity = emotionIntensity_; s.emotionMinutes = emotionMinutes_; s.lastEmotionCause = lastEmotionCause_;
  s.rivalSpecies = rivalSpecies_; s.rivalPersonality = rivalPersonality_; s.rivalBond = rivalBond_; s.rivalRespect = rivalRespect_; s.rivalRank = rivalRank_; s.rivalWins = rivalWins_; s.rivalLosses = rivalLosses_; s.rivalStory = rivalStory_;
  s.groupId = groupId_; s.groupLevel = groupLevel_; s.groupMeetings = groupMeetings_; s.groupQuest = groupQuest_;
  s.partnerHarmony = partnerHarmony_; s.familyMood = familyMood_; s.argumentState = argumentState_; s.anniversaryDay = anniversaryDay_;
  s.childCount = childCount_; s.chosenHeir = chosenHeir_;
  memcpy(s.childSpecies, childSpecies_, sizeof(childSpecies_)); memcpy(s.childPersonality, childPersonality_, sizeof(childPersonality_)); memcpy(s.childTalent, childTalent_, sizeof(childTalent_)); memcpy(s.childBond, childBond_, sizeof(childBond_)); memcpy(s.childAge, childAge_, sizeof(childAge_)); memcpy(s.siblingBond, siblingBond_, sizeof(siblingBond_));
  s.mentorSkill = mentorSkill_; s.mentorProgress = mentorProgress_;
  s.legacySiblingCount = legacySiblingCount_; memcpy(s.legacySiblingSpecies, legacySiblingSpecies_, sizeof(legacySiblingSpecies_)); memcpy(s.legacySiblingBond, legacySiblingBond_, sizeof(legacySiblingBond_)); s.grandparentSpecies = grandparentSpecies_; s.grandparentBond = grandparentBond_;
  s.pendingEvent = pendingEvent_; s.pendingEventChoice = pendingEventChoice_; s.pendingEventDay = pendingEventDay_; s.storyFlags = storyFlags_; s.eventLogHead = eventLogHead_; s.eventLogCount = eventLogCount_;
  memcpy(s.eventLogType, eventLogType_, sizeof(eventLogType_)); memcpy(s.eventLogValue, eventLogValue_, sizeof(eventLogValue_)); memcpy(s.eventLogDay, eventLogDay_, sizeof(eventLogDay_));
  s.secretHintMask = secretHintMask_; s.secretUnlockMask = secretUnlockMask_; s.wildSeenMask = wildSeenMask_; s.wildFriendMask = wildFriendMask_;
  memcpy(s.fossilFragments, fossilFragments_, sizeof(fossilFragments_)); s.fossilCompleteMask = fossilCompleteMask_; s.fossilDonatedMask = fossilDonatedMask_;
  s.fishSeenMask = fishSeenMask_; memcpy(s.fishBest, fishBest_, sizeof(fishBest_)); s.fishingLevel = fishingLevel_; s.fishingXp = fishingXp_;
  s.expeditionSeed = expeditionSeed_; s.expeditionWins = expeditionWins_; s.expeditionLosses = expeditionLosses_; s.expeditionDepth = expeditionDepth_;
  s.season = season_; s.rareWeather = rareWeather_; s.weatherSeenMask = weatherSeenMask_; s.seasonalCollectionMask = seasonalCollectionMask_;
  s.achievementsLo = achievementsLo_; s.achievementsHi = achievementsHi_; s.badgeMask = badgeMask_; s.titleId = titleId_; s.museumLevel = museumLevel_; s.museumDonations = museumDonations_; s.newGamePlus = newGamePlus_;
  s.accessibilityFlags = accessibilityFlags_; s.tutorialStep = tutorialStep_; s.tutorialDone = tutorialDone_; s.infoReason = infoReason_;
  s.dailyGoal = dailyGoal_; s.dailyProgress = dailyProgress_; s.dailyTarget = dailyTarget_; s.dailyGoalDay = dailyGoalDay_; s.weeklyGoal = weeklyGoal_; s.weeklyProgress = weeklyProgress_; s.weeklyTarget = weeklyTarget_; s.weeklyGoalWeek = weeklyGoalWeek_;
  s.economyEarned = economyEarned_; s.economySpent = economySpent_; s.economyTransactions = economyTransactions_; s.economyDayEarned = economyDayEarned_; s.economyDaySpent = economyDaySpent_; s.economyDay = economyDay_;
  s.saveHealthChecks = saveHealthChecks_ + 1; s.migrationCount = migrationCount_; s.lastSaveHealth = lastSaveHealth_;
  s.checksum = checksum(s);
}

void DeepLifeSystem::applySave(const DeepLifeSave &s) {
  sequence_ = s.sequence;
  emotion_ = s.emotion % 10; emotionIntensity_ = constrain(s.emotionIntensity, 1, 10); emotionMinutes_ = s.emotionMinutes; lastEmotionCause_ = s.lastEmotionCause;
  rivalSpecies_ = s.rivalSpecies % 50; rivalPersonality_ = s.rivalPersonality % 8; rivalBond_ = min<uint8_t>(s.rivalBond, 100); rivalRespect_ = min<uint8_t>(s.rivalRespect, 100); rivalRank_ = min<uint8_t>(s.rivalRank, 9); rivalWins_ = s.rivalWins; rivalLosses_ = s.rivalLosses; rivalStory_ = min<uint8_t>(s.rivalStory, 5);
  groupId_ = s.groupId < 8 ? s.groupId : 255; groupLevel_ = min<uint8_t>(s.groupLevel, 9); groupMeetings_ = s.groupMeetings; groupQuest_ = s.groupQuest;
  partnerHarmony_ = min<uint8_t>(s.partnerHarmony, 100); familyMood_ = min<uint8_t>(s.familyMood, 100); argumentState_ = min<uint8_t>(s.argumentState, 3); anniversaryDay_ = s.anniversaryDay;
  childCount_ = min<uint8_t>(s.childCount, DEEP_CHILD_COUNT); chosenHeir_ = s.chosenHeir < childCount_ ? s.chosenHeir : 255;
  memcpy(childSpecies_, s.childSpecies, sizeof(childSpecies_)); memcpy(childPersonality_, s.childPersonality, sizeof(childPersonality_)); memcpy(childTalent_, s.childTalent, sizeof(childTalent_)); memcpy(childBond_, s.childBond, sizeof(childBond_)); memcpy(childAge_, s.childAge, sizeof(childAge_)); memcpy(siblingBond_, s.siblingBond, sizeof(siblingBond_));
  mentorSkill_ = s.mentorSkill; mentorProgress_ = s.mentorProgress;
  legacySiblingCount_ = min<uint8_t>(s.legacySiblingCount, DEEP_LEGACY_SIBLING_COUNT); memcpy(legacySiblingSpecies_, s.legacySiblingSpecies, sizeof(legacySiblingSpecies_)); memcpy(legacySiblingBond_, s.legacySiblingBond, sizeof(legacySiblingBond_)); grandparentSpecies_ = s.grandparentSpecies < 50 ? s.grandparentSpecies : 255; grandparentBond_ = min<uint8_t>(s.grandparentBond, 100);
  pendingEvent_ = s.pendingEvent < TOTAL_LIFE_EVENT_COUNT ? s.pendingEvent : 255; pendingEventChoice_ = s.pendingEventChoice; pendingEventDay_ = s.pendingEventDay; storyFlags_ = s.storyFlags; eventLogHead_ = s.eventLogHead % DEEP_EVENT_LOG_COUNT; eventLogCount_ = min<uint8_t>(s.eventLogCount, DEEP_EVENT_LOG_COUNT);
  memcpy(eventLogType_, s.eventLogType, sizeof(eventLogType_)); memcpy(eventLogValue_, s.eventLogValue, sizeof(eventLogValue_)); memcpy(eventLogDay_, s.eventLogDay, sizeof(eventLogDay_));
  secretHintMask_ = s.secretHintMask; secretUnlockMask_ = s.secretUnlockMask; wildSeenMask_ = s.wildSeenMask; wildFriendMask_ = s.wildFriendMask;
  memcpy(fossilFragments_, s.fossilFragments, sizeof(fossilFragments_)); for (uint8_t &v : fossilFragments_) v = min<uint8_t>(v, 4); fossilCompleteMask_ = s.fossilCompleteMask; fossilDonatedMask_ = s.fossilDonatedMask;
  fishSeenMask_ = s.fishSeenMask; memcpy(fishBest_, s.fishBest, sizeof(fishBest_)); fishingLevel_ = min<uint8_t>(s.fishingLevel, 9); fishingXp_ = s.fishingXp;
  expeditionSeed_ = s.expeditionSeed; expeditionWins_ = s.expeditionWins; expeditionLosses_ = s.expeditionLosses; expeditionDepth_ = min<uint8_t>(s.expeditionDepth, 9);
  season_ = s.season % 4; rareWeather_ = s.rareWeather % 8; weatherSeenMask_ = s.weatherSeenMask; seasonalCollectionMask_ = s.seasonalCollectionMask;
  achievementsLo_ = s.achievementsLo; achievementsHi_ = s.achievementsHi; badgeMask_ = s.badgeMask; titleId_ = s.titleId % DEEP_TITLE_COUNT; museumLevel_ = min<uint8_t>(s.museumLevel, 9); museumDonations_ = s.museumDonations; newGamePlus_ = min<uint8_t>(s.newGamePlus, 9);
  accessibilityFlags_ = s.accessibilityFlags; tutorialStep_ = min<uint8_t>(s.tutorialStep, 15); tutorialDone_ = s.tutorialDone ? 1 : 0; infoReason_ = s.infoReason;
  dailyGoal_ = s.dailyGoal % 8; dailyProgress_ = s.dailyProgress; dailyTarget_ = max<uint8_t>(1, s.dailyTarget); dailyGoalDay_ = s.dailyGoalDay;
  weeklyGoal_ = s.weeklyGoal % 8; weeklyProgress_ = s.weeklyProgress; weeklyTarget_ = max<uint8_t>(1, s.weeklyTarget); weeklyGoalWeek_ = s.weeklyGoalWeek;
  economyEarned_ = s.economyEarned; economySpent_ = s.economySpent; economyTransactions_ = s.economyTransactions; economyDayEarned_ = s.economyDayEarned; economyDaySpent_ = s.economyDaySpent; economyDay_ = s.economyDay;
  saveHealthChecks_ = s.saveHealthChecks; migrationCount_ = s.migrationCount; lastSaveHealth_ = s.lastSaveHealth;
}

bool DeepLifeSystem::load() {
  DeepLifeSave a = storeA_.read(), b = storeB_.read();
  const bool va = valid(a), vb = valid(b);
  if (!va && !vb) { reset(); lastSaveHealth_ = 50; return false; }
  applySave(va && vb ? (a.sequence >= b.sequence ? a : b) : (va ? a : b));
  lastSaveHealth_ = va && vb ? 100 : 75;
  ++saveHealthChecks_;
  return true;
}

void DeepLifeSystem::saveNow() {
  DeepLifeSave s; fillSave(s);
  if (s.sequence & 1UL) storeA_.write(s); else storeB_.write(s);
  sequence_ = s.sequence; saveHealthChecks_ = s.saveHealthChecks;
}

void DeepLifeSystem::changeByte(uint8_t &value, int amount, uint8_t maximum) {
  value = static_cast<uint8_t>(constrain(static_cast<int>(value) + amount, 0, maximum));
}

void DeepLifeSystem::setEmotion(uint8_t emotion, uint8_t intensity, uint8_t cause) {
  emotion_ = emotion % 10; emotionIntensity_ = constrain(intensity, 1, 10);
  emotionMinutes_ = static_cast<uint16_t>(30 + emotionIntensity_ * 20); lastEmotionCause_ = cause;
}

void DeepLifeSystem::addEventLog(uint8_t type, uint8_t value, uint16_t day) {
  const uint8_t i = eventLogHead_ % DEEP_EVENT_LOG_COUNT;
  eventLogType_[i] = type; eventLogValue_[i] = value; eventLogDay_[i] = day;
  eventLogHead_ = static_cast<uint8_t>((eventLogHead_ + 1) % DEEP_EVENT_LOG_COUNT);
  if (eventLogCount_ < DEEP_EVENT_LOG_COUNT) ++eventLogCount_;
}

void DeepLifeSystem::setAchievement(uint8_t achievement) {
  if (achievement < 32) achievementsLo_ |= (1UL << achievement);
  else if (achievement < 64) achievementsHi_ |= (1UL << (achievement - 32));
}

bool DeepLifeSystem::hasAchievement(uint8_t achievement) const {
  if (achievement < 32) return (achievementsLo_ & (1UL << achievement)) != 0;
  if (achievement < 64) return (achievementsHi_ & (1UL << (achievement - 32))) != 0;
  return false;
}

uint8_t DeepLifeSystem::achievementCount() const {
  return static_cast<uint8_t>(popcount32(achievementsLo_) + popcount32(achievementsHi_));
}

void DeepLifeSystem::evaluateAchievements() {
  if (rivalWins_ >= 3) setAchievement(0);
  if (rivalBond_ >= 60) setAchievement(1);
  if (groupLevel_ >= 3) setAchievement(2);
  if (partnerHarmony_ >= 80) setAchievement(3);
  if (childCount_ >= 2) setAchievement(4);
  if (mentorProgress_ >= 10) setAchievement(5);
  if (expeditionWins_ >= 5) setAchievement(6);
  if (popcount32(wildSeenMask_) >= 8) setAchievement(7);
  if (popcount32(wildFriendMask_) >= 4) setAchievement(8);
  if (popcount16(fossilCompleteMask_) >= 4) setAchievement(9);
  if (popcount16(fishSeenMask_) >= 8) setAchievement(10);
  if (weatherSeenMask_ == 0xFFFFU) setAchievement(11);
  if (museumLevel_ >= 3) setAchievement(12);
  if (economyEarned_ >= 250) setAchievement(13);
  if (generation >= 5) setAchievement(14);
  if (newGamePlus_ > 0) setAchievement(15);
  if (secretUnlockMask_ != 0) setAchievement(16);
  if (eventLogCount_ >= DEEP_EVENT_LOG_COUNT) setAchievement(17);
  if (achievementCount() >= 20) setAchievement(18);
  if (achievementCount() >= 32) setAchievement(19);
  // 20-28 are awarded directly by daily/weekly goals, story choices, elder
  // archives, anniversaries, births, donations, and tutorial completion.
  if (popcount32(wildSeenMask_) >= DEEP_WILD_COUNT) setAchievement(29);
  if (popcount32(wildFriendMask_) >= DEEP_WILD_COUNT) setAchievement(30);
  if (fossilCompleteMask_ == 0xFFU) setAchievement(31);
  if (fossilDonatedMask_ == 0xFFU) setAchievement(32);
  if ((fishSeenMask_ & 0x0FFFU) == 0x0FFFU) setAchievement(33);
  if (fishingLevel_ >= 9) setAchievement(34);
  if (expeditionWins_ >= 15) setAchievement(35);
  if (expeditionWins_ >= 5 && expeditionLosses_ == 0) setAchievement(36);
  if (rivalRank_ >= 9) setAchievement(37);
  if (rivalRespect_ >= 90) setAchievement(38);
  if (groupLevel_ >= 9) setAchievement(39);
  if (groupMeetings_ >= 25) setAchievement(40);
  if (partnerHarmony_ >= 100) setAchievement(41);
  if (familyMood_ >= 100) setAchievement(42);
  if (childCount_ >= 3) setAchievement(43);
  bool allChildBonds = childCount_ > 0, allSiblingBonds = childCount_ > 1;
  for (uint8_t i = 0; i < childCount_; ++i) {
    if (childBond_[i] < 80) allChildBonds = false;
    if (childCount_ > 1 && siblingBond_[i] < 80) allSiblingBonds = false;
  }
  if (allChildBonds) setAchievement(44);
  if (allSiblingBonds) setAchievement(45);
  if (mentorProgress_ >= 25) setAchievement(46);
  if (seasonalCollectionMask_ == 0xFFFFU) setAchievement(47);
  if (popcount16(weatherSeenMask_) >= 8) setAchievement(48);
  if (economyEarned_ >= 1000) setAchievement(49);
  if (economyTransactions_ >= 100) setAchievement(50);
  if (economySpent_ >= 500) setAchievement(51);
  if (museumLevel_ >= 5) setAchievement(52);
  if (generation >= 10) setAchievement(53);
  if (newGamePlus_ >= 3) setAchievement(54);
  if (popcount16(secretUnlockMask_) >= 4) setAchievement(55);
  if ((secretUnlockMask_ & 0x00FFU) == 0x00FFU) setAchievement(56);
  if (popcount16(badgeMask_) >= 8) setAchievement(58);
  if (popcount32(storyFlags_) >= 8) setAchievement(59);
  if (popcount32(storyFlags_) >= 16) setAchievement(60);
  if (skillKind >= 15 && groupLevel_ >= 5) setAchievement(61);
  if (health >= 8 && full >= 8 && energy >= 8 && happy >= 8 && clean >= 8) setAchievement(62);

  const uint8_t count = achievementCount();
  if (count >= 28) titleId_ = 15;
  else if (count >= 24) titleId_ = 14;
  else if (museumLevel_ >= 3) titleId_ = 11;
  else if (childCount_ > 0 && mentorProgress_ >= 5) titleId_ = 10;
  else if (fishingLevel_ >= 4) titleId_ = 8;
  else if (popcount32(wildFriendMask_) >= 4) titleId_ = 6;
  else if (expeditionWins_ >= 3) titleId_ = 5;
  else if (rivalBond_ >= 50) titleId_ = 4;
  else if (economyEarned_ >= 100) titleId_ = 3;
  else if (rivalWins_ > 0) titleId_ = 2;
  else if (partnerHarmony_ >= 60) titleId_ = 1;
  if (titleId_ == 15) setAchievement(57);
  if (achievementCount() >= 60) setAchievement(63);
}

void DeepLifeSystem::configureGoals(uint16_t day) {
  if (dailyGoalDay_ != day) {
    dailyGoalDay_ = day; dailyGoal_ = static_cast<uint8_t>((day + temperament + speciesType) % 8);
    dailyProgress_ = 0; dailyTarget_ = dailyGoal_ == 0 ? 2 : 1;
  }
  const uint16_t week = static_cast<uint16_t>(day / 7U);
  if (weeklyGoalWeek_ != week) {
    weeklyGoalWeek_ = week; weeklyGoal_ = static_cast<uint8_t>((week + generation + temperament) % 8);
    weeklyProgress_ = 0; weeklyTarget_ = weeklyGoal_ == 0 ? 3 : (weeklyGoal_ == 3 ? 20 : 3);
  }
}

void DeepLifeSystem::updateGoals(uint8_t action) {
  bool dailyMatch = false;
  if (dailyGoal_ == 0) dailyMatch = action == ACTION_FOOD || action == ACTION_CLEAN || action == ACTION_LOVE;
  else if (dailyGoal_ == 1) dailyMatch = action == ACTION_GAME;
  else if (dailyGoal_ == 2) dailyMatch = action == ACTION_TALK || action == ACTION_CALL;
  else if (dailyGoal_ == 3) dailyMatch = action == ACTION_STUDY || action == ACTION_TRAIN;
  else if (dailyGoal_ == 4) dailyMatch = action == ACTION_COOK || action == ACTION_FOOD;
  else if (dailyGoal_ == 5) dailyMatch = action == ACTION_CLEAN || action == ACTION_MED;
  else if (dailyGoal_ == 6) dailyMatch = action == ACTION_EXPLORE || action == ACTION_WALK;
  else dailyMatch = action == ACTION_FAMILY || action == ACTION_LOVE;
  if (dailyMatch && dailyProgress_ < dailyTarget_) ++dailyProgress_;

  bool weeklyMatch = false;
  if (weeklyGoal_ == 1) weeklyMatch = action == ACTION_CLEAN || action == ACTION_MED || action == ACTION_TALK;
  else if (weeklyGoal_ == 2) weeklyMatch = action == ACTION_EXPLORE || action == ACTION_WALK;
  else if (weeklyGoal_ == 4) weeklyMatch = action == ACTION_TALK || action == ACTION_MUSIC || action == ACTION_GARDEN;
  else if (weeklyGoal_ == 7) weeklyMatch = action == ACTION_STUDY || action == ACTION_FAMILY;
  if (weeklyMatch && weeklyProgress_ < weeklyTarget_) ++weeklyProgress_;

  if (dailyProgress_ == dailyTarget_) { addCoins(2); setEmotion(3, 5, 20); setAchievement(20); ++dailyProgress_; }
  if (weeklyProgress_ == weeklyTarget_) { addCoins(8); badgeMask_ |= static_cast<uint16_t>(1U << (weeklyGoal_ % 16)); setEmotion(3, 7, 21); setAchievement(21); ++weeklyProgress_; }
}

void DeepLifeSystem::generateDailyEvent(uint16_t day, uint8_t stageValue, uint8_t weather) {
  if (pendingEvent_ < TOTAL_LIFE_EVENT_COUNT || pendingEventDay_ == day || stageValue == 0) return;

  const uint16_t seed = static_cast<uint16_t>((day * 73U) ^ (generation * 131U) ^
      (temperament * 29U) ^ (weather * 17U) ^ (ownerBond * 11U) ^
      (ownerTrust * 7U) ^ storyFlags_ ^ (familyMood_ * 3U));

  // Keep the original sixteen broad stories in circulation roughly one day
  // out of six. The other days use the 224 highly contextual event variants.
  if ((seed % 6U) == 0U && stageValue >= 2) {
    uint8_t allowed[ORIGINAL_EVENT_COUNT];
    uint8_t allowedCount = 0;
    for (uint8_t event = 0; event < ORIGINAL_EVENT_COUNT; ++event) {
      if (classicEventAllowed(event, stageValue, childCount_)) allowed[allowedCount++] = event;
    }
    if (allowedCount > 0) {
      pendingEvent_ = allowed[(seed / 6U) % allowedCount];
      pendingEventChoice_ = 255; pendingEventDay_ = day; infoReason_ = 1;
      return;
    }
  }

  uint8_t eligible[28]; uint8_t count = 0;
  auto addCategory = [&](uint8_t category, uint8_t weight = 1) {
    while (weight-- && count < sizeof(eligible)) eligible[count++] = category;
  };

  if (stageValue == 1) addCategory(0, 4);                 // baby
  if (stageValue == 2) addCategory(1, 4);                 // child
  if (stageValue == 3) addCategory(2, 4);                 // teen
  if (stageValue >= 4) addCategory(3, 3);                 // adult purpose
  if (stageValue >= 5) addCategory(4, 4);                 // elder legacy

  addCategory(5, ownerBond < 35 || ownerTrust < 35 ? 3 : 1);
  if (partnerId < 8) addCategory(6, partnerHarmony_ < 40 || partnerHarmony_ > 80 ? 3 : 2);
  if (childCount_ > 0) {
    uint16_t childBondTotal = 0;
    for (uint8_t i = 0; i < childCount_; ++i) childBondTotal += childBond_[i];
    uint8_t averageChildBond = static_cast<uint8_t>(childBondTotal / childCount_);
    addCategory(7, averageChildBond < 35 || familyMood_ < 40 ? 3 : 2);
  }
  if (lifeWorld.hasCompanion()) addCategory(8, lifeWorld.companionMood() < 4 || lifeWorld.companionBond() < 35 ? 3 : 2);
  if (botany.gardenCount() > 0) addCategory(9, botany.needsCare() || botany.gardenMatureCount() > 0 ? 3 : 2);
  if (botany.indoorCount() > 0) addCategory(10, botany.indoorAverageHealth() < 5 ? 3 : 2);
  addCategory(11, rareWeather_ >= 5 ? 3 : 1);
  addCategory(12, sick || health < 6 || energy < 4 || emotionIntensity_ >= 6 ? 3 : 1);
  if (stageValue >= 2 || groupId_ < 8) addCategory(13, groupId_ < 8 ? 2 : 1);

  if (count == 0) addCategory(12);
  const uint8_t category = eligible[(seed >> 3U) % count];
  uint8_t variant = static_cast<uint8_t>((seed + day + category * 19U +
      botany.gardenAverageStage() * 5U + lifeWorld.companionBond()) % PROCEDURAL_EVENTS_PER_CATEGORY);

  // Avoid showing the exact same event twice in succession.
  if (eventLogCount_) {
    const uint8_t latest = static_cast<uint8_t>((eventLogHead_ + DEEP_EVENT_LOG_COUNT - 1) % DEEP_EVENT_LOG_COUNT);
    const uint8_t candidate = static_cast<uint8_t>(ORIGINAL_EVENT_COUNT + category * PROCEDURAL_EVENTS_PER_CATEGORY + variant);
    if (eventLogType_[latest] == candidate) variant = static_cast<uint8_t>((variant + 1U) % PROCEDURAL_EVENTS_PER_CATEGORY);
  }

  pendingEvent_ = static_cast<uint8_t>(ORIGINAL_EVENT_COUNT + category * PROCEDURAL_EVENTS_PER_CATEGORY + variant);
  pendingEventChoice_ = 255; pendingEventDay_ = day; infoReason_ = static_cast<uint8_t>(40 + category);
}

const char *DeepLifeSystem::eventTitle(uint8_t event) const {
  if (event < ORIGINAL_EVENT_COUNT) return EVENT_TITLES[event];
  if (event >= TOTAL_LIFE_EVENT_COUNT) return "No event";
  const uint8_t offset = static_cast<uint8_t>(event - ORIGINAL_EVENT_COUNT);
  return PROCEDURAL_EVENT_TITLES[offset / PROCEDURAL_EVENTS_PER_CATEGORY][offset % PROCEDURAL_EVENTS_PER_CATEGORY];
}

const char *DeepLifeSystem::eventCategoryName(uint8_t event) const {
  if (event < ORIGINAL_EVENT_COUNT) return "CLASSIC STORY";
  if (event >= TOTAL_LIFE_EVENT_COUNT) return "NO EVENT";
  return PROCEDURAL_CATEGORY_NAMES[(event - ORIGINAL_EVENT_COUNT) / PROCEDURAL_EVENTS_PER_CATEGORY];
}

const char *DeepLifeSystem::eventPrompt(uint8_t event) const {
  if (event < ORIGINAL_EVENT_COUNT) return CLASSIC_EVENT_PROMPTS[event];
  if (event >= TOTAL_LIFE_EVENT_COUNT) return "Nothing is waiting";
  const uint8_t offset = static_cast<uint8_t>(event - ORIGINAL_EVENT_COUNT);
  return PROCEDURAL_EVENT_PROMPTS[offset / PROCEDURAL_EVENTS_PER_CATEGORY][offset % PROCEDURAL_EVENTS_PER_CATEGORY];
}

const char *DeepLifeSystem::eventChoiceLabel(uint8_t event, uint8_t choice) const {
  if (event < ORIGINAL_EVENT_COUNT)
    return choice == 0 ? CLASSIC_EVENT_CHOICE_A[event] : CLASSIC_EVENT_CHOICE_B[event];
  if (event >= TOTAL_LIFE_EVENT_COUNT) return "No choice";
  const uint8_t offset = static_cast<uint8_t>(event - ORIGINAL_EVENT_COUNT);
  const uint8_t category = static_cast<uint8_t>(offset / PROCEDURAL_EVENTS_PER_CATEGORY);
  const uint8_t variant = static_cast<uint8_t>(offset % PROCEDURAL_EVENTS_PER_CATEGORY);
  return choice == 0 ? PROCEDURAL_EVENT_CHOICE_A[category][variant]
                     : PROCEDURAL_EVENT_CHOICE_B[category][variant];
}

void DeepLifeSystem::printEventContext(uint8_t event) const {
  if (event < ORIGINAL_EVENT_COUNT) { display_.print(F("Classic story")); return; }
  const uint8_t category = static_cast<uint8_t>((event - ORIGINAL_EVENT_COUNT) / PROCEDURAL_EVENTS_PER_CATEGORY);
  switch (category) {
    case 0: display_.print(F("Baby bond ")); display_.print(ownerBond); break;
    case 1: display_.print(F("Child trust ")); display_.print(ownerTrust); break;
    case 2: display_.print(F("Teen rival ")); display_.print(rivalRespect_); break;
    case 3: display_.print(F("Career rank ")); display_.print(lifeWorld.careerRank()); break;
    case 4: display_.print(F("Legacy Gen ")); display_.print(generation); break;
    case 5: display_.print(F("Bond ")); display_.print(ownerBond); display_.print(F(" T")); display_.print(ownerTrust); break;
    case 6: display_.print(F("Harmony ")); display_.print(partnerHarmony_); break;
    case 7: display_.print(F("Kids ")); display_.print(childCount_); display_.print(F(" Mood ")); display_.print(familyMood_); break;
    case 8: display_.print(F("Pet bond ")); display_.print(lifeWorld.companionBond()); break;
    case 9: display_.print(F("Garden stage ")); display_.print(botany.gardenAverageStage()); break;
    case 10: display_.print(F("Plant health ")); display_.print(botany.indoorAverageHealth()); break;
    case 11: display_.print(F("Season ")); display_.print(season_ + 1); display_.print(F(" W")); display_.print(rareWeather_); break;
    case 12: display_.print(F("HP ")); display_.print(health); display_.print(F(" EN ")); display_.print(energy); break;
    default: display_.print(F("Club Lv ")); display_.print(groupLevel_); break;
  }
}

void DeepLifeSystem::resolveProceduralEvent(uint8_t event, uint8_t choice, bool &positive) {
  const uint8_t offset = static_cast<uint8_t>(event - ORIGINAL_EVENT_COUNT);
  const uint8_t category = offset / PROCEDURAL_EVENTS_PER_CATEGORY;
  const uint8_t variant = offset % PROCEDURAL_EVENTS_PER_CATEGORY;
  const uint8_t amount = static_cast<uint8_t>(1 + variant % 3);
  const uint8_t targetSkill = variant % 4;
  const uint8_t material = variant % 6;
  positive = true;

  switch (category) {
    case 0: // Baby memories: caretaker response shapes bond, trust and temperament.
      if (choice == 0) { addStat(happy, amount); changeBond(2 + amount); changeTrust(1); setEmotion(0, 4 + amount, event); }
      else { addStat(discipline, amount); addSkill((targetSkill & 1U) ? skillFit : skillSmart, 1); changeTrust(1 + amount); setEmotion(7, 3 + amount, event); }
      if ((variant % 5U) == 0U) addStat(full, 1);
      break;
    case 1: // Childhood school/play decisions.
      if (choice == 0) { changeByte(groupLevel_, 1, 9); changeByte(rivalBond_, amount); addSkill((targetSkill & 1U) ? skillKind : skillFit, amount); setEmotion(1, 4 + amount, event); }
      else { addSkill((targetSkill & 1U) ? skillStyle : skillSmart, 1 + amount); changeTrust(amount); setEmotion(9, 4 + amount, event); }
      lifeWorld.recordMemory(20, petAgeDays(), variant);
      break;
    case 2: // Teen identity, peer pressure and future direction.
      if (choice == 0) { changeByte(rivalRespect_, 2 + amount); addSkill((targetSkill < 2) ? skillStyle : skillFit, amount); changeByte(groupLevel_, 1, 9); setEmotion(3, 4 + amount, event); }
      else { changeByte(rivalBond_, 1 + amount); addSkill((targetSkill < 2) ? skillSmart : skillKind, amount); changeTrust(amount); setEmotion(9, 5, event); }
      if ((variant % 4U) == 3U) secretHintMask_ |= (1UL << (variant % 8U));
      break;
    case 3: // Adult work, ethics and balance.
      if (choice == 0) { lifeWorld.eventCareerProgress(static_cast<uint16_t>(4 + amount * 2), 2, 1 + amount); addCoins(1 + amount); subStat(energy, amount); setEmotion(3, 5, event); }
      else { lifeWorld.eventCareerProgress(static_cast<uint16_t>(2 + amount), 0, amount); addSkill((targetSkill & 1U) ? skillKind : skillSmart, amount); changeByte(familyMood_, amount); setEmotion(0, 4, event); }
      break;
    case 4: // Elder teaching and permanent legacy.
      if (choice == 0) { changeByte(mentorProgress_, 2 + amount); if (childCount_) { uint8_t child = variant % childCount_; changeByte(childTalent_[child], amount, 12); changeByte(childBond_[child], 2 + amount); } changeByte(familyMood_, 2 + amount); setEmotion(9, 7, event); }
      else { secretHintMask_ |= (1UL << (variant % 8U)); addSkill(skillSmart, amount); lifeWorld.recordMemory(21, petAgeDays(), variant); setEmotion(0, 6, event); }
      break;
    case 5: // Caretaker bond and trust level events.
      if (choice == 0) { changeBond((ownerBond < 40 ? 4 : 2) + amount); changeTrust(1 + amount); addStat(happy, amount); setEmotion(ownerBond < 35 ? 1 : 0, 4 + amount, event); }
      else { changeTrust((ownerTrust < 40 ? 4 : 2) + amount); addStat(energy, 1); addStat(discipline, amount); setEmotion(7, 3 + amount, event); }
      break;
    case 6: // Marriage, compromise and partner bond.
      if (choice == 0) { changeByte(partnerHarmony_, (partnerHarmony_ < 40 ? 5 : 2) + amount); changeByte(partnerBond, 1 + amount); changeByte(familyMood_, 2 + amount); argumentState_ = 0; setEmotion(1, 5, event); }
      else { changeByte(partnerHarmony_, 1 + amount); changeByte(partnerBond, amount); addSkill(skillKind, amount); if (argumentState_) argumentState_ = 0; setEmotion(0, 4, event); }
      if ((variant % 8U) == 0U) anniversaryDay_ = static_cast<uint16_t>(petAgeDays());
      break;
    case 7: { // Parenting, siblings and family bond.
      uint8_t child = childCount_ ? variant % childCount_ : 0;
      if (choice == 0 && childCount_) { changeByte(childBond_[child], 3 + amount); changeByte(childTalent_[child], amount, 12); changeByte(mentorProgress_, amount); setEmotion(9, 5, event); }
      else { for (uint8_t i = 0; i < childCount_; ++i) changeByte(siblingBond_[i], 1 + amount); changeByte(familyMood_, 2 + amount); setEmotion(0, 5, event); }
      break;
    }
    case 8: // Companion care and training.
      if (choice == 0) { lifeWorld.eventAdjustCompanion(3 + amount, 2 + amount, 1, 2 + amount); changeBond(amount); setEmotion(1, 5, event); }
      else { lifeWorld.eventAdjustCompanion(1 + amount, 1, -1, 5 + amount * 2); wildSeenMask_ |= (1UL << ((variant + lifeWorld.companionType()) % DEEP_WILD_COUNT)); addSkill(skillFit, amount); setEmotion(3, 5, event); }
      break;
    case 9: // Garden events respond to maturity, water and health.
      if (choice == 0) { botany.eventWaterGarden(2 + amount); botany.eventTendGarden(1 + amount); addSkill(skillKind, amount); }
      else { botany.eventTendGarden(2 + amount); craftMaterials[material] = min<uint8_t>(99, craftMaterials[material] + amount); if (botany.gardenMatureCount()) { addCoins(amount); if (gardenHarvests < 250) ++gardenHarvests; } }
      if (gardenLevel < 100) ++gardenLevel;
      setEmotion(9, 4 + amount, event);
      break;
    case 10: // Indoor plant-room events.
      if (choice == 0) { botany.eventWaterIndoor(1 + amount); botany.eventTendIndoor(2 + amount); addStat(clean, 1); }
      else { botany.eventTendIndoor(1 + amount); addSkill(skillStyle, amount); addStat(happy, amount); }
      setEmotion(0, 4 + amount, event);
      break;
    case 11: // Weather and seasonal decisions.
      if (choice == 0) { subStat(energy, amount); wildSeenMask_ |= (1UL << ((variant + rareWeather_) % DEEP_WILD_COUNT)); seasonalCollectionMask_ |= static_cast<uint16_t>(1U << (season_ % 16)); lifeWorld.eventCommunityProgress(variant % LIFE_PROJECT_COUNT, 1, 4, amount); setEmotion(7, 5, event); }
      else { addStat(health, amount); addStat(clean, 1); changeByte(familyMood_, amount); if (partnerId < 8) changeByte(partnerHarmony_, amount); setEmotion(0, 4, event); }
      if (variant == 4 || variant == 13) secretHintMask_ |= (1UL << 5);
      break;
    case 12: // Health and emotional regulation.
      if (choice == 0) { addStat(health, 1 + amount); addStat(energy, amount); if (sick && health >= 7 && (variant % 3U) == 0U) sick = false; setEmotion(0, 5, event); }
      else { addSkill((targetSkill & 1U) ? skillFit : skillSmart, amount); addCoins(amount); subStat(energy, amount); if (health > 1 && (variant % 4U) == 0U) subStat(health, 1); setEmotion(3, 4, event); positive = health >= 4; }
      break;
    case 13: // Community projects, clubs and reputation.
      if (choice == 0) { lifeWorld.eventCommunityProgress(variant % LIFE_PROJECT_COUNT, 2 + amount, 0, 2 + amount); changeByte(groupMeetings_, 1); changeByte(groupLevel_, 1, 9); addSkill(skillKind, amount); setEmotion(3, 5, event); }
      else { lifeWorld.eventCommunityProgress(variant % LIFE_PROJECT_COUNT, 1 + amount, 2, 1 + amount); changeByte(rivalRespect_, amount); addSkill(skillSmart, amount); setEmotion(9, 4, event); }
      break;
  }
  lifeWorld.recordMemory(static_cast<uint8_t>(32 + category), petAgeDays(), variant);
}

void DeepLifeSystem::resolveEvent(uint8_t choice) {
  if (pendingEvent_ >= TOTAL_LIFE_EVENT_COUNT) return;
  const uint8_t event = pendingEvent_;
  pendingEventChoice_ = choice;
  const uint16_t day = static_cast<uint16_t>(petAgeDays());
  bool positive = false;

  if (event >= ORIGINAL_EVENT_COUNT) {
    resolveProceduralEvent(event, choice, positive);
  } else switch (event) {
    case 0: // package
      if (choice == 0) { addCoins(2); changeByte(rivalRespect_, 3); setEmotion(3, 5, event); positive = true; }
      else { craftMaterials[0] = min<uint8_t>(99, craftMaterials[0] + 2); setEmotion(8, 3, event); }
      break;
    case 1: // storm
      if (choice == 0) { subStat(energy, 1); changeByte(partnerHarmony_, 2); setEmotion(2, 4, event); positive = true; }
      else { subStat(clean, 1); secretHintMask_ |= (1UL << 1); setEmotion(7, 4, event); }
      break;
    case 2: // rival challenge
      if (choice == 0) { changeByte(rivalRespect_, 4); ++rivalWins_; setEmotion(3, 6, event); positive = true; }
      else { changeByte(rivalBond_, 4); setEmotion(1, 4, event); positive = true; }
      break;
    case 3:
      if (choice == 0) { changeByte(rivalBond_, 2); addSkill(skillKind, 1); setAchievement(22); positive = true; }
      else { addCoins(2); setEmotion(6, 3, event); }
      break;
    case 4:
      if (choice == 0) { wildSeenMask_ |= (1UL << ((day + event) % DEEP_WILD_COUNT)); setEmotion(7, 7, event); positive = true; }
      else { addCoins(3); setEmotion(0, 3, event); }
      break;
    case 5:
      if (choice == 0) { craftMaterials[4] = min<uint8_t>(99, craftMaterials[4] + 2); addSkill(skillKind, 1); positive = true; }
      else { subStat(clean, 1); setEmotion(8, 4, event); }
      break;
    case 6:
      groupId_ = choice == 0 ? static_cast<uint8_t>((temperament + day) % 8) : groupId_;
      if (choice == 0) { groupLevel_ = max<uint8_t>(1, groupLevel_); changeByte(rivalBond_, 1); positive = true; }
      break;
    case 7:
      if (choice == 0) { changeByte(partnerHarmony_, 5); changeByte(familyMood_, 5); setEmotion(1, 5, event); positive = true; }
      else { addCoins(3); changeByte(partnerHarmony_, -2); }
      break;
    case 8:
      if (choice == 0) { wildFriendMask_ |= (1UL << ((day + 3) % DEEP_WILD_COUNT)); setEmotion(1, 6, event); positive = true; }
      else { addCoins(2); }
      break;
    case 9:
      if (choice == 0) { addCoins(5); subStat(energy, 2); setEmotion(3, 4, event); positive = true; }
      else { changeByte(familyMood_, 3); setEmotion(0, 3, event); }
      break;
    case 10:
      expeditionSeed_ = static_cast<uint8_t>(day ^ speciesType ^ 0x5A); secretHintMask_ |= (1UL << ((day + 2) % 8)); setEmotion(7, 6, event); positive = true;
      break;
    case 11:
      if (choice == 0) { addSkill(skillFit, 1); changeByte(rivalRespect_, 2); positive = true; }
      else { addSkill(skillStyle, 1); changeByte(groupLevel_, 1, 9); positive = true; }
      break;
    case 12:
      if (childCount_ > 1) { for (uint8_t i = 0; i < childCount_; ++i) changeByte(siblingBond_[i], choice == 0 ? 4 : -2); positive = choice == 0; }
      setEmotion(choice == 0 ? 0 : 8, 4, event);
      break;
    case 13:
      wildSeenMask_ |= (1UL << ((day + weatherSeenMask_) % DEEP_WILD_COUNT)); seasonalCollectionMask_ |= (1U << season_); positive = true; setEmotion(1, 6, event); break;
    case 14:
      if (choice == 0) { if (craftMaterials[2] > 0) --craftMaterials[2]; addSkill(skillSmart, 1); positive = true; }
      else { addCoins(1); setEmotion(8, 3, event); }
      break;
    case 15:
      secretHintMask_ |= (1UL << ((generation + day) % 8)); changeByte(familyMood_, 3); setEmotion(9, 7, event); positive = true; break;
  }
  const uint8_t storyBit = event < ORIGINAL_EVENT_COUNT ? event : static_cast<uint8_t>(16 + (event - ORIGINAL_EVENT_COUNT) / PROCEDURAL_EVENTS_PER_CATEGORY);
  storyFlags_ |= (1UL << storyBit);
  addEventLog(event, static_cast<uint8_t>((choice & 1U) | (positive ? 0x80U : 0U)), day);
  if (positive) changeTrust(1);
  pendingEvent_ = 255; infoReason_ = 2; evaluateAchievements(); markSaveDirty();
}

void DeepLifeSystem::updateSeason(uint16_t day, uint8_t weather) {
  season_ = static_cast<uint8_t>((day / 30U) % 4U);
  rareWeather_ = static_cast<uint8_t>((weather + (day % 7U)) % 8U);
  weatherSeenMask_ |= static_cast<uint16_t>(1U << rareWeather_);
  if (rareWeather_ == 5) secretHintMask_ |= (1UL << 5); // meteor shower
  if (rareWeather_ == 6) secretHintMask_ |= (1UL << 6); // rainbow
}

uint8_t DeepLifeSystem::effectiveDifficulty() const {
  return difficultyMode > 3 ? 1 : difficultyMode;
}

void DeepLifeSystem::startGeneration(uint8_t species, uint16_t gen, uint16_t birthCode,
                                     uint8_t temperamentValue) {
  emotion_ = 0; emotionIntensity_ = 2; emotionMinutes_ = 120;
  rivalSpecies_ = static_cast<uint8_t>((species + 7 + (birthCode % 23)) % 50);
  if (rivalSpecies_ == species) rivalSpecies_ = static_cast<uint8_t>((rivalSpecies_ + 11) % 50);
  rivalPersonality_ = static_cast<uint8_t>((temperamentValue + gen + birthCode) % 8);
  rivalBond_ = newGamePlus_ ? 15 : 5; rivalRespect_ = newGamePlus_ ? 10 : 5;
  rivalRank_ = min<uint8_t>(9, newGamePlus_); rivalWins_ = 0; rivalLosses_ = 0; rivalStory_ = 0;
  groupId_ = 255; groupLevel_ = 0; groupMeetings_ = 0; groupQuest_ = 0;
  partnerHarmony_ = 50; familyMood_ = 55; argumentState_ = 0; anniversaryDay_ = 0;
  childCount_ = 0; chosenHeir_ = 255; mentorSkill_ = 255; mentorProgress_ = 0;
  memset(childSpecies_, 255, sizeof(childSpecies_)); memset(childBond_, 0, sizeof(childBond_)); memset(childAge_, 0, sizeof(childAge_)); memset(siblingBond_, 50, sizeof(siblingBond_));
  pendingEvent_ = 255; pendingEventDay_ = 65535;
  expeditionSeed_ = static_cast<uint8_t>(birthCode ^ species ^ gen);
  configureGoals(static_cast<uint16_t>(petAgeDays()));
  addEventLog(31, species, static_cast<uint16_t>(petAgeDays()));
  markSaveDirty();
}

void DeepLifeSystem::archiveGeneration(uint8_t species, uint16_t gen, uint8_t ageDays) {
  (void)gen;
  legacySiblingCount_ = 0;
  grandparentSpecies_ = species % 50;
  grandparentBond_ = familyMood_;
  if (childCount_ > 0 && chosenHeir_ < childCount_) {
    inheritedTemperament = childPersonality_[chosenHeir_] % 8;
    inheritedFavoriteGame = childTalent_[chosenHeir_] % 13;
    inheritedBodyType = childSpecies_[chosenHeir_] % 12;
    inheritedTrait = childTalent_[chosenHeir_] % 4;
    for (uint8_t i = 0; i < childCount_ && legacySiblingCount_ < DEEP_LEGACY_SIBLING_COUNT; ++i) {
      if (i == chosenHeir_) continue;
      legacySiblingSpecies_[legacySiblingCount_] = childSpecies_[i] % 50;
      legacySiblingBond_[legacySiblingCount_] = childBond_[i];
      ++legacySiblingCount_;
    }
  }
  for (uint8_t i = legacySiblingCount_; i < DEEP_LEGACY_SIBLING_COUNT; ++i) {
    legacySiblingSpecies_[i] = 255; legacySiblingBond_[i] = 0;
  }
  if (ageDays >= 20) setAchievement(23);
  if (partnerHarmony_ >= 80) setAchievement(24);
  addEventLog(30, species, ageDays);
  evaluateAchievements(); markSaveDirty();
}

void DeepLifeSystem::tick(uint32_t dayLong, uint8_t hour, uint8_t stageValue, uint8_t weather) {
  const uint16_t day = static_cast<uint16_t>(min<uint32_t>(dayLong, 65535));
  configureGoals(day); updateSeason(day, weather); generateDailyEvent(day, stageValue, weather);
  if (emotionMinutes_ > 0) --emotionMinutes_; else if (emotion_ != 0) { emotion_ = 0; emotionIntensity_ = 1; }
  if (economyDay_ != day) { economyDay_ = day; economyDayEarned_ = 0; economyDaySpent_ = 0; }
  if (stageValue >= 2 && groupId_ < 8 && hour == 16 && (day % 3U) == (groupId_ % 3U)) {
    if (groupMeetings_ < 255) ++groupMeetings_;
    changeByte(groupLevel_, 1, 9); changeByte(rivalBond_, 1); setEmotion(1, 3, 30);
  }
  if (partnerId < 8 && stageValue >= 4) {
    if (anniversaryDay_ == 0) anniversaryDay_ = day;
    if (day > anniversaryDay_ && ((day - anniversaryDay_) % 30U) == 0U) {
      changeByte(partnerHarmony_, 5); changeByte(familyMood_, 4); setEmotion(3, 6, 31); setAchievement(25);
    }
    if (partnerHarmony_ < 25 && argumentState_ == 0) argumentState_ = 1;
  }
  for (uint8_t i = 0; i < childCount_; ++i) {
    if (childAge_[i] < 99 && hour == 7 && (day % 2U) == (i % 2U)) ++childAge_[i];
  }
  evaluateAchievements();
}

void DeepLifeSystem::recordAction(uint8_t action, uint32_t dayLong, uint8_t hour, uint8_t weather) {
  (void)hour;
  (void)weather;
  const uint16_t day = static_cast<uint16_t>(min<uint32_t>(dayLong, 65535));
  configureGoals(day); updateGoals(action);
  if (action == ACTION_GAME || action == ACTION_MUSIC || action == ACTION_TOY) setEmotion(1, 4, action);
  else if (action == ACTION_STUDY || action == ACTION_EXPLORE) setEmotion(7, 4, action);
  else if (action == ACTION_JOB || action == ACTION_TRAIN) setEmotion(3, 3, action);
  else if (action == ACTION_TALK || action == ACTION_LOVE || action == ACTION_FAMILY) {
    setEmotion(0, 4, action); changeByte(partnerHarmony_, partnerId < 8 ? 2 : 0); changeByte(familyMood_, 2);
    for (uint8_t i = 0; i < childCount_; ++i) changeByte(childBond_[i], 1);
  } else if (action == ACTION_CLEAN || action == ACTION_MED) setEmotion(0, 2, action);
  else if (action == ACTION_CALL && happy < 3) setEmotion(4, 4, action);
  else if (action == ACTION_COOK || action == ACTION_FOOD) { changeByte(familyMood_, 1); if (groupId_ == 7) changeByte(groupLevel_, 1, 9); }
  else if (action == ACTION_GARDEN && groupId_ == 2) changeByte(groupLevel_, 1, 9);

  if (action == ACTION_MUSIC && groupId_ == 3) changeByte(groupLevel_, 1, 9);
  if (action == ACTION_STUDY && groupId_ == 5) changeByte(groupLevel_, 1, 9);
  if (action == ACTION_GAME && groupId_ == 4) changeByte(groupLevel_, 1, 9);
  if (action == ACTION_TALK && groupId_ == 6) changeByte(groupLevel_, 1, 9);

  if (stage == 1 && (action == ACTION_LOVE || action == ACTION_TALK)) changeBond(1);
  if (stage == 2 && (action == ACTION_STUDY || action == ACTION_GAME)) { addSkill(skillSmart, 1); secretHintMask_ |= 1UL; }
  if (stage == 3 && (action == ACTION_GAME || action == ACTION_TRAIN)) changeByte(rivalRespect_, 1);
  if (stage == 5 && (action == ACTION_STUDY || action == ACTION_FAMILY)) changeByte(mentorProgress_, 1);

  if (rareWeather_ == 2 && action == ACTION_GARDEN) craftMaterials[4] = min<uint8_t>(99, craftMaterials[4] + 1);
  if (rareWeather_ == 5 && action == ACTION_EXPLORE) secretHintMask_ |= (1UL << 7);
  infoReason_ = static_cast<uint8_t>(40 + (action % 20));
  markSaveDirty();
}

void DeepLifeSystem::recordGameResult(uint8_t gameId, bool won, uint8_t score) {
  if (won) {
    ++rivalWins_; changeByte(rivalRespect_, 2); setEmotion(3, 5, gameId);
    if (weeklyGoal_ == 0 && weeklyProgress_ < weeklyTarget_) ++weeklyProgress_;
  } else {
    ++rivalLosses_; changeByte(rivalBond_, 1); setEmotion(score > 0 ? 6 : 8, 3, gameId);
  }
  if (gameId == favoriteGameId && won) secretHintMask_ |= (1UL << 2);
  if (rivalWins_ + rivalLosses_ >= static_cast<uint8_t>((rivalRank_ + 1) * 4) && rivalRank_ < 9) ++rivalRank_;
  if (rivalRespect_ >= 30 && rivalStory_ < 1) rivalStory_ = 1;
  if (rivalBond_ >= 40 && rivalStory_ < 2) rivalStory_ = 2;
  evaluateAchievements(); markSaveDirty();
}

void DeepLifeSystem::recordNeighborInteraction(uint8_t resident, uint8_t interaction, uint8_t relation) {
  if (resident == rivalPersonality_ % 8) {
    changeByte(rivalBond_, interaction == 0 ? 3 : 1);
    changeByte(rivalRespect_, relation >= 50 ? 2 : 1);
  }
  if (groupId_ < 8) { changeByte(groupLevel_, 1, 9); if (groupMeetings_ < 255) ++groupMeetings_; }
  setEmotion(relation >= 50 ? 1 : 2, 3, resident); markSaveDirty();
}

void DeepLifeSystem::recordEconomy(uint16_t amount, bool earned) {
  if (amount == 0) return;
  if (earned) { economyEarned_ += amount; economyDayEarned_ = static_cast<uint16_t>(min<uint32_t>(65535, economyDayEarned_ + amount)); }
  else { economySpent_ += amount; economyDaySpent_ = static_cast<uint16_t>(min<uint32_t>(65535, economyDaySpent_ + amount)); }
  if (economyTransactions_ < 65535) ++economyTransactions_;
  if (weeklyGoal_ == 3 && earned) weeklyProgress_ = static_cast<uint8_t>(min<uint16_t>(255, weeklyProgress_ + amount));
  evaluateAchievements();
}

uint8_t DeepLifeSystem::applySecretEvolution(uint8_t normalSpecies) {
  uint8_t secret = 255;
  if ((secretHintMask_ & (1UL << 0)) && skillSmart >= 12 && clean >= 8) secret = 0;
  else if ((secretHintMask_ & (1UL << 1)) && season_ == 0 && partnerHarmony_ >= 70) secret = 1;
  else if ((secretHintMask_ & (1UL << 2)) && rivalWins_ >= 5 && rivalBond_ >= 40) secret = 2;
  else if ((secretHintMask_ & (1UL << 3)) && popcount32(wildFriendMask_) >= 4) secret = 3;
  else if ((secretHintMask_ & (1UL << 4)) && popcount16(fossilCompleteMask_) >= 3) secret = 4;
  else if ((secretHintMask_ & (1UL << 5)) && (weatherSeenMask_ & (1U << 5)) && expeditionWins_ >= 3) secret = 5;
  else if ((secretHintMask_ & (1UL << 6)) && childCount_ >= 2 && partnerHarmony_ >= 80) secret = 6;
  else if ((secretHintMask_ & (1UL << 7)) && achievementCount() >= 20) secret = 7;
  if (secret < 8) {
    secretUnlockMask_ |= static_cast<uint16_t>(1U << secret); setAchievement(16);
    addEventLog(29, SECRET_SPECIES[secret], static_cast<uint16_t>(petAgeDays()));
    return SECRET_SPECIES[secret];
  }
  return normalSpecies;
}

uint8_t DeepLifeSystem::extraGameAssist(uint8_t gameId) const {
  uint8_t assist = 0;
  if ((accessibilityFlags_ & 0x04U) != 0) ++assist; // simple games
  if (emotion_ == 9 && emotionIntensity_ >= 5) ++assist;
  if (emotion_ == 2 || emotion_ == 6) assist = assist > 0 ? assist - 1 : 0;
  if (rivalBond_ >= 75 && gameId == favoriteGameId) ++assist;
  if (stage == 5 && mentorProgress_ >= 8) ++assist;
  return min<uint8_t>(3, assist);
}

const __FlashStringHelper *DeepLifeSystem::emotionName() const {
  switch (emotion_ % 10) {
    case 0: return F("Calm"); case 1: return F("Excited"); case 2: return F("Nervous");
    case 3: return F("Proud"); case 4: return F("Lonely"); case 5: return F("Jealous");
    case 6: return F("Embarrassed"); case 7: return F("Curious"); case 8: return F("Grumpy");
    default: return F("Inspired");
  }
}

const __FlashStringHelper *DeepLifeSystem::titleName() const {
  switch (titleId_ % DEEP_TITLE_COUNT) {
    case 0: return F("New Caretaker"); case 1: return F("Kind Neighbor"); case 2: return F("Game Learner");
    case 3: return F("City Helper"); case 4: return F("Rival Friend"); case 5: return F("Trail Scout");
    case 6: return F("Wild Whisperer"); case 7: return F("Fossil Keeper"); case 8: return F("Master Angler");
    case 9: return F("Season Watcher"); case 10: return F("Family Mentor"); case 11: return F("Museum Curator");
    case 12: return F("Festival Star"); case 13: return F("City Legend"); case 14: return F("Legacy Keeper");
    default: return F("World Guardian");
  }
}

const __FlashStringHelper *DeepLifeSystem::stageBenefit(uint8_t s) const {
  if (s == 0) return F("Egg: family watches");
  if (s == 1) return F("Baby: bond grows fast");
  if (s == 2) return F("Child: learns quickly");
  if (s == 3) return F("Teen: rival & clubs");
  if (s == 4) return F("Adult: career/family");
  return F("Elder: mentor legacy");
}

void DeepLifeSystem::drawHeader(const __FlashStringHelper *title, uint8_t page, uint8_t count) const {
  display_.setTextSize(1); display_.setTextColor(SSD1306_WHITE);
  display_.setCursor(1, 1); display_.print(title); display_.setCursor(104, 1); display_.print(page); display_.print(F("/")); display_.print(count);
  display_.drawLine(0, 11, 127, 11, SSD1306_WHITE);
}

void DeepLifeSystem::drawEmotionIcon(uint8_t id, int x, int y, bool filled) const {
  if (filled) display_.fillCircle(x + 7, y + 7, 7, SSD1306_WHITE);
  else display_.drawCircle(x + 7, y + 7, 7, SSD1306_WHITE);
  const uint16_t face = filled ? SSD1306_BLACK : SSD1306_WHITE;
  display_.drawPixel(x + 4, y + 5, face); display_.drawPixel(x + 10, y + 5, face);
  if (id == 0 || id == 3 || id == 9) display_.drawLine(x + 4, y + 10, x + 10, y + 10, face);
  else if (id == 1 || id == 7) { display_.drawLine(x + 4, y + 9, x + 7, y + 11, face); display_.drawLine(x + 7, y + 11, x + 10, y + 9, face); }
  else if (id == 2 || id == 4 || id == 6) { display_.drawLine(x + 4, y + 11, x + 7, y + 9, face); display_.drawLine(x + 7, y + 9, x + 10, y + 11, face); }
  else display_.drawLine(x + 4, y + 9, x + 10, y + 9, face);
  if (id == 2) display_.drawPixel(x + 12, y + 8, face);
  if (id == 3) { display_.drawPixel(x + 1, y + 1, SSD1306_WHITE); display_.drawPixel(x + 13, y + 1, SSD1306_WHITE); }
  if (id == 5) display_.drawLine(x + 1, y + 2, x + 4, y + 4, SSD1306_WHITE);
}

void DeepLifeSystem::drawHubIcon(uint8_t id, int x, int y) const {
  display_.drawRoundRect(x, y, 28, 28, 5, SSD1306_WHITE);
  switch (id) {
    case 0: drawEmotionIcon(emotion_, x + 7, y + 7, false); break;
    case 1: display_.drawRoundRect(x + 5, y + 6, 18, 16, 3, SSD1306_WHITE); display_.drawLine(x + 9, y + 22, x + 6, y + 26, SSD1306_WHITE); break;
    case 2: drawRival(x + 6, y + 6, 0); break;
    case 3: for (uint8_t i = 0; i < 3; ++i) display_.drawCircle(x + 7 + i * 7, y + 10 + (i & 1) * 4, 4, SSD1306_WHITE); break;
    case 4: display_.drawCircle(x + 10, y + 12, 5, SSD1306_WHITE); display_.drawCircle(x + 18, y + 12, 5, SSD1306_WHITE); display_.fillTriangle(x + 5, y + 14, x + 23, y + 14, x + 14, y + 24, SSD1306_WHITE); break;
    case 5: drawChild(0, x + 8, y + 8, 0); break;
    case 6: display_.drawRect(x + 4, y + 4, 20, 20, SSD1306_WHITE); display_.drawLine(x + 8, y + 4, x + 8, y + 24, SSD1306_WHITE); display_.drawLine(x + 16, y + 4, x + 16, y + 24, SSD1306_WHITE); break;
    case 7: drawWild(0, x + 7, y + 7, 0); break;
    case 8: drawFossil(0, x + 6, y + 6); break;
    case 9: drawFish(0, x + 6, y + 8, 0); break;
    case 10: display_.drawCircle(x + 14, y + 14, 7, SSD1306_WHITE); for (uint8_t i = 0; i < 8; ++i) display_.drawPixel(x + 4 + (i * 3) % 21, y + 4 + (i * 5) % 21, SSD1306_WHITE); break;
    case 11: display_.drawCircle(x + 14, y + 14, 10, SSD1306_WHITE); display_.drawLine(x + 14, y + 4, x + 14, y + 24, SSD1306_WHITE); break;
    case 12: display_.drawRect(x + 4, y + 7, 20, 17, SSD1306_WHITE); display_.drawTriangle(x + 3, y + 7, x + 14, y + 1, x + 25, y + 7, SSD1306_WHITE); break;
    case 13: display_.drawCircle(x + 14, y + 14, 10, SSD1306_WHITE); display_.setCursor(x + 8, y + 10); display_.print(F("+")); break;
    case 14: display_.drawRect(x + 5, y + 5, 18, 18, SSD1306_WHITE); display_.drawLine(x + 8, y + 10, x + 20, y + 10, SSD1306_WHITE); display_.drawLine(x + 8, y + 16, x + 20, y + 16, SSD1306_WHITE); break;
    case 15: display_.drawRect(x + 6, y + 5, 16, 20, SSD1306_WHITE); display_.drawLine(x + 9, y + 10, x + 19, y + 10, SSD1306_WHITE); break;
    case 16: display_.drawLine(x + 5, y + 23, x + 5, y + 10, SSD1306_WHITE); display_.drawLine(x + 12, y + 23, x + 12, y + 6, SSD1306_WHITE); display_.drawLine(x + 19, y + 23, x + 19, y + 14, SSD1306_WHITE); break;
    default: display_.drawCircle(x + 14, y + 14, 10, SSD1306_WHITE); display_.drawCircle(x + 14, y + 14, 3, SSD1306_WHITE); break;
  }
}

void DeepLifeSystem::drawRival(int x, int y, uint8_t frame) const {
  display_.drawRoundRect(x + 2, y + 4, 14, 13, 4, SSD1306_WHITE);
  display_.drawTriangle(x + 4, y + 5, x + 7, y, x + 9, y + 5, SSD1306_WHITE);
  display_.drawTriangle(x + 11, y + 5, x + 14, y, x + 16, y + 6, SSD1306_WHITE);
  display_.drawPixel(x + 6, y + 9, SSD1306_WHITE); display_.drawPixel(x + 12, y + 9, SSD1306_WHITE);
  display_.drawLine(x + 7, y + 13, x + 11, y + 13, SSD1306_WHITE);
  display_.drawLine(x + 5, y + 17, x + 3, y + 21 + (frame & 1), SSD1306_WHITE);
  display_.drawLine(x + 13, y + 17, x + 16, y + 21 - (frame & 1), SSD1306_WHITE);
  if (rivalRank_ >= 3) display_.drawLine(x + 2, y + 3, x + 16, y + 3, SSD1306_WHITE);
}

void DeepLifeSystem::drawChild(uint8_t id, int x, int y, uint8_t frame) const {
  const uint8_t type = id % 3;
  display_.drawRoundRect(x + 2, y + 4, 10, 9, 3, SSD1306_WHITE);
  if (type == 0) { display_.drawTriangle(x + 3, y + 5, x + 5, y + 1, x + 7, y + 5, SSD1306_WHITE); display_.drawTriangle(x + 8, y + 5, x + 11, y + 1, x + 12, y + 6, SSD1306_WHITE); }
  else if (type == 1) display_.drawCircle(x + 7, y + 3, 2, SSD1306_WHITE);
  else display_.drawLine(x + 3, y + 3, x + 11, y + 3, SSD1306_WHITE);
  display_.drawPixel(x + 5, y + 8, SSD1306_WHITE); display_.drawPixel(x + 9, y + 8, SSD1306_WHITE);
  display_.drawLine(x + 4, y + 13, x + 3, y + 16 + (frame & 1), SSD1306_WHITE);
  display_.drawLine(x + 10, y + 13, x + 11, y + 16 - (frame & 1), SSD1306_WHITE);
}

void DeepLifeSystem::drawWild(uint8_t id, int x, int y, uint8_t frame) const {
  switch (id % 8) {
    case 0: display_.drawCircle(x + 8, y + 9, 6, SSD1306_WHITE); display_.drawLine(x + 3, y + 4, x, y + 1, SSD1306_WHITE); display_.drawLine(x + 13, y + 4, x + 16, y + 1, SSD1306_WHITE); break;
    case 1: display_.drawRoundRect(x + 2, y + 5, 13, 10, 4, SSD1306_WHITE); display_.drawCircle(x + 3, y + 13, 2, SSD1306_WHITE); display_.drawCircle(x + 14, y + 13, 2, SSD1306_WHITE); break;
    case 2: display_.drawCircle(x + 8, y + 8, 4, SSD1306_WHITE); display_.drawTriangle(x + 4, y + 8, x, y + 3, x + 1, y + 12, SSD1306_WHITE); display_.drawTriangle(x + 12, y + 8, x + 16, y + 3, x + 15, y + 12, SSD1306_WHITE); break;
    case 3: display_.drawCircle(x + 8, y + 9, 6, SSD1306_WHITE); display_.drawLine(x + 8, y + 3, x + 8, y + 15, SSD1306_WHITE); display_.drawLine(x + 3, y + 9, x + 13, y + 9, SSD1306_WHITE); break;
    case 4: display_.drawCircle(x + 8, y + 8, 5, SSD1306_WHITE); display_.drawLine(x + 5, y + 13, x + 3, y + 17, SSD1306_WHITE); display_.drawLine(x + 8, y + 13, x + 8, y + 17, SSD1306_WHITE); display_.drawLine(x + 11, y + 13, x + 13, y + 17, SSD1306_WHITE); break;
    case 5: display_.drawRoundRect(x + 2, y + 4, 13, 12, 3, SSD1306_WHITE); for (uint8_t i = 0; i < 3; ++i) display_.drawLine(x + 3 + i * 5, y + 16, x + 2 + i * 5, y + 19, SSD1306_WHITE); break;
    case 6: display_.drawCircle(x + 8, y + 9, 7, SSD1306_WHITE); display_.drawPixel(x + 5, y + 8, SSD1306_WHITE); display_.drawPixel(x + 11, y + 8, SSD1306_WHITE); break;
    default: display_.drawTriangle(x + 2, y + 14, x + 8, y + 2, x + 14, y + 14, SSD1306_WHITE); display_.drawCircle(x + 8, y + 10, 2, SSD1306_WHITE); break;
  }
  display_.drawPixel(x + 6, y + 8 + (frame & 1), SSD1306_WHITE);
}

void DeepLifeSystem::drawFish(uint8_t id, int x, int y, uint8_t frame) const {
  const uint8_t type = id % 4;
  display_.drawCircle(x + 7, y + 7, 5, SSD1306_WHITE);
  display_.drawCircle(x + 10, y + 7, 4, SSD1306_WHITE);
  display_.drawTriangle(x + 1, y + 7, x - 3, y + 3 + (frame & 1), x - 3, y + 11 - (frame & 1), SSD1306_WHITE);
  display_.drawPixel(x + 12, y + 6, SSD1306_WHITE);
  if (type == 1) display_.drawLine(x + 6, y + 3, x + 9, y, SSD1306_WHITE);
  else if (type == 2) display_.drawLine(x + 5, y + 11, x + 10, y + 14, SSD1306_WHITE);
  else if (type == 3) { display_.drawLine(x + 4, y + 5, x + 10, y + 9, SSD1306_WHITE); display_.drawLine(x + 6, y + 3, x + 12, y + 7, SSD1306_WHITE); }
}

void DeepLifeSystem::drawFossil(uint8_t id, int x, int y) const {
  if ((id & 1U) == 0) {
    display_.drawCircle(x + 8, y + 8, 7, SSD1306_WHITE); display_.drawCircle(x + 8, y + 8, 4, SSD1306_WHITE); display_.drawCircle(x + 8, y + 8, 1, SSD1306_WHITE);
  } else {
    display_.drawLine(x + 2, y + 8, x + 14, y + 8, SSD1306_WHITE); display_.drawCircle(x + 2, y + 8, 3, SSD1306_WHITE); display_.drawCircle(x + 14, y + 8, 3, SSD1306_WHITE);
  }
}

void DeepLifeSystem::drawEmotionBadge(int x, int y, uint8_t frame) const {
  if (emotion_ == 0 && emotionIntensity_ <= 2) return;
  if (reducedMotion() && (frame & 1U)) return;
  display_.fillRoundRect(x, y, 18, 18, 4, SSD1306_BLACK);
  display_.drawRoundRect(x, y, 18, 18, 4, SSD1306_WHITE);
  drawEmotionIcon(emotion_, x + 2, y + 2, false);
}

void DeepLifeSystem::drawContextPose(int x, int y, uint8_t frame) const {
  if (reducedMotion() && (frame & 1U)) return;
  if (emotion_ == 1) { display_.drawPixel(x - 3, y - 2, SSD1306_WHITE); display_.drawPixel(x + 18, y - 3, SSD1306_WHITE); }
  else if (emotion_ == 2) { display_.drawPixel(x + 18, y + 2, SSD1306_WHITE); display_.drawLine(x + 19, y + 3, x + 19, y + 6, SSD1306_WHITE); }
  else if (emotion_ == 3) { display_.drawLine(x - 2, y - 2, x + 1, y + 1, SSD1306_WHITE); display_.drawLine(x + 17, y - 2, x + 14, y + 1, SSD1306_WHITE); }
  else if (emotion_ == 4) { display_.drawCircle(x + 18, y + 5, 2, SSD1306_WHITE); }
  else if (emotion_ == 8) { display_.drawLine(x - 2, y, x + 2, y + 2, SSD1306_WHITE); }
  else if (emotion_ == 9) { display_.drawLine(x + 18, y - 3, x + 18, y + 1, SSD1306_WHITE); display_.drawLine(x + 16, y - 1, x + 20, y - 1, SSD1306_WHITE); }
}

void DeepLifeSystem::drawFamilyActors(uint8_t room, uint8_t frame) const {
  if (dead) return;
  if (childCount_ > 0) {
    const uint8_t shown = static_cast<uint8_t>((frame / 12U) % childCount_);
    int x = room == 4 ? 14 : 104; int y = room == 4 ? 36 : 34;
    display_.fillRoundRect(x - 3, y - 4, 18, 22, 4, SSD1306_BLACK);
    display_.drawRoundRect(x - 3, y - 4, 18, 22, 4, SSD1306_WHITE);
    drawChild(childSpecies_[shown], x, y, frame);
    if (childCount_ > 1) { display_.fillRect(x - 2, y - 3, 7, 7, SSD1306_BLACK); display_.setCursor(x - 1, y - 3); display_.print(shown + 1); }
    return;
  }
  // Grandparents can make a quiet weekly home visit after the family has moved
  // to a new generation. They use the same protected side pad as children.
  if (room == 0 && grandparentSpecies_ < 50 && (petAgeDays() % 7U) == 0U) {
    const int x = 104, y = 34;
    display_.fillRoundRect(x - 3, y - 4, 20, 22, 4, SSD1306_BLACK);
    display_.drawRoundRect(x - 3, y - 4, 20, 22, 4, SSD1306_WHITE);
    drawSprite16(grandparentSpecies_, x, y + ((frame / 7U) & 1U));
  }
}

void DeepLifeSystem::drawSeasonOverlay(bool cityScene, uint8_t sceneId, uint8_t frame) const {
  if (reducedMotion() && (frame & 1U)) return;
  const uint8_t phase = static_cast<uint8_t>((frame / 3U) & 3U);
  if (season_ == 0) { // winter, edge snow only
    for (uint8_t i = 0; i < 5; ++i) display_.drawPixel(5 + i * 26 + (phase & 1), 14 + ((i * 7 + phase) % 34), SSD1306_WHITE);
    if (cityScene) display_.drawLine(0, 51, 127, 51, SSD1306_WHITE);
  } else if (season_ == 1) { // spring flowers on lower edges
    for (uint8_t i = 0; i < 4; ++i) { int x = 4 + i * 35; display_.drawPixel(x, 50, SSD1306_WHITE); display_.drawPixel(x - 1, 49, SSD1306_WHITE); display_.drawPixel(x + 1, 49, SSD1306_WHITE); }
  } else if (season_ == 2) { // summer sun glint
    display_.drawCircle(cityScene ? 116 : 9, 16, 4, SSD1306_WHITE); display_.drawPixel(cityScene ? 110 : 3, 16, SSD1306_WHITE); display_.drawPixel(cityScene ? 122 : 15, 16, SSD1306_WHITE);
  } else { // autumn leaves at edges
    for (uint8_t i = 0; i < 4; ++i) display_.drawPixel(2 + i * 40 + phase, 22 + ((i * 9 + phase * 3) % 28), SSD1306_WHITE);
  }
  if (rareWeather_ == 5 && cityScene) { display_.drawLine(96, 14 + phase, 105, 19 + phase, SSD1306_WHITE); }
  if (rareWeather_ == 6) { display_.drawCircle(109, 24, 8, SSD1306_WHITE); display_.fillRect(100, 24, 19, 9, SSD1306_BLACK); }
  (void)sceneId;
}

void DeepLifeSystem::playTransition(uint8_t type) const {
  if (reducedMotion()) return;
  for (uint8_t step = 0; step < 4; ++step) {
    display_.clearDisplay();
    if (type % 3 == 0) {
      for (int x = step; x < 128; x += 4) display_.drawFastVLine(x, 0, 64, SSD1306_WHITE);
    } else if (type % 3 == 1) {
      display_.drawRoundRect(8 + step * 10, 5 + step * 5, 112 - step * 20, 54 - step * 10, 6, SSD1306_WHITE);
    } else {
      display_.drawLine(0, step * 16, 127, step * 16, SSD1306_WHITE);
      display_.drawLine(0, 63 - step * 16, 127, 63 - step * 16, SSD1306_WHITE);
    }
    display_.display(); delay(45);
  }
}

void DeepLifeSystem::popup(const __FlashStringHelper *a, const __FlashStringHelper *b, uint16_t holdMs) const {
  display_.clearDisplay(); display_.setTextSize(1); display_.setTextColor(SSD1306_WHITE);
  display_.drawRoundRect(3, 13, 122, 39, 5, SSD1306_WHITE); display_.setCursor(9, 23); display_.print(a); display_.setCursor(9, 36); display_.print(b); display_.display(); delay(holdMs);
}

void DeepLifeSystem::popup(const __FlashStringHelper *a, const char *b, uint16_t holdMs) const {
  display_.clearDisplay(); display_.setTextSize(1); display_.setTextColor(SSD1306_WHITE);
  display_.drawRoundRect(3, 13, 122, 39, 5, SSD1306_WHITE); display_.setCursor(9, 23); display_.print(a); display_.setCursor(9, 36); display_.print(b); display_.display(); delay(holdMs);
}

bool DeepLifeSystem::confirm(const __FlashStringHelper *title, const __FlashStringHelper *detail) const {
  if (!confirmations()) return true;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH; uint8_t pick = 1; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return false;
    bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if ((oldL == HIGH && l == LOW) || (oldR == HIGH && r == LOW)) { pick ^= 1U; clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) { waitButtonRelease(); return pick == 0; }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); drawHeader(title, 1, 1); display_.setCursor(4, 20); display_.print(detail);
    display_.drawRoundRect(18, 38, 40, 17, 4, SSD1306_WHITE); display_.drawRoundRect(70, 38, 40, 17, 4, SSD1306_WHITE);
    if (pick == 0) display_.fillRoundRect(20, 40, 36, 13, 3, SSD1306_WHITE); else display_.fillRoundRect(72, 40, 36, 13, 3, SSD1306_WHITE);
    display_.setTextColor(pick == 0 ? SSD1306_BLACK : SSD1306_WHITE); display_.setCursor(29, 43); display_.print(F("YES"));
    display_.setTextColor(pick == 1 ? SSD1306_BLACK : SSD1306_WHITE); display_.setCursor(82, 43); display_.print(F("NO")); display_.setTextColor(SSD1306_WHITE); display_.display(); delay(25);
  }
}

void DeepLifeSystem::runHub() {
  uint8_t pick = 0; bool oldL = HIGH, oldS = HIGH, oldR = HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? 17 : pick - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { pick = static_cast<uint8_t>((pick + 1) % 18); clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) {
      waitButtonRelease(); playTransition(pick);
      switch (pick) {
        case 0: todayPage(); break; case 1: storyPage(); break; case 2: rivalPage(); break;
        case 3: groupsPage(); break; case 4: familyPage(); break; case 5: mentoringPage(); break;
        case 6: adventurePage(); break; case 7: creaturesPage(); break; case 8: fossilsPage(); break;
        case 9: fishingPage(); break; case 10: seasonsPage(); break; case 11: achievementsPage(); break;
        case 12: museumPage(); break; case 13: newGamePlusPage(); break; case 14: settingsPage(); break;
        case 15: tutorialPage(); break; case 16: economyPage(); break; default: diagnosticsPage(); break;
      }
      oldL = oldS = oldR = HIGH; waitButtonRelease();
    }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); drawHeader(F("DEEP LIFE WORLD"), pick + 1, 18);
    display_.drawRoundRect(4, 15, 120, 37, 5, SSD1306_WHITE); drawHubIcon(pick, 9, 19);
    display_.setCursor(43, 21); display_.print(HUB_NAMES[pick]);
    display_.setCursor(43, 34);
    if (pick == 0) display_.print(EMOTION_NAMES[emotion_]);
    else if (pick == 1) display_.print(pendingEvent_ < TOTAL_LIFE_EVENT_COUNT ? eventCategoryName(pendingEvent_) : "240 events");
    else if (pick == 2) display_.print(F("Rival grows with you"));
    else if (pick == 3) display_.print(groupId_ < 8 ? GROUP_NAMES[groupId_] : "8 social groups");
    else if (pick == 4) display_.print(F("Marriage and family"));
    else if (pick == 5) display_.print(F("Children and legacy"));
    else if (pick == 6) display_.print(F("Playable path maps"));
    else if (pick == 7) display_.print(F("16 wild creatures"));
    else if (pick == 8) display_.print(F("Excavate and donate"));
    else if (pick == 9) display_.print(F("12 fish and records"));
    else if (pick == 10) display_.print(F("Seasonal world"));
    else if (pick == 11) { display_.print(achievementCount()); display_.print(F(" / 64 earned")); }
    else if (pick == 12) display_.print(F("Living collections"));
    else if (pick == 13) display_.print(F("Advanced legacy loop"));
    else if (pick == 14) display_.print(F("Difficulty and access"));
    else if (pick == 15) display_.print(F("Interactive lessons"));
    else if (pick == 16) display_.print(F("Income and balance"));
    else display_.print(F("Logs and save health"));
    display_.setCursor(2, 56); display_.print(F("D0/D2 browse  D1 open")); display_.display(); delay(25);
  }
}

void DeepLifeSystem::todayPage() {
  uint8_t page = 0; bool oldL = HIGH, oldS = HIGH, oldR = HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { page = page == 0 ? 2 : page - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { page = static_cast<uint8_t>((page + 1) % 3); clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) {
      if (page == 0) { setEmotion(0, 3, 60); changeBond(1); popup(F("You checked in"), F("Calm and understood")); }
      else if (page == 1 && dailyProgress_ < dailyTarget_) popup(F("Daily goal"), DAILY_GOALS[dailyGoal_]);
      else if (page == 2 && weeklyProgress_ < weeklyTarget_) popup(F("Weekly goal"), WEEKLY_GOALS[weeklyGoal_]);
      markSaveDirty();
    }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); drawHeader(F("TODAY"), page + 1, 3);
    if (page == 0) {
      drawEmotionIcon(emotion_, 5, 19, false); display_.setCursor(28, 16); display_.print(EMOTION_NAMES[emotion_]);
      display_.setCursor(28, 29); display_.print(F("Intensity ")); display_.print(emotionIntensity_); display_.print(F("/10"));
      display_.setCursor(28, 42); display_.print(F("Title ")); display_.print(TITLE_NAMES[titleId_]);
      display_.setCursor(2, 55); display_.print(stageBenefit(stage));
    } else if (page == 1) {
      display_.setCursor(3, 17); display_.print(F("Daily: ")); display_.print(DAILY_GOALS[dailyGoal_]);
      display_.drawRect(3, 31, 120, 10, SSD1306_WHITE); uint8_t w = static_cast<uint8_t>(min<int>(118, (118 * min<uint8_t>(dailyProgress_, dailyTarget_)) / dailyTarget_)); display_.fillRect(4, 32, w, 8, SSD1306_WHITE);
      display_.setCursor(3, 47); display_.print(dailyProgress_); display_.print(F("/")); display_.print(dailyTarget_); display_.print(F("  Reward 2G"));
    } else {
      display_.setCursor(3, 17); display_.print(F("Weekly: ")); display_.print(WEEKLY_GOALS[weeklyGoal_]);
      display_.drawRect(3, 31, 120, 10, SSD1306_WHITE); uint8_t w = static_cast<uint8_t>(min<int>(118, (118 * min<uint8_t>(weeklyProgress_, weeklyTarget_)) / weeklyTarget_)); display_.fillRect(4, 32, w, 8, SSD1306_WHITE);
      display_.setCursor(3, 47); display_.print(weeklyProgress_); display_.print(F("/")); display_.print(weeklyTarget_); display_.print(F("  Badge + 8G"));
    }
    display_.display(); delay(25);
  }
}

void DeepLifeSystem::storyPage() {
  if (pendingEvent_ >= TOTAL_LIFE_EVENT_COUNT) { popup(F("No event waiting"), F("A new day brings one")); return; }

  auto wrapText = [](const char *text, char lines[][22], uint8_t maxLines) -> uint8_t {
    if (!text || maxLines == 0) return 0;
    memset(lines, 0, static_cast<size_t>(maxLines) * 22U);
    uint8_t line = 0, col = 0;
    const char *cursor = text;
    while (*cursor && line < maxLines) {
      while (*cursor == ' ') ++cursor;
      if (!*cursor) break;
      const char *word = cursor;
      uint8_t length = 0;
      while (cursor[length] && cursor[length] != ' ') ++length;
      if (col && col + 1 + length > 21) { ++line; col = 0; if (line >= maxLines) break; }
      if (col) lines[line][col++] = ' ';
      for (uint8_t i = 0; i < length && line < maxLines; ++i) {
        if (col >= 21) { ++line; col = 0; if (line >= maxLines) break; }
        lines[line][col++] = word[i];
      }
      if (line < maxLines) lines[line][col] = 0;
      cursor += length;
    }
    return line >= maxLines ? maxLines : static_cast<uint8_t>(line + 1);
  };

  auto showReadable = [&](const char *heading, const char *text, const char *footer) {
    char lines[16][22];
    const uint8_t count = wrapText(text, lines, 16);
    uint8_t top = 0;
    bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
    waitButtonRelease();
    while (true) {
      if (rightHoldExitCheck()) return false;
      const bool l = digitalRead(btnL_), sel = digitalRead(btnS_), r = digitalRead(btnR_);
      if (oldL == HIGH && l == LOW) { if (top) --top; clickSound(); delay(90); }
      if (oldR == HIGH && r == LOW) { if (top + 5 < count) ++top; clickSound(); delay(90); }
      if (oldS == HIGH && sel == LOW) { clickSound(); waitButtonRelease(); return true; }
      oldL = l; oldS = sel; oldR = r;
      display_.clearDisplay(); display_.setTextSize(1); display_.setTextColor(SSD1306_WHITE); display_.setTextWrap(false);
      display_.setCursor(1, 0); display_.print(heading);
      display_.drawLine(0, 8, 127, 8, SSD1306_WHITE);
      for (uint8_t row = 0; row < 5; ++row) {
        const uint8_t idx = static_cast<uint8_t>(top + row);
        if (idx >= count) break;
        display_.setCursor(1, 11 + row * 9); display_.print(lines[idx]);
      }
      display_.drawLine(0, 55, 127, 55, SSD1306_WHITE);
      display_.setCursor(1, 57);
      if (count > 5) {
        display_.print(F("D0/D2 ")); display_.print(top + 1); display_.print('/');
        display_.print(max<uint8_t>(1, static_cast<uint8_t>(count - 4))); display_.print(F(" D1 next"));
      } else display_.print(footer);
      display_.display(); delay(18);
    }
  };

  char intro[150];
  snprintf(intro, sizeof(intro), "%s\n%s", eventTitle(pendingEvent_), eventPrompt(pendingEvent_));
  if (!showReadable("LIFE EVENT", intro, "D1 choose response")) return;

  uint8_t choice = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  char optionLines[2][3][22];
  const uint8_t optionCountA = wrapText(eventChoiceLabel(pendingEvent_, 0), optionLines[0], 3);
  const uint8_t optionCountB = wrapText(eventChoiceLabel(pendingEvent_, 1), optionLines[1], 3);
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_), sel = digitalRead(btnS_), r = digitalRead(btnR_);
    if ((oldL == HIGH && l == LOW) || (oldR == HIGH && r == LOW)) { choice ^= 1U; clickSound(); delay(100); }
    if (oldS == HIGH && sel == LOW) {
      const uint8_t resolvedEvent = pendingEvent_;
      const char *picked = eventChoiceLabel(resolvedEvent, choice);
      const char *resolvedTitle = eventTitle(resolvedEvent);
      waitButtonRelease(); resolveEvent(choice); goodSound();
      char result[120]; snprintf(result, sizeof(result), "%s\n%s", resolvedTitle, picked);
      showReadable("CHOICE REMEMBERED", result, "D1 close");
      return;
    }
    oldL = l; oldS = sel; oldR = r;
    display_.clearDisplay(); display_.setTextSize(1); display_.setTextColor(SSD1306_WHITE); display_.setTextWrap(false);
    display_.setCursor(1, 0); display_.print(F("CHOOSE YOUR RESPONSE"));
    display_.drawLine(0, 8, 127, 8, SSD1306_WHITE);
    display_.setCursor(1, 11); display_.print(eventTitle(pendingEvent_));
    display_.setCursor(1, 22); display_.print(choice == 0 ? '>' : ' ');
    for (uint8_t row = 0; row < min<uint8_t>(2, optionCountA); ++row) {
      display_.setCursor(8, 22 + row * 9); display_.print(optionLines[0][row]);
    }
    display_.setCursor(1, 41); display_.print(choice == 1 ? '>' : ' ');
    for (uint8_t row = 0; row < min<uint8_t>(2, optionCountB); ++row) {
      display_.setCursor(8, 41 + row * 9); display_.print(optionLines[1][row]);
    }
    display_.setCursor(1, 59); display_.print(F("D0/D2 choose D1 OK"));
    display_.display(); delay(18);
  }
}

void DeepLifeSystem::rivalPage() {
  uint8_t action = 0; bool oldL = HIGH, oldS = HIGH, oldR = HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { action = action == 0 ? 2 : action - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { action = static_cast<uint8_t>((action + 1) % 3); clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) {
      waitButtonRelease();
      if (stage < 3) { popup(F("Rival is still young"), F("Starts at teen stage")); return; }
      if (action == 0) { bool won = random(0, 100) < 45 + rivalRespect_ / 5 + skillFit; recordGameResult(favoriteGameId, won, won ? 9 : 3); won ? winSound() : badSound(); popup(won ? F("You won the challenge") : F("Rival won this time"), won ? F("Respect increased") : F("Bond still increased")); }
      else if (action == 1) { changeByte(rivalBond_, 5); changeByte(rivalRespect_, 1); setEmotion(1, 4, 70); goodSound(); popup(F("Trained together"), F("Rival became a friend")); }
      else { changeByte(rivalBond_, 2); addCoins(1); setEmotion(0, 3, 71); popup(F("Shared a quiet snack"), F("Competition cooled")); }
      evaluateAchievements(); markSaveDirty(); return;
    }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); drawHeader(F("GENERATION RIVAL"), action + 1, 3); drawRival(7, 23, millis() / 400);
    display_.setCursor(31, 16); display_.print(F("Species #")); display_.print(rivalSpecies_);
    display_.setCursor(31, 28); display_.print(F("Bond ")); display_.print(rivalBond_); display_.print(F(" Respect ")); display_.print(rivalRespect_);
    display_.setCursor(31, 40); display_.print(F("Rank ")); display_.print(rivalRank_); display_.print(F(" W")); display_.print(rivalWins_); display_.print(F(" L")); display_.print(rivalLosses_);
    display_.setCursor(31, 52); display_.print(action == 0 ? F("D1 challenge") : action == 1 ? F("D1 train together") : F("D1 reconcile")); display_.display(); delay(25);
  }
}

void DeepLifeSystem::groupsPage() {
  uint8_t pick = groupId_ < 8 ? groupId_ : 0; bool oldL = HIGH, oldS = HIGH, oldR = HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? 7 : pick - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { pick = static_cast<uint8_t>((pick + 1) % 8); clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) {
      waitButtonRelease();
      if (stage < 2) { popup(F("Clubs unlock at child"), F("Grow a little more")); return; }
      if (groupId_ != pick && !confirm(F("JOIN GROUP"), F("Replace current club?"))) continue;
      groupId_ = pick; changeByte(groupLevel_, 1, 9); ++groupMeetings_; changeByte(rivalBond_, 1); setEmotion(1, 5, pick); goodSound(); popup(F("Group meeting held"), GROUP_NAMES[pick]); evaluateAchievements(); markSaveDirty(); return;
    }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); drawHeader(F("FRIEND GROUPS"), pick + 1, 8); drawHubIcon(3, 5, 18);
    display_.setCursor(39, 17); display_.print(GROUP_NAMES[pick]);
    display_.setCursor(39, 30); display_.print(F("Level ")); display_.print(groupId_ == pick ? groupLevel_ : 0); display_.print(F("  Meets ")); display_.print(groupId_ == pick ? groupMeetings_ : 0);
    display_.setCursor(39, 43); display_.print(groupId_ == pick ? F("Your current circle") : F("D1 join and meet"));
    display_.setCursor(2, 56); display_.print(F("Scheduled meetings")); display_.display(); delay(25);
  }
}

void DeepLifeSystem::familyPage() {
  uint8_t action = 0; const uint8_t count = 5; bool oldL = HIGH, oldS = HIGH, oldR = HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { action = action == 0 ? count - 1 : action - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { action = static_cast<uint8_t>((action + 1) % count); clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) {
      waitButtonRelease();
      if (action == 4) { legacyRelativesPage(); oldL = oldS = oldR = HIGH; continue; }
      if (partnerId >= 8) { popup(F("No spouse at home"), F("Build city romance")); return; }
      if (action == 0) { changeByte(partnerHarmony_, 4); changeByte(familyMood_, 4); addStat(happy, 1); setEmotion(1, 5, 80); popup(F("Family outing"), F("Everyone feels closer")); }
      else if (action == 1) { if (argumentState_ > 0) { argumentState_ = 0; changeByte(partnerHarmony_, 8); setEmotion(0, 5, 81); popup(F("Talked it through"), F("Argument resolved")); } else popup(F("Home is peaceful"), F("No argument today")); }
      else if (action == 2) {
        if (stage < 4 || partnerBond < 60) { popup(F("Family not ready"), F("Adult + bond 60")); return; }
        if (childCount_ >= DEEP_CHILD_COUNT) { popup(F("Home is full"), F("Three children max")); return; }
        if (!confirm(F("NEW CHILD"), F("Welcome a new child?"))) continue;
        const uint8_t i = childCount_++;
        childSpecies_[i] = static_cast<uint8_t>((speciesType + rivalSpecies_ + partnerId * 7 + generation + i * 11) % 50);
        childPersonality_[i] = static_cast<uint8_t>((temperament + partnerId + i * 3) % 8);
        childTalent_[i] = static_cast<uint8_t>((favoriteGameId + i * 5 + skillSmart + skillKind) % 13);
        childBond_[i] = 30; childAge_[i] = 0; siblingBond_[i] = 50;
        chosenHeir_ = chosenHeir_ == 255 ? 0 : chosenHeir_;
        changeByte(partnerHarmony_, 5); setEmotion(3, 8, 82); setAchievement(26); winSound(); popup(F("A child joined"), F("Traits mixed uniquely"));
      } else {
        changeByte(partnerHarmony_, 2); changeByte(familyMood_, 6); for (uint8_t i = 0; i < childCount_; ++i) changeByte(childBond_[i], 2); popup(F("Shared family dinner"), F("Chores were divided"));
      }
      evaluateAchievements(); markSaveDirty(); return;
    }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); drawHeader(F("FAMILY HOME"), action + 1, count); drawHubIcon(4, 5, 18);
    display_.setCursor(39, 16); display_.print(F("Harmony ")); display_.print(partnerHarmony_); display_.print(F(" Mood ")); display_.print(familyMood_);
    display_.setCursor(39, 29); display_.print(F("Children ")); display_.print(childCount_); display_.print(F("/3"));
    display_.setCursor(39, 42); display_.print(action == 0 ? F("Family outing") : action == 1 ? F("Reconcile") : action == 2 ? F("Welcome child") : action == 3 ? F("Family dinner") : F("Family in city"));
    display_.setCursor(2, 56); display_.print(F("Marriage grows")); display_.display(); delay(25);
  }
}

void DeepLifeSystem::legacyRelativesPage() {
  const uint8_t count = static_cast<uint8_t>((grandparentSpecies_ < 50 ? 1 : 0) + legacySiblingCount_);
  if (count == 0) { popup(F("No city relatives"), F("Choose an heir first")); return; }
  uint8_t pick = 0; bool oldL = HIGH, oldS = HIGH, oldR = HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? count - 1 : pick - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { pick = static_cast<uint8_t>((pick + 1) % count); clickSound(); delay(100); }
    const bool grand = grandparentSpecies_ < 50 && pick == 0;
    const uint8_t siblingIndex = grandparentSpecies_ < 50 ? static_cast<uint8_t>(pick - 1) : pick;
    const uint8_t relativeSpecies = grand ? grandparentSpecies_ : legacySiblingSpecies_[siblingIndex];
    uint8_t &relativeBond = grand ? grandparentBond_ : legacySiblingBond_[siblingIndex];
    if (oldS == HIGH && s == LOW) {
      waitButtonRelease(); changeByte(relativeBond, 4); changeByte(familyMood_, 3); addSkill(skillKind, 1); setEmotion(0, 5, 91); addEventLog(28, relativeSpecies, static_cast<uint16_t>(petAgeDays())); popup(grand ? F("Grandparent visited") : F("Sibling visited"), F("Family bond increased")); markSaveDirty(); return;
    }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); drawHeader(F("FAMILY IN CITY"), pick + 1, count);
    display_.drawRect(3, 15, 122, 39, SSD1306_WHITE); display_.drawLine(31, 15, 31, 54, SSD1306_WHITE);
    drawSprite16(relativeSpecies, 9, 27 + ((millis() / 450) & 1));
    display_.setCursor(37, 19); display_.print(grand ? F("Grandparent") : F("Sibling"));
    display_.setCursor(37, 32); display_.print(F("Species #")); display_.print(relativeSpecies);
    display_.setCursor(37, 44); display_.print(F("Bond ")); display_.print(relativeBond);
    display_.setCursor(2, 56); display_.print(F("D1 visit city home")); display_.display(); delay(25);
  }
}

void DeepLifeSystem::mentoringPage() {
  if (childCount_ == 0) { popup(F("No children to mentor"), F("Build a family first")); return; }
  const uint8_t optionCount = static_cast<uint8_t>(childCount_ * 2U);
  uint8_t pick = chosenHeir_ < childCount_ ? static_cast<uint8_t>(chosenHeir_ * 2U) : 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? optionCount - 1 : pick - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { pick = static_cast<uint8_t>((pick + 1) % optionCount); clickSound(); delay(100); }
    const uint8_t child = static_cast<uint8_t>(pick / 2U);
    const uint8_t action = static_cast<uint8_t>(pick % 2U);
    if (oldS == HIGH && s == LOW) {
      waitButtonRelease();
      if (action == 0) {
        mentorSkill_ = static_cast<uint8_t>((skillSmart + skillFit + skillStyle + skillKind + childTalent_[child]) % 8);
        changeByte(mentorProgress_, stage == 5 ? 3 : 1); changeByte(childTalent_[child], 1, 12); changeByte(childBond_[child], 3); setEmotion(9, 6, 90); popup(F("Lesson passed down"), F("Talent and bond grew"));
      } else {
        chosenHeir_ = child; inheritedTemperament = childPersonality_[child]; inheritedFavoriteGame = childTalent_[child]; inheritedBodyType = childSpecies_[child] % 12; inheritedTrait = childTalent_[child] % 4; popup(F("Heir selected"), F("Traits pass onward"));
      }
      evaluateAchievements(); markSaveDirty(); return;
    }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); drawHeader(F("CHILDREN & MENTOR"), pick + 1, optionCount); drawChild(childSpecies_[child], 8, 23, millis() / 400);
    display_.setCursor(31, 16); display_.print(F("Species #")); display_.print(childSpecies_[child]); display_.setCursor(31, 28); display_.print(F("Bond ")); display_.print(childBond_[child]); display_.print(F(" Talent ")); display_.print(childTalent_[child]);
    display_.setCursor(31, 40); display_.print(F("Sibling ")); display_.print(siblingBond_[child]); if (chosenHeir_ == child) display_.print(F(" HEIR"));
    display_.setCursor(31, 52); display_.print(action == 0 ? F("D1 teach skill") : F("D1 choose heir")); display_.display(); delay(25);
  }
}

void DeepLifeSystem::adventurePage() {
  uint8_t action = 0; bool oldL = HIGH, oldS = HIGH, oldR = HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { action = action == 0 ? 2 : action - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { action = static_cast<uint8_t>((action + 1) % 3); clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) {
      waitButtonRelease();
      if (stage < 2) { popup(F("Adventure needs child"), F("Babies stay near home")); return; }
      if (action == 0) expeditionMapGame();
      else if (action == 1) { expeditionSeed_ = static_cast<uint8_t>(random(1, 255)); popup(F("Map rerolled"), F("New paths generated")); }
      else { if (craftMaterials[0] > 0 && craftMaterials[2] > 0) { --craftMaterials[0]; --craftMaterials[2]; changeByte(expeditionDepth_, 1, 9); popup(F("Trail kit crafted"), F("Future maps easier")); } else popup(F("Need wood + metal"), F("Explore for materials")); }
      markSaveDirty(); return;
    }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); drawHeader(F("MAP ADVENTURES"), action + 1, 3); drawHubIcon(6, 5, 18);
    display_.setCursor(39, 16); display_.print(F("Seed ")); display_.print(expeditionSeed_); display_.print(F(" Depth ")); display_.print(expeditionDepth_);
    display_.setCursor(39, 29); display_.print(F("Wins ")); display_.print(expeditionWins_); display_.print(F(" Losses ")); display_.print(expeditionLosses_);
    display_.setCursor(39, 42); display_.print(action == 0 ? F("Play generated trail") : action == 1 ? F("Generate new map") : F("Craft trail kit"));
    display_.setCursor(2, 56); display_.print(F("Life, relics, items")); display_.display(); delay(25);
  }
}

void DeepLifeSystem::expeditionMapGame() {
  uint8_t lane = 1, column = 0, score = 0, lives = 3; bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  uint8_t obstacles[12], rewards[12];
  uint8_t seed = expeditionSeed_;
  for (uint8_t i = 0; i < 12; ++i) { seed = static_cast<uint8_t>(seed * 73U + 41U); obstacles[i] = seed % 3; seed = static_cast<uint8_t>(seed * 29U + 17U); rewards[i] = seed % 3; }
  waitButtonRelease();
  while (column < 12 && lives > 0) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW && lane > 0) { --lane; clickSound(); }
    if (oldR == HIGH && r == LOW && lane < 2) { ++lane; clickSound(); }
    if (oldS == HIGH && s == LOW) {
      ++column;
      if (column < 12) {
        if (lane == obstacles[column]) { --lives; badSound(); }
        else if (lane == rewards[column]) { ++score; goodSound(); }
      }
      delay(120);
    }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); drawHeader(F("TRAIL MAP"), column + 1, 12);
    for (uint8_t c = 0; c < 12; ++c) {
      int x = 3 + c * 10; for (uint8_t ln = 0; ln < 3; ++ln) display_.drawRect(x, 16 + ln * 13, 8, 10, SSD1306_WHITE);
      if (c >= column && obstacles[c] < 3) display_.fillRect(x + 2, 18 + obstacles[c] * 13, 4, 6, SSD1306_WHITE);
      if (c >= column && rewards[c] < 3 && rewards[c] != obstacles[c]) display_.drawCircle(x + 4, 21 + rewards[c] * 13, 2, SSD1306_WHITE);
    }
    int px = 3 + min<uint8_t>(column, 11) * 10; display_.fillRoundRect(px, 16 + lane * 13, 8, 10, 3, SSD1306_BLACK); display_.drawRoundRect(px, 16 + lane * 13, 8, 10, 3, SSD1306_WHITE);
    display_.setCursor(1, 56); display_.print(F("D0/D2 lane D1 go L")); display_.print(lives); display_.print(F(" S")); display_.print(score); display_.display(); delay(25);
  }
  if (lives > 0) {
    ++expeditionWins_; expeditionDepth_ = min<uint8_t>(9, expeditionDepth_ + 1); setEmotion(3, 6, 100);
    const uint8_t wild = static_cast<uint8_t>((expeditionSeed_ + score + season_ * 3) % DEEP_WILD_COUNT); wildSeenMask_ |= (1UL << wild); if (score >= 3) wildFriendMask_ |= (1UL << wild);
    const uint8_t fossil = static_cast<uint8_t>((expeditionSeed_ + score) % DEEP_FOSSIL_COUNT); if (fossilFragments_[fossil] < 4) ++fossilFragments_[fossil]; if (fossilFragments_[fossil] >= 4) fossilCompleteMask_ |= static_cast<uint8_t>(1U << fossil);
    craftMaterials[static_cast<uint8_t>((score + season_) % 6)] = min<uint8_t>(99, craftMaterials[(score + season_) % 6] + 1);
    if (weeklyGoal_ == 2 && weeklyProgress_ < weeklyTarget_) ++weeklyProgress_;
    winSound(); popup(F("Trail completed"), WILD_NAMES[wild]);
  } else { ++expeditionLosses_; setEmotion(6, 4, 101); badSound(); popup(F("Returned safely"), F("Prepare and try again")); }
  evaluateAchievements(); markSaveDirty();
}

void DeepLifeSystem::creaturesPage() {
  uint8_t pick = 0; bool oldL = HIGH, oldS = HIGH, oldR = HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? DEEP_WILD_COUNT - 1 : pick - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { pick = static_cast<uint8_t>((pick + 1) % DEEP_WILD_COUNT); clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW && (wildSeenMask_ & (1UL << pick))) {
      waitButtonRelease(); if (spendCoins(1)) { wildFriendMask_ |= (1UL << pick); setEmotion(1, 4, 110); goodSound(); popup(F("Left a favorite snack"), F("Creature bond grew")); } else popup(F("Need 1 coin"), F("For a field snack")); evaluateAchievements(); markSaveDirty();
    }
    oldL = l; oldS = s; oldR = r;
    const bool seen = (wildSeenMask_ & (1UL << pick)) != 0; const bool friendFound = (wildFriendMask_ & (1UL << pick)) != 0;
    display_.clearDisplay(); drawHeader(F("FIELD GUIDE"), pick + 1, DEEP_WILD_COUNT); drawWild(pick, 8, 24, millis() / 400);
    display_.setCursor(32, 18); display_.print(seen ? WILD_NAMES[pick] : "????"); display_.setCursor(32, 31); display_.print(F("Seen ")); display_.print(seen ? F("yes") : F("no")); display_.print(F("  Friend ")); display_.print(friendFound ? F("yes") : F("no"));
    display_.setCursor(32, 44); display_.print(seen ? F("D1 offer 1G snack") : F("Explore to discover"));
    display_.setCursor(2, 56); display_.print(F("Time changes wildlife")); display_.display(); delay(25);
  }
}

void DeepLifeSystem::fossilsPage() {
  uint8_t pick = 0; bool oldL = HIGH, oldS = HIGH, oldR = HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? 7 : pick - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { pick = static_cast<uint8_t>((pick + 1) % 8); clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) {
      waitButtonRelease();
      if ((fossilCompleteMask_ & (1U << pick)) == 0) { popup(F("Fossil incomplete"), F("Find 4 fragments")); }
      else if ((fossilDonatedMask_ & (1U << pick)) != 0) { popup(F("Already in museum"), F("Display is permanent")); }
      else { fossilDonatedMask_ |= static_cast<uint8_t>(1U << pick); ++museumDonations_; museumLevel_ = min<uint8_t>(9, museumDonations_ / 3); addCoins(4); setAchievement(27); winSound(); popup(F("Fossil donated"), FOSSIL_NAMES[pick]); }
      evaluateAchievements(); markSaveDirty();
    }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); drawHeader(F("ARCHAEOLOGY"), pick + 1, 8); drawFossil(pick, 8, 24);
    display_.setCursor(32, 18); display_.print(FOSSIL_NAMES[pick]); display_.setCursor(32, 31); display_.print(F("Fragments ")); display_.print(fossilFragments_[pick]); display_.print(F("/4"));
    display_.setCursor(32, 44); display_.print((fossilDonatedMask_ & (1U << pick)) ? F("Museum display active") : (fossilCompleteMask_ & (1U << pick)) ? F("D1 donate for 4G") : F("Find in caves/ruins"));
    display_.setCursor(2, 56); display_.print(F("Sets reveal hints")); display_.display(); delay(25);
  }
}

void DeepLifeSystem::fishingPage() {
  uint8_t action = 0; bool oldL = HIGH, oldS = HIGH, oldR = HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { action = action == 0 ? 1 : 0; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { action ^= 1U; clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) { waitButtonRelease(); if (action == 0) fishingGame(); else { fishSeenMask_ = fishSeenMask_; popup(F("Fishing records"), F("Browse records below")); } markSaveDirty(); return; }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); drawHeader(F("FISHING"), action + 1, 2); drawFish(static_cast<uint8_t>((season_ * 3 + visualHour()) % DEEP_FISH_COUNT), 8, 26, millis() / 400);
    display_.setCursor(35, 17); display_.print(F("Level ")); display_.print(fishingLevel_); display_.print(F(" XP ")); display_.print(fishingXp_);
    display_.setCursor(35, 30); display_.print(F("Species ")); display_.print(popcount16(fishSeenMask_)); display_.print(F("/12"));
    display_.setCursor(35, 43); display_.print(action == 0 ? F("D1 cast line") : F("D1 view summary"));
    display_.setCursor(2, 56); display_.print(F("Season/rain matter")); display_.display(); delay(25);
  }
}

void DeepLifeSystem::fishingGame() {
  uint8_t marker = 0; int8_t dir = 1; uint8_t frames = 0; bool oldS = HIGH; waitButtonRelease();
  while (frames < 100) {
    if (rightHoldExitCheck()) return;
    marker = static_cast<uint8_t>(marker + dir); if (marker >= 18) { marker = 18; dir = -1; } if (marker == 0) dir = 1;
    bool s = digitalRead(btnS_);
    if (oldS == HIGH && s == LOW) {
      const uint8_t centerDistance = marker > 9 ? marker - 9 : 9 - marker;
      uint8_t quality = centerDistance >= 9 ? 0 : static_cast<uint8_t>(10 - centerDistance);
      if ((accessibilityFlags_ & 0x04U) != 0) quality = min<uint8_t>(10, quality + 2);
      const uint8_t fish = static_cast<uint8_t>((season_ * 3 + visualHour() + rareWeather_ + fishingLevel_ + quality) % DEEP_FISH_COUNT);
      if (quality >= 4) {
        fishSeenMask_ |= static_cast<uint16_t>(1U << fish); if (quality > fishBest_[fish]) fishBest_[fish] = quality;
        fishingXp_ = static_cast<uint8_t>(min<int>(255, fishingXp_ + quality)); if (fishingXp_ >= 20 && fishingLevel_ < 9) { fishingXp_ -= 20; ++fishingLevel_; }
        if (weeklyGoal_ == 5 && weeklyProgress_ < weeklyTarget_) ++weeklyProgress_;
        addCoins(static_cast<byte>(1 + quality / 4)); setEmotion(3, 5, 120); goodSound(); popup(F("Fish caught!"), FISH_NAMES[fish]);
      } else { setEmotion(6, 3, 121); badSound(); popup(F("It slipped away"), F("Press near the center")); }
      evaluateAchievements(); markSaveDirty(); return;
    }
    oldS = s; ++frames;
    display_.clearDisplay(); drawHeader(F("CAST TIMING"), 1, 1); display_.drawRect(8, 28, 112, 12, SSD1306_WHITE); display_.fillRect(58, 29, 12, 10, SSD1306_WHITE); display_.drawFastVLine(10 + marker * 6, 25, 18, SSD1306_WHITE);
    display_.setCursor(14, 48); display_.print(F("D1 at the center")); display_.display(); delay(reducedMotion() ? 80 : 45);
  }
  popup(F("The fish left"), F("Try another cast"));
}

void DeepLifeSystem::seasonsPage() {
  uint8_t page = 0; bool oldL = HIGH, oldS = HIGH, oldR = HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { page = page == 0 ? 2 : page - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { page = static_cast<uint8_t>((page + 1) % 3); clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) { seasonalCollectionMask_ |= static_cast<uint16_t>(1U << (season_ * 4 + (rareWeather_ % 4))); setEmotion(7, 4, 130); popup(F("Season journaled"), F("Collection mark saved")); evaluateAchievements(); markSaveDirty(); }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); drawHeader(F("SEASONS & WEATHER"), page + 1, 3); drawSeasonOverlay(false, 0, millis() / 250);
    if (page == 0) { display_.setCursor(4, 17); display_.print(F("Season ")); display_.print(season_ == 0 ? F("Winter") : season_ == 1 ? F("Spring") : season_ == 2 ? F("Summer") : F("Autumn")); display_.setCursor(4, 31); display_.print(F("Weather type ")); display_.print(rareWeather_); display_.setCursor(4, 45); display_.print(F("Seen ")); display_.print(popcount16(weatherSeenMask_)); display_.print(F("/16")); }
    else if (page == 1) { display_.setCursor(4, 17); display_.print(F("Weather affects:")); display_.setCursor(4, 29); display_.print(F("crops, fish, mood,")); display_.setCursor(4, 41); display_.print(F("events, maps, health")); display_.setCursor(4, 53); display_.print(F("and NPC schedules")); }
    else { display_.setCursor(4, 17); display_.print(F("Seasonal collection")); display_.setCursor(4, 30); display_.print(popcount16(seasonalCollectionMask_)); display_.print(F("/16 moments")); display_.setCursor(4, 43); display_.print(F("D1 record today")); }
    display_.display(); delay(25);
  }
}

void DeepLifeSystem::achievementsPage() {
  uint8_t pick = 0; bool oldL = HIGH, oldS = HIGH, oldR = HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? 63 : pick - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { pick = static_cast<uint8_t>((pick + 1) % 64); clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW && hasAchievement(pick)) { titleId_ = static_cast<uint8_t>(min<int>(15, pick / 4)); badgeMask_ |= static_cast<uint16_t>(1U << (pick % 16)); popup(F("Title equipped"), TITLE_NAMES[titleId_]); markSaveDirty(); }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); drawHeader(F("ACHIEVEMENTS"), pick + 1, 64);
    display_.setCursor(2, 14); display_.print(ACHIEVEMENT_NAMES[pick]); drawHubIcon(11, 5, 29);
    display_.setCursor(39, 30); display_.print(hasAchievement(pick) ? F("COMPLETE") : F("Not complete"));
    display_.setCursor(39, 43); display_.print(hasAchievement(pick) ? F("D1 equip title") : F("Try varied play"));
    display_.setCursor(2, 56); display_.print(F("Earned ")); display_.print(achievementCount()); display_.print(F("  Badges ")); display_.print(popcount16(badgeMask_)); display_.display(); delay(25);
  }
}

void DeepLifeSystem::drawMuseumScene(uint8_t page, uint8_t frame) const {
  display_.drawRect(2, 14, 124, 41, SSD1306_WHITE); display_.drawLine(2, 45, 126, 45, SSD1306_WHITE);
  display_.drawRect(7, 20, 25, 24, SSD1306_WHITE); display_.drawRect(38, 20, 25, 24, SSD1306_WHITE); display_.drawRect(69, 20, 25, 24, SSD1306_WHITE); display_.drawRect(100, 20, 20, 24, SSD1306_WHITE);
  if (page == 0) { drawFossil(0, 12, 26); drawFossil(1, 43, 26); drawFish(9, 77, 29, frame); drawWild(2, 103, 25, frame); }
  else { drawFossil(4, 12, 26); drawFish(10, 44, 29, frame); drawWild(7, 74, 25, frame); drawChild(1, 103, 26, frame); }
}

void DeepLifeSystem::museumPage() {
  uint8_t page = 0; bool oldL = HIGH, oldS = HIGH, oldR = HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { page ^= 1U; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { page ^= 1U; clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) { museumDonations_ = static_cast<uint8_t>(popcount16(fossilDonatedMask_) + popcount16(fishSeenMask_) / 3 + popcount32(wildFriendMask_) / 4); museumLevel_ = min<uint8_t>(9, museumDonations_ / 3); evaluateAchievements(); popup(F("Museum cataloged"), F("Displays refreshed")); markSaveDirty(); }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); drawHeader(F("CITY MUSEUM"), page + 1, 2); drawMuseumScene(page, millis() / 400);
    display_.fillRect(2, 47, 124, 8, SSD1306_BLACK); display_.setCursor(4, 48); display_.print(F("Level ")); display_.print(museumLevel_); display_.print(F(" Donations ")); display_.print(museumDonations_);
    display_.setCursor(2, 56); display_.print(F("D1 refresh catalog")); display_.display(); delay(25);
  }
}

void DeepLifeSystem::newGamePlusPage() {
  bool unlocked = generation >= 5 && achievementCount() >= 16 && museumLevel_ >= 2;
  bool oldS = HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool s = digitalRead(btnS_);
    if (oldS == HIGH && s == LOW) {
      waitButtonRelease();
      if (!unlocked) { popup(F("Legacy trial locked"), F("Need Gen5, 16A, Mus2")); return; }
      if (!confirm(F("NEW GAME PLUS"), F("Raise challenge?"))) continue;
      newGamePlus_ = min<uint8_t>(9, newGamePlus_ + 1); difficultyMode = max<uint8_t>(2, difficultyMode); secretHintMask_ |= (1UL << 7); addCoins(10); setAchievement(15); winSound(); popup(F("Legacy tier raised"), F("New events and rivals")); markSaveDirty(); return;
    }
    oldS = s;
    display_.clearDisplay(); drawHeader(F("NEW GAME PLUS"), 1, 1); drawHubIcon(13, 6, 20);
    display_.setCursor(40, 17); display_.print(F("Tier ")); display_.print(newGamePlus_); display_.setCursor(40, 29); display_.print(unlocked ? F("READY") : F("LOCKED"));
    display_.setCursor(40, 41); display_.print(F("Keeps museum, tree,")); display_.setCursor(40, 51); display_.print(F("recipes and records"));
    display_.setCursor(2, 56); display_.print(F("D1 ascend legacy")); display_.display(); delay(25);
  }
}

void DeepLifeSystem::settingsPage() {
  uint8_t pick = 0; const uint8_t count = 8; bool oldL = HIGH, oldS = HIGH, oldR = HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? count - 1 : pick - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { pick = static_cast<uint8_t>((pick + 1) % count); clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) {
      waitButtonRelease();
      if (pick == 0) difficultyMode = static_cast<uint8_t>((difficultyMode + 1) % 4);
      else if (pick == 1) accessibilityFlags_ ^= 0x01U;
      else if (pick == 2) accessibilityFlags_ ^= 0x02U;
      else if (pick == 3) accessibilityFlags_ ^= 0x04U;
      else if (pick == 4) accessibilityFlags_ ^= 0x08U;
      else if (pick == 5) accessibilityFlags_ ^= 0x10U;
      else if (pick == 6) accessibilityFlags_ ^= 0x20U;
      else { soundMode = static_cast<uint8_t>((soundMode + 1) % 3); accessibilityFlags_ ^= 0x40U; }
      goodSound(); markSaveDirty();
    }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); drawHeader(F("ACCESSIBILITY"), pick + 1, count); drawHubIcon(14, 5, 18);
    display_.setCursor(39, 17);
    if (pick == 0) { display_.print(F("Difficulty ")); display_.print(difficultyMode); }
    else if (pick == 1) display_.print(F("High contrast")); else if (pick == 2) display_.print(F("Reduced motion")); else if (pick == 3) display_.print(F("Simpler minigames"));
    else if (pick == 4) display_.print(F("Longer text time")); else if (pick == 5) display_.print(F("No rapid tapping")); else if (pick == 6) display_.print(F("Confirm big choices")); else display_.print(F("Sound intensity"));
    display_.setCursor(39, 31); if (pick == 0) display_.print(F("0 relax 1 normal")); else if (pick == 7) { display_.print(F("Mode ")); display_.print(soundMode); } else display_.print((accessibilityFlags_ & (1U << (pick - 1))) ? F("ON") : F("OFF"));
    display_.setCursor(39, 44); display_.print(F("D1 toggle/change")); display_.setCursor(2, 56); display_.print(F("All modes keep all")); display_.display(); delay(25);
  }
}

void DeepLifeSystem::tutorialPage() {
  static const char *const lessons[12] = {
    "Feed and clean first", "Games grow happiness", "Rooms have safe zones", "Weather changes play",
    "Talk builds memory", "Rival grows with you", "Clubs use schedules", "Explore random maps",
    "Mentor your children", "Donate to the museum", "Back up before tests", "Hold D2 to leave"
  };
  uint8_t step = min<uint8_t>(tutorialStep_, 11); bool oldL = HIGH, oldS = HIGH, oldR = HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { step = step == 0 ? 11 : step - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { step = static_cast<uint8_t>((step + 1) % 12); clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) { tutorialStep_ = static_cast<uint8_t>(min<int>(12, step + 1)); if (tutorialStep_ >= 12) { tutorialDone_ = 1; setAchievement(28); } goodSound(); popup(F("Lesson practiced"), lessons[step]); markSaveDirty(); }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); drawHeader(F("GUIDED TUTORIAL"), step + 1, 12); drawHubIcon(15, 5, 18);
    display_.setCursor(39, 18); display_.print(lessons[step]); display_.setCursor(39, 33); display_.print(step < tutorialStep_ ? F("Completed") : F("Not practiced")); display_.setCursor(39, 46); display_.print(F("D1 mark practiced"));
    display_.setCursor(2, 56); display_.print(F("Skip; Help remembers")); display_.display(); delay(25);
  }
}

void DeepLifeSystem::economyPage() {
  uint8_t page = 0; bool oldL = HIGH, oldS = HIGH, oldR = HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { page ^= 1U; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { page ^= 1U; clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) { const uint32_t net = economyEarned_ >= economySpent_ ? economyEarned_ - economySpent_ : 0; popup(net > 200 ? F("Economy is generous") : economySpent_ > economyEarned_ ? F("Spending is too high") : F("Economy is balanced"), F("Use report to tune")); }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); drawHeader(F("ECONOMY REPORT"), page + 1, 2); drawHubIcon(16, 5, 18);
    if (page == 0) { display_.setCursor(39, 16); display_.print(F("Earned ")); display_.print(economyEarned_); display_.setCursor(39, 29); display_.print(F("Spent  ")); display_.print(economySpent_); display_.setCursor(39, 42); display_.print(F("Transactions ")); display_.print(economyTransactions_); }
    else { display_.setCursor(39, 16); display_.print(F("Today +")); display_.print(economyDayEarned_); display_.print(F(" -")); display_.print(economyDaySpent_); display_.setCursor(39, 29); display_.print(F("Wallet ")); display_.print(coins); display_.setCursor(39, 42); display_.print(F("NG+ tier ")); display_.print(newGamePlus_); }
    display_.setCursor(2, 56); display_.print(F("D1 balance check")); display_.display(); delay(25);
  }
}

void DeepLifeSystem::diagnosticsPage() {
  uint8_t page = 0; bool oldL = HIGH, oldS = HIGH, oldR = HIGH; waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { page = page == 0 ? 3 : page - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { page = static_cast<uint8_t>((page + 1) % 4); clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) { if (page == 0) popup(F("Latest reason code"), F("Shows last big change")); else if (page == 1) popup(F("Save integrity"), lastSaveHealth_ >= 90 ? F("Both slots healthy") : F("One slot will rewrite")); else if (page == 2) popup(F("Evolution hints"), secretHintMask_ ? F("Secret clues found") : F("No secret clue yet")); else popup(F("Event history"), F("Newest event is shown")); }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); drawHeader(F("DEEP DIAGNOSTICS"), page + 1, 4);
    if (page == 0) { display_.setCursor(3, 17); display_.print(F("Why code ")); display_.print(infoReason_); display_.setCursor(3, 30); display_.print(F("Emotion cause ")); display_.print(lastEmotionCause_); display_.setCursor(3, 43); display_.print(F("Pending event ")); display_.print(pendingEvent_); }
    else if (page == 1) { display_.setCursor(3, 17); display_.print(F("Save health ")); display_.print(lastSaveHealth_); display_.print(F("%")); display_.setCursor(3, 30); display_.print(F("Checks ")); display_.print(saveHealthChecks_); display_.setCursor(3, 43); display_.print(F("Migrations ")); display_.print(migrationCount_); }
    else if (page == 2) { display_.setCursor(3, 17); display_.print(F("Hint mask 0x")); display_.print(secretHintMask_); display_.setCursor(3, 30); display_.print(F("Secret forms ")); display_.print(popcount16(secretUnlockMask_)); display_.setCursor(3, 43); display_.print(F("Normal forms remain")); }
    else { display_.setCursor(3, 17); display_.print(F("Logged events ")); display_.print(eventLogCount_); if (eventLogCount_) { uint8_t i = static_cast<uint8_t>((eventLogHead_ + DEEP_EVENT_LOG_COUNT - 1) % DEEP_EVENT_LOG_COUNT); display_.setCursor(3, 30); display_.print(F("Last type ")); display_.print(eventLogType_[i]); display_.print(F(" value ")); display_.print(eventLogValue_[i]); display_.setCursor(3, 43); display_.print(F("Day ")); display_.print(eventLogDay_[i]); } }
    display_.setCursor(2, 56); display_.print(F("D1 explain this page")); display_.display(); delay(25);
  }
}

void DeepLifeSystem::developerMax() {
  emotion_ = 9; emotionIntensity_ = 10; rivalBond_ = 100; rivalRespect_ = 100; rivalRank_ = 9; rivalWins_ = 99; rivalStory_ = 5;
  groupId_ = 0; groupLevel_ = 9; groupMeetings_ = 99; partnerHarmony_ = 100; familyMood_ = 100;
  childCount_ = 3; chosenHeir_ = 0; for (uint8_t i = 0; i < 3; ++i) { childSpecies_[i] = static_cast<uint8_t>(40 + i); childPersonality_[i] = i; childTalent_[i] = 12; childBond_[i] = 100; childAge_[i] = 20; siblingBond_[i] = 100; }
  mentorProgress_ = 99; storyFlags_ = 0xFFFFFFFFUL; secretHintMask_ = 0xFFFFFFFFUL; secretUnlockMask_ = 0x00FFU; wildSeenMask_ = 0x0000FFFFUL; wildFriendMask_ = 0x0000FFFFUL;
  for (uint8_t &v : fossilFragments_) v = 4;
  fossilCompleteMask_ = 0xFF; fossilDonatedMask_ = 0xFF; fishSeenMask_ = 0x0FFF;
  for (uint8_t &v : fishBest_) v = 10;
  fishingLevel_ = 9;
  expeditionWins_ = 99; expeditionDepth_ = 9; weatherSeenMask_ = 0xFFFF; seasonalCollectionMask_ = 0xFFFF; achievementsLo_ = 0xFFFFFFFFUL; achievementsHi_ = 0xFFFFFFFFUL; badgeMask_ = 0xFFFF; titleId_ = 15; museumLevel_ = 9; museumDonations_ = 99; newGamePlus_ = 9; tutorialDone_ = 1; tutorialStep_ = 12; saveNow();
}

void DeepLifeSystem::developerReset() { reset(); saveNow(); }

uint8_t DeepLifeSystem::lowestChildBond() const {
  if (childCount_ == 0) return 100;
  uint8_t value = 100;
  for (uint8_t i = 0; i < childCount_; ++i) value = min<uint8_t>(value, childBond_[i]);
  return value;
}

uint8_t DeepLifeSystem::averageSiblingBond() const {
  if (childCount_ < 2) return 100;
  uint16_t total = 0;
  for (uint8_t i = 0; i < childCount_; ++i) total += siblingBond_[i];
  return static_cast<uint8_t>(total / childCount_);
}

void DeepLifeSystem::eventParentConversation(int partnerHarmonyDelta, int familyMoodDelta,
                                             int rivalBondDelta, int childBondDelta,
                                             uint8_t emotion, uint8_t intensity,
                                             uint8_t topic, uint16_t day) {
  changeByte(partnerHarmony_, partnerHarmonyDelta);
  changeByte(familyMood_, familyMoodDelta);
  changeByte(rivalBond_, rivalBondDelta);
  for (uint8_t i = 0; i < childCount_; ++i) changeByte(childBond_[i], childBondDelta);
  setEmotion(emotion, max<uint8_t>(1, intensity), topic);
  addEventLog(31, topic, day);
  markSaveDirty();
}
