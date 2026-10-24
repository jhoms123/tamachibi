#include "MusicPlayer.h"
#include "NativeUI.h"
#include <SPI.h>
#include <SD.h>
#include "Esp32Platform.h"
#include <cstring>
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

struct Mp3FrameInfo {
  uint32_t bitrateKbps;
  uint32_t sampleRate;
  uint16_t samplesPerFrame;
  uint8_t version;
  bool mono;
  bool hasCrc;
};

bool parseMp3FrameHeader(const uint8_t *header, Mp3FrameInfo &info) {
  if (header[0] != 0xff || (header[1] & 0xe0U) != 0xe0U) return false;
  const uint8_t version = (header[1] >> 3) & 0x03U;
  const uint8_t layer = (header[1] >> 1) & 0x03U;
  const uint8_t bitrateIndex = (header[2] >> 4) & 0x0fU;
  const uint8_t sampleIndex = (header[2] >> 2) & 0x03U;
  if (version == 1 || layer != 1 || bitrateIndex == 0 || bitrateIndex == 15 ||
      sampleIndex == 3)
    return false;

  static const uint16_t mpeg1Layer3Kbps[16] = {
      0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};
  static const uint16_t mpeg2Layer3Kbps[16] = {
      0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0};
  static const uint32_t sampleRates[3] = {44100UL, 48000UL, 32000UL};

  info.version = version;
  info.bitrateKbps = version == 3 ? mpeg1Layer3Kbps[bitrateIndex]
                                  : mpeg2Layer3Kbps[bitrateIndex];
  info.sampleRate = sampleRates[sampleIndex];
  if (version == 2) info.sampleRate /= 2;
  else if (version == 0) info.sampleRate /= 4;
  info.samplesPerFrame = version == 3 ? 1152 : 576;
  info.mono = ((header[3] >> 6) & 0x03U) == 3;
  info.hasCrc = (header[1] & 0x01U) == 0;
  return info.bitrateKbps != 0 && info.sampleRate != 0;
}

uint32_t estimateMp3DurationSeconds(const char *path, bool &approximate) {
  approximate = true;
  File file = SD.open(path);
  if (!file) return 0;
  const uint32_t fileSize = file.size();
  if (fileSize < 16) { file.close(); return 0; }

  uint32_t audioStart = 0;
  uint8_t id3[10] = {};
  if (file.read(id3, sizeof(id3)) == sizeof(id3) && id3[0] == 'I' &&
      id3[1] == 'D' && id3[2] == '3') {
    const uint32_t tagSize =
        (static_cast<uint32_t>(id3[6] & 0x7fU) << 21) |
        (static_cast<uint32_t>(id3[7] & 0x7fU) << 14) |
        (static_cast<uint32_t>(id3[8] & 0x7fU) << 7) |
        static_cast<uint32_t>(id3[9] & 0x7fU);
    const uint64_t afterTag = 10ULL + tagSize + ((id3[5] & 0x10U) ? 10ULL : 0ULL);
    if (afterTag >= fileSize) { file.close(); return 0; }
    audioStart = static_cast<uint32_t>(afterTag);
  }

  if (!file.seek(audioStart)) { file.close(); return 0; }
  uint8_t probe[128] = {};
  const size_t probeBytes = file.read(probe, sizeof(probe));
  Mp3FrameInfo frameInfo{};
  int frameOffsetInProbe = -1;
  for (size_t i = 0; i + 4 <= probeBytes; ++i) {
    if (parseMp3FrameHeader(probe + i, frameInfo)) {
      frameOffsetInProbe = static_cast<int>(i);
      break;
    }
  }
  if (frameOffsetInProbe < 0) { file.close(); return 0; }
  const uint32_t frameOffset = audioStart + static_cast<uint32_t>(frameOffsetInProbe);

  const uint32_t sideInfo = frameInfo.version == 3
                                ? (frameInfo.mono ? 17UL : 32UL)
                                : (frameInfo.mono ? 9UL : 17UL);
  const uint32_t xingOffset = frameOffset + 4UL + (frameInfo.hasCrc ? 2UL : 0UL) + sideInfo;
  uint8_t xing[12] = {};
  if (xingOffset + sizeof(xing) <= fileSize && file.seek(xingOffset) &&
      file.read(xing, sizeof(xing)) == sizeof(xing) &&
      ((memcmp(xing, "Xing", 4) == 0) || (memcmp(xing, "Info", 4) == 0))) {
    const uint32_t flags = (static_cast<uint32_t>(xing[4]) << 24) |
                           (static_cast<uint32_t>(xing[5]) << 16) |
                           (static_cast<uint32_t>(xing[6]) << 8) |
                           static_cast<uint32_t>(xing[7]);
    const uint32_t frames = (static_cast<uint32_t>(xing[8]) << 24) |
                            (static_cast<uint32_t>(xing[9]) << 16) |
                            (static_cast<uint32_t>(xing[10]) << 8) |
                            static_cast<uint32_t>(xing[11]);
    if ((flags & 0x01U) && frames) {
      approximate = false;
      file.close();
      return static_cast<uint32_t>(
          (static_cast<uint64_t>(frames) * frameInfo.samplesPerFrame +
           frameInfo.sampleRate / 2) / frameInfo.sampleRate);
    }
  }

  if (frameInfo.version == 3) {
    uint8_t vbri[18] = {};
    const uint32_t vbriOffset = frameOffset + 36UL;
    if (vbriOffset + sizeof(vbri) <= fileSize && file.seek(vbriOffset) &&
        file.read(vbri, sizeof(vbri)) == sizeof(vbri) &&
        memcmp(vbri, "VBRI", 4) == 0) {
      const uint32_t frames = (static_cast<uint32_t>(vbri[14]) << 24) |
                              (static_cast<uint32_t>(vbri[15]) << 16) |
                              (static_cast<uint32_t>(vbri[16]) << 8) |
                              static_cast<uint32_t>(vbri[17]);
      if (frames) {
        approximate = false;
        file.close();
        return static_cast<uint32_t>(
            (static_cast<uint64_t>(frames) * frameInfo.samplesPerFrame +
             frameInfo.sampleRate / 2) / frameInfo.sampleRate);
      }
    }
  }

  uint32_t audioEnd = fileSize;
  if (fileSize >= 128 && file.seek(fileSize - 128)) {
    uint8_t id3v1[3] = {};
    if (file.read(id3v1, sizeof(id3v1)) == sizeof(id3v1) &&
        memcmp(id3v1, "TAG", 3) == 0)
      audioEnd -= 128;
  }
  const uint32_t audioBytes = audioEnd > frameOffset ? audioEnd - frameOffset : 0;
  const uint64_t bitsPerSecond =
      static_cast<uint64_t>(frameInfo.bitrateKbps) * 1000ULL;
  file.close();
  if (!audioBytes || !bitsPerSecond) return 0;
  return static_cast<uint32_t>((static_cast<uint64_t>(audioBytes) * 8ULL +
                                bitsPerSecond / 2ULL) / bitsPerSecond);
}
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
      status_("STOPPED"), source_(nullptr), decoder_(nullptr), output_(nullptr),
      activeOutput_(nullptr) {}

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
  if (activeOutput_) {
    activeOutput_->stop();
    activeOutput_ = nullptr;
  }
  if (output_) {
    delete output_;
    output_ = nullptr;
  }
  bluetoothOutput_.stop();
  durationSeconds_ = 0;
  playbackElapsedMs_ = 0;
  playbackClockRunning_ = false;
  paused_ = false;
  status_ = status;
}

bool Esp32MusicPlayer::isPlaying() const {
  return decoder_ && decoder_->isRunning();
}

void Esp32MusicPlayer::startPlayback() {
  if (trackCount_ == 0) return;
  stopPlayback("OPENING TRACK");
  durationSeconds_ = estimateMp3DurationSeconds(
      tracks_[selectedTrack_].c_str(), durationApproximate_);
  source_ = new (std::nothrow) AudioFileSourceSD();
  if (!source_ || !source_->open(tracks_[selectedTrack_].c_str())) {
    stopPlayback("CANNOT OPEN FILE");
    return;
  }

  if (bluetoothEnabled_) {
    activeOutput_ = &bluetoothOutput_;
  } else {
    output_ = new (std::nothrow) AudioOutputI2S();
    if (!output_) {
      stopPlayback("AUDIO OUT OF MEMORY");
      return;
    }
    // Stay below the ESP32 DMA frame-size limit while covering OLED transfers.
    output_->SetBuffers(12, 2304);
    output_->SetPinout(AUDIO_BCLK_PIN, AUDIO_LRCLK_PIN, AUDIO_DATA_PIN);
    activeOutput_ = output_;
  }
  activeOutput_->SetGain(gain_);

  buffer_ = new (std::nothrow) AudioFileSourceBuffer(source_, audioBuffer_, sizeof(audioBuffer_));
  if (!buffer_) { stopPlayback("AUDIO OUT OF MEMORY"); return; }
  decoder_ = new (std::nothrow) AudioGeneratorMP3();
  if (!decoder_ || !decoder_->begin(buffer_, activeOutput_)) {
    stopPlayback(bluetoothEnabled_ ? "BT START FAILED" : "MP3 START FAILED");
    return;
  }
  paused_ = false;
  playbackElapsedMs_ = 0;
  playbackClockRunning_ = false;
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
  if (changed && activeOutput_) activeOutput_->SetGain(gain_);
}

void Esp32MusicPlayer::updatePlaybackClock(uint32_t now) {
  const bool advancing = isPlaying() && !paused_ &&
      (!bluetoothEnabled_ || bluetoothOutput_.isConnected());
  if (advancing && !playbackClockRunning_) {
    playbackSegmentStartMs_ = now;
    playbackClockRunning_ = true;
  } else if (!advancing && playbackClockRunning_) {
    playbackElapsedMs_ += now - playbackSegmentStartMs_;
    playbackClockRunning_ = false;
  }
}

uint32_t Esp32MusicPlayer::currentElapsedSeconds() const {
  uint64_t elapsedMs = playbackElapsedMs_;
  if (playbackClockRunning_) elapsedMs += millis() - playbackSegmentStartMs_;
  return static_cast<uint32_t>(elapsedMs / 1000ULL);
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
  String playbackStatus = status_;
  if (bluetoothEnabled_ && isPlaying() && !paused_)
    playbackStatus = bluetoothOutput_.isConnected() ? "BT PLAY" : "PAIRING";
  else if (bluetoothEnabled_ && status_ == "STOPPED")
    playbackStatus = "BT OUTPUT";
  NativeUI::Scope frame(display_); display_.clearDisplay();
  if (browser_) drawBrowser();
  else if (bluetoothMenu_) {
    NativeUI::header(display_, "BLUETOOTH", "S:SELECT");
    String connection = "STATUS: ";
    connection += bluetoothOutput_.connectionStatus();
    NativeUI::text(display_, connection.c_str(), 4, 29, 120);
    char deviceName[33];
    bluetoothOutput_.copyDeviceName(deviceName, sizeof(deviceName));
    NativeUI::text(display_, deviceName[0] ? deviceName
                                           : "Put headphones in pairing mode",
                   4, 44, 120);
    const char *labels[] = {"PAIR / SEARCH", "DISCONNECT", "BACK TO OPTIONS"};
    for (uint8_t row = 0; row < 3; ++row) {
      NativeUI::text(display_, row == bluetoothOption_ ? ">" : " ",
                     4, 62 + row * 16, 8);
      NativeUI::text(display_, labels[row], 14, 62 + row * 16, 110);
    }
    NativeUI::footer(display_, "PREV", "SELECT", "NEXT");
  }
  else if (options_) {
    NativeUI::header(display_, "OPTIONS", "S:SET");
    const char *labels[] = {"BACK TO PLAYER", "BROWSE SONGS", "CHOOSE FOLDER", "SHUFFLE", "REPEAT", "STOP", "EXIT PLAYER", "BLUETOOTH MENU"};
    const uint8_t first = option_ < 4 ? 0 : 4;
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
    NativeUI::text(display_, playbackStatus.c_str(), 7, 79, 56);
    if (isPlaying()) {
      const uint32_t elapsed = currentElapsedSeconds();
      const uint32_t remaining = durationSeconds_ > elapsed
                                     ? durationSeconds_ - elapsed
                                     : 0;
      char timeRemaining[12];
      if (durationSeconds_) {
        snprintf(timeRemaining, sizeof(timeRemaining), "%s%lu:%02lu",
                 durationApproximate_ ? "~" : "",
                 static_cast<unsigned long>(remaining / 60),
                 static_cast<unsigned long>(remaining % 60));
      } else {
        snprintf(timeRemaining, sizeof(timeRemaining), "--:--");
      }
      NativeUI::text(display_, "LEFT", 65, 79, 25);
      NativeUI::text(display_, timeRemaining, 92, 79, 36);
    }
    NativeUI::text(display_, "VOL", 7, 93, 20);
    NativeUI::meter(display_, 32, 94, 91, static_cast<int>(gain_ * 100), 90);
    NativeUI::text(display_, "Hold S:menu R:exit", 4, 105, 120);
    NativeUI::footer(display_, "PREV", paused_ || !isPlaying() ? "PLAY" : "PAUSE", "NEXT");
  }
  display_.display();
  if (info_) {
    String title = trackCount_ ? trackName(selectedTrack_) : "No MP3 files";
    String detail = playbackStatus;
    if (browser_) {
      const uint16_t count = folders_ ? folderCount() : trackCount_;
      title = browseIndex_ == count ? "Back to options" : folders_ ? folderAt(browseIndex_) : trackName(browseIndex_);
      if (folders_ && browseIndex_ == 0) title = "All folders";
      detail = folders_ ? "Choose a collection" : "Select song to play";
    } else if (bluetoothMenu_) {
      title = "Bluetooth headphones";
      detail = bluetoothOutput_.connectionStatus();
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
  options_ = false; browser_ = false; bluetoothMenu_ = false; paused_ = false;
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
    bluetoothOutput_.service();
    updatePlaybackClock(now);
    const uint8_t pins[3] = {buttonLeft_, buttonSelect_, buttonRight_};
    for (uint8_t i = 0; i < 3; ++i) {
      const bool sample = digitalRead(pins[i]) == LOW;
      if (sample != raw[i]) { raw[i] = sample; rawSince[i] = now; }
      const bool pressed = now - rawSince[i] >= 20UL ? raw[i] : prev[i];
      if (pressed && !prev[i]) { down[i] = now; held[i] = false; }
      if (i != 0 && pressed && !held[i] && now - down[i] >= RIGHT_HOLD_EXIT_MS) {
        held[i] = true;
        if (i == 2) {
          stopPlayback("STOPPED");
          bluetoothOutput_.disconnect();
          bluetoothEnabled_ = false;
          bluetoothMenu_ = false;
          waitForButtonsReleased();
          return;
        }
        if (i == 1) {
          if (bluetoothMenu_) {
            bluetoothMenu_ = false;
            options_ = true;
            option_ = 7;
          } else {
            browser_ = false;
            options_ = !options_;
            option_ = 0;
          }
        }
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
        } else if (bluetoothMenu_) {
          if (i != 1) {
            bluetoothOption_ = (bluetoothOption_ + (i == 0 ? 2 : 1)) % 3;
          } else if (bluetoothOption_ == 0) {
            if (!bluetoothEnabled_ && isPlaying()) stopPlayback("STOPPED");
            bluetoothEnabled_ = true;
            if (!bluetoothOutput_.isConnected())
              bluetoothOutput_.startPairing();
          } else if (bluetoothOption_ == 1) {
            if (bluetoothEnabled_) stopPlayback("STOPPED");
            bluetoothOutput_.disconnect();
            bluetoothEnabled_ = false;
          } else {
            bluetoothMenu_ = false;
            options_ = true;
            option_ = 7;
          }
        } else if (options_) {
          if (i != 1) option_ = (option_ + (i == 0 ? 7 : 1)) % 8;
          else if (option_ == 0) options_ = false;
          else if (option_ == 1 || option_ == 2) { browser_ = true; folders_ = option_ == 2; browseIndex_ = 0; }
          else if (option_ == 3) { shuffle_ = !shuffle_; rebuildQueue(); }
          else if (option_ == 4) repeat_ = (repeat_ + 1) % 3;
          else if (option_ == 5) stopPlayback("STOPPED");
          else if (option_ == 6) {
            stopPlayback("STOPPED");
            bluetoothOutput_.disconnect();
            bluetoothEnabled_ = false;
            return;
          }
          else if (option_ == 7) {
            if (!bluetoothEnabled_ && isPlaying()) stopPlayback("STOPPED");
            bluetoothEnabled_ = true;
            bluetoothMenu_ = true;
            bluetoothOption_ = 0;
            options_ = false;
            if (!bluetoothOutput_.isConnected())
              bluetoothOutput_.begin();
          }
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

