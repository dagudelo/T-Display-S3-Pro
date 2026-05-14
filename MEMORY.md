# T-Display-S3-Pro — Project Memory

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
