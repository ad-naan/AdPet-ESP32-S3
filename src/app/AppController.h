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
  AdPetNetworkManager _network;
  VoiceManager _voice;
  LlmClient _llm;
  bool _audioReactionActive = false;
  unsigned long _audioReactionStartMs = 0;

  void updateAudioTestMode();
  void handleVoiceToLlm();
  void handleLlmReply();
  void printMemoryStats(const char* stage);
};
