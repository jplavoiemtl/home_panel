# Light Status Request on Boot – Implementation Plan

## Context and Goal

After a reboot, all lights show as UNKNOWN (purple) until someone toggles a light
and Node-RED publishes a status update. This can leave the panel in an inaccurate
state for hours if no one interacts with the lights.

The goal is to **request the current status of all lights at boot** and after any
**MQTT reconnection** so the panel displays accurate ON/OFF states within seconds.

---

## Design Overview

A simple request/response pattern over the existing `m18toggle` MQTT topic:

1. **ESP32** publishes a `"status"` payload to `m18toggle` at boot and after any MQTT reconnection
2. **Node-RED** receives the `"status"` payload and responds by publishing the current ON/OFF status for each light (e.g., `cu_on`, `sa_of`, `st_on`, etc.)
3. **ESP32** receives these status payloads through the existing `light_service_handleMQTT()` — no changes needed on the receive side

---

## ESP32 Changes

### `src/light/light_service.h`

Add a new public function:

```cpp
// Request current light status from Node-RED via MQTT
void light_service_requestStatus();
```

### `src/light/light_service.cpp`

Add a payload constant:

```cpp
static const char PAYLOAD_STATUS[] = "status";
```

Implement `light_service_requestStatus()`:

```cpp
void light_service_requestStatus() {
    if (mqtt && mqtt->connected()) {
        mqtt->publish(TOPIC_LIGHT, PAYLOAD_STATUS);
        Serial.println("Light service: requested status from Node-RED");
    }
}
```

Call it at the end of `light_service_init()`, after `updateUI()`:

```cpp
light_service_requestStatus();
```

No other light service changes are needed. The existing `light_service_handleMQTT()`
already handles all status payloads (`cu_on`, `sa_of`, etc.) and updates the UI.

### `home_panel.ino`

Call `light_service_requestStatus()` after a successful MQTT reconnection.
The `netCheckMqtt()` function in `net_module` handles reconnection, and the
`mqttCallback` is already wired. The cleanest approach is to detect reconnection
in the main loop and call the status request:

```cpp
// In loop(), after netCheckMqtt():
static bool wasMqttConnected = false;
bool isMqttConnected = mqttClient.connected();
if (isMqttConnected && !wasMqttConnected) {
    light_service_requestStatus();
}
wasMqttConnected = isMqttConnected;
```

This detects any transition from disconnected to connected (both initial boot
connection and reconnections after drops) and requests fresh status.

---

## Node-RED Changes

Add a flow that:

1. Listens on topic `m18toggle` for the payload `"status"`
2. For each light, reads the current state from the light's Node-RED state/context
3. Publishes the corresponding status payload (e.g., `cu_on` or `cu_of`) back to `m18toggle`

The response payloads should be sent with a small delay between each (e.g., 50–100 ms)
to avoid overwhelming the ESP32's MQTT receive buffer.

---

## Edge Cases

### MQTT not connected at boot

If MQTT is not yet connected when `light_service_init()` runs, the publish in init
will silently fail. The reconnection detection in `loop()` will catch the transition
to connected and re-request status automatically. No lights will stay UNKNOWN
indefinitely.

### MQTT disconnection mid-operation

When MQTT drops and reconnects, the connection state tracker in `loop()` detects
the transition and requests fresh status. This ensures lights return to accurate
states after any network interruption.

### Node-RED restart

If Node-RED restarts and loses light state context, it will respond with whatever
default state it has. This is a Node-RED-side concern and outside the scope of this
plan.

### Duplicate status messages

The ESP32 may receive status payloads it has already processed (e.g., if another
device also triggers a status request). This is harmless — `light_service_handleMQTT()`
is idempotent and simply overwrites the existing state.

---

## Summary of Changes

| Component | Change |
|-----------|--------|
| `src/light/light_service.h` | Add `light_service_requestStatus()` declaration |
| `src/light/light_service.cpp` | Implement `light_service_requestStatus()`, call it from init |
| `home_panel.ino` | Add MQTT connection state tracker in `loop()` to re-request on reconnect |
| Node-RED | Add flow to respond to `"status"` with current light states |
