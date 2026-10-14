# Limitations and planned improvements

## Capture

This version relies on GPIO interrupts and software timestamp sampling. ISR/Wi-Fi contention can miss rapid transitions. The number of actual bus transitions may differ from the event buffer, and a frame can look syntactically valid even after missed edges.

- Developed for classic ESP32 GPIO numbers 0–31; physical port, Flash and strapping constraints still apply.
- No hardware I²C frequency detection, no validated operation at 100/400 kHz.
- The RAW ring has 2048 slots and BURST buffer holds at most 2048 events. Long traffic may truncate or overflow.
- ACK/NACK is decoded from the sampled ninth bit; on `PARTIAL` frames or when `simultaneous > 0`, interpret cautiously.
- Logs can be long (recording and printing one burst can temporarily slow the ESPHome main loop). Set `raw_edges: false` for routine monitoring.
- Only selected sensor commands receive descriptive hints; all other data remains in HEX.
- The ESP32 GPIOs are not 5 V tolerant. Do not attach to unverified 5 V logic.
- Not designed to sniff multi-master arbitration with reliability, I²C high-speed mode or buses using clock stretching under difficult timings.

## Possible enhancements

- Export captures in VCD/CSV format to PulseView-compatible workflows.
- Hardware-assisted capture (RMT / other capture peripheral) for more reliable timing.
- Built-in CRC verification for SHT3x/SHT4x data.
- Optional frame filtering and statistics exposed as Home Assistant diagnostic entities.
- Improved recovery after missing edges and unit/integration tests with real ESPHome toolchains.