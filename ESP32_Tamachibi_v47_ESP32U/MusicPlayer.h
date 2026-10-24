#pragma once

#include <Arduino.h>
#include "GameDisplay.h"
#include "MusicQueue.h"
#include <AudioOutput.h>
#include <AudioOutputI2S.h>
#include <AudioFileSourceBuffer.h>
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include "BluetoothAudioOutput.h"

class Esp32MusicPlayer {
public:
  Esp32MusicPlayer(GameDisplay &display, uint8_t buttonLeft,
                   uint8_t buttonSelect, uint8_t buttonRight,
                   uint8_t volumeEncoderClk, uint8_t volumeEncoderDt);
  void run();
  void setViews(void (*scene)(bool), void (*info)(const String &, const String &)) { scene_ = scene; info_ = info; }

private:
  static constexpr uint8_t MAX_TRACKS = 128;
  GameDisplay &display_;
  uint8_t buttonLeft_;
  uint8_t buttonSelect_;
  uint8_t buttonRight_;
  uint8_t volumeEncoderClk_;
  uint8_t volumeEncoderDt_;
  String tracks_[MAX_TRACKS];
  uint8_t trackCount_;
  uint8_t selectedTrack_;
  uint8_t encoderPreviousState_;
  int8_t encoderTransitionCount_;
  bool spiStarted_;
  bool sdReady_;
  bool libraryLimited_ = false;
  bool browser_ = false;
  bool folders_ = false;
  bool bluetoothMenu_ = false;
  bool playbackClockRunning_ = false;
  bool durationApproximate_ = true;
  uint16_t browseIndex_ = 0;
  MusicQueue queue_;
  AudioFileSourceBuffer *buffer_ = nullptr;
  uint8_t audioBuffer_[8192];
  void rebuildQueue();
  uint16_t folderCount() const;
  String folderAt(uint16_t index) const;
  void drawBrowser() const;
  float gain_;
  String status_;
  uint32_t durationSeconds_ = 0;
  uint32_t playbackElapsedMs_ = 0;
  uint32_t playbackSegmentStartMs_ = 0;
  AudioFileSourceSD *source_;
  AudioGeneratorMP3 *decoder_;
  AudioOutputI2S *output_;
  AudioOutput *activeOutput_;
  BluetoothAudioOutput bluetoothOutput_;

  void (*scene_)(bool) = nullptr;
  void (*info_)(const String &, const String &) = nullptr;
  bool paused_ = false, shuffle_ = false, options_ = false;
  bool bluetoothEnabled_ = false;
  uint8_t repeat_ = 0, option_ = 0, bluetoothOption_ = 0;
  String folder_;
  uint8_t folderTracks_[MAX_TRACKS];
  uint16_t folderCount_ = 0;
  void scanFolder(const String &path, uint8_t depth);
  bool advance(int8_t direction, bool wrap = true);
  bool initializeCard();
  bool scanMusicDirectory();
  void startPlayback();
  void stopPlayback(const char *status);
  bool isPlaying() const;
  void updateGain();
  void updatePlaybackClock(uint32_t now);
  uint32_t currentElapsedSeconds() const;
  void drawPlayerScreen() const;
  void waitForButtonsReleased() const;
  void moveSelection(int8_t direction);
  static bool isMp3Path(const String &path);
  String trackName(uint8_t index) const;
};

