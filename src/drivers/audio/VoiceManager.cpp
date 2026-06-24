#include "VoiceManager.h"
#include "../../core/AppConfig.h"
#include "../../core/ConfigManager.h"
#include <driver/i2s.h>
#include <math.h>
#include <WiFiClient.h>

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
  const uint16_t MELODY_GREETING_NOTES[] = { 784, 988, 1175, 0, 1175 };
  const uint8_t MELODY_GREETING_LEN = 5;
  const uint16_t MELODY_SUCCESS_NOTES[] = { 523, 659, 784 };
  const uint8_t MELODY_SUCCESS_LEN = 3;
  const uint16_t MELODY_ERROR_NOTES[] = { 392, 330, 262 };
  const uint8_t MELODY_ERROR_LEN = 3;

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

  _initCooldownStartMs = millis();
  beginAudio();
}

void VoiceManager::update() {
  if (!_audioReady || _recordingInProgress) {
    return;
  }

  unsigned long nowMs = millis();
  if (nowMs - _initCooldownStartMs < 2000) {
    return; // 开机 2.0 秒浪涌与电容充放电底噪冷却期
  }

  // 大模型请求失败后的惩罚静默保护，防断网时无限录音超时的死循环
  if (nowMs - _lastFailureTimeMs < 10000) {
    return;
  }

  if (nowMs - _lastStatusPrintMs >= 3000) {
    _lastStatusPrintMs = nowMs;
    Serial.println("[Voice] listening");
  }

  if (_micReady && nowMs - _lastMeterPrintMs >= METER_INTERVAL_MS) {
    _lastMeterPrintMs = nowMs;
    uint32_t rms = 0;
    uint32_t peak = 0;
    if (readMicLevel(rms, peak)) {
      const RuntimeConfig& cfg = AppConfigStore.get();
      
      Serial.print("[Voice] mic rms=");
      Serial.print(rms);
      Serial.print(" peak=");
      Serial.print(peak);
      Serial.print(" | Threshold: rms=");
      Serial.print(cfg.triggerRms);
      Serial.print(" peak=");
      Serial.println(cfg.triggerPeak);

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

void VoiceManager::triggerFailureCooldown() {
  _lastFailureTimeMs = millis();
  Serial.println("[Voice] Net failure detected. Entering 10s cooldown...");
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
  i2sConfig.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  i2sConfig.communication_format = (i2s_comm_format_t)ADPET_I2S_COMM_FORMAT;
  i2sConfig.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  i2sConfig.dma_buf_count = 8;
  i2sConfig.dma_buf_len = 1024;
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
  unsigned long recordStartMs = millis();

  while (writtenSamples < sampleCount) {
    // 录音总耗时硬熔断保护：如果超过预期录音时长+3秒，判定硬件断线，强退，绝不锁死系统
    if (millis() - recordStartMs > durationMs + 3000) {
      Serial.println("[Voice] record timeout, hardware mic disconnected?");
      break;
    }

    size_t bytesRead = 0;
    // 100ms 短读超时，代替 portMAX_DELAY 无限死等，防止麦克风故障死锁
    esp_err_t err = i2s_read(MIC_I2S_PORT, raw, sizeof(raw), &bytesRead, 100 / portTICK_PERIOD_MS);
    if (err != ESP_OK || bytesRead == 0) {
      delay(1); // 暂无数据可读时微小调度延迟
      continue;
    }

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

void VoiceManager::playMelody(MelodyType type) {
  if (!_speakerReady) return;
  _melodyPlaying = true;
  _melodyNoteIndex = 0;
  _melodyType = type;
  switch (type) {
    case MELODY_SUCCESS: _currentToneHz = MELODY_SUCCESS_NOTES[0]; _melodyLength = MELODY_SUCCESS_LEN; break;
    case MELODY_ERROR:   _currentToneHz = MELODY_ERROR_NOTES[0];   _melodyLength = MELODY_ERROR_LEN;   break;
    default:             _currentToneHz = MELODY_GREETING_NOTES[0]; _melodyLength = MELODY_GREETING_LEN; break;
  }
  _tonePhase = 0.0f;
  _beepStartMs = millis();
  Serial.println("[Voice] melody");
}

void VoiceManager::updateMelody(unsigned long nowMs) {
  if (!_speakerReady || !_melodyPlaying) return;

  if (nowMs - _beepStartMs >= NOTE_DURATION_MS) {
    _melodyNoteIndex++;
    _beepStartMs = nowMs;
    if (_melodyNoteIndex >= _melodyLength) {
      _melodyPlaying = false;
      _currentToneHz = 0.0f;
      return;
    }
    switch (_melodyType) {
      case MELODY_SUCCESS: _currentToneHz = MELODY_SUCCESS_NOTES[_melodyNoteIndex]; break;
      case MELODY_ERROR:   _currentToneHz = MELODY_ERROR_NOTES[_melodyNoteIndex];   break;
      default:             _currentToneHz = MELODY_GREETING_NOTES[_melodyNoteIndex]; break;
    }
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
  uint16_t numChannels = wavData[22] | ((uint16_t)wavData[23] << 8); // 声道数
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

  // 始终以 I2S_CHANNEL_STEREO 启动物理时钟，适配 MAX98357A
  i2s_set_clk(SPEAKER_I2S_PORT, sampleRate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
  const uint8_t* pcm = wavData + dataOffset;
  
  Serial.print("[Voice] play wav bytes=");
  Serial.print(pcmBytes);
  Serial.print(" channels=");
  Serial.print(numChannels);
  Serial.print(" sampleRate=");
  Serial.println(sampleRate);

  size_t totalWritten = 0;
  bool ok = true;

  if (numChannels == 1) {
    // 单声道 Mono WAV 数据需要动态复制为双声道 Stereo 数据，防止播放速度加快一倍且听不清
    const size_t monoChunkSize = 512; // 512 字节 mono 数据对应 256 个采样
    int16_t stereoBuffer[512];        // 256 个立体声采样对 (512个采样 = 1024 字节)

    while (totalWritten < pcmBytes) {
      size_t remaining = pcmBytes - totalWritten;
      size_t toWriteMono = remaining < monoChunkSize ? remaining : monoChunkSize;
      
      size_t monoSamples = toWriteMono / 2;
      const int16_t* monoPtr = (const int16_t*)(pcm + totalWritten);
      for (size_t i = 0; i < monoSamples; i++) {
        stereoBuffer[i * 2] = monoPtr[i];     // 左声道
        stereoBuffer[i * 2 + 1] = monoPtr[i]; // 右声道
      }

      size_t bytesToWriteStereo = monoSamples * 4;
      size_t writtenStereo = 0;
      esp_err_t err = i2s_write(SPEAKER_I2S_PORT, stereoBuffer, bytesToWriteStereo, &writtenStereo, 100 / portTICK_PERIOD_MS);
      if (err != ESP_OK) {
        Serial.print("[Voice] mono-to-stereo i2s_write error: ");
        Serial.println(err);
        ok = false;
        break;
      }

      if (writtenStereo == 0) {
        delay(1);
      }
      
      // 算出实际消耗了多少 mono 单声道字节数据
      size_t writtenMono = (writtenStereo / 4) * 2;
      totalWritten += writtenMono;
    }
  } else {
    // 原本就是双声道数据，直接推送即可
    const size_t chunkSize = 1024;
    while (totalWritten < pcmBytes) {
      size_t remaining = pcmBytes - totalWritten;
      size_t toWrite = remaining < chunkSize ? remaining : chunkSize;
      size_t written = 0;

      esp_err_t err = i2s_write(SPEAKER_I2S_PORT, pcm + totalWritten, toWrite, &written, 100 / portTICK_PERIOD_MS);
      if (err != ESP_OK) {
        Serial.print("[Voice] stereo i2s_write error: ");
        Serial.println(err);
        ok = false;
        break;
      }

      if (written == 0) {
        delay(1);
      }
      totalWritten += written;
    }
  }

  Serial.print("[Voice] finished playing, total mono written=");
  Serial.println(totalWritten);

  // 播放完成后恢复默认立体声配置
  i2s_set_clk(SPEAKER_I2S_PORT, AppConfig::Audio::SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
  return ok;
}

bool VoiceManager::playWavFromStream(WiFiClient* stream, size_t totalBytes) {
  if (!_speakerReady || stream == nullptr || totalBytes <= 44) {
    return false;
  }

  // 1. 同步读取并解析 WAV 头部（前 44 字节）
  uint8_t header[44];
  stream->setTimeout(3000); // 3 秒超时限制
  size_t headerRead = stream->readBytes(header, 44);
  if (headerRead < 44) {
    Serial.println("[Voice] failed to read WAV header from stream");
    return false;
  }

  uint32_t sampleRate = header[24]
    | ((uint32_t)header[25] << 8)
    | ((uint32_t)header[26] << 16)
    | ((uint32_t)header[27] << 24);
  uint16_t numChannels = header[22] | ((uint16_t)header[23] << 8);
  uint16_t bitsPerSample = header[34] | ((uint16_t)header[35] << 8);

  if (bitsPerSample != 16 || sampleRate == 0) {
    Serial.println("[Voice] stream WAV format unsupported");
    return false;
  }

  // 始终以双声道时钟模式适配 MAX98357A
  i2s_set_clk(SPEAKER_I2S_PORT, sampleRate, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);

  Serial.print("[Voice] play wav stream: channels=");
  Serial.print(numChannels);
  Serial.print(" sampleRate=");
  Serial.println(sampleRate);

  size_t dataBytesToRead = totalBytes - 44;
  size_t totalBytesRead = 0;
  bool ok = true;

  if (numChannels == 1) {
    // 1. 开局预加载机制：首包攒够 32KB (约 680ms 播放口粮) 瞬间塞满 I2S 底层 DMA 队列，极佳地对抗 4G 网络开头握手抖动
    const size_t preBufferSize = 32768;
    uint8_t* preBuffer = (uint8_t*)malloc(preBufferSize);
    if (preBuffer != nullptr) {
      size_t preReadTotal = 0;
      stream->setTimeout(3000); // 允许最多 3 秒建立缓冲
      while (preReadTotal < preBufferSize && totalBytesRead < dataBytesToRead && stream->connected()) {
        size_t remaining = dataBytesToRead - totalBytesRead;
        size_t toRead = preBufferSize - preReadTotal;
        if (toRead > remaining) toRead = remaining;

        size_t bytesRead = stream->readBytes(preBuffer + preReadTotal, toRead);
        if (bytesRead == 0) {
          delay(5);
          continue;
        }
        preReadTotal += bytesRead;
        totalBytesRead += bytesRead;
      }

      // 将预加载的 mono 单声道数据实时展开为双声道并直送 I2S
      if (preReadTotal > 0) {
        size_t monoSamples = preReadTotal / 2;
        const int16_t* monoPtr = (const int16_t*)preBuffer;
        const size_t tempStereoSamples = 1024; // 每次推送 1024 采样对
        int16_t tempStereoBuffer[tempStereoSamples * 2];

        for (size_t offset = 0; offset < monoSamples; offset += tempStereoSamples) {
          size_t chunkSamples = monoSamples - offset;
          if (chunkSamples > tempStereoSamples) chunkSamples = tempStereoSamples;

          for (size_t i = 0; i < chunkSamples; i++) {
            tempStereoBuffer[i * 2] = monoPtr[offset + i];
            tempStereoBuffer[i * 2 + 1] = monoPtr[offset + i];
          }
          size_t writtenStereo = 0;
          i2s_write(SPEAKER_I2S_PORT, tempStereoBuffer, chunkSamples * 4, &writtenStereo, portMAX_DELAY);
        }
      }
      free(preBuffer);
    }

    // 2. 常规播放阶段：将分包从 512 字节调大到 4096 字节（约 128ms 播放），降低网络 Socket 交互频次
    const size_t monoChunkSize = 4096;
    uint8_t* monoBuffer = (uint8_t*)malloc(monoChunkSize);
    int16_t* stereoBuffer = (int16_t*)malloc(monoChunkSize * 2); // 包含 4096 个采样 = 8192 字节

    if (monoBuffer != nullptr && stereoBuffer != nullptr) {
      while (totalBytesRead < dataBytesToRead && stream->connected()) {
        size_t remaining = dataBytesToRead - totalBytesRead;
        size_t toReadMono = remaining < monoChunkSize ? remaining : monoChunkSize;

        size_t bytesRead = stream->readBytes(monoBuffer, toReadMono);
        if (bytesRead == 0) {
          // 网络饥饿状态，塞入 16ms 静音数据垫底，防止 I2S 硬件悬空抖动产生刺耳的兹拉杂音
          int16_t silentBuffer[256 * 2] = {0};
          size_t written = 0;
          i2s_write(SPEAKER_I2S_PORT, silentBuffer, sizeof(silentBuffer), &written, 10 / portTICK_PERIOD_MS);
          delay(5);
          continue;
        }

        size_t monoSamples = bytesRead / 2;
        const int16_t* monoPtr = (const int16_t*)monoBuffer;
        for (size_t i = 0; i < monoSamples; i++) {
          stereoBuffer[i * 2] = monoPtr[i];     // 左
          stereoBuffer[i * 2 + 1] = monoPtr[i]; // 右
        }

        size_t bytesToWriteStereo = monoSamples * 4;
        size_t writtenStereo = 0;
        esp_err_t err = i2s_write(SPEAKER_I2S_PORT, stereoBuffer, bytesToWriteStereo, &writtenStereo, 100 / portTICK_PERIOD_MS);
        if (err != ESP_OK) {
          Serial.print("[Voice] stream mono-to-stereo write error: ");
          Serial.println(err);
          ok = false;
          break;
        }

        totalBytesRead += bytesRead;
      }
    }
    if (monoBuffer != nullptr) free(monoBuffer);
    if (stereoBuffer != nullptr) free(stereoBuffer);
  } else {
    // 双声道直通流式播放：同样调大分包到 4096 字节，减轻慢网交互压力
    const size_t chunkSize = 4096;
    uint8_t* buffer = (uint8_t*)malloc(chunkSize);
    if (buffer != nullptr) {
      while (totalBytesRead < dataBytesToRead && stream->connected()) {
        size_t remaining = dataBytesToRead - totalBytesRead;
        size_t toRead = remaining < chunkSize ? remaining : chunkSize;

        size_t bytesRead = stream->readBytes(buffer, toRead);
        if (bytesRead == 0) {
          int16_t silentBuffer[256 * 2] = {0};
          size_t written = 0;
          i2s_write(SPEAKER_I2S_PORT, silentBuffer, sizeof(silentBuffer), &written, 10 / portTICK_PERIOD_MS);
          delay(5);
          continue;
        }

        size_t written = 0;
        esp_err_t err = i2s_write(SPEAKER_I2S_PORT, buffer, bytesRead, &written, 100 / portTICK_PERIOD_MS);
        if (err != ESP_OK) {
          Serial.print("[Voice] stream stereo write error: ");
          Serial.println(err);
          ok = false;
          break;
        }

        totalBytesRead += bytesRead;
      }
      free(buffer);
    }
  }

  Serial.print("[Voice] finished streaming play, total read bytes=");
  Serial.println(totalBytesRead);

  // 恢复默认立体声配置
  i2s_set_clk(SPEAKER_I2S_PORT, AppConfig::Audio::SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_STEREO);
  return ok;
}
