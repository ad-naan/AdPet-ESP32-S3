# Wiring

## Current Hardware

| OLED | ESP32-S3 SuperMini |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 8 |
| SCL | GPIO 9 |

Display assumptions:

| Item | Value |
| --- | --- |
| Driver | SSD1306 |
| Resolution | 128x64 |
| Interface | I2C |
| Address | `0x3C` |

## Planned Expansion Pins

The shell is small and has no button holes, so the interaction plan avoids physical push buttons.

| Module | Signal | ESP32-S3 SuperMini |
| --- | --- | --- |
| INMP441 mic | VDD | 3V3 |
| INMP441 mic | GND | GND |
| INMP441 mic | SCK/BCLK | GPIO 4 |
| INMP441 mic | WS/LRCLK | GPIO 5 |
| INMP441 mic | SD/DOUT | GPIO 6 |
| INMP441 mic | L/R | GND |
| MAX98357A amp | VIN | 5V |
| MAX98357A amp | GND | GND |
| MAX98357A amp | BCLK | GPIO 10 |
| MAX98357A amp | LRC | GPIO 11 |
| MAX98357A amp | DIN | GPIO 12 |
| Hidden touch pad left | Copper foil pad | GPIO 10 |
| Hidden touch pad right | Copper foil pad | GPIO 11 |

The INMP441 and MAX98357A share BCLK and LRCLK. The mic uses `GPIO 6` as audio input data, and the amplifier uses `GPIO 7` as audio output data.

Power is supplied by USB. No battery charging or battery voltage detection is planned for the current version.

## Shell Notes

For a small shell, prefer:

1. Copper foil touch pads glued inside the shell instead of physical buttons.
2. A small I2S microphone placed near a tiny sound hole or grille.
3. A thin speaker with a sound outlet or a small resonance cavity.
4. Short wires and stacked modules only during prototyping; final assembly should use a small perfboard or custom PCB.

Avoid:

1. Tactile buttons that require holes.
2. Large TTP223 touch modules unless there is enough internal space.
3. Large round speakers if the shell has no room for an acoustic cavity.
4. Battery and charging modules in the first version.

## Diagrams

Use the AI prompt from the design notes to generate a clean final wiring diagram.

## Troubleshooting

If the screen does not light up, first check:

1. `SDA_PIN` and `SCL_PIN` in `src/core/AppConfig.h`.
2. OLED address, usually `0x3C` or `0x3D`.
3. Whether the board selected in Arduino IDE is `ESP32S3 Dev Module`.
