#pragma once

#include <Arduino.h>

struct RuntimeConfig {
  String wifiSsid;
  String wifiPassword;

  String gatewayBaseUrl;
  String gatewayApiKey;
  String deviceId;
  String systemPrompt;

  String idleEmotion;
  String triggerEmotion;
  String replyEmotion;

  uint16_t recordMs;
  uint32_t triggerRms;
  uint32_t triggerPeak;
};

class ConfigManager {
public:
  void begin();
  const RuntimeConfig& get() const;
  RuntimeConfig& edit();
  void save();
  void resetDefaults();

private:
  RuntimeConfig _config;

  void applyDefaults();
};

extern ConfigManager AppConfigStore;
