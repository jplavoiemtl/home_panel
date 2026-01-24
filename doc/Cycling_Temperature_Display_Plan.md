# Implementation Plan: Cycling Temperature Display

## Overview

This document outlines the implementation plan for the cycling temperature display feature as specified in `Cycling_Temperature_Display_Feature.md`. The feature allows displaying temperature data from multiple locations using a single UI area, with button-based cycling between locations.

## Data Structure Design

### Single Source of Truth Approach

The `locationMeta` array is the only place to define locations. The count is derived automatically:

```cpp
struct LocationMeta {
    const char* label;      // Display name: "Outside"
    const char* mqttKey;    // JSON key: "OutsideTemp"
};

// Add/remove locations here - everything else adapts automatically
const LocationMeta locationMeta[] = {
    { "Outside", "OutsideTemp" },
    { "Ambient", "AmbientTemp" },
    { "Kitchen", "KitchenTemp" }
};

// Count derived at compile time
constexpr size_t TEMP_LOC_COUNT = sizeof(locationMeta) / sizeof(locationMeta[0]);
```

### Temperature Sample Structure

```cpp
struct TempSample {
    float temperatureC;
    char timeHHMM[6];   // "hh:mm" + null terminator
    bool valid;         // false until first MQTT update
};

// Storage sized automatically from locationMeta
TempSample tempSamples[TEMP_LOC_COUNT];
```

### Current Location State

```cpp
size_t currentLocation = 0;  // Index into locationMeta/tempSamples
```

## Module Organization

### File Structure

```
src/
└── temperature/
    ├── temperature_service.h    # Public API
    └── temperature_service.cpp  # Implementation
```

### Public API

```cpp
// temperature_service.h

#ifndef TEMPERATURE_SERVICE_H
#define TEMPERATURE_SERVICE_H

#include <lvgl.h>

// Initialize the temperature service (call in setup())
void temperature_service_init(lv_obj_t* locLabel, lv_obj_t* tempLabel, lv_obj_t* timeLabel);

// Handle incoming MQTT weather message (call from mqttCallback)
void temperature_service_handleMQTT(const char* payload);

// Cycle to next location (call from button handler)
void temperature_service_cycleLocation();

// Periodic processing for NVS debounce (call in loop())
void temperature_service_loop();

#endif
```

## Implementation Steps

### Step 1: Create Temperature Service Module

Create `src/temperature/temperature_service.h` and `src/temperature/temperature_service.cpp` with:

- Struct and array definitions
- Static storage for temperature samples
- Current location tracking
- NVS load/save functions with debounce

### Step 2: Implement Core Functions

**Initialization:**

- Load saved location from NVS
- Store label pointers
- Display initial state ("--" for temp/time, location name for label)

**MQTT Handler:**

- Parse JSON payload to find matching location key
- Update corresponding TempSample
- Get current time from time_service
- If current location matches, update UI

**Cycle Location:**

- Increment current location with wrap-around
- Update UI with stored data (or "--" if invalid)
- Start/reset 30-second NVS debounce timer

**Loop Processing:**

- Check if debounce timer expired
- Save to NVS if pending

### Step 3: Create UI Elements in SquareLine Studio

Add to Screen1:

- `ui_ButtonTempLocation` - Button to cycle locations
- `ui_labelTempLoc` - Location name label
- `ui_labelTemp` - Temperature value label
- `ui_labelTempTime` - Sample time label

### Step 4: Integrate with Main Sketch

**In home_panel.ino:**

- Include temperature_service.h
- Call `temperature_service_init()` in `setup()`
- Call `temperature_service_loop()` in `loop()`
- Route weather topic to `temperature_service_handleMQTT()`

**In ui_events.h / ui_Screen1.c:**

- Declare and implement `buttonTempLocation_event_handler()`

## UI Behavior

### Startup State (No Data Yet)

| Label | Display | Rationale |
|-------|---------|-----------|
| Location | "Outside" (or saved) | Immediate feedback that system is working |
| Temperature | "--" | Clear indication no data received |
| Time | "--:--" | Consistent with temperature, shows expected format |

### Data Received

| Label | Display | Example |
|-------|---------|---------|
| Location | Location name | "Outside" |
| Temperature | Value with unit | "-15.8 C" |
| Time | Sample time | "14:57" |

### Color Coding

Apply color to temperature value based on thresholds:

| Condition | Color |
|-----------|-------|
| temp < 0 | Blue (`LV_PALETTE_BLUE`) |
| 0 <= temp <= 25 | Green (`LV_PALETTE_GREEN`) |
| temp > 25 | Red (`LV_PALETTE_RED`) |

### Stale Data

Display last known value with no indicator. User expects live data; if system is working, data will update.

### Partial Data Availability

When cycling to a location with no data yet:

- Show location name normally
- Show "--" for temperature and "--:--" for time
- Do not skip the location (user should know it exists)

## NVS Debounce Strategy

```cpp
namespace {
    constexpr unsigned long NVS_DEBOUNCE_MS = 30000;  // 30 seconds
    unsigned long lastLocationChangeTime = 0;
    bool nvsSavePending = false;
    size_t pendingLocation;
}

void temperature_service_cycleLocation() {
    currentLocation = (currentLocation + 1) % TEMP_LOC_COUNT;
    updateUI();

    // Start/reset debounce timer
    lastLocationChangeTime = millis();
    nvsSavePending = true;
    pendingLocation = currentLocation;
}

void temperature_service_loop() {
    if (nvsSavePending && (millis() - lastLocationChangeTime >= NVS_DEBOUNCE_MS)) {
        saveLocationToNVS(pendingLocation);
        nvsSavePending = false;
    }
}
```

## MQTT Parsing Logic

### JSON Format Expected

```json
{"OutsideTemp": -15.8}
{"AmbientTemp": 21.3}
{"KitchenTemp": 20.2}
```

### Parsing Implementation

```cpp
void temperature_service_handleMQTT(const char* payload) {
    for (size_t i = 0; i < TEMP_LOC_COUNT; i++) {
        char key[32];
        snprintf(key, sizeof(key), "\"%s\":", locationMeta[i].mqttKey);

        char* pos = strstr(payload, key);
        if (pos) {
            pos += strlen(key);
            float temp = strtof(pos, NULL);

            // Update sample
            tempSamples[i].temperatureC = temp;
            tempSamples[i].valid = true;

            // Get current time (hh:mm only)
            String timeStr = time_service_getFormattedTime();
            strncpy(tempSamples[i].timeHHMM, timeStr.c_str(), 5);
            tempSamples[i].timeHHMM[5] = '\0';

            // Update UI if this is current location
            if (i == currentLocation) {
                updateUI();
            }
            return;
        }
    }
}
```

## Button Event Handler

```cpp
void buttonTempLocation_event_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        temperature_service_cycleLocation();
    }
}
```

## Integration Points Summary

| File | Changes |
|------|---------|
| `home_panel.ino` | Add includes, init call, loop call, MQTT routing |
| `ui_Screen1.c` | Add button and labels (via SquareLine Studio) |
| `ui_events.h` | Declare button handler |
| `src/temperature/*` | New module (2 files) |

## Testing Checklist

- [ ] Boot with no NVS data - defaults to first location
- [ ] Boot with saved location - restores correctly
- [ ] Receive MQTT for current location - updates immediately
- [ ] Receive MQTT for other location - stored but not displayed
- [ ] Cycle through all locations - wraps correctly
- [ ] Rapid button presses - NVS not written until debounce
- [ ] Location with no data - shows "--" correctly
- [ ] Negative temperatures - blue color, correct formatting
- [ ] High temperatures - red color, correct formatting
