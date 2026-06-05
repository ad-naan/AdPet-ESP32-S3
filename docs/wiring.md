# Wiring

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

These pins are reserved for future modules. Do not use them for other hardware unless the plan changes.

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
| MAX98357A amp | BCLK | GPIO 4 |
| MAX98357A amp | LRC | GPIO 5 |
| MAX98357A amp | DIN | GPIO 7 |
| Button A | Signal | GPIO 15 |
| Button B | Signal | GPIO 16 |

The INMP441 and MAX98357A share BCLK and LRCLK. The mic uses `GPIO 6` as audio input data, and the amplifier uses `GPIO 7` as audio output data.
Power is supplied by USB. No battery charging or battery voltage detection is planned for the current version.

## Diagram

Open `docs/wiring.svg` for the visual wiring diagram.

## Troubleshooting

If the screen does not light up, first check:

1. `SDA_PIN` and `SCL_PIN` in `src/core/AppConfig.h`.
2. OLED address, usually `0x3C` or `0x3D`.
3. Whether the board selected in Arduino IDE is `ESP32S3 Dev Module`.
