#pragma once

#include <Arduino.h>

class VoiceManager {
public:
  void begin();
  void update();
  bool hasUserSpeech() const;
  String takeUserText();

private:
  bool _hasSpeech = false;
  String _lastText;
};
