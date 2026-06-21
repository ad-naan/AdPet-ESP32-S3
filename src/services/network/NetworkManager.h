#pragma once

#include <Arduino.h>
#include <WebServer.h>

class AdPetNetworkManager {
public:
  void begin();
  void update();
  bool isConnected() const;
  bool hasChatRequest() const;
  String takeChatRequest();
  void setLastReply(const String& reply);

private:
  WebServer _server = WebServer(80);
  bool _connected = false;
  String _pendingChat;
  String _lastReply;

  void startAccessPoint();
  void connectStation();
  void setupRoutes();
  void handleRoot();
  void handleSave();
  void handleChat();
  void handleStatus();
  String htmlEscape(const String& value) const;
};
