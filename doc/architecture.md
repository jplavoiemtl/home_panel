# Home Panel Architecture

> **Purpose:** This document describes the architecture of the Home Panel firmware, including current state and target architecture after porting from the Companion project.

---

## 1. System Overview

### 1.1 Hardware Platform

| Component | Specification |
|-----------|---------------|
| **MCU** | ESP32-S3 |
| **Display** | AXS15231B QSPI LCD, 320×480 pixels |
| **Touch** | I2C capacitive touch controller |
| **Memory** | PSRAM (8MB typical) |
| **Storage** | SD card (SD_MMC interface) |
| **Power** | USB-C (always powered, no battery) |
| **Audio** | I2S audio output (optional) |

### 1.2 Key Hardware Differences from Companion

| Feature | Companion | Home Panel |
|---------|-----------|------------|
| Display | SH8601 AMOLED 368×448 | AXS15231B QSPI 320×480 |
| Touch | FT3168 | Different I2C controller |
| PMIC | AXP2101 | None |
| Battery | Yes | No |
| IMU | QMI8658 | None |
| Power | Battery + USB | USB-C only |

---

## 2. Current State (Hardware Validation)

The Home Panel firmware is currently a **minimal hardware validation project** that demonstrates:

- Display initialization and rendering
- Touch input handling
- SD card access
- JPEG decoding and display
- LVGL integration

### 2.1 Current Folder Structure

```
home_panel/
├── home_panel.ino          # Main sketch (SD card image viewer demo)
├── pincfg.h                # Pin definitions
├── esp_bsp.h               # Board support package API
├── display.h               # Display control API
├── lv_port.h               # LVGL port configuration
├── lv_conf.h               # LVGL settings
├── esp_lcd_axs15231b.h     # Display driver
├── esp_lcd_touch.h         # Touch driver
├── bsp_err_check.h         # Error handling macros
├── MjpegClass.h            # MJPEG support (optional)
├── secrets_private.h       # Credentials (gitignored)
├── secrets_private.cpp     # Credential implementations (gitignored)
├── secrets_private.example.h  # Template for credentials
├── doc/                    # Documentation
└── companion_reference_code/  # Reference only (gitignored)
```

### 2.2 Current Initialization Flow

```
setup()
├── Serial.begin()
├── SD_MMC.begin()              # Mount SD card
├── bsp_display_start_with_config()  # Initialize display + LVGL
├── Allocate PSRAM buffers      # For image decoding
├── Create LVGL image object
├── listDir()                   # Scan SD card for images
└── show_new_pic()              # Display first image

loop()
├── Check timer (8 second cycle)
├── show_new_pic()              # Cycle to next image
└── delay(1)
```

---

## 3. Target Architecture (After Porting)

### 3.1 Target Folder Structure

```
home_panel/
├── home_panel.ino              # Main sketch (simplified)
├── pincfg.h                    # Pin definitions
├── esp_bsp.h                   # Board support package
├── display.h                   # Display control
├── lv_port.h                   # LVGL port
├── lv_conf.h                   # LVGL settings
├── esp_lcd_axs15231b.h         # Display driver
├── esp_lcd_touch.h             # Touch driver
├── bsp_err_check.h             # Error handling
├── secrets_private.h           # Credentials (gitignored)
├── secrets_private.cpp         # Credential implementations
├── secrets_private.example.h   # Credential template
├── src/
│   ├── net/
│   │   ├── net_module.h        # Network/MQTT API
│   │   └── net_module.cpp      # Network/MQTT implementation
│   ├── image/
│   │   ├── image_fetcher.h     # HTTP image fetcher API
│   │   └── image_fetcher.cpp   # HTTP image fetcher implementation
│   ├── temperature/
│   │   ├── temperature_service.h   # Cycling temperature display API
│   │   └── temperature_service.cpp # Cycling temperature display implementation
│   ├── light/
│   │   ├── light_service.h     # Light control API
│   │   └── light_service.cpp   # Light control implementation
│   ├── time/
│   │   ├── time_service.h      # NTP time display API
│   │   └── time_service.cpp    # NTP time display implementation
│   ├── screen/
│   │   ├── screen_power.h      # Screen power management API
│   │   └── screen_power.cpp    # Day/night auto-dim implementation
│   └── ui_custom.h             # Custom UI extensions
├── ui/                         # SquareLine Studio generated (gitignored)
│   ├── ui.h
│   ├── ui.c
│   ├── ui_Screen1.c            # Home screen
│   ├── ui_Screen2.c            # Image display screen
│   └── ui_*.c                  # Other UI files
└── doc/
    ├── architecture.md         # This document
    ├── companion_analysis.md   # Companion analysis
    └── home_panel_plan.md      # Project plan
```

### 3.2 Target Module Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      home_panel.ino                         │
│                      (Main Sketch)                          │
│  - Hardware initialization                                  │
│  - Module coordination                                      │
│  - Main loop                                                │
└──────────────────────────┬──────────────────────────────────┘
                           │
        ┌──────────────────┼──────────────────┐
        │                  │                  │
        ▼                  ▼                  ▼
┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐
│ net_module   │ │image_fetcher │ │ temp_service │ │light_service │
│              │ │              │ │              │ │              │
│- WiFi mgmt  │ │- HTTP GET    │ │- Cycling loc │ │- Cycling sel │
│- MQTT client│ │- JPEG decode │ │- MQTT status │ │- MQTT toggle │
│- Reconnect  │ │- Display     │ │- NVS persist │ │- NVS persist │
└──────┬───────┘ └──────┬───────┘ └──────────────┘ └──────────────┘
        │                  │
        │                  │
        ▼                  ▼
┌─────────────────────────────────────────────────────────────┐
│                     LVGL UI Layer                           │
│  ┌────────────────────┐    ┌────────────────────┐          │
│  │     Screen 1       │    │     Screen 2       │          │
│  │   (Home Screen)    │◄──►│  (Image Display)   │          │
│  │                    │    │                    │          │
│  │ - Power display    │    │ - Camera image     │          │
│  │ - Energy display   │    │ - Status text      │          │
│  │ - Camera buttons   │    │ - Back button      │          │
│  │ - Status indicator │    │                    │          │
│  └────────────────────┘    └────────────────────┘          │
└─────────────────────────────────────────────────────────────┘
        │
        ▼
┌─────────────────────────────────────────────────────────────┐
│                   Hardware Abstraction                      │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐   │
│  │ Display  │  │  Touch   │  │  WiFi    │  │  PSRAM   │   │
│  │ BSP API  │  │  Driver  │  │  Stack   │  │  Alloc   │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘   │
└─────────────────────────────────────────────────────────────┘
```

---

## 4. Module Responsibilities

### 4.1 Main Sketch (`home_panel.ino`)

**Responsibilities:**

- Hardware initialization (display, touch, I2C)
- LVGL initialization and configuration
- Module initialization (net, image_fetcher)
- WiFi connection management
- MQTT connection management
- Main loop coordination
- UI event handling setup

**Key Functions:**

- `setup()` - Initialize all hardware and modules
- `loop()` - Call module update functions, handle LVGL
- `initWiFi()` - Connect to WiFi network
- `initMQTT()` - Establish MQTT connection
- `mqttCallback()` - Handle incoming MQTT messages

### 4.2 Network Module (`src/net/`)

**Responsibilities:**

- MQTT client management
- Dual-server failover (primary/secondary)
- Connection monitoring and reconnection
- Topic subscription management
- Secure/non-secure connection handling

**API:**

```cpp
void netInit(const NetConfig& config);
void netConfigureMqttClient(PubSubClient* client);
bool netCheckMqtt();
bool netIsMqttConnected();
bool netHasInitialMqttSuccess();
```

**Configuration:**

```cpp
struct NetConfig {
    const char* primaryServer;
    const char* secondaryServer;
    uint16_t primaryPort;
    uint16_t secondaryPort;
    const char* mqttUser;
    const char* mqttPassword;
    const char* clientId;
    // TLS certificates (optional)
};
```

### 4.3 Temperature Service Module (`src/temperature/`)

**Responsibilities:**

- Cycle through temperature locations (select button)
- Receive temperature data via MQTT from multiple sources
- Request temperature status on boot and after MQTT reconnection
- Display location name, temperature value, and sample time
- Color coding based on temperature thresholds (blue/orange/red)
- Persist selected location index to NVS (30-second debounce)

**API:**

```cpp
void temperature_service_init(lv_obj_t* locLabel, lv_obj_t* tempLabel,
                              lv_obj_t* timeLabel, PubSubClient* mqtt);
void temperature_service_handleMQTT(const char* payload);
void temperature_service_cycleLocation();
void temperature_service_requestStatus();
void temperature_service_loop();
```

**MQTT:**

- Topic: `weather` (receives individual JSON payloads per source)
- Publishes `"status"` request at boot and on MQTT reconnect
- Parses keys: `OutsideTemp`, `AmbientTemp`, `BurjpTemp`, `MaitreTemp`, `MyriamTemp`

### 4.4 Image Fetcher Module (`src/image/`)

**Responsibilities:**

- HTTP/HTTPS image retrieval (optimized with `yield()` instead of `delay(1)`)
- JPEG decoding to RGB565
- LVGL image widget update
- Request queueing and timeout handling
- Memory management for image buffers
- Streamlined serial logging with download duration timing

**API:**

```cpp
void imageFetcherInit(const ImageFetcherConfig& config);
void imageFetcherLoop();
void requestLatestImage();
void onButtonLatestPressed(lv_event_t* e);
void onButtonNewPressed(lv_event_t* e);
```

**Configuration:**

```cpp
struct ImageFetcherConfig {
    uint16_t screenWidth;
    uint16_t screenHeight;
    lv_obj_t* imageScreen;
    lv_obj_t* imageWidget;
    lv_obj_t* statusLabel;
    const char* imageUrl;
};
```

### 4.5 Light Service Module (`src/light/`)

**Responsibilities:**

- Cycle through available lights (select button)
- Toggle the currently selected light via MQTT publish
- Receive light status updates via MQTT (ON/OFF)
- Request light status on boot and after MQTT reconnection
- Display light name, button color, and ON/OFF images
- Persist selected light index to NVS (30-second debounce)

**API:**

```cpp
void light_service_init(lv_obj_t* selectBtn, lv_obj_t* lightBtn,
                        lv_obj_t* label, lv_obj_t* imgOn, lv_obj_t* imgOff,
                        PubSubClient* mqtt);
void light_service_handleMQTT(const char* payload);
void light_service_cycleLight();
void light_service_toggleCurrent();
void light_service_requestStatus();
void light_service_loop();
```

**Data Model:**

```cpp
struct LightMeta {
    const char* description;       // Display name ("Cuisine")
    const char* togglePayload;     // Published to toggle ("cuisine")
    const char* statusOnPayload;   // Received when ON ("cu_on")
    const char* statusOffPayload;  // Received when OFF ("cu_of")
};
```

**MQTT:**

- Topic: `m18toggle` (shared for commands and status)
- Publishes toggle payload on button press
- Publishes `"status"` request at boot and on MQTT reconnect
- Receives status payloads and updates `LightState` (UNKNOWN, ON, OFF)

**UI Feedback:**

| State   | Button color | ON image | OFF image |
|---------|-------------|----------|-----------|
| ON      | Yellow      | Visible  | Hidden    |
| OFF     | Dark grey   | Hidden   | Visible   |
| UNKNOWN | Purple      | Hidden   | Hidden    |

### 4.6 Screen Power Module (`src/screen/`)

**Responsibilities:**

- Automatic screen dimming after 2 minutes of inactivity
- Day/night aware brightness: 10% during day (06:30–23:30), 3% at night
- Boot in DIM state at day brightness to avoid full-brightness burst
- Automatic brightness adjustment on day/night boundary crossing while dimmed
- Wake to full brightness on touch or MQTT image activity

**API:**

```cpp
void screenPowerInit(void);       // Boot into DIM state
void screenPowerLoop(void);       // State machine: ON timeout + DIM boundary check
void screenPowerActivity(void);   // Wake screen and reset inactivity timer
ScreenPowerState screenPowerGetState(void);
```

**Time detection:** Uses `getLocalTime()` from `<Arduino.h>` to get `tm_hour` and
`tm_min`, converted to minutes-since-midnight. Falls back to day brightness if NTP
is not yet synced.

### 4.7 Screen Memory Module (`src/screen_memory/`) - Optional

**Responsibilities:**

- Track currently active screen
- Persist screen ID to NVS
- Restore last screen on boot
- Debounce writes to prevent NVS wear

**API:**

```cpp
void screenMemoryInit(const ScreenMemoryConfig& config);
void screenMemoryUpdate();
void screenMemoryOnScreenLoaded(lv_obj_t* screen);
```

---

## 5. Data Flow

### 5.1 Power/Energy Display Flow

```
MQTT Broker
    │
    │ publish: "ha/hilo_meter_power", "hilo_energie"
    ▼
net_module (subscription)
    │
    │ mqttCallback(topic, payload)
    ▼
home_panel.ino (callback handler)
    │
    │ parse payload, extract values
    ▼
LVGL Labels
    │
    │ lv_label_set_text(ui_labelPowerValue, ...)
    │ lv_label_set_text(ui_labelEnergyValue, ...)
    ▼
Display Update
```

### 5.2 Image Request Flow

```
User Touch (Camera Button)
    │
    │ LVGL event callback
    ▼
image_fetcher.requestLatestImage()
    │
    │ HTTP GET request
    ▼
HTTP Server (camera)
    │
    │ JPEG response
    ▼
image_fetcher (decode)
    │
    │ TJpg_Decoder → PSRAM buffer
    ▼
LVGL Image Widget
    │
    │ lv_img_set_src(ui_imgScreen2Background, ...)
    ▼
Screen 2 Display
```

### 5.3 Light Control Flow

```
User Touch (Select Button)          User Touch (Toggle Button)
    │                                   │
    │ buttonSelectLight_event_handler   │ buttonLight_event_handler
    ▼                                   ▼
light_service_cycleLight()          light_service_toggleCurrent()
    │                                   │
    │ Update label + button color       │ mqtt->publish("m18toggle", payload)
    │ Start NVS debounce timer          ▼
    ▼                               MQTT Broker
Display Update                          │
                                        │ Node-RED processes toggle
                                        ▼
                                    MQTT Broker
                                        │
                                        │ publish: "m18toggle" (status payload)
                                        ▼
                                    light_service_handleMQTT()
                                        │
                                        │ Update lightStates[], button color,
                                        │ show/hide ON/OFF images
                                        ▼
                                    Display Update

Boot / MQTT Reconnect
    │
    │ light_service_requestStatus()
    │ mqtt->publish("m18toggle", "status")
    ▼
MQTT Broker
    │
    │ Node-RED responds with each light's current state
    │ (e.g., cu_on, sa_of, st_on, ...)
    ▼
light_service_handleMQTT()  (called once per status payload)
    │
    │ Update lightStates[], refresh UI for selected light
    ▼
Display Update
```

### 5.4 MQTT-Triggered Image Flow

```
MQTT Broker
    │
    │ publish: "esp32image" (URL payload)
    ▼
net_module (subscription)
    │
    │ mqttCallback(topic, payload)
    ▼
home_panel.ino
    │
    │ image_fetcher.requestImage(url)
    ▼
[Same as Image Request Flow above]
```

---

## 6. LVGL Screen Hierarchy

### 6.1 Screen 1 (Home Screen)

```
ui_Screen1
├── Power Display Section
│   ├── ui_labelPowerTitle ("Power")
│   └── ui_labelPowerValue ("0.0 kW")
├── Energy Display Section
│   ├── ui_labelEnergyTitle ("Energy")
│   └── ui_labelEnergyValue ("0.0 kWh")
├── Camera Buttons Section
│   ├── ui_btnLatest (Request latest image)
│   ├── ui_btnNew (Request new capture)
│   └── ui_btnBack (Return from image view)
├── Light Control Section
│   ├── ui_ButtonSelectLight (Cycle through lights)
│   ├── ui_selectButtonLabel (Select button text)
│   ├── ui_ButtonLight (Toggle selected light)
│   ├── ui_lightLabel (Selected light name)
│   ├── ui_lightONImage (Light bulb ON image)
│   └── ui_lightOFFImage (Light bulb OFF image)
├── Status Section
│   ├── ui_labelConnectionStatus (WiFi/MQTT status)
│   └── ui_ActivitySpinner (Loading indicator)
└── Navigation
    └── Button to Screen 2
```

### 6.2 Screen 2 (Image Display)

```
ui_Screen2
├── ui_imgScreen2Background (Full-screen image)
├── ui_screen2Text (Status/error text)
└── ui_Button2 (Back to Screen 1)
```

---

## 7. Build and Flash Process

### 7.1 Prerequisites

- Arduino IDE 2.x or PlatformIO
- ESP32 Arduino Core 3.1.x (avoid 3.2.0+ for I2C compatibility)
- Required libraries:
  - LVGL 8.4.0
  - PubSubClient 2.8
  - TJpg_Decoder 1.1.0
  - ESP32_JPEG_Library

### 7.2 Configuration

1. Copy `secrets_private.example.h` to `secrets_private.h`
2. Edit `secrets_private.h` with:
   - WiFi SSID and password
   - MQTT server address and credentials
   - Image server URL

### 7.3 Build Commands

**Arduino CLI:**

```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3 home_panel
arduino-cli upload --fqbn esp32:esp32:esp32s3 -p COM_PORT home_panel
```

**PlatformIO:**

```bash
pio run
pio run --target upload
```

### 7.4 Serial Monitor

```bash
# Arduino CLI
arduino-cli monitor -p COM_PORT -c baudrate=115200

# PlatformIO
pio device monitor --baud 115200
```

---

## 8. Memory Layout

### 8.1 RAM Usage

| Region | Purpose | Typical Size |
|--------|---------|--------------|
| DRAM | Variables, stack | ~320 KB |
| IRAM | Code cache | ~128 KB |
| PSRAM | Image buffers, LVGL draw buffers | 8 MB |

### 8.2 PSRAM Allocations

| Buffer | Size | Purpose |
|--------|------|---------|
| LVGL draw buffer | ~153 KB | `320 × 480 / 10 × 2` bytes |
| Image decode buffer | ~307 KB | `480 × 320 × 2` bytes |
| HTTP read buffer | ~307 KB | JPEG file temporary storage |

---

## 9. Configuration Files

### 9.1 `secrets_private.h` (Credentials - Gitignored)

```cpp
// Build mode
#define HOME    // or CAR for companion

// MQTT configuration
#define MQTT_CLIENT_ID "HOMEPANEL"
#define MQTT_TOPIC_POWER "ha/hilo_meter_power"
#define MQTT_TOPIC_ENERGY "hilo_energie"
#define MQTT_TOPIC_IMAGE "esp32image"

// WiFi
extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;

// MQTT
extern const char* MQTT_SERVER;
extern const char* MQTT_USER;
extern const char* MQTT_PASSWORD;

// Image server
extern const char* IMAGE_SERVER_URL;
```

### 9.3 `lv_conf.h` (LVGL Configuration)

Key settings:

- Color depth: 16-bit (RGB565)
- Memory: Use PSRAM for large allocations
- Fonts: Built-in fonts enabled
- Widgets: Label, Image, Button, Spinner enabled

---

## 10. Error Handling

### 10.1 Network Errors

- **WiFi disconnect:** Automatic reconnection with exponential backoff
- **MQTT disconnect:** Reconnect every 15 seconds via `netCheckMqtt()`
- **HTTP timeout:** 15-second timeout, status displayed on UI

### 10.2 Memory Errors

- **PSRAM allocation failure:** Fall back to regular heap
- **Image too large:** Skip with error log (60KB JPEG limit)
- **Heap exhaustion:** Log warning, skip operation

### 10.3 Hardware Errors

- **Display init failure:** Halt with error on serial
- **SD card failure:** Continue without SD features
- **Touch init failure:** Continue without touch (degraded mode)

---

*Document version: 1.0*
*Generated for Home Panel project*
