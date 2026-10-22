#include "MusicPlayer.h"
#include <SPI.h>
#include <SD.h>
#include "Esp32Platform.h"

#ifndef SD_MISO_PIN
#define SD_MISO_PIN 19
#define SD_CS_PIN 27
#define SD_SCK_PIN 18
#define SD_MOSI_PIN 23
#define AUDIO_BCLK_PIN 14
#define AUDIO_LRCLK_PIN 13
#define AUDIO_DATA_PIN 4
#endif

namespace {
constexpr uint32_t SD_SPI_HZ = 8000000UL;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 35UL;
constexpr uint32_t SCREEN_REFRESH_MS = 180UL;
constexpr uint32_t RIGHT_HOLD_EXIT_MS = 900UL;
constexpr const char *MUSIC_DIRECTORY = "/music";
}

Esp32MusicPlayer::Esp32MusicPlayer(Adafruit_SSD1306 &display,
                                   uint8_t buttonLeft,
                                   uint8_t buttonSelect,
                                   uint8_t buttonRight,
                                   uint8_t volumePin)
    : display_(display), buttonLeft_(buttonLeft), buttonSelect_(buttonSelect),
      buttonRight_(buttonRight), volumePin_(volumePin), trackCount_(0),
      selectedTrack_(0), spiStarted_(false), sdReady_(false),
      playbackFinished_(false), gain_(0.0f), status_("STOPPED"),
      source_(nullptr), decoder_(nullptr), output_(nullptr) {}

bool Esp32MusicPlayer::initializeCard() {
  if (!spiStarted_) {
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    spiStarted_ = true;
  }
  if (!sdReady_) sdReady_ = SD.begin(SD_CS_PIN, SPI, SD_SPI_HZ);
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
  if (decoder_) {
    if (decoder_->isRunning()) decoder_->stop();
    delete decoder_;
    decoder_ = nullptr;
  }
  if (source_) {
    source_->close();
    delete source_;
    source_ = nullptr;
  }
  if (output_) {
    output_->stop();
    delete output_;
    output_ = nullptr;
  }
  status_ = status;
}

bool Esp32MusicPlayer::isPlaying() const {
  return decoder_ && decoder_->isRunning();
}

void Esp32MusicPlayer::startPlayback() {
  if (trackCount_ == 0) return;
  stopPlayback("OPENING TRACK");
  source_ = new AudioFileSourceSD();
  if (!source_ || !source_->open(tracks_[selectedTrack_].c_str())) {
    stopPlayback("CANNOT OPEN FILE");
    return;
  }

  output_ = new AudioOutputI2S();
  if (!output_) {
    stopPlayback("AUDIO OUT OF MEMORY");
    return;
  }
  output_->SetPinout(AUDIO_BCLK_PIN, AUDIO_LRCLK_PIN, AUDIO_DATA_PIN);
  output_->SetGain(gain_);

  decoder_ = new AudioGeneratorMP3();
  if (!decoder_ || !decoder_->begin(source_, output_)) {
    stopPlayback("MP3 START FAILED");
    return;
  }
  playbackFinished_ = false;
  status_ = "PLAYING";
}

void Esp32MusicPlayer::updateGain() {
  const int raw = analogRead(volumePin_);
  const int bounded = constrain(raw, 0, 1023);
  gain_ = (static_cast<float>(bounded) / 1023.0f) * 0.90f;
  if (output_) output_->SetGain(gain_);
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
  const uint8_t bars = static_cast<uint8_t>(gain_ * 10.0f + 0.5f);
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
    status_ = "SD CARD NOT READY";
    drawPlayerScreen();
    waitForButtonsReleased();
    while (digitalRead(buttonSelect_) == HIGH) delay(10);
    waitForButtonsReleased();
    return;
  }
  scanMusicDirectory();
  selectedTrack_ = trackCount_ == 0 ? 0 : selectedTrack_ % trackCount_;
  updateGain();
  drawPlayerScreen();
  waitForButtonsReleased();

  bool oldLeft = HIGH;
  bool oldSelect = HIGH;
  bool rightWasDown = false;
  bool rightHoldExited = false;
  uint32_t rightDownAt = 0;
  uint32_t lastDrawAt = 0;
  uint32_t lastGainAt = 0;

  while (true) {
    const uint32_t now = millis();
    if (isPlaying() && !decoder_->loop()) {
      stopPlayback("FINISHED");
      playbackFinished_ = true;
    }
    if (now - lastGainAt >= 80UL) {
      updateGain();
      lastGainAt = now;
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
