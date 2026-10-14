# Changelog

## [1.0.0] - 2026-10-14

- First public source snapshot of the passive ESP32 / ESPHome I²C sniffer.
- Raw GPIO edge capture with microsecond timestamps and burst grouping.
- START/STOP, 7-bit address, READ/WRITE, bytes and ACK/NACK decoding.
- Diagnostics for duplicate interrupts, incomplete frames and buffer overflow.
- Optional interpretation hints for selected temperature/humidity sensors.
- Generic configuration plus optional VEVOR + DS18B20 example.
- Sanitized configuration: no actual Wi-Fi details, secrets, fixed LAN IPs or packet captures.

## Next

- Hardware-assisted capture, export files, configurable filtering and improved decoder tests.