# Component Selection

The shell is small, has no button holes, and uses USB power only. The component plan should prioritize small size, low wiring complexity, and no external controls that need openings.

## Recommended Core Components

| Function | Recommended Part | Why |
| --- | --- | --- |
| Main control | ESP32-S3 SuperMini | Already available, small, Wi-Fi capable, supports I2C and I2S. |
| Display | SSD1306 128x64 I2C OLED | Already available, simple wiring, good for facial expressions. |
| Hidden touch | Copper foil tape inside shell | No hole needed, uses ESP32-S3 capacitive touch GPIO directly. |
| Microphone | INMP441 I2S mic module | Small digital microphone, cleaner than analog mic modules. |
| Speaker amp | MAX98357A I2S mono amp | Simple I2S output, can drive a small speaker directly. |
| Speaker | Thin 8 ohm speaker, 0.5W to 1W | Smaller and safer for USB-powered compact shells than large 3W speakers. |
| Motion sensing | QMI8658 or MPU6050 module | Optional; enables shake, pickup, tilt, and tap interactions without holes. |

## Interaction Strategy

Use these input methods instead of push buttons:

1. Hidden left/right capacitive touch pads.
2. Motion gestures from an IMU.
3. Voice wake or push-to-talk through web UI.
4. Web control page over Wi-Fi.

## Size-Conscious Notes

1. Use copper foil pads instead of TTP223 modules when possible.
2. If using a mic, the shell should have a tiny acoustic opening near the microphone port.
3. If using a speaker, leave an outlet or grille; a sealed shell will make sound quiet and muffled.
4. For final assembly, replace loose Dupont wires with short silicone wires or a small carrier PCB.
5. Keep the first build USB-only; battery hardware adds volume, heat, charging safety work, and wiring complexity.
