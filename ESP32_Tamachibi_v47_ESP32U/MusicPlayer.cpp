#include "MusicPlayer.h"
#include "NativeUI.h"
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

Esp32MusicPlayer::Esp32MusicPlayer(GameDisplay &display,
                                   uint8_t buttonLeft,
                                   uint8_t buttonSelect,
                                   uint8_t buttonRight,
                                   uint8_t volumeEncoderClk,
                                   uint8_t volumeEncoderDt)
    : display_(display), buttonLeft_(buttonLeft), buttonSelect_(buttonSelect),
      buttonRight_(buttonRight), volumeEncoderClk_(volumeEncoderClk),
      volumeEncoderDt_(volumeEncoderDt), trackCount_(0), selectedTrack_(0),
      encoderPreviousState_(0), encoderTransitionCount_(0), spiStarted_(false),
      sdReady_(false), playbackFinished_(false), gain_(0.55f),
      status_("STOPPED"), source_(nullptr), decoder_(nullptr), output_(nullptr) {}

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
    gain_ = gain_ < 0.90f ? gain_ + 0.05f : 0.90f;
    encoderTransitionCount_ = 0;
    changed = true;
  } else if (encoderTransitionCount_ <= -4) {
    gain_ = gain_ > 0.0f ? gain_ - 0.05f : 0.0f;
    encoderTransitionCount_ = 0;
    changed = true;
  }
  if (changed && output_) output_->SetGain(gain_);
}

void Esp32MusicPlayer::drawPlayerScreen() const {
  NativeUI::Scope frame(display_);display_.clearDisplay();char counter[12];snprintf(counter,sizeof(counter),"%u/%u",trackCount_?selectedTrack_+1:0,trackCount_);
  NativeUI::header(display_,"MUSIC",counter);NativeUI::icon(display_,6,56,25);
  String name=trackCount_?trackName(selectedTrack_):String("Add MP3 files to /music");
  NativeUI::paragraph(display_,name.c_str(),7,50,114,2);
  NativeUI::text(display_,status_.c_str(),7,78,114);
  NativeUI::text(display_,"VOL",7,101,18);NativeUI::meter(display_,32,102,91,static_cast<int>(gain_*100),90);
  NativeUI::footer(display_,"PREV",isPlaying()?"PAUSE":"PLAY","NEXT");display_.display();
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
    if (now - lastGainAt >= 4UL) {
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
