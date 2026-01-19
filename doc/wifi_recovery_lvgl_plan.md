# WiFi Recovery & LVGL Time Update Improvement Plan

## Feasibility Analysis

### Validation Results

| Requirement                      | Status                  | Notes                                                            |
| -------------------------------- | ----------------------- | ---------------------------------------------------------------- |
| WiFi state machine support       | **Feasible**            | Current `checkWiFi()` can be replaced with state machine         |
| LVGL loop during recovery        | **Already Works**       | Main loop is non-blocking; `lv_timer_handler()` runs continuously |
| Timekeeping decoupled from WiFi  | **Already Implemented** | `time_service.cpp` uses ESP32 internal RTC offline               |

### Key Findings

1. **Time service already handles WiFi disconnection** - In `time_service.cpp` lines 117-124, the `labelTimerCallback` checks WiFi status and continues updating the time label using the ESP32's internal RTC via `getLocalTime()`.

2. **LVGL is non-blocking** - The UI runs via `lv_timer_handler()` in the main loop. WiFi operations are polling-based, so LVGL will never freeze during recovery.

3. **Minimal changes required** - Only `home_panel.ino` needs modification (~50 lines). No changes to time_service, net_module, or other modules.

---

## Concrete Implementation Plan

### Step 1: Add State Variables

Add near line 57 in `home_panel.ino`:

```cpp
// WiFi recovery state
enum WifiState { WIFI_CONNECTED, WIFI_RECOVERING, WIFI_FAILED };
WifiState wifiState = WIFI_CONNECTED;
unsigned long wifiDisconnectTime = 0;
constexpr unsigned long WIFI_RECOVERY_TIMEOUT_MS = 60000;  // 1 minute
constexpr unsigned long WIFI_RECONNECT_ATTEMPT_MS = 5000;  // 5 seconds between attempts
unsigned long lastWifiReconnectAttempt = 0;
```

### Step 2: Replace checkWiFi() Function

Replace current `checkWiFi()` (lines 230-235) with state machine:

```cpp
void checkWiFi() {
    bool connected = (WiFi.status() == WL_CONNECTED);

    switch (wifiState) {
        case WIFI_CONNECTED:
            if (!connected) {
                // WiFi just disconnected - start recovery
                wifiState = WIFI_RECOVERING;
                wifiDisconnectTime = millis();
                lastWifiReconnectAttempt = 0;
                Serial.println("WiFi: Disconnected, entering recovery mode...");
            }
            break;

        case WIFI_RECOVERING:
            if (connected) {
                // WiFi reconnected successfully
                wifiState = WIFI_CONNECTED;
                Serial.println("WiFi: Recovered successfully");
                time_service_sync();  // Re-sync time after recovery
                updateConnectionStatus();
            } else if (millis() - wifiDisconnectTime > WIFI_RECOVERY_TIMEOUT_MS) {
                // Timeout - trigger restart
                wifiState = WIFI_FAILED;
                Serial.println("WiFi: Recovery timeout, restarting...");
                restartESP();
            } else if (millis() - lastWifiReconnectAttempt > WIFI_RECONNECT_ATTEMPT_MS) {
                // Attempt reconnection
                lastWifiReconnectAttempt = millis();
                Serial.printf("WiFi: Reconnecting... (%lu/%lu ms)\n",
                              millis() - wifiDisconnectTime,
                              WIFI_RECOVERY_TIMEOUT_MS);
                WiFi.reconnect();
            }
            break;

        case WIFI_FAILED:
            // Should not reach here - restarting
            restartESP();
            break;
    }
}
```

### Step 3: Update Status Display

Modify `updateConnectionStatus()` to show recovery state:

```cpp
void updateConnectionStatus() {
    if (ui_labelConnectionStatus) {
        if (wifiState == WIFI_RECOVERING) {
            unsigned long elapsed = (millis() - wifiDisconnectTime) / 1000;
            char buf[64];
            snprintf(buf, sizeof(buf), "WiFi: Recovering (%lus)", elapsed);
            lv_label_set_text(ui_labelConnectionStatus, buf);
            lv_obj_set_style_text_color(ui_labelConnectionStatus,
                                        lv_color_hex(0xFFA500), LV_PART_MAIN);  // Orange
        } else if (WiFi.status() != WL_CONNECTED) {
            lv_label_set_text(ui_labelConnectionStatus, "WiFi: Disconnected");
            lv_obj_set_style_text_color(ui_labelConnectionStatus,
                                        lv_color_hex(0xFF0000), LV_PART_MAIN);
        } else if (!netIsMqttConnected()) {
            char buf[64];
            snprintf(buf, sizeof(buf), "IP: %s | No MQTT",
                     WiFi.localIP().toString().c_str());
            lv_label_set_text(ui_labelConnectionStatus, buf);
            lv_obj_set_style_text_color(ui_labelConnectionStatus,
                                        lv_color_hex(0xFFFF00), LV_PART_MAIN);
        } else {
            char buf[64];
            snprintf(buf, sizeof(buf), "IP: %s | MQTT %s",
                     WiFi.localIP().toString().c_str(), netGetMqttServerName());
            lv_label_set_text(ui_labelConnectionStatus, buf);
            lv_obj_set_style_text_color(ui_labelConnectionStatus,
                                        lv_color_hex(0x00FF00), LV_PART_MAIN);
        }
    }
}
```

### Step 4: Update Forward Declaration

Add `WifiState` enum before forward declarations or move enum to top of file.

---

## Files to Modify

| File              | Changes                                                                          |
| ----------------- | -------------------------------------------------------------------------------- |
| `home_panel.ino`  | Add state enum, variables, replace `checkWiFi()`, update `updateConnectionStatus()` |

No changes required to:

- `src/time/time_service.cpp` (already handles offline case)
- `src/net/net_module.cpp`
- Any UI files

---

## Expected Behavior After Implementation

1. **WiFi disconnects** → State changes to `WIFI_RECOVERING`
2. **Status label** → Shows "WiFi: Recovering (Xs)" in orange
3. **Time display** → Continues updating every second (unchanged)
4. **Reconnect attempts** → Every 5 seconds via `WiFi.reconnect()`
5. **WiFi recovers** → State returns to `WIFI_CONNECTED`, NTP re-syncs
6. **Timeout (60s)** → Device restarts into WiFiManager AP mode

---

## Original Request

### Context

Currently, when the WiFi access point is turned off:

- The module loses WiFi connectivity.
- The LVGL time and date display stops updating (seconds no longer change).
- After a short delay, the module fully restarts and enters **WiFiManager Access Point mode** so a new hotspot can be configured.

This behavior is functional but sub-optimal:

- A short WiFi outage forces a full reboot.
- The UI appears "frozen" even though only the network is down.
- Recovery could be faster and smoother without restarting the device.

---

### Objective

Improve WiFi disconnection handling so the module attempts to **recover WiFi for a longer period** before restarting, while keeping the UI fully functional.

---

### Proposed Behavior

#### 1. WiFi Recovery Window

- Introduce a constant:

```cpp
const uint32_t WIFI_RECOVERY_DELAY_MS = 60000;   // 1 minute
```

- When WiFi disconnects:
  - Start a **recovery timer**.
  - Attempt to reconnect to WiFi repeatedly during this period.
  - Do **not** reboot immediately.

#### 2. Successful Recovery

If WiFi reconnects **within the recovery window**:

- Cancel the reboot.
- Resume normal operation.
- Synchronize time if needed (NTP / MQTT / server).
- LVGL continues updating seamlessly.

#### 3. Failure After Timeout

If WiFi is **still not connected after 1 minute**:

- Restart the module.
- Launch WiFiManager in Access Point mode as currently implemented.

---

### UI Requirement

During the WiFi recovery period:

- The **LVGL clock must keep updating every second**:
  - Time
  - Date
  - Seconds animation
- The UI must **never freeze** just because WiFi is down.

This implies:

- Timekeeping must rely on:
  - `millis()`-based RTC logic, or
  - an internal software clock updated every second, not directly on WiFi/NTP availability.

---

### Validation

Please validate that:

1. The current WiFi state machine can support a recovery phase without rebooting.
2. The LVGL task loop can continue running even while WiFi is reconnecting.
3. Timekeeping can be decoupled from WiFi so seconds continue to tick offline.
