#include "CityWorld.h"
#include "NeighborStories.h"
#include "NativeUI.h"
#include "PolishedScenes.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

extern byte happy, energy, health, ownerBond, ownerTrust, skillKind, skillStyle;
extern byte stage, speciesType, temperament, friendCount, partnerId, partnerBond, familyReady;
extern unsigned int coins;
extern bool spendCoins(byte amount);
extern void addStat(byte &value, byte amount);
extern void subStat(byte &value, byte amount);
extern void addSkill(byte &value, byte amount);
extern void changeBond(int amount);
extern void changeTrust(int amount);
extern void markRoutineAction(byte routineBit);
extern void markSaveDirty();
extern void clampAll();
extern void waitButtonRelease();
extern bool rightHoldExitCheck();
extern void clickSound();
extern void goodSound();
extern void badSound();
extern void winSound();
extern void drawSprite16(byte id, int x, int y);
extern void drawCurrentPetTiny(int x, int y);
extern bool lifeResidentAvailable(byte resident, byte hour);
extern const __FlashStringHelper* lifeResidentLocation(byte resident, byte hour);
extern void lifeRecordNeighborInteraction(byte resident, byte interaction, byte relation);
extern byte lifeResidentStoryChapter(byte resident);
extern byte lifeResidentInteractionCount(byte resident);
extern bool lifeResidentInvitePending(byte resident);
extern void lifeSetResidentInvitePending(byte resident, bool pending);
extern byte visualHour();

namespace {
constexpr uint8_t STAGE_ADULT_CITY = 4;
constexpr uint8_t ROUTINE_TALK_CITY = 8;
constexpr uint8_t ROUTINE_PLAY_CITY = 16;

const uint8_t RESIDENT_SPECIES[CITY_RESIDENT_COUNT] = {
  2, 8, 19, 29, 42, 43, 48, 38
};

const char *residentVoice(uint8_t id, uint8_t variant) {
  static const char *const VOICES[CITY_RESIDENT_COUNT][6] = {
    {"I saved you a story.", "Look what I made.", "I kept our promise.",
     "I thought of you today.", "I saved the coziest seat.", "I have a tiny surprise."},
    {"The clouds made a wish.", "I found a star shape.", "Let's watch the sky.",
     "The moon looked extra close.", "I named a new constellation.", "The evening felt magical."},
    {"I have zoomies!", "Race you to the gate.", "I saved a game.",
     "I nearly beat my record.", "I found a shortcut.", "My feet are ready to go."},
    {"The garden feels calm.", "I noticed new leaves.", "I like quiet walks.",
     "A bird visited this morning.", "The breeze smelled like rain.", "I found a sunny patch."},
    {"I found a small clue.", "The map has a secret.", "Let's explore that.",
     "I marked a new trail.", "Something glimmered by the path.", "I think there is more to find."},
    {"I made up a tune.", "I drew us together.", "I have a new idea.",
     "The chorus is almost ready.", "I saved you the blue crayon.", "I want to show you a sketch."},
    {"Ready for a race?", "I practiced my jump.", "Let's take the long way.",
     "I learned a new trick.", "Bet you can't catch me!", "I saved my fastest lap."},
    {"I solved a puzzle.", "Can you spot the pattern?", "I have a bright plan.",
     "I tested a clever shortcut.", "Two clues fit together now.", "I wrote down a new idea."}
  };
  return VOICES[id % CITY_RESIDENT_COUNT][variant % 6];
}

uint8_t replyIndex(uint8_t action) {
  switch (action) {
    case NeighborStories::CHAT: return 0;
    case NeighborStories::PLAY: return 1;
    case NeighborStories::GIFT: return 2;
    case NeighborStories::OUTING: return 3;
    default: return 4;
  }
}

const char *actionReply(uint8_t action, uint8_t variant, bool favorite,
                        bool repeated) {
  if (repeated) {
    static const char *const AGAIN[6] = {
      "Our little ritual again!", "I always like this with you.", "Let's make a new memory.",
      "We know this one by heart.", "Same cozy feeling, new day.", "Let's add a new twist next time."
    };
    return AGAIN[variant % 6];
  }
  if (favorite) {
    static const char *const FAVORITE[5][6] = {
      {"You picked my kind of chat.", "I could talk all day.", "You know what I love.",
       "I was hoping we'd talk.", "That is my favorite subject.", "You always listen so well."},
      {"This game is our thing!", "You make play feel easy.", "One more round together?",
       "I saved a new challenge.", "We make a great team.", "You nearly got my record!"},
      {"You remembered my favorite!", "This gift feels like me.", "I will keep it close.",
       "You know me so well.", "This made my whole day.", "I have the perfect spot for it."},
      {"My favorite place with you!", "I hoped we'd come here.", "This feels just right.",
       "I could stay here awhile.", "Let's make this our place.", "I always feel happy here."},
      {"I have been hoping you'd ask.", "I'm glad we're together.", "Let's make this official.",
       "This feels like a new beginning.", "I want to choose you too.", "I am ready to share our days."}
    };
    return FAVORITE[replyIndex(action)][variant % 6];
  }
  static const char *const LINES[5][6] = {
    {"Tell me about your day.", "I have a little story.", "What should we talk about?",
     "I was saving this thought.", "Want to hear something funny?", "Let's catch up slowly."},
    {"Ready when you are!", "Let's try a quick game.", "Show me your best move!",
     "I made up a new round.", "Let's see how we do today.", "Your turn to pick the game."},
    {"A surprise for me?", "You are thoughtful.", "I like little surprises.",
     "I wonder what it could be.", "You did not have to, but thanks.", "Let's see what you brought."},
    {"A change of scene sounds nice.", "Let's take the scenic way.", "We can explore together.",
     "Which path should we take?", "I could use some fresh air.", "Let's find a new favorite spot."},
    {"I like spending time with you.", "Let's see where this goes.", "I feel close to you.",
     "You make today feel special.", "I would like to see you again.", "I am glad we found each other."}
  };
  return LINES[replyIndex(action)][variant % 6];
}

const char *memoryCallback(uint8_t memory, uint8_t currentMemory,
                           uint8_t variant = 0) {
  if (memory == 0) {
    static const char *const NEW[3] = {
      "A new memory starts.", "I will remember today.", "This is our first one."
    };
    return NEW[variant % 3];
  }
  if (memory == currentMemory) {
    static const char *const SAME[6] = {
      "We love doing this.", "I know this one by heart.", "This still feels special.",
      "Our little tradition lives on.", "I like coming back to this.", "Same joy, another day."
    };
    return SAME[variant % 6];
  }
  switch (memory) {
    case 1: return variant & 1 ? "I remember that chat." : "We talked for ages.";
    case 2: return variant & 1 ? "I still think of our game." : "We laughed while we played.";
    case 3: return variant & 1 ? "I kept your flower safe." : "Your flower brightened my room.";
    case 4: return variant & 1 ? "I saved your little snack." : "We shared a tasty treat.";
    case 5: return variant & 1 ? "Your charm is on my shelf." : "I keep your charm close.";
    case 6: return variant & 1 ? "I remember our outing." : "That walk stayed with me.";
    default: return variant & 1 ? "I remember our first date." : "That day changed everything.";
  }
}

const char *chapterMoment(uint8_t chapter) {
  switch (chapter) {
    case 1: return "You made me feel welcome.";
    case 2: return "We have our own thing now.";
    default: return "You're one of my people!";
  }
}

uint8_t actionFromPick(uint8_t pick) {
  if (pick == 0) return NeighborStories::CHAT;
  if (pick == 1) return NeighborStories::PLAY;
  if (pick == 2) return NeighborStories::GIFT;
  if (pick == 3) return NeighborStories::OUTING;
  return NeighborStories::DATE;
}

}

CityWorldSystem::CityWorldSystem(size_t storageBase, GameDisplay &display,
                                 uint8_t buttonLeft, uint8_t buttonSelect,
                                 uint8_t buttonRight)
    : storeA_(storageBase), storeB_(storageBase + sizeof(CityWorldSave)),
      display_(display), btnL_(buttonLeft), btnS_(buttonSelect), btnR_(buttonRight) {
  reset();
}

void CityWorldSystem::reset() {
  sequence_ = 0;
  memset(relation_, 0, sizeof(relation_));
  metMask_ = 0;
  selectedResident_ = 0;
  selectedHouseRoom_ = 0;
  selectedCityPlace_ = 0;
  memset(neighborMemory_, 0, sizeof(neighborMemory_));
}

uint32_t CityWorldSystem::checksum(const CityWorldSave &save) const {
  const uint8_t *raw = reinterpret_cast<const uint8_t *>(&save);
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < offsetof(CityWorldSave, checksum); ++i) {
    hash ^= raw[i];
    hash *= 16777619UL;
  }
  return hash;
}

bool CityWorldSystem::valid(const CityWorldSave &save) const {
  return save.magic == CITY_SAVE_MAGIC &&
         (save.version == 2 || save.version == CITY_SAVE_VERSION) &&
         save.checksum == checksum(save);
}

void CityWorldSystem::fillSave(CityWorldSave &save) const {
  memset(&save, 0, sizeof(save));
  save.magic = CITY_SAVE_MAGIC;
  save.version = CITY_SAVE_VERSION;
  save.sequence = sequence_ + 1UL;
  memcpy(save.relation, relation_, sizeof(relation_));
  save.metMask = metMask_;
  save.selectedResident = selectedResident_;
  save.selectedHouseRoom = selectedHouseRoom_;
  save.selectedCityPlace = selectedCityPlace_;
  memcpy(save.neighborMemory, neighborMemory_, sizeof(neighborMemory_));
  save.checksum = checksum(save);
}

void CityWorldSystem::applySave(const CityWorldSave &save) {
  sequence_ = save.sequence;
  memcpy(relation_, save.relation, sizeof(relation_));
  for (uint8_t i = 0; i < CITY_RESIDENT_COUNT; ++i) {
    relation_[i] = min(static_cast<uint8_t>(100), relation_[i]);
  }
  metMask_ = save.metMask & static_cast<uint16_t>((1UL << CITY_RESIDENT_COUNT) - 1UL);
  selectedResident_ = save.selectedResident % CITY_RESIDENT_COUNT;
  selectedHouseRoom_ = save.selectedHouseRoom % HOUSE_ROOM_COUNT;
  selectedCityPlace_ = save.selectedCityPlace % CITY_PLACE_COUNT;
  if (save.version >= 3) memcpy(neighborMemory_, save.neighborMemory, sizeof(neighborMemory_));
  else memset(neighborMemory_, 0, sizeof(neighborMemory_));
}

bool CityWorldSystem::load() {
  const CityWorldSave a = storeA_.read();
  const CityWorldSave b = storeB_.read();
  const bool validA = valid(a);
  const bool validB = valid(b);
  if (!validA && !validB) {
    reset();
    return false;
  }
  applySave(validA && validB ? (a.sequence >= b.sequence ? a : b)
                              : (validA ? a : b));
  return true;
}

void CityWorldSystem::saveNow() {
  CityWorldSave save;
  fillSave(save);
  if (save.sequence & 1UL) storeA_.write(save);
  else storeB_.write(save);
  sequence_ = save.sequence;
}

void CityWorldSystem::previousHouseRoom() {
  selectedHouseRoom_ = selectedHouseRoom_ == 0 ? HOUSE_ROOM_COUNT - 1 : selectedHouseRoom_ - 1;
  markSaveDirty();
}

void CityWorldSystem::nextHouseRoom() {
  selectedHouseRoom_ = static_cast<uint8_t>((selectedHouseRoom_ + 1) % HOUSE_ROOM_COUNT);
  markSaveDirty();
}

void CityWorldSystem::previousCityPlace() {
  selectedCityPlace_ = selectedCityPlace_ == 0 ? CITY_PLACE_COUNT - 1 : selectedCityPlace_ - 1;
  markSaveDirty();
}

void CityWorldSystem::nextCityPlace() {
  selectedCityPlace_ = static_cast<uint8_t>((selectedCityPlace_ + 1) % CITY_PLACE_COUNT);
  markSaveDirty();
}

const __FlashStringHelper *CityWorldSystem::houseRoomName(uint8_t id) const {
  switch (id % HOUSE_ROOM_COUNT) {
    case 0: return F("Pet Room");
    case 1: return F("Aquarium");
    case 2: return F("Plant Room");
    case 3: return F("Garden");
    default: return F("Kitchen");
  }
}

const __FlashStringHelper *CityWorldSystem::cityPlaceName(uint8_t id) const {
  switch (id % CITY_PLACE_COUNT) {
    case 0: return F("Work District");
    case 1: return F("Market");
    case 2: return F("Cafe");
    case 3: return F("School");
    case 4: return F("City Park");
    default: return F("Neighbors");
  }
}

const __FlashStringHelper *CityWorldSystem::residentName(uint8_t id) const {
  switch (id % CITY_RESIDENT_COUNT) {
    case 0: return F("MOCHI");
    case 1: return F("LUMI");
    case 2: return F("PIKO");
    case 3: return F("NORI");
    case 4: return F("KUMO");
    case 5: return F("BIBI");
    case 6: return F("TOTO");
    default: return F("MIMI");
  }
}

uint8_t CityWorldSystem::residentSpecies(uint8_t id) const {
  return RESIDENT_SPECIES[id % CITY_RESIDENT_COUNT];
}

uint8_t CityWorldSystem::residentRoomStyle(uint8_t id) const {
  return id % CITY_RESIDENT_COUNT;
}

uint8_t CityWorldSystem::relation(uint8_t id) const {
  return relation_[id % CITY_RESIDENT_COUNT];
}

uint8_t CityWorldSystem::friendCountMet() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < CITY_RESIDENT_COUNT; ++i) {
    if (metMask_ & (1U << i)) ++count;
  }
  return count;
}

uint8_t CityWorldSystem::lastNeighborMemory(uint8_t id) const {
  return NeighborStories::readMemory(neighborMemory_, id);
}

void CityWorldSystem::rememberNeighbor(uint8_t id, uint8_t memory) {
  NeighborStories::writeMemory(neighborMemory_, id, memory);
  markSaveDirty();
}

void CityWorldSystem::drawResidentTiny(uint8_t id, int x, int y, bool wave) const {
  drawSprite16(residentSpecies(id), x, y);
  if (wave) {
    display_.drawLine(x + 14, y + 8, x + 18, y + 4, SSD1306_WHITE);
    display_.drawPixel(x + 18, y + 3, SSD1306_WHITE);
  }
}

bool CityWorldSystem::partnerLivesAtHome(uint8_t id, uint8_t bond) const {
  return id < CITY_RESIDENT_COUNT && bond >= 80;
}

void CityWorldSystem::drawPartnerAtHome(uint8_t id, int x, int y, bool wave) const {
  if (id >= CITY_RESIDENT_COUNT) return;
  drawResidentTiny(id, x, y, wave);
  // A tiny ring/heart marker distinguishes the resident spouse from a visitor.
  display_.drawPixel(x + 6, y - 2, SSD1306_WHITE);
  display_.drawPixel(x + 9, y - 2, SSD1306_WHITE);
  display_.drawPixel(x + 5, y - 1, SSD1306_WHITE);
  display_.drawPixel(x + 7, y - 1, SSD1306_WHITE);
  display_.drawPixel(x + 8, y - 1, SSD1306_WHITE);
  display_.drawPixel(x + 10, y - 1, SSD1306_WHITE);
  display_.drawPixel(x + 8, y, SSD1306_WHITE);
}

void CityWorldSystem::drawRoomFurniture(uint8_t style) const {
  drawPolishedInteriorShell(display_, style);

  // Every resident room now follows the same clean architectural language,
  // while the side furniture still reflects that resident's personality.
  switch (style % CITY_RESIDENT_COUNT) {
    case 0: { // cozy lounge
      display_.fillRoundRect(8, 39, 31, 10, 3, SSD1306_WHITE);
      display_.drawRoundRect(11, 34, 25, 8, 2, SSD1306_WHITE);
      display_.fillRect(13, 36, 9, 4, SSD1306_BLACK);
      display_.fillRect(25, 36, 8, 4, SSD1306_BLACK);
      display_.drawLine(14, 49, 13, 54, SSD1306_WHITE);
      display_.drawLine(34, 49, 35, 54, SSD1306_WHITE);
      display_.drawLine(111, 51, 111, 31, SSD1306_WHITE);
      display_.fillTriangle(104, 32, 118, 32, 111, 23, SSD1306_WHITE);
      display_.drawLine(107, 32, 115, 32, SSD1306_BLACK);
      break;
    }
    case 1: { // mushroom nook
      display_.fillRect(15, 39, 7, 12, SSD1306_WHITE);
      display_.fillCircle(15, 35, 8, SSD1306_WHITE);
      display_.fillCircle(23, 35, 8, SSD1306_WHITE);
      display_.fillRect(15, 29, 9, 13, SSD1306_WHITE);
      display_.drawLine(9, 35, 31, 35, SSD1306_BLACK);
      display_.drawPixel(14, 32, SSD1306_BLACK);
      display_.drawPixel(25, 31, SSD1306_BLACK);
      display_.drawLine(101, 32, 119, 32, SSD1306_WHITE);
      display_.drawLine(104, 38, 116, 38, SSD1306_WHITE);
      display_.drawCircle(107, 28, 3, SSD1306_WHITE);
      display_.drawCircle(115, 25, 2, SSD1306_WHITE);
      break;
    }
    case 2: { // pond / frog room
      display_.drawRoundRect(7, 43, 35, 8, 4, SSD1306_WHITE);
      display_.drawLine(11, 47, 38, 47, SSD1306_WHITE);
      display_.drawCircle(21, 42, 3, SSD1306_WHITE);
      display_.drawPixel(20, 41, SSD1306_BLACK);
      display_.fillRoundRect(104, 46, 15, 6, 2, SSD1306_WHITE);
      display_.drawLine(111, 46, 111, 28, SSD1306_WHITE);
      display_.drawLine(111, 33, 104, 27, SSD1306_WHITE);
      display_.drawLine(111, 36, 118, 29, SSD1306_WHITE);
      break;
    }
    case 3: { // forest den
      display_.fillRect(14, 38, 5, 15, SSD1306_WHITE);
      display_.fillCircle(16, 30, 9, SSD1306_WHITE);
      display_.drawPixel(12, 28, SSD1306_BLACK);
      display_.drawPixel(20, 33, SSD1306_BLACK);
      display_.fillRect(108, 41, 5, 12, SSD1306_WHITE);
      display_.fillCircle(110, 34, 7, SSD1306_WHITE);
      display_.drawRoundRect(7, 50, 31, 5, 2, SSD1306_WHITE);
      break;
    }
    case 4: { // magic observatory
      display_.drawCircle(19, 34, 8, SSD1306_WHITE);
      display_.drawCircle(19, 34, 5, SSD1306_WHITE);
      display_.drawPixel(17, 31, SSD1306_WHITE);
      display_.drawPixel(22, 29, SSD1306_WHITE);
      display_.drawLine(12, 43, 26, 43, SSD1306_WHITE);
      display_.drawLine(15, 43, 13, 52, SSD1306_WHITE);
      display_.drawLine(23, 43, 25, 52, SSD1306_WHITE);
      display_.drawLine(105, 48, 117, 36, SSD1306_WHITE);
      display_.drawLine(109, 39, 116, 39, SSD1306_WHITE);
      display_.drawPixel(112, 27, SSD1306_WHITE);
      display_.drawPixel(118, 22, SSD1306_WHITE);
      break;
    }
    case 5: { // kitchen
      display_.fillRect(7, 38, 36, 15, SSD1306_WHITE);
      display_.fillRect(10, 41, 30, 9, SSD1306_BLACK);
      display_.drawCircle(18, 44, 3, SSD1306_WHITE);
      display_.drawCircle(32, 44, 3, SSD1306_WHITE);
      display_.drawLine(12, 50, 38, 50, SSD1306_WHITE);
      display_.fillRect(103, 31, 16, 22, SSD1306_WHITE);
      display_.fillRect(106, 34, 10, 8, SSD1306_BLACK);
      display_.drawLine(104, 44, 118, 44, SSD1306_BLACK);
      break;
    }
    case 6: { // star bedroom
      display_.drawCircle(108, 28, 8, SSD1306_WHITE);
      display_.fillCircle(111, 26, 7, SSD1306_BLACK);
      for (uint8_t i = 0; i < 5; ++i) {
        const int px = 10 + i * 24;
        display_.drawPixel(px, 25 + (i & 1) * 5, SSD1306_WHITE);
        display_.drawLine(px - 1, 26 + (i & 1) * 5, px + 1, 26 + (i & 1) * 5, SSD1306_WHITE);
      }
      display_.fillRoundRect(7, 44, 34, 9, 3, SSD1306_WHITE);
      display_.fillRect(11, 46, 26, 5, SSD1306_BLACK);
      break;
    }
    default: { // workshop
      display_.fillRect(7, 40, 37, 13, SSD1306_WHITE);
      display_.fillRect(10, 43, 31, 7, SSD1306_BLACK);
      display_.drawCircle(20, 34, 6, SSD1306_WHITE);
      display_.drawCircle(20, 34, 2, SSD1306_WHITE);
      display_.drawLine(20, 28, 20, 40, SSD1306_WHITE);
      display_.fillRect(103, 31, 17, 22, SSD1306_WHITE);
      display_.fillRect(106, 34, 11, 5, SSD1306_BLACK);
      display_.fillRect(106, 42, 11, 8, SSD1306_BLACK);
      break;
    }
  }
}

void CityWorldSystem::drawResidentRoom(uint8_t id, uint8_t frame, bool showFooter) const {
  id %= CITY_RESIDENT_COUNT;
  NativeUI::Scope native(display_);
  display_.clearDisplay();
  char context[16];
  snprintf(context, sizeof(context), "STORY %u/3",
           static_cast<unsigned>(lifeResidentStoryChapter(id)));
  NativeUI::header(display_, residentName(id), context);
  NativeUI::room(display_, 0, residentRoomStyle(id), visualHour(), frame);
  drawCurrentPetTiny(48, 64 + ((frame & 1U) ? 1 : 0));
  NativeUI::pet(display_, 72, 64 + ((frame & 1U) ? 1 : 0),
                residentSpecies(id), 4, frame);
  const uint8_t last = lastNeighborMemory(id);
  const uint8_t nextMemory = NeighborStories::memoryCode(NeighborStories::CHAT);
  NativeUI::text(display_, memoryCallback(last, nextMemory), 4, 91, 120);
  NativeUI::text(display_, "FRIENDSHIP", 5, 104, 60);
  NativeUI::meter(display_, 71, 105, 52, relation_[id], 100);
  if (showFooter) NativeUI::footer(display_, "PREV", "VISIT", "NEXT");
}

void CityWorldSystem::changeRelation(uint8_t id, int amount) {
  id %= CITY_RESIDENT_COUNT;
  const bool wasNew = (metMask_ & (1U << id)) == 0;
  int value = static_cast<int>(relation_[id]) + amount;
  relation_[id] = static_cast<uint8_t>(constrain(value, 0, 100));
  metMask_ |= static_cast<uint16_t>(1U << id);
  if (wasNew && friendCount < 250) ++friendCount;

  // Relationship progress with the chosen partner also advances the family
  // bond. At 80, the partner is considered married/moved in and appears in
  // the player's home room.
  if (partnerId == id) {
    partnerBond = max(partnerBond, relation_[id]);
    if (partnerBond >= 80) familyReady = 1;
  }
  markSaveDirty();
}

void CityWorldSystem::popup(const __FlashStringHelper *line1,
                            const __FlashStringHelper *line2,
                            uint16_t holdMs) {
  display_.clearDisplay();
  display_.setTextWrap(false);
  display_.setTextSize(1);
  display_.setTextColor(SSD1306_WHITE);
  display_.drawRoundRect(3, 13, 122, 39, 5, SSD1306_WHITE);
  display_.setCursor(10, 23); display_.print(line1);
  display_.setCursor(10, 36); display_.print(line2);
  display_.display();
  delay(holdMs);
}

void CityWorldSystem::drawNeighborChoice(uint8_t id, uint8_t action,
                                             uint8_t choice, uint8_t frame,
                                             bool invited) const {
  id %= CITY_RESIDENT_COUNT;
  NativeUI::Scope native(display_);
  display_.clearDisplay();
  if (action == NeighborStories::OUTING) {
    const uint8_t outing = invited ? NeighborStories::profile(id).favoriteOuting : choice;
    NativeUI::header(display_, residentName(id), invited ? "INVITATION" : "OUTING");
    NativeUI::town(display_, NeighborStories::outingCityPlace(outing), frame);
    const int offset = frame % 5;
    NativeUI::pet(display_, 25 + offset, 55, speciesType, stage, frame);
    NativeUI::pet(display_, 82 - offset, 55, residentSpecies(id), 4, frame);
  } else {
    NativeUI::header(display_, residentName(id), "NEIGHBOR");
    NativeUI::room(display_, 0, residentRoomStyle(id), visualHour(), frame);
    NativeUI::pet(display_, 31, 59, speciesType, stage, frame);
    NativeUI::pet(display_, 82, 59, residentSpecies(id), 4, frame);
  }

  const char *label = "";
  const char *detail = "";
  if (action == NeighborStories::CHAT) {
    label = "TALK ABOUT...";
    detail = NeighborStories::topicName(choice);
  } else if (action == NeighborStories::PLAY) {
    label = "PLAY TOGETHER";
    detail = "Follow the light";
  } else if (action == NeighborStories::GIFT) {
    label = "A LITTLE SURPRISE";
    detail = NeighborStories::giftName(choice);
  } else if (action == NeighborStories::OUTING) {
    label = invited ? "THEY SAVED YOU A PLACE" : "GO SOMEWHERE TOGETHER";
    detail = NeighborStories::outingName(invited
        ? NeighborStories::profile(id).favoriteOuting : choice);
  } else {
    label = partnerId == id ? "A DATE TOGETHER" : "ASK TO DATE";
    detail = relation_[id] >= 80 ? "A home together is ready"
                                 : "Grow your friendship first";
  }

  NativeUI::text(display_, label, 4, 91, 120);
  NativeUI::text(display_, detail, 4, 102, 120);
  if (action == NeighborStories::GIFT && relation_[id] >= 25) {
    char favorite[22];
    snprintf(favorite, sizeof(favorite), "LIKES: %s",
             NeighborStories::giftName(NeighborStories::profile(id).favoriteGift));
    NativeUI::text(display_, favorite, 4, 80, 120);
  }
  if (action != NeighborStories::OUTING || invited)
    NativeUI::footer(display_, "PREV", "CHOOSE", "NEXT");
  else
    NativeUI::footer(display_, "PREV", "GO", "NEXT");
  display_.display();
}

void CityWorldSystem::showNeighborMoment(uint8_t id, uint8_t action,
                                         uint8_t choice, uint8_t previousMemory,
                                         uint8_t visits, uint8_t chapterBefore,
                                         uint8_t chapterAfter, bool favorite,
                                         bool repeated) {
  id %= CITY_RESIDENT_COUNT;
  const uint8_t memory = NeighborStories::memoryCode(action, choice);
  const uint8_t variant = static_cast<uint8_t>((
      NeighborStories::responseVariant(
          id, action, visits, visualHour(), temperament, relation_[id]) +
      choice * 2U) % 6U);
  const char *firstLine = chapterAfter > chapterBefore
      ? chapterMoment(chapterAfter)
      : actionReply(action, variant, favorite, repeated);
  const char *secondLine = residentVoice(
      id, static_cast<uint8_t>((variant + action + visits) % 6));
  const char *thirdLine = lifeResidentInvitePending(id)
      ? "They saved you an invite."
      : memoryCallback(previousMemory, memory, variant);

  NativeUI::Scope native(display_);
  for (uint8_t frame = 0; frame < 6; ++frame) {
    display_.clearDisplay();
    NativeUI::header(display_, residentName(id), "MOMENT");
    NativeUI::room(display_, 0, residentRoomStyle(id), visualHour(), frame);
    const int petBob = (frame & 1U) ? 1 : 0;
    NativeUI::pet(display_, 28, 61 + petBob, speciesType, stage, frame);
    NativeUI::pet(display_, 82, 61 - petBob, residentSpecies(id), 4, frame);

    if (action == NeighborStories::CHAT) {
      display_.drawRoundRect(57, 30, 18, 11, 3, SSD1306_WHITE);
      display_.drawPixel(61, 42, SSD1306_WHITE);
      display_.drawPixel(60, 43, SSD1306_WHITE);
    } else if (action == NeighborStories::PLAY) {
      const int starX = 54 + static_cast<int>((frame * 9U) % 28U);
      display_.drawLine(starX, 34, starX, 42, SSD1306_WHITE);
      display_.drawLine(starX - 4, 38, starX + 4, 38, SSD1306_WHITE);
    } else if (action == NeighborStories::GIFT) {
      display_.drawRect(60, 34, 13, 11, SSD1306_WHITE);
      display_.drawLine(66, 34, 66, 45, SSD1306_WHITE);
      display_.drawLine(60, 38, 73, 38, SSD1306_WHITE);
      display_.drawCircle(63, 32, 2, SSD1306_WHITE);
      display_.drawCircle(69, 32, 2, SSD1306_WHITE);
    } else if (action == NeighborStories::OUTING) {
      display_.drawPixel(58 + frame * 2, 40, SSD1306_WHITE);
      display_.drawPixel(70 + frame * 2, 44, SSD1306_WHITE);
    } else {
      display_.drawPixel(62, 34, SSD1306_WHITE);
      display_.drawPixel(67, 34, SSD1306_WHITE);
      display_.drawPixel(60, 36, SSD1306_WHITE);
      display_.drawPixel(64, 36, SSD1306_WHITE);
      display_.drawPixel(69, 36, SSD1306_WHITE);
      display_.drawPixel(64, 39, SSD1306_WHITE);
    }

    NativeUI::text(display_, firstLine, 4, 89, 120);
    NativeUI::text(display_, secondLine, 4, 101, 120);
    NativeUI::text(display_, thirdLine, 4, 112, 120);
    display_.display();
    delay(70);
  }
}

void CityWorldSystem::chooseNeighborChat() {
  uint8_t topic = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) {
      topic = topic == 0 ? NeighborStories::TOPIC_COUNT - 1 : topic - 1;
      clickSound(); delay(90);
    }
    if (oldR == HIGH && r == LOW) {
      topic = static_cast<uint8_t>((topic + 1) % NeighborStories::TOPIC_COUNT);
      clickSound(); delay(90);
    }
    if (oldS == HIGH && s == LOW) {
      waitButtonRelease();
      break;
    }
    oldL = l; oldS = s; oldR = r;
    drawNeighborChoice(selectedResident_, NeighborStories::CHAT, topic,
                       static_cast<uint8_t>(millis() / 350UL));
    delay(35);
  }

  const uint8_t id = selectedResident_;
  const uint8_t visits = lifeResidentInteractionCount(id);
  const uint8_t chapterBefore = lifeResidentStoryChapter(id);
  const uint8_t previous = lastNeighborMemory(id);
  const uint8_t memory = NeighborStories::memoryCode(NeighborStories::CHAT);
  const bool favorite = topic == NeighborStories::profile(id).favoriteTopic ||
      NeighborStories::favoriteAction(id, NeighborStories::CHAT);
  const bool temperamentMatch =
      temperament == NeighborStories::profile(id).favoriteTemperament;
  const bool repeated = previous == memory;

  changeRelation(id, NeighborStories::relationReward(2, favorite, temperamentMatch, repeated));
  addStat(happy, favorite ? 2 : 1);
  addSkill(skillKind, 1);
  if (!repeated) changeBond(1);
  markRoutineAction(ROUTINE_TALK_CITY);
  lifeRecordNeighborInteraction(id, NeighborStories::chatRecord(topic), relation_[id]);
  rememberNeighbor(id, memory);
  clampAll();
  showNeighborMoment(id, NeighborStories::CHAT, topic, previous, visits,
                     chapterBefore, lifeResidentStoryChapter(id), favorite, repeated);
}

void CityWorldSystem::chooseNeighborGift() {
  uint8_t gift = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) {
      gift = gift == 0 ? NeighborStories::GIFT_COUNT - 1 : gift - 1;
      clickSound(); delay(90);
    }
    if (oldR == HIGH && r == LOW) {
      gift = static_cast<uint8_t>((gift + 1) % NeighborStories::GIFT_COUNT);
      clickSound(); delay(90);
    }
    if (oldS == HIGH && s == LOW) {
      waitButtonRelease();
      break;
    }
    oldL = l; oldS = s; oldR = r;
    drawNeighborChoice(selectedResident_, NeighborStories::GIFT, gift,
                       static_cast<uint8_t>(millis() / 350UL));
    delay(35);
  }

  if (!spendCoins(3)) {
    badSound(); popup(F("Need 3 coins"), F("For a neighbor gift"));
    return;
  }
  const uint8_t id = selectedResident_;
  const uint8_t visits = lifeResidentInteractionCount(id);
  const uint8_t chapterBefore = lifeResidentStoryChapter(id);
  const uint8_t previous = lastNeighborMemory(id);
  const uint8_t memory = NeighborStories::memoryCode(NeighborStories::GIFT, gift);
  const bool favorite = NeighborStories::favoriteGift(id, gift);
  const bool temperamentMatch =
      temperament == NeighborStories::profile(id).favoriteTemperament;
  const bool repeated = previous == memory;

  changeRelation(id, NeighborStories::relationReward(
      favorite ? 4 : 2, favorite, temperamentMatch, repeated));
  addStat(happy, favorite ? 2 : 1);
  addSkill(skillStyle, 1);
  if (favorite) changeTrust(1);
  if (!repeated) changeBond(1);
  markSaveDirty();
  lifeRecordNeighborInteraction(id, static_cast<uint8_t>(6 + gift), relation_[id]);
  rememberNeighbor(id, memory);
  clampAll();
  goodSound();
  showNeighborMoment(id, NeighborStories::GIFT, gift, previous, visits,
                     chapterBefore, lifeResidentStoryChapter(id), favorite, repeated);
}

void CityWorldSystem::runNeighborPlay() {
  if (energy == 0) {
    badSound(); popup(F("Too tired to play"), F("Rest, then visit"));
    return;
  }
  const uint8_t id = selectedResident_;
  const uint8_t visits = lifeResidentInteractionCount(id);
  const uint8_t chapterBefore = lifeResidentStoryChapter(id);
  const uint8_t previous = lastNeighborMemory(id);
  const uint8_t memory = NeighborStories::memoryCode(NeighborStories::PLAY);
  uint8_t score = 0;
  NativeUI::Scope native(display_);
  waitButtonRelease();

  for (uint8_t round = 0; round < 3; ++round) {
    const uint8_t target = static_cast<uint8_t>(random(0, 3));
    uint8_t answer = 255;
    const unsigned long started = millis();
    while (millis() - started < 1700UL) {
      if (rightHoldExitCheck()) return;
      display_.clearDisplay();
      NativeUI::header(display_, residentName(id), "FOLLOW THE LIGHT");
      NativeUI::room(display_, 0, residentRoomStyle(id), visualHour(), round);
      NativeUI::pet(display_, 35, 60, speciesType, stage, round);
      NativeUI::pet(display_, 82, 60, residentSpecies(id), 4, round);
      const int x = target == 0 ? 27 : target == 1 ? 63 : 99;
      display_.drawCircle(x, 39, 7, SSD1306_WHITE);
      display_.drawPixel(x, 39, SSD1306_WHITE);
      display_.setCursor(8, 91);
      display_.print(target == 0 ? F("Tap LEFT")
                    : target == 1 ? F("Tap SELECT") : F("Tap RIGHT"));
      NativeUI::footer(display_, "LEFT", "SELECT", "RIGHT");
      display_.display();

      if (digitalRead(btnL_) == LOW) { answer = 0; break; }
      if (digitalRead(btnS_) == LOW) { answer = 1; break; }
      if (digitalRead(btnR_) == LOW) { answer = 2; break; }
      delay(12);
    }
    waitButtonRelease();
    if (answer == target) ++score;
  }

  subStat(energy, 1);
  const bool favorite = NeighborStories::favoriteAction(id, NeighborStories::PLAY);
  const bool temperamentMatch =
      temperament == NeighborStories::profile(id).favoriteTemperament;
  const bool repeated = previous == memory;
  changeRelation(id, NeighborStories::relationReward(
      static_cast<uint8_t>(1 + score), favorite, temperamentMatch, repeated));
  if (score > 0) {
    addStat(happy, score >= 2 ? 2 : 1);
    addSkill(skillKind, 1);
  }
  if (score >= 2 && !repeated) changeBond(1);
  markRoutineAction(ROUTINE_PLAY_CITY);
  lifeRecordNeighborInteraction(id, NeighborStories::PLAY, relation_[id]);
  rememberNeighbor(id, memory);
  clampAll();
  goodSound();
  showNeighborMoment(id, NeighborStories::PLAY, score, previous, visits,
                     chapterBefore, lifeResidentStoryChapter(id), favorite, repeated);
}

void CityWorldSystem::chooseNeighborOuting(bool invited) {
  if (invited) {
    runNeighborOuting(NeighborStories::profile(selectedResident_).favoriteOuting, true);
    return;
  }
  uint8_t outing = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) {
      outing = outing == 0 ? NeighborStories::OUTING_COUNT - 1 : outing - 1;
      clickSound(); delay(90);
    }
    if (oldR == HIGH && r == LOW) {
      outing = static_cast<uint8_t>((outing + 1) % NeighborStories::OUTING_COUNT);
      clickSound(); delay(90);
    }
    if (oldS == HIGH && s == LOW) {
      waitButtonRelease();
      break;
    }
    oldL = l; oldS = s; oldR = r;
    drawNeighborChoice(selectedResident_, NeighborStories::OUTING, outing,
                       static_cast<uint8_t>(millis() / 350UL), false);
    delay(35);
  }
  runNeighborOuting(outing, false);
}

void CityWorldSystem::runNeighborOuting(uint8_t outing, bool invited) {
  if (energy < 2) {
    badSound(); popup(F("Save some energy"), F("Your invite can wait"));
    return;
  }
  const uint8_t id = selectedResident_;
  const uint8_t visits = lifeResidentInteractionCount(id);
  const uint8_t chapterBefore = lifeResidentStoryChapter(id);
  const uint8_t previous = lastNeighborMemory(id);
  const uint8_t memory = NeighborStories::memoryCode(NeighborStories::OUTING);
  const bool favorite = invited || NeighborStories::favoriteOuting(id, outing) ||
      NeighborStories::favoriteAction(id, NeighborStories::OUTING);
  const bool temperamentMatch =
      temperament == NeighborStories::profile(id).favoriteTemperament;
  const bool repeated = previous == memory;

  subStat(energy, 2);
  addStat(happy, 2);
  addSkill(skillKind, 1);
  changeRelation(id, NeighborStories::relationReward(
      3, favorite, temperamentMatch, repeated));
  if (favorite) changeTrust(1);
  if (!repeated) changeBond(1);

  NativeUI::Scope native(display_);
  for (uint8_t frame = 0; frame < 7; ++frame) {
    display_.clearDisplay();
    NativeUI::header(display_, residentName(id), NeighborStories::outingName(outing));
    NativeUI::town(display_, NeighborStories::outingCityPlace(outing), frame);
    const int step = frame < 4 ? frame * 4 : (7 - frame) * 4;
    NativeUI::pet(display_, 23 + step, 56, speciesType, stage, frame);
    NativeUI::pet(display_, 82 - step, 56, residentSpecies(id), 4, frame);
    if (frame & 1U) {
      display_.drawPixel(62, 34, SSD1306_WHITE);
      display_.drawPixel(67, 40, SSD1306_WHITE);
    } else {
      display_.drawPixel(67, 34, SSD1306_WHITE);
      display_.drawPixel(62, 40, SSD1306_WHITE);
    }
    NativeUI::text(display_, invited ? "A promised day out" : "Out with a friend",
                   4, 99, 120);
    NativeUI::footer(display_, "", "TOGETHER", "");
    display_.display();
    delay(85);
  }

  markRoutineAction(ROUTINE_PLAY_CITY);
  lifeRecordNeighborInteraction(id, NeighborStories::outingRecord(outing), relation_[id]);
  rememberNeighbor(id, memory);
  if (invited) lifeSetResidentInvitePending(id, false);
  clampAll();
  goodSound();
  showNeighborMoment(id, NeighborStories::OUTING, outing, previous, visits,
                     chapterBefore, lifeResidentStoryChapter(id), favorite, repeated);
}

void CityWorldSystem::runNeighborDate() {
  const uint8_t id = selectedResident_;
  if (stage < STAGE_ADULT_CITY) {
    badSound(); popup(F("Grow up first"), F("Dating starts as an adult"));
    return;
  }
  if (relation_[id] < 60) {
    badSound(); popup(F("Build your friendship"), F("Reach bond 60 first"));
    return;
  }
  if (partnerId != 255 && partnerId != id) {
    badSound(); popup(F("Partner already set"), F("Use Family menu"));
    return;
  }

  const uint8_t visits = lifeResidentInteractionCount(id);
  const uint8_t chapterBefore = lifeResidentStoryChapter(id);
  const uint8_t previous = lastNeighborMemory(id);
  const uint8_t memory = NeighborStories::memoryCode(NeighborStories::DATE);
  const bool repeated = previous == memory;
  const bool favorite = NeighborStories::favoriteAction(id, NeighborStories::DATE);
  const bool temperamentMatch =
      temperament == NeighborStories::profile(id).favoriteTemperament;

  if (partnerId == 255) {
    partnerId = id;
    partnerBond = relation_[id];
    familyReady = 0;
  }
  changeRelation(id, NeighborStories::relationReward(
      3, favorite, temperamentMatch, repeated));
  partnerBond = max(partnerBond, relation_[id]);
  if (partnerBond >= 80) familyReady = 1;
  if (!repeated) changeBond(1);
  changeTrust(1);
  markSaveDirty();
  lifeRecordNeighborInteraction(id, NeighborStories::DATE, relation_[id]);
  rememberNeighbor(id, memory);
  clampAll();
  goodSound();
  showNeighborMoment(id, NeighborStories::DATE, 0, previous, visits,
                     chapterBefore, lifeResidentStoryChapter(id), favorite, repeated);
}

void CityWorldSystem::residentInteractionMenu() {
  if (!lifeResidentAvailable(selectedResident_, visualHour())) {
    badSound();
    popup(F("Resident is away"), lifeResidentLocation(selectedResident_, visualHour()));
    return;
  }
  constexpr uint8_t ACTION_COUNT = 5;
  uint8_t pick = 0;
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();

  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_);
    const bool s = digitalRead(btnS_);
    const bool r = digitalRead(btnR_);

    if (oldL == HIGH && l == LOW) {
      pick = pick == 0 ? ACTION_COUNT - 1 : pick - 1;
      clickSound(); delay(100);
    }
    if (oldR == HIGH && r == LOW) {
      pick = static_cast<uint8_t>((pick + 1) % ACTION_COUNT);
      clickSound(); delay(100);
    }
    if (oldS == HIGH && s == LOW) {
      waitButtonRelease();
      if (pick == 0) chooseNeighborChat();
      else if (pick == 1) runNeighborPlay();
      else if (pick == 2) chooseNeighborGift();
      else if (pick == 3) chooseNeighborOuting(lifeResidentInvitePending(selectedResident_));
      else runNeighborDate();
      return;
    }
    oldL = l; oldS = s; oldR = r;

    const uint8_t action = actionFromPick(pick);
    drawNeighborChoice(selectedResident_, action, 0,
                       static_cast<uint8_t>(millis() / 350UL),
                       action == NeighborStories::OUTING &&
                           lifeResidentInvitePending(selectedResident_));
    delay(30);
  }
}

void CityWorldSystem::visitResidents() {
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();

  while (true) {
    if (rightHoldExitCheck()) return;
    const bool l = digitalRead(btnL_);
    const bool s = digitalRead(btnS_);
    const bool r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) {
      selectedResident_ = selectedResident_ == 0 ? CITY_RESIDENT_COUNT - 1 : selectedResident_ - 1;
      clickSound(); delay(100);
    }
    if (oldR == HIGH && r == LOW) {
      selectedResident_ = static_cast<uint8_t>((selectedResident_ + 1) % CITY_RESIDENT_COUNT);
      clickSound(); delay(100);
    }
    if (oldS == HIGH && s == LOW) {
      residentInteractionMenu();
      oldL = oldS = oldR = HIGH;
      waitButtonRelease();
    }
    oldL = l; oldS = s; oldR = r;

    drawResidentRoom(selectedResident_, static_cast<uint8_t>(millis() / 350UL), true);
    if (!lifeResidentAvailable(selectedResident_, visualHour())) {
      display_.fillRoundRect(22, 18, 84, 23, 5, SSD1306_BLACK);
      display_.drawRoundRect(22, 18, 84, 23, 5, SSD1306_WHITE);
      display_.setCursor(33, 22); display_.print(F("OUT AT"));
      display_.setCursor(33, 32); display_.print(lifeResidentLocation(selectedResident_, visualHour()));
    }
    display_.display();
    delay(90);
  }
}

void CityWorldSystem::developerMaxRelations() {
  metMask_ = static_cast<uint16_t>((1UL << CITY_RESIDENT_COUNT) - 1UL);
  for (uint8_t i = 0; i < CITY_RESIDENT_COUNT; ++i) relation_[i] = 100;
  friendCount = max(friendCount, static_cast<uint8_t>(CITY_RESIDENT_COUNT));
  saveNow();
}

void CityWorldSystem::developerResetRelations() {
  memset(relation_, 0, sizeof(relation_));
  metMask_ = 0;
  selectedResident_ = 0;
  saveNow();
}
