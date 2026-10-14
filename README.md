# ESP32 I²C Sniffer for ESPHome

**v1.0.0 · passive I²C bus analyzer · ESP32 + ESPHome (ESP-IDF)**

Pasywny analizator magistrali I²C oparty na klasycznym ESP32 i własnym komponencie ESPHome (`components/i2c_sniffer`). Nasłuchuje stanów SCL/SDA, zapisuje zdarzenia z czasem w mikrosekundach i dekoduje próbki na adresy, bajty oraz ACK/NACK. **Nie inicjuje transmisji i nie wysyła komend I²C.**

> [!WARNING]
> **Projekt diagnostyczny, nie profesjonalny analizator logiczny.** Rejestracja zboczy przez przerwania GPIO może gubić zmiany przy szybkiej transmisji, dużym obciążeniu Wi-Fi lub długich przewodach. Nie traktuj zdekodowanych ramek jako pewnych, jeśli widzisz `PARTIAL`, `simultaneous > 0` lub `lost > 0`.

## Funkcje

- pasywny odczyt zmian SCL i SDA (wejścia GPIO, bez generowania zegara),
- adresy 7-bit I²C oraz kierunek READ/WRITE,
- START, STOP i REPEATED START,
- surowe bajty w HEX, flagi ACK/NACK,
- opcjonalny zrzut wszystkich przechwyconych zmian `RAW` wraz z przesunięciem czasowym w µs,
- ramki niekompletne, liczniki `SCLedges`, `SDAedges`, `frames`, `partial`, `lost`, `duplicate_ISRs`,
- podpowiedzi rozpoznawanych poleceń SHT3x, SHT4x, SHTC3 oraz AHT20 (bez zmiany uniwersalnego dekodowania),
- logi ESPHome oraz opcjonalny podgląd statusu przez Home Assistant / web_server,
- przykładowa integracja z niezależnym skanerem ROM DS18B20.

## Status i testy

**v1.0.0 to pierwszy publiczny snapshot działającego kodu RAW v3.** Funkcja podsłuchu została sprawdzona w rzeczywistym układzie **ESP32-WROOM-32U + stacja VEVOR**. Sniffer wykrywał cyklicznie adres `0x44`, polecenie `0xFD` oraz próbę odczytu sześciu bajtów bez podłączonego czujnika. To odpowiada sekwencji używanej przez **SHT4x**, ale nie jest testem kompatybilności zamiennika ani dowodem poprawnego pomiaru czujnika.

W testach zdarzały się ramki `PARTIAL` i `simultaneous`. **Nie deklarujemy bezstratnego podsłuchu ani gwarantowanej obsługi 100/400 kHz.** Pełny, odtwarzalny build ESPHome w środowisku publikacji **nie był wykonany**; kod opiera się na wersji lokalnie uruchomionej i sprawdzonej logami sprzętowymi.

## Szybka instalacja

1. Skopiuj plik `config/esp32-i2c-sniffer.yaml` do głównego katalogu konfiguracji ESPHome jako `esp32-i2c-sniffer.yaml`.
2. Skopiuj obok niego cały katalog `components/i2c_sniffer/`.
3. Skopiuj `secrets.yaml.example` do **lokalnego** `secrets.yaml` i wpisz własne Wi-Fi oraz hasło OTA. Nigdy nie publikuj `secrets.yaml`.
4. Podłącz wspólną masę, SCL i SDA do urządzenia, którego magistralę chcesz podsłuchać. Upewnij się, że stany logiczne nie przekraczają 3,3 V. **Nie podłączaj sygnałów 5 V bez konwertera poziomów.**
5. Skompiluj i wgraj konfigurację przez ESPHome; otwórz logi. Na czas pierwszego uruchomienia możesz pozostawić linie I²C odłączone, z wewnętrznymi Pull-upami włączonymi w przykładzie.

Przykładowa topologia:

```text
I2C target (controller + sensor)
             SCL ───────────► ESP32 GPIO4 (input)
             SDA ───────────► ESP32 GPIO5 (input)
             GND ──────────── ESP32 GND
      Target VCC  ✕   (do not connect to ESP32 GPIO)
```

Pinout GPIO4/GPIO5 jest **przykładem**: możesz użyć innych odpowiednich GPIO klasycznego ESP32, ale nie pinów zajętych przez Flash (GPIO6–11) lub USB/UART. Unikaj bootstrapping GPIO przy podłączonych urządzeniach. Komponent obsługuje numery GPIO **poniżej 32**, a nie wszystkie piny każdego wariantu ESP32.

## Konfiguracja

```yaml
i2c_sniffer:
  id: passive_i2c
  scl_pin:
    number: GPIO4
    mode:
      input: true
      pullup: true
  sda_pin:
    number: GPIO5
    mode:
      input: true
      pullup: true
  raw_edges: true
  quiet_time_ms: 200
```

`raw_edges: false` wyłącza długie zrzuty RAW, zachowując dekodowanie. `quiet_time_ms` (50–2000 ms) wyznacza przerwę rozdzielającą grupy zdarzeń. Przy magistrali mającej własne Pull-upy rozważ ich pozostawienie jako jedynych rezystorów podciągających i ustaw `pullup: false` w ESP32.

**Nie dodawaj `i2c:` ESPHome na tych samych GPIO.** Ten komponent używa własnych przerwań, a nie kontrolera I²C ESP32.

## Przykładowy log

```text
==== BURST #1 events=189 duration=29475us start(SCL,SDA)=11 ====
  FRAME 1: ADDR=0x44 WRITE NACK | bytes=2 clocks=19 ... | STOP [PARTIAL]
    BYTES: 88[N] FD[N]
    HINT: SHT4x high precision measurement
  FRAME 2: ADDR=0x44 READ NACK | bytes=7 clocks=64 ... | STOP [PARTIAL]
    BYTES: 89[N] FF[A] FF[A] FF[A] FF[A] FF[A] FF[A]
  SUMMARY frames=2 START=2 STOP=2 outside_clocks=0 simultaneous=7
```

W tej próbie nie było czujnika pod adresem `0x44`, więc `NACK` jest oczekiwany; oznaczenia ACK przy bajtach odczytu należy traktować ostrożnie, skoro ramka miała błędy przechwycenia. Wartości dotyczą **jednej konkretnej stacji testowej**, nie stanowią wzorca dla każdego urządzenia.

## Struktura

```text
components/i2c_sniffer/
  __init__.py
  i2c_sniffer.cpp
  i2c_sniffer.h
config/
  esp32-i2c-sniffer.yaml
examples/
  vevor-with-ds18b20.yaml
docs/
  INSTALLATION.md
  PROTOCOL.md
  LIMITATIONS.md
secrets.yaml.example
.gitignore
CHANGELOG.md
VERSION
LICENSE
README.md
```

## Bezpieczeństwo danych

Repozytorium **nie zawiera** prawdziwych identyfikatorów Wi-Fi, haseł, prywatnej adresacji IP, tokenów ani danych urządzeń. `secrets.yaml`, logi, pliki capture i lokalne konfiguracje są ignorowane przez Git. Nie publikuj pełnych logów nieznanych urządzeń bez sprawdzenia ich zawartości — magistrala może przenosić dane wrażliwe.

## Dokumentacja

- [Instalacja i podłączenie](docs/INSTALLATION.md)
- [Interpretacja logów i protokołu](docs/PROTOCOL.md)
- [Ograniczenia techniczne](docs/LIMITATIONS.md)
- [Historia zmian](CHANGELOG.md)

## Licencja

Copyright (C) 2026 ImixVega and contributors.

**GNU General Public License v3.0 or later (GPL-3.0-or-later)**. Możesz używać, modyfikować i rozpowszechniać ten kod na warunkach GPL. Zobacz pełny tekst w [LICENSE](LICENSE).

`SPDX-License-Identifier: GPL-3.0-or-later`