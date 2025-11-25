# weatherCrow 5.7

ESP32-S3 firmware that renders current conditions, AQI, and a short forecast on a 5.7" e-paper panel. It pulls weather from OpenWeatherMap One Call 3.0 and optional U.S. AQI from AirNow, then deep-sleeps between refreshes to conserve power.

## Features
- Current conditions with temperature (UV index and wind speed included)
- Haze icon added alongside existing weather glyphs
- AQI overlay via AirNow (falls back gracefully if unavailable)
- OpenWeatherMap One Call 3.0 for forecasts and hourly precip chance
- Deep-sleep scheduling to conserve battery

## Hardware
- ESP32-S3 module
- 5.7" (792x272) e-paper driven by dual SSD1683 controllers
- Screen power on GPIO 7 (set `HIGH` before drawing)
- Power via USB or battery; enable `LOW_POWER_MODE` when on battery

## Repository layout
- `weatherCrow5.7.ino` — main application: Wi-Fi, API calls, rendering, and sleep schedule
- `config.example.h` — template for credentials and runtime constants (copy to `config.h` locally, keep secrets out of commits)
- `EPD*.h/.cpp`, `spi*.{h,cpp}` — display driver and SPI wiring
- `font*.cpp`, `fonts.h`, `weatherIcons.h` — typography and icon assets
- `weatherCrowbackup.h`, `EPD_Init*` — reference defaults and legacy init sequences

## Prerequisites
- Arduino IDE 2.x or `arduino-cli`
- ESP32 board package installed (e.g., `esp32:esp32:esp32s3`)
- ArduinoJson library

## Configure
1) Copy `config.example.h` to `config.h` (this file is gitignored).
2) Edit `config.h` with your Wi-Fi SSID/password, `OPEN_WEATHER_MAP_API_KEY`, optional `AIRNOW_API_KEY`, units, coordinates, and `LOCATION_NAME`.
3) Tune behavior: `REFRESH_MINUITES`, `DEFAULT_TIMEZONE_OFFSET`, `HOUR_INTERVAL`, `LOW_POWER_MODE`, and optional `NIGHT_START_HOUR`/`NIGHT_END_HOUR` quiet hours.
4) Keep `config.h` untracked and avoid committing API keys; `.gitignore` already excludes secrets and build outputs.

> OpenWeatherMap One Call 3.0 requires a billed account; set a daily request cap (e.g., 900/day) in the OWM dashboard to stay within free-tier limits and avoid HTTP 429s.

## Build and flash
Example commands with `arduino-cli` (adjust port and fqbn if needed):

```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3 weatherCrow5.7.ino --output-dir build
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32s3 weatherCrow5.7.ino
```

## Run and monitor
- Open the serial monitor at `115200` baud.
- Confirm the e-paper power pin (GPIO 7) is wired.
- On success the display clears, draws the current weather/AQI panel, then deep-sleeps for `REFRESH_MINUITES` minutes (or 5 minutes after a failure).
- For battery builds, enable `LOW_POWER_MODE` to downclock before sleep; the CPU is raised temporarily during Wi-Fi connect.

## Troubleshooting
- If Wi-Fi fails, verify SSID/password and router reachability; the device retries and shows an error panel with helpful metadata.
- HTTP errors from AirNow are non-fatal; the rest of the panel still renders.
- For layout changes, call a full clear (already done before rendering) and capture panel photos to verify alignment.
