// Home Panel - ESP32-S3 Smart Display
// Displays power/energy data via MQTT and camera images via HTTP

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager
#include <WebServer.h>
#include <ElegantOTA.h>           // https://github.com/ayushsharma82/ElegantOTA
#include <PubSubClient.h>
#include <lvgl.h>

#include "display.h"
#include "esp_bsp.h"
#include "lv_port.h"
#include "pincfg.h"

// UI includes
#include "ui.h"

// Module includes
#include "src/net/net_module.h"
#include "src/image/image_fetcher.h"
#include "src/screen/screen_power.h"
#include "src/time/time_service.h"

// Secrets (credentials)
#include "secrets_private.h"

// ============================================================================
// Configuration
// ============================================================================

#define LVGL_PORT_ROTATION_DEGREE (90)

// Display dimensions (after rotation)
#define SCREEN_WIDTH  480
#define SCREEN_HEIGHT 320

// MQTT Topics
#define TOPIC_IMAGE "esp32image"
#define TOPIC_POWER "ha/hilo_meter_power"
#define TOPIC_ENERGY "hilo_energie"

// ============================================================================
// Global Objects
// ============================================================================

// Network clients
WiFiClient wifiClient;
WiFiClientSecure secureClient;
PubSubClient mqttClient;

// Web server for OTA updates
WebServer server(80);

// Timing
unsigned long lastWiFiCheck = 0;
constexpr unsigned long WIFI_CHECK_INTERVAL = 10000;  // 10 seconds
unsigned long lastStatusUpdate = 0;
constexpr unsigned long STATUS_UPDATE_INTERVAL = 2000;  // 2 seconds
unsigned long lastHeapLog = 0;
constexpr unsigned long HEAP_LOG_INTERVAL = 300000;  // 5 minutes

// ============================================================================
// Forward Declarations
// ============================================================================

void configModeCallback(WiFiManager *myWiFiManager);
void restartESP();
void initWiFiManager();
void checkWiFi();
void initMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void updateConnectionStatus();
void logHeapStatus();
void showConnectScreen(const char* message);
void showMainScreen();
void initOTA();


// ============================================================================
// Screen Switching Functions
// ============================================================================

// Show connect screen with status message
void showConnectScreen(const char* message) {
    lv_label_set_text(ui_labelConnectStatus, message);
    lv_scr_load(ui_ScreenConnect);
    lv_timer_handler();  // Force render before blocking WiFi operations
}

// Show main screen after successful connection
void showMainScreen() {
    Serial.println("Switching to main screen...");
    lv_scr_load(ui_Screen1);
    bsp_display_backlight_on();
    Serial.println("Main screen active");
}

// ============================================================================
// OTA Functions (ElegantOTA)
// ============================================================================

void onOTAStart() {
    Serial.println("OTA update started - switching to OTA screen");

    // Switch to OTA screen
    if (ui_ScreenOTA) {
        lv_label_set_text(ui_labelOTAStatus, "Updating firmware...");
        lv_label_set_text(ui_labelOTAProgress, "0%");
        lv_scr_load(ui_ScreenOTA);
        lv_refr_now(NULL);  // Force immediate refresh
        Serial.println("OTA screen loaded");
    } else {
        Serial.println("ERROR: ui_ScreenOTA is NULL!");
    }
}

void onOTAProgress(size_t current, size_t final) {
    static size_t lastKB = 0;

    // ElegantOTA 3.x bug: 'current' is cumulative bytes received, 'final' is previous value
    // Show KB received since we don't have total size
    size_t kb = current / 1024;

    // Only update display every 10 KB to reduce overhead
    if (kb >= lastKB + 10) {
        lastKB = kb;
        Serial.printf("OTA: %u KB\n", kb);
        char buf[16];
        snprintf(buf, sizeof(buf), "%u KB", kb);
        lv_label_set_text(ui_labelOTAProgress, buf);
        lv_refr_now(NULL);  // Force immediate refresh
    }
}

void onOTAEnd(bool success) {
    if (success) {
        Serial.println("OTA update finished successfully");
        lv_label_set_text(ui_labelOTAStatus, "Update complete!");
        lv_label_set_text(ui_labelOTAProgress, "Restarting...");
    } else {
        Serial.println("OTA update failed");
        lv_label_set_text(ui_labelOTAStatus, "Update failed!");
        lv_label_set_text(ui_labelOTAProgress, "");
    }
    lv_refr_now(NULL);  // Force immediate refresh
}

// Initialize OTA web server
void initOTA() {
    Serial.println("Initializing OTA...");

    // Root endpoint - device identification
    server.on("/", []() {
        server.send(200, "text/plain", "Home Panel module - OTA available at /update");
    });

    // Initialize ElegantOTA with authentication
    ElegantOTA.begin(&server, OTA_USERNAME, OTA_PASSWORD);
    ElegantOTA.onStart(onOTAStart);
    ElegantOTA.onProgress(onOTAProgress);
    ElegantOTA.onEnd(onOTAEnd);

    server.begin();
    Serial.printf("OTA ready at http://%s/update\n", WiFi.localIP().toString().c_str());
}

// ============================================================================
// WiFi Functions (WiFiManager)
// ============================================================================

// Callback when entering captive portal configuration mode
void configModeCallback(WiFiManager *myWiFiManager) {
    Serial.println("Entered WiFi configuration portal mode");
    Serial.print("Connect to AP: ");
    Serial.println(myWiFiManager->getConfigPortalSSID());

    // Show portal info on connect screen
    char buf[64];
    snprintf(buf, sizeof(buf), "Portal: %s (192.168.4.1)", myWiFiManager->getConfigPortalSSID().c_str());
    showConnectScreen(buf);
    bsp_display_brightness_set(25);  // Dim for portal mode
}

// Restart ESP after WiFi failure - allows self-recovery when network returns
void restartESP() {
    Serial.println("WiFi connection failed, restarting in 10 seconds...");
    delay(10000);
    ESP.restart();
}

// Initialize WiFi using WiFiManager (captive portal for configuration)
void initWiFiManager() {
    Serial.println("Initializing WiFiManager...");

    WiFiManager wm;

    // Configuration portal timeout (3 minutes)
    wm.setConfigPortalTimeout(180);

    // Set callback for when entering config portal mode
    wm.setAPCallback(configModeCallback);

    // reset settings - wipe stored credentials for testing
    // these are stored by the esp library
    // wm.resetSettings();

    // Attempt to connect using stored credentials, or start config portal
    // AP name: "homepanel", password protected
    bool res = wm.autoConnect("homepanel", "password");

    if (!res) {
        // WiFi connection failed after portal timeout
        restartESP();
    } else {
        // Connected successfully
        Serial.println("Connected to WiFi");
        Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
        Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());

        // Switch to main screen
        showMainScreen();
    }

    updateConnectionStatus();
}

// Check WiFi connection - restart-based recovery for reliability
void checkWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected, restarting for recovery...");
        restartESP();
    }
}

// ============================================================================
// MQTT Functions
// ============================================================================

void initMQTT() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Cannot init MQTT - WiFi not connected");
        return;
    }

    Serial.println("Initializing MQTT...");

    // Load MQTT server preference from NVS (defaults to LOCAL)
    netLoadMqttServerFromNVS();

    // Attempt connection with fallback
    if (!netConnectMqttWithFallback()) {
        Serial.println("MQTT: Both servers failed, will retry periodically");
    }

    updateConnectionStatus();
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Null-terminate the payload
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';

    Serial.printf("MQTT [%s]: %s\n", topic, message);

    // Handle power topic
    if (strcmp(topic, TOPIC_POWER) == 0) {
        if (ui_labelPowerValue) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%s kW", message);
            lv_label_set_text(ui_labelPowerValue, buf);
        }
    }
    // Handle energy topic
    else if (strcmp(topic, TOPIC_ENERGY) == 0) {
        if (ui_labelEnergyValue) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%s", message);
            lv_label_set_text(ui_labelEnergyValue, buf);
        }
    }
    // Handle image topic
    else if (strcmp(topic, TOPIC_IMAGE) == 0) {
        Serial.println("Image request received via MQTT");
        requestLatestImage();
    }
}

// ============================================================================
// UI Update Functions
// ============================================================================

void updateConnectionStatus() {
    // Single-threaded mode: no lock needed
    if (ui_labelConnectionStatus) {
        if (WiFi.status() != WL_CONNECTED) {
            lv_label_set_text(ui_labelConnectionStatus, "WiFi: Disconnected");
            lv_obj_set_style_text_color(ui_labelConnectionStatus, lv_color_hex(0xFF0000), LV_PART_MAIN);
        } else if (!netIsMqttConnected()) {
            char buf[64];
            snprintf(buf, sizeof(buf), "IP: %s | No MQTT", WiFi.localIP().toString().c_str());
            lv_label_set_text(ui_labelConnectionStatus, buf);
            lv_obj_set_style_text_color(ui_labelConnectionStatus, lv_color_hex(0xFFFF00), LV_PART_MAIN);
        } else {
            char buf[64];
            snprintf(buf, sizeof(buf), "IP: %s | MQTT %s", WiFi.localIP().toString().c_str(), netGetMqttServerName());
            lv_label_set_text(ui_labelConnectionStatus, buf);
            lv_obj_set_style_text_color(ui_labelConnectionStatus, lv_color_hex(0x00FF00), LV_PART_MAIN);
        }
    }
}

// ============================================================================
// Diagnostics Functions
// ============================================================================

void logHeapStatus() {
    Serial.printf("[HEAP] Free: %d | Min: %d | PSRAM Free: %d\n",
                  ESP.getFreeHeap(),
                  ESP.getMinFreeHeap(),
                  ESP.getFreePsram());
}

// ============================================================================
// Setup
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n\n=== Home Panel Starting ===");
    Serial.printf("Chip: %s\n", ESP.getChipModel());
    Serial.printf("Heap: %d free of %d\n", ESP.getFreeHeap(), ESP.getHeapSize());
    Serial.printf("PSRAM: %d free of %d\n", ESP.getFreePsram(), ESP.getPsramSize());

    // Initialize display
    Serial.println("Initializing display...");
    uint32_t buffer_size = EXAMPLE_LCD_QSPI_H_RES * EXAMPLE_LCD_QSPI_V_RES;
    if (psramFound()) {
        Serial.println("PSRAM found, using full buffer");
    } else {
        buffer_size = buffer_size / 10;
        Serial.println("No PSRAM, using reduced buffer");
    }

    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = buffer_size,
#if LVGL_PORT_ROTATION_DEGREE == 90
        .rotate = LV_DISP_ROT_90,
#elif LVGL_PORT_ROTATION_DEGREE == 270
        .rotate = LV_DISP_ROT_270,
#elif LVGL_PORT_ROTATION_DEGREE == 180
        .rotate = LV_DISP_ROT_180,
#else
        .rotate = LV_DISP_ROT_NONE,
#endif
    };

    bsp_display_start_with_config(&cfg);
    screenPowerInit();  // Initialize screen power manager (turns backlight on)
    Serial.println("Display initialized");

    // Initialize LVGL UI (single-threaded mode, no lock needed)
    Serial.println("Initializing UI...");
    ui_init();
    Serial.println("UI initialized");

    // Show connect screen before WiFi attempt
    showConnectScreen("Connecting...");

    // Initialize network module
    Serial.println("Initializing network module...");
    NetConfig netCfg = {
        .server1 = SERVER1,
        .serverPort1 = SERVERPORT1,
        .server2 = SERVER2,
        .serverPort2 = SERVERPORT2,
        .caCert = ca_cert,
        .mqttClient = &mqttClient,
        .wifiClient = &wifiClient,
        .secureClient = &secureClient,
        .mqttCallback = mqttCallback,
        .topics = {
            .image = TOPIC_IMAGE,
            .power = TOPIC_POWER,
            .energy = TOPIC_ENERGY
        }
    };
    netInit(netCfg);
    Serial.println("Network module initialized");

    // Initialize image fetcher
    Serial.println("Initializing image fetcher...");
    ImageFetcherConfig imgCfg = {
        .screenWidth = SCREEN_WIDTH,
        .screenHeight = SCREEN_HEIGHT,
        .screen1 = ui_Screen1,
        .screen2 = ui_Screen2,
        .imgScreen2Background = ui_imgScreen2Background
    };
    imageFetcherInit(imgCfg);
    Serial.println("Image fetcher initialized");

    // Connect to WiFi using WiFiManager (captive portal for configuration)
    initWiFiManager();

    // Initialize OTA web server (after WiFi is connected)
    initOTA();

    // Initialize MQTT
    initMQTT();

    // Initialize time service (NTP sync + label updates)
    time_service_init();

    Serial.println("=== Setup Complete ===\n");
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
    // Process LVGL - single-threaded mode, called directly from main loop
    lv_timer_handler();

    // Check WiFi connection periodically
    if (millis() - lastWiFiCheck > WIFI_CHECK_INTERVAL) {
        lastWiFiCheck = millis();
        checkWiFi();
    }

    // Process MQTT if connected
    if (WiFi.status() == WL_CONNECTED) {
        mqttClient.loop();
        netCheckMqtt();
    }

    // Process OTA web server
    server.handleClient();
    ElegantOTA.loop();

    // Process image fetcher
    imageFetcherLoop();

    // Periodic heap logging for memory leak detection
    if (millis() - lastHeapLog > HEAP_LOG_INTERVAL) {
        lastHeapLog = millis();
        logHeapStatus();
    }

    // Update connection status periodically
    if (millis() - lastStatusUpdate > STATUS_UPDATE_INTERVAL) {
        lastStatusUpdate = millis();
        updateConnectionStatus();
    }

    // Screen power management (auto-dim after inactivity)
    screenPowerLoop();

    // Small delay to prevent watchdog issues and yield to other tasks
    delay(2);
}