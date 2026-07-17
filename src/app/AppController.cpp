#include "AppController.h"
#include "../core/AppConfig.h"
#include "../core/ConfigManager.h"
#include <esp32-hal-psram.h>
#include <esp_heap_caps.h>

void AppController::begin() {
  Serial.begin(AppConfig::SerialPort::BAUD_RATE);
  delay(200);

  Serial.println();
  Serial.println("AdPet framework starting...");
  printMemoryStats("boot");
  AppConfigStore.begin();

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
  printMemoryStats("ready");
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

  if (_voice.isRecording()) {
    Emotion triggerEmotion = emotionFromName(AppConfigStore.get().triggerEmotion, EMOTION_LISTENING);
    if (_brain.currentEmotion() != triggerEmotion) {
      _brain.setEmotion(triggerEmotion);
      _display.drawFace(triggerEmotion);
    }
  }

  handleVoiceToLlm();
  handleLlmReply();

  if (_audioReactionActive && millis() - _audioReactionStartMs > 900) {
    _audioReactionActive = false;
    Emotion idleEmotion = emotionFromName(AppConfigStore.get().idleEmotion, EMOTION_IDLE);
    _brain.setEmotion(idleEmotion);
    _display.drawFace(idleEmotion);
  }

  PetRenderState renderState = _brain.update(nowMs);
  if (renderState.changed) {
    _display.drawFace(renderState.emotion);
  }

  delay(AppConfig::LOOP_DELAY_MS);
}

void AppController::updateAudioTestMode() {
  _voice.update();

  if (_voice.hasRecording()) {
    uint8_t* wavData = nullptr;
    size_t wavSize = 0;
    if (_voice.takeRecording(wavData, wavSize)) {
      Serial.print("[AudioTest] captured wav bytes=");
      Serial.println(wavSize);
      free(wavData);
      if (AppConfig::Feature::AUDIO_TEST_DISPLAY) {
        _display.drawFace(EMOTION_SURPRISED);
        _audioReactionActive = true;
        _audioReactionStartMs = millis();
      }
    }
  }

  if (AppConfig::Feature::AUDIO_TEST_DISPLAY && _audioReactionActive && millis() - _audioReactionStartMs > 800) {
    _audioReactionActive = false;
    _display.drawFace(EMOTION_IDLE);
  }
}

void AppController::handleVoiceToLlm() {
  if (_llm.isBusy()) {
    return;
  }

  if (_voice.hasRecording()) {
    uint8_t* wavData = nullptr;
    size_t wavSize = 0;
    if (!_voice.takeRecording(wavData, wavSize)) {
      return;
    }

    _brain.setEmotion(EMOTION_THINKING);
    _display.drawFace(EMOTION_THINKING);

    String transcript;
    String reply;
    String replyEmotionName;
    uint8_t* ttsWav = nullptr;
    size_t ttsWavSize = 0;
    bool ok = _llm.voiceChat(wavData, wavSize, transcript, reply, replyEmotionName, ttsWav, ttsWavSize);
    free(wavData);

    if (ok) {
      _network.setLastReply(reply);
      Emotion configuredReply = emotionFromName(AppConfigStore.get().replyEmotion, EMOTION_TALKING);
      Emotion replyEmotion = emotionFromName(replyEmotionName, configuredReply);
      _brain.setEmotion(replyEmotion);
      _display.drawFace(replyEmotion);
      if (ttsWav != nullptr && ttsWavSize > 0) {
        bool played = _voice.playWav(ttsWav, ttsWavSize);
        _llm.freeTts(ttsWav);
        if (played) {
          Emotion idleEmotion = emotionFromName(AppConfigStore.get().idleEmotion, EMOTION_IDLE);
          _brain.setEmotion(idleEmotion);
          _display.drawFace(idleEmotion);
        } else {
          _voice.playMelody();
          _audioReactionActive = true;
          _audioReactionStartMs = millis();
        }
      } else {
        _voice.playMelody();
        _audioReactionActive = true;
        _audioReactionStartMs = millis();
      }
    } else {
      _brain.setEmotion(EMOTION_ANGRY);
      _display.drawFace(EMOTION_ANGRY);
      _voice.playMelody();
      _audioReactionActive = true;
      _audioReactionStartMs = millis();
    }
    return;
  }

  if (_network.hasChatRequest()) {
    _brain.setEmotion(EMOTION_THINKING);
    _display.drawFace(EMOTION_THINKING);
    _llm.ask(_network.takeChatRequest());
    return;
  }

  if (!_voice.hasUserSpeech()) {
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
  _network.setLastReply(reply);
  _brain.setEmotion(EMOTION_TALKING);
  _display.drawFace(EMOTION_TALKING);
  _voice.playMelody();
  _audioReactionActive = true;
  _audioReactionStartMs = millis();
}

void AppController::printMemoryStats(const char* stage) {
  Serial.print("[Memory] ");
  Serial.print(stage);
  Serial.print(" freeHeap=");
  Serial.print(ESP.getFreeHeap());
  Serial.print(" largestBlock=");
  Serial.print(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  Serial.print(" psram=");
  Serial.print(psramFound() ? "yes" : "no");
  Serial.print(" psramSize=");
  Serial.print(ESP.getPsramSize());
  Serial.print(" freePsram=");
  Serial.println(ESP.getFreePsram());
}
