#pragma once

#include <Arduino.h>

class NetworkManager {
public:
  void begin();
  void update();
  bool isConnected() const;

private:
  bool _connected = false;
};
