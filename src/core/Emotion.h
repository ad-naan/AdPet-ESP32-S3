#pragma once

#include <Arduino.h>

enum Emotion {
  EMOTION_IDLE,
  EMOTION_HAPPY,
  EMOTION_SAD,
  EMOTION_SLEEPY,
  EMOTION_BLINK,
  EMOTION_SURPRISED,
  EMOTION_ANGRY,
  EMOTION_THINKING,
  EMOTION_TALKING,
  EMOTION_LOOK_LEFT,
  EMOTION_LOOK_RIGHT
};

enum PetMood {
  MOOD_EXCITED,
  MOOD_CONTENT,
  MOOD_BORED,
  MOOD_DROWSY
};

inline Emotion emotionFromString(const String& str) {
  if (str == "happy")     return EMOTION_HAPPY;
  if (str == "sad")       return EMOTION_SAD;
  if (str == "sleepy")    return EMOTION_SLEEPY;
  if (str == "surprised") return EMOTION_SURPRISED;
  if (str == "angry")     return EMOTION_ANGRY;
  if (str == "thinking")  return EMOTION_THINKING;
  if (str == "talking")   return EMOTION_TALKING;
  return EMOTION_IDLE;
}
