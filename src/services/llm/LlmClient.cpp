#include "LlmClient.h"
#include "../../core/AppConfig.h"

void LlmClient::begin() {
  if (!AppConfig::Feature::LLM_ENABLED) {
    Serial.println("[LLM] disabled");
    return;
  }

  Serial.println("[LLM] TODO: HTTP client, auth, prompt, response parser");
}

void LlmClient::update() {
}

bool LlmClient::isBusy() const {
  return _busy;
}

void LlmClient::ask(const String& userText) {
  if (!AppConfig::Feature::LLM_ENABLED) {
    return;
  }

  _busy = true;
  Serial.print("[LLM] user: ");
  Serial.println(userText);
}

bool LlmClient::hasReply() const {
  return _hasReply;
}

String LlmClient::takeReply() {
  _hasReply = false;
  String reply = _reply;
  _reply = "";
  return reply;
}
