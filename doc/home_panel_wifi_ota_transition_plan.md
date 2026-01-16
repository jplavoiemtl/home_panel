
# Home Panel – WiFiManager & OTA Transition Plan

## Purpose

The Home Panel project is currently stable and has been running reliably using hardcoded WiFi and MQTT credentials. While this approach works, it limits flexibility when deploying the module on different networks and makes firmware updates more cumbersome.

The goal of this plan is to **introduce WiFiManager and ElegantOTA** to:
- Remove hardcoded WiFi credentials
- Allow dynamic WiFi configuration via a captive portal
- Enable over-the-air (OTA) firmware updates through a web interface
- Preserve the long-term reliability of the existing system

**This document is strictly a planning and review document. No code changes should be made at this stage.**

---

## Target Libraries

- **WiFiManager**  
  Used to manage WiFi credentials dynamically via a captive portal.

- **ElegantOTA**  
  https://github.com/ayushsharma82/ElegantOTA  
  Used to upload and install firmware updates through a browser at `/update`.

---

## Reference Implementation (Do Not Modify)

The folder **`power_display_reference_code`** contains a working and proven reference implementation:
- File: `power_display.ino`
- Platform: ESP32-S3
- Status: Stable and running successfully for months

⚠️ **This code must not be edited.**  
It serves as the baseline reference for how WiFiManager and ElegantOTA should be integrated into the Home Panel project.

All design and implementation decisions for Home Panel should align with the patterns used in this reference sketch.

---

## Expected Module Operation

### Power-Up Sequence

1. On boot, the module attempts to connect to WiFi using credentials stored by WiFiManager.
2. If the connection fails:
   - The device automatically enters **WiFiManager captive portal mode**
   - The user can configure a new WiFi network
3. Once WiFi is connected:
   - Normal operation resumes
   - MQTT connection logic proceeds as usual

### OTA Firmware Updates

- ElegantOTA provides a web interface accessible at:
  ```
  http://<device-ip>/update
  ```
- Firmware updates can be applied without:
  - Physical access
  - Reflashing via USB
  - Power cycling the device

---

## Key Reference Code Patterns to Preserve

### WiFiManager Configuration Timeout

This section of the reference code is important and should be mirrored in the Home Panel project.  
**Comments must be preserved**, as they are critical for maintenance and debugging.

```cpp
wm.setConfigPortalTimeout(180);
wm.setAPCallback(configModeCallback);  // Set the callback function
// reset settings - wipe stored credentials for testing
// these are stored by the esp library
// wm.resetSettings();
```

This allows:
- Automatic exit from captive portal mode
- Optional credential reset during development or troubleshooting

---

### Automatic Restart on WiFi Failure

The reference code includes a robust self-recovery mechanism to handle WiFi outages (for example, when a hotspot is temporarily turned off).

```cpp
bool res;
res = wm.autoConnect("PowerDisp24","password"); // password protected AP

if(!res) {
    restartESP();
} 
else {
    Serial.println("Connected to WiFi");
}
```

This logic ensures:
- The device restarts automatically if WiFi is unavailable
- The module recovers on its own when WiFi and MQTT services return
- No manual unplugging or physical intervention is required

This behavior is **highly desirable** and should be preserved in the Home Panel project.

---

## Design Principles for the Transition

- Keep existing WiFi and MQTT modules **as intact as possible**
- Adapt them only where necessary to match the reference behavior
- Follow the same initialization and recovery strategy used in `power_display.ino`
- Prioritize reliability and self-recovery over new features
- Avoid unnecessary complexity

The intent is not to redesign the system, but to **extend it safely**.

---

## Reliability Expectations

- The module must recover automatically after:
  - WiFi outages
  - Router restarts
  - MQTT broker restarts
- OTA updates should be:
  - Rare
  - Intentional
  - Non-disruptive to long-term stability

The device should continue to behave as a fully autonomous module once deployed.

---

## Review & Planning Tasks

Before any code changes:

1. Review `power_display.ino` in detail
2. Identify:
   - WiFi initialization flow
   - OTA initialization timing
   - Restart and recovery logic
3. Compare with the current Home Panel architecture
4. Define:
   - Which modules remain unchanged
   - Which modules require adaptation
5. Review possible edge cases:
   - WiFi credentials change
   - Temporary network loss
   - MQTT broker downtime

---



## WiFi and MQTT Strategy for Home Panel

Currently, the Home Panel project uses **two build options**:
- **HOME build**: Connects to a local network MQTT server (no TLS)
- **CAR build**: Connects to a remote MQTT server over iPhone hotspot with TLS certificates

The proposed transition using WiFiManager will **consolidate these builds into a single configuration**:
1. WiFiManager will handle a single WiFi connection dynamically
2. Once connected to WiFi:
   - The module will attempt a connection to the **local MQTT server without TLS** first
   - If successful, the module assumes it is on the local network
   - This network state is saved in **ESP32 NVS memory**
3. On subsequent power-ups:
   - The module reads the MQTT configuration from NVS
   - Connects automatically to the correct MQTT server (local or remote with TLS)
4. If the WiFi configuration changes via the captive portal:
   - The MQTT configuration in NVS is updated
   - Ensures fast and correct connection on next startup

This strategy removes the need for separate builds while maintaining **reliable network detection and automatic MQTT configuration**.


## Scope Limitation

❌ **Out of scope for this phase**

- Code refactoring
- Feature additions
- UI changes
- MQTT redesign

✅ **In scope**

- Planning
- Architecture review
- Risk assessment
- Transition strategy definition

---

## Architectural Decisions (Approved)

The following decisions have been reviewed and approved:

| Question | Decision |
|----------|----------|
| Display during captive portal | **Accept freeze** – simpler implementation, portal is infrequent |
| OTA credentials | **Use reference credentials** (`"jp"/"delphijpl"`) – update later |
| AP name | **`"homepanel"`** – simple and descriptive |
| NVS namespace | **`"homepanel"`** with key `mqtt_mode` |
| Physical reset button | **None** – use `wm.resetSettings()` in code for development only |

---

## Implementation Plan

### Phase A: WiFiManager Integration

**Goal**: Replace hardcoded WiFi with captive portal configuration.

**Steps**:

1. Add WiFiManager library dependency
2. Replace `initWiFi()` in `home_panel.ino` with WiFiManager pattern:

   ```cpp
   WiFiManager wm;
   wm.setConfigPortalTimeout(180);
   wm.setAPCallback(configModeCallback);
   // wm.resetSettings();  // Uncomment for testing only

   bool res = wm.autoConnect("homepanel", "password");
   if (!res) {
       restartESP();
   }
   ```

3. Add `configModeCallback()` to update display status label
4. Add `restartESP()` function (10s delay then `ESP.restart()`)
5. Remove dual-network fallback logic (ssid1/ssid2 selection)
6. Update `checkWiFi()` to use restart-based recovery instead of reconnect loop

**Files Modified**:

- `home_panel.ino` – WiFi init, loop recovery logic
- `secrets_private.h` – Remove ssid1/ssid2 extern declarations (optional, can keep as fallback)

---

### Phase B: ElegantOTA Integration

**Goal**: Enable over-the-air firmware updates via web interface.

**Steps**:

1. Add ElegantOTA and WebServer library dependencies
2. Add WebServer instance on port 80
3. Initialize ElegantOTA after WiFi connects:

   ```cpp
   ElegantOTA.begin(&server, "jp", "delphijpl");
   ElegantOTA.onStart(onOTAStart);
   ElegantOTA.onProgress(onOTAProgress);
   ElegantOTA.onEnd(onOTAEnd);

   server.on("/", []() {
       server.send(200, "text/plain", "Home Panel module");
   });
   server.begin();
   ```

4. Add OTA callback functions for logging
5. Add `server.handleClient()` and `ElegantOTA.loop()` to main loop
6. Verify partition scheme supports OTA (2 app partitions required)

**Files Modified**:

- `home_panel.ino` – WebServer, OTA init, loop handlers

---

### Phase C: NVS MQTT Auto-Detection

**Goal**: Automatically detect and remember MQTT server preference (home vs remote).

**Steps**:

1. Add NVS helper functions to `net_module.cpp`:

   ```cpp
   // NVS keys
   // Namespace: "homepanel"
   // Key: "mqtt_mode" -> 0=unknown, 1=home, 2=remote

   int nvsReadMqttMode();
   void nvsWriteMqttMode(int mode);
   void nvsClearMqttMode();
   ```

2. Implement MQTT probe logic:
   - On boot, read NVS `mqtt_mode`
   - If unknown (0): Try local MQTT (port 1883) first
     - Success → save mode=1 (home), use local server
     - Fail → save mode=2 (remote), use TLS server
   - If known (1 or 2): Connect directly to saved preference

3. Add WiFiManager save callback to clear NVS when WiFi changes:

   ```cpp
   wm.setSaveConfigCallback([]() {
       nvsClearMqttMode();  // Force re-probe on next boot
   });
   ```

4. Update `netConfigureMqttClient()` to use NVS-based mode selection

**Files Modified**:

- `net_module.cpp` – NVS functions, probe logic
- `net_module.h` – New function declarations
- `home_panel.ino` – WiFiManager save callback

---

## Potential Challenges & Edge Cases

### WiFi-Related

| Challenge | Risk Level | Mitigation |
|-----------|------------|------------|
| Captive portal blocks `loop()` | Low | Accepted – display freezes during config only |
| Portal timeout (180s) | Low | Proven value from reference; restart recovers |
| Hidden networks | Low | WiFiManager supports manual SSID entry |
| Accidental credential reset | Medium | Keep `wm.resetSettings()` commented out |

### MQTT Configuration

| Challenge | Risk Level | Mitigation |
|-----------|------------|------------|
| Local MQTT probe false positive | Low | Specific port (1883) + quick timeout |
| TLS handshake delay | Low | Probe non-TLS first (faster fail) |
| NVS write wear | Low | Only write on actual change |
| Network switching mid-session | Medium | Re-probe only when WiFi SSID changes via portal |

### Recovery & Reliability

| Challenge | Risk Level | Mitigation |
|-----------|------------|------------|
| WiFi lost during OTA | Low | ElegantOTA uses dual partition (safe) |
| Router restart | Low | ESP restart + auto-reconnect pattern |
| MQTT broker down | Low | Existing 15s retry is appropriate |
| Power loss during NVS write | Low | ESP32 NVS uses atomic writes |

---

## Testing Matrix

| Test Case | Expected Behavior |
|-----------|-------------------|
| Fresh boot (no credentials) | Enters captive portal, shows "homepanel" AP |
| Normal boot (saved credentials) | Auto-connects to saved WiFi |
| WiFi disappears mid-operation | Restarts ESP, reconnects when WiFi returns |
| Router reboot | Auto-recovers after restart cycle |
| MQTT broker down | Retries every 15s, reconnects when available |
| OTA update via browser | Succeeds at `http://<ip>/update` |
| Portal timeout (180s) | Restarts ESP, does not hang |
| First boot on home network | Probes local MQTT, saves mode=1 |
| First boot on iPhone hotspot | Local probe fails, uses TLS, saves mode=2 |
| WiFi changed via portal | NVS cleared, re-probes MQTT on next boot |

---

## File Change Summary

| File | Phase | Changes |
|------|-------|---------|
| `home_panel.ino` | A, B | Replace `initWiFi()`, add WebServer, add ElegantOTA, add callbacks |
| `net_module.cpp` | C | Add NVS read/write, add MQTT probe logic |
| `net_module.h` | C | Add NVS function declarations |
| `secrets_private.h` | A | Remove HOME/CAR build flags (optional) |
| `platformio.ini` or build config | A, B | Add library dependencies, verify OTA partition |

---

## Pre-Implementation Checklist

- [ ] Confirm WiFiManager library version compatibility with ESP32-S3
- [ ] Confirm ElegantOTA library version (sync mode preferred for simplicity)
- [ ] Verify partition scheme has 2 app partitions for OTA
- [ ] Document current flash partition table
- [ ] Back up working firmware before changes

---

## Summary

This plan defines a careful and conservative transition toward WiFiManager and ElegantOTA for the Home Panel project, based on a proven and stable reference implementation. The emphasis is on reliability, autonomy, and maintainability.

The implementation is structured in three phases:

- **Phase A**: WiFiManager (captive portal, auto-recovery)
- **Phase B**: ElegantOTA (web-based firmware updates)
- **Phase C**: NVS MQTT auto-detection (unified build, no HOME/CAR split)

Each phase can be tested independently before proceeding to the next.

**Status**: Plan reviewed and approved. Ready for implementation.
