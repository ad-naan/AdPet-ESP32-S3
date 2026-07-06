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

1. Upload `AdPet.ino` with Arduino IDE.
2. Open Serial Monitor at `115200`.
3. Connect your phone/computer to Wi-Fi AP `AdPet-Setup`, password `adpet1234`.
4. Open the setup page printed in Serial Monitor, usually `http://192.168.4.1`.
5. Fill Wi-Fi and Gateway settings.
6. Save config and reset the board.

Default gateway settings:

```text
Gateway Base URL: http://192.168.1.100:8787
Device ID: adpet-001
```

## Runtime

When the microphone level crosses the configured threshold:

1. OLED changes expression.
2. ESP32 records about 2 seconds of audio by default.
3. Gateway receives WAV and handles STT, LLM, TTS, and memory.
4. Gateway returns WAV audio.
6. The speaker plays the reply.

Serial Monitor prints each step, including HTTP status codes.

## Project Structure

```text
AdPet.ino
src/app              app orchestration
src/core             config, emotions, pet state
src/drivers/audio    INMP441/MAX98357A audio
src/drivers/display  SSD1306 faces
src/services/network Wi-Fi and web config
src/services/llm     AdPet Gateway client
```
