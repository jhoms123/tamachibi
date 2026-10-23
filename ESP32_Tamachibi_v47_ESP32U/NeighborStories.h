#pragma once

#include <stdint.h>

namespace NeighborStories {

constexpr uint8_t RESIDENT_COUNT = 8;

enum Action : uint8_t {
  CHAT = 0,
  PLAY = 1,
  GIFT = 2,
  DATE = 3,
  OUTING = 4
};

enum Gift : uint8_t { FLOWER = 0, SNACK = 1, CHARM = 2, GIFT_COUNT = 3 };
enum Topic : uint8_t { FUNNY_STORY = 0, FAVORITE_HOBBY = 1, TODAY = 2, TOPIC_COUNT = 3 };
enum Outing : uint8_t { CAFE = 0, CITY_PARK = 1, NEIGHBORHOOD_WALK = 2, OUTING_COUNT = 3 };

struct ResidentProfile {
  uint8_t favoriteAction;
  uint8_t favoriteGift;
  uint8_t favoriteOuting;
  uint8_t favoriteTemperament;
  uint8_t favoriteTopic;
};

// These preferences give the eight neighbors distinct habits while keeping
// their identities stable across generations and save files.
static const ResidentProfile RESIDENTS[RESIDENT_COUNT] = {
  {CHAT,   FLOWER, CAFE,              0, FAVORITE_HOBBY},
  {OUTING, CHARM,  CITY_PARK,         2, FUNNY_STORY},
  {PLAY,   SNACK,  CITY_PARK,         1, TODAY},
  {CHAT,   FLOWER, NEIGHBORHOOD_WALK, 5, TODAY},
  {OUTING, CHARM,  NEIGHBORHOOD_WALK, 0, FAVORITE_HOBBY},
  {GIFT,   FLOWER, CAFE,              3, FUNNY_STORY},
  {PLAY,   SNACK,  CITY_PARK,         1, FAVORITE_HOBBY},
  {GIFT,   CHARM,  CAFE,              6, TODAY}
};

inline const ResidentProfile &profile(uint8_t resident) {
  return RESIDENTS[resident % RESIDENT_COUNT];
}

inline uint8_t actionFromRecord(uint8_t interaction) {
  if (interaction >= 6 && interaction <= 8) return GIFT;
  if (interaction >= 9 && interaction <= 11) return CHAT;
  if (interaction >= 12 && interaction <= 14) return OUTING;
  return interaction <= OUTING ? interaction : DATE;
}

inline uint8_t giftFromRecord(uint8_t interaction) {
  return interaction >= 6 && interaction <= 8
      ? static_cast<uint8_t>(interaction - 6)
      : 255;
}

inline uint8_t chatTopicFromRecord(uint8_t interaction) {
  return interaction >= 9 && interaction <= 11
      ? static_cast<uint8_t>(interaction - 9)
      : 255;
}

inline uint8_t outingFromRecord(uint8_t interaction) {
  return interaction >= 12 && interaction <= 14
      ? static_cast<uint8_t>(interaction - 12)
      : 255;
}

inline uint8_t chatRecord(uint8_t topic) {
  return static_cast<uint8_t>(9 + topic % TOPIC_COUNT);
}

inline uint8_t outingRecord(uint8_t outing) {
  return static_cast<uint8_t>(12 + outing % OUTING_COUNT);
}

inline bool favoriteAction(uint8_t resident, uint8_t action) {
  return profile(resident).favoriteAction == action;
}

inline bool favoriteGift(uint8_t resident, uint8_t gift) {
  return profile(resident).favoriteGift == gift;
}

inline bool favoriteOuting(uint8_t resident, uint8_t outing) {
  return profile(resident).favoriteOuting == outing;
}

inline bool likesRecordedInteraction(uint8_t resident, uint8_t interaction) {
  const uint8_t action = actionFromRecord(interaction);
  if (action == GIFT) return favoriteGift(resident, giftFromRecord(interaction));
  if (action == CHAT && interaction >= 9 && interaction <= 11) {
    return favoriteAction(resident, CHAT) ||
        profile(resident).favoriteTopic == chatTopicFromRecord(interaction);
  }
  if (action == OUTING && interaction >= 12 && interaction <= 14) {
    return favoriteAction(resident, OUTING) ||
        favoriteOuting(resident, outingFromRecord(interaction));
  }
  return favoriteAction(resident, action);
}

inline uint8_t memoryCode(uint8_t action, uint8_t choice = 0) {
  if (action == CHAT) return 1;
  if (action == PLAY) return 2;
  if (action == GIFT) return static_cast<uint8_t>(3 + choice % GIFT_COUNT);
  if (action == OUTING) return 6;
  return 7;
}

inline uint8_t actionForMemory(uint8_t memory) {
  if (memory == 1) return CHAT;
  if (memory == 2) return PLAY;
  if (memory >= 3 && memory <= 5) return GIFT;
  if (memory == 6) return OUTING;
  return DATE;
}

inline uint8_t giftForMemory(uint8_t memory) {
  return memory >= 3 && memory <= 5 ? static_cast<uint8_t>(memory - 3) : 255;
}

inline uint8_t responseVariant(uint8_t resident, uint8_t action,
                               uint8_t interactions, uint8_t hour,
                               uint8_t temperament, uint8_t relation) {
  const uint16_t seed = static_cast<uint16_t>(resident) * 29U +
      static_cast<uint16_t>(action) * 17U +
      static_cast<uint16_t>(interactions) * 7U +
      static_cast<uint16_t>(hour) * 3U +
      static_cast<uint16_t>(temperament) * 11U + relation / 8U;
  return static_cast<uint8_t>(seed % 6U);
}

inline uint8_t chapterAction(uint8_t resident) {
  return profile(resident).favoriteAction;
}

inline const char *actionName(uint8_t action) {
  switch (action) {
    case CHAT: return "talking";
    case PLAY: return "playing";
    case GIFT: return "gifts";
    case OUTING: return "going out";
    default: return "spending time";
  }
}

inline const char *giftName(uint8_t gift) {
  switch (gift % GIFT_COUNT) {
    case FLOWER: return "flower";
    case SNACK: return "snack";
    default: return "charm";
  }
}

inline const char *topicName(uint8_t topic) {
  switch (topic % TOPIC_COUNT) {
    case FUNNY_STORY: return "Something funny";
    case FAVORITE_HOBBY: return "Their hobby";
    default: return "Your day";
  }
}

inline const char *outingName(uint8_t outing) {
  switch (outing % OUTING_COUNT) {
    case CAFE: return "Cafe";
    case CITY_PARK: return "City Park";
    default: return "Neighborhood Walk";
  }
}

inline uint8_t outingCityPlace(uint8_t outing) {
  switch (outing % OUTING_COUNT) {
    case CAFE: return 2;
    case CITY_PARK: return 4;
    default: return 5;
  }
}

inline uint8_t relationReward(uint8_t base, bool favorite,
                              bool temperamentMatch, bool repeated) {
  uint8_t reward = static_cast<uint8_t>(base + (favorite ? 3 : 0) +
                                        (temperamentMatch ? 1 : 0));
  if (repeated) reward = reward > 2 ? static_cast<uint8_t>(reward - 2) : 1;
  return reward;
}

inline uint8_t readMemory(const uint8_t packed[3], uint8_t resident) {
  const uint32_t bits = static_cast<uint32_t>(packed[0]) |
      (static_cast<uint32_t>(packed[1]) << 8) |
      (static_cast<uint32_t>(packed[2]) << 16);
  const uint8_t shift = static_cast<uint8_t>((resident % RESIDENT_COUNT) * 3U);
  return static_cast<uint8_t>((bits >> shift) & 0x07U);
}

inline void writeMemory(uint8_t packed[3], uint8_t resident, uint8_t memory) {
  const uint8_t shift = static_cast<uint8_t>((resident % RESIDENT_COUNT) * 3U);
  uint32_t bits = static_cast<uint32_t>(packed[0]) |
      (static_cast<uint32_t>(packed[1]) << 8) |
      (static_cast<uint32_t>(packed[2]) << 16);
  const uint32_t mask = 0x07UL << shift;
  bits = (bits & ~mask) | ((static_cast<uint32_t>(memory) & 0x07UL) << shift);
  packed[0] = static_cast<uint8_t>(bits);
  packed[1] = static_cast<uint8_t>(bits >> 8);
  packed[2] = static_cast<uint8_t>(bits >> 16);
}

// The old visit byte tops out at 100. Keep visits in the lower seven bits and
// use the spare high bit for the chapter-earned invitation.
inline uint8_t interactionCount(uint8_t savedProgress) {
  return static_cast<uint8_t>(savedProgress & 0x7fU);
}

inline uint8_t progressWithInvite(uint8_t savedProgress, uint8_t count) {
  return static_cast<uint8_t>((savedProgress & 0x80U) | (count & 0x7fU));
}

inline bool invitePending(uint8_t savedProgress) {
  return (savedProgress & 0x80U) != 0;
}

inline uint8_t progressWithPendingInvite(uint8_t savedProgress, bool pending) {
  return static_cast<uint8_t>((savedProgress & 0x7fU) | (pending ? 0x80U : 0U));
}

} // namespace NeighborStories
