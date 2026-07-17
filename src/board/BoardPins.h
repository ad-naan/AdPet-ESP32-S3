#pragma once

#include <stdint.h>

// AdPet hardware wiring lives in this file only.
// Change the GPIO numbers here when the enclosure layout changes.
// Power rails are not GPIOs:
//   OLED VCC and INMP441 VDD -> 3V3
//   MAX98357A VIN             -> USB 5V
//   All module GND pins       -> common GND
//   INMP441 L/R               -> GND (left channel)
//
// ESP32-S3 notes:
// - Avoid GPIO0, GPIO45, and GPIO46 unless you understand their boot roles.
// - GPIO19/GPIO20 are normally used by native USB D-/D+.
// - Flash/PSRAM pins depend on the exact SuperMini module; only use pins that
//   are exposed by its pinout.
// - Keep every active signal unique. The static_assert below checks this.

namespace BoardPins {
  namespace Oled {
    constexpr uint8_t SDA = 8;
    constexpr uint8_t SCL = 9;
    constexpr uint8_t I2C_ADDRESS = 0x3C;
  }

  namespace Microphone {
    constexpr uint8_t BCLK = 4;   // INMP441 SCK/BCLK
    constexpr uint8_t LRCLK = 5;  // INMP441 WS/LRCLK
    constexpr uint8_t DATA = 6;   // INMP441 SD/DOUT -> ESP32 input
  }

  namespace Speaker {
    constexpr uint8_t BCLK = 10;   // MAX98357A BCLK
    constexpr uint8_t LRCLK = 11;  // MAX98357A LRC
    constexpr uint8_t DATA = 12;   // ESP32 output -> MAX98357A DIN
  }

  namespace FutureInput {
    // Reserved for future enclosure sensors. They are not used yet.
    constexpr uint8_t LEFT = 13;
    constexpr uint8_t RIGHT = 14;
  }

  namespace Validation {
    template<uint8_t Needle, uint8_t... Values>
    struct NotContains;

    template<uint8_t Needle>
    struct NotContains<Needle> {
      static constexpr bool value = true;
    };

    template<uint8_t Needle, uint8_t First, uint8_t... Rest>
    struct NotContains<Needle, First, Rest...> {
      static constexpr bool value = Needle != First && NotContains<Needle, Rest...>::value;
    };

    template<uint8_t... Pins>
    struct AllUnique;

    template<>
    struct AllUnique<> {
      static constexpr bool value = true;
    };

    template<uint8_t First, uint8_t... Rest>
    struct AllUnique<First, Rest...> {
      static constexpr bool value = NotContains<First, Rest...>::value && AllUnique<Rest...>::value;
    };
  }

  static_assert(
    Validation::AllUnique<
      Oled::SDA,
      Oled::SCL,
      Microphone::BCLK,
      Microphone::LRCLK,
      Microphone::DATA,
      Speaker::BCLK,
      Speaker::LRCLK,
      Speaker::DATA,
      FutureInput::LEFT,
      FutureInput::RIGHT
    >::value,
    "BoardPins.h contains duplicate GPIO assignments"
  );
}
