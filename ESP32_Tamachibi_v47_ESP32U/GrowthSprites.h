#pragma once

#include <stdint.h>

constexpr uint8_t TAMACHIBI_FAMILY_COUNT = 10;
constexpr uint8_t TAMACHIBI_CARE_TIER_COUNT = 5;

// Builds one original 32x32 egg, baby, child, or teen mask.
// route 0 is the thriving teen; route 1 is the care-challenged teen.
void buildGrowthRows(uint8_t family, uint8_t stage, uint8_t route,
                     uint32_t rows[32]);

// Adult branches are ordered from highest to lowest care quality.
uint8_t growthAdultSpecies(uint8_t family, uint8_t careTier);
uint8_t growthFamilyForAdult(uint8_t adultSpecies);
uint8_t growthCareTierForAdult(uint8_t adultSpecies);

// Recognizable inherited feature retained by every adult in the family.
void addGrowthFamilyTrait(uint8_t family, uint32_t rows[32]);
