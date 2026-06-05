#pragma once

#include <Arduino.h>
#include "../core/PetBrain.h"
#include "../drivers/audio/VoiceManager.h"
#include "../drivers/display/DisplayManager.h"
#include "../services/llm/LlmClient.h"
#include "../services/network/NetworkManager.h"

class AppController {
public:
  void begin();
  void update();

private:
  DisplayManager _display;
  PetBrain _brain;
  NetworkManager _network;
  VoiceManager _voice;
  LlmClient _llm;

  void handleVoiceToLlm();
  void handleLlmReply();
};
