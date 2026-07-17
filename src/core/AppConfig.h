#pragma once

#include <Arduino.h>
#include "../board/BoardPins.h"

namespace AppConfig {
  const unsigned long LOOP_DELAY_MS = 20;
  const unsigned long DEMO_EMOTION_INTERVAL_MS = 4000;
  const unsigned long IDLE_BLINK_INTERVAL_MS = 3500;
  const unsigned long BLINK_DURATION_MS = 150;

  namespace SerialPort {
    const uint32_t BAUD_RATE = 115200;
  }

  namespace Display {
    const uint8_t SDA_PIN = BoardPins::Oled::SDA;
    const uint8_t SCL_PIN = BoardPins::Oled::SCL;
    const uint8_t I2C_ADDRESS = BoardPins::Oled::I2C_ADDRESS;
  }

  namespace Audio {
    const uint8_t MIC_BCLK_PIN = BoardPins::Microphone::BCLK;
    const uint8_t MIC_LRCLK_PIN = BoardPins::Microphone::LRCLK;
    const uint8_t MIC_DATA_PIN = BoardPins::Microphone::DATA;
    const uint8_t SPEAKER_BCLK_PIN = BoardPins::Speaker::BCLK;
    const uint8_t SPEAKER_LRCLK_PIN = BoardPins::Speaker::LRCLK;
    const uint8_t SPEAKER_DATA_PIN = BoardPins::Speaker::DATA;
    const uint32_t SAMPLE_RATE = 16000;
    const uint16_t TEST_TONE_HZ = 880;
    const uint32_t SOUND_TRIGGER_RMS = 650;
    const uint32_t SOUND_TRIGGER_PEAK = 1200;
    const unsigned long SOUND_TRIGGER_COOLDOWN_MS = 1500;
    const uint16_t PRE_ROLL_MS = 300;
    const uint16_t TRIGGER_CONFIRM_MS = 80;
    const uint16_t END_SILENCE_MS = 700;
    const uint16_t MIN_SPEECH_MS = 350;
    const uint16_t POST_PLAYBACK_GUARD_MS = 700;
  }

  namespace Input {
    const uint8_t TOUCH_LEFT_PIN = BoardPins::FutureInput::LEFT;
    const uint8_t TOUCH_RIGHT_PIN = BoardPins::FutureInput::RIGHT;
  }

  namespace Feature {
    const bool AUDIO_TEST_MODE = false;
    const bool AUDIO_TEST_MIC = true;
    const bool AUDIO_TEST_SPEAKER = true;
    const bool AUDIO_TEST_DISPLAY = true;
    const bool NETWORK_ENABLED = true;
    const bool VOICE_ENABLED = true;
    const bool LLM_ENABLED = true;
    const bool DEMO_EMOTIONS_ENABLED = false;
  }
}
