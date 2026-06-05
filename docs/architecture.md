# AdPet Architecture

AdPet is organized as an Arduino sketch with a layered `src` folder.

```text
AdPet.ino
  -> src/app/AppController
       -> src/core/PetBrain
       -> src/drivers/display/DisplayManager
       -> src/drivers/audio/VoiceManager
       -> src/services/network/NetworkManager
       -> src/services/llm/LlmClient
```

## Layers

| Layer | Purpose |
| --- | --- |
| `app` | Startup, scheduling, and module coordination. |
| `core` | Pet state, emotions, config, and logic that should not depend on specific hardware. |
| `drivers` | Direct hardware integration such as OLED, mic, speaker, keys, battery, and sensors. |
| `services` | External or high-level services such as Wi-Fi, web config, cloud APIs, and LLM calls. |

## Expansion Plan

1. Keep the display and pet state machine stable first.
2. Add input modules as drivers, for example buttons, touch, or IMU.
3. Add network setup in `services/network`.
4. Add voice input/output in `drivers/audio`.
5. Add LLM request flow in `services/llm`.
6. Let `AppController` translate module events into pet emotions and display states.
