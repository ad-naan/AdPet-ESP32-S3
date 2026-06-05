#include "VoiceManager.h"
#include "../../core/AppConfig.h"

void VoiceManager::begin() {
  if (!AppConfig::Feature::VOICE_ENABLED) {
    Serial.println("[Voice] disabled");
    return;
  }

  Serial.println("[Voice] TODO: mic, wake word, STT, speaker/TTS");
}

void VoiceManager::update() {
}

bool VoiceManager::hasUserSpeech() const {
  return _hasSpeech;
}

String VoiceManager::takeUserText() {
  _hasSpeech = false;
  String text = _lastText;
  _lastText = "";
  return text;
}
