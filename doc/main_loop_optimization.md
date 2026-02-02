# Optimization: Image Download Speed

## Context

The original plan proposed three changes: throttling `lv_timer_handler()`, reducing
the main loop delay, and removing the image download delay. After analysis, only
the image download change produces a measurable benefit. The other two are technically
sound but yield negligible real-world improvement.

## Analysis Summary

| Change | Benefit | Status |
|--------|---------|--------|
| Throttle LVGL to 200 Hz | Negligible — `lv_timer_handler()` returns quickly when idle | **Not implementing** |
| `delay(2)` → `delay(1)` | Negligible — network stack runs on RTOS background task | **Not implementing** |
| Replace image download `delay(1)` with `yield()` | Measurable — eliminates 300-500 ms dead time per download | **Implementing** |

---

## Approved Change: Replace `delay(1)` with `yield()` in image download loop

### Problem

In `src/image/image_fetcher.cpp`, the HTTP receive loop calls `delay(1)` after
each packet read. This adds 1 ms of dead time per iteration. For a typical JPEG
download with 300-500 read cycles, this totals 300-500 ms of unnecessary delay.

### Solution

Replace `delay(1)` with `yield()`. The `yield()` function yields to the ESP-IDF
RTOS scheduler (preventing watchdog resets) without the 1 ms minimum sleep penalty
of `delay()`. The download loop can then process packets at line speed.

### Code Change

**File: `src/image/image_fetcher.cpp`** (line 395)

Before:

```cpp
delay(1);
```

After:

```cpp
yield();
```

### Why `yield()` instead of removing entirely

Removing the call entirely risks a watchdog reset during long downloads. The ESP32
watchdog expects periodic yielding to the RTOS scheduler. `yield()` satisfies this
requirement with near-zero overhead.

---

## Expected Impact

- **Image download time**: Reduced by 300-500 ms per download (the accumulated
  `delay(1)` overhead)
- **UI responsiveness**: Unchanged — `yield()` still allows RTOS tasks to run
- **Stability**: Maintained — `yield()` prevents watchdog resets

## Verification Plan

1. Download several images and verify they complete faster than before
2. Monitor serial output for watchdog reset warnings
3. Verify UI remains responsive during downloads
4. Test with both small and large JPEG files
