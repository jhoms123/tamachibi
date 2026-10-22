#include "CityWorld.h"
#include "PolishedScenes.h"
#include <stddef.h>
#include <string.h>

extern byte happy, energy, health, ownerBond, ownerTrust, skillKind, skillStyle;
extern byte stage, friendCount, partnerId, partnerBond, familyReady;
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
extern byte visualHour();

namespace {
constexpr uint8_t STAGE_ADULT_CITY = 4;
constexpr uint8_t ROUTINE_TALK_CITY = 8;
constexpr uint8_t ROUTINE_PLAY_CITY = 16;

const uint8_t RESIDENT_SPECIES[CITY_RESIDENT_COUNT] = {
  2, 8, 19, 29, 42, 43, 48, 38
};

void printRight(Adafruit_SSD1306 &display, int right, int y, const char *text) {
  const int width = static_cast<int>(strlen(text)) * 6;
  display.setCursor(max(0, right - width + 1), y);
  display.print(text);
}
}

CityWorldSystem::CityWorldSystem(size_t storageBase, Adafruit_SSD1306 &display,
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
  return save.magic == CITY_SAVE_MAGIC && save.version == CITY_SAVE_VERSION &&
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
  display_.clearDisplay();
  display_.setTextWrap(false);
  display_.setTextSize(1);
  display_.setTextColor(SSD1306_WHITE);
  display_.setCursor(2, 0);
  display_.print(residentName(id));
  display_.setCursor(83, 0);
  display_.print(F("ROOM"));
  drawRoomFurniture(residentRoomStyle(id));
  drawCurrentPetTiny(39, 31 + ((frame / 6) & 1));
  drawResidentTiny(id, 73, 31 + ((frame / 7) & 1), (frame / 8) & 1);
  display_.drawLine(62, 34, 66, 34, SSD1306_WHITE);
  display_.drawPixel(64, 32, SSD1306_WHITE);
  if (showFooter) {
    char relationText[10];
    snprintf(relationText, sizeof(relationText), "REL %u", relation_[id]);
    display_.fillRect(0, 55, 128, 9, SSD1306_BLACK);
    display_.setCursor(1, 56);
    display_.print(F("VISITING"));
    printRight(display_, 126, 56, relationText);
  }
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

void CityWorldSystem::residentInteractionMenu() {
  if (!lifeResidentAvailable(selectedResident_, visualHour())) {
    badSound();
    popup(F("Resident is away"), lifeResidentLocation(selectedResident_, visualHour()));
    return;
  }
  constexpr uint8_t ACTION_COUNT = 4;
  uint8_t pick = 0;
  uint8_t frame = 0;
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
      if (pick == 0) {
        changeRelation(selectedResident_, 3);
        addStat(happy, 1); addSkill(skillKind, 1); changeBond(1);
        markRoutineAction(ROUTINE_TALK_CITY);
        goodSound(); popup(F("Nice conversation"), residentName(selectedResident_));
        lifeRecordNeighborInteraction(selectedResident_, 0, relation_[selectedResident_]);
      } else if (pick == 1) {
        if (energy == 0) {
          badSound(); popup(F("Too tired to play"), F("Rest first"));
        } else {
          subStat(energy, 1); addStat(happy, 2);
          changeRelation(selectedResident_, 4); addSkill(skillKind, 1);
          markRoutineAction(ROUTINE_PLAY_CITY);
          goodSound(); popup(F("Played together"), residentName(selectedResident_));
          lifeRecordNeighborInteraction(selectedResident_, 1, relation_[selectedResident_]);
        }
      } else if (pick == 2) {
        if (!spendCoins(3)) {
          badSound(); popup(F("Need 3 coins"), F("Work or sell items"));
        } else {
          changeRelation(selectedResident_, 7); addSkill(skillStyle, 1);
          changeTrust(1); winSound(); popup(F("Gift delivered"), residentName(selectedResident_));
          lifeRecordNeighborInteraction(selectedResident_, 2, relation_[selectedResident_]);
        }
      } else {
        if (stage < STAGE_ADULT_CITY) {
          badSound(); popup(F("Adult stage needed"), F("Visit as friends now"));
        } else if (relation_[selectedResident_] < 60) {
          badSound(); popup(F("Build relation first"), F("Reach at least 60"));
        } else if (partnerId != 255 && partnerId != selectedResident_) {
          badSound(); popup(F("Partner already set"), F("Use Family menu"));
        } else {
          partnerId = selectedResident_;
          partnerBond = max(partnerBond, relation_[selectedResident_]);
          if (partnerBond >= 80) {
            familyReady = 1;
            winSound(); popup(F("Married and moved in"), residentName(selectedResident_));
          } else {
            familyReady = 0;
            goodSound(); popup(F("Now dating"), F("Reach bond 80"));
          }
          changeRelation(selectedResident_, 3);
          lifeRecordNeighborInteraction(selectedResident_, 3, relation_[selectedResident_]);
        }
      }
      clampAll();
      markSaveDirty();
      return;
    }

    oldL = l; oldS = s; oldR = r;
    drawResidentRoom(selectedResident_, frame++, false);
    display_.fillRoundRect(16, 51, 96, 13, 4, SSD1306_BLACK);
    display_.drawRoundRect(16, 51, 96, 13, 4, SSD1306_WHITE);
    display_.setCursor(31, 54);
    if (pick == 0) display_.print(F("CHAT"));
    else if (pick == 1) display_.print(F("PLAY TOGETHER"));
    else if (pick == 2) display_.print(F("GIVE GIFT $3"));
    else if (relation_[selectedResident_] >= 80) display_.print(F("MARRY / MOVE IN"));
    else display_.print(F("DATE / PROPOSE"));
    display_.display();
    delay(70);
  }
}

void CityWorldSystem::visitResidents() {
  bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  uint8_t frame = 0;
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

    drawResidentRoom(selectedResident_, frame++, true);
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
