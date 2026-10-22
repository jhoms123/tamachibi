#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "GameDisplay.h"

namespace V46UI {

constexpr uint8_t HEADER_H = 9;
constexpr uint8_t FOOTER_Y = 55;
constexpr uint8_t CONTENT_Y = 10;
constexpr uint8_t CONTENT_H = 44;

void drawHeader(GameDisplay &display,
                const __FlashStringHelper *title,
                uint8_t hour,
                uint8_t minute,
                bool showAlert,
                uint8_t worldIcon);

void drawFooter(GameDisplay &display,
                const __FlashStringHelper *left,
                const __FlashStringHelper *center,
                const __FlashStringHelper *right);

void drawCarouselFrame(GameDisplay &display,
                       uint8_t selected,
                       uint8_t total,
                       bool locked,
                       uint8_t anim);

void drawCutawayRoom(GameDisplay &display,
                     uint8_t room,
                     uint8_t theme,
                     uint8_t hour,
                     uint8_t weather,
                     uint8_t anim,
                     bool lightsOn,
                     bool familyLayout = false);

void drawPetRoomBackdrop(GameDisplay &display,
                         uint8_t theme,
                         uint8_t hour,
                         uint8_t weather,
                         uint8_t anim,
                         bool lightsOn);

void clearCharacterZone(GameDisplay &display,
                        int16_t x,
                        int16_t y,
                        int16_t w,
                        int16_t h,
                        bool outline = true);

void drawNotificationToast(GameDisplay &display,
                           const __FlashStringHelper *text,
                           uint8_t icon,
                           uint8_t anim);

void drawTimeline(GameDisplay &display,
                  uint8_t wakeHour,
                  uint8_t startHour,
                  uint8_t endHour,
                  uint8_t bedHour,
                  uint8_t nowHour,
                  bool activePeriod);

void playCheckerTransition(GameDisplay &display,
                           bool reducedMotion,
                           uint8_t direction);

}  // namespace V46UI
