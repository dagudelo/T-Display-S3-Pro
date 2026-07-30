# T-Display-S3-Pro — Project Memory

## 2026-07-30: Device Apps — Feature Implementation + Refactor

### Features implemented
- **WiFi Details**: Toast with SSID+IP on connect; tappable wifi icon opens full screen with SSID, IP, MAC, RSSI, Gateway + Refresh button
- **Digital Clock + Weather**: Replaces analog clock; shows HH:MM:SS + YYYY-MM-DD + live weather from Open-Meteo API (no key needed)
- **Stocks**: 5 tickers (USD/COP, NU, AMZN, AAPL, NVDA) with prev/next selector; 4 time ranges (12H/1D/1W/1M); live quotes from Yahoo Finance API
- **Photos by day**: Captures now save to `/photos/YYYY-MM-DD/imgXXXX.bmp`; toast shows saved filename
- **Music/Tone Player**: PWM-based piano keyboard using vibrator pin (GPIO16); no DAC or speaker on board — ESP32-S3 has no internal DAC

### Architecture refactor
Extracted 4 new encapsulated app classes following existing `app_*.h/cpp` pattern:

| File | Purpose |
|------|---------|
| `app_clock_weather.h/cpp` | Digital clock + weather screen |
| `app_wifi_details.h/cpp` | WiFi info screen |
| `app_stocks.h/cpp` | Stock quotes screen |
| `app_music.h/cpp` | PWM tone player screen |

Each exposes a single `xxx_app_create()` → `lv_obj_t*`. All state is file-static, self-contained, and independently testable. `ui.cpp` grew by only 26 lines (3352→3378) — thin wiring layer delegates to the new classes.

### Changed files
- `examples/factory/ui.cpp` — +26 lines (4 event handlers, wifi toast, photos by-day)
- `examples/factory/ui.h` — +4 includes
- `examples/factory/app_clock_weather.{h,cpp}` — new
- `examples/factory/app_wifi_details.{h,cpp}` — new
- `examples/factory/app_stocks.{h,cpp}` — new
- `examples/factory/app_music.{h,cpp}` — new

### Hardware notes
- **No audio DAC**: ESP32-S3 lacks internal DAC; board schematic has no I2S codec. Music app uses PWM on GPIO16 (vibrating motor) for simple tones.
- **Stock/weather APIs**: Free-tier, no API keys required. Open-Meteo (weather), Yahoo Finance v8 (stocks).

### Testing environment
Three-tier test suite:
1. **`pio run -e factory`** — full PlatformIO firmware build (syntax, type, link checks)
2. **`python3 scripts/qemu_test.py`** — QEMU ESP32-S3 emulation (Espressif fork, `qemu-system-xtensa`). Boots firmware, checks ROM boot + entry point. Crashes on peripheral access are expected (QEMU lacks TFT/touch/camera/PMU emulation).
3. **`g++ -std=c++17 tests/test_apps.cpp && ./a.out`** — 30 g++ unit tests for pure logic (JSON parsers, buffer safety, array indexing, code classification)

QEMU setup: download binary from `github.com/espressif/qemu/releases`, extract, copy `qemu/bin/qemu-system-xtensa` to `~/.local/bin/` and `qemu/share/qemu/esp32s3_rev0_rom.bin` to `~/.local/share/qemu/`.

## 2026-05-14: Touch Latency Fix

**Problem**: Touch response on the T-Display-S3-Pro was very slow and didn't allow interaction with UI functionalities.

**Root cause**: Three compounding bottlenecks:
1. I2C bus at Arduino default 100 kHz — every touch read blocked for ~3.5 ms
2. Touch IRQ pin (GPIO7) ignored — reads fired even when no touch data was available
3. Serial prints in the hot path (ALS sensor readings every 500 ms)

**Fix applied** (branch `fix/touch-latency`):
- `LV_Helper.cpp`: I2C bumped to 400 kHz + IRQ-gated touch reads
- `factory.ino`: `lv_timer_handler()` replaces `lv_task_handler(); delay(2)`, ALS prints conditional

**Test hooks**: `TOUCH_PERF_TEST`, `FACTORY_SELFTEST`, `DEBUG_LIGHT_SENSOR` (inert unless defined at build time)

**Full details**: `investigation/touch-latency-findings.md`
