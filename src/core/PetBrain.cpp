#include "PetBrain.h"
#include "AppConfig.h"

void PetBrain::begin() {
  _emotion = EMOTION_IDLE;
  _mood = MOOD_CONTENT;
  _lastInteractionMs = millis();
  _lastEmotionChangeMs = millis();
  _lastBlinkMs = millis();
  _nextAutoAnimMs = millis() + 5000;
  _autoAnimUntilMs = 0;
  _blinking = false;
  _externalOverride = false;
  _dirty = true;
  randomSeed(analogRead(0) ^ micros());
}

void PetBrain::updateMood(unsigned long nowMs) {
  unsigned long elapsed = nowMs - _lastInteractionMs;
  PetMood newMood;

  if (elapsed < 30000)       newMood = MOOD_EXCITED;
  else if (elapsed < 120000) newMood = MOOD_CONTENT;
  else if (elapsed < 300000) newMood = MOOD_BORED;
  else                       newMood = MOOD_DROWSY;

  if (newMood != _mood) {
    _mood = newMood;
    if (!_externalOverride && _autoAnimUntilMs == 0) {
      Emotion base = moodBaseEmotion();
      if (base != _emotion) {
        _emotion = base;
        _dirty = true;
      }
    }
  }
}

Emotion PetBrain::moodBaseEmotion() const {
  switch (_mood) {
    case MOOD_EXCITED: return EMOTION_HAPPY;
    case MOOD_CONTENT: return EMOTION_IDLE;
    case MOOD_BORED:   return EMOTION_IDLE;
    case MOOD_DROWSY:  return EMOTION_SLEEPY;
  }
  return EMOTION_IDLE;
}

PetRenderState PetBrain::update(unsigned long nowMs) {
  updateMood(nowMs);

  // External override: don't run autonomous logic
  if (_externalOverride) {
    if (_dirty) { _dirty = false; return { _emotion, true }; }
    return { _emotion, false };
  }

  // Auto-animation expired → revert to base
  if (_autoAnimUntilMs > 0 && nowMs >= _autoAnimUntilMs) {
    _autoAnimUntilMs = 0;
    Emotion base = moodBaseEmotion();
    if (_emotion != base) {
      _emotion = base;
      _dirty = true;
    }
  }

  // Blink
  Emotion base = moodBaseEmotion();
  bool atBase = (_autoAnimUntilMs == 0 && _emotion == base);

  if (!_blinking && atBase && nowMs - _lastBlinkMs > AppConfig::IDLE_BLINK_INTERVAL_MS) {
    _blinking = true;
    _lastBlinkMs = nowMs;
    return { EMOTION_BLINK, true };
  }
  if (_blinking && nowMs - _lastBlinkMs > AppConfig::BLINK_DURATION_MS) {
    _blinking = false;
    return { _emotion, true };
  }

  // Autonomous animation trigger
  if (!_blinking && _autoAnimUntilMs == 0 && nowMs >= _nextAutoAnimMs) {
    triggerAutonomousAnim(nowMs);
    scheduleNextAuto(nowMs);
  }

  if (_dirty) { _dirty = false; return { _emotion, true }; }
  return { _emotion, false };
}

void PetBrain::setEmotion(Emotion emotion) {
  _externalOverride = true;
  _autoAnimUntilMs = 0;
  _blinking = false;
  if (_emotion == emotion) return;
  _emotion = emotion;
  _lastEmotionChangeMs = millis();
  _dirty = true;
}

void PetBrain::clearOverride() {
  _externalOverride = false;
  _lastInteractionMs = millis();
  _mood = MOOD_EXCITED;
  Emotion base = moodBaseEmotion();
  if (_emotion != base) {
    _emotion = base;
    _dirty = true;
  }
  scheduleNextAuto(millis());
}

void PetBrain::onInteraction() {
  _lastInteractionMs = millis();
  _mood = MOOD_EXCITED;
}

Emotion PetBrain::currentEmotion() const { return _emotion; }
PetMood PetBrain::currentMood() const { return _mood; }

void PetBrain::triggerAutonomousAnim(unsigned long nowMs) {
  int r = random(100);
  Emotion anim;

  switch (_mood) {
    case MOOD_EXCITED:
      if      (r < 30) anim = EMOTION_LOOK_LEFT;
      else if (r < 60) anim = EMOTION_LOOK_RIGHT;
      else if (r < 85) anim = EMOTION_HAPPY;
      else             anim = EMOTION_SURPRISED;
      break;
    case MOOD_CONTENT:
      if      (r < 25) anim = EMOTION_LOOK_LEFT;
      else if (r < 50) anim = EMOTION_LOOK_RIGHT;
      else if (r < 75) anim = EMOTION_HAPPY;
      else             anim = EMOTION_BLINK;
      break;
    case MOOD_BORED:
      if      (r < 30) anim = EMOTION_LOOK_LEFT;
      else if (r < 60) anim = EMOTION_LOOK_RIGHT;
      else if (r < 80) anim = EMOTION_SAD;
      else             anim = EMOTION_SLEEPY;
      break;
    case MOOD_DROWSY:
      if      (r < 50) anim = EMOTION_SLEEPY;
      else if (r < 80) anim = EMOTION_BLINK;
      else             anim = EMOTION_LOOK_LEFT;
      break;
    default:
      anim = EMOTION_BLINK;
  }

  _emotion = anim;
  _lastEmotionChangeMs = nowMs;
  _autoAnimUntilMs = nowMs + 800;
  _dirty = true;
}

void PetBrain::scheduleNextAuto(unsigned long nowMs) {
  unsigned long minMs, maxMs;
  switch (_mood) {
    case MOOD_EXCITED: minMs = 8000;  maxMs = 15000; break;
    case MOOD_CONTENT: minMs = 5000;  maxMs = 10000; break;
    case MOOD_BORED:   minMs = 3000;  maxMs = 7000;  break;
    case MOOD_DROWSY:  minMs = 10000; maxMs = 20000; break;
    default:           minMs = 5000;  maxMs = 10000;
  }
  _nextAutoAnimMs = nowMs + random(minMs, maxMs);
}
