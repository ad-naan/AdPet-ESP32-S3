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
  void clearOverride();
  void onInteraction();
  Emotion currentEmotion() const;
  PetMood currentMood() const;
  Emotion moodBaseEmotion() const;

private:
  PetMood _mood = MOOD_CONTENT;
  Emotion _emotion = EMOTION_IDLE;

  unsigned long _lastInteractionMs = 0;
  unsigned long _lastEmotionChangeMs = 0;
  unsigned long _lastBlinkMs = 0;
  unsigned long _nextAutoAnimMs = 0;
  unsigned long _autoAnimUntilMs = 0;

  bool _blinking = false;
  bool _dirty = true;
  bool _externalOverride = false;

  void updateMood(unsigned long nowMs);
  void triggerAutonomousAnim(unsigned long nowMs);
  void scheduleNextAuto(unsigned long nowMs);
};
