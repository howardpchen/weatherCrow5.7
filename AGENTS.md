# Repository Guidelines

## Project Structure & Module Organization
- `weatherCrow5.7.ino` hosts the ESP32-S3 setup, Wi-Fi/API calls, and render flow for the 5.7" e-ink display.
- `config.h` holds live credentials and runtime constants; `config.example.h` is the template to copy and fill (keep the example tracked, never commit secrets).
- Display driver and waveform setup live in `EPD*.{h,cpp}`; `spi.{h,cpp}` manages SPI pins; fonts and icons are in `font*.cpp`, `fonts.h`, and `weatherIcons.h`.
- Backup or reference defaults: `weatherCrowbackup.h` and historical init sequences in `EPD_Init*`.

## Build, Flash, and Run
- Tooling: Arduino IDE 2.x or `arduino-cli` with the ESP32 board package (e.g., `esp32:esp32:esp32s3`); install ArduinoJson.
- Example compile: `arduino-cli compile --fqbn esp32:esp32:esp32s3 weatherCrow5.7.ino --output-dir build`.
- Example flash: `arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32s3 weatherCrow5.7.ino`.
- Monitor serial logs at `115200` baud; ensure the e-ink power pin (GPIO 7) is wired and battery mode matches `LOW_POWER_MODE`.
- File manipulation: If necessary use `python3` instead of `python` or `perl`.

## Configuration & Security
- Copy `config.example.h` to `config.h`, set Wi-Fi, API keys, units, coordinates, and power settings. Keep API keys out of commits; rotate if accidentally pushed.
- Timezone/refresh knobs: `REFRESH_MINUITES`, `DEFAULT_TIMEZONE_OFFSET`, `HOUR_INTERVAL`, `LOW_POWER_MODE`. Use `NIGHT_START_HOUR`/`NIGHT_END_HOUR` to suppress updates during quiet hours.
- OpenWeatherMap One Call 3.0 now requires billing info; set a per-day request cap (e.g., 900) in the OpenWeather dashboard to stay within the 1k/day free tier and avoid surprise charges/HTTP 429s.

## Coding Style & Naming Conventions
- Follow the existing Arduino-style C++: 2-space indentation, braces on the same line, camelCase for functions/variables, ALL_CAPS for constants and macros.
- Prefer `String` and `DynamicJsonDocument` patterns already used; log via `logPrint*` helpers to keep serial output consistent.

## Testing & Validation
- No automated tests; validate by flashing and observing: Wi-Fi connects, API responses succeed (HTTP 200), and the display shows current time, AQI, and temperature.
- If changing rendering/layout, trigger a full clear via `clearScreen()` and capture photos of the panel to confirm glyphs and alignment.
- For network changes, test both cold boot and wake-from-sleep; ensure sleep intervals respect `REFRESH_MINUITES` and night hours.

## Commit & Pull Request Guidelines
- Use concise messages in the style `scope: summary` (e.g., `display: adjust AQI layout`); one topic per commit.
- PRs should describe the board/firmware version used, include before/after screenshots or panel photos, note config knobs touched, and link any related issue.
- Run a fresh compile before opening a PR; scrub secrets from diffs and replace with placeholders in examples.
