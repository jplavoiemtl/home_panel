# Temperature Status Request on Boot – Implementation Plan

## Context and Goal

After a reboot, all temperatures show `"--"` until each source happens to publish
a new reading. Depending on update intervals, this can leave the panel without
temperature data for several minutes.

The goal is to **request the current temperature values at boot** and after any
**MQTT reconnection**, following the same pattern used for the light status request.

---

## Design Overview

1. **ESP32** publishes `"status"` to the `weather` topic at boot and on MQTT reconnect
2. **Node-RED** receives `"status"` and responds by publishing the latest cached temperature for each source
3. **ESP32** receives these via the existing `temperature_service_handleMQTT()` — no parsing changes needed

---

## ESP32 Changes

### `src/temperature/temperature_service.h`

Add a new public function and accept PubSubClient pointer at init:

```cpp
void temperature_service_init(lv_obj_t* locLabel, lv_obj_t* tempLabel,
                              lv_obj_t* timeLabel, PubSubClient* mqtt);
void temperature_service_requestStatus();
```

### `src/temperature/temperature_service.cpp`

Add MQTT topic and status payload constants:

```cpp
static const char TOPIC_WEATHER[] = "weather";
static const char PAYLOAD_STATUS[] = "status";
```

Store the MQTT client pointer in module state:

```cpp
PubSubClient* mqtt = nullptr;
```

Update `temperature_service_init()` to accept and store the MQTT pointer, and
call `temperature_service_requestStatus()` at the end.

Implement `temperature_service_requestStatus()`:

```cpp
void temperature_service_requestStatus() {
    if (mqtt && mqtt->connected()) {
        mqtt->publish(TOPIC_WEATHER, PAYLOAD_STATUS);
        Serial.println("Temperature service: requested status from Node-RED");
    }
}
```

### `home_panel.ino`

Update the init call to pass the MQTT client:

```cpp
temperature_service_init(ui_tempLocLabel, ui_labelOutsideTemp,
                         ui_tempTimeLabel, &mqttClient);
```

Add `temperature_service_requestStatus()` to the existing MQTT reconnection
detector alongside the light status request:

```cpp
if (isMqttConnected && !wasMqttConnected) {
    light_service_requestStatus();
    temperature_service_requestStatus();
}
```

---

## Node-RED Changes

Two types of changes are needed:

### Step 1: Cache latest temperatures in global context

Each temperature function node needs one `global.set()` line added to store the
latest value. The Salon node already does this — the same pattern is applied to
the other four.

**Bureau JP** — node `c2c642a2f8f47d5b` (Air quality tab, name: "temperature")

Add after the `let temp = ...` line:

```javascript
global.set("temp_burjp", temp);
```

**Maitre** — node `b10b04ce8adf23f8` (Air quality tab, name: "temperature")

Add after the `let temp = ...` line:

```javascript
global.set("temp_maitre", temp);
```

**Salon** — node `d51909d3ef03b7a5` (Chauffage tab, name: "Influx Salon")

No change needed. This node already sets `global.set("objet.ambiantTemp", temperature)`.
The status response flow reads this existing global directly.

**Outside** — node `c67d750c.62d5c8` (Weather and Sun tab, name: "Outside temp")

Add after the `let temp = ...` line:

```javascript
global.set("temp_outside", temp);
```

**Myriam** — node `e6d1b606e912487e` (Yolink sensors tab, name: "Myriam")

Add after the `let temp = ...` line (inside the function, before the return):

```javascript
global.set("temp_myriam", temp);
```

### Step 2: Create status response flow (importable)

Create a new flow with 3 nodes that you can import via Import > Clipboard:

**Node 1: MQTT In** — subscribes to `weather` topic

**Node 2: Function** — checks for `"status"` payload, builds response messages

```javascript
// Only respond to status requests
if (msg.payload !== "status") {
    return null;
}

// Read cached temperatures from global context
var burjp   = global.get("temp_burjp");
var maitre  = global.get("temp_maitre");
var salon   = global.get("objet.ambiantTemp");
var outside = global.get("temp_outside");
var myriam  = global.get("temp_myriam");

// Build individual messages (same format as normal updates)
var msgs = [];

if (burjp !== undefined && burjp !== null) {
    msgs.push({ topic: "weather", payload: { BurjpTemp: burjp } });
}
if (maitre !== undefined && maitre !== null) {
    msgs.push({ topic: "weather", payload: { MaitreTemp: maitre } });
}
if (salon !== undefined && salon !== null) {
    msgs.push({ topic: "weather", payload: { AmbientTemp: salon } });
}
if (outside !== undefined && outside !== null) {
    msgs.push({ topic: "weather", payload: { OutsideTemp: outside } });
}
if (myriam !== undefined && myriam !== null) {
    msgs.push({ topic: "weather", payload: { MyriamTemp: myriam } });
}

// Return array of messages (sent sequentially)
return [msgs];
```

**Node 3: MQTT Out** — publishes to broker (topic set by msg.topic)

Wire: MQTT In → Function → MQTT Out

### Why the message array approach is safe (no delays needed)

The function returns an array of messages via `return [msgs]`. Node-RED sends
these sequentially to the MQTT Out node. This is safe because:

- The MQTT broker (Mosquitto) queues messages internally and delivers them to
  subscribers in order
- Each payload is tiny (under 50 bytes), well within PubSubClient's buffer
- The ESP32 processes one message per `mqttClient.loop()` call in its main loop,
  so messages are handled one at a time naturally
- With only 5 messages, there is no risk of buffer overflow or message loss

If this ever needed to scale to dozens of messages, a delay node (e.g., 50 ms
rate limit) could be added between the function and MQTT Out. For 5 messages,
it is not necessary.

---

## Timestamp Behavior

The ESP32 stamps each temperature with the current time when it receives the MQTT
message. On a status response, the timestamp will reflect when the panel received
the cached value, not when the original sensor reading was taken. This is acceptable:

- The panel shows "when I last got this data" which is technically accurate
- The next periodic update from each sensor will overwrite with a fresh reading
- The goal is to avoid `"--"` for minutes after reboot, not sub-minute precision

---

## Edge Cases

### Node-RED restart

If Node-RED restarts, global context is cleared. The status response will return
no temperatures until each source publishes its next reading. The ESP32 will show
`"--"` until then — same as today, no regression.

### Partial data

If only some globals are set (e.g., Node-RED just restarted and only Outside has
published), the function only returns messages for available temperatures. The ESP32
handles partial updates correctly — each temperature is independent.

### MQTT not connected at ESP32 boot

Same as the light service: the publish in init silently fails, and the reconnection
detector in `loop()` catches the transition and re-requests.

### Duplicate status requests (echo loop)

Because the ESP32 is subscribed to the `weather` topic and also publishes `"status"`
to it, the broker echoes the message back. Node-RED sees both the original and the
echo, causing a double response. This is resolved with a **2-second debounce** in
the Node-RED status response function:

```javascript
var now = Date.now();
var last = context.get("lastStatus") || 0;
if (now - last < 2000) {
    return null;
}
context.set("lastStatus", now);
```

---

## Summary of Changes

| Component | File/Node | Change |
|-----------|-----------|--------|
| ESP32 | `temperature_service.h` | Add `PubSubClient*` to init, add `requestStatus()` |
| ESP32 | `temperature_service.cpp` | Store mqtt pointer, implement `requestStatus()` |
| ESP32 | `home_panel.ino` | Update init call, add to reconnect detector |
| Node-RED | Bureau JP function | Add `global.set("temp_burjp", temp)` |
| Node-RED | Maitre function | Add `global.set("temp_maitre", temp)` |
| Node-RED | Salon function | No change — already has `global.set("objet.ambiantTemp", temperature)` |
| Node-RED | Outside function | Add `global.set("temp_outside", temp)` |
| Node-RED | Myriam function | Add `global.set("temp_myriam", temp)` |
| Node-RED | New flow | MQTT In → Function → MQTT Out (status response) |
