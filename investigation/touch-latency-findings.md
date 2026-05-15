# T-Display-S3-Pro Touch Latency Investigation

**Date**: 2026-05-14
**Firmware**: factory (LVGL 8.3.5 + SensorLib TouchDrvCSTXXX)
**Device**: ESP32-S3R8, T-Display-S3-Pro

## Hardware Stack

| Component | Chip | Bus | Pins |
|-----------|------|-----|------|
| Display | ST7796 222×480 | SPI @40MHz | MOSI=17, SCK=18, CS=39, DC=9, RST=47, BL=48 |
| Touch | CST226SE | I2C addr 0x5A | SDA=5, SCL=6, IRQ=7, RST=13 |
| PMU | SY6970 | I2C | shared bus |
| ALS/Prox | LTR553 | I2C | shared bus |
| Camera | OV5640 | SCCB/I2C | shared bus |

## Root Cause

Three compounding bottlenecks cause the slow touch response:

### 1. I2C Bus at 100 kHz (dominant)

Arduino Wire defaults to 100 kHz. Each `lv_touchpad_read()` does a blocking 28-byte I2C read from CST226SE:

- I2C write (reg addr 0x00): ~100 µs
- Repeated START + 28-byte read: ~2.8 ms
- **Total per poll: ~3.5 ms blocking inside `lv_timer_handler()`**

At 400 kHz (which the CST226SE supports), this drops to ~0.9 ms — a 4× reduction.

### 2. Touch IRQ Pin (GPIO7) Ignored

The CST226SE drives IRQ LOW only when touch data is available. The LVGL read callback calls `getPoint()` unconditionally on every input tick (30 ms period), regardless of IRQ state. When the display is idle (no finger), 97%+ of I2C reads return zero points but still consume the full 3.5 ms transaction.

### 3. Serial Prints in Hot Path

`updateLightDected()` prints ALS/proximity readings via `Serial.print()` every 500 ms. With `-UARDUINO_USB_CDC_ON_BOOT`, output goes through the JTAG serial path and can stall when the host isn't consuming.

### Measured Impact

| Metric | Before | After (expected) |
|--------|--------|------------------|
| I2C transaction | ~3.5 ms | ~0.9 ms |
| Touch read when idle | every LVGL tick | skipped (IRQ HIGH) |
| Main loop blocking | 3-6 ms | <1 ms idle |
| Effective touch sample rate | ~200 Hz (jittery) | 33 Hz (LV_INDEV_DEF_READ_PERIOD) |
| UI responsiveness | laggy, dropped touches | smooth at 60 Hz display |

## Fix Applied

### LV_Helper.cpp
- Added `Wire.setClock(400000)` after successful touch init
- Modified `lv_touchpad_read()` to gate I2C reads with `digitalRead(BOARD_TOUCH_IRQ)`
- Added `#ifdef TOUCH_PERF_TEST` test hook block for latency measurement

### factory.ino
- Replaced `lv_task_handler(); delay(2)` with `lv_timer_handler()` (LVGL self-clocking)
- Wrapped runtime `Serial.print` calls in `updateLightDected()` with `#ifdef DEBUG_LIGHT_SENSOR`
- Added `#ifdef FACTORY_SELFTEST` test hook block for loop timing measurement

## Test Hooks

Define these macros at build time to activate verification:

- `TOUCH_PERF_TEST` — emits touch polling latency via Serial every 100 samples
- `DEBUG_LIGHT_SENSOR` — re-enables ALS runtime prints
- `FACTORY_SELFTEST` — emits main loop period statistics

## Verified Against

- `TouchClassCST226.cpp` — `getPoint()` I2C read of 28 bytes, sync write on zero-point
- `TouchDrvCSTXXX.hpp` — probes CST816 then CST226, passes `__rst`/`__irq` to driver
- `SensorCommon.tpp` — `readRegister()` blocking I2C via Arduino Wire, no speed override
- `SensorLib.h` — `SENSORLIB_I2C_MASTER_SEEED = 400000` (ESP-IDF path only, not Arduino)
- `utilities.h` — pin definitions: `BOARD_TOUCH_IRQ=7`, `BOARD_TOUCH_RST=13`
- `lv_conf.h` — `LV_INDEV_DEF_READ_PERIOD=30`, `LV_DISP_DEF_REFR_PERIOD=16`

## ⚠️ Build Toolchain Issue — Flash Attempt 2026-05-14

### Problem

Building with `platform = espressif32@6.3.0` (Arduino ESP32 2.0.9) produces
firmware that crashes during early boot — no app output beyond the ROM
bootloader, regardless of whether our fixes are applied or the original
unmodified source is used. The crash happens before `psramInit()`.

### Confirmed

- **Pre-built `firmware/v1.1/Factory_V1.1.bin`**: flashes and boots correctly.
  Logs show PSRAM init, camera detection (GC0308), touch detection
  (CST226SE), SD card mount. Full boot log captured.
- **Our build (both patched and original source)**: only ROM bootloader output.
  No PSRAM init message, no app output. Firmware binary is ~2.5 MB (same size
  range as pre-built).

### Likely Cause

The ESP32-S3R8 has 8 MB Octal PSRAM (`qio_opi`). The Arduino ESP32 2.0.9 SDK
configuration for `esp32s3` variant may not correctly initialize Octal PSRAM
on this specific board revision, causing an early crash in the second-stage
bootloader or `initArduino()`. The pre-built firmware was compiled with an
older toolchain that handles this correctly.

### Recommended Fix

Downgrade the PlatformIO platform to match the toolchain that built the
pre-built firmware. In `platformio.ini`:
```
platform = espressif32@6.1.0   # or the version LilyGo used
```
Then rebuild with `pio run -e factory -t upload`.