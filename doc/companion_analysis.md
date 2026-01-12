# Companion Reference Code Analysis

> **Purpose:** This document captures the analysis of the `companion_reference_code/` folder to guide porting decisions for the Home Panel project.

---

## 1. Folder Structure Overview

```
companion_reference_code/
├── companion.ino                 # Main sketch - entry point (~28KB)
├── calibration.cpp/h             # IMU calibration with NVS persistence
├── pin_config.h                  # Hardware pin definitions
├── secrets.h                     # Build config (HOME/CAR), MQTT topics
├── secrets_private.h             # WiFi credentials, servers, TLS certs
├── lv_conf.h                     # LVGL configuration
├── src/
│   ├── image/
│   │   ├── image_fetcher.cpp     # HTTP/S JPEG fetching & decode
│   │   └── image_fetcher.h       # Config struct, public API
│   ├── imu/
│   │   ├── imu_module.cpp        # QMI8658 driver, motion, inclinometer
│   │   └── imu_module.h          # Public IMU API & state exports
│   ├── net/
│   │   ├── net_module.cpp        # MQTT dual-server failover
│   │   └── net_module.h          # NetConfig struct, MQTT client mgmt
│   └── screen_memory/
│       ├── screen_memory.cpp     # NVS screen persistence (30s debounce)
│       └── screen_memory.h       # ScreenMemoryConfig, API
├── ui*.c/h                       # SquareLine Studio generated UI files
└── ui_img_*.c                    # Generated image assets
```

**Total Files:** ~46 source files

- **Core Modules:** 4 module pairs in `src/`
- **UI Generated:** ~28 SquareLine Studio artifacts
- **Config/Build:** ~8 headers + main sketch

---

## 2. Module Classification

### 2.1 REUSE Modules (Port as-is or with minimal changes)

#### Network Module (`src/net/net_module.cpp/h`)

| Attribute | Details |
|-----------|---------|
| **Purpose** | MQTT dual-server failover, pub/sub management |
| **Dependencies** | PubSubClient 2.8, WiFiClient, WiFiClientSecure |
| **API** | `netInit()`, `netConfigureMqttClient()`, `netCheckMqtt()`, `netIsMqttConnected()`, `netHasInitialMqttSuccess()` |
| **Config** | NetConfig struct with servers, ports, TLS certs, topics |
| **Features** | 15-second reconnection interval, secure port detection (9735/8883) |
| **Porting Notes** | Minimal adaptation - redirect MQTT callbacks to Home Panel handlers |

#### Image Fetcher Module (`src/image/image_fetcher.cpp/h`)

| Attribute | Details |
|-----------|---------|
| **Purpose** | HTTP/S JPEG image fetching, decode to PSRAM, LVGL display |
| **Dependencies** | HTTPClient, TJpg_Decoder, WiFi/WiFiClientSecure, LVGL |
| **API** | `imageFetcherInit()`, `imageFetcherLoop()`, `requestLatestImage()`, button handlers |
| **Config** | ImageFetcherConfig with screen dimensions, screen objects |
| **Features** | 15s HTTP timeout, 60KB max JPEG, Screen2 auto-hide after 60s |
| **Decoding** | TJpg_Decoder callback-based, writes RGB565 to PSRAM |
| **Porting Notes** | Ready to reuse - handles all image display logic |

---

### 2.2 EXCLUDE Modules (Do NOT port)

#### IMU Module (`src/imu/imu_module.cpp/h`)

| Attribute | Details |
|-----------|---------|
| **Purpose** | QMI8658 sensor driver, motion detection, inclinometer |
| **Dependencies** | SensorQMI8658.hpp, calibration.h, I2C mutex |
| **Features** | Complementary filter for pitch/roll, motion thresholds, peak tracking |
| **Exports** | `g_isCurrentlyMoving`, `pitch_angle`, `roll_angle` |
| **Exclusion Reason** | No IMU hardware on Home Panel |

#### Calibration Module (`calibration.cpp/h`)

| Attribute | Details |
|-----------|---------|
| **Purpose** | IMU calibration (gravity vector, rotation matrix) with NVS |
| **Features** | Two-step calibration for car-based IMU |
| **NVS Namespace** | "imuCal" |
| **Exclusion Reason** | Only needed for car-based IMU applications |

---

### 2.3 ADAPT Modules (Modify for Home Panel)

#### Screen Memory Module (`src/screen_memory/screen_memory.cpp/h`)

| Attribute | Details |
|-----------|---------|
| **Purpose** | Persist active screen to NVS, restore on boot |
| **API** | `screenMemoryInit()`, `screenMemoryUpdate()`, `screenMemoryOnScreenLoaded()` |
| **Behavior** | 30-second debounce before saving (prevents NVS wear) |
| **NVS Namespace** | "screenMem" (key: "lastScr") |
| **Adaptation** | Modify to track only Home Panel screens (2 screens vs 5) |

---

## 3. LVGL Screens Inventory

### Companion Screens

| Screen | Purpose | Persistent | Port to Home Panel |
|--------|---------|------------|-------------------|
| **ui_Screen1** | Dashboard (power, energy, buttons) | YES | YES - Home Screen |
| **ui_Screen2** | Image viewer | NO (temp) | YES - Image Display |
| **ui_Screen3** | G-Meter display | YES | NO - Exclude |
| **ui_InclinometerScreen** | Pitch/Roll angles | YES | NO - Exclude |
| **ui_calibrationScreen** | IMU Calibration UI | NO (temp) | NO - Exclude |

### Key LVGL Objects to Reuse

**From ui_Screen1 (Dashboard):**

- `ui_labelPowerValue` - Power display (kW)
- `ui_labelEnergyValue` - Energy display (kWh)
- `ui_labelConnectionStatus` - Network status
- `ui_ActivitySpinner` - Loading indicator
- Camera request buttons (Latest, New, Back)

**From ui_Screen2 (Image Viewer):**

- `ui_imgScreen2Background` - Image display area
- `ui_screen2Text` - Status text
- `ui_Button2` - Back/navigation button

### Objects to EXCLUDE (not present on Home Panel)

- `ui_labelBatteryPercent` - Battery percentage
- `ui_labelBatteryVoltage` - Battery voltage
- `ui_labelBatteryStatus` - Charging status
- `ui_labelMotionIcon` - Motion indicator (all screens)
- All ui_Screen3 objects (G-Meter)
- All ui_InclinometerScreen objects
- All ui_calibrationScreen objects

---

## 4. Initialization Flow

### Setup Sequence (from `setup()`)

```
1.  USBSerial.begin(115200)
2.  Create I2C mutex
3.  Wire.begin() - I2C bus
4.  initPMIC() - AXP2101 power management          [ADAPT - no battery]
5.  initIOExpander() - TCA9554 GPIO expander       [REVIEW - needed?]
6.  initTouch() - FT3168 capacitive touch          [ADAPT - different driver]
7.  initDisplay() - SH8601 AMOLED driver           [ADAPT - different driver]
8.  initLVGL() - LVGL library, draw buffers
9.  initUIHandlers() - Add LVGL event callbacks
10. screenMemoryInit() - Restore last screen
11. initPSRAM() - Verify PSRAM available
12. imageFetcherInit() - HTTP image fetching       [REUSE]
13. netInit() - MQTT module                        [REUSE]
14. initIMU() - QMI8658 sensor                     [EXCLUDE]
15. initBattery() - Battery readings               [EXCLUDE]
16. initWiFi() - Connect to WiFi
17. initMQTT() - Establish MQTT connection
18. reinitializeMotionBaseline()                   [EXCLUDE]
```

### Loop Sequence (from `loop()`)

```
1. lv_timer_handler() - LVGL UI update (2ms tick)
2. updateImuData()                                 [EXCLUDE]
3. updateMotionState()                             [EXCLUDE]
4. updateGMeterDisplay()                           [EXCLUDE]
5. updateCalibration()                             [EXCLUDE]
6. IF WiFi connected:
   - mqttClient.loop() - MQTT keep-alive           [REUSE]
   - netCheckMqtt() - Reconnect if needed          [REUSE]
7. imageFetcherLoop() - Process HTTP response      [REUSE]
8. IF 200ms elapsed:
   - updateBatteryInfo()                           [EXCLUDE]
   - updateMotionStatusUI()                        [EXCLUDE]
   - updateBatteryInfoUI()                         [EXCLUDE]
9. screenMemoryUpdate()                            [ADAPT]
```

---

## 5. MQTT Topics

### Subscribed Topics (Receive)

| Topic | Purpose | Port to Home Panel |
|-------|---------|-------------------|
| `esp32image` | Image URL triggers | YES |
| `ha/hilo_meter_power` | Power (kW) | YES |
| `hilo_energie` | Energy (kWh) | YES |

### Published Topics (Transmit)

| Topic | Purpose | Port to Home Panel |
|-------|---------|-------------------|
| `companion/motion` | Motion state | NO - Exclude |
| `companion/imu` | IMU peak data | NO - Exclude |
| `companion/calibration` | Calibration status | NO - Exclude |

---

## 6. Key Dependencies

### Libraries Required for Home Panel

| Library | Version | Purpose |
|---------|---------|---------|
| LVGL | 8.4.0 | UI framework |
| PubSubClient | 2.8 | MQTT client |
| TJpg_Decoder | 1.1.0 | JPEG decoding |
| HTTPClient | ESP32 core | HTTP requests |
| WiFi | ESP32 core | Network connectivity |
| WiFiClientSecure | ESP32 core | TLS connections |

### Libraries NOT Needed for Home Panel

| Library | Reason |
|---------|--------|
| SensorLib 0.3.1 | QMI8658 IMU driver - no IMU hardware |
| XPowersLib 0.2.6 | AXP2101 PMIC - different power management |

---

## 7. NVS Namespaces

| Namespace | Key | Purpose | Port Status |
|-----------|-----|---------|-------------|
| "imuCal" | gravity, scale, rotation | IMU calibration | EXCLUDE |
| "screenMem" | lastScr | Last active screen | ADAPT |

---

## 8. Critical Observations

### Code Organization

- **Main sketch is monolithic:** `companion.ino` (~28KB) contains all hardware init, UI updates, and coordination logic
- **Modules are well-isolated:** `src/` modules have clean APIs with config structs
- **UI is generated:** All `ui_*.c` files are SquareLine Studio output - do not modify

### Hardware Differences

| Feature | Companion | Home Panel |
|---------|-----------|------------|
| MCU | ESP32-S3 | ESP32-S3 |
| Display | SH8601 AMOLED 368×448 | Different (TBD) |
| Touch | FT3168 | Different (TBD) |
| PMIC | AXP2101 | None (USB-C only) |
| Battery | Yes | No |
| IMU | QMI8658 | None |
| Power | Battery + USB | USB-C only |

### Porting Strategy Implications

1. **Create new main sketch:** Don't try to strip down `companion.ino` - create fresh `home_panel.ino`
2. **Copy modules directly:** `net_module` and `image_fetcher` can be copied with minimal changes
3. **Generate new UI:** Use SquareLine Studio to create Home Panel specific screens
4. **Simplify init:** Remove all battery, IMU, motion, and power management initialization

---

## 9. Porting Checklist Summary

### Files to Copy and Adapt

- [ ] `src/net/net_module.cpp/h` → Update MQTT callbacks
- [ ] `src/image/image_fetcher.cpp/h` → Update screen references
- [ ] `src/screen_memory/screen_memory.cpp/h` → Reduce to 2 screens

### Files to Exclude

- [ ] `src/imu/imu_module.cpp/h`
- [ ] `calibration.cpp/h`
- [ ] `ui_Screen3.c` (G-Meter)
- [ ] `ui_InclinometerScreen.c`
- [ ] `ui_calibrationScreen.c`

### New Files to Create

- [ ] `home_panel.ino` - Simplified main sketch
- [ ] Hardware init functions for Home Panel display/touch
- [ ] MQTT callback handlers for power/energy data

---

## 10. Module Dependency Diagram

```
                    ┌─────────────────┐
                    │  home_panel.ino │
                    │  (main sketch)  │
                    └────────┬────────┘
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
        ▼                    ▼                    ▼
┌───────────────┐   ┌───────────────┐   ┌───────────────┐
│  net_module   │   │ image_fetcher │   │ screen_memory │
│               │   │               │   │               │
│ - WiFi        │   │ - HTTP GET    │   │ - NVS persist │
│ - MQTT client │   │ - JPEG decode │   │ - Screen track│
│ - Reconnect   │   │ - LVGL update │   │               │
└───────┬───────┘   └───────┬───────┘   └───────────────┘
        │                   │
        │                   │
        ▼                   ▼
┌─────────────────────────────────────┐
│           LVGL UI Layer             │
│                                     │
│  ┌──────────┐      ┌──────────┐    │
│  │ Screen1  │ ←──► │ Screen2  │    │
│  │ (Home)   │      │ (Image)  │    │
│  └──────────┘      └──────────┘    │
└─────────────────────────────────────┘
```

---

*Document generated from companion_reference_code/ analysis*
