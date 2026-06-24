#pragma once

#include <Arduino.h>

enum MelodyType {
  MELODY_GREETING,
  MELODY_SUCCESS,
  MELODY_ERROR
};

#include <WiFiClient.h>

class VoiceManager {
public:
  void begin();
  void update();

  bool hasUserSpeech() const;
  String takeUserText();

  bool hasRecording() const;
  bool takeRecording(uint8_t*& wavData, size_t& wavSize);
  void freeRecording();
  void triggerFailureCooldown();

  void playMelody(MelodyType type = MELODY_GREETING);
  bool playWav(const uint8_t* wavData, size_t wavSize);
  bool playWavFromStream(WiFiClient* stream, size_t totalBytes);

private:
  bool _audioReady = false;
  bool _micReady = false;
  bool _speakerReady = false;
  bool _melodyPlaying = false;
  bool _hasSpeech = false;
  bool _hasRecording = false;
  bool _recordingInProgress = false;

  unsigned long _lastMeterPrintMs = 0;
  unsigned long _beepStartMs = 0;
  unsigned long _lastStatusPrintMs = 0;
  unsigned long _lastSoundTriggerMs = 0;
  unsigned long _initCooldownStartMs = 0;
  unsigned long _lastFailureTimeMs = 0;

  uint8_t* _recording = nullptr;
  size_t _recordingSize = 0;
  uint8_t _melodyNoteIndex = 0;
  uint8_t _melodyLength = 0;
  MelodyType _melodyType = MELODY_GREETING;
  float _tonePhase = 0.0f;
  float _currentToneHz = 0.0f;
  String _lastText;

  void beginAudio();
  bool beginMic();
  bool beginSpeaker();
  bool readMicLevel(uint32_t& rms, uint32_t& peak);
  bool recordWav(uint16_t durationMs);
  void writeWavHeader(uint8_t* buffer, uint32_t pcmBytes);
  void updateMelody(unsigned long nowMs);
  void playTestToneChunk();
};
