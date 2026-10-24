#include "MusicPlayer.h"
#include "NativeUI.h"
#include <SPI.h>
#include <SD.h>
#include "Esp32Platform.h"
#include <new>

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
      sdReady_(false), gain_(0.55f),
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
  // Re-mount on entry so a removed or replaced card never leaves stale tracks.
  if (sdReady_) SD.end();
  sdReady_ = SD.begin(SD_CS_PIN, SPI, SD_SPI_HZ, "/sd", 12);
  return sdReady_;
}

bool Esp32MusicPlayer::isMp3Path(const String &path) {
  String lower = path;
  lower.toLowerCase();
  return lower.endsWith(".mp3");
}

bool Esp32MusicPlayer::scanMusicDirectory() {
  for (auto &track : tracks_) track = String();
  trackCount_ = 0;
  libraryLimited_ = false;
  queue_.reset();
  folderCount_ = 0;
  folder_ = String();
  File directory = SD.open(MUSIC_DIRECTORY);
  if (!directory || !directory.isDirectory()) {
    if (directory) directory.close();
    status_ = "CREATE /music FOLDER";
    return false;
  }

  directory.close();
  scanFolder(MUSIC_DIRECTORY, 0);

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
  for (uint16_t i = 0; i < trackCount_; ++i) {
    const String parent = tracks_[i].substring(0, tracks_[i].lastIndexOf('/'));
    bool seen = false;
    for (uint16_t j = 0; j < folderCount_; ++j) {
      const String &path = tracks_[folderTracks_[j]];
      if (path.substring(0, path.lastIndexOf('/')) == parent) { seen = true; break; }
    }
    if (!seen) folderTracks_[folderCount_++] = static_cast<uint8_t>(i);
  }
  rebuildQueue();
  if (trackCount_ == 0) status_ = "NO MP3 IN /music";
  else status_ = "STOPPED";
  return trackCount_ > 0;
}

void Esp32MusicPlayer::scanFolder(const String &path, uint8_t depth) {
  if (depth > 8 || trackCount_ >= MAX_TRACKS) { libraryLimited_ = true; return; }
  File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) return;
  File entry = dir.openNextFile();
  while (entry && trackCount_ < MAX_TRACKS) {
    String name = entry.name();
    if (name.startsWith(".") || name.indexOf("/.") >= 0) { entry.close(); entry = dir.openNextFile(); continue; }
    String full = name.startsWith("/") ? name : path + "/" + name;
    if (entry.isDirectory()) scanFolder(full, depth + 1);
    else if (isMp3Path(full)) tracks_[trackCount_++] = full;
    entry.close();
    entry = dir.openNextFile();
  }
  if (entry) { libraryLimited_ = true; entry.close(); }
  dir.close();
}

void Esp32MusicPlayer::rebuildQueue() {
  queue_.reset();
  for (uint16_t i = 0; i < trackCount_; ++i)
    if (!folder_.length() || tracks_[i].substring(0, tracks_[i].lastIndexOf('/')) == folder_)
      queue_.add(static_cast<uint8_t>(i));
  if (shuffle_) queue_.shuffle(static_cast<uint32_t>(random(1, 0x7fffffff)), selectedTrack_);
  else queue_.select(selectedTrack_);
  selectedTrack_ = queue_.current();
}

bool Esp32MusicPlayer::advance(int8_t direction, bool wrap) {
  if (!queue_.step(direction, wrap)) return false;
  selectedTrack_ = queue_.current();
  return true;
}

uint16_t Esp32MusicPlayer::folderCount() const { return folderCount_ + 1; }

String Esp32MusicPlayer::folderAt(uint16_t index) const {
  if (!index || index > folderCount_) return String();
  const String &path = tracks_[folderTracks_[index - 1]];
  return path.substring(0, path.lastIndexOf('/'));
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
  if (buffer_) {
    buffer_->close();
    delete buffer_;
    buffer_ = nullptr;
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
  paused_ = false;
  status_ = status;
}

bool Esp32MusicPlayer::isPlaying() const {
  return decoder_ && decoder_->isRunning();
}

void Esp32MusicPlayer::startPlayback() {
  if (trackCount_ == 0) return;
  stopPlayback("OPENING TRACK");
  source_ = new (std::nothrow) AudioFileSourceSD();
  if (!source_ || !source_->open(tracks_[selectedTrack_].c_str())) {
    stopPlayback("CANNOT OPEN FILE");
    return;
  }

  output_ = new (std::nothrow) AudioOutputI2S();
  if (!output_) {
    stopPlayback("AUDIO OUT OF MEMORY");
    return;
  }
  // Stay below the ESP32 DMA frame-size limit while covering OLED transfers.
  output_->SetBuffers(12, 2304);
  output_->SetPinout(AUDIO_BCLK_PIN, AUDIO_LRCLK_PIN, AUDIO_DATA_PIN);
  output_->SetGain(gain_);

  buffer_ = new (std::nothrow) AudioFileSourceBuffer(source_, audioBuffer_, sizeof(audioBuffer_));
  if (!buffer_) { stopPlayback("AUDIO OUT OF MEMORY"); return; }
  decoder_ = new (std::nothrow) AudioGeneratorMP3();
  if (!decoder_ || !decoder_->begin(buffer_, output_)) {
    stopPlayback("MP3 START FAILED");
    return;
  }
  paused_ = false;
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
  if (gain_ < 0.0f) gain_ = 0.0f;
  if (gain_ > 0.90f) gain_ = 0.90f;
  if (changed && output_) output_->SetGain(gain_);
}

void Esp32MusicPlayer::drawBrowser() const {
  const uint16_t count = folders_ ? folderCount() : trackCount_;
  NativeUI::header(display_, folders_ ? "FOLDERS" : "SONGS", "S:OPEN");
  const uint16_t first = (browseIndex_ / 5) * 5;
  for (uint16_t row = 0; row < 5 && first + row <= count; ++row) {
    uint16_t index = first + row;
    String label = index == count ? "< BACK" : folders_ ? folderAt(index) : trackName(index);
    if (folders_ && index == 0) label = "ALL FOLDERS";
    if (label.startsWith("/music/")) label = label.substring(7);
    NativeUI::text(display_, index == browseIndex_ ? ">" : " ", 4, 30 + row * 14, 8);
    NativeUI::text(display_, label.c_str(), 14, 30 + row * 14, 110);
  }
  NativeUI::footer(display_, "PREV", "OPEN", "NEXT");
}

void Esp32MusicPlayer::drawPlayerScreen() const {
  NativeUI::Scope frame(display_); display_.clearDisplay();
  if (browser_) drawBrowser();
  else if (options_) {
    NativeUI::header(display_, "OPTIONS", "S:SET");
    const char *labels[] = {"BACK TO PLAYER", "BROWSE SONGS", "CHOOSE FOLDER", "SHUFFLE", "REPEAT", "STOP", "EXIT PLAYER"};
    const uint8_t first = option_ < 4 ? 0 : 3;
    for (uint8_t row = 0; row < 4; ++row) {
      uint8_t index = first + row;
      String label = labels[index];
      if (index == 3) label += shuffle_ ? " ON" : " OFF";
      if (index == 4) label += repeat_ == 0 ? " OFF" : repeat_ == 1 ? " ALL" : " ONE";
      NativeUI::text(display_, index == option_ ? ">" : " ", 4, 30 + row * 16, 8);
      NativeUI::text(display_, label.c_str(), 14, 30 + row * 16, 110);
    }
    NativeUI::text(display_, "Hold RIGHT to exit", 4, 102, 120);
    NativeUI::footer(display_, "PREV", "CHOOSE", "NEXT");
  } else {
    char context[20];
    snprintf(context, sizeof(context), "%u/%u", queue_.count() ? queue_.position() + 1 : 0, queue_.count());
    NativeUI::header(display_, "MUSIC", context);
    if (scene_) scene_(isPlaying() && !paused_);
    NativeUI::text(display_, status_.c_str(), 7, 79, isPlaying() ? 66 : 114);
    if (isPlaying()) {
      String flags = shuffle_ ? "S/" : "";
      flags += repeat_ == 2 ? "R1" : repeat_ == 1 ? "RA" : "";
      NativeUI::text(display_, flags.c_str(), 89, 79, 36);
    }
    NativeUI::text(display_, "VOL", 7, 93, 20);
    NativeUI::meter(display_, 32, 94, 91, static_cast<int>(gain_ * 100), 90);
    NativeUI::text(display_, "Hold S:menu R:exit", 4, 105, 120);
    NativeUI::footer(display_, "PREV", paused_ || !isPlaying() ? "PLAY" : "PAUSE", "NEXT");
  }
  display_.display();
  if (info_) {
    String title = trackCount_ ? trackName(selectedTrack_) : "No MP3 files";
    String detail = status_;
    if (browser_) {
      const uint16_t count = folders_ ? folderCount() : trackCount_;
      title = browseIndex_ == count ? "Back to options" : folders_ ? folderAt(browseIndex_) : trackName(browseIndex_);
      if (folders_ && browseIndex_ == 0) title = "All folders";
      detail = folders_ ? "Choose a collection" : "Select song to play";
    } else if (libraryLimited_) detail += " (library limit)";
    info_(title, detail);
  }
}

void Esp32MusicPlayer::waitForButtonsReleased() const {
  while (digitalRead(buttonLeft_) == LOW || digitalRead(buttonSelect_) == LOW ||
         digitalRead(buttonRight_) == LOW) {
    delay(10);
  }
}

void Esp32MusicPlayer::moveSelection(int8_t direction) {
  const bool resume = isPlaying() && !paused_;
  stopPlayback("STOPPED");
  if (advance(direction) && resume) startPlayback();
  paused_ = false;
}

void Esp32MusicPlayer::run() {
  options_ = false; browser_ = false; paused_ = false;
  trackCount_ = 0; folderCount_ = 0; libraryLimited_ = false; queue_.reset();
  if (!initializeCard()) status_ = "SD CARD NOT READY";
  else scanMusicDirectory();
  if (selectedTrack_ >= trackCount_) selectedTrack_ = 0;
  waitForButtonsReleased();
  bool prev[3] = {false, false, false};
  bool raw[3] = {false, false, false};
  uint32_t rawSince[3] = {0, 0, 0};
  uint32_t down[3] = {0, 0, 0}; bool held[3] = {false, false, false};
  uint32_t redraw = 0;
  drawPlayerScreen();
  while (true) {
    uint32_t now = millis();
    if (isPlaying() && !paused_ && !decoder_->loop()) {
      stopPlayback("FINISHED");
      if (repeat_ == 2) startPlayback();
      else if (advance(1, repeat_ == 1)) startPlayback();
    }
    updateGain();
    const uint8_t pins[3] = {buttonLeft_, buttonSelect_, buttonRight_};
    for (uint8_t i = 0; i < 3; ++i) {
      const bool sample = digitalRead(pins[i]) == LOW;
      if (sample != raw[i]) { raw[i] = sample; rawSince[i] = now; }
      const bool pressed = now - rawSince[i] >= 20UL ? raw[i] : prev[i];
      if (pressed && !prev[i]) { down[i] = now; held[i] = false; }
      if (i != 0 && pressed && !held[i] && now - down[i] >= RIGHT_HOLD_EXIT_MS) {
        held[i] = true;
        if (i == 2) { stopPlayback("STOPPED"); waitForButtonsReleased(); return; }
        if (i == 1) { browser_ = false; options_ = !options_; option_ = 0; }
      }
      if (!pressed && prev[i] && !held[i] && now - down[i] >= BUTTON_DEBOUNCE_MS) {
        if (browser_) {
          uint16_t count = folders_ ? folderCount() : trackCount_;
          if (i != 1) browseIndex_ = (browseIndex_ + count + 1 + (i == 0 ? -1 : 1)) % (count + 1);
          else if (browseIndex_ == count) browser_ = false;
          else {
            stopPlayback("STOPPED");
            if (folders_) { folder_ = folderAt(browseIndex_); rebuildQueue(); }
            else { folder_ = String(); selectedTrack_ = browseIndex_; rebuildQueue(); startPlayback(); }
            browser_ = false; options_ = false;
          }
        } else if (options_) {
          if (i != 1) option_ = (option_ + (i == 0 ? 6 : 1)) % 7;
          else if (option_ == 0) options_ = false;
          else if (option_ == 1 || option_ == 2) { browser_ = true; folders_ = option_ == 2; browseIndex_ = 0; }
          else if (option_ == 3) { shuffle_ = !shuffle_; rebuildQueue(); }
          else if (option_ == 4) repeat_ = (repeat_ + 1) % 3;
          else if (option_ == 5) stopPlayback("STOPPED");
          else if (option_ == 6) { stopPlayback("STOPPED"); return; }
        } else if (i == 0) moveSelection(-1);
        else if (i == 2) moveSelection(1);
        else if (isPlaying()) { paused_ = !paused_; status_ = paused_ ? "PAUSED" : "PLAYING"; }
        else startPlayback();
      }
      prev[i] = pressed;
    }
    if (now - redraw >= SCREEN_REFRESH_MS) { drawPlayerScreen(); redraw = now; }
    delay(1);
  }
}
