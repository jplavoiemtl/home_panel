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
| Replace image download `delay(1)` with `yield()` | Measurable — eliminates 300-500 ms dead time per download | **Implemented** |

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

## Verified Results

Cached image downloads now complete in 47-72 ms. New image generation (server-side)
takes ~4.5 s, dominated by server processing time rather than transfer overhead.

| Request type | Download time | Notes |
|-------------|--------------|-------|
| Cached (back/latest) | 47-72 ms | Network transfer only |
| New generation | ~4,558 ms | Server-side AI generation dominates |

- **UI responsiveness**: Unchanged — `yield()` still allows RTOS tasks to run
- **Stability**: Maintained — no watchdog resets observed
- **Serial logs**: Streamlined to show only key events with download timing

## Serial Log Streamlining

As part of this optimization, serial output from the image fetcher was streamlined
for better readability. Verbose messages were removed, and download duration timing
was added.

**Typical output:**

```
Button: latest
Image request: latest
Image downloaded: 22685 bytes in 60ms. Decoding...
Image displayed: 480x320
```

Removed messages: HTTP GET details, content-length, "starting to receive", JPEG
decode confirmation, LVGL source update, and screen unload notifications. JPEG
dimension mismatch warnings are still logged when they occur.
