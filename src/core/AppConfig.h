#pragma once

#include <Arduino.h>

namespace AppConfig {
  const unsigned long LOOP_DELAY_MS = 20;
  const unsigned long DEMO_EMOTION_INTERVAL_MS = 4000;
  const unsigned long IDLE_BLINK_INTERVAL_MS = 3500;
  const unsigned long BLINK_DURATION_MS = 150;

  namespace SerialPort {
    const uint32_t BAUD_RATE = 115200;
  }

  namespace Display {
    const uint8_t SDA_PIN = 8;
    const uint8_t SCL_PIN = 9;
    const uint8_t I2C_ADDRESS = 0x3C;
  }

  namespace Audio {
    const uint8_t I2S_BCLK_PIN = 4;
    const uint8_t I2S_LRCLK_PIN = 5;
    const uint8_t MIC_DATA_PIN = 6;
    const uint8_t SPEAKER_DATA_PIN = 7;
  }

  namespace Input {
    const uint8_t BUTTON_A_PIN = 15;
    const uint8_t BUTTON_B_PIN = 16;
  }

  namespace Feature {
    const bool NETWORK_ENABLED = false;
    const bool VOICE_ENABLED = false;
    const bool LLM_ENABLED = false;
  }
}
