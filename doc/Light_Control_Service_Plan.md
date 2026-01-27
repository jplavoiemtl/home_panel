# Light Control Service – Implementation Plan

## Context and Goal

Add a **Light Control Service** to the Home Panel firmware on **Screen 1**.
The service allows the user to **toggle multiple lights** using a minimalist cycling UI,
reusing the same interaction pattern as the existing `temperature_service`.

This document is the **approved implementation plan**. No code changes yet.

---

## Architecture Overview

New module files:

- `src/light/light_service.h`
- `src/light/light_service.cpp`

The module follows the same patterns as `temperature_service`:

- Static internal state in an anonymous namespace
- `init()` receives LVGL object pointers and MQTT client pointer
- `loop()` handles NVS debounce
- Event handlers use `extern "C"` linkage for LVGL compatibility

---

## Data Model

```cpp
enum class LightState : uint8_t {
    UNKNOWN,
    ON,
    OFF
};

struct LightMeta {
    const char* description;       // Display name ("Cuisine")
    const char* togglePayload;     // Sent to toggle ("cuisine")
    const char* statusOnPayload;   // Received when ON ("cu_on")
    const char* statusOffPayload;  // Received when OFF ("cu_of")
};

const LightMeta lightMeta[] = {
    { "Cuisine",  PAYLOAD_CUI, CUISINE_ON,  CUISINE_OFF  },
    { "Salon",    PAYLOAD_SAL, SALON_ON,    SALON_OFF    },
    { "Statue",   PAYLOAD_STA, STATUE_ON,   STATUE_OFF   },
    { "Galerie",  PAYLOAD_GAL, GALERIE_ON,  GALERIE_OFF  },
    { "Piscine",  PAYLOAD_PIS, PISCINE_ON,  PISCINE_OFF  },
};

constexpr size_t LIGHT_COUNT = sizeof(lightMeta) / sizeof(lightMeta[0]);

LightState lightStates[LIGHT_COUNT];  // All initialized to UNKNOWN
```

**Design rationale:**

- A typed enum (`LightState`) avoids ambiguous boolean states and cleanly represents the UNKNOWN boot state
- Adding a new light requires only one new entry in `lightMeta[]` plus its payload constants
- `lightStates[]` is a parallel array indexed the same as `lightMeta[]`

---

## MQTT Integration

### Topic

A single MQTT topic is used for both commands and status:

```cpp
const char TOPIC_LIGHT[] = "m18toggle";
```

### Publishing (toggle commands)

The light service needs access to `PubSubClient*` to publish.
This pointer is passed at init time.

```cpp
void light_service_init(lv_obj_t* selectBtn, lv_obj_t* lightBtn,
                        lv_obj_t* label, PubSubClient* mqtt);
```

When the user presses `ButtonLight`:

```cpp
mqttClient->publish(TOPIC_LIGHT, lightMeta[currentLight].togglePayload);
```

### Subscribing (status updates)

The `m18toggle` topic must be added to `NetTopics` and subscribed in `net_module`:

```cpp
struct NetTopics {
    const char* image;
    const char* power;
    const char* energy;
    const char* weather;
    const char* light;     // NEW
};
```

Both subscription points in `net_module.cpp` (initial connect and reconnect) must subscribe to this new topic.

### Receiving status

In `mqttCallback()` in `home_panel.ino`, add a new branch:

```cpp
else if (strcmp(topic, TOPIC_LIGHT) == 0) {
    light_service_handleMQTT(message);
}
```

`light_service_handleMQTT()` iterates over `lightMeta[]` and matches the payload against `statusOnPayload` / `statusOffPayload` using `strcmp()`. On match, it updates the corresponding `lightStates[]` entry and refreshes the UI if the matched light is currently selected.

---

## UI Elements (SquareLine Studio)

All three UI elements are already defined in SLS and must not be modified.

### ButtonSelectLight (`ui_ButtonSelectLight`)

- Cycles to the next light on each click
- Event handler: `buttonSelectLight_event_handler`

### ButtonLight (`ui_ButtonLight`)

- Publishes the toggle command for the currently selected light
- Event handler: `buttonLight_event_handler`
- Background color reflects state of selected light:
  - **Yellow** (`0xFFFF00`) — ON
  - **Dark grey** (`0x404040`) — OFF
  - **Purple** (`0x800080`) — UNKNOWN (before first status received)

### lightLabel (`ui_lightLabel`)

- Shows the `description` field of the currently selected light

---

## State Handling and Persistence

### Light on/off states

- Stored in RAM only (`lightStates[]`)
- Derived exclusively from MQTT status payloads
- All lights start as `UNKNOWN` at boot
- Not persisted in NVS (transient by design)

### Selected light index

- Persisted in NVS under namespace `"homepanel"`, key `"light_idx"`
- Loaded at `light_service_init()`
- Validated against `LIGHT_COUNT` on load (default to 0 if out of range)

### NVS write strategy

Identical to `temperature_service`:

- On selection change, set `nvsSavePending = true` and record `millis()`
- In `light_service_loop()`, after 30 seconds have elapsed:
  - Compare pending index vs last saved index
  - Only write if different
- This minimizes flash wear

---

## Button Debounce

Both buttons use 300 ms debounce (same as `temperature_service`):

```cpp
void buttonSelectLight_event_handler(lv_event_t* e) {
    static unsigned long lastClickTime = 0;
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if (millis() - lastClickTime >= 300) {
            lastClickTime = millis();
            light_service_cycleLight();
        }
    }
}

void buttonLight_event_handler(lv_event_t* e) {
    static unsigned long lastClickTime = 0;
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if (millis() - lastClickTime >= 300) {
            lastClickTime = millis();
            light_service_toggleCurrent();
        }
    }
}
```

---

## Public API Summary

**File: `src/light/light_service.h`**

```cpp
#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include <PubSubClient.h>

void light_service_init(lv_obj_t* selectBtn, lv_obj_t* lightBtn,
                        lv_obj_t* label, PubSubClient* mqtt);
void light_service_handleMQTT(const char* payload);
void light_service_cycleLight();
void light_service_toggleCurrent();
void light_service_loop();

#ifdef __cplusplus
extern "C" {
#endif
void buttonSelectLight_event_handler(lv_event_t* e);
void buttonLight_event_handler(lv_event_t* e);
#ifdef __cplusplus
}
#endif
```

---

## Integration Checklist (Changes to Existing Files)

### `src/net/net_module.h`

- Add `const char* light;` field to `NetTopics`

### `src/net/net_module.cpp`

- Subscribe to `cfg.topics.light` in both subscription locations (initial connect and reconnect)

### `home_panel.ino`

- Add `#include "src/light/light_service.h"`
- Define `#define TOPIC_LIGHT "m18toggle"`
- Pass `TOPIC_LIGHT` in `NetConfig.topics.light`
- Add `else if` branch in `mqttCallback()` routing to `light_service_handleMQTT()`
- Call `light_service_init(ui_ButtonSelectLight, ui_ButtonLight, ui_lightLabel, &mqttClient)` in `setup()`
- Call `light_service_loop()` in the main `loop()`

---

## Edge Cases and Considerations

### Boot state

All lights start as `UNKNOWN` (purple button). The UI correctly reflects that no status has been received yet. Status will update naturally as Node-RED publishes state.

### MQTT disconnection

If MQTT is disconnected, toggle presses will silently fail (`PubSubClient::publish()` returns false). The button color won't change because no status update will arrive. This is the correct behavior — the UI stays consistent with actual state.

### Rapid toggling

The 300 ms debounce prevents accidental double-toggles. Node-RED is responsible for handling toggle semantics, so even if two toggles arrive in sequence, the result is deterministic from Node-RED's perspective.

### Status for non-selected light

`light_service_handleMQTT()` updates `lightStates[]` for **all** lights, not just the selected one. The UI only refreshes the button color when the status matches the currently selected light. When the user cycles to another light, the button color is immediately set from the cached `lightStates[]` value.

### Adding new lights

Adding a light requires:

1. Define its payload constants (toggle + on/off status)
2. Add one entry to `lightMeta[]`

No other changes needed — `LIGHT_COUNT` is computed automatically.

---

## Summary of New Files

| File | Purpose |
|------|---------|
| `src/light/light_service.h` | Public API declarations |
| `src/light/light_service.cpp` | Full implementation (state, MQTT, UI, NVS) |

## Summary of Modified Files

| File | Change |
|------|--------|
| `src/net/net_module.h` | Add `light` field to `NetTopics` |
| `src/net/net_module.cpp` | Subscribe to light topic (2 locations) |
| `home_panel.ino` | Init, callback routing, loop call |
