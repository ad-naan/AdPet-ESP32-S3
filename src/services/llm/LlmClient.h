#pragma once

#include <Arduino.h>

class LlmClient {
public:
  void begin();
  void update();
  bool isBusy() const;
  void ask(const String& userText);
  bool hasReply() const;
  String takeReply();

private:
  bool _busy = false;
  bool _hasReply = false;
  String _reply;
};
