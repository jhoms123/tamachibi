#pragma once

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

constexpr uint8_t TAMACHIBI_CARE_MEMORY_COUNT = 8;

// References to the already-saved pet and care data. This is a temporary view,
// so adding narrative memory does not change or migrate the EEPROM layout.
struct TamachibiMemoryContext {
  const char *ownerNickname;
  const char *lastTripName;
  const char *favoriteGameName;
  uint8_t careTypes[TAMACHIBI_CARE_MEMORY_COUNT];
  uint16_t careDays[TAMACHIBI_CARE_MEMORY_COUNT];
  uint8_t careHead;
  uint8_t careCount;
  uint8_t caredDayStreak;
  uint8_t missedCareDays;
  uint8_t rescuedFromSickness;
  uint8_t ownerBond;
  uint8_t ownerTrust;
  uint8_t lastFood;
  uint8_t learnedFavoriteFood;
  uint8_t learnedDislikedFood;
  uint8_t favoriteGivenFood;
  uint8_t favoriteGamePlays;
  uint8_t lastTripEvent;
  uint8_t gardenCount;
  uint8_t companionType;
  uint8_t companionBond;
  uint8_t fishCount;
};

inline const char *tamachibiFoodMemoryName(uint8_t food) {
  switch (food) {
    case 0: return "a basic meal";
    case 1: return "a cooked meal";
    case 2: return "a light snack";
    case 3: return "a healthy meal";
    case 4: return "a treat";
    default: return "a meal";
  }
}

inline void buildTamachibiCareMemory(const TamachibiMemoryContext &m,
                                     uint32_t turn, char *out, size_t outSize) {
  if (!out || outSize == 0) return;
  out[0] = '\0';
  if (m.careCount == 0) {
    snprintf(out, outSize, "We are still making our first care memories, one little visit at a time.");
    return;
  }
  const uint8_t count = min<uint8_t>(m.careCount, TAMACHIBI_CARE_MEMORY_COUNT);
  const uint8_t offset = static_cast<uint8_t>(turn % count);
  const uint8_t index = static_cast<uint8_t>(
      (m.careHead + TAMACHIBI_CARE_MEMORY_COUNT - 1U - offset) % TAMACHIBI_CARE_MEMORY_COUNT);
  const uint8_t type = m.careTypes[index];
  const unsigned int day = m.careDays[index];
  switch (type) {
    case 1: snprintf(out, outSize, "I remember calling for food on day %u.", day); break;
    case 2: snprintf(out, outSize, "I remember needing help to feel clean on day %u.", day); break;
    case 3: snprintf(out, outSize, "I remember feeling sick on day %u.", day); break;
    case 4: snprintf(out, outSize, "I remember wanting company on day %u.", day); break;
    case 5: snprintf(out, outSize, "I remember my birthday call on day %u.", day); break;
    case 6:
      if (m.ownerNickname && m.ownerNickname[0])
        snprintf(out, outSize, "You answered my call on day %u, my %s. I still remember.", day, m.ownerNickname);
      else snprintf(out, outSize, "You answered my call on day %u. I still remember.", day);
      break;
    case 7: snprintf(out, outSize, "Day %u was a hard care moment. It is one part of our story, not all of it.", day); break;
    case 8: snprintf(out, outSize, "I remember wandering away on day %u. Home mattered a lot when I came back.", day); break;
    case 9: snprintf(out, outSize, "I found my way home on day %u. It felt good to return.", day); break;
    case 10: snprintf(out, outSize, "You fed me on day %u. I remember that little act of care.", day); break;
    case 11: snprintf(out, outSize, "You gave me medicine on day %u, and I felt looked after.", day); break;
    case 12: snprintf(out, outSize, "You cleaned me up on day %u. Being comfortable matters.", day); break;
    case 13: snprintf(out, outSize, "You made time for affection on day %u. I kept that close.", day); break;
    case 14: snprintf(out, outSize, "We played together on day %u. I remember the fun part.", day); break;
    case 15: snprintf(out, outSize, "We went exploring together on day %u. I still picture it.", day); break;
    default: snprintf(out, outSize, "I remember one of the little moments when you cared for me on day %u.", day); break;
  }
}

inline void buildTamachibiBabyMemory(const TamachibiMemoryContext &m,
                                     uint32_t turn, char *out, size_t outSize) {
  if (!out || outSize == 0) return;
  out[0] = '\0';
  if (m.careCount == 0) {
    snprintf(out, outSize, "I know your voice, and I feel safe when you come close.");
    return;
  }
  const uint8_t count = min<uint8_t>(m.careCount, TAMACHIBI_CARE_MEMORY_COUNT);
  const uint8_t offset = static_cast<uint8_t>(turn % count);
  const uint8_t index = static_cast<uint8_t>(
      (m.careHead + TAMACHIBI_CARE_MEMORY_COUNT - 1U - offset) % TAMACHIBI_CARE_MEMORY_COUNT);
  const uint8_t type = m.careTypes[index];
  const unsigned int day = m.careDays[index];
  switch (type) {
    case 6: snprintf(out, outSize, "You came when I called on day %u. I know you come back.", day); break;
    case 10: snprintf(out, outSize, "You fed me on day %u. I remember being held close.", day); break;
    case 11: snprintf(out, outSize, "You helped me feel better on day %u. Your care feels safe.", day); break;
    case 12: snprintf(out, outSize, "You made me comfy and clean on day %u. I remember.", day); break;
    case 13: snprintf(out, outSize, "You gave me a gentle cuddle on day %u. I liked that.", day); break;
    case 14: snprintf(out, outSize, "We played together on day %u. I remember laughing with you.", day); break;
    case 15: snprintf(out, outSize, "We went out together on day %u. I liked staying near you.", day); break;
    default: buildTamachibiCareMemory(m, turn, out, outSize); break;
  }
}

inline void buildTamachibiPersonalMemory(const TamachibiMemoryContext &m,
                                         uint8_t memoryKind, uint32_t turn,
                                         char *out, size_t outSize) {
  if (!out || outSize == 0) return;
  out[0] = '\0';
  switch (memoryKind % 8U) {
    case 0:
      buildTamachibiCareMemory(m, turn, out, outSize);
      break;
    case 1:
      if (m.learnedFavoriteFood < 5 && m.learnedDislikedFood < 5 &&
          m.learnedDislikedFood != m.learnedFavoriteFood)
        snprintf(out, outSize, "You learned I love %s and that I get tired of %s; you remember both.",
                 tamachibiFoodMemoryName(m.learnedFavoriteFood),
                 tamachibiFoodMemoryName(m.learnedDislikedFood));
      else if (m.learnedFavoriteFood < 5)
        snprintf(out, outSize, "You learned that I love %s, and you remember it.", tamachibiFoodMemoryName(m.learnedFavoriteFood));
      else if (m.learnedDislikedFood < 5)
        snprintf(out, outSize, "You noticed I need a break from %s. I like that you pay attention.",
                 tamachibiFoodMemoryName(m.learnedDislikedFood));
      else if (m.lastFood < 5)
        snprintf(out, outSize, "Last time you chose %s for me. I remember that kind of care.", tamachibiFoodMemoryName(m.lastFood));
      else if (m.favoriteGivenFood < 5)
        snprintf(out, outSize, "You have remembered the meals that make me feel cared for.");
      else
        snprintf(out, outSize, "We are still learning which foods feel right for me.");
      break;
    case 2:
      if (m.favoriteGamePlays && m.favoriteGameName)
        snprintf(out, outSize, "We have played %s %u times; that little rhythm feels like ours.", m.favoriteGameName, m.favoriteGamePlays);
      else
        snprintf(out, outSize, "We are still finding the game that feels like our thing.");
      break;
    case 3:
      if (m.lastTripEvent < 9 && m.lastTripName)
        snprintf(out, outSize, "I still think about our trip to %s. That memory feels bright.", m.lastTripName);
      else
        snprintf(out, outSize, "There are still places I want us to discover together.");
      break;
    case 4:
      if (m.caredDayStreak >= 2)
        snprintf(out, outSize, "You showed up for me %u days in a row. I can feel our routine becoming ours.", m.caredDayStreak);
      else if (m.missedCareDays > 0)
        snprintf(out, outSize, "Even after a hard day, we can start our little routine again.");
      else
        snprintf(out, outSize, "Every visit gives us a chance to make a small routine of our own.");
      break;
    case 5:
      if (m.rescuedFromSickness)
        snprintf(out, outSize, "You helped me recover from sickness %u time%s. I remember being cared for.",
                 m.rescuedFromSickness, m.rescuedFromSickness == 1 ? "" : "s");
      else
        snprintf(out, outSize, "I like when you notice my little needs before they get too big.");
      break;
    case 6:
      if (m.fishCount)
        snprintf(out, outSize, "We have %u fish to watch over together. The tank is one of our shared places.", m.fishCount);
      else if (m.gardenCount)
        snprintf(out, outSize, "The garden is part of our life; I notice when we tend it together.");
      else if (m.companionType < 8)
        snprintf(out, outSize, "Our little companion feels like part of the family. Their bond with us is %u.", m.companionBond);
      else
        snprintf(out, outSize, "I think about our home and the routines that are becoming ours.");
      break;
    default:
      if (m.ownerBond >= 80 && m.ownerTrust >= 70)
        snprintf(out, outSize, "I trust you with the thoughts I keep close.");
      else if (m.ownerTrust >= 55)
        snprintf(out, outSize, "I am learning I can bring the hard things to you.");
      else if (m.ownerBond >= 45)
        snprintf(out, outSize, "I want us to keep building trust one small moment at a time.");
      else
        snprintf(out, outSize, "I am still learning what it feels like to be understood by you.");
      break;
  }
}
