#include "AppController.h"
#include "../core/AppConfig.h"

void AppController::begin() {
  Serial.begin(AppConfig::SerialPort::BAUD_RATE);
  delay(200);

  Serial.println();
  Serial.println("AdPet framework starting...");

  if (!AppConfig::Feature::AUDIO_TEST_MODE || AppConfig::Feature::AUDIO_TEST_DISPLAY) {
    _display.begin();
    _display.showBoot();
  }

  _brain.begin();

  if (!AppConfig::Feature::AUDIO_TEST_MODE) {
    _network.begin();
  }

  _voice.begin();

  if (!AppConfig::Feature::AUDIO_TEST_MODE) {
    _llm.begin();
    _display.drawFace(_brain.currentEmotion());
  } else if (AppConfig::Feature::AUDIO_TEST_DISPLAY) {
    _display.drawFace(EMOTION_IDLE);
  }

  Serial.println("AdPet framework ready.");
}

void AppController::update() {
  if (AppConfig::Feature::AUDIO_TEST_MODE) {
    updateAudioTestMode();
    delay(AppConfig::LOOP_DELAY_MS);
    return;
  }

  unsigned long nowMs = millis();

  _network.update();
  _voice.update();
  _llm.update();

  handleVoiceToLlm();
  handleLlmReply();

  PetRenderState renderState = _brain.update(nowMs);
  if (renderState.changed) {
    _display.drawFace(renderState.emotion);
  }

  delay(AppConfig::LOOP_DELAY_MS);
}

void AppController::updateAudioTestMode() {
  _voice.update();

  if (AppConfig::Feature::AUDIO_TEST_DISPLAY && _voice.hasUserSpeech()) {
    _voice.takeUserText();
    _display.drawFace(EMOTION_SURPRISED);
    _audioReactionActive = true;
    _audioReactionStartMs = millis();
  }

  if (AppConfig::Feature::AUDIO_TEST_DISPLAY && _audioReactionActive && millis() - _audioReactionStartMs > 800) {
    _audioReactionActive = false;
    _display.drawFace(EMOTION_IDLE);
  }
}

void AppController::handleVoiceToLlm() {
  if (!_voice.hasUserSpeech() || _llm.isBusy()) {
    return;
  }

  _brain.setEmotion(EMOTION_THINKING);
  _llm.ask(_voice.takeUserText());
}

void AppController::handleLlmReply() {
  if (!_llm.hasReply()) {
    return;
  }

  String reply = _llm.takeReply();
  Serial.print("[LLM] reply: ");
  Serial.println(reply);
  _brain.setEmotion(EMOTION_TALKING);
}
