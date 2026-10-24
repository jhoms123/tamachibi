#pragma once

#include <AudioOutput.h>
#include <BluetoothA2DPSource.h>
#include <freertos/FreeRTOS.h>
#include <stdint.h>

// Bridges the ESP8266Audio MP3 decoder to the ESP32's Bluetooth A2DP source.
// The decoder pushes samples into a small ring; the Bluetooth stack pulls
// 44.1 kHz stereo PCM from it on its own task.
class BluetoothAudioOutput final : public AudioOutput {
public:
  BluetoothAudioOutput();
  ~BluetoothAudioOutput() override;

  bool begin() override;
  bool SetRate(int hz) override;
  bool SetChannels(int channels) override;
  bool SetGain(float gain) override;
  bool ConsumeSample(int16_t sample[2]) override;
  bool stop() override;

  bool isConnected() const { return connected_; }

private:
  static constexpr uint32_t kBluetoothSampleRate = 44100UL;
  static constexpr uint32_t kRingFrames = 4096UL;
  static constexpr uint32_t kRingMask = kRingFrames - 1;

  static BluetoothAudioOutput *active_;

  BluetoothA2DPSource source_;
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  int16_t pcmRing_[kRingFrames * 2] = {};
  volatile uint32_t readFrame_ = 0;
  volatile uint32_t writeFrame_ = 0;
  volatile bool feedEnabled_ = false;
  volatile bool connected_ = false;
  bool bluetoothStarted_ = false;
  bool havePrevious_ = false;
  int inputRate_ = static_cast<int>(kBluetoothSampleRate);
  uint32_t resamplePhase_ = kBluetoothSampleRate;
  int16_t previousLeft_ = 0;
  int16_t previousRight_ = 0;

  static int32_t provideAudio(uint8_t *data, int32_t byteCount);
  static bool selectAudioSink(const char *name, esp_bd_addr_t address,
                              int rssi);
  static void connectionChanged(esp_a2d_connection_state_t state,
                                void *context);

  int32_t fillAudio(uint8_t *data, int32_t byteCount);
  void clearPcm(bool enableFeed);
};
