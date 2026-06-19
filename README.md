# AdPet

ESP32-S3 SuperMini + I2C OLED desktop pet framework for Arduino IDE.

## Hardware

Default wiring:

| OLED | ESP32-S3 SuperMini |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 8 |
| SCL | GPIO 9 |

The code assumes a 128x64 SSD1306 I2C OLED at address `0x3C`.

If your board wiring is different, edit `src/core/AppConfig.h`.

## Arduino IDE Setup

1. Install Arduino IDE 2 from the official Arduino website.
2. Open Arduino IDE.
3. Go to `File > Preferences`.
4. In `Additional boards manager URLs`, add:

```text
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

5. Open `Tools > Board > Boards Manager`.
6. Search `esp32`, then install `esp32 by Espressif Systems`.
7. Open `Sketch > Include Library > Manage Libraries`.
8. Search `U8g2`, then install `U8g2 by olikraus`.
9. Select board: `Tools > Board > esp32 > ESP32S3 Dev Module`.
10. Select the correct serial port in `Tools > Port`.
11. Recommended options:
    - `USB CDC On Boot`: `Enabled`
    - `Upload Mode`: `UART`, if available
    - `Upload Speed`: `921600`, or lower it to `115200` if upload fails

## Upload

Open `AdPet.ino` in Arduino IDE, then click Upload.

The demo cycles through several expressions and blinks while idle.

## Project Structure

Arduino IDE supports sketch source files under the `src` folder. Keep the whole `AdPet` folder together.

| File | Responsibility |
| --- | --- |
| `AdPet.ino` | Arduino entry point only: calls app setup and loop. |
| `src/app` | Top-level scheduler and module coordination. |
| `src/core` | Pins, shared types, and pet state machine. |
| `src/drivers/display` | OLED setup and all face drawing. |
| `src/drivers/audio` | Placeholder for mic, wake word, STT, speaker/TTS. |
| `src/services/network` | Placeholder for Wi-Fi and web config portal. |
| `src/services/llm` | Placeholder for LLM HTTP calls and response parsing. |
| `docs` | Wiring and architecture notes. |
| `assets/faces` | Future bitmap face animation frames. |

## Current Architecture

```text
AdPet.ino
  -> src/app/AppController
       -> src/core/PetBrain
       -> src/drivers/display/DisplayManager
       -> src/drivers/audio/VoiceManager
       -> src/services/network/NetworkManager
       -> src/services/llm/LlmClient
```

The current version intentionally keeps network, voice, and LLM disabled in `src/core/AppConfig.h`.
This lets the display and state-machine framework run first, then new hardware can be added one module at a time.

## Audio Hardware Test

`AUDIO_TEST_MODE` is currently enabled in `src/core/AppConfig.h`.
In this mode the OLED is not initialized.

Default test target:

```cpp
const bool AUDIO_TEST_MIC = true;
const bool AUDIO_TEST_SPEAKER = true;
const bool AUDIO_TEST_DISPLAY = true;
```

This runs the whole local hardware loop. Open Serial Monitor at `115200` baud and look for lines like:

```text
[AudioTest] mic rms=123 peak=456
```

Speak or clap near the mic. When the sound is loud enough, the OLED changes expression and the MAX98357A plays a beep:

```text
[AudioTest] sound trigger -> display reaction + beep
[AudioTest] beep
```

After the mic and speaker are verified, set `AUDIO_TEST_MODE` to `false`.

## Suggested Next Layers

1. Add button/touch input and map it to emotions.
2. Add Wi-Fi credentials and a small web config page in `NetworkManager`.
3. Add voice input/output hardware in `VoiceManager`.
4. Add HTTP LLM calls in `LlmClient`.
5. Add mood, sleepiness, hunger, and interaction cooldowns in `PetBrain`.
6. Replace procedural faces with bitmap animation frames in `DisplayManager`.
