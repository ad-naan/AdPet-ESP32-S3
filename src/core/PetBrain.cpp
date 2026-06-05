#include "PetBrain.h"
#include "AppConfig.h"

void PetBrain::begin() {
  _emotion = EMOTION_IDLE;
  _lastEmotionChangeMs = millis();
  _lastBlinkMs = millis();
  _blinking = false;
  _dirty = true;
}

PetRenderState PetBrain::update(unsigned long nowMs) {
  if (!_blinking && _emotion == EMOTION_IDLE && nowMs - _lastBlinkMs > AppConfig::IDLE_BLINK_INTERVAL_MS) {
    _blinking = true;
    _lastBlinkMs = nowMs;
    return { EMOTION_BLINK, true };
  }

  if (_blinking && nowMs - _lastBlinkMs > AppConfig::BLINK_DURATION_MS) {
    _blinking = false;
    return { _emotion, true };
  }

  if (nowMs - _lastEmotionChangeMs > AppConfig::DEMO_EMOTION_INTERVAL_MS) {
    setEmotion(nextDemoEmotion(_emotion));
  }

  if (_dirty) {
    _dirty = false;
    return { _emotion, true };
  }

  return { _emotion, false };
}

void PetBrain::setEmotion(Emotion emotion) {
  if (_emotion == emotion) {
    return;
  }

  _emotion = emotion;
  _lastEmotionChangeMs = millis();
  _dirty = true;
}

Emotion PetBrain::currentEmotion() const {
  return _emotion;
}

Emotion PetBrain::nextDemoEmotion(Emotion current) const {
  switch (current) {
    case EMOTION_IDLE:
      return EMOTION_HAPPY;
    case EMOTION_HAPPY:
      return EMOTION_SURPRISED;
    case EMOTION_SURPRISED:
      return EMOTION_ANGRY;
    case EMOTION_ANGRY:
      return EMOTION_SLEEPY;
    case EMOTION_SLEEPY:
    case EMOTION_BLINK:
    case EMOTION_THINKING:
    case EMOTION_TALKING:
      return EMOTION_IDLE;
  }

  return EMOTION_IDLE;
}
