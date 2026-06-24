#pragma once

#include <Arduino.h>

class VoiceManager;

class LlmClient {
public:
  void begin();
  void update();
  bool isBusy() const;
  void ask(const String& userText);
  bool hasReply() const;
  String takeReply();

  bool voiceChat(const uint8_t* wavData, size_t wavSize, String& transcript, String& reply, String& emotionStr, VoiceManager& voice);

private:
  bool _busy = false;
  bool _hasReply = false;
  String _reply;

  String joinUrl(const String& baseUrl, const String& path) const;
  String jsonEscape(const String& value) const;
  String extractJsonString(const String& json, const String& key) const;
  bool postGatewayVoice(const uint8_t* wavData, size_t wavSize, String& transcript, String& reply, String& emotionStr, VoiceManager& voice);
  bool postGatewayText(const String& text, String& reply);
};
