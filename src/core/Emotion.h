#pragma once

#include <Arduino.h>

enum Emotion {
  EMOTION_IDLE,
  EMOTION_LISTENING,
  EMOTION_HAPPY,
  EMOTION_SAD,
  EMOTION_CONFUSED,
  EMOTION_SLEEPY,
  EMOTION_BLINK,
  EMOTION_SURPRISED,
  EMOTION_ANGRY,
  EMOTION_THINKING,
  EMOTION_TALKING
};

inline Emotion emotionFromName(const String& value, Emotion fallback = EMOTION_IDLE) {
  String name = value;
  name.trim();
  name.toLowerCase();
  if (name == "idle") return EMOTION_IDLE;
  if (name == "listening") return EMOTION_LISTENING;
  if (name == "happy") return EMOTION_HAPPY;
  if (name == "sad") return EMOTION_SAD;
  if (name == "confused") return EMOTION_CONFUSED;
  if (name == "sleepy") return EMOTION_SLEEPY;
  if (name == "blink") return EMOTION_BLINK;
  if (name == "surprised") return EMOTION_SURPRISED;
  if (name == "angry") return EMOTION_ANGRY;
  if (name == "thinking") return EMOTION_THINKING;
  if (name == "talking") return EMOTION_TALKING;
  return fallback;
}
