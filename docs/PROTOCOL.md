# Log format / interpretacja

The component samples both GPIO levels at each ISR, queues edge events with timestamps in microseconds, and decodes the events in the normal ESPHome `loop()` context.

- `BURST`: series of events separated from the next series by `quiet_time_ms` of inactivity.
- `RAW ... +123:10`: 123 µs since first captured event, then `SCL=1`, `SDA=0`.
- `START`: SDA falls while SCL is high (as reconstructed from captured events).
- `STOP`: SDA rises while SCL is high.
- `REPEATED_START`: another START within an active frame.
- `BYTES: 88[N] FD[N]`: hexadecimal byte followed by sampled ninth-bit ACK (`[A]` means SDA low; `[N]` means SDA high).
- `ADDR=0x44 WRITE`: the first decoded byte interpreted as seven-bit target address and R/W bit.
- `PARTIAL`: frame did not finish on a clean ninth bit boundary, or a buffer limit was hit. Do not assume all decoded bytes are valid.
- `simultaneous`: both pin levels changed between successive ISR snapshots. Potential missed edges; try shorter leads, reduced bus speed or a dedicated logic analyzer.
- `duplicate_ISRs`: interrupts where neither sampled pin differed from the previous recorded state. Does not necessarily mean a fatal error.
- `lost`: ring buffer overflow; decoding may be incomplete and some data irrecoverable.

## Device-specific hints (not universal protocol rules)

The decoder retains all addresses and bytes. It prints interpretation hints only for selected well-known commands:

| Address | Bytes | Example hint |
| --- | --- | --- |
| `0x44`, `0x45` | `FD` | SHT4x high precision measurement |
| `0x44`, `0x45` | `F6` | SHT4x medium precision measurement |
| `0x44`, `0x45` | `E0` | SHT4x low precision measurement |
| `0x44`, `0x45` | `24 00` | SHT3x high repeatability no clock stretch |
| `0x44`, `0x45` | `2C 06` | SHT3x high repeatability clock stretch |
| `0x70` | `35 17` | SHTC3 wake-up |
| `0x38` | `AC` | AHT20-style measurement |

A matching hint is **not proof** of the connected sensor model. With a missing sensor, the master may continue clocking even after NACK; the sniffer can display bytes that are never acknowledged by a slave. CRC for measurement payloads is **not validated** in this version.