#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <PubSubClient.h>

// ============================================================================
// Temperature Service Module
// ============================================================================
// Provides cycling temperature display for multiple locations.
// Temperature data is received via MQTT, current location is persisted in NVS.
//
// Features:
// - Display temperatures from multiple locations in a single UI area
// - Cycle through locations with button press
// - NVS persistence of selected location (with 30-second debounce)
// - Color coding based on temperature thresholds
// ============================================================================

// Initialize temperature service
// Pass pointers to the LVGL labels created in SquareLine Studio:
// - locLabel: displays location name (e.g., "Outside")
// - tempLabel: displays temperature value (e.g., "-15.8 C")
// - timeLabel: displays sample time (e.g., "14:57")
// - mqtt: PubSubClient pointer for publishing status requests
void temperature_service_init(lv_obj_t* locLabel, lv_obj_t* tempLabel,
                              lv_obj_t* timeLabel, PubSubClient* mqtt);

// Handle incoming MQTT weather message
// Parses JSON payload for known location keys (e.g., "OutsideTemp": -15.8)
void temperature_service_handleMQTT(const char* payload);

// Cycle to the next location (call from button handler)
void temperature_service_cycleLocation();

// Request current temperature status from Node-RED via MQTT
// Called at init and after MQTT reconnection
void temperature_service_requestStatus();

// Periodic processing - handles NVS debounce save (call in loop())
void temperature_service_loop();

#ifdef __cplusplus
extern "C" {
#endif

// Button event handler for LVGL (called from SLS-generated C code)
void buttonTempLocation_event_handler(lv_event_t* e);

#ifdef __cplusplus
}
#endif