# Installation / instalacja

## Requirements

- Classic ESP32-WROOM-32 or ESP32-WROOM-32U development board (ESP32-C3/S2/S3 are **not supported by this implementation**).
- ESPHome configured for ESP-IDF.
- Two free digital input pins with GPIO numbers less than 32, plus common ground.
- A target I²C bus with **3.3 V-compatible** logic levels.

## Setup

1. Copy `config/esp32-i2c-sniffer.yaml` to your ESPHome config directory.
2. Copy `components/i2c_sniffer/` to `<esphome_config>/components/i2c_sniffer/`.
3. Copy `secrets.yaml.example` to `secrets.yaml` (same config directory) and edit your own credentials. Keep it private.
4. Verify the correct board/pin assignment on your **specific carrier PCB**. On ESP32 use GPIO numbers, not connector ordinal numbers.
5. Test disconnected inputs: with internal pull-ups enabled, DIAG should report `SCL=1 SDA=1` and increments of `+0`.
6. With the target powered down, connect its GND to ESP32 GND, SCL to GPIO4 and SDA to GPIO5. Then power up and inspect ESPHome logs.
7. For a busy bus use `raw_edges: false` to reduce log traffic.

## Wiring and safety

| Bus | Target | ESP32 sample |
| --- | --- | --- |
| SCL | clock line | GPIO4 |
| SDA | data line | GPIO5 |
| GND | ground | GND |

- **Do not connect 5 V logic directly** to ESP32 pins. Verify idle line levels against GND before connection.
- Sniffer is intended to be high impedance; internal pull-ups can affect effective bus pull-up resistance. Consider `pullup: false` on a healthy, already biased 3.3 V bus.
- Never wire the original VCC into a random ESP32 GPIO. Power ESP32 from its own suitable supply/USB.
- Do not use GPIO6–11 (on-module Flash) and avoid serial/strapping pins unless you understand consequences.
- Do not add `i2c:` ESPHome to the same pins: master/slave I²C configuration could drive lines, undermining passive sniffing.

## Optional example

`examples/vevor-with-ds18b20.yaml` includes an **independent GPIO27 1-Wire ROM scanner**, originally used in the VEVOR diagnostics. It does not replace I²C decoding. Its code is provided for continuity, not as a required sniffer dependency.

## ESPHome validation

Run `esphome config esp32-i2c-sniffer.yaml` and `esphome compile esp32-i2c-sniffer.yaml` within an ESPHome environment after copying the component directory next to the YAML. No local secrets are included in this repository.