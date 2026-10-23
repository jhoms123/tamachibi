#pragma once

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

constexpr uint8_t TAMACHIBI_CARE_MEMORY_COUNT = 8;

// This is a read-only view over the game's existing saved state. It deliberately
// adds no EEPROM fields, so existing pet saves keep their current layout.
struct TamachibiMemoryContext {
  const char *ownerNickname;
  const char *lastTripName;
  const char *favoriteGameName;
  const char *companionName;
  const char *emotionName;
  const char *careerName;
  uint8_t careTypes[TAMACHIBI_CARE_MEMORY_COUNT];
  uint16_t careDays[TAMACHIBI_CARE_MEMORY_COUNT];
  uint8_t careHours[TAMACHIBI_CARE_MEMORY_COUNT];
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
  uint8_t gardenHealth;
  uint8_t companionType;
  uint8_t companionBond;
  uint8_t companionMood;
  uint8_t careerId;
  uint8_t careerRank;
  uint8_t fishCount;
  uint8_t temperament;
  uint8_t emotion;
  uint8_t emotionIntensity;
  uint8_t familyMood;
  uint8_t childCount;
  uint8_t siblingBond;
  uint8_t familyPresent;
};

enum TamachibiMemoryKind : uint8_t {
  TAMA_MEMORY_CARE = 0,
  TAMA_MEMORY_FOOD,
  TAMA_MEMORY_GAME,
  TAMA_MEMORY_TRIP,
  TAMA_MEMORY_ROUTINE,
  TAMA_MEMORY_RECOVERY,
  TAMA_MEMORY_GARDEN,
  TAMA_MEMORY_COMPANION,
  TAMA_MEMORY_EMOTION,
  TAMA_MEMORY_CAREER,
  TAMA_MEMORY_TRUST,
  TAMA_MEMORY_PERSONALITY,
  TAMA_MEMORY_FAMILY,
  TAMA_MEMORY_AQUARIUM,
  TAMA_MEMORY_KIND_COUNT
};

inline const char *tamachibiFoodMemoryName(uint8_t food) {
  switch (food) {
    case 0: return "a basic meal";
    case 1: return "a cooked meal";
    case 2: return "a snack";
    case 3: return "a healthy meal";
    case 4: return "a treat";
    default: return "that meal";
  }
}

inline uint8_t tamachibiCareMemoryIndex(const TamachibiMemoryContext &m,
                                       uint32_t turn) {
  const uint8_t count = min<uint8_t>(m.careCount, TAMACHIBI_CARE_MEMORY_COUNT);
  if (count == 0) return 0;
  const uint8_t offset = static_cast<uint8_t>(turn % count);
  return static_cast<uint8_t>(
      (m.careHead + TAMACHIBI_CARE_MEMORY_COUNT - 1U - offset) % TAMACHIBI_CARE_MEMORY_COUNT);
}

inline const char *tamachibiDayPart(uint8_t hour) {
  if (hour < 5) return "before dawn";
  if (hour < 11) return "in the morning";
  if (hour < 17) return "in the afternoon";
  if (hour < 21) return "in the evening";
  return "late at night";
}

inline void buildTamachibiCareMemory(const TamachibiMemoryContext &m,
                                     uint32_t turn, char *out, size_t outSize) {
  if (!out || outSize == 0) return;
  out[0] = '\0';
  if (m.careCount == 0) {
    snprintf(out, outSize, "We are still making our first care memories, one little visit at a time.");
    return;
  }

  const uint8_t index = tamachibiCareMemoryIndex(m, turn);
  const unsigned int day = m.careDays[index];
  const char *when = tamachibiDayPart(m.careHours[index]);
  const uint8_t variant = static_cast<uint8_t>((turn / max<uint8_t>(1, min<uint8_t>(m.careCount, TAMACHIBI_CARE_MEMORY_COUNT))) % 3U);
  const char *lead = variant == 0 ? "I still remember" :
                     (variant == 1 ? "One little moment stayed with me:" : "I can still picture");
  char event[112];
  switch (m.careTypes[index]) {
    case 1: snprintf(event, sizeof(event), "you noticed I was hungry on day %u %s", day, when); break;
    case 2: snprintf(event, sizeof(event), "you helped me feel clean on day %u %s", day, when); break;
    case 3: snprintf(event, sizeof(event), "you stayed with me when I felt sick on day %u %s", day, when); break;
    case 4: snprintf(event, sizeof(event), "I wanted company and you came by on day %u %s", day, when); break;
    case 5: snprintf(event, sizeof(event), "my birthday call and your visit on day %u %s", day, when); break;
    case 6:
      if (m.ownerNickname && m.ownerNickname[0])
        snprintf(event, sizeof(event), "you answered my call on day %u %s, %s", day, when, m.ownerNickname);
      else snprintf(event, sizeof(event), "you answered my call on day %u %s", day, when);
      break;
    case 7: snprintf(event, sizeof(event), "we got through a difficult care moment on day %u %s", day, when); break;
    case 8: snprintf(event, sizeof(event), "I wandered off on day %u %s, and coming home mattered", day, when); break;
    case 9: snprintf(event, sizeof(event), "I found my way home on day %u %s and felt safe again", day, when); break;
    case 10: snprintf(event, sizeof(event), "you fed me on day %u %s", day, when); break;
    case 11: snprintf(event, sizeof(event), "you gave me medicine on day %u %s and helped me recover", day, when); break;
    case 12: snprintf(event, sizeof(event), "you cleaned me up on day %u %s", day, when); break;
    case 13: snprintf(event, sizeof(event), "you made time for affection on day %u %s", day, when); break;
    case 14: snprintf(event, sizeof(event), "we played together on day %u %s", day, when); break;
    case 15: snprintf(event, sizeof(event), "we went exploring together on day %u %s", day, when); break;
    case 20: snprintf(event, sizeof(event), "we shared a basic meal on day %u %s", day, when); break;
    case 21: snprintf(event, sizeof(event), "you made me a cooked meal on day %u %s", day, when); break;
    case 22: snprintf(event, sizeof(event), "you brought me a little snack on day %u %s", day, when); break;
    case 23: snprintf(event, sizeof(event), "you chose a healthy meal for me on day %u %s", day, when); break;
    case 24: snprintf(event, sizeof(event), "you gave me a special treat on day %u %s", day, when); break;
    case 30: snprintf(event, sizeof(event), "you gave me cold syrup on day %u %s", day, when); break;
    case 31: snprintf(event, sizeof(event), "you helped my tummy on day %u %s", day, when); break;
    case 32: snprintf(event, sizeof(event), "you helped my skin feel better on day %u %s", day, when); break;
    case 33: snprintf(event, sizeof(event), "you helped when I felt worn out on day %u %s", day, when); break;
    case 34: snprintf(event, sizeof(event), "you brought me comfort tea on day %u %s", day, when); break;
    case 35: snprintf(event, sizeof(event), "you gave me strong medicine on day %u %s", day, when); break;
    case 40: snprintf(event, sizeof(event), "you gave me a cuddle on day %u %s", day, when); break;
    case 41: snprintf(event, sizeof(event), "you noticed my effort and praised me on day %u %s", day, when); break;
    case 42: snprintf(event, sizeof(event), "we sat quietly together on day %u %s", day, when); break;
    default: snprintf(event, sizeof(event), "you made time for me on day %u %s", day, when); break;
  }
  snprintf(out, outSize, "%s %s.", lead, event);
}

inline void buildTamachibiBabyMemory(const TamachibiMemoryContext &m,
                                     uint32_t turn, char *out, size_t outSize) {
  if (!out || outSize == 0) return;
  out[0] = '\0';
  if (m.careCount == 0) {
    snprintf(out, outSize, "I know your voice, and I feel safe when you come close.");
    return;
  }
  const uint8_t index = tamachibiCareMemoryIndex(m, turn);
  const uint16_t day = m.careDays[index];
  switch (m.careTypes[index]) {
    case 6: snprintf(out, outSize, "You came when I called on day %u. I know you come back.", day); break;
    case 10: snprintf(out, outSize, "You fed me on day %u. I remember feeling cared for.", day); break;
    case 11: snprintf(out, outSize, "You helped me feel better on day %u. Your care feels safe.", day); break;
    case 12: snprintf(out, outSize, "You made me comfy and clean on day %u. I remember.", day); break;
    case 13: snprintf(out, outSize, "You gave me a gentle cuddle on day %u. I liked that.", day); break;
    case 14: snprintf(out, outSize, "We played together on day %u. I remember laughing with you.", day); break;
    case 15: snprintf(out, outSize, "We went out together on day %u. I liked staying near you.", day); break;
    case 20: case 21: case 22: case 23: case 24:
      snprintf(out, outSize, "You made sure I had something to eat on day %u. I remember that care.", day); break;
    case 30: case 31: case 32: case 33: case 34: case 35:
      snprintf(out, outSize, "You helped when I felt unwell on day %u. I felt safer with you there.", day); break;
    case 40: snprintf(out, outSize, "You gave me a cuddle on day %u. I felt close to you.", day); break;
    case 41: snprintf(out, outSize, "You noticed me trying on day %u. I wanted to keep learning.", day); break;
    case 42: snprintf(out, outSize, "We sat together quietly on day %u. I felt safe beside you.", day); break;
    default: buildTamachibiCareMemory(m, turn, out, outSize); break;
  }
}

inline void buildTamachibiPersonalMemory(const TamachibiMemoryContext &m,
                                         uint8_t memoryKind, uint32_t turn,
                                         char *out, size_t outSize) {
  if (!out || outSize == 0) return;
  out[0] = '\0';
  const uint8_t variant = static_cast<uint8_t>(turn % 4U);
  switch (memoryKind % TAMA_MEMORY_KIND_COUNT) {
    case TAMA_MEMORY_CARE:
      buildTamachibiCareMemory(m, turn, out, outSize);
      break;
    case TAMA_MEMORY_FOOD: {
      const uint8_t favorite = m.learnedFavoriteFood < 5 ? m.learnedFavoriteFood : m.favoriteGivenFood;
      if (favorite < 5 && m.learnedDislikedFood < 5 && m.learnedDislikedFood != favorite) {
        snprintf(out, outSize, variant & 1U
          ? "You learned that I love %s and that I need a break from %s. I like being known that well."
          : "You remember I enjoy %s, and you noticed when I got tired of %s. That feels thoughtful.",
          tamachibiFoodMemoryName(favorite), tamachibiFoodMemoryName(m.learnedDislikedFood));
      } else if (favorite < 5) {
        snprintf(out, outSize, variant & 1U
          ? "You remembered that %s is the food I look forward to."
          : "After all those meals, you figured out that I really like %s.",
          tamachibiFoodMemoryName(favorite));
      } else if (m.learnedDislikedFood < 5) {
        snprintf(out, outSize, "You noticed I needed a break from %s. I like that you paid attention.",
                 tamachibiFoodMemoryName(m.learnedDislikedFood));
      } else if (m.lastFood < 5) {
        snprintf(out, outSize, "Last time you gave me %s. I remember sharing that meal with you.",
                 tamachibiFoodMemoryName(m.lastFood));
      } else {
        snprintf(out, outSize, "We are still learning which foods feel right for me.");
      }
      break;
    }
    case TAMA_MEMORY_GAME:
      if (m.favoriteGamePlays && m.favoriteGameName) {
        if (variant & 1U)
          snprintf(out, outSize, "We have played %s %u times. I like having a game that feels like ours.",
                   m.favoriteGameName, static_cast<unsigned int>(m.favoriteGamePlays));
        else
          snprintf(out, outSize, "You keep making time for %s with me. We have played %u times, and I still look forward to it.",
                   m.favoriteGameName, static_cast<unsigned int>(m.favoriteGamePlays));
      } else {
        snprintf(out, outSize, "We are still finding the game that feels like our thing.");
      }
      break;
    case TAMA_MEMORY_TRIP:
      if (m.lastTripEvent < 9 && m.lastTripName) {
        if (variant & 1U)
          snprintf(out, outSize, "I still think about our trip to %s. I noticed the little things because we were there together.",
                   m.lastTripName);
        else
          snprintf(out, outSize, "That trip to %s stayed with me. I would like another day out with you.",
                   m.lastTripName);
      } else {
        snprintf(out, outSize, "There are still places I want us to discover together.");
      }
      break;
    case TAMA_MEMORY_ROUTINE:
      if (m.caredDayStreak >= 2) {
        snprintf(out, outSize, variant & 1U
          ? "You showed up for me %u days in a row. That little routine feels like ours."
          : "I have been able to count on you for %u days running. I feel that rhythm.",
          static_cast<unsigned int>(m.caredDayStreak));
      } else if (m.missedCareDays > 0) {
        snprintf(out, outSize, "We can start our routine again after a hard day. I know one missed moment is not our whole story.");
      } else if (m.careCount > 1) {
        snprintf(out, outSize, "Our visits are starting to feel familiar. I look forward to the little routine we are making.");
      } else {
        snprintf(out, outSize, "Every visit gives us a chance to make a small routine of our own.");
      }
      break;
    case TAMA_MEMORY_RECOVERY:
      if (m.rescuedFromSickness) {
        snprintf(out, outSize, variant & 1U
          ? "You have helped me recover from sickness %u time%s. I remember being looked after."
          : "When I was unwell, you stayed with me. We have made it through that %u time%s.",
          static_cast<unsigned int>(m.rescuedFromSickness),
          m.rescuedFromSickness == 1 ? "" : "s");
      } else if (m.careCount) {
        snprintf(out, outSize, "You noticed my needs before I could explain them. That helps me feel safe bringing you a hard day.");
      } else {
        snprintf(out, outSize, "I am learning that I can ask you for help when something feels wrong.");
      }
      break;
    case TAMA_MEMORY_GARDEN:
      if (m.gardenCount) {
        if (m.gardenHealth < 45)
          snprintf(out, outSize, "Some of our %u plants need extra care. I like that we can notice and help together.",
                   static_cast<unsigned int>(m.gardenCount));
        else if (variant & 1U)
          snprintf(out, outSize, "We have %u growing things to care for. I like watching the garden change with you.",
                   static_cast<unsigned int>(m.gardenCount));
        else
          snprintf(out, outSize, "The garden has become one of our shared places. I notice when you make time for it.");
      } else {
        snprintf(out, outSize, "I like the small living things we get to look after together.");
      }
      break;
    case TAMA_MEMORY_COMPANION:
      if (m.companionType < 8 && m.companionName) {
        if (m.companionMood <= 3 || m.companionBond < 30)
          snprintf(out, outSize, "%s has seemed a little quiet lately. I like when we both make time for them.",
                   m.companionName);
        else if (variant & 1U)
          snprintf(out, outSize, "%s has become part of our little family. I notice how you look after both of us.",
                   m.companionName);
        else
          snprintf(out, outSize, "I like having %s nearby when we spend time together. That feels like home.",
                   m.companionName);
      } else {
        snprintf(out, outSize, "I like the small friends and living things we get to look after together.");
      }
      break;
    case TAMA_MEMORY_EMOTION:
      if (m.emotionName && m.emotionIntensity >= 5)
        snprintf(out, outSize, "I feel %s pretty strongly today. I like that I can tell you instead of pretending I am fine.", m.emotionName);
      else if (m.emotionName)
        snprintf(out, outSize, "I feel %s today. It helps when you ask and give me room to answer.", m.emotionName);
      else
        snprintf(out, outSize, "My feelings change from day to day, and I am learning how to tell you about them.");
      break;
    case TAMA_MEMORY_CAREER:
      if (m.careerId < 10 && m.careerName) {
        if (variant & 1U)
          snprintf(out, outSize, "I have reached rank %u in %s. I am still deciding what I want to learn next.",
                   static_cast<unsigned int>(m.careerRank), m.careerName);
        else
          snprintf(out, outSize, "My work in %s has grown to rank %u. I like having you notice how far I have come.",
                   m.careerName, static_cast<unsigned int>(m.careerRank));
      } else {
        snprintf(out, outSize, "I am still figuring out what kind of work would make me feel useful and like myself.");
      }
      break;
    case TAMA_MEMORY_TRUST:
      if (m.ownerBond >= 80 && m.ownerTrust >= 70)
        snprintf(out, outSize, variant & 1U
          ? "I trust you with the thoughts I usually keep close. You have earned that slowly."
          : "You know the quiet details about me, and I feel safe letting you see more of who I am.");
      else if (m.ownerTrust >= 55)
        snprintf(out, outSize, "I am learning I can bring the hard things to you and still be myself.");
      else if (m.ownerBond >= 45)
        snprintf(out, outSize, "I want us to keep building trust one small, steady moment at a time.");
      else if (m.careCount)
        snprintf(out, outSize, "I am still learning what it feels like to be understood. I do notice when you come back.");
      else
        snprintf(out, outSize, "I am still learning what it feels like to be understood by you.");
      break;
    case TAMA_MEMORY_PERSONALITY: {
      static const char *const traits[8] = {
        "I take my time and notice the quiet details.",
        "I want to see what is beyond the next corner.",
        "I keep asking questions because the world is interesting.",
        "I feel best when we make time for each other.",
        "I like the playful parts of life and the occasional harmless prank.",
        "I feel connected to slow mornings and growing things.",
        "I notice when you remember what makes me happy.",
        "I like trying things on my own while knowing you are nearby."
      };
      static const char *const reflections[4] = {
        "That part of me has been here since I was small.",
        "I think you have seen that side of me grow.",
        "It is one of the ways I feel most like myself.",
        "You know that about me because you have paid attention."
      };
      snprintf(out, outSize, "%s %s", traits[m.temperament % 8U], reflections[variant]);
      break;
    }
    case TAMA_MEMORY_FAMILY:
      if (m.childCount) {
        if (m.familyMood < 40)
          snprintf(out, outSize, "The family has felt tense lately. I want us to keep making room for one another.");
        else if (variant & 1U)
          snprintf(out, outSize, "Our family has %u little ones in it, and I think about how each of us belongs.",
                   static_cast<unsigned int>(m.childCount));
        else
          snprintf(out, outSize, "I can feel the family's mood today. I want us to keep making room for one another.");
      } else if (m.familyPresent) {
        if (m.siblingBond && m.siblingBond < 35)
          snprintf(out, outSize, "I wish things felt easier with my sibling. I hope we can find a small way to reconnect.");
        else if (m.familyMood < 40)
          snprintf(out, outSize, "Home feels a little tense today. I would like us to listen to each other more.");
        else
          snprintf(out, outSize, "I like the way our family keeps finding small ways to make room for one another.");
      } else if (m.companionType < 8 && m.companionName) {
        snprintf(out, outSize, "%s feels like part of our family. I like that we care for them together.", m.companionName);
      } else {
        snprintf(out, outSize, "Home is becoming a collection of small routines and stories we share.");
      }
      break;
    case TAMA_MEMORY_AQUARIUM:
      if (m.fishCount) {
        if (variant & 1U)
          snprintf(out, outSize, "There are %u fish in our tank now. I like checking on them with you.",
                   static_cast<unsigned int>(m.fishCount));
        else
          snprintf(out, outSize, "Our aquarium has %u fish, and it has become one of our shared little worlds.",
                   static_cast<unsigned int>(m.fishCount));
      } else {
        snprintf(out, outSize, "Maybe one day we will have a little aquarium story of our own.");
      }
      break;
    default:
      buildTamachibiCareMemory(m, turn, out, outSize);
      break;
  }
}

inline uint8_t tamachibiMemoryForParentTopic(uint8_t topic) {
  if (topic <= 7) return TAMA_MEMORY_CARE;
  if ((topic >= 8 && topic <= 17) || topic == 23 || topic == 57)
    return TAMA_MEMORY_GAME;
  if (topic == 18 || topic == 47) return TAMA_MEMORY_GARDEN;
  if (topic == 19 || topic == 48) return TAMA_MEMORY_COMPANION;
  if (topic == 20 || topic == 21 || topic == 24 || topic == 25 || topic == 62 || topic == 64)
    return TAMA_MEMORY_TRUST;
  if (topic == 22 || topic == 58) return TAMA_MEMORY_GAME;
  if (topic == 26 || topic == 27 || topic == 37 || topic == 50) return TAMA_MEMORY_PERSONALITY;
  if (topic == 35 || topic == 38 || topic == 39 || topic == 40 || topic == 41 || topic == 42 ||
      topic == 43 || topic == 49 || topic == 59) return TAMA_MEMORY_FAMILY;
  if (topic == 44 || topic == 61) return TAMA_MEMORY_EMOTION;
  if (topic == 46 || topic == 60) return TAMA_MEMORY_TRIP;
  if (topic == 28 || topic == 53 || topic == 55) return TAMA_MEMORY_ROUTINE;
  if (topic == 29 || topic == 32 || topic == 33 || topic == 34)
    return TAMA_MEMORY_FAMILY;
  if (topic == 30 || topic == 45 || topic == 63 || topic == 64) return TAMA_MEMORY_TRUST;
  if (topic == 31 || topic == 36 || topic == 51 || topic == 52) return TAMA_MEMORY_RECOVERY;
  if (topic == 54) return TAMA_MEMORY_FOOD;
  if (topic == 56) return TAMA_MEMORY_CAREER;
  return TAMA_MEMORY_CARE;
}

inline uint8_t tamachibiMemoryForGrowingFeature(uint8_t feature) {
  switch (feature) {
    case 0: case 2: case 9: case 14: case 15: case 20: case 26:
      return TAMA_MEMORY_TRUST;
    case 1: case 12: case 13: case 18: case 19: case 23:
      return TAMA_MEMORY_FAMILY;
    case 3: case 4: case 11: case 17: case 24: case 25:
      return TAMA_MEMORY_ROUTINE;
    case 5: case 6: case 7: case 22:
      return TAMA_MEMORY_CARE;
    case 8: case 10: case 16:
      return TAMA_MEMORY_GAME;
    case 21:
      return TAMA_MEMORY_PERSONALITY;
    default:
      return TAMA_MEMORY_CARE;
  }
}

inline uint8_t tamachibiMemoryForDirectTalk(uint8_t talkTopic) {
  switch (talkTopic) {
    case 1: return TAMA_MEMORY_CARE;
    case 2: return TAMA_MEMORY_CARE;
    case 3: return TAMA_MEMORY_CARE;
    case 4: return TAMA_MEMORY_GAME;
    case 5: return TAMA_MEMORY_CARE;
    case 6: return TAMA_MEMORY_TRIP;
    case 7: return TAMA_MEMORY_CARE;
    case 8: return TAMA_MEMORY_ROUTINE;
    case 9: return TAMA_MEMORY_CARE;
    case 12: return TAMA_MEMORY_AQUARIUM;
    default: return TAMA_MEMORY_TRUST;
  }
}
