#include "NetworkManager.h"
#include "../../core/AppConfig.h"

void NetworkManager::begin() {
  if (!AppConfig::Feature::NETWORK_ENABLED) {
    Serial.println("[Network] disabled");
    return;
  }

  Serial.println("[Network] TODO: Wi-Fi and web config portal");
}

void NetworkManager::update() {
}

bool NetworkManager::isConnected() const {
  return _connected;
}
