## Feature Plan: Day / Night Screen Dim Brightness — Implemented

### Background

The Home Panel previously used a single constant `SCREEN_DIM_BRIGHTNESS` (7%) applied
whenever the screen entered the dim state after inactivity. At night, this was still
brighter than desired and could be annoying during sleeping hours.

### Goal

Add time-aware screen dimming so that:

- During the day, the dimmed screen uses a higher brightness (10%)
- During the night, the dimmed screen uses a lower brightness (3%)
- The correct brightness is applied when the screen transitions to dim
- If a day/night boundary is crossed while the screen is already dimmed, the
  brightness adjusts automatically
- On boot, the screen starts dimmed (no full-brightness burst)

### Configuration Constants

All constants live in `src/screen/screen_power.h`:

```c
#define SCREEN_DIM_DAY_BRIGHTNESS    10              // 10% brightness when dimmed during day
#define SCREEN_DIM_NIGHT_BRIGHTNESS   3              // 3% brightness when dimmed at night
#define DAY_START_MINUTES            (6 * 60 + 30)   // 06:30 = 390 minutes since midnight
#define NIGHT_START_MINUTES          (23 * 60 + 30)   // 23:30 = 1410 minutes since midnight
```

Minutes-since-midnight allows minute-level precision for tuning.
To set night at 22:30, use `(22 * 60 + 30)`. To set day at 6:45, use `(6 * 60 + 45)`.

### Time Rules

The current time is converted to minutes since midnight: `tm_hour * 60 + tm_min`.

- **Day:** current minutes >= DAY_START_MINUTES and < NIGHT_START_MINUTES
- **Night:** current minutes >= NIGHT_START_MINUTES or < DAY_START_MINUTES
  (spans midnight)

### Boot Behavior

The screen starts in the DIM state at day brightness (10%). This avoids a
full-brightness burst when booting at night. Once NTP syncs (a few seconds later),
the DIM-state boundary check adjusts to night brightness if applicable. The first
touch wakes the screen to full brightness.

### Edge Cases

**NTP not yet synced at boot:**
`getLocalTime()` returns false before NTP completes. In this case, default to
**day brightness**. Rationale: the user just powered on the device; using the
brighter dim level is the safer choice. Once NTP syncs, the boundary check
corrects to night brightness if needed.

**Day/night boundary while screen is ON (active):**
No immediate action needed. The correct dim brightness is determined at the moment
the screen transitions from ON to DIM. Since `screenPowerLoop()` checks the current
time when entering the dim state, this is handled naturally.

**Day/night boundary while screen is already DIM:**
`screenPowerLoop()` checks the current time each loop iteration while in DIM state.
If the period has changed, it updates the brightness. A `lastAppliedBrightness`
variable prevents redundant `bsp_display_brightness_set()` calls — the hardware
command is only sent when the value actually changes.

### Implementation Details

**Files modified:**

| File | Change |
|------|--------|
| `src/screen/screen_power.h` | Replace `SCREEN_DIM_BRIGHTNESS` with day/night constants and minute thresholds |
| `src/screen/screen_power.cpp` | Add `getDimBrightness()` helper, boot in DIM state, boundary detection in DIM state |
| `home_panel.ino` | Remove `bsp_display_backlight_on()` from `showMainScreen()` to preserve dim on boot |

**No new dependencies.** The screen power module uses `getLocalTime()` from
`<Arduino.h>`. No dependency on `time_service.h` is needed.

### Code Flow

**Helper function — `getDimBrightness()`:**

```cpp
static int getDimBrightness() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 0)) {
        return SCREEN_DIM_DAY_BRIGHTNESS;  // NTP not synced, default to day
    }
    int now = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    bool isNight = (now >= NIGHT_START_MINUTES || now < DAY_START_MINUTES);
    return isNight ? SCREEN_DIM_NIGHT_BRIGHTNESS : SCREEN_DIM_DAY_BRIGHTNESS;
}
```

**`screenPowerInit()` — starts in DIM state:**

```cpp
void screenPowerInit(void) {
    currentState = SCREEN_STATE_DIM;
    lastActivityTime = millis();
    lastAppliedBrightness = SCREEN_DIM_DAY_BRIGHTNESS;
    bsp_display_brightness_set(SCREEN_DIM_DAY_BRIGHTNESS);
}
```

**`screenPowerLoop()` — handles dim and boundary crossing:**

```cpp
void screenPowerLoop(void) {
    if (currentState == SCREEN_STATE_ON) {
        if (millis() - lastActivityTime > SCREEN_ON_TIMEOUT_MS) {
            int brightness = getDimBrightness();
            bsp_display_brightness_set(brightness);
            lastAppliedBrightness = brightness;
            currentState = SCREEN_STATE_DIM;
        }
    } else if (currentState == SCREEN_STATE_DIM) {
        int brightness = getDimBrightness();
        if (brightness != lastAppliedBrightness) {
            bsp_display_brightness_set(brightness);
            lastAppliedBrightness = brightness;
        }
    }
}
```

### Verification

- Boot at night → screen starts at day dim (10%), adjusts to night dim (3%) after NTP sync
- Boot during day → screen starts and stays at day dim (10%)
- No full-brightness burst on boot
- During daytime inactivity → screen dims to 10%
- During nighttime inactivity → screen dims to 3%
- Cross night boundary while dimmed → brightness drops to 3%, single log line
- Cross day boundary while dimmed → brightness rises to 10%, single log line
- Cross boundary while screen is active → correct brightness applied when screen dims
- Touch screen while dimmed → wakes to full brightness, 2-minute timeout resumes
- Serial log shows brightness changes only when they actually occur
