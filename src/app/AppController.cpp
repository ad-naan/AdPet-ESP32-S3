#include "AppController.h"
#include "../core/AppConfig.h"
#include "../core/ConfigManager.h"

void AppController::begin() {
  Serial.begin(AppConfig::SerialPort::BAUD_RATE);
  delay(200);

  Serial.println();
  Serial.println("AdPet framework starting...");
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
    String emotionStr;
    bool ok = _llm.voiceChat(wavData, wavSize, transcript, reply, emotionStr, _voice);
    free(wavData);

    if (ok) {
      // Use Gateway emotion if available, otherwise default to TALKING
      Emotion replyEmotion = EMOTION_TALKING;
      if (emotionStr.length() > 0) {
        replyEmotion = emotionFromString(emotionStr);
        Serial.print("[App] gateway emotion: ");
        Serial.println(emotionStr);
      }

      _brain.setEmotion(replyEmotion);
      _display.drawFace(replyEmotion);

      // 流式音频播放已在 llm 内部完成，此处做短暂延时展示表情后收尾清除
      delay(800);
      _brain.clearOverride();
      _display.drawFace(_brain.currentEmotion());
    } else {
      _brain.setEmotion(EMOTION_ANGRY);
      _display.drawFace(EMOTION_ANGRY);
      _voice.playMelody(MELODY_ERROR);
      _voice.triggerFailureCooldown(); // 失败静默惩罚 10 秒，防止断网死循环
      // Brief pause to show error emotion, then clear
      delay(500);
      _brain.clearOverride();
      _display.drawFace(_brain.currentEmotion());
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
  _voice.playMelody(MELODY_SUCCESS);

  // Clear override after melody
  _brain.clearOverride();
  _display.drawFace(_brain.currentEmotion());
}
