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
    const uint8_t MIC_BCLK_PIN = 4;
    const uint8_t MIC_LRCLK_PIN = 5;
    const uint8_t MIC_DATA_PIN = 6;
    const uint8_t SPEAKER_BCLK_PIN = 10;
    const uint8_t SPEAKER_LRCLK_PIN = 11;
    const uint8_t SPEAKER_DATA_PIN = 12;
    const uint32_t SAMPLE_RATE = 16000;
    const uint16_t TEST_TONE_HZ = 880;
    const uint32_t SOUND_TRIGGER_RMS = 650;
    const uint32_t SOUND_TRIGGER_PEAK = 1200;
    const unsigned long SOUND_TRIGGER_COOLDOWN_MS = 1500;
  }

  namespace Input {
    const uint8_t TOUCH_LEFT_PIN = 13;
    const uint8_t TOUCH_RIGHT_PIN = 14;
  }

  namespace Feature {
    const bool AUDIO_TEST_MODE = false;
    const bool AUDIO_TEST_MIC = true;
    const bool AUDIO_TEST_SPEAKER = true;
    const bool AUDIO_TEST_DISPLAY = true;
    const bool NETWORK_ENABLED = true;
    const bool VOICE_ENABLED = true;
    const bool LLM_ENABLED = true;
  }
}
