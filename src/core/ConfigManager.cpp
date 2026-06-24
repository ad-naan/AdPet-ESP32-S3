#include "ConfigManager.h"
#include <Preferences.h>

ConfigManager AppConfigStore;

namespace {
  Preferences preferences;
  const char* NAMESPACE = "adpet";
}

void ConfigManager::begin() {
  applyDefaults();
  preferences.begin(NAMESPACE, true);
  _config.wifiSsid = preferences.getString("wifiSsid", _config.wifiSsid);
  _config.wifiPassword = preferences.getString("wifiPass", _config.wifiPassword);
  _config.gatewayBaseUrl = preferences.getString("gwUrl", _config.gatewayBaseUrl);
  _config.gatewayApiKey = preferences.getString("gwKey", _config.gatewayApiKey);
  _config.deviceId = preferences.getString("deviceId", _config.deviceId);
  _config.systemPrompt = preferences.getString("sysPrompt", _config.systemPrompt);
  _config.idleEmotion = preferences.getString("idleEmo", _config.idleEmotion);
  _config.triggerEmotion = preferences.getString("trigEmo", _config.triggerEmotion);
  _config.replyEmotion = preferences.getString("replyEmo", _config.replyEmotion);
  _config.recordMs = preferences.getUShort("recordMs", _config.recordMs);
  _config.triggerRms = preferences.getUInt("trigRms", _config.triggerRms);
  _config.triggerPeak = preferences.getUInt("trigPeak", _config.triggerPeak);
  
  if (_config.gatewayBaseUrl == "http://192.168.1.100:8787" || _config.gatewayBaseUrl.length() == 0) {
    _config.gatewayBaseUrl = "https://pet.adnaan.site";
  }

  // 深度优化：防呆机制。将自愈触发底线调整至 rms=350 peak=700，兼顾过滤底噪与极佳的说话灵敏度
  if (_config.triggerRms < 100 || _config.triggerRms > 5000) {
    _config.triggerRms = 350;
  }
  if (_config.triggerPeak < 200 || _config.triggerPeak > 10000) {
    _config.triggerPeak = 700;
  }
  
  preferences.end();
}

const RuntimeConfig& ConfigManager::get() const {
  return _config;
}

RuntimeConfig& ConfigManager::edit() {
  return _config;
}

void ConfigManager::save() {
  preferences.begin(NAMESPACE, false);
  preferences.putString("wifiSsid", _config.wifiSsid);
  preferences.putString("wifiPass", _config.wifiPassword);
  preferences.putString("gwUrl", _config.gatewayBaseUrl);
  preferences.putString("gwKey", _config.gatewayApiKey);
  preferences.putString("deviceId", _config.deviceId);
  preferences.putString("sysPrompt", _config.systemPrompt);
  preferences.putString("idleEmo", _config.idleEmotion);
  preferences.putString("trigEmo", _config.triggerEmotion);
  preferences.putString("replyEmo", _config.replyEmotion);
  preferences.putUShort("recordMs", _config.recordMs);
  preferences.putUInt("trigRms", _config.triggerRms);
  preferences.putUInt("trigPeak", _config.triggerPeak);
  preferences.end();
}

void ConfigManager::resetDefaults() {
  preferences.begin(NAMESPACE, false);
  preferences.clear();
  preferences.end();
  applyDefaults();
}

void ConfigManager::applyDefaults() {
  _config.wifiSsid = "";
  _config.wifiPassword = "";
  _config.gatewayBaseUrl = "https://pet.adnaan.site/";
  _config.gatewayApiKey = "";
  _config.deviceId = "adpet-001";
  _config.systemPrompt = "You are AdPet, a cute tiny desktop pet living inside a small OLED screen. Reply briefly and warmly.";
  _config.idleEmotion = "idle";
  _config.triggerEmotion = "surprised";
  _config.replyEmotion = "talking";
  _config.recordMs = 2000;
  _config.triggerRms = 650;
  _config.triggerPeak = 1200;
}
