**Phase C – MQTT NVS Auto-Detection & Fallback Strategy**

This document describes the MQTT configuration, NVS storage, and fallback logic for Phase C.

---

### 1. Core Principle

MQTT configuration stored in NVS should only be updated when the **MQTT server changes**, not when the WiFi network changes.

WiFi and MQTT are independent:

* WiFi is managed by **WiFiManager** (Phase A)
* MQTT servers can be **LOCAL** (non-TLS) or **REMOTE** (TLS)
* Switching WiFi does **not** imply switching MQTT

Because MQTT servers rarely change, NVS writes will be rare and will not cause wear issues.

---

### 2. NVS MQTT Storage

**Namespace:** `homepanel`

**Key:** `mqtt_server`

**Values:** `LOCAL` or `REMOTE`

**Default:** `LOCAL` (when NVS is empty on first boot)

---

### 3. Connection Logic

#### 3.1 On Boot

1. Read `mqtt_server` from NVS (default to `LOCAL` if empty)
2. Configure MQTT client for stored server
3. Attempt connection (single attempt, bypass rate limit)
4. If connection **succeeds** → done, do not write to NVS
5. If connection **fails** → try fallback server:
   - If stored = `LOCAL`, try `REMOTE`
   - If stored = `REMOTE`, try `LOCAL`
6. If fallback **succeeds** → update NVS with new server value
7. If both **fail** → show "No MQTT", retry periodically

#### 3.2 During Session (Reconnection)

* Use existing 15-second reconnect interval from `net_module.cpp`
* Retry the current/stored server (they are always the same)
* No fallback logic during mid-session reconnects
* If disconnected for extended period, show "No MQTT" status

#### 3.3 Both Servers Fail

* Retry every 30 seconds
* Alternate between LOCAL and REMOTE on each retry cycle
* Update NVS only when a server successfully connects
* Display "No MQTT" until connection is restored

---

### 4. UI Update (Screen 1)

Update the MQTT status label to show one of:

| State | Label Text |
|-------|------------|
| Connected to LOCAL | `MQTT Local` |
| Connected to REMOTE | `MQTT Remote` |
| Not connected | `No MQTT` |

Keep it simple: show only the connected state, not "connecting" state.

---

### 5. Code Changes

#### 5.1 net_module.cpp

Add new functions:

```cpp
// NVS functions
void netLoadMqttServerFromNVS();     // Load stored server preference
void netSaveMqttServerToNVS();       // Save current server to NVS
int netGetCurrentMqttServer();       // Returns 1 (LOCAL) or 2 (REMOTE)
const char* netGetMqttServerName();  // Returns "Local" or "Remote"

// Enhanced connection with fallback
bool netConnectMqttWithFallback();   // Try stored, then fallback, update NVS
```

#### 5.2 secrets_private.h/.cpp Simplification

Remove:

* `HOME` / `CAR` build mode defines
* `ssid1`, `password1`, `ssid2`, `password2` (WiFiManager handles WiFi)

Keep:

* `SERVER1` → LOCAL MQTT server (non-TLS, port 1883)
* `SERVER2` → REMOTE MQTT server (TLS, port 9735)
* `SERVERPORT1`, `SERVERPORT2`
* `USERNAME`, `KEY`, `CLIENT_ID`
* `ca_cert` for TLS connections
* `remote_server_ca_cert` for HTTPS image server

#### 5.3 home_panel.ino

Update `initMQTT()` to use fallback logic:

```cpp
void initMQTT() {
    netLoadMqttServerFromNVS();  // Load preference (defaults to LOCAL)

    if (!netConnectMqttWithFallback()) {
        Serial.println("MQTT: Both servers failed, will retry periodically");
    }

    updateConnectionStatus();
}
```

Update `updateConnectionStatus()` to show MQTT server name:

```cpp
if (netIsMqttConnected()) {
    snprintf(buf, sizeof(buf), "MQTT %s", netGetMqttServerName());
} else {
    snprintf(buf, sizeof(buf), "No MQTT");
}
```

---

### 6. TLS Handling

TLS is already implemented in `net_module.cpp`:

* `isSecurePort()` detects secure ports (9735, 8883)
* `netConfigureMqttClient()` sets up `WiFiClientSecure` with CA cert for TLS
* `ca_cert` is defined in `secrets_private.cpp`

No changes needed for TLS support.

---

### 7. Result

* **One firmware build** (no HOME/CAR modes)
* **One WiFiManager flow** (Phase A complete)
* **One MQTT auto-detection system** with bidirectional fallback
* **NVS updated only when MQTT server actually changes**
* **Clear UI feedback** showing connection status

---

### 8. Implementation Order

1. Add NVS read/write functions for MQTT preference
2. Implement fallback connection logic
3. Update `initMQTT()` to use new logic
4. Update UI status display
5. Simplify `secrets_private.h/.cpp` (remove WiFi credentials, build modes)
6. Test LOCAL → REMOTE fallback
7. Test REMOTE → LOCAL fallback
8. Test NVS persistence across reboots
