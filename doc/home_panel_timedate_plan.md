# Home Panel ESP32 -- Time & Date Display Plan (NTP Based)

## Objective

Add a live time and date display on **Screen 1** of the Home Panel UI
using NTP time synchronization.

The time and date must be displayed in this format:

> **18 Jan 2026, 14:31:10**

The label already exists in SquareLine Studio:

-   **Label name:** `labelTimeDate`

The update rate must be **once per second** to avoid overloading LVGL.

We will base all NTP functionality on the proven reference sketch:

> **File:** `ntp_reference_code/ntp.ino`\
> **Purpose:** Reference only -- do NOT modify this file

------------------------------------------------------------------------

## Design Principles

1.  **Reuse proven NTP logic**
    -   Use the same servers, timezone handling, and configuration as
        the reference code:
        -   `pool.ntp.org`
        -   `configTime(0, 0, "pool.ntp.org")`
        -   `EST5EDT,M3.2.0,M11.1.0` (Montreal timezone)
    -   Keep the same functions conceptually:
        -   `setTimezone()`
        -   `initTime()`
        -   `syncTime()`
2.  **Non-blocking UI updates**
    -   Never block the LVGL task
    -   Time refresh must be lightweight and safe
3.  **Once-per-second refresh**
    -   Use a software timer or millis() based scheduler
    -   Update `labelTimeDate` exactly once per second

------------------------------------------------------------------------

## High Level Architecture (Simplified)

### Single Time Service Module

All time logic lives in **one module**:

-   `time_service.h`
-   `time_service.cpp`

**Responsibilities** - Initialize NTP when WiFi connects\
- Maintain local time using ESP32 system clock\
- Periodically re-sync with NTP (ex: every 6 hours)\
- Format time and date string\
- Update `labelTimeDate` once per second using an LVGL timer

------------------------------------------------------------------------

## Boot Flow

    WiFi Connected
          ↓
    time_service_init()
          ↓
    - initTime("EST5EDT,M3.2.0,M11.1.0")
    - create 1-second LVGL timer (label update)
    - create 6-hour NTP re-sync timer
          ↓
    labelTimeDate updates automatically

------------------------------------------------------------------------

## Public API (time_service.h)

  ---------------------------------------------------------------------------
  Function                                Purpose
  --------------------------------------- -----------------------------------
  `void time_service_init();`             Called once after WiFi is connected

  `void time_service_sync();`             Manual NTP re-sync (optional)

  `String time_service_getFormatted();`   Returns `18 Jan 2026, 14:31:10`
  ---------------------------------------------------------------------------

------------------------------------------------------------------------

## Formatting Requirements

### Desired Format

    18 Jan 2026, 14:31:10

**Rules**

-   Day: no leading zero\
-   Month: 3-letter English abbreviation\
-   Year: 4 digits\
-   24-hour format\
-   Leading zero for minutes and seconds

------------------------------------------------------------------------

## Error Handling

  Situation              Behavior
  ---------------------- ---------------------------------
  WiFi not connected     Display `-- --- ----, --:--:--`
  NTP fails              Keep last known time
  Time not initialized   Display `Syncing time...`

------------------------------------------------------------------------

## Performance Considerations

-   Never call `configTime()` every second\
-   Never block inside LVGL task\
-   Time formatting only once per second\
-   NTP sync only on boot and every few hours

------------------------------------------------------------------------

## Testing Plan

1.  Boot with WiFi → Time appears within 5 seconds\
2.  Disconnect WiFi → Time continues running\
3.  Reconnect WiFi → NTP resync happens\
4.  Leave device running 24h → Drift corrected automatically\
5.  Verify label updates smoothly at 1 Hz

------------------------------------------------------------------------

## Files To Touch

  File                 Purpose
  -------------------- ---------------------------------------
  `time_service.h`     Time service public API
  `time_service.cpp`   NTP + formatting + LVGL timers
  `main.ino`           Call `time_service_init()` after WiFi

------------------------------------------------------------------------

## What NOT To Do

-   Do NOT modify `ntp_reference_code/ntp.ino`\
-   Do NOT update LVGL faster than 1 Hz\
-   Do NOT call NTP inside the LVGL timer

------------------------------------------------------------------------

## Acceptance Criteria

-   Time displayed exactly like: `18 Jan 2026, 14:31:10`\
-   Updates once per second\
-   Survives WiFi loss\
-   Uses same NTP logic as reference\
-   No LVGL performance degradation

------------------------------------------------------------------------

## Next Step After Approval

Once this plan is approved, we will:

1.  Implement `time_service` module\
2.  Wire it into Home Panel boot sequence\
3.  Validate on real ESP32 hardware
