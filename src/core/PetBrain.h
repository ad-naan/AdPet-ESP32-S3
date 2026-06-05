#pragma once

#include <Arduino.h>
#include "Emotion.h"

struct PetRenderState {
  Emotion emotion;
  bool changed;
};

class PetBrain {
public:
  void begin();
  PetRenderState update(unsigned long nowMs);
  void setEmotion(Emotion emotion);
  Emotion currentEmotion() const;

private:
  Emotion _emotion = EMOTION_IDLE;
  unsigned long _lastEmotionChangeMs = 0;
  unsigned long _lastBlinkMs = 0;
  bool _blinking = false;
  bool _dirty = true;

  Emotion nextDemoEmotion(Emotion current) const;
};
