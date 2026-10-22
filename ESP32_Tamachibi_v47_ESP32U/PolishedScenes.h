#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "GameDisplay.h"

constexpr uint8_t POLISHED_CITY_SCENE_COUNT = 6;
constexpr uint8_t POLISHED_HOUSE_SCENE_COUNT = 5;
constexpr uint8_t POLISHED_SCENE_W = 128;
constexpr uint8_t POLISHED_SCENE_H = 44;
constexpr uint8_t POLISHED_SCENE_Y = 9;

void drawPolishedCityScene(GameDisplay &display, uint8_t place);
void drawPolishedHouseScene(GameDisplay &display, uint8_t room);
void drawPolishedInteriorShell(GameDisplay &display, uint8_t style);
