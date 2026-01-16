# Screen Power Management – Implementation Planning Document

## Context

We have completed initial testing and confirmed that the display supports three operational states:

- **Fully ON** (normal brightness)
- **Dimmed** (low brightness)
- **OFF**

For the next step, **do not modify any code**.  
This document is strictly for **planning and design review** prior to implementation.

---

## Critical Constraint: SquareLine Studio Files

- **SquareLine Studio–generated files must NOT be modified.**
- These files must be treated as **read-only** and left unchanged.
- Any new logic must:
  - Live outside of SquareLine-generated code, or
  - Be integrated via approved extension points such as:
    - User-defined callbacks
    - Custom source/header files
    - Existing hooks like `ui_custom.h`
- The plan must clearly describe **how to integrate behavior without editing generated files**.

---

## Target Screen Behavior

### Power-Up Behavior

- On power-up, the screen must be **fully ON**.
- Define a configurable constant (e.g. `SCREEN_ON_TIMEOUT`) with an initial value of **1 minute**.
- The screen remains fully ON for this duration.
- Once the timeout expires, the screen transitions automatically to the **dimmed (low brightness)** state.

---

## Returning to Full Brightness (Screen ON)

The screen must transition back to the fully ON state and restart the ON timeout cycle under **either** of the following conditions.

### 1. Touch Interaction

- Any touch on the screen:
  - Immediately turns the screen fully ON.
  - Restarts the `SCREEN_ON_TIMEOUT` timer.
- Additional touches while the screen is already ON:
  - Reset the timer to extend the ON period.
- When the timeout expires with no further activity:
  - The screen returns to the dimmed state.

### 2. MQTT Image Update

- When a **new image is received via MQTT** that initiates a "latest image" display request:
  - The screen must be turned fully ON to ensure visibility.
  - The ON timeout cycle is restarted.
- After the timeout expires:
  - The screen transitions back to the dimmed state.

---

## LVGL / SquareLine Integration Strategy

The project defines an `activity_event_handler` function for **Screen 1 and Screen 2** in `ui_custom.h`.

- This handler is triggered when the user touches the screen.
- Analyze whether this existing handler can be used as a **centralized activity signal** to:
  - Detect user interaction
  - Trigger the screen ON state
  - Reset or restart the ON timeout timer
- Any solution must:
  - Avoid direct edits to SquareLine-generated files
  - Use only supported extension mechanisms

---

## Planning Requirements

Provide a clear **implementation plan only**, including:

- A high-level **screen state model** (e.g. `ON → DIM → ON`)
- Required:

  - Timers
  - State variables
  - Brightness control mechanisms

- Identification of:

  - Event sources (touch events, MQTT image updates)
  - Where logic should reside conceptually (e.g. main loop, custom screen manager module, MQTT handler)

- How SquareLine-generated code is integrated **without modification**
- Edge cases to consider:

  - Repeated touches
  - Frequent MQTT updates
  - Power-up defaults
  - State synchronization between screens

---

## Out of Scope

- No code changes
- No SquareLine Studio file modifications
- No brightness tuning values
- No performance optimizations

This document exists solely to validate the design before implementation begins.

---

# Implementation Plan

## Screen State Model

```
           ┌──────────────────────────────────────┐
           │                                      │
           ▼                                      │
    ┌─────────────┐    timeout     ┌─────────────┐
    │    ON       │ ─────────────► │    DIM      │
    │ (100%)      │                │ (20%)       │
    └─────────────┘                └─────────────┘
           ▲                              │
           │      activity event          │
           └──────────────────────────────┘
```

**States:**

- `SCREEN_ON` - Full brightness (100%)
- `SCREEN_DIM` - Low brightness (configurable, default 20%)

**Transitions:**

- `ON → DIM`: After `SCREEN_ON_TIMEOUT` (default 60 seconds) with no activity
- `DIM → ON`: On any activity event (touch or MQTT image update)
- `ON → ON`: Activity while already ON resets the timeout timer

---

## New Module: Screen Power Manager

Create new files: `src/screen/screen_power.cpp` and `src/screen/screen_power.h`

### Header File (`screen_power.h`)

```cpp
#ifndef SCREEN_POWER_H
#define SCREEN_POWER_H

#ifdef __cplusplus
extern "C" {
#endif

// Configuration constants
#define SCREEN_ON_TIMEOUT_MS      60000   // 1 minute
#define SCREEN_DIM_BRIGHTNESS     20      // 20% brightness

// Screen power states
typedef enum {
    SCREEN_STATE_ON,
    SCREEN_STATE_DIM
} ScreenPowerState;

// Initialize screen power manager (call from setup)
void screenPowerInit(void);

// Process screen power state machine (call from loop)
void screenPowerLoop(void);

// Signal activity - wakes screen and resets timer
// Call this from activity_event_handler() and MQTT image handler
void screenPowerActivity(void);

// Get current state (for debugging/status)
ScreenPowerState screenPowerGetState(void);

#ifdef __cplusplus
}
#endif

#endif // SCREEN_POWER_H
```

### Implementation File (`screen_power.cpp`)

**State Variables:**

```cpp
static ScreenPowerState currentState = SCREEN_STATE_ON;
static unsigned long lastActivityTime = 0;
```

**Key Functions:**

1. `screenPowerInit()`:

   - Set `currentState = SCREEN_STATE_ON`
   - Set `lastActivityTime = millis()`
   - Call `bsp_display_backlight_on()` to ensure full brightness

2. `screenPowerActivity()`:

   - Update `lastActivityTime = millis()`
   - If `currentState == SCREEN_STATE_DIM`:
     - Call `bsp_display_backlight_on()`
     - Set `currentState = SCREEN_STATE_ON`
     - Log: "Screen wake - activity detected"

3. `screenPowerLoop()`:

   - If `currentState == SCREEN_STATE_ON`:
     - Check if `millis() - lastActivityTime > SCREEN_ON_TIMEOUT_MS`
     - If timeout expired:
       - Call `bsp_display_brightness_set(SCREEN_DIM_BRIGHTNESS)`
       - Set `currentState = SCREEN_STATE_DIM`
       - Log: "Screen dim - timeout expired"

---

## Integration Points (No SquareLine Modifications)

### 1. Touch Activity Integration

**File:** `src/ui_custom.c`

The `activity_event_handler()` function is already:

- Declared in `ui_custom.h`
- Called by SquareLine-generated code on all button clicks and screen touches
- Currently a no-op placeholder

**Change Required:**

```cpp
#include "screen/screen_power.h"

void activity_event_handler(lv_event_t * e) {
    screenPowerActivity();  // Wake screen and reset timer
    (void)e;
}
```

**Verified Call Sites (already in SquareLine code - no changes needed):**

- `ui_Screen1.c:22` - Screen1 clicked
- `ui_Screen1.c:31` - ButtonLatest clicked
- `ui_Screen1.c:43` - ButtonBack clicked
- `ui_Screen1.c:55` - ButtonNew clicked
- `ui_Screen2.c:18` - Screen2 clicked
- `ui_Screen2.c:30` - ButtonBackScreen2 clicked

### 2. MQTT Image Update Integration

**File:** `src/image/image_fetcher.cpp`

**Change Required in `requestLatestImage()`:**

```cpp
#include "../screen/screen_power.h"

bool requestLatestImage() {
    // Existing validation code...

    screenPowerActivity();  // Wake screen for incoming image

    // Existing request code...
}
```

This ensures the screen wakes when MQTT triggers an image display.

### 3. Main Loop Integration

**File:** `home_panel.ino`

**Changes Required:**

```cpp
#include "src/screen/screen_power.h"

void setup() {
    // ... existing setup code ...

    screenPowerInit();  // Initialize screen power manager

    // Remove old backlight test code
}

void loop() {
    // ... existing loop code ...

    screenPowerLoop();  // Process screen power state machine

    // Remove old backlight test switch statement
}
```

---

## Code Removal

Remove from `home_panel.ino`:

- Constants: `BACKLIGHT_OFF_AFTER_MS`, `BACKLIGHT_OFF_DURATION_MS`, `BACKLIGHT_DIM_DURATION_MS`, `BACKLIGHT_DIM_PERCENT`
- Variables: `backlightTestState`, `backlightStateStartTime`
- The entire backlight test `switch` statement in `loop()`

---

## Edge Cases Handled

| Edge Case | Handling |
|-----------|----------|
| Repeated touches | Each touch resets timer; no penalty for rapid touches |
| Frequent MQTT updates | Each update resets timer; screen stays ON during active updates |
| Power-up default | `screenPowerInit()` sets ON state and full brightness |
| Screen transitions | Activity handler is screen-agnostic; works on both screens |
| Timer overflow | Using `millis()` subtraction handles 49-day rollover correctly |

---

## File Summary

| File | Action | Description |
|------|--------|-------------|
| `src/screen/screen_power.h` | **CREATE** | Header with API declarations |
| `src/screen/screen_power.cpp` | **CREATE** | State machine implementation |
| `src/ui_custom.c` | **MODIFY** | Add `screenPowerActivity()` call |
| `src/image/image_fetcher.cpp` | **MODIFY** | Add `screenPowerActivity()` call |
| `home_panel.ino` | **MODIFY** | Init/loop calls, remove test code |
| `ui_Screen1.c` | **NO CHANGE** | SquareLine file - unchanged |
| `ui_Screen2.c` | **NO CHANGE** | SquareLine file - unchanged |

---

## Risk Assessment: Low

- Simple 2-state machine with proven pattern from backlight test
- No SquareLine files modified
- Uses existing, tested `bsp_display_*` functions
- Single point of activity signaling prevents race conditions
- Graceful behavior: worst case is screen stays ON or DIM longer than expected
