#include "AppController.h"
#include "../core/AppConfig.h"

void AppController::begin() {
  Serial.begin(AppConfig::SerialPort::BAUD_RATE);
  delay(200);

  Serial.println();
  Serial.println("AdPet framework starting...");

  _display.begin();
  _display.showBoot();

  _brain.begin();
  _network.begin();
  _voice.begin();
  _llm.begin();

  _display.drawFace(_brain.currentEmotion());
  Serial.println("AdPet framework ready.");
}

void AppController::update() {
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
