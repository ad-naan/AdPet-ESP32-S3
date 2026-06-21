#include "VoiceManager.h"
#include "../../core/AppConfig.h"
#include "../../core/ConfigManager.h"
#include <driver/i2s.h>
#include <math.h>

#ifndef I2S_COMM_FORMAT_STAND_I2S
#define ADPET_I2S_COMM_FORMAT I2S_COMM_FORMAT_I2S
#else
#define ADPET_I2S_COMM_FORMAT I2S_COMM_FORMAT_STAND_I2S
#endif

namespace {
  const i2s_port_t MIC_I2S_PORT = I2S_NUM_0;
  const i2s_port_t SPEAKER_I2S_PORT = I2S_NUM_1;
  const size_t MIC_SAMPLE_COUNT = 128;
  const size_t TONE_SAMPLE_COUNT = 256;
  const unsigned long METER_INTERVAL_MS = 500;
  const unsigned long NOTE_DURATION_MS = 120;
  const float ADPET_TWO_PI = 6.28318530718f;
  const uint16_t REACTION_MELODY[] = { 784, 988, 1175, 0, 1175 };
  const uint8_t REACTION_MELODY_LENGTH = sizeof(REACTION_MELODY) / sizeof(REACTION_MELODY[0]);

  void writeLe16(uint8_t* p, uint16_t v) {
    p[0] = v & 0xff;
    p[1] = (v >> 8) & 0xff;
  }

  void writeLe32(uint8_t* p, uint32_t v) {
    p[0] = v & 0xff;
    p[1] = (v >> 8) & 0xff;
    p[2] = (v >> 16) & 0xff;
    p[3] = (v >> 24) & 0xff;
  }
}

void VoiceManager::begin() {
  if (!AppConfig::Feature::VOICE_ENABLED && !AppConfig::Feature::AUDIO_TEST_MODE) {
    Serial.println("[Voice] disabled");
    return;
  }

  beginAudio();
}

void VoiceManager::update() {
  if (!_audioReady || _recordingInProgress) {
    return;
  }

  unsigned long nowMs = millis();
  if (nowMs - _lastStatusPrintMs >= 3000) {
    _lastStatusPrintMs = nowMs;
    Serial.println("[Voice] listening");
  }

  if (_micReady && nowMs - _lastMeterPrintMs >= METER_INTERVAL_MS) {
    _lastMeterPrintMs = nowMs;
    uint32_t rms = 0;
    uint32_t peak = 0;
    if (readMicLevel(rms, peak)) {
      Serial.print("[Voice] mic rms=");
      Serial.print(rms);
      Serial.print(" peak=");
      Serial.println(peak);

      const RuntimeConfig& cfg = AppConfigStore.get();
      bool loudEnough = rms >= cfg.triggerRms || peak >= cfg.triggerPeak;
      if (loudEnough && nowMs - _lastSoundTriggerMs >= AppConfig::Audio::SOUND_TRIGGER_COOLDOWN_MS) {
        _lastSoundTriggerMs = nowMs;
        _hasSpeech = true;
        _lastText = "voice detected";
        playMelody();
        recordWav(cfg.recordMs);
      }
    }
  }

  updateMelody(nowMs);
}

bool VoiceManager::hasUserSpeech() const {
  return _hasSpeech;
}

String VoiceManager::takeUserText() {
  _hasSpeech = false;
  String text = _lastText;
  _lastText = "";
  return text;
}

bool VoiceManager::hasRecording() const {
  return _hasRecording;
}

bool VoiceManager::takeRecording(uint8_t*& wavData, size_t& wavSize) {
  if (!_hasRecording || _recording == nullptr || _recordingSize == 0) {
    wavData = nullptr;
    wavSize = 0;
    return false;
  }

  wavData = _recording;
  wavSize = _recordingSize;
  _hasRecording = false;
  _hasSpeech = false;
  _lastText = "";
  _recording = nullptr;
  _recordingSize = 0;
  return true;
}

void VoiceManager::freeRecording() {
  if (_recording != nullptr) {
    free(_recording);
    _recording = nullptr;
  }
  _recordingSize = 0;
  _hasRecording = false;
}

void VoiceManager::beginAudio() {
  Serial.println("[Voice] starting audio");
  bool ready = false;
  ready = beginMic() || ready;
  ready = beginSpeaker() || ready;
  _audioReady = ready;
  _lastMeterPrintMs = millis();
  _lastStatusPrintMs = millis();

  if (_audioReady) {
    Serial.println("[Voice] audio ready");
    playMelody();
  } else {
    Serial.println("[Voice] audio init failed");
  }
}

bool VoiceManager::beginMic() {
  i2s_config_t i2sConfig = {};
  i2sConfig.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  i2sConfig.sample_rate = AppConfig::Audio::SAMPLE_RATE;
  i2sConfig.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  i2sConfig.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  i2sConfig.communication_format = (i2s_comm_format_t)ADPET_I2S_COMM_FORMAT;
  i2sConfig.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  i2sConfig.dma_buf_count = 4;
  i2sConfig.dma_buf_len = 256;
  i2sConfig.use_apll = false;
  i2sConfig.tx_desc_auto_clear = false;
  i2sConfig.fixed_mclk = 0;

  esp_err_t result = i2s_driver_install(MIC_I2S_PORT, &i2sConfig, 0, NULL);
  if (result != ESP_OK) {
    Serial.print("[Voice] mic i2s_driver_install failed: ");
    Serial.println((int)result);
    return false;
  }

  i2s_pin_config_t pinConfig = {};
  pinConfig.mck_io_num = I2S_PIN_NO_CHANGE;
  pinConfig.bck_io_num = AppConfig::Audio::MIC_BCLK_PIN;
  pinConfig.ws_io_num = AppConfig::Audio::MIC_LRCLK_PIN;
  pinConfig.data_out_num = I2S_PIN_NO_CHANGE;
  pinConfig.data_in_num = AppConfig::Audio::MIC_DATA_PIN;

  result = i2s_set_pin(MIC_I2S_PORT, &pinConfig);
  if (result != ESP_OK) {
    Serial.print("[Voice] mic i2s_set_pin failed: ");
    Serial.println((int)result);
    i2s_driver_uninstall(MIC_I2S_PORT);
    return false;
  }

  _micReady = true;
  Serial.println("[Voice] mic ready");
  return true;
}

bool VoiceManager::beginSpeaker() {
  i2s_config_t i2sConfig = {};
  i2sConfig.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  i2sConfig.sample_rate = AppConfig::Audio::SAMPLE_RATE;
  i2sConfig.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  i2sConfig.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  i2sConfig.communication_format = (i2s_comm_format_t)ADPET_I2S_COMM_FORMAT;
  i2sConfig.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  i2sConfig.dma_buf_count = 4;
  i2sConfig.dma_buf_len = 256;
  i2sConfig.use_apll = false;
  i2sConfig.tx_desc_auto_clear = true;
  i2sConfig.fixed_mclk = 0;

  esp_err_t result = i2s_driver_install(SPEAKER_I2S_PORT, &i2sConfig, 0, NULL);
  if (result != ESP_OK) {
    Serial.print("[Voice] speaker i2s_driver_install failed: ");
    Serial.println((int)result);
    return false;
  }

  i2s_pin_config_t pinConfig = {};
  pinConfig.mck_io_num = I2S_PIN_NO_CHANGE;
  pinConfig.bck_io_num = AppConfig::Audio::SPEAKER_BCLK_PIN;
  pinConfig.ws_io_num = AppConfig::Audio::SPEAKER_LRCLK_PIN;
  pinConfig.data_out_num = AppConfig::Audio::SPEAKER_DATA_PIN;
  pinConfig.data_in_num = I2S_PIN_NO_CHANGE;

  result = i2s_set_pin(SPEAKER_I2S_PORT, &pinConfig);
  if (result != ESP_OK) {
    Serial.print("[Voice] speaker i2s_set_pin failed: ");
    Serial.println((int)result);
    i2s_driver_uninstall(SPEAKER_I2S_PORT);
    return false;
  }

  i2s_zero_dma_buffer(SPEAKER_I2S_PORT);
  _speakerReady = true;
  Serial.println("[Voice] speaker ready");
  return true;
}

bool VoiceManager::readMicLevel(uint32_t& rms, uint32_t& peak) {
  int32_t samples[MIC_SAMPLE_COUNT];
  size_t bytesRead = 0;
  esp_err_t result = i2s_read(MIC_I2S_PORT, samples, sizeof(samples), &bytesRead, 10 / portTICK_PERIOD_MS);
  if (result != ESP_OK || bytesRead == 0) {
    Serial.println("[Voice] mic read failed");
    return false;
  }

  size_t count = bytesRead / sizeof(samples[0]);
  uint64_t sumSquares = 0;
  uint32_t maxPeak = 0;
  for (size_t i = 0; i < count; i++) {
    int32_t sample = samples[i] >> 14;
    uint32_t magnitude = abs(sample);
    if (magnitude > maxPeak) maxPeak = magnitude;
    sumSquares += (uint64_t)magnitude * (uint64_t)magnitude;
  }

  rms = (uint32_t)sqrt((double)sumSquares / (double)count);
  peak = maxPeak;
  return true;
}

bool VoiceManager::recordWav(uint16_t durationMs) {
  if (!_micReady) {
    return false;
  }

  _recordingInProgress = true;
  freeRecording();

  uint32_t sampleCount = ((uint32_t)AppConfig::Audio::SAMPLE_RATE * durationMs) / 1000;
  uint32_t pcmBytes = sampleCount * 2;
  _recordingSize = 44 + pcmBytes;
  _recording = (uint8_t*)malloc(_recordingSize);
  if (_recording == nullptr) {
    Serial.println("[Voice] record malloc failed");
    _recordingSize = 0;
    _recordingInProgress = false;
    return false;
  }

  writeWavHeader(_recording, pcmBytes);
  Serial.print("[Voice] recording ms=");
  Serial.println(durationMs);

  uint32_t writtenSamples = 0;
  int32_t raw[64];
  while (writtenSamples < sampleCount) {
    size_t bytesRead = 0;
    i2s_read(MIC_I2S_PORT, raw, sizeof(raw), &bytesRead, portMAX_DELAY);
    size_t count = bytesRead / sizeof(raw[0]);
    for (size_t i = 0; i < count && writtenSamples < sampleCount; i++) {
      int16_t pcm = (int16_t)(raw[i] >> 16);
      size_t offset = 44 + writtenSamples * 2;
      _recording[offset] = pcm & 0xff;
      _recording[offset + 1] = (pcm >> 8) & 0xff;
      writtenSamples++;
    }
  }

  _hasRecording = true;
  _recordingInProgress = false;
  Serial.print("[Voice] recording ready bytes=");
  Serial.println(_recordingSize);
  return true;
}

void VoiceManager::writeWavHeader(uint8_t* buffer, uint32_t pcmBytes) {
  memcpy(buffer, "RIFF", 4);
  writeLe32(buffer + 4, 36 + pcmBytes);
  memcpy(buffer + 8, "WAVEfmt ", 8);
  writeLe32(buffer + 16, 16);
  writeLe16(buffer + 20, 1);
  writeLe16(buffer + 22, 1);
  writeLe32(buffer + 24, AppConfig::Audio::SAMPLE_RATE);
  writeLe32(buffer + 28, AppConfig::Audio::SAMPLE_RATE * 2);
  writeLe16(buffer + 32, 2);
  writeLe16(buffer + 34, 16);
  memcpy(buffer + 36, "data", 4);
  writeLe32(buffer + 40, pcmBytes);
}

void VoiceManager::playMelody() {
  if (!_speakerReady) return;
  _melodyPlaying = true;
  _melodyNoteIndex = 0;
  _currentToneHz = REACTION_MELODY[0];
  _tonePhase = 0.0f;
  _beepStartMs = millis();
  Serial.println("[Voice] melody");
}

void VoiceManager::updateMelody(unsigned long nowMs) {
  if (!_speakerReady || !_melodyPlaying) return;

  if (nowMs - _beepStartMs >= NOTE_DURATION_MS) {
    _melodyNoteIndex++;
    _beepStartMs = nowMs;
    if (_melodyNoteIndex >= REACTION_MELODY_LENGTH) {
      _melodyPlaying = false;
      _currentToneHz = 0.0f;
      return;
    }
    _currentToneHz = REACTION_MELODY[_melodyNoteIndex];
    _tonePhase = 0.0f;
  }

  playTestToneChunk();
}

void VoiceManager::playTestToneChunk() {
  int16_t samples[TONE_SAMPLE_COUNT];
  const float phaseStep = ADPET_TWO_PI * _currentToneHz / (float)AppConfig::Audio::SAMPLE_RATE;
  const float amplitude = 12000.0f;

  for (size_t i = 0; i < TONE_SAMPLE_COUNT; i++) {
    if (_currentToneHz <= 0.0f) {
      samples[i] = 0;
    } else {
      samples[i] = (int16_t)(sinf(_tonePhase) * amplitude);
      _tonePhase += phaseStep;
      if (_tonePhase >= ADPET_TWO_PI) _tonePhase -= ADPET_TWO_PI;
    }
  }

  size_t bytesWritten = 0;
  i2s_write(SPEAKER_I2S_PORT, samples, sizeof(samples), &bytesWritten, 10 / portTICK_PERIOD_MS);
}

bool VoiceManager::playWav(const uint8_t* wavData, size_t wavSize) {
  if (!_speakerReady || wavData == nullptr || wavSize <= 44) {
    return false;
  }

  uint32_t sampleRate = wavData[24]
    | ((uint32_t)wavData[25] << 8)
    | ((uint32_t)wavData[26] << 16)
    | ((uint32_t)wavData[27] << 24);
  uint16_t bitsPerSample = wavData[34] | ((uint16_t)wavData[35] << 8);

  size_t dataOffset = 0;
  size_t pcmBytes = 0;
  for (size_t i = 12; i + 8 < wavSize; ) {
    uint32_t chunkSize = wavData[i + 4]
      | ((uint32_t)wavData[i + 5] << 8)
      | ((uint32_t)wavData[i + 6] << 16)
      | ((uint32_t)wavData[i + 7] << 24);

    if (memcmp(wavData + i, "data", 4) == 0) {
      dataOffset = i + 8;
      pcmBytes = min((size_t)chunkSize, wavSize - dataOffset);
      break;
    }

    i += 8 + chunkSize;
    if (chunkSize & 1) i++;
  }

  if (dataOffset == 0 || pcmBytes == 0 || bitsPerSample != 16 || sampleRate == 0) {
    Serial.println("[Voice] unsupported wav");
    return false;
  }

  i2s_set_clk(SPEAKER_I2S_PORT, sampleRate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
  const uint8_t* pcm = wavData + dataOffset;
  size_t written = 0;
  Serial.print("[Voice] play wav bytes=");
  Serial.println(pcmBytes);
  bool ok = i2s_write(SPEAKER_I2S_PORT, pcm, pcmBytes, &written, portMAX_DELAY) == ESP_OK;
  i2s_set_clk(SPEAKER_I2S_PORT, AppConfig::Audio::SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
  return ok;
}
