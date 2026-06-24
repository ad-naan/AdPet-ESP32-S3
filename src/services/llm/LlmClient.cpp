#include "LlmClient.h"
#include "../../core/AppConfig.h"
#include "../../core/ConfigManager.h"
#include "../../drivers/audio/VoiceManager.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

static String urlDecode(const String& str) {
  String decoded = "";
  char temp[] = "00";
  for (unsigned int i = 0; i < str.length(); i++) {
    if (str[i] == '%') {
      if (i + 2 < str.length()) {
        temp[0] = str[i + 1];
        temp[1] = str[i + 2];
        decoded += (char)strtol(temp, NULL, 16);
        i += 2;
      }
    } else if (str[i] == '+') {
      decoded += ' ';
    } else {
      decoded += str[i];
    }
  }
  return decoded;
}

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

bool LlmClient::voiceChat(const uint8_t* wavData, size_t wavSize, String& transcript, String& reply, String& emotionStr, VoiceManager& voice) {
  if (!AppConfig::Feature::LLM_ENABLED) return false;
  if (wavData == nullptr || wavSize == 0) return false;

  _busy = true;
  bool ok = postGatewayVoice(wavData, wavSize, transcript, reply, emotionStr, voice);
  if (ok) {
    _reply = reply;
    _hasReply = true;
  }
  _busy = false;
  return ok;
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

bool LlmClient::postGatewayVoice(const uint8_t* wavData, size_t wavSize, String& transcript, String& reply, String& emotionStr, VoiceManager& voice) {
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
  uint8_t* body = (uint8_t*)malloc(bodySize);
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
  http.setConnectTimeout(15000); // 15 秒连接超时保护，适配 4G 插卡网络握手
  http.setTimeout(30000);       // 30 秒数据接收超时保护，给大模型 ASR+LLM+TTS 合成预留充足的时间

  const char* headers[] = { "X-AdPet-Transcript", "X-AdPet-Reply", "X-AdPet-Emotion" };
  http.collectHeaders(headers, 3);
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  if (cfg.gatewayApiKey.length() > 0) {
    http.addHeader("Authorization", "Bearer " + cfg.gatewayApiKey);
  }
  http.addHeader("X-AdPet-Device-Id", cfg.deviceId);

  Serial.println("[Gateway] POST /adpet/chat");
  int code = http.POST(body, bodySize);
  free(body);
  Serial.print("[Gateway] HTTP ");
  Serial.println(code);
  if (code < 200 || code >= 300) {
    Serial.println(http.getString());
    http.end();
    return false;
  }

  transcript = urlDecode(http.header("X-AdPet-Transcript"));
  reply = urlDecode(http.header("X-AdPet-Reply"));
  emotionStr = http.header("X-AdPet-Emotion");

  int len = http.getSize();
  if (len <= 44 || len > 350000) {
    Serial.print("[Gateway] unsupported audio length: ");
    Serial.println(len);
    http.end();
    return reply.length() > 0;
  }

  auto* stream = http.getStreamPtr();
  bool playOk = false;
  if (stream != nullptr) {
    Serial.println("[Gateway] starting real-time TTS audio stream play...");
    playOk = voice.playWavFromStream(stream, len);
    Serial.print("[Gateway] stream play completed, status=");
    Serial.println(playOk);
  } else {
    Serial.println("[Gateway] stream is null!");
  }
  http.end();

  return playOk;
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
  http.setConnectTimeout(10000); // 10 秒连接超时保护，适配 4G 弱网握手
  http.setTimeout(20000);       // 20 秒数据接收超时保护，给大模型文本计算预留充足时间

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
