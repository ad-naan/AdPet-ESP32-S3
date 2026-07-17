#pragma once

#include <Arduino.h>

class VoiceManager {
public:
  void begin();
  void update();

  bool hasUserSpeech() const;
  String takeUserText();

  bool hasRecording() const;
  bool isRecording() const;
  bool takeRecording(uint8_t*& wavData, size_t& wavSize);
  void freeRecording();

  void playMelody();
  bool playWav(const uint8_t* wavData, size_t wavSize);

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
  unsigned long _suppressTriggerUntilMs = 0;

  uint8_t* _recording = nullptr;
  size_t _recordingSize = 0;
  size_t _recordingCapacitySamples = 0;
  size_t _recordedSamples = 0;
  size_t _samplesSinceTrigger = 0;
  size_t _silenceSamples = 0;
  size_t _loudSamples = 0;
  int16_t* _preRoll = nullptr;
  size_t _preRollCapacity = 0;
  size_t _preRollCount = 0;
  size_t _preRollWrite = 0;
  uint8_t _melodyNoteIndex = 0;
  float _tonePhase = 0.0f;
  float _currentToneHz = 0.0f;
  String _lastText;

  void beginAudio();
  bool beginMic();
  bool beginSpeaker();
  bool readMicChunk(int16_t* pcm, size_t capacity, size_t& sampleCount, uint32_t& rms, uint32_t& peak);
  void processMicChunk(const int16_t* pcm, size_t sampleCount, uint32_t rms, uint32_t peak, unsigned long nowMs);
  void pushPreRoll(const int16_t* pcm, size_t sampleCount);
  bool startRecording();
  void appendRecording(const int16_t* pcm, size_t sampleCount);
  void finishRecording();
  void discardMicInput();
  void writeWavHeader(uint8_t* buffer, uint32_t pcmBytes);
  void updateMelody(unsigned long nowMs);
  void playTestToneChunk();
};
