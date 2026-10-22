#include "ParentDialogue.h"
#include <cstring>
#include <cstdio>

extern void drawCurrentPetTiny(int x, int y);
extern void clickSound();
extern void goodSound();
extern void badSound();
extern void waitButtonRelease();
extern unsigned long lastInputAt;

namespace {
constexpr int clampInt(const int value, const int minimum, const int maximum) {
  return value < minimum ? minimum : (value > maximum ? maximum : value);
}

constexpr uint8_t STYLE_COMFORT = 0;
constexpr uint8_t STYLE_LISTEN = 1;
constexpr uint8_t STYLE_COACH = 2;
constexpr uint8_t STYLE_BOUNDARY = 3;
constexpr uint8_t STYLE_ENCOURAGE = 4;
constexpr uint8_t STYLE_AUTONOMY = 5;
constexpr uint8_t STYLE_DISMISS = 6;
constexpr uint8_t STYLE_MISCHIEF = 7;

constexpr uint8_t RESPONSE_SET_COUNT = PARENT_DIALOGUE_TOPIC_COUNT;

const char *const BABBLE_SYLLABLES[] = {
  "ba", "da", "ma", "na", "la", "ya", "wa", "ga",
  "boo", "dee", "mimi", "nunu", "baba", "dada", "mama", "lulu",
  "pya", "wawa", "gigi", "nana", "bibi", "yaya", "momo", "dudu"
};

const char *const BABY_CUES[] = {
  "I saw you!", "Up, please?", "Hear my sound?", "My toy moved!",
  "I missed you.", "Warm and safe.", "Tummy feels odd.", "My eyes are sleepy.",
  "The room is big.", "I copied you!", "Something surprised me.", "Stay close?",
  "I found my toes.", "The light changed.", "My pet is funny.", "I made a mess."
};

const char *const BABY_QUESTIONS[] = {
  "What do you think I mean?", "Will you answer me?", "Can we do this together?"
};

const char *const STAGE_LEADS[5][8] = {
  {"Baba!", "Mmm...", "Da-da!", "Wawa?", "Nana!", "Boo-boo.", "Mimi!", "Ah-ah!"},
  {"Can I tell you?", "Something happened.", "I need your help.", "I was thinking.", "At school today...", "I have a question.", "Please listen.", "I am not sure."},
  {"Can we talk?", "This is awkward.", "I need advice.", "I keep thinking...", "Today was complicated.", "Please do not laugh.", "I trust you with this.", "I want your honest view."},
  {"I value your advice.", "Can I talk this through?", "Something is weighing on me.", "I need another perspective.", "I want to handle this well.", "Family question.", "Work was difficult.", "I have a decision."},
  {"Sit with me a moment.", "I remembered something.", "May I share some wisdom?", "I still need your thoughts.", "Family has changed so much.", "I wonder about my legacy.", "An old feeling returned.", "Let us talk slowly."}
};

// Four phrasing variants per topic. Combined with eight stage openings and
// multiple question endings, these produce tens of thousands of full lines
// without wasting ESP32 flash on 1,000 nearly duplicated complete sentences.
const char *const TOPIC_LINES[4][PARENT_DIALOGUE_TOPIC_COUNT] = {
{
  "I made a new sound and wanted you to hear it.", "I reached for you when the room felt big.", "You went away and I wondered if you return.", "My eyes feel heavy but I do not want to miss you.", "My tummy is asking for something.", "I made a mess and watched your face.", "I copied the way you smiled at me.", "The little companion came near me.",
  "A new classmate sat alone today.", "Someone laughed at the way I answered.", "Two friends wanted the same toy.", "The teacher gave a rule I did not like.", "I broke something by accident.", "My school work felt too hard.", "The group played without asking me.", "The teacher corrected me in front of everyone.", "A loud sound scared me at school.", "I know who made the mess, but nobody asked yet.", "One garden sprout looks weaker than the others.", "My companion would not listen to me.",
  "Friends wanted me to do something that felt wrong.", "I like things they say are strange.", "My rival beat me and bragged about it.", "I studied but my score was still low.", "My closest friend shared my secret.", "I want some space, but I do not want to hurt you.", "Everyone asks what I will become.", "I tripped and everyone noticed.", "I promised to finish a chore and forgot.", "The family keeps treating me like a little kid.",
  "A coworker blamed me for their mistake.", "I keep working even when I am exhausted.", "My partner and I disagree about a family choice.", "One child keeps testing every rule.", "I want something the family cannot easily afford.", "The neighborhood needs help, but I am already busy.", "I have ignored signs that I need rest.", "My old dream does not match my current life.",
  "I remembered the first day you cared for me.", "I wonder what our family will remember about me.", "A younger relative asked about an old family argument.", "There is something I wish I had done differently.", "Someone young wants me to teach them everything at once.", "The city is changing faster than I expected.",
  "A strong feeling is making it hard to think.", "I need care, but part of me wants to do it alone.", "The weather changed all of our plans.", "Something in the garden needs attention.", "My companion is acting different today.", "The family mood feels tense.", "I did something I am proud of.", "I tried hard and still failed.", "I do not feel well and I am worried.", "I cannot settle down enough to sleep.", "I want my favorite food again.", "I noticed the room is getting messy.", "A career decision is coming soon.", "Something happened at school that I cannot stop replaying.", "My rival wants another challenge.", "Two siblings are angry with each other.", "The season is changing and I feel different too.", "Sometimes I worry I am not good enough.", "Someone helped me and I do not know how to thank them.", "I hurt someone and now I feel ashamed.", "I saw something unsafe and froze."
},
{
  "This new babble feels important to me.", "I lifted my arms because I wanted closeness.", "When I could not see you, I felt unsure.", "I am tired and fussy at the same time.", "I cannot name the empty feeling in my belly.", "I spilled something and waited to see what happens.", "Your expression made me try the same one.", "The companion sniffed me and I squeaked.",
  "Someone new looked nervous during class.", "A classmate called my idea silly.", "Sharing turned into a loud argument.", "I understand the rule, but it feels unfair.", "I am scared to admit what I damaged.", "I kept erasing my answer until class ended.", "My friends made a team without me.", "I felt small when the teacher pointed out my error.", "I wanted to hide when the alarm sounded.", "My friend asked me to keep a dishonest secret.", "The garden is growing, but one plant is drooping.", "The companion knocked something over again.",
  "Everyone else agreed, so saying no felt impossible.", "I am changing, and I do not know what fits me.", "Losing to my rival made me want to quit.", "I am afraid my grades decide how smart I am.", "A friend apologized, but I still feel betrayed.", "I need privacy and also want you nearby.", "Choosing a future feels like closing every other door.", "I laughed too, but I was actually embarrassed.", "I avoided a responsibility until it became worse.", "I want the family to trust me with more freedom.",
  "Work felt unfair, and I almost said something cruel.", "I am tired in a way sleep has not fixed.", "My partner heard my words as criticism.", "I want to guide the children without controlling them.", "Money stress is making everyone tense.", "I want to help the city without neglecting home.", "I keep telling everyone I am fine when I am not.", "I miss a dream I set aside for the family.",
  "An old memory feels as clear as yesterday.", "I want to leave more than coins and objects.", "The younger ones know only one side of our story.", "Regret keeps visiting when the house is quiet.", "Teaching takes patience I do not always have.", "I am proud of the new city and homesick for the old one.",
  "I can name the feeling, but I cannot calm it yet.", "Asking for help makes me feel weak.", "Bad weather made me angry even though nobody caused it.", "I do not know whether to rescue or replace the plant.", "My companion seems sad, restless, or bored.", "Everyone is speaking, but nobody feels heard.", "I succeeded and now I feel pressure to do it again.", "Failing made me want to hide the whole attempt.", "Being sick makes every worry feel bigger.", "My thoughts keep looping when the lights go low.", "I love this food, but maybe I ask for it too often.", "I know I should clean, but I keep putting it off.", "The safe career and the exciting one pull me apart.", "I was told to choose between two friends at school.", "My rival says winning proves who is better.", "The siblings both say the other one started it.", "A new season makes old routines feel wrong.", "I compare myself to everyone around me.", "I want my thanks to feel sincere, not automatic.", "I want to apologize without making excuses.", "I did not know whether to run, hide, or ask for help."
},
{
  "My sounds change when you answer me.", "Being held helps my whole body relax.", "I am learning that people can leave and return.", "I need help moving from play into sleep.", "I am trying to tell you what my body needs.", "I am watching whether mistakes make you angry.", "I learn feelings by studying your face.", "I want to know if the companion is safe.",
  "I wanted to invite the new student, but I felt shy.", "I wanted to speak up when they teased me.", "I was not sure how to make sharing fair.", "I followed the rule, but my friend did not.", "I could blame someone else and avoid trouble.", "I want help without having the work done for me.", "I wondered if being left out means they dislike me.", "I respect the teacher and still feel hurt.", "I felt afraid and embarrassed about being afraid.", "Telling the truth might get my friend in trouble.", "The plant problem may be water, light, or patience.", "I got frustrated and raised my voice at the companion.",
  "I want to belong without ignoring my own judgment.", "I want to be accepted without pretending.", "I admire my rival and resent them at the same time.", "I need a plan that does not turn studying into panic.", "I do not know whether trust can be rebuilt.", "I want you to respect my space without disappearing.", "I have interests, but no single perfect answer.", "I worry everyone will remember my awkward moment.", "I know responsibility matters more than excuses.", "I want more freedom and know it requires trust.",
  "I need to respond professionally without accepting blame.", "I am losing patience with people I care about.", "We agree on the goal but disagree on the method.", "The child needs limits and also connection.", "I do not want money fear to control our choices.", "I may need to ask others to share the community work.", "Rest feels irresponsible even though I need it.", "I wonder whether an old dream can take a new form.",
  "Some memories make me feel young again.", "I want my lessons to help rather than burden the family.", "I do not want old conflict to become inherited resentment.", "I may still have time to repair part of my regret.", "The student needs room to learn through mistakes.", "Change can be beautiful and still feel like loss.",
  "I need a way to respond instead of only react.", "I am deciding between immediate comfort and learning a skill.", "We may need a new plan rather than forcing the old one.", "I want to care for the garden without panicking.", "I need to understand what the companion is communicating.", "I think the family needs a calmer conversation.", "I want to celebrate without becoming arrogant.", "I need to learn from failure without becoming it.", "I want honest reassurance, not pretend certainty.", "I need a bedtime routine that feels safe.", "I want enjoyment and balance, not a fight about food.", "I need a small first step toward a cleaner space.", "I want a career choice that matches my values.", "I need a safe way to respond to the school problem.", "I want competition to help me grow, not become cruel.", "I want to be fair without ignoring who was hurt.", "My energy and interests shift with the season.", "I need to separate my worth from one result.", "I want to show gratitude through action.", "I want to repair the harm, not only stop feeling guilty.", "I want a clear safety plan before it happens again."
},
{
  "I babbled just to start a conversation with you.", "I reached out because I trust your arms.", "I am testing whether our connection stays when you leave.", "My body needs sleep and my mind wants company.", "I am using sound, movement, and expression to ask.", "I need to learn that messes can be repaired safely.", "I think your smile means we are okay.", "I want to make friends with the companion gently.",
  "I could help the new student feel included tomorrow.", "I want words that are brave without being mean.", "I want everyone to feel the solution is fair.", "I wonder when it is right to question a rule respectfully.", "I want to tell the truth and help repair the damage.", "I want a study plan I can actually follow.", "I want to ask to join without begging for approval.", "I want to learn from correction without feeling humiliated.", "I want a plan for what to do when I get scared.", "I want to be loyal without helping someone lie.", "Maybe caring for the weak plant can teach me patience.", "I want to correct the companion without frightening it.",
  "I want a response ready before pressure happens again.", "I want to explore who I am safely.", "I want to compete hard and still respect my rival.", "I want support that helps me own my learning.", "I need boundaries before I can risk trusting again.", "I want a way to ask for space respectfully.", "I could test different paths instead of choosing forever.", "I want to recover instead of avoiding everyone.", "I need to make a repair plan and follow it.", "I want the family to notice that I am growing.",
  "I want to document facts and speak without attacking.", "I need a boundary before exhaustion becomes resentment.", "I want us to listen for the fear under the argument.", "I want consistent rules that still leave room for mercy.", "I need a family plan that protects needs and hopes.", "I can contribute and also organize help from others.", "I want to model healthy rest instead of secret burnout.", "I may be able to honor the dream in a smaller way.",
  "I want to tell the memory while I still remember the details.", "I want my legacy to be how people felt around me.", "I want to explain the past without choosing villains.", "I want to turn regret into one act of repair.", "I want to teach the reason, not only the answer.", "I want to welcome change without erasing our history.",
  "I want to practice handling this feeling with you.", "I want help that builds my confidence for next time.", "I want to adapt rather than blame the weather.", "I want to learn what the garden is showing us.", "I want to care for the companion as a teammate.", "I want each family member to feel safe speaking.", "I want to share this success with the people who helped.", "I want to try again with a different strategy.", "I want to understand the symptoms and next steps.", "I want us to build a calm ending to the day.", "I want to enjoy favorites without losing variety.", "I want to clean with a plan instead of shame.", "I want to compare the real costs and benefits.", "I want to know which adult or friend can help.", "I want to set a fair rule for our competition.", "I want the siblings to repair, not only stop fighting.", "I want seasonal changes to become family traditions.", "I want to remember qualities that scores cannot measure.", "I want to say exactly what their help meant.", "I want to ask what repair would help them.", "I want to trust my warning feeling and seek support."
}
};

// Topic-specific response data: no topic shares unrelated choices.
#include "ParentDialogueResponseData.h"

uint8_t clampedStageIndex(uint8_t stage) {
  if (stage <= 1) return 0;
  if (stage >= 5) return 4;
  return static_cast<uint8_t>(stage - 1);
}

// Hard topic gates. A saved or randomly queued topic is never allowed to
// cross into a life stage for which its authored wording was not written.
bool topicAllowedForStage(uint8_t topic, uint8_t stage) {
  if (stage == 0 || topic >= PARENT_DIALOGUE_TOPIC_COUNT) return false;
  if (topic <= 7) return stage == 1;       // baby-only babble contexts
  if (topic <= 19) return stage == 2;      // child and school wording
  if (topic <= 29) return stage == 3;      // teen wording
  if (topic <= 37) return stage == 4;      // adult wording
  if (topic <= 43) return stage >= 5;      // elder wording
  if (topic == 56) return stage >= 4;      // career decision
  if (topic == 57) return stage == 2 || stage == 3; // school problem
  if (topic == 58) return stage >= 2;      // age-scaled rivalry
  if (topic == 61) return stage >= 2;      // self-worth
  return stage >= 2;                       // shared care/life contexts
}
}

ParentDialogueSystem::ParentDialogueSystem(size_t storageBase, Adafruit_SSD1306 &display,
                                           uint8_t buttonLeft, uint8_t buttonSelect,
                                           uint8_t buttonRight)
    : storeA_(storageBase), storeB_(storageBase + sizeof(ParentDialogueSave)),
      display_(display), btnL_(buttonLeft), btnS_(buttonSelect), btnR_(buttonRight) {
  reset();
}

void ParentDialogueSystem::reset() {
  sequence_ = 0;
  nextPromptTick_ = 0; lastPromptTick_ = 0;
  pendingSeed_ = 0; pendingTopic_ = 255; pendingVariant_ = 0;
  promptMode_ = 2; dailyCount_ = 0; dailyDay_ = 65535; dismissedCount_ = 0;
  generationConversations_ = 0; lifetimeConversations_ = 0; lastDialogueCode_ = 0;
  memset(stageConversations_, 0, sizeof(stageConversations_));
  memset(recentTopics_, 255, sizeof(recentTopics_)); recentHead_ = 0;
  openness_ = 35; confidence_ = 30; empathy_ = 35; responsibility_ = 25;
  resilience_ = 30; honesty_ = 35; independence_ = 20; emotionWords_ = 20;
  schoolConfidence_ = 30; socialConfidence_ = 30; safetyConfidence_ = 30;
  attachmentSecurity_ = 35;
  memset(responseStyleCount_, 0, sizeof(responseStyleCount_));
  schoolTalks_ = familyTalks_ = petTalks_ = gardenTalks_ = difficultTalks_ = 0;
}

uint32_t ParentDialogueSystem::checksum(const ParentDialogueSave &save) const {
  const uint8_t *p = reinterpret_cast<const uint8_t *>(&save);
  const size_t n = offsetof(ParentDialogueSave, checksum);
  uint32_t h = 2166136261UL;
  for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619UL; }
  return h;
}

bool ParentDialogueSystem::valid(const ParentDialogueSave &save) const {
  return save.magic == PARENT_DIALOGUE_SAVE_MAGIC &&
         save.version == PARENT_DIALOGUE_SAVE_VERSION &&
         save.checksum == checksum(save);
}

void ParentDialogueSystem::fillSave(ParentDialogueSave &s) const {
  memset(&s, 0, sizeof(s));
  s.magic = PARENT_DIALOGUE_SAVE_MAGIC; s.version = PARENT_DIALOGUE_SAVE_VERSION;
  s.sequence = sequence_ + 1;
  s.nextPromptTick = nextPromptTick_; s.lastPromptTick = lastPromptTick_;
  s.pendingSeed = pendingSeed_; s.pendingTopic = pendingTopic_; s.pendingVariant = pendingVariant_;
  s.promptMode = promptMode_; s.dailyCount = dailyCount_; s.dailyDay = dailyDay_; s.dismissedCount = dismissedCount_;
  s.generationConversations = generationConversations_; s.lifetimeConversations = lifetimeConversations_;
  memcpy(s.stageConversations, stageConversations_, sizeof(stageConversations_));
  s.lastDialogueCode = lastDialogueCode_; memcpy(s.recentTopics, recentTopics_, sizeof(recentTopics_)); s.recentHead = recentHead_;
  s.openness = openness_; s.confidence = confidence_; s.empathy = empathy_; s.responsibility = responsibility_;
  s.resilience = resilience_; s.honesty = honesty_; s.independence = independence_; s.emotionWords = emotionWords_;
  s.schoolConfidence = schoolConfidence_; s.socialConfidence = socialConfidence_; s.safetyConfidence = safetyConfidence_;
  s.attachmentSecurity = attachmentSecurity_; memcpy(s.responseStyleCount, responseStyleCount_, sizeof(responseStyleCount_));
  s.schoolTalks = schoolTalks_; s.familyTalks = familyTalks_; s.petTalks = petTalks_; s.gardenTalks = gardenTalks_;
  s.difficultTalks = difficultTalks_; s.checksum = checksum(s);
}

void ParentDialogueSystem::applySave(const ParentDialogueSave &s) {
  sequence_ = s.sequence; nextPromptTick_ = s.nextPromptTick; lastPromptTick_ = s.lastPromptTick;
  pendingSeed_ = s.pendingSeed; pendingTopic_ = s.pendingTopic < PARENT_DIALOGUE_TOPIC_COUNT ? s.pendingTopic : 255;
  pendingVariant_ = s.pendingVariant % 4; promptMode_ = min<uint8_t>(s.promptMode, 2);
  dailyCount_ = s.dailyCount; dailyDay_ = s.dailyDay; dismissedCount_ = s.dismissedCount;
  generationConversations_ = s.generationConversations; lifetimeConversations_ = s.lifetimeConversations;
  memcpy(stageConversations_, s.stageConversations, sizeof(stageConversations_)); lastDialogueCode_ = s.lastDialogueCode;
  memcpy(recentTopics_, s.recentTopics, sizeof(recentTopics_)); recentHead_ = s.recentHead % PARENT_DIALOGUE_RECENT_COUNT;
  openness_ = min<uint8_t>(s.openness, 100); confidence_ = min<uint8_t>(s.confidence, 100);
  empathy_ = min<uint8_t>(s.empathy, 100); responsibility_ = min<uint8_t>(s.responsibility, 100);
  resilience_ = min<uint8_t>(s.resilience, 100); honesty_ = min<uint8_t>(s.honesty, 100);
  independence_ = min<uint8_t>(s.independence, 100); emotionWords_ = min<uint8_t>(s.emotionWords, 100);
  schoolConfidence_ = min<uint8_t>(s.schoolConfidence, 100); socialConfidence_ = min<uint8_t>(s.socialConfidence, 100);
  safetyConfidence_ = min<uint8_t>(s.safetyConfidence, 100); attachmentSecurity_ = min<uint8_t>(s.attachmentSecurity, 100);
  memcpy(responseStyleCount_, s.responseStyleCount, sizeof(responseStyleCount_));
  schoolTalks_ = s.schoolTalks; familyTalks_ = s.familyTalks; petTalks_ = s.petTalks;
  gardenTalks_ = s.gardenTalks; difficultTalks_ = s.difficultTalks;
}

bool ParentDialogueSystem::load() {
  ParentDialogueSave a = storeA_.read(), b = storeB_.read();
  const bool va = valid(a), vb = valid(b);
  if (!va && !vb) { reset(); return false; }
  applySave(va && vb ? (a.sequence >= b.sequence ? a : b) : (va ? a : b));
  return true;
}

void ParentDialogueSystem::saveNow() {
  ParentDialogueSave s; fillSave(s);
  if (s.sequence & 1UL) storeA_.write(s); else storeB_.write(s);
  sequence_ = s.sequence;
}

void ParentDialogueSystem::changeByte(uint8_t &value, int amount, uint8_t maximum) {
  value = static_cast<uint8_t>(clampInt(static_cast<int>(value) + amount, 0, static_cast<int>(maximum)));
}

void ParentDialogueSystem::startGeneration(uint16_t generation, uint32_t ageTicks) {
  (void)generation;
  generationConversations_ = 0;
  memset(stageConversations_, 0, sizeof(stageConversations_));
  pendingTopic_ = 255; pendingSeed_ = 0; pendingVariant_ = 0;
  dailyCount_ = 0; dailyDay_ = 65535; dismissedCount_ = 0;
  memset(recentTopics_, 255, sizeof(recentTopics_)); recentHead_ = 0;
  // The player's long-term parenting tendencies remain, but each child starts
  // with a fresh relationship and room to develop.
  const uint16_t warm = responseStyleCount_[STYLE_COMFORT] + responseStyleCount_[STYLE_LISTEN];
  const uint16_t coach = responseStyleCount_[STYLE_COACH] + responseStyleCount_[STYLE_ENCOURAGE];
  openness_ = static_cast<uint8_t>(constrain(28 + static_cast<int>(warm / 8), 25, 50));
  confidence_ = static_cast<uint8_t>(constrain(26 + static_cast<int>(coach / 10), 25, 50));
  empathy_ = 32; responsibility_ = 24; resilience_ = 28; honesty_ = 32;
  independence_ = 18; emotionWords_ = 18; schoolConfidence_ = 30;
  socialConfidence_ = 30; safetyConfidence_ = 30; attachmentSecurity_ = 32;
  nextPromptTick_ = ageTicks + 2; lastPromptTick_ = ageTicks;
}

void ParentDialogueSystem::scheduleNext(const ParentDialogueContext &ctx, bool afterDismissal) {
  uint32_t low = 10, high = 22;
  if (ctx.stage <= 1) { low = 3; high = 9; }
  else if (ctx.stage == 2) { low = 7; high = 17; }
  else if (ctx.stage == 3) { low = 10; high = 24; }
  else if (ctx.stage == 4) { low = 14; high = 34; }
  else { low = 10; high = 28; }
  if (promptMode_ == 0) { low *= 2; high *= 2; }
  else if (promptMode_ == 2) { low = max<uint32_t>(2, low * 2 / 3); high = max<uint32_t>(low + 1, high * 2 / 3); }
  // A child who feels safe and heard initiates more conversations. A very
  // closed-off child still speaks, but needs a little more time between talks.
  if (openness_ >= 75 || attachmentSecurity_ >= 80) {
    low = max<uint32_t>(2, low * 3 / 4); high = max<uint32_t>(low + 1, high * 3 / 4);
  } else if (openness_ < 25 || attachmentSecurity_ < 25) {
    low = low * 5 / 4; high = high * 5 / 4;
  }
  if (afterDismissal) { low = max<uint32_t>(4, low / 2); high = max<uint32_t>(low + 1, high / 2); }
  nextPromptTick_ = ctx.ageTicks + static_cast<uint32_t>(random(static_cast<long>(low), static_cast<long>(high + 1)));
}

bool ParentDialogueSystem::recentTopic(uint8_t topic) const {
  for (uint8_t v : recentTopics_) if (v == topic) return true;
  return false;
}

void ParentDialogueSystem::selectTopic(const ParentDialogueContext &ctx, bool manual) {
  uint8_t pool[128]; uint8_t count = 0;
  auto add = [&](uint8_t topic, uint8_t weight) {
    if (!topicAllowedForStage(topic, ctx.stage)) return;
    for (uint8_t i = 0; i < weight && count < sizeof(pool); ++i) pool[count++] = topic;
  };

  if (ctx.stage <= 1) {
    for (uint8_t t = 0; t <= 7; ++t) add(t, 2);
    if (ctx.full <= 3) add(4, 5);
    if (ctx.energy <= 3) add(3, 5);
    if (ctx.clean <= 3 || ctx.poop) add(5, 5);
    if (ctx.ownerBond < 35 || ctx.ownerTrust < 35) add(2, 4);
    if (ctx.companionType < 8) add(7, 3);
  } else if (ctx.stage == 2) {
    for (uint8_t t = 8; t <= 19; ++t) add(t, 2);
    add(57, 4); add(61, 2); add(62, 2); add(63, 2); add(64, 2);
  } else if (ctx.stage == 3) {
    for (uint8_t t = 20; t <= 29; ++t) add(t, 3);
    add(57, 3); add(58, 3); add(61, 3); add(63, 2); add(64, 2);
  } else if (ctx.stage == 4) {
    for (uint8_t t = 30; t <= 37; ++t) add(t, 3);
    add(49, 2); add(56, 3); add(62, 2); add(63, 2);
  } else {
    for (uint8_t t = 38; t <= 43; ++t) add(t, 4);
    add(49, 3); add(62, 2); add(63, 2);
  }

  if (ctx.emotionIntensity >= 5) add(44, 5);
  if (ctx.pendingCallType || ctx.health <= 3 || ctx.full <= 2 || ctx.clean <= 2) add(45, 5);
  if (ctx.weather >= 4 || ctx.weather == 1) add(46, 3);
  if (ctx.gardenCount) add(47, ctx.gardenHealth < 45 || ctx.gardenWater < 35 ? 6 : 2);
  if (ctx.companionType < 8) add(48, ctx.companionBond < 45 || ctx.companionMood < 40 ? 6 : 2);
  if (ctx.childCount || ctx.partnerHarmony < 55 || ctx.familyMood < 55) add(49, 5);
  if (ctx.happy >= 8 || confidence_ >= 70) add(50, 2);
  if (ctx.happy <= 3 || schoolConfidence_ < 35) add(51, 4);
  if (ctx.sick || ctx.health <= 4) add(52, 6);
  if (ctx.energy <= 3 || ctx.hour >= 21) add(53, 4);
  if (ctx.full <= 4 || ctx.favoriteFood < 255) add(54, 2);
  if (ctx.clean <= 4 || ctx.poop) add(55, 4);
  if (ctx.stage >= 4 && ctx.careerId < 10) add(56, 4);
  if ((ctx.stage == 2 || ctx.stage == 3) && (ctx.location == 3 || ctx.hour < 16)) add(57, 5);
  if (ctx.stage >= 3) add(58, ctx.rivalBond < 45 ? 4 : 2);
  if (ctx.childCount >= 2 || ctx.siblingBond < 50) add(59, 4);
  add(60, 1); add(61, ctx.ownerTrust < 45 ? 4 : 1); add(62, 1); add(63, ctx.mischief > 2 ? 3 : 1); add(64, 1);

  if (count == 0) { add(ctx.stage <= 1 ? 0 : 44, 1); }
  uint8_t picked = pool[random(0, count)];
  for (uint8_t tries = 0; tries < 12 && recentTopic(picked); ++tries) picked = pool[random(0, count)];
  pendingTopic_ = picked;
  pendingVariant_ = static_cast<uint8_t>(random(0, 4));
  pendingSeed_ = static_cast<uint16_t>((ctx.ageTicks * 37UL + ctx.ownerBond * 19U + ctx.ownerTrust * 11U + random(1, 65535)) & 0xFFFFU);
  if (manual && pendingTopic_ <= 7 && ctx.stage > 1) pendingTopic_ = 44;
}

void ParentDialogueSystem::tick(const ParentDialogueContext &ctx, bool catchUpMode) {
  if (ctx.stage == 0) return;
  if (hasPending() && !topicAllowedForStage(pendingTopic_, ctx.stage)) pendingTopic_ = 255;
  if (dailyDay_ != ctx.ageDays) { dailyDay_ = ctx.ageDays; dailyCount_ = 0; }
  if (nextPromptTick_ == 0) scheduleNext(ctx);
  if (hasPending() || ctx.ageTicks < nextPromptTick_) return;
  const uint8_t maxDaily = ctx.stage <= 1 ? (promptMode_ == 2 ? 6 : 4) :
                           ctx.stage == 2 ? (promptMode_ == 2 ? 5 : 3) :
                           (promptMode_ == 2 ? 4 : 2);
  if (dailyCount_ >= maxDaily) { nextPromptTick_ = ctx.ageTicks + 12; return; }
  selectTopic(ctx, false);
  if (catchUpMode) {
    // Keep one conversation waiting after an offline period, but do not open UI
    // or generate a backlog of dozens of missed conversations.
    nextPromptTick_ = ctx.ageTicks + 6;
  }
}

bool ParentDialogueSystem::shouldAutoOpen(const ParentDialogueContext &ctx, unsigned long idleMs) const {
  if (!hasPending() || !topicAllowedForStage(pendingTopic_, ctx.stage) || ctx.stage == 0 || ctx.sick > 1 || ctx.ageTicks < nextPromptTick_) return false;
  return idleMs >= 3500UL;
}

void ParentDialogueSystem::scheduleManual(const ParentDialogueContext &ctx) {
  if (hasPending() && !topicAllowedForStage(pendingTopic_, ctx.stage)) pendingTopic_ = 255;
  if (!hasPending()) selectTopic(ctx, true);
}

uint8_t ParentDialogueSystem::responseSetForTopic(uint8_t topic) const {
  return topic < PARENT_DIALOGUE_TOPIC_COUNT ? topic : 0;
}

uint8_t ParentDialogueSystem::topicCategory(uint8_t topic) const {
  if ((topic >= 8 && topic <= 17) || topic == 23 || topic == 57) return 1; // school
  if (topic == 19 || topic == 48 || topic == 7) return 2; // companion
  if (topic == 18 || topic == 47) return 3; // garden
  if ((topic >= 29 && topic <= 34) || (topic >= 38 && topic <= 43) || topic == 49 || topic == 59) return 4; // family
  if (topic == 9 || topic == 14 || topic == 16 || topic == 20 || topic == 24 || topic == 27 || topic == 44 || topic == 51 || topic == 52 || topic == 61 || topic == 63 || topic == 64) return 5;
  return 0;
}

const char *ParentDialogueSystem::topicLine(uint8_t topic, uint8_t variant) const {
  return TOPIC_LINES[variant % 4][topic % PARENT_DIALOGUE_TOPIC_COUNT];
}

const char *ParentDialogueSystem::topicQuestion(uint8_t topic, uint8_t variant) const {
  const uint8_t set = responseSetForTopic(topic);
  return RESPONSE_QUESTIONS[set % RESPONSE_SET_COUNT][(pendingSeed_ + variant) % 3U];
}

void ParentDialogueSystem::buildBabyPrompt(const ParentDialogueContext &ctx, char *page1, size_t page1Size,
                                           char *page2, size_t page2Size) const {
  const uint8_t a = pendingSeed_ % 24U;
  const uint8_t b = (pendingSeed_ / 7U + ctx.temperament * 3U) % 24U;
  const uint8_t cue = (pendingSeed_ / 13U + pendingTopic_ + ctx.hour) % 16U;
  snprintf(page1, page1Size, "%s-%s! %s", BABBLE_SYLLABLES[a], BABBLE_SYLLABLES[b], BABY_CUES[cue]);
  snprintf(page2, page2Size, "%s", BABY_QUESTIONS[(pendingSeed_ / 31U) % 3U]);
}

void ParentDialogueSystem::buildPrompt(const ParentDialogueContext &ctx, char *page1, size_t page1Size,
                                       char *page2, size_t page2Size) const {
  if (ctx.stage <= 1) { buildBabyPrompt(ctx, page1, page1Size, page2, page2Size); return; }
  const uint8_t stageIndex = clampedStageIndex(ctx.stage);
  const char *lead = STAGE_LEADS[stageIndex][(pendingSeed_ + pendingVariant_) % 8U];
  const char *line = topicLine(pendingTopic_, pendingVariant_);
  // The dialogue runner splits this full sentence across as many OLED pages
  // as needed, so no context sentence is silently clipped.
  snprintf(page1, page1Size, "%s %s", lead, line);
  snprintf(page2, page2Size, "%s", topicQuestion(pendingTopic_, pendingVariant_));
}

const char *ParentDialogueSystem::choiceLabel(uint8_t responseSet, uint8_t choice) const {
  const ChoiceText &t = CHOICE_TEXT[responseSet % RESPONSE_SET_COUNT];
  return choice == 0 ? t.a : (choice == 1 ? t.b : t.c);
}

uint8_t ParentDialogueSystem::choiceStyle(uint8_t responseSet, uint8_t choice) const {
  return CHOICE_STYLES[responseSet % RESPONSE_SET_COUNT][choice % 3];
}

const char *ParentDialogueSystem::resultReply(uint8_t stage, uint8_t responseSet,
                                                    uint8_t choice) const {
  (void)stage;
  return RESPONSE_RESULTS[responseSet % RESPONSE_SET_COUNT][choice % 3];
}

void ParentDialogueSystem::applyChoice(uint8_t topic, uint8_t style, ParentDialogueOutcome &o) {
  o.responseStyle = style; o.emotion = 0; o.emotionIntensity = 4;
  bool positive = true;
  switch (style) {
    case STYLE_COMFORT:
      o.bond += 3; o.trust += 3; o.happy += 2; changeByte(attachmentSecurity_, 3); changeByte(emotionWords_, 2); changeByte(openness_, 2); changeByte(empathy_, 1); break;
    case STYLE_LISTEN:
      o.bond += 2; o.trust += 4; o.happy += 1; changeByte(openness_, 4); changeByte(empathy_, 2); changeByte(honesty_, 2); changeByte(attachmentSecurity_, 2); break;
    case STYLE_COACH:
      o.bond += 1; o.trust += 2; o.smart += 1; changeByte(confidence_, 3); changeByte(resilience_, 3); changeByte(responsibility_, 1); changeByte(independence_, 1); break;
    case STYLE_BOUNDARY:
      o.trust += 2; o.discipline += 1; changeByte(responsibility_, 4); changeByte(safetyConfidence_, 2); changeByte(honesty_, 1); break;
    case STYLE_ENCOURAGE:
      o.bond += 2; o.trust += 2; o.happy += 2; changeByte(confidence_, 4); changeByte(resilience_, 2); changeByte(openness_, 1); break;
    case STYLE_AUTONOMY:
      o.trust += 2; changeByte(independence_, 4); changeByte(confidence_, 2); changeByte(responsibility_, 1); break;
    case STYLE_DISMISS:
      positive = false; o.bond -= 2; o.trust -= 3; o.happy -= 1; changeByte(openness_, -4); changeByte(attachmentSecurity_, -2); changeByte(resilience_, -1); o.emotion = 4; o.emotionIntensity = 5; break;
    case STYLE_MISCHIEF:
      positive = false; o.trust -= 2; o.mischief += 2; changeByte(honesty_, -3); changeByte(responsibility_, -3); changeByte(empathy_, -1); o.emotion = 5; o.emotionIntensity = 4; break;
  }
  if (responseStyleCount_[style] < 255) ++responseStyleCount_[style];

  const uint8_t category = topicCategory(topic);
  if (category == 1) {
    ++schoolTalks_;
    if (positive) changeByte(schoolConfidence_, style == STYLE_COACH || style == STYLE_BOUNDARY ? 4 : 2);
    else changeByte(schoolConfidence_, -3);
  } else if (category == 2) {
    ++petTalks_; o.companionBond += positive ? 3 : -2; o.companionMood += positive ? 2 : -2;
  } else if (category == 3) {
    ++gardenTalks_; if (positive) { o.gardenWater += 1; o.gardenCare += 1; }
  } else if (category == 4) {
    ++familyTalks_; o.familyMood += positive ? 3 : -2; o.partnerHarmony += positive ? 2 : -1;
  } else if (category == 5) {
    ++difficultTalks_; if (positive) changeByte(resilience_, 2);
  }

  if (topic == 8 || topic == 10 || topic == 14 || topic == 24 || topic == 59) {
    if (positive) changeByte(socialConfidence_, 3); else changeByte(socialConfidence_, -2);
  }
  if (topic == 9 || topic == 16 || topic == 20 || topic == 64) {
    if (style == STYLE_BOUNDARY || style == STYLE_COACH) changeByte(safetyConfidence_, 4);
  }
  if (topic == 12 || topic == 17 || topic == 28 || topic == 63) {
    if (style == STYLE_BOUNDARY || style == STYLE_COACH || style == STYLE_LISTEN) changeByte(honesty_, 3);
  }
  if (topic == 22 || topic == 58) o.rivalBond += positive ? 2 : -1;

}

void ParentDialogueSystem::printWrapped(const char *text, int x, int y, uint8_t maxChars,
                                        uint8_t maxLines) const {
  if (!text) return;
  char line[32]; uint8_t len = 0, lines = 0;
  const char *p = text;
  while (*p && lines < maxLines) {
    while (*p == ' ') ++p;
    const char *word = p; uint8_t wordLen = 0;
    while (p[wordLen] && p[wordLen] != ' ') ++wordLen;
    if (wordLen == 0) break;
    if (len > 0 && len + 1 + wordLen > maxChars) {
      line[len] = 0; display_.setCursor(x, y + lines * 9); display_.print(line); ++lines; len = 0;
      if (lines >= maxLines) break;
    }
    if (len > 0) line[len++] = ' ';
    uint8_t copy = min<uint8_t>(wordLen, static_cast<uint8_t>(sizeof(line) - 1 - len));
    memcpy(line + len, word, copy); len += copy; p += wordLen;
  }
  if (len > 0 && lines < maxLines) { line[len] = 0; display_.setCursor(x, y + lines * 9); display_.print(line); }
}

void ParentDialogueSystem::drawDialoguePage(const ParentDialogueContext &ctx, const char *text,
                                             uint8_t page, uint8_t pages) const {
  display_.clearDisplay(); display_.setTextSize(1); display_.setTextColor(WHITE); display_.setTextWrap(false);
  display_.fillRoundRect(1, 1, 25, 17, 4, BLACK); display_.drawRoundRect(1, 1, 25, 17, 4, WHITE);
  drawCurrentPetTiny(4, 2);
  display_.setCursor(31, 2); display_.print(ctx.stage <= 1 ? F("BABY BABBLE") : F("TAMA TALK"));
  display_.setCursor(108, 2); display_.print(page); display_.print('/'); display_.print(pages);
  display_.drawRoundRect(2, 20, 124, 34, 5, WHITE);
  printWrapped(text, 7, 24, 19, 4);
  display_.setCursor(3, 57); display_.print(F("D1 next"));
  display_.display();
}

bool ParentDialogueSystem::waitForContinue(bool allowDismiss) const {
  // v43 dialogue is intentionally persistent. The parameter remains for save
  // and source compatibility, but no narrative page can time out or be
  // dismissed accidentally; D1 is the only way forward.
  (void)allowDismiss;
  waitButtonRelease();
  while (true) {
    if (digitalRead(btnS_) == LOW) { clickSound(); waitButtonRelease(); return true; }
    lastInputAt = millis(); delay(10);
  }
}

void ParentDialogueSystem::drawChoicePage(const ParentDialogueContext &ctx, uint8_t responseSet,
                                           uint8_t selected) const {
  display_.clearDisplay(); display_.setTextSize(1); display_.setTextColor(WHITE); display_.setTextWrap(false);
  display_.setCursor(24, 1); display_.print(ctx.stage <= 1 ? F("HOW DO YOU RESPOND?") : F("YOUR RESPONSE"));
  for (uint8_t i = 0; i < 3; ++i) {
    const int y = 14 + i * 15;
    if (i == selected) { display_.fillRoundRect(2, y, 124, 12, 3, WHITE); display_.setTextColor(BLACK); }
    else { display_.drawRoundRect(2, y, 124, 12, 3, WHITE); display_.setTextColor(WHITE); }
    display_.setCursor(6, y + 2); display_.print(i == 0 ? 'A' : (i == 1 ? 'B' : 'C')); display_.print(F(" "));
    display_.print(choiceLabel(responseSet, i));
    display_.setTextColor(WHITE);
  }
  display_.setCursor(3, 59); display_.print(F("D0/D2 choose  D1 answer"));
  display_.display();
}

void ParentDialogueSystem::drawResultPage(const ParentDialogueContext &ctx, const char *reply,
                                           const ParentDialogueOutcome &o) const {
  display_.clearDisplay(); display_.setTextSize(1); display_.setTextColor(WHITE); display_.setTextWrap(false);
  display_.fillRoundRect(1, 1, 25, 17, 4, BLACK); display_.drawRoundRect(1, 1, 25, 17, 4, WHITE); drawCurrentPetTiny(4, 2);
  display_.setCursor(31, 2); display_.print(F("RESPONSE REMEMBERED"));
  display_.drawRoundRect(2, 20, 124, 27, 5, WHITE); printWrapped(reply, 7, 24, 19, 3);
  display_.setCursor(3, 50);
  if (o.trust > 0) { display_.print(F("Trust +")); display_.print(o.trust); }
  else if (o.trust < 0) { display_.print(F("Trust ")); display_.print(o.trust); }
  display_.setCursor(65, 50);
  if (o.bond > 0) { display_.print(F("Bond +")); display_.print(o.bond); }
  else if (o.bond < 0) { display_.print(F("Bond ")); display_.print(o.bond); }
  display_.setCursor(3, 59); display_.print(ctx.stage <= 1 ? F("Early bond is forming") : F("This shapes future talks"));
  display_.display();
}

uint8_t ParentDialogueSystem::wrapScrollable(const char *text, char lines[][22],
                                             uint8_t maxLines) const {
  if (!text || maxLines == 0) return 0;
  memset(lines, 0, static_cast<size_t>(maxLines) * 22U);
  uint8_t line = 0, col = 0;
  const char *p = text;
  while (*p && line < maxLines) {
    if (*p == '\n') { ++line; col = 0; ++p; continue; }
    while (*p == ' ') ++p;
    if (!*p) break;
    const char *word = p;
    uint8_t len = 0;
    while (p[len] && p[len] != ' ' && p[len] != '\n') ++len;
    if (col > 0 && col + 1 + len > 21) { ++line; col = 0; if (line >= maxLines) break; }
    if (col > 0) lines[line][col++] = ' ';
    uint8_t copied = 0;
    while (copied < len && line < maxLines) {
      if (col >= 21) { ++line; col = 0; if (line >= maxLines) break; }
      lines[line][col++] = word[copied++];
    }
    if (line < maxLines) lines[line][col] = 0;
    p += len;
  }
  return line >= maxLines ? maxLines : static_cast<uint8_t>(line + 1);
}

void ParentDialogueSystem::showScrollableText(const ParentDialogueContext &ctx,
                                               const char *title,
                                               const char *text,
                                               const char *footer) const {
  (void)ctx;
  char lines[40][22];
  const uint8_t count = wrapScrollable(text, lines, 40);
  uint8_t top = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    const bool l = digitalRead(btnL_), sel = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { if (top > 0) --top; clickSound(); delay(90); }
    if (oldR == HIGH && r == LOW) { if (top + 5 < count) ++top; clickSound(); delay(90); }
    if (oldS == HIGH && sel == LOW) { clickSound(); waitButtonRelease(); return; }
    oldL = l; oldS = sel; oldR = r;
    display_.clearDisplay(); display_.setTextSize(1); display_.setTextColor(WHITE); display_.setTextWrap(false);
    display_.setCursor(1, 0);
    for (uint8_t i = 0; title && title[i] && i < 21; ++i) display_.print(title[i]);
    display_.drawLine(0, 8, 127, 8, WHITE);
    for (uint8_t row = 0; row < 5; ++row) {
      const uint8_t idx = static_cast<uint8_t>(top + row);
      if (idx >= count) break;
      display_.setCursor(1, 11 + row * 9); display_.print(lines[idx]);
    }
    display_.drawLine(0, 55, 127, 55, WHITE);
    display_.setCursor(1, 57);
    if (count > 5) {
      display_.print(F("D0/D2 ")); display_.print(top + 1); display_.print('/');
      display_.print(max<uint8_t>(1, static_cast<uint8_t>(count - 4)));
      display_.print(F(" D1 next"));
    } else display_.print(footer ? footer : "D1 continue");
    display_.display(); lastInputAt = millis(); delay(18);
  }
}

ParentDialogueOutcome ParentDialogueSystem::runPending(const ParentDialogueContext &ctx) {
  ParentDialogueOutcome o{}; o.topic = pendingTopic_; o.resolved = false; o.dismissed = false;
  if (!hasPending()) return o;
  // Reject stale pending text from older saves or a just-completed evolution.
  if (!topicAllowedForStage(pendingTopic_, ctx.stage)) {
    pendingTopic_ = 255;
    selectTopic(ctx, false);
    o.topic = pendingTopic_;
    if (!hasPending()) return o;
  }

  char narrative[240], question[120], combined[400];
  buildPrompt(ctx, narrative, sizeof(narrative), question, sizeof(question));
  snprintf(combined, sizeof(combined), "%s\n%s", narrative, question);
  showScrollableText(ctx, ctx.stage <= 1 ? "BABY BABBLE" : "TAMA TALK", combined, "D1 response");

  const uint8_t set = responseSetForTopic(pendingTopic_);
  uint8_t selected = 0; bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    bool l = digitalRead(btnL_), sel = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { selected = selected == 0 ? 2 : selected - 1; clickSound(); delay(110); }
    if (oldR == HIGH && r == LOW) { selected = (selected + 1) % 3; clickSound(); delay(110); }
    if (oldS == HIGH && sel == LOW) { clickSound(); waitButtonRelease(); break; }
    oldL = l; oldS = sel; oldR = r; drawChoicePage(ctx, set, selected); lastInputAt = millis(); delay(18);
  }

  const uint8_t style = choiceStyle(set, selected);
  applyChoice(pendingTopic_, style, o);
  o.resolved = true;
  const bool positive = style != STYLE_DISMISS && style != STYLE_MISCHIEF;
  const char *reply = resultReply(ctx.stage, set, selected);
  showScrollableText(ctx, "RESPONSE REMEMBERED", reply, "D1 close");
  positive ? goodSound() : badSound();

  ++generationConversations_; ++lifetimeConversations_;
  const uint8_t si = clampedStageIndex(ctx.stage); if (stageConversations_[si] < 65535) ++stageConversations_[si];
  if (dailyCount_ < 255) ++dailyCount_;
  lastPromptTick_ = ctx.ageTicks; lastDialogueCode_ = static_cast<uint16_t>(pendingTopic_ * 16U + pendingVariant_ * 4U + selected);
  recentTopics_[recentHead_ % PARENT_DIALOGUE_RECENT_COUNT] = pendingTopic_;
  recentHead_ = static_cast<uint8_t>((recentHead_ + 1) % PARENT_DIALOGUE_RECENT_COUNT);
  pendingTopic_ = 255; pendingSeed_ = 0; pendingVariant_ = 0; dismissedCount_ = 0;
  scheduleNext(ctx);
  return o;
}

void ParentDialogueSystem::recordSchoolResult(bool successful) {
  changeByte(schoolConfidence_, successful ? 2 : -1);
  changeByte(resilience_, successful ? 1 : 0);
}

void ParentDialogueSystem::recordSocialResult(bool successful) {
  changeByte(socialConfidence_, successful ? 2 : -1);
  changeByte(empathy_, successful ? 1 : 0);
}

void ParentDialogueSystem::eventAdjustDevelopment(int openness, int confidence,
                                                        int empathy, int responsibility,
                                                        int resilience, int honesty,
                                                        int independence, int emotionWords,
                                                        int schoolConfidence,
                                                        int socialConfidence,
                                                        int safetyConfidence,
                                                        int attachmentSecurity) {
  changeByte(openness_, openness);
  changeByte(confidence_, confidence);
  changeByte(empathy_, empathy);
  changeByte(responsibility_, responsibility);
  changeByte(resilience_, resilience);
  changeByte(honesty_, honesty);
  changeByte(independence_, independence);
  changeByte(emotionWords_, emotionWords);
  changeByte(schoolConfidence_, schoolConfidence);
  changeByte(socialConfidence_, socialConfidence);
  changeByte(safetyConfidence_, safetyConfidence);
  changeByte(attachmentSecurity_, attachmentSecurity);
}

void ParentDialogueSystem::runJournal(const ParentDialogueContext &ctx) {
  uint8_t page = 0; const uint8_t pages = 7; bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { page = page == 0 ? pages - 1 : page - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { page = (page + 1) % pages; clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) {
      waitButtonRelease();
      if (page == 6) { promptMode_ = (promptMode_ + 1) % 3; goodSound(); scheduleNext(ctx); }
      else return;
    }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); display_.setTextSize(1); display_.setTextColor(WHITE); display_.setTextWrap(false);
    display_.setCursor(20, 1); display_.print(F("PARENT BOND JOURNAL")); display_.setCursor(108, 1); display_.print(page + 1); display_.print('/'); display_.print(pages);
    if (page == 0) {
      display_.setCursor(2, 14); display_.print(F("This pet talks ")); display_.print(generationConversations_);
      display_.setCursor(2, 25); display_.print(F("Lifetime talks ")); display_.print(lifetimeConversations_);
      display_.setCursor(2, 36); display_.print(F("Bond ")); display_.print(ctx.ownerBond); display_.print(F(" Trust ")); display_.print(ctx.ownerTrust);
      display_.setCursor(2, 47); display_.print(F("Attachment ")); display_.print(attachmentSecurity_);
      display_.setCursor(2, 58); display_.print(F("Lines >")); display_.print(estimatedLinePool());
    } else if (page == 1) {
      display_.setCursor(2, 14); display_.print(F("Open ")); display_.print(openness_); display_.setCursor(68,14); display_.print(F("Conf ")); display_.print(confidence_);
      display_.setCursor(2, 26); display_.print(F("Empathy ")); display_.print(empathy_); display_.setCursor(68,26); display_.print(F("Honest ")); display_.print(honesty_);
      display_.setCursor(2, 38); display_.print(F("Resil ")); display_.print(resilience_); display_.setCursor(68,38); display_.print(F("Resp ")); display_.print(responsibility_);
      display_.setCursor(2, 50); display_.print(F("Indep ")); display_.print(independence_); display_.setCursor(68,50); display_.print(F("Feel words ")); display_.print(emotionWords_);
    } else if (page == 2) {
      display_.setCursor(2, 14); display_.print(F("School confidence ")); display_.print(schoolConfidence_);
      display_.setCursor(2, 27); display_.print(F("Social confidence ")); display_.print(socialConfidence_);
      display_.setCursor(2, 40); display_.print(F("Safety confidence ")); display_.print(safetyConfidence_);
      display_.setCursor(2, 53); display_.print(F("Difficult talks ")); display_.print(difficultTalks_);
    } else if (page == 3) {
      display_.setCursor(2, 13); display_.print(F("Baby ")); display_.print(stageConversations_[0]); display_.setCursor(68,13); display_.print(F("Child ")); display_.print(stageConversations_[1]);
      display_.setCursor(2, 27); display_.print(F("Teen ")); display_.print(stageConversations_[2]); display_.setCursor(68,27); display_.print(F("Adult ")); display_.print(stageConversations_[3]);
      display_.setCursor(2, 41); display_.print(F("Elder ")); display_.print(stageConversations_[4]);
      display_.setCursor(2, 55); display_.print(F("Last code ")); display_.print(lastDialogueCode_);
    } else if (page == 4) {
      display_.setCursor(2, 13); display_.print(F("School talks ")); display_.print(schoolTalks_);
      display_.setCursor(2, 25); display_.print(F("Family talks ")); display_.print(familyTalks_);
      display_.setCursor(2, 37); display_.print(F("Pet talks ")); display_.print(petTalks_);
      display_.setCursor(2, 49); display_.print(F("Garden talks ")); display_.print(gardenTalks_);
      display_.setCursor(2, 59); display_.print(F("D1 close"));
    } else if (page == 5) {
      const uint16_t warmth = responseStyleCount_[STYLE_COMFORT] + responseStyleCount_[STYLE_LISTEN];
      const uint16_t guidance = responseStyleCount_[STYLE_COACH] + responseStyleCount_[STYLE_BOUNDARY];
      const uint16_t encouragement = responseStyleCount_[STYLE_ENCOURAGE] + responseStyleCount_[STYLE_AUTONOMY];
      const uint16_t harsh = responseStyleCount_[STYLE_DISMISS] + responseStyleCount_[STYLE_MISCHIEF];
      display_.setCursor(2, 13); display_.print(F("Warm/listen ")); display_.print(warmth);
      display_.setCursor(2, 25); display_.print(F("Coach/rules ")); display_.print(guidance);
      display_.setCursor(2, 37); display_.print(F("Encourage ")); display_.print(encouragement);
      display_.setCursor(2, 49); display_.print(F("Dismiss/prank ")); display_.print(harsh);
      display_.setCursor(2, 59); display_.print(F("Patterns shape new babies"));
    } else {
      display_.setCursor(2, 16); display_.print(F("Random popups:"));
      display_.setCursor(2, 30); display_.print(promptMode_ == 0 ? F("QUIET") : (promptMode_ == 1 ? F("NORMAL") : F("CHATTY")));
      display_.setCursor(2, 44); display_.print(F("D1 changes frequency"));
      display_.setCursor(2, 57); display_.print(F("D0/D2 pages"));
    }
    display_.display(); lastInputAt = millis(); delay(20);
  }
}

void ParentDialogueSystem::developerMax() {
  openness_ = confidence_ = empathy_ = responsibility_ = resilience_ = honesty_ = 100;
  independence_ = emotionWords_ = schoolConfidence_ = socialConfidence_ = 100;
  safetyConfidence_ = attachmentSecurity_ = 100; promptMode_ = 2;
}

void ParentDialogueSystem::developerReset() { reset(); }
