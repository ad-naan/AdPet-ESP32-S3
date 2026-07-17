#include "LlmClient.h"
#include "../../core/AppConfig.h"
#include "../../core/ConfigManager.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp32-hal-psram.h>

void LlmClient::begin() {
  if (!AppConfig::Feature::LLM_ENABLED) {
    Serial.println("[Gateway] disabled");
    return;
  }

  Serial.println("[Gateway] client ready");
}

void LlmClient::update() {
}

bool LlmClient::isBusy() const {
  return _busy;
}

void LlmClient::ask(const String& userText) {
  if (!AppConfig::Feature::LLM_ENABLED) return;
  _busy = true;
  if (postGatewayText(userText, _reply)) {
    _hasReply = true;
  }
  _busy = false;
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

bool LlmClient::voiceChat(const uint8_t* wavData, size_t wavSize, String& transcript, String& reply, String& emotion, uint8_t*& ttsWav, size_t& ttsWavSize) {
  ttsWav = nullptr;
  ttsWavSize = 0;
  if (!AppConfig::Feature::LLM_ENABLED) return false;
  if (wavData == nullptr || wavSize == 0) return false;

  _busy = true;
  bool ok = postGatewayVoice(wavData, wavSize, transcript, reply, emotion, ttsWav, ttsWavSize);
  _busy = false;
  return ok;
}

void LlmClient::freeTts(uint8_t* ttsWav) {
  if (ttsWav != nullptr) free(ttsWav);
}

String LlmClient::joinUrl(const String& baseUrl, const String& path) const {
  if (baseUrl.endsWith("/")) return baseUrl.substring(0, baseUrl.length() - 1) + path;
  return baseUrl + path;
}

String LlmClient::jsonEscape(const String& value) const {
  String out;
  out.reserve(value.length() + 16);
  for (size_t i = 0; i < value.length(); i++) {
    char ch = value[i];
    if (ch == '\\') out += "\\\\";
    else if (ch == '"') out += "\\\"";
    else if (ch == '\n') out += "\\n";
    else if (ch == '\r') out += "\\r";
    else if (ch == '\t') out += "\\t";
    else out += ch;
  }
  return out;
}

String LlmClient::extractJsonString(const String& json, const String& key) const {
  String marker = "\"" + key + "\"";
  int keyPos = json.indexOf(marker);
  if (keyPos < 0) return "";
  int colon = json.indexOf(':', keyPos + marker.length());
  if (colon < 0) return "";
  int start = json.indexOf('"', colon + 1);
  if (start < 0) return "";

  String out;
  bool escaping = false;
  for (int i = start + 1; i < (int)json.length(); i++) {
    char ch = json[i];
    if (escaping) {
      if (ch == 'n') out += '\n';
      else if (ch == 'r') out += '\r';
      else if (ch == 't') out += '\t';
      else out += ch;
      escaping = false;
    } else if (ch == '\\') {
      escaping = true;
    } else if (ch == '"') {
      return out;
    } else {
      out += ch;
    }
  }
  return out;
}

bool LlmClient::postGatewayVoice(const uint8_t* wavData, size_t wavSize, String& transcript, String& reply, String& emotion, uint8_t*& ttsWav, size_t& ttsWavSize) {
  const RuntimeConfig& cfg = AppConfigStore.get();
  if (cfg.gatewayBaseUrl.length() == 0) {
    Serial.println("[Gateway] missing base URL");
    return false;
  }

  String boundary = "----AdPetBoundary7MA4YWxkTrZu0gW";
  String meta = "{\"device_id\":\"" + jsonEscape(cfg.deviceId) + "\",\"system_prompt\":\"" + jsonEscape(cfg.systemPrompt) + "\"}";
  String head = "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"metadata\"\r\n";
  head += "Content-Type: application/json\r\n\r\n";
  head += meta + "\r\n";
  head += "--" + boundary + "\r\n";
  head += "Content-Disposition: form-data; name=\"audio\"; filename=\"adpet.wav\"\r\n";
  head += "Content-Type: audio/wav\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";

  size_t bodySize = head.length() + wavSize + tail.length();
  uint8_t* body = psramFound() ? (uint8_t*)ps_malloc(bodySize) : nullptr;
  if (body == nullptr) body = (uint8_t*)malloc(bodySize);
  if (body == nullptr) {
    Serial.println("[Gateway] request malloc failed");
    return false;
  }

  memcpy(body, head.c_str(), head.length());
  memcpy(body + head.length(), wavData, wavSize);
  memcpy(body + head.length() + wavSize, tail.c_str(), tail.length());

  HTTPClient http;
  WiFiClientSecure secureClient;
  String url = joinUrl(cfg.gatewayBaseUrl, "/adpet/chat");
  if (url.startsWith("https://")) {
    secureClient.setInsecure();
    http.begin(secureClient, url);
  } else {
    http.begin(url);
  }
  http.setTimeout(45000);

  const char* headers[] = { "X-AdPet-Transcript", "X-AdPet-Reply", "X-AdPet-Emotion" };
  http.collectHeaders(headers, 3);
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  if (cfg.gatewayApiKey.length() > 0) {
    http.addHeader("Authorization", "Bearer " + cfg.gatewayApiKey);
  }
  http.addHeader("X-AdPet-Device-Id", cfg.deviceId);

  Serial.println("[Gateway] POST /adpet/chat");
  Serial.print("[Gateway] request bytes=");
  Serial.print(bodySize);
  Serial.print(" freeHeap=");
  Serial.print(ESP.getFreeHeap());
  Serial.print(" freePsram=");
  Serial.println(ESP.getFreePsram());
  int code = http.POST(body, bodySize);
  free(body);
  Serial.print("[Gateway] HTTP ");
  Serial.println(code);
  if (code < 200 || code >= 300) {
    Serial.println(http.getString());
    http.end();
    return false;
  }

  transcript = http.header("X-AdPet-Transcript");
  reply = http.header("X-AdPet-Reply");
  emotion = http.header("X-AdPet-Emotion");

  int len = http.getSize();
  if (len <= 44 || len > 220000) {
    Serial.print("[Gateway] unsupported audio length: ");
    Serial.println(len);
    http.end();
    return reply.length() > 0;
  }

  ttsWav = psramFound() ? (uint8_t*)ps_malloc(len) : nullptr;
  if (ttsWav == nullptr) ttsWav = (uint8_t*)malloc(len);
  if (ttsWav == nullptr) {
    Serial.println("[Gateway] response malloc failed");
    http.end();
    return reply.length() > 0;
  }

  WiFiClient* stream = http.getStreamPtr();
  size_t offset = 0;
  while (http.connected() && offset < (size_t)len) {
    size_t available = stream->available();
    if (available) {
      size_t remaining = (size_t)len - offset;
      size_t toRead = available < remaining ? available : remaining;
      int readNow = stream->readBytes(ttsWav + offset, toRead);
      offset += readNow;
    } else {
      delay(1);
    }
  }
  http.end();

  ttsWavSize = offset;
  if (ttsWavSize <= 44 || ttsWavSize != (size_t)len) {
    Serial.print("[Gateway] incomplete audio bytes=");
    Serial.println(ttsWavSize);
    free(ttsWav);
    ttsWav = nullptr;
    ttsWavSize = 0;
    return reply.length() > 0;
  }
  return true;
}

bool LlmClient::postGatewayText(const String& text, String& reply) {
  const RuntimeConfig& cfg = AppConfigStore.get();
  if (cfg.gatewayBaseUrl.length() == 0) return false;

  String body = "{\"device_id\":\"" + jsonEscape(cfg.deviceId) + "\",";
  body += "\"system_prompt\":\"" + jsonEscape(cfg.systemPrompt) + "\",";
  body += "\"text\":\"" + jsonEscape(text) + "\"}";

  HTTPClient http;
  WiFiClientSecure secureClient;
  String url = joinUrl(cfg.gatewayBaseUrl, "/adpet/text");
  if (url.startsWith("https://")) {
    secureClient.setInsecure();
    http.begin(secureClient, url);
  } else {
    http.begin(url);
  }

  http.addHeader("Content-Type", "application/json");
  if (cfg.gatewayApiKey.length() > 0) {
    http.addHeader("Authorization", "Bearer " + cfg.gatewayApiKey);
  }
  http.addHeader("X-AdPet-Device-Id", cfg.deviceId);

  int code = http.POST(body);
  String response = http.getString();
  http.end();
  Serial.print("[GatewayText] HTTP ");
  Serial.println(code);
  if (code < 200 || code >= 300) {
    Serial.println(response);
    return false;
  }

  reply = extractJsonString(response, "reply");
  return reply.length() > 0;
}
