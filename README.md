# AdPet

ESP32-S3 SuperMini desktop pet using an SSD1306 OLED, INMP441 microphone, and MAX98357A speaker amp.

## Current Mode

The project now runs the gateway-based voice-chat flow:

```text
voice trigger
  -> record WAV from INMP441
  -> POST WAV to AdPet Gateway
  -> Gateway handles STT / LLM / TTS / memory
  -> play WAV through MAX98357A
  -> update OLED emotions
```

The ESP32 no longer talks directly to STT/LLM/TTS providers. See `docs/gateway.md`.

## Wiring

All GPIO assignments are centralized in `src/board/BoardPins.h`. When the enclosure layout changes, edit that file only and rewire the matching signals. Duplicate active GPIO assignments cause a compile-time error.

OLED:

| OLED | ESP32-S3 SuperMini |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO8 |
| SCL | GPIO9 |

INMP441:

| INMP441 | ESP32-S3 SuperMini |
| --- | --- |
| VDD | 3V3 |
| GND | GND |
| SCK/BCLK | GPIO4 |
| WS/LRCLK | GPIO5 |
| SD/DOUT | GPIO6 |
| L/R | GND |

MAX98357A:

| MAX98357A | ESP32-S3 SuperMini |
| --- | --- |
| VIN | 5V |
| GND | GND |
| BCLK | GPIO10 |
| LRC | GPIO11 |
| DIN | GPIO12 |
| SPK+ | Speaker + |
| SPK- | Speaker - |

## Setup

1. In Arduino IDE select `ESP32S3 Dev Module`.
2. Set the flash size to match the module marking. If the module includes PSRAM, select its matching `QSPI PSRAM` or `OPI PSRAM`; otherwise select `Disabled`.
3. Upload `AdPet.ino` with Arduino IDE.
4. Open Serial Monitor at `115200` and check the `[Memory]` line for `psram=yes/no`.
5. Connect your phone/computer to Wi-Fi AP `AdPet-Setup`, password `adpet1234`.
6. Open the setup page printed in Serial Monitor, usually `http://192.168.4.1`.
7. Fill Wi-Fi and Gateway settings.
8. Save config and reset the board.

Default gateway settings:

```text
Gateway Base URL: http://192.168.1.100:8787
Device ID: adpet-001
```

## Runtime

When the microphone level crosses the configured threshold:

1. OLED changes to the configured listening expression.
2. ESP32 includes about 300 ms of pre-roll audio so the first word is not cut off.
3. Recording ends after about 700 ms of silence or at the configured maximum duration.
4. Gateway receives WAV and handles STT, LLM, TTS, and memory.
5. Gateway returns WAV audio and an emotion name.
6. The speaker plays the reply, then microphone triggering resumes after a short guard period.

Serial Monitor prints each step, including HTTP status codes.

Without PSRAM, recording is capped at 3000 ms to preserve memory for Wi-Fi and HTTP. With PSRAM enabled, recordings, HTTP request bodies, and TTS WAV data prefer PSRAM.

## Project Structure

```text
AdPet.ino
src/app              app orchestration
src/board            all hardware GPIO assignments
src/core             config, emotions, pet state
src/drivers/audio    INMP441/MAX98357A audio
src/drivers/display  SSD1306 faces
src/services/network Wi-Fi and web config
src/services/llm     AdPet Gateway client
```
