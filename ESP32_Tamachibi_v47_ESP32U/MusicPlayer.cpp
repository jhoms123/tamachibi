#include "MusicPlayer.h"
#include <SPI.h>
#include <SD.h>

namespace {
constexpr uint32_t SD_SPI_HZ = 8000000UL;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 35UL;
constexpr uint32_t SCREEN_REFRESH_MS = 180UL;
constexpr uint32_t RIGHT_HOLD_EXIT_MS = 900UL;
constexpr const char *MUSIC_DIRECTORY = "/music";

// GPIO connections for the HW-261 shield. These are ESP32 GPIO numbers;
// the silk-screened D2/D6/D7/D8/D9/D11/D12/D13 numbers are Arduino Uno pins.
constexpr uint8_t SD_CS_PIN = 27;          // Shield D9: TF-card chip select
constexpr uint8_t SD_SCK_PIN = 18;          // Shield D13: SPI clock
constexpr uint8_t SD_MISO_PIN = 19;         // Shield D12: SPI MISO
constexpr uint8_t SD_MOSI_PIN = 23;         // Shield D11: SPI MOSI
constexpr uint8_t VS1053_RESET_PIN = 4;     // Shield D8: X_RESET
constexpr uint8_t VS1053_XDCS_PIN = 13;     // Shield D7: X_DCS
constexpr uint8_t VS1053_XCS_PIN = 14;      // Shield D6: X_CS
constexpr uint8_t VS1053_DREQ_PIN = 34;     // Shield D2: DREQ (input only is OK)
}

Esp32MusicPlayer::Esp32MusicPlayer(Adafruit_SSD1306 &display,
                                   uint8_t buttonLeft,
                                   uint8_t buttonSelect,
                                   uint8_t buttonRight,
                                   uint8_t volumeEncoderClk,
                                   uint8_t volumeEncoderDt)
    : display_(display), buttonLeft_(buttonLeft), buttonSelect_(buttonSelect),
      buttonRight_(buttonRight), volumeEncoderClk_(volumeEncoderClk),
      volumeEncoderDt_(volumeEncoderDt),
      codec_(VS1053_RESET_PIN, VS1053_XCS_PIN, VS1053_XDCS_PIN,
             VS1053_DREQ_PIN, SD_CS_PIN),
      trackCount_(0), selectedTrack_(0), encoderPreviousState_(0),
      encoderTransitionCount_(0), lastVolumeAttenuation_(255),
      spiStarted_(false), codecReady_(false), sdReady_(false),
      playbackFinished_(false), volumeLevel_(0.55f), status_("STOPPED") {}

bool Esp32MusicPlayer::initializeCard() {
  if (!spiStarted_) {
    pinMode(SD_CS_PIN, OUTPUT);
    digitalWrite(SD_CS_PIN, HIGH);
    pinMode(volumeEncoderClk_, INPUT);
    pinMode(volumeEncoderDt_, INPUT);
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    spiStarted_ = true;
    encoderPreviousState_ =
        (static_cast<uint8_t>(digitalRead(volumeEncoderClk_)) << 1) |
        static_cast<uint8_t>(digitalRead(volumeEncoderDt_));
  }

  if (!codecReady_) {
    if (!codec_.begin()) {
      status_ = "VS1053 NOT FOUND";
      return false;
    }
    codecReady_ = true;
    lastVolumeAttenuation_ = 255;
    const uint8_t attenuation = volumeAttenuation();
    codec_.setVolume(attenuation, attenuation);
    lastVolumeAttenuation_ = attenuation;
  }

  if (!sdReady_) {
    sdReady_ = SD.begin(SD_CS_PIN, SPI, SD_SPI_HZ);
  }
  if (!sdReady_) status_ = "SD CARD NOT READY";
  return sdReady_;
}

bool Esp32MusicPlayer::isMp3Path(const String &path) {
  String lower = path;
  lower.toLowerCase();
  return lower.endsWith(".mp3");
}

bool Esp32MusicPlayer::scanMusicDirectory() {
  trackCount_ = 0;
  File directory = SD.open(MUSIC_DIRECTORY);
  if (!directory || !directory.isDirectory()) {
    if (directory) directory.close();
    status_ = "CREATE /music FOLDER";
    return false;
  }

  File entry = directory.openNextFile();
  while (entry && trackCount_ < MAX_TRACKS) {
    if (!entry.isDirectory()) {
      String path = entry.name();
      if (!path.startsWith("/")) path = String(MUSIC_DIRECTORY) + "/" + path;
      if (isMp3Path(path)) tracks_[trackCount_++] = path;
    }
    entry.close();
    entry = directory.openNextFile();
  }
  directory.close();

  // Keep playback order predictable: 01_..., 02_..., and so on.
  for (uint8_t i = 0; i < trackCount_; ++i) {
    for (uint8_t j = i + 1; j < trackCount_; ++j) {
      if (tracks_[i].compareTo(tracks_[j]) > 0) {
        String temp = tracks_[i];
        tracks_[i] = tracks_[j];
        tracks_[j] = temp;
      }
    }
  }
  if (trackCount_ == 0) status_ = "NO MP3 IN /music";
  else status_ = "STOPPED";
  return trackCount_ > 0;
}

String Esp32MusicPlayer::trackName(uint8_t index) const {
  if (index >= trackCount_) return String();
  const int slash = tracks_[index].lastIndexOf('/');
  return slash >= 0 ? tracks_[index].substring(slash + 1) : tracks_[index];
}

void Esp32MusicPlayer::stopPlayback(const char *status) {
  if (codecReady_ && codec_.playingMusic) codec_.stopPlaying();
  status_ = status;
}

bool Esp32MusicPlayer::isPlaying() const {
  return codecReady_ && codec_.playingMusic;
}

void Esp32MusicPlayer::startPlayback() {
  if (trackCount_ == 0 || !codecReady_) return;
  stopPlayback("OPENING TRACK");
  if (!codec_.startPlayingFile(tracks_[selectedTrack_].c_str())) {
    status_ = "CANNOT OPEN FILE";
    return;
  }
  const uint8_t attenuation = volumeAttenuation();
  codec_.setVolume(attenuation, attenuation);
  lastVolumeAttenuation_ = attenuation;
  playbackFinished_ = false;
  status_ = "PLAYING";
}

uint8_t Esp32MusicPlayer::volumeAttenuation() const {
  // VS1053 volume bytes are attenuation: 0 is loudest and 255 is mute.
  // A curved scale gives the encoder useful control over the listening range.
  const float remaining = 1.0f - volumeLevel_;
  int attenuation = static_cast<int>(remaining * remaining * 255.0f + 0.5f);
  return static_cast<uint8_t>(constrain(attenuation, 0, 255));
}

void Esp32MusicPlayer::updateVolume() {
  static const int8_t transitions[16] = {
      0, -1, 1, 0, 1, 0, 0, -1,
      -1, 0, 0, 1, 0, 1, -1, 0};
  const uint8_t currentState =
      (static_cast<uint8_t>(digitalRead(volumeEncoderClk_)) << 1) |
      static_cast<uint8_t>(digitalRead(volumeEncoderDt_));
  const uint8_t transition = (encoderPreviousState_ << 2) | currentState;
  encoderTransitionCount_ += transitions[transition];
  encoderPreviousState_ = currentState;

  bool changed = false;
  if (encoderTransitionCount_ >= 4) {
    volumeLevel_ = min(0.95f, volumeLevel_ + 0.05f);
    encoderTransitionCount_ = 0;
    changed = true;
  } else if (encoderTransitionCount_ <= -4) {
    volumeLevel_ = max(0.0f, volumeLevel_ - 0.05f);
    encoderTransitionCount_ = 0;
    changed = true;
  }

  if (changed && codecReady_) {
    const uint8_t attenuation = volumeAttenuation();
    if (attenuation != lastVolumeAttenuation_) {
      codec_.setVolume(attenuation, attenuation);
      lastVolumeAttenuation_ = attenuation;
    }
  }
}

void Esp32MusicPlayer::drawPlayerScreen() const {
  display_.clearDisplay();
  display_.setTextSize(1);
  display_.setTextColor(SSD1306_WHITE);
  display_.setTextWrap(false);
  display_.setCursor(0, 0);
  display_.print(F("SD MUSIC"));
  display_.setCursor(83, 0);
  if (trackCount_ > 0) {
    display_.print(selectedTrack_ + 1);
    display_.print('/');
    display_.print(trackCount_);
  }
  display_.drawLine(0, 10, 127, 10, SSD1306_WHITE);

  if (trackCount_ > 0) {
    String label = trackName(selectedTrack_);
    if (label.length() > 20) label = label.substring(0, 20);
    display_.setCursor(0, 17);
    display_.print(label);
  } else {
    display_.setCursor(0, 18);
    display_.print(F("Add .mp3 files to"));
    display_.setCursor(0, 29);
    display_.print(MUSIC_DIRECTORY);
  }

  display_.setCursor(0, 39);
  display_.print(status_);
  display_.setCursor(0, 50);
  display_.print(F("VOL "));
  const uint8_t bars = static_cast<uint8_t>(volumeLevel_ * 10.0f + 0.5f);
  for (uint8_t i = 0; i < 10; ++i) display_.print(i < bars ? '|' : '.');
  display_.setCursor(0, 59);
  display_.print(F("D0 prev D1 play D2 next"));
  display_.display();
}

void Esp32MusicPlayer::waitForButtonsReleased() const {
  while (digitalRead(buttonLeft_) == LOW || digitalRead(buttonSelect_) == LOW ||
         digitalRead(buttonRight_) == LOW) {
    delay(10);
  }
}

void Esp32MusicPlayer::moveSelection(int8_t direction) {
  if (trackCount_ == 0) return;
  if (isPlaying()) stopPlayback("STOPPED");
  if (direction < 0) selectedTrack_ = selectedTrack_ == 0 ? trackCount_ - 1 : selectedTrack_ - 1;
  else selectedTrack_ = (selectedTrack_ + 1) % trackCount_;
  playbackFinished_ = false;
}

void Esp32MusicPlayer::run() {
  if (!initializeCard()) {
    drawPlayerScreen();
    waitForButtonsReleased();
    while (digitalRead(buttonSelect_) == HIGH) delay(10);
    waitForButtonsReleased();
    return;
  }
  scanMusicDirectory();
  selectedTrack_ = trackCount_ == 0 ? 0 : selectedTrack_ % trackCount_;
  updateVolume();
  drawPlayerScreen();
  waitForButtonsReleased();

  bool oldLeft = HIGH;
  bool oldSelect = HIGH;
  bool rightWasDown = false;
  bool rightHoldExited = false;
  uint32_t rightDownAt = 0;
  uint32_t lastDrawAt = 0;
  uint32_t lastVolumeAt = 0;

  while (true) {
    const uint32_t now = millis();
    const bool wasPlaying = isPlaying();
    if (wasPlaying) codec_.feedBuffer();
    if (wasPlaying && !isPlaying()) {
      status_ = "FINISHED";
      playbackFinished_ = true;
    }
    if (now - lastVolumeAt >= 4UL) {
      updateVolume();
      lastVolumeAt = now;
    }

    const bool left = digitalRead(buttonLeft_);
    const bool select = digitalRead(buttonSelect_);
    const bool right = digitalRead(buttonRight_);
    if (oldLeft == HIGH && left == LOW) {
      moveSelection(-1);
      delay(BUTTON_DEBOUNCE_MS);
    }
    if (oldSelect == HIGH && select == LOW) {
      if (isPlaying()) stopPlayback("STOPPED");
      else if (trackCount_ > 0) startPlayback();
      delay(BUTTON_DEBOUNCE_MS);
    }

    if (!rightWasDown && right == LOW) {
      rightWasDown = true;
      rightDownAt = now;
      rightHoldExited = false;
    } else if (rightWasDown && right == LOW && !rightHoldExited &&
               now - rightDownAt >= RIGHT_HOLD_EXIT_MS) {
      rightHoldExited = true;
      stopPlayback("STOPPED");
      waitForButtonsReleased();
      return;
    } else if (rightWasDown && right == HIGH) {
      if (!rightHoldExited) {
        moveSelection(1);
        delay(BUTTON_DEBOUNCE_MS);
      }
      rightWasDown = false;
    }

    oldLeft = left;
    oldSelect = select;
    if (now - lastDrawAt >= SCREEN_REFRESH_MS) {
      drawPlayerScreen();
      lastDrawAt = now;
    }
    delay(1);
  }
}
