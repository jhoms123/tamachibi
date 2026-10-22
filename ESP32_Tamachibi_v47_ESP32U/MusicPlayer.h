#pragma once

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>

class Esp32MusicPlayer {
public:
  Esp32MusicPlayer(Adafruit_SSD1306 &display, uint8_t buttonLeft,
                   uint8_t buttonSelect, uint8_t buttonRight,
                   uint8_t volumeEncoderClk, uint8_t volumeEncoderDt);
  void run();

private:
  static constexpr uint8_t MAX_TRACKS = 32;
  Adafruit_SSD1306 &display_;
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
  bool playbackFinished_;
  float gain_;
  String status_;
  AudioFileSourceSD *source_;
  AudioGeneratorMP3 *decoder_;
  AudioOutputI2S *output_;

  bool initializeCard();
  bool scanMusicDirectory();
  void startPlayback();
  void stopPlayback(const char *status);
  bool isPlaying() const;
  void updateGain();
  void drawPlayerScreen() const;
  void waitForButtonsReleased() const;
  void moveSelection(int8_t direction);
  static bool isMp3Path(const String &path);
  String trackName(uint8_t index) const;
};
