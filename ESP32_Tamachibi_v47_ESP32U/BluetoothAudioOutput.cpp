#include "BluetoothAudioOutput.h"

#include <cstring>

BluetoothAudioOutput *BluetoothAudioOutput::active_ = nullptr;

BluetoothAudioOutput::BluetoothAudioOutput() {
  hertz = static_cast<uint16_t>(kBluetoothSampleRate);
  channels = 2;
  gainF2P6 = 1U << 6;
}

BluetoothAudioOutput::~BluetoothAudioOutput() {
  stop();
  if (bluetoothStarted_) {
    if (source_.is_discovery_active()) source_.cancel_discovery();
  }
  if (active_ == this) active_ = nullptr;
}

bool BluetoothAudioOutput::begin() {
  if (active_ && active_ != this) return false;
  active_ = this;
  pairingEnabled_ = true;
  clearPcm(true);
  havePrevious_ = false;
  resamplePhase_ = static_cast<uint32_t>(inputRate_);

  if (!bluetoothStarted_) {
    source_.set_local_name("Tamachibi Audio");
    // Include telephony-class headsets such as some true-wireless earbuds in
    // addition to devices advertising only the audio/rendering service.
    source_.set_valid_cod_service(ESP_BT_COD_SRVC_RENDERING |
                                  ESP_BT_COD_SRVC_AUDIO |
                                  ESP_BT_COD_SRVC_TELEPHONY);
    source_.set_ssp_enabled(true);
    source_.set_auto_reconnect(true, 3);
    source_.set_ssid_callback(selectAudioSink);
    source_.set_discovery_mode_callback(discoveryChanged);
    source_.set_on_connection_state_changed(connectionChanged, this);
    source_.set_data_callback(provideAudio);
    clearDeviceName();
    discoveryStarted_ = false;
    scanning_ = true;
    source_.start();
    bluetoothStarted_ = true;
  } else if (!source_.is_connected() && !source_.is_discovery_active() &&
             !(scanning_ && !discoveryStarted_) &&
             source_.get_connection_state() ==
                 ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
    // A deliberate disconnect disables the library's automatic retry. Reuse
    // the remembered headset when Bluetooth output is selected again.
    const bool reconnectStarted = source_.reconnect();
    source_.set_auto_reconnect(true, 3);
    if (!reconnectStarted && discoveryStarted_) return restartSource();
  }
  return true;
}

bool BluetoothAudioOutput::startPairing() {
  pairingEnabled_ = true;
  if (!bluetoothStarted_) {
    clearDeviceName();
    return begin();
  }

  const esp_a2d_connection_state_t state = source_.get_connection_state();
  if (state != ESP_A2D_CONNECTION_STATE_DISCONNECTED ||
      source_.is_discovery_active() || (scanning_ && !discoveryStarted_))
    return true;

  return restartSource();
}

bool BluetoothAudioOutput::restartSource() {
  // The library has no public "start discovery again" call. End the idle,
  // disconnected profile and start it again so PAIR / SEARCH really scans
  // instead of silently retrying only the previously remembered address.
  source_.cancel_discovery();
  source_.end(false);
  bluetoothStarted_ = false;
  connected_ = false;
  scanning_ = false;
  discoveryStarted_ = false;
  connectionState_ = ESP_A2D_CONNECTION_STATE_DISCONNECTED;
  clearDeviceName();
  return begin();
}

bool BluetoothAudioOutput::SetRate(int hz) {
  // MP3 supports rates up to 48 kHz. Resample those to A2DP's 44.1 kHz PCM.
  if (hz < 8000 || hz > 48000) return false;
  if (inputRate_ != hz) {
    inputRate_ = hz;
    havePrevious_ = false;
    resamplePhase_ = static_cast<uint32_t>(inputRate_);
  }
  hertz = static_cast<uint16_t>(hz);
  return true;
}

bool BluetoothAudioOutput::SetChannels(int channelCount) {
  if (channelCount < 1 || channelCount > 2) return false;
  channels = static_cast<uint8_t>(channelCount);
  return true;
}

bool BluetoothAudioOutput::SetGain(float gain) {
  const float clippedGain = gain < 0.0f ? 0.0f : gain > 0.9f ? 0.9f : gain;
  bluetoothVolume_ = static_cast<uint8_t>(
      clippedGain * (127.0f / 0.9f) + 0.5f);
  if (source_.is_connected()) source_.set_volume(bluetoothVolume_);
  return AudioOutput::SetGain(gain);
}

bool BluetoothAudioOutput::ConsumeSample(int16_t sample[2]) {
  if (!sample) return false;
  MakeSampleStereo16(sample);

  int16_t generated[8][2];
  uint8_t generatedFrames = 0;
  uint32_t nextPhase = resamplePhase_;

  if (!havePrevious_) {
    generated[0][0] = Amplify(sample[0]);
    generated[0][1] = Amplify(sample[1]);
    generatedFrames = 1;
    nextPhase = static_cast<uint32_t>(inputRate_);
  } else {
    while (nextPhase <= kBluetoothSampleRate && generatedFrames < 8) {
      const int32_t fraction = static_cast<int32_t>(nextPhase);
      const int32_t left = previousLeft_ +
          ((static_cast<int32_t>(sample[0]) - previousLeft_) * fraction) /
              static_cast<int32_t>(kBluetoothSampleRate);
      const int32_t right = previousRight_ +
          ((static_cast<int32_t>(sample[1]) - previousRight_) * fraction) /
              static_cast<int32_t>(kBluetoothSampleRate);
      generated[generatedFrames][0] = Amplify(static_cast<int16_t>(left));
      generated[generatedFrames][1] = Amplify(static_cast<int16_t>(right));
      ++generatedFrames;
      nextPhase += static_cast<uint32_t>(inputRate_);
    }
    nextPhase -= kBluetoothSampleRate;
  }

  bool accepted = false;
  portENTER_CRITICAL(&mux_);
  const uint32_t buffered = writeFrame_ - readFrame_;
  const uint32_t freeFrames = buffered <= kRingFrames
                                  ? kRingFrames - buffered
                                  : 0;
  if (feedEnabled_ && freeFrames >= generatedFrames) {
    for (uint8_t i = 0; i < generatedFrames; ++i) {
      const uint32_t slot = writeFrame_ & kRingMask;
      pcmRing_[slot * 2] = generated[i][0];
      pcmRing_[slot * 2 + 1] = generated[i][1];
      ++writeFrame_;
    }
    accepted = true;
  }
  portEXIT_CRITICAL(&mux_);

  // Returning false applies backpressure without losing the MP3 decoder's
  // pending sample. It also leaves the player controls responsive while the
  // headphones are still connecting.
  if (!accepted) return false;

  previousLeft_ = sample[0];
  previousRight_ = sample[1];
  havePrevious_ = true;
  resamplePhase_ = nextPhase;
  return true;
}

bool BluetoothAudioOutput::stop() {
  clearPcm(false);
  havePrevious_ = false;
  resamplePhase_ = static_cast<uint32_t>(inputRate_);
  return true;
}

void BluetoothAudioOutput::disconnect() {
  pairingEnabled_ = false;
  stop();
  if (bluetoothStarted_ &&
      source_.get_connection_state() !=
          ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
    connectionState_ = ESP_A2D_CONNECTION_STATE_DISCONNECTING;
    // Do not wait for the Bluetooth stack while the game controls are running.
    source_.disconnect();
  }
}

const char *BluetoothAudioOutput::connectionStatus() const {
  if (!bluetoothStarted_) return "NOT STARTED";
  if (pairingEnabled_ && scanning_ && !discoveryStarted_)
    return "STARTING";
  switch (connectionState_) {
    case ESP_A2D_CONNECTION_STATE_CONNECTED:
      return "CONNECTED";
    case ESP_A2D_CONNECTION_STATE_CONNECTING:
      return "CONNECTING";
    case ESP_A2D_CONNECTION_STATE_DISCONNECTING:
      return "DISCONNECTING";
    default:
      if (pairingEnabled_ && scanning_) return "SEARCHING";
      return "DISCONNECTED";
  }
}

void BluetoothAudioOutput::copyDeviceName(char *destination,
                                          size_t capacity) const {
  if (!destination || capacity == 0) return;
  portENTER_CRITICAL(&mux_);
  strncpy(destination, deviceName_, capacity - 1);
  destination[capacity - 1] = '\0';
  portEXIT_CRITICAL(&mux_);
}

void BluetoothAudioOutput::service() {
  if (!volumeSyncPending_ || !connected_ || !source_.is_connected()) return;
  volumeSyncPending_ = false;

  // The library skips identical volume values. Nudge its cached value so the
  // desired level is sent again after a headset reconnects.
  const uint8_t currentVolume = bluetoothVolume_;
  source_.set_volume(currentVolume == 127 ? 126 : currentVolume + 1);
  source_.set_volume(currentVolume);
}

int32_t BluetoothAudioOutput::provideAudio(uint8_t *data, int32_t byteCount) {
  if (!data || byteCount <= 0) return 0;
  if (active_) return active_->fillAudio(data, byteCount);
  memset(data, 0, static_cast<size_t>(byteCount));
  return byteCount;
}

bool BluetoothAudioOutput::selectAudioSink(const char *name,
                                           esp_bd_addr_t address, int rssi) {
  (void)address;
  (void)rssi;
  // ESP32-A2DP already filters scan results to audio renderers. Accept the
  // first named sink found while the intended headphones are in pairing mode.
  if (!active_ || !active_->pairingEnabled_ || !name || name[0] == '\0')
    return false;
  portENTER_CRITICAL(&active_->mux_);
  strncpy(active_->deviceName_, name, sizeof(active_->deviceName_) - 1);
  active_->deviceName_[sizeof(active_->deviceName_) - 1] = '\0';
  portEXIT_CRITICAL(&active_->mux_);
  return true;
}

void BluetoothAudioOutput::connectionChanged(esp_a2d_connection_state_t state,
                                             void *context) {
  auto *output = static_cast<BluetoothAudioOutput *>(context);
  if (output) {
    output->connectionState_ = state;
    output->connected_ = state == ESP_A2D_CONNECTION_STATE_CONNECTED;
    output->volumeSyncPending_ = output->connected_;
  }
}

void BluetoothAudioOutput::discoveryChanged(
    esp_bt_gap_discovery_state_t state) {
  if (active_) {
    active_->scanning_ = state == ESP_BT_GAP_DISCOVERY_STARTED;
    if (state == ESP_BT_GAP_DISCOVERY_STARTED)
      active_->discoveryStarted_ = true;
  }
}

int32_t BluetoothAudioOutput::fillAudio(uint8_t *data, int32_t byteCount) {
  memset(data, 0, static_cast<size_t>(byteCount));
  const int32_t alignedBytes = byteCount - (byteCount % 4);

  portENTER_CRITICAL(&mux_);
  for (int32_t offset = 0; offset < alignedBytes; offset += 4) {
    int16_t left = 0;
    int16_t right = 0;
    if (feedEnabled_ && connected_ && readFrame_ != writeFrame_) {
      const uint32_t slot = readFrame_ & kRingMask;
      left = pcmRing_[slot * 2];
      right = pcmRing_[slot * 2 + 1];
      ++readFrame_;
    }

    const uint16_t leftBits = static_cast<uint16_t>(left);
    const uint16_t rightBits = static_cast<uint16_t>(right);
    data[offset] = static_cast<uint8_t>(leftBits & 0xffU);
    data[offset + 1] = static_cast<uint8_t>(leftBits >> 8);
    data[offset + 2] = static_cast<uint8_t>(rightBits & 0xffU);
    data[offset + 3] = static_cast<uint8_t>(rightBits >> 8);
  }
  portEXIT_CRITICAL(&mux_);
  return byteCount;
}

void BluetoothAudioOutput::clearPcm(bool enableFeed) {
  portENTER_CRITICAL(&mux_);
  readFrame_ = 0;
  writeFrame_ = 0;
  feedEnabled_ = enableFeed;
  portEXIT_CRITICAL(&mux_);
}

void BluetoothAudioOutput::clearDeviceName() {
  portENTER_CRITICAL(&mux_);
  deviceName_[0] = '\0';
  portEXIT_CRITICAL(&mux_);
}
