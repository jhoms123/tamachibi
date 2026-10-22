#pragma once

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

class SaveProfileManager {
public:
  SaveProfileManager(Adafruit_SSD1306 &display, uint8_t buttonLeft,
                     uint8_t buttonSelect, uint8_t buttonRight);
  bool begin();
  void runMenu();
  void createBootBackup();
  void eraseAll();
  uint8_t activeProfile() const { return activeProfile_; }
  bool filesystemReady() const { return ready_; }

private:
  Adafruit_SSD1306 &display_;
  uint8_t btnL_, btnS_, btnR_;
  bool ready_;
  uint8_t activeProfile_;

  const char *profilePath(uint8_t profile) const;
  bool fileExists(const char *path) const;
  bool saveSnapshot(const char *path);
  bool loadSnapshot(const char *path);
  bool copyFile(const char *from, const char *to);
  const char *checksumPath(const char *path) const;
  const char *metadataPath(const char *path) const;
  bool writeSnapshotInfo(const char *path, uint32_t checksum);
  bool validateSnapshot(const char *path);
  bool readSnapshotPreview(const char *path, char *name, uint16_t &generation, uint16_t &day) const;
  void writeActiveMarker();
  void readActiveMarker();
  void popup(const __FlashStringHelper *line1, const __FlashStringHelper *line2,
             uint16_t holdMs = 900) const;
};
