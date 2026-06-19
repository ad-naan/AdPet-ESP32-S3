#include "VoiceManager.h"
#include "../../core/AppConfig.h"
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
  const unsigned long BEEP_DURATION_MS = 180;
  const float ADPET_TWO_PI = 6.28318530718f;
}

void VoiceManager::begin() {
  if (AppConfig::Feature::AUDIO_TEST_MODE) {
    beginAudioTest();
    return;
  }

  if (!AppConfig::Feature::VOICE_ENABLED) {
    Serial.println("[Voice] disabled");
    return;
  }

  Serial.println("[Voice] TODO: mic, wake word, STT, speaker/TTS");
}

void VoiceManager::update() {
  if (AppConfig::Feature::AUDIO_TEST_MODE) {
    updateAudioTest();
  }
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

void VoiceManager::beginAudioTest() {
  Serial.println("[AudioTest] starting INMP441 + MAX98357A test");
  Serial.println("[AudioTest] mic wiring: BCLK=GPIO4, LRCLK=GPIO5, DATA=GPIO6");
  Serial.println("[AudioTest] speaker wiring: BCLK=GPIO10, LRCLK=GPIO11, DATA=GPIO12");

  if (AppConfig::Feature::AUDIO_TEST_MIC) {
    Serial.println("[AudioTest] mic test enabled: serial output shows RMS/Peak");
  }

  if (AppConfig::Feature::AUDIO_TEST_SPEAKER) {
    Serial.println("[AudioTest] speaker test enabled: sound trigger plays a beep");
  }

  if (!AppConfig::Feature::AUDIO_TEST_MIC && !AppConfig::Feature::AUDIO_TEST_SPEAKER) {
    Serial.println("[AudioTest] no test target enabled");
    return;
  }

  bool ready = false;
  if (AppConfig::Feature::AUDIO_TEST_MIC) {
    ready = beginMicTest() || ready;
  }
  if (AppConfig::Feature::AUDIO_TEST_SPEAKER) {
    ready = beginSpeakerTest() || ready;
  }

  if (!ready) {
    Serial.println("[AudioTest] no audio device initialized");
    return;
  }

  _audioReady = true;
  _lastMeterPrintMs = millis();
  _lastBeepMs = millis();
  _lastStatusPrintMs = millis();
  Serial.println("[AudioTest] ready");

  if (_speakerReady) {
    startBeep();
  }
}

bool VoiceManager::beginMicTest() {
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
    Serial.print("[AudioTest] mic i2s_driver_install failed: ");
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
    Serial.print("[AudioTest] mic i2s_set_pin failed: ");
    Serial.println((int)result);
    i2s_driver_uninstall(MIC_I2S_PORT);
    return false;
  }

  _micReady = true;
  Serial.println("[AudioTest] mic ready");
  return true;
}

bool VoiceManager::beginSpeakerTest() {
  i2s_config_t i2sConfig = {};
  i2sConfig.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  i2sConfig.sample_rate = AppConfig::Audio::SAMPLE_RATE;
  i2sConfig.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
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
    Serial.print("[AudioTest] speaker i2s_driver_install failed: ");
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
    Serial.print("[AudioTest] speaker i2s_set_pin failed: ");
    Serial.println((int)result);
    i2s_driver_uninstall(SPEAKER_I2S_PORT);
    return false;
  }

  i2s_zero_dma_buffer(SPEAKER_I2S_PORT);
  _speakerReady = true;
  Serial.println("[AudioTest] speaker ready");
  return true;
}

void VoiceManager::updateAudioTest() {
  unsigned long nowMs = millis();

  if (!_audioReady) {
    if (nowMs - _lastStatusPrintMs >= 2000) {
      _lastStatusPrintMs = nowMs;
      Serial.println("[AudioTest] not ready, check earlier init error and wiring");
    }
    return;
  }

  if (nowMs - _lastStatusPrintMs >= 2000) {
    _lastStatusPrintMs = nowMs;
    Serial.println("[AudioTest] running");
  }

  if (_micReady && nowMs - _lastMeterPrintMs >= METER_INTERVAL_MS) {
    _lastMeterPrintMs = nowMs;
    uint32_t rms = 0;
    uint32_t peak = 0;
    if (readMicLevel(rms, peak)) {
      Serial.print("[AudioTest] mic rms=");
      Serial.print(rms);
      Serial.print(" peak=");
      Serial.println(peak);

      bool loudEnough = rms >= AppConfig::Audio::SOUND_TRIGGER_RMS
        || peak >= AppConfig::Audio::SOUND_TRIGGER_PEAK;

      if (loudEnough && nowMs - _lastSoundTriggerMs >= AppConfig::Audio::SOUND_TRIGGER_COOLDOWN_MS) {
        _lastSoundTriggerMs = nowMs;
        _hasSpeech = true;
        _lastText = "sound trigger";
        Serial.println("[AudioTest] sound trigger -> display reaction + beep");
        startBeep();
      }
    }
  }

  if (_speakerReady && _beeping) {
    if (nowMs - _beepStartMs >= BEEP_DURATION_MS) {
      _beeping = false;
      return;
    }

    playTestToneChunk();
  }
}

bool VoiceManager::readMicLevel(uint32_t& rms, uint32_t& peak) {
  int32_t samples[MIC_SAMPLE_COUNT];
  size_t bytesRead = 0;
  esp_err_t result = i2s_read(MIC_I2S_PORT, samples, sizeof(samples), &bytesRead, 10 / portTICK_PERIOD_MS);

  if (result != ESP_OK || bytesRead == 0) {
    Serial.println("[AudioTest] mic read failed");
    return false;
  }

  size_t count = bytesRead / sizeof(samples[0]);
  uint64_t sumSquares = 0;
  uint32_t maxPeak = 0;

  for (size_t i = 0; i < count; i++) {
    int32_t sample = samples[i] >> 14;
    uint32_t magnitude = abs(sample);
    if (magnitude > maxPeak) {
      maxPeak = magnitude;
    }
    sumSquares += (uint64_t)magnitude * (uint64_t)magnitude;
  }

  rms = (uint32_t)sqrt((double)sumSquares / (double)count);
  peak = maxPeak;
  return true;
}

void VoiceManager::startBeep() {
  if (!_speakerReady) {
    return;
  }

  _beeping = true;
  _beepStartMs = millis();
  _lastBeepMs = _beepStartMs;
  Serial.println("[AudioTest] beep");
}

void VoiceManager::playTestToneChunk() {
  int32_t samples[TONE_SAMPLE_COUNT];
  const float phaseStep = ADPET_TWO_PI * (float)AppConfig::Audio::TEST_TONE_HZ / (float)AppConfig::Audio::SAMPLE_RATE;
  const float amplitude = 120000000.0f;

  for (size_t i = 0; i < TONE_SAMPLE_COUNT; i++) {
    samples[i] = (int32_t)(sinf(_tonePhase) * amplitude);
    _tonePhase += phaseStep;
    if (_tonePhase >= ADPET_TWO_PI) {
      _tonePhase -= ADPET_TWO_PI;
    }
  }

  size_t bytesWritten = 0;
  i2s_write(SPEAKER_I2S_PORT, samples, sizeof(samples), &bytesWritten, 10 / portTICK_PERIOD_MS);
}
