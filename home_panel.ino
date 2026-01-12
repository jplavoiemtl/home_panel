// Home Panel - ESP32-S3 Smart Display
// Displays power/energy data via MQTT and camera images via HTTP

#include <Arduino.h>
#include <WiFi.h>
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

// Timing
unsigned long lastWiFiCheck = 0;
constexpr unsigned long WIFI_CHECK_INTERVAL = 10000;  // 10 seconds
unsigned long lastStatusUpdate = 0;
constexpr unsigned long STATUS_UPDATE_INTERVAL = 2000;  // 2 seconds

// WiFi state
int currentWiFiNetwork = 1;  // 1 = primary (ssid1), 2 = secondary (ssid2)

// ============================================================================
// Forward Declarations
// ============================================================================

void initWiFi();
void checkWiFi();
void initMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void updateConnectionStatus();


// ============================================================================
// WiFi Functions
// ============================================================================

void initWiFi() {
    Serial.println("Connecting to WiFi...");

    // Update UI
    bsp_display_lock(0);
    if (ui_labelConnectionStatus) {
        lv_label_set_text(ui_labelConnectionStatus, "Connecting WiFi...");
    }
    bsp_display_unlock();

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid1, password1);
    currentWiFiNetwork = 1;

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nConnected to %s\n", ssid1);
        Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\nPrimary WiFi failed, trying secondary...");
        WiFi.begin(ssid2, password2);
        currentWiFiNetwork = 2;

        attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\nConnected to %s\n", ssid2);
            Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        } else {
            Serial.println("\nWiFi connection failed!");
        }
    }

    updateConnectionStatus();
}

void checkWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected, reconnecting...");
        initWiFi();
        // Reconfigure and reconnect MQTT after WiFi reconnection
        if (WiFi.status() == WL_CONNECTED) {
            initMQTT();
        }
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

    // Configure MQTT client based on current network
    netConfigureMqttClient(currentWiFiNetwork);

    // Attempt initial connection
    netCheckMqtt(true);  // Bypass rate limit for initial connection

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
        bsp_display_lock(0);
        if (ui_labelPowerValue) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%s kW", message);
            lv_label_set_text(ui_labelPowerValue, buf);
        }
        bsp_display_unlock();
    }
    // Handle energy topic
    else if (strcmp(topic, TOPIC_ENERGY) == 0) {
        bsp_display_lock(0);
        if (ui_labelEnergyValue) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%s", message);
            lv_label_set_text(ui_labelEnergyValue, buf);
        }
        bsp_display_unlock();
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
    bsp_display_lock(0);

    if (ui_labelConnectionStatus) {
        if (WiFi.status() != WL_CONNECTED) {
            lv_label_set_text(ui_labelConnectionStatus, "WiFi: Disconnected");
            lv_obj_set_style_text_color(ui_labelConnectionStatus, lv_color_hex(0xFF0000), LV_PART_MAIN);
        } else if (!netIsMqttConnected()) {
            char buf[64];
            snprintf(buf, sizeof(buf), "IP: %s | MQTT: ...", WiFi.localIP().toString().c_str());
            lv_label_set_text(ui_labelConnectionStatus, buf);
            lv_obj_set_style_text_color(ui_labelConnectionStatus, lv_color_hex(0xFFFF00), LV_PART_MAIN);
        } else {
            char buf[64];
            snprintf(buf, sizeof(buf), "IP: %s | MQTT: OK", WiFi.localIP().toString().c_str());
            lv_label_set_text(ui_labelConnectionStatus, buf);
            lv_obj_set_style_text_color(ui_labelConnectionStatus, lv_color_hex(0x00FF00), LV_PART_MAIN);
        }
    }

    // Show/hide activity spinner based on MQTT connection
    if (ui_ActivitySpinner) {
        if (netIsMqttConnected()) {
            lv_obj_add_flag(ui_ActivitySpinner, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(ui_ActivitySpinner, LV_OBJ_FLAG_HIDDEN);
        }
    }

    bsp_display_unlock();
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
    bsp_display_backlight_on();
    Serial.println("Display initialized");

    // Initialize LVGL UI
    Serial.println("Initializing UI...");
    bsp_display_lock(0);
    ui_init();
    bsp_display_unlock();
    Serial.println("UI initialized");

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

    // Connect to WiFi
    initWiFi();

    // Initialize MQTT
    initMQTT();

    Serial.println("=== Setup Complete ===\n");
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
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

    // Process image fetcher
    imageFetcherLoop();

    // Update connection status periodically
    if (millis() - lastStatusUpdate > STATUS_UPDATE_INTERVAL) {
        lastStatusUpdate = millis();
        updateConnectionStatus();
    }

    // Small delay to prevent watchdog issues
    delay(5);
}