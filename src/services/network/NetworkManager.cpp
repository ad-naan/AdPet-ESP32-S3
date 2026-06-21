#include "NetworkManager.h"
#include "../../core/AppConfig.h"
#include "../../core/ConfigManager.h"
#include <WiFi.h>

void AdPetNetworkManager::begin() {
  if (!AppConfig::Feature::NETWORK_ENABLED) {
    Serial.println("[Network] disabled");
    return;
  }

  WiFi.mode(WIFI_AP_STA);
  startAccessPoint();
  connectStation();
  setupRoutes();
  _server.begin();
  Serial.println("[Network] web config server started");
}

void AdPetNetworkManager::update() {
  if (AppConfig::Feature::NETWORK_ENABLED) {
    _server.handleClient();
  }
}

bool AdPetNetworkManager::isConnected() const {
  return _connected;
}

bool AdPetNetworkManager::hasChatRequest() const {
  return _pendingChat.length() > 0;
}

String AdPetNetworkManager::takeChatRequest() {
  String text = _pendingChat;
  _pendingChat = "";
  return text;
}

void AdPetNetworkManager::setLastReply(const String& reply) {
  _lastReply = reply;
}

void AdPetNetworkManager::startAccessPoint() {
  WiFi.softAP("AdPet-Setup", "adpet1234");
  Serial.print("[Network] setup AP: http://");
  Serial.println(WiFi.softAPIP());
}

void AdPetNetworkManager::connectStation() {
  const RuntimeConfig& config = AppConfigStore.get();
  if (config.wifiSsid.length() == 0) {
    Serial.println("[Network] Wi-Fi SSID empty, AP-only mode");
    _connected = false;
    return;
  }

  Serial.print("[Network] connecting Wi-Fi: ");
  Serial.println(config.wifiSsid);
  WiFi.begin(config.wifiSsid.c_str(), config.wifiPassword.c_str());

  unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < 8000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  _connected = WiFi.status() == WL_CONNECTED;
  if (_connected) {
    Serial.print("[Network] STA IP: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[Network] Wi-Fi connect failed, AP still available");
  }
}

void AdPetNetworkManager::setupRoutes() {
  _server.on("/", HTTP_GET, [this]() { handleRoot(); });
  _server.on("/save", HTTP_POST, [this]() { handleSave(); });
  _server.on("/chat", HTTP_POST, [this]() { handleChat(); });
  _server.on("/api/status", HTTP_GET, [this]() { handleStatus(); });
}

void AdPetNetworkManager::handleRoot() {
  const RuntimeConfig& c = AppConfigStore.get();
  String page;
  page.reserve(10000);
  page += "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
  page += "<title>AdPet</title><style>body{font-family:Arial,sans-serif;margin:18px;line-height:1.4;background:#f6f7f9;color:#111827}main{max-width:760px;margin:auto}.card{background:white;border:1px solid #d1d5db;border-radius:8px;padding:14px;margin:12px 0}label{display:block;font-weight:700;margin-top:10px}input,textarea{box-sizing:border-box;width:100%;padding:9px;margin-top:4px;border:1px solid #cbd5e1;border-radius:6px}textarea{min-height:86px}button{padding:10px 14px;border:0;border-radius:6px;background:#2563eb;color:white;font-weight:700;margin-top:12px}.hint{color:#4b5563;font-size:14px}.reply{white-space:pre-wrap;background:#111827;color:#f8fafc;border-radius:6px;padding:10px}</style></head><body><main>";
  page += "<h1>AdPet Config</h1><p class='hint'>AP: AdPet-Setup / adpet1234. After Wi-Fi is saved, check Serial Monitor for the LAN IP. Voice chat is handled by your AdPet Gateway.</p>";
  page += "<div class='card'><form method='post' action='/save'>";
  page += "<h2>Wi-Fi</h2><label>SSID</label><input name='wifiSsid' value='" + htmlEscape(c.wifiSsid) + "'>";
  page += "<label>Password</label><input name='wifiPassword' type='password' value='" + htmlEscape(c.wifiPassword) + "'>";
  page += "<h2>Gateway</h2><label>Base URL</label><input name='gatewayBaseUrl' value='" + htmlEscape(c.gatewayBaseUrl) + "'>";
  page += "<label>Gateway API Key</label><input name='gatewayApiKey' type='password' value='" + htmlEscape(c.gatewayApiKey) + "'>";
  page += "<label>Device ID</label><input name='deviceId' value='" + htmlEscape(c.deviceId) + "'>";
  page += "<label>System Prompt</label><textarea name='systemPrompt'>" + htmlEscape(c.systemPrompt) + "</textarea>";
  page += "<h2>Voice Trigger</h2><label>Record ms</label><input name='recordMs' type='number' value='" + String(c.recordMs) + "'>";
  page += "<label>Trigger RMS</label><input name='triggerRms' type='number' value='" + String(c.triggerRms) + "'>";
  page += "<label>Trigger Peak</label><input name='triggerPeak' type='number' value='" + String(c.triggerPeak) + "'>";
  page += "<h2>Emotions</h2><label>Idle emotion</label><input name='idleEmotion' value='" + htmlEscape(c.idleEmotion) + "'>";
  page += "<label>Trigger emotion</label><input name='triggerEmotion' value='" + htmlEscape(c.triggerEmotion) + "'>";
  page += "<label>Reply emotion</label><input name='replyEmotion' value='" + htmlEscape(c.replyEmotion) + "'>";
  page += "<button type='submit'>Save Config</button></form></div>";
  page += "<div class='card'><h2>Text Chat Test</h2><form method='post' action='/chat'><label>Message</label><textarea name='message'>Hello, introduce yourself briefly.</textarea><button type='submit'>Send</button></form>";
  page += "<h3>Last Reply</h3><div class='reply'>" + htmlEscape(_lastReply) + "</div></div>";
  page += "</main></body></html>";
  _server.send(200, "text/html; charset=utf-8", page);
}

void AdPetNetworkManager::handleSave() {
  RuntimeConfig& c = AppConfigStore.edit();
  c.wifiSsid = _server.arg("wifiSsid");
  c.wifiPassword = _server.arg("wifiPassword");
  c.gatewayBaseUrl = _server.arg("gatewayBaseUrl");
  c.gatewayApiKey = _server.arg("gatewayApiKey");
  c.deviceId = _server.arg("deviceId");
  c.systemPrompt = _server.arg("systemPrompt");
  c.idleEmotion = _server.arg("idleEmotion");
  c.triggerEmotion = _server.arg("triggerEmotion");
  c.replyEmotion = _server.arg("replyEmotion");
  long recordMs = _server.arg("recordMs").toInt();
  long triggerRms = _server.arg("triggerRms").toInt();
  long triggerPeak = _server.arg("triggerPeak").toInt();

  if (recordMs < 1000) recordMs = 1000;
  if (recordMs > 5000) recordMs = 5000;
  if (triggerRms < 1) triggerRms = 1;
  if (triggerPeak < 1) triggerPeak = 1;

  c.recordMs = (uint16_t)recordMs;
  c.triggerRms = (uint32_t)triggerRms;
  c.triggerPeak = (uint32_t)triggerPeak;
  AppConfigStore.save();
  _server.sendHeader("Location", "/");
  _server.send(303);
  Serial.println("[Network] config saved");
}

void AdPetNetworkManager::handleChat() {
  _pendingChat = _server.arg("message");
  _server.sendHeader("Location", "/");
  _server.send(303);
  Serial.print("[Network] chat request: ");
  Serial.println(_pendingChat);
}

void AdPetNetworkManager::handleStatus() {
  String json = "{\"connected\":";
  json += _connected ? "true" : "false";
  json += ",\"apIp\":\"";
  json += WiFi.softAPIP().toString();
  json += "\",\"staIp\":\"";
  json += WiFi.localIP().toString();
  json += "\"}";
  _server.send(200, "application/json", json);
}

String AdPetNetworkManager::htmlEscape(const String& value) const {
  String out;
  out.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); i++) {
    char ch = value[i];
    if (ch == '&') out += "&amp;";
    else if (ch == '<') out += "&lt;";
    else if (ch == '>') out += "&gt;";
    else if (ch == '"') out += "&quot;";
    else out += ch;
  }
  return out;
}
