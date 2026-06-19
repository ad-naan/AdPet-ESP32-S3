#pragma once

#include <Arduino.h>

class VoiceManager {
public:
  void begin();
  void update();
  bool hasUserSpeech() const;
  String takeUserText();

private:
  bool _audioReady = false;
  bool _micReady = false;
  bool _speakerReady = false;
  bool _beeping = false;
  bool _hasSpeech = false;
  unsigned long _lastMeterPrintMs = 0;
  unsigned long _lastBeepMs = 0;
  unsigned long _beepStartMs = 0;
  unsigned long _lastStatusPrintMs = 0;
  unsigned long _lastSoundTriggerMs = 0;
  float _tonePhase = 0.0f;
  String _lastText;

  void beginAudioTest();
  bool beginMicTest();
  bool beginSpeakerTest();
  void updateAudioTest();
  bool readMicLevel(uint32_t& rms, uint32_t& peak);
  void startBeep();
  void playTestToneChunk();
};
