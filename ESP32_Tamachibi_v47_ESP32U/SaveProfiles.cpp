#include "SaveProfiles.h"
#include <EEPROM.h>
#include <LittleFS.h>
#include "Esp32Platform.h"

extern void saveGameNow();
extern bool loadGameSave();
extern void waitButtonRelease();
extern bool rightHoldExitCheck();
extern void clickSound();
extern void goodSound();
extern void badSound();
extern void forceDisplayOn();
extern void drawScreen(bool doDisplay);
extern unsigned int generation;
extern unsigned int petAgeDays();
extern byte stage, speciesType;
extern char petName[7];


namespace {
struct SnapshotMeta {
  uint32_t magic;
  uint8_t version;
  uint8_t stage;
  uint8_t species;
  uint16_t generation;
  uint16_t day;
  char name[7];
  uint32_t checksum;
};
constexpr uint32_t SNAPSHOT_META_MAGIC = 0x534E5031UL;

uint32_t fnvUpdate(uint32_t value, const uint8_t *data, size_t count) {
  for (size_t i = 0; i < count; ++i) { value ^= data[i]; value *= 16777619UL; }
  return value;
}
}
SaveProfileManager::SaveProfileManager(GameDisplay &display,
                                       uint8_t buttonLeft,
                                       uint8_t buttonSelect,
                                       uint8_t buttonRight)
    : display_(display), btnL_(buttonLeft), btnS_(buttonSelect), btnR_(buttonRight),
      ready_(false), activeProfile_(0) {}

bool SaveProfileManager::begin() {
  ready_ = LittleFS.begin();
  if (ready_) readActiveMarker();
  return ready_;
}

const char *SaveProfileManager::profilePath(uint8_t profile) const {
  return profile == 0 ? "/tama_main.bin" : "/tama_test.bin";
}

const char *SaveProfileManager::checksumPath(const char *path) const {
  if (strcmp(path, "/tama_main.bin") == 0) return "/tama_main.crc";
  if (strcmp(path, "/tama_test.bin") == 0) return "/tama_test.crc";
  if (strcmp(path, "/tama_backup.bin") == 0) return "/tama_backup.crc";
  if (strcmp(path, "/tama_auto.bin") == 0) return "/tama_auto.crc";
  return "/tama_lastgood.crc";
}

const char *SaveProfileManager::metadataPath(const char *path) const {
  if (strcmp(path, "/tama_main.bin") == 0) return "/tama_main.meta";
  if (strcmp(path, "/tama_test.bin") == 0) return "/tama_test.meta";
  if (strcmp(path, "/tama_backup.bin") == 0) return "/tama_backup.meta";
  if (strcmp(path, "/tama_auto.bin") == 0) return "/tama_auto.meta";
  return "/tama_lastgood.meta";
}

bool SaveProfileManager::writeSnapshotInfo(const char *path, uint32_t checksum) {
  File cf = LittleFS.open(checksumPath(path), "w");
  if (!cf || cf.write(reinterpret_cast<const uint8_t *>(&checksum), sizeof(checksum)) != sizeof(checksum)) { if (cf) cf.close(); return false; }
  cf.close();
  SnapshotMeta meta{}; meta.magic = SNAPSHOT_META_MAGIC; meta.version = 1; meta.stage = stage; meta.species = speciesType; meta.generation = generation; meta.day = petAgeDays(); strncpy(meta.name, petName, sizeof(meta.name) - 1); meta.checksum = checksum;
  File mf = LittleFS.open(metadataPath(path), "w");
  if (!mf || mf.write(reinterpret_cast<const uint8_t *>(&meta), sizeof(meta)) != sizeof(meta)) { if (mf) mf.close(); return false; }
  mf.close(); return true;
}

bool SaveProfileManager::validateSnapshot(const char *path) {
  if (!ready_ || !LittleFS.exists(path) || !LittleFS.exists(checksumPath(path))) return false;
  File f = LittleFS.open(path, "r"); if (!f || f.size() != TAMACHIBI_EEPROM_BYTES) { if (f) f.close(); return false; }
  uint8_t block[64]; uint32_t hash = 2166136261UL;
  while (f.available()) { int got = f.read(block, sizeof(block)); if (got <= 0) break; hash = fnvUpdate(hash, block, static_cast<size_t>(got)); } f.close();
  File cf = LittleFS.open(checksumPath(path), "r"); uint32_t stored = 0; if (!cf || cf.read(reinterpret_cast<uint8_t *>(&stored), sizeof(stored)) != static_cast<int>(sizeof(stored))) { if (cf) cf.close(); return false; } cf.close();
  return hash == stored;
}

bool SaveProfileManager::readSnapshotPreview(const char *path, char *name, uint16_t &gen, uint16_t &day) const {
  name[0] = 0; gen = 0; day = 0; if (!ready_ || !LittleFS.exists(metadataPath(path))) return false;
  File mf = LittleFS.open(metadataPath(path), "r"); SnapshotMeta meta{}; if (!mf || mf.read(reinterpret_cast<uint8_t *>(&meta), sizeof(meta)) != static_cast<int>(sizeof(meta))) { if (mf) mf.close(); return false; } mf.close();
  if (meta.magic != SNAPSHOT_META_MAGIC || meta.version != 1) return false;
  strncpy(name, meta.name, 6); name[6] = 0; gen = meta.generation; day = meta.day;
  return true;
}

bool SaveProfileManager::fileExists(const char *path) const {
  return ready_ && LittleFS.exists(path);
}

bool SaveProfileManager::saveSnapshot(const char *path) {
  if (!ready_) return false;
  File f = LittleFS.open(path, "w"); if (!f) return false;
  uint8_t block[64]; uint32_t hash = 2166136261UL;
  for (size_t base = 0; base < TAMACHIBI_EEPROM_BYTES; base += sizeof(block)) {
    const size_t count = min(sizeof(block), static_cast<size_t>(TAMACHIBI_EEPROM_BYTES - base));
    for (size_t i = 0; i < count; ++i) block[i] = EEPROM.read(base + i);
    hash = fnvUpdate(hash, block, count);
    if (f.write(block, count) != count) { f.close(); return false; }
  }
  f.close(); return writeSnapshotInfo(path, hash);
}

bool SaveProfileManager::loadSnapshot(const char *path) {
  if (!validateSnapshot(path)) return false;
  File f = LittleFS.open(path, "r");
  if (!f || f.size() != TAMACHIBI_EEPROM_BYTES) { if (f) f.close(); return false; }
  uint8_t block[64];
  size_t base = 0;
  while (base < TAMACHIBI_EEPROM_BYTES) {
    const size_t want = min(sizeof(block), static_cast<size_t>(TAMACHIBI_EEPROM_BYTES - base));
    const int got = f.read(block, want);
    if (got != static_cast<int>(want)) { f.close(); return false; }
    for (size_t i = 0; i < want; ++i) EEPROM.write(base + i, block[i]);
    base += want;
  }
  f.close();
  if (!EEPROM.commit()) return false;
  const bool ok = loadGameSave();
  if (ok && strcmp(path, "/tama_lastgood.bin") != 0) saveSnapshot("/tama_lastgood.bin");
  return ok;
}

bool SaveProfileManager::copyFile(const char *from, const char *to) {
  if (!ready_ || !LittleFS.exists(from)) return false;
  File src = LittleFS.open(from, "r");
  File dst = LittleFS.open(to, "w");
  if (!src || !dst) { if (src) src.close(); if (dst) dst.close(); return false; }
  uint8_t block[64];
  while (src.available()) {
    int got = src.read(block, sizeof(block));
    if (got <= 0 || dst.write(block, got) != static_cast<size_t>(got)) { src.close(); dst.close(); return false; }
  }
  src.close(); dst.close();
  if (LittleFS.exists(checksumPath(from))) { File csrc = LittleFS.open(checksumPath(from), "r"); File cdst = LittleFS.open(checksumPath(to), "w"); uint8_t b[16]; while (csrc.available()) { int g = csrc.read(b, sizeof(b)); if (g > 0) cdst.write(b, g); } csrc.close(); cdst.close(); }
  if (LittleFS.exists(metadataPath(from))) { File msrc = LittleFS.open(metadataPath(from), "r"); File mdst = LittleFS.open(metadataPath(to), "w"); uint8_t b[32]; while (msrc.available()) { int g = msrc.read(b, sizeof(b)); if (g > 0) mdst.write(b, g); } msrc.close(); mdst.close(); }
  return validateSnapshot(to);
}

void SaveProfileManager::writeActiveMarker() {
  if (!ready_) return;
  File f = LittleFS.open("/tama_active.dat", "w");
  if (f) { f.write(&activeProfile_, 1); f.close(); }
}

void SaveProfileManager::readActiveMarker() {
  activeProfile_ = 0;
  if (!LittleFS.exists("/tama_active.dat")) return;
  File f = LittleFS.open("/tama_active.dat", "r");
  if (f) { uint8_t v = 0; if (f.read(&v, 1) == 1) activeProfile_ = v == 1 ? 1 : 0; f.close(); }
}

void SaveProfileManager::createBootBackup() {
  if (!ready_) return;
  saveSnapshot("/tama_auto.bin");
  if (!fileExists(profilePath(activeProfile_))) saveSnapshot(profilePath(activeProfile_));
}

void SaveProfileManager::eraseAll() {
  if (!ready_) return;
  LittleFS.remove("/tama_main.bin");
  LittleFS.remove("/tama_test.bin");
  LittleFS.remove("/tama_backup.bin");
  LittleFS.remove("/tama_auto.bin");
  LittleFS.remove("/tama_lastgood.bin");
  const char *bases[] = {"/tama_main.bin","/tama_test.bin","/tama_backup.bin","/tama_auto.bin","/tama_lastgood.bin"};
  for (uint8_t i = 0; i < 5; ++i) { LittleFS.remove(checksumPath(bases[i])); LittleFS.remove(metadataPath(bases[i])); }
  LittleFS.remove("/tama_active.dat");
  activeProfile_ = 0;
}

void SaveProfileManager::popup(const __FlashStringHelper *line1,
                               const __FlashStringHelper *line2,
                               uint16_t holdMs) const {
  display_.clearDisplay(); display_.setTextSize(1); display_.setTextColor(SSD1306_WHITE);
  display_.drawRoundRect(3, 13, 122, 39, 5, SSD1306_WHITE);
  display_.setCursor(9, 23); display_.print(line1);
  display_.setCursor(9, 36); display_.print(line2);
  display_.display(); delay(holdMs);
}

void SaveProfileManager::runMenu() {
  if (!ready_) { popup(F("LittleFS unavailable"), F("Select 64KB+ FS")); return; }
  const uint8_t count = 9; uint8_t pick = 0; bool oldL = HIGH, oldS = HIGH, oldR = HIGH;
  waitButtonRelease();
  while (true) {
    if (rightHoldExitCheck()) return;
    bool l = digitalRead(btnL_), s = digitalRead(btnS_), r = digitalRead(btnR_);
    if (oldL == HIGH && l == LOW) { pick = pick == 0 ? count - 1 : pick - 1; clickSound(); delay(100); }
    if (oldR == HIGH && r == LOW) { pick = (pick + 1) % count; clickSound(); delay(100); }
    if (oldS == HIGH && s == LOW) {
      waitButtonRelease();
      if (pick == 0) {
        saveGameNow(); bool ok = saveSnapshot(profilePath(activeProfile_));
        ok ? goodSound() : badSound(); popup(ok ? F("Profile saved") : F("Save failed"), activeProfile_ ? F("TEST profile") : F("MAIN profile"));
      } else if (pick == 1) {
        saveGameNow(); saveSnapshot(profilePath(activeProfile_));
        uint8_t target = activeProfile_ ? 0 : 1;
        if (!fileExists(profilePath(target))) copyFile(profilePath(activeProfile_), profilePath(target));
        if (loadSnapshot(profilePath(target))) { activeProfile_ = target; writeActiveMarker(); forceDisplayOn(); goodSound(); popup(F("Profile switched"), activeProfile_ ? F("TEST sandbox") : F("MAIN save")); return; }
        badSound(); popup(F("Switch failed"), F("Current save kept"));
      } else if (pick == 2) {
        saveGameNow(); saveSnapshot(profilePath(activeProfile_));
        bool ok = copyFile(profilePath(activeProfile_), "/tama_test.bin");
        ok ? goodSound() : badSound(); popup(ok ? F("Test cloned") : F("Clone failed"), F("Main state copied"));
      } else if (pick == 3) {
        saveGameNow(); bool ok = saveSnapshot("/tama_backup.bin");
        ok ? goodSound() : badSound(); popup(ok ? F("Backup created") : F("Backup failed"), F("Manual restore point"));
      } else if (pick == 4) {
        bool ok = loadSnapshot("/tama_backup.bin");
        ok ? goodSound() : badSound(); popup(ok ? F("Backup restored") : F("No valid backup"), ok ? F("State reloaded") : F("Nothing changed")); if (ok) return;
      } else if (pick == 5) {
        bool ok = loadSnapshot("/tama_auto.bin");
        ok ? goodSound() : badSound(); popup(ok ? F("Auto backup restored") : F("No auto backup"), ok ? F("Boot state reloaded") : F("Nothing changed")); if (ok) return;
      } else if (pick == 6) {
        bool ok = loadSnapshot(profilePath(activeProfile_));
        ok ? goodSound() : badSound(); popup(ok ? F("Profile reloaded") : F("Reload failed"), activeProfile_ ? F("TEST profile") : F("MAIN profile")); if (ok) return;
      } else if (pick == 7) {
        bool ok = loadSnapshot("/tama_lastgood.bin"); ok ? goodSound() : badSound(); popup(ok ? F("Last-good restored") : F("No valid last-good"), ok ? F("Verified state loaded") : F("Nothing changed")); if (ok) return;
      } else {
        if (LittleFS.exists("/tama_test.bin")) LittleFS.remove("/tama_test.bin");
        LittleFS.remove(checksumPath("/tama_test.bin")); LittleFS.remove(metadataPath("/tama_test.bin"));
        if (activeProfile_ == 1) activeProfile_ = 0;
        writeActiveMarker(); goodSound(); popup(F("Test sandbox erased"), F("Main save untouched"));
      }
    }
    oldL = l; oldS = s; oldR = r;
    display_.clearDisplay(); display_.setTextSize(1); display_.setTextColor(SSD1306_WHITE);
    display_.setCursor(20, 1); display_.print(F("SAFE SAVE PROFILES"));
    display_.setCursor(103, 1); display_.print(pick + 1); display_.print(F("/")); display_.print(count);
    display_.drawLine(0, 11, 127, 11, SSD1306_WHITE);
    display_.drawRoundRect(4, 17, 120, 34, 5, SSD1306_WHITE);
    display_.setCursor(12, 24);
    if (pick == 0) display_.print(F("Save active profile"));
    else if (pick == 1) display_.print(activeProfile_ ? F("Switch to MAIN") : F("Switch to TEST"));
    else if (pick == 2) display_.print(F("Clone active to TEST"));
    else if (pick == 3) display_.print(F("Create restore point"));
    else if (pick == 4) display_.print(F("Restore backup"));
    else if (pick == 5) display_.print(F("Restore auto backup"));
    else if (pick == 6) display_.print(F("Reload active profile"));
    else if (pick == 7) display_.print(F("Restore last-good"));
    else display_.print(F("Erase TEST sandbox"));
    char previewName[7]; uint16_t previewGen = 0, previewDay = 0; const char *previewPath = pick == 4 ? "/tama_backup.bin" : pick == 5 ? "/tama_auto.bin" : pick == 7 ? "/tama_lastgood.bin" : profilePath(activeProfile_);
    display_.setCursor(12, 38); if (readSnapshotPreview(previewPath, previewName, previewGen, previewDay)) { display_.print(previewName); display_.print(F(" G")); display_.print(previewGen); display_.print(F(" D")); display_.print(previewDay); } else { display_.print(F("Active: ")); display_.print(activeProfile_ ? F("TEST") : F("MAIN")); }
    display_.setCursor(2, 56); display_.print(F("D0/D2 browse D1 select"));
    display_.display(); delay(25);
  }
}
