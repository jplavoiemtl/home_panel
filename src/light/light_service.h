#pragma once

#include <Arduino.h>
#include <lvgl.h>
#include <PubSubClient.h>

// ============================================================================
// Light Control Service Module
// ============================================================================
// Provides cycling light control for multiple lights via MQTT.
// Light status is received via MQTT, selected light index is persisted in NVS.
//
// Features:
// - Cycle through lights with a select button
// - Toggle the currently selected light via MQTT
// - Visual feedback: yellow (ON), dark grey (OFF), purple (UNKNOWN)
// - NVS persistence of selected light index (with 30-second debounce)
// ============================================================================

// Initialize light service
// Pass pointers to LVGL objects created in SquareLine Studio and the MQTT client:
// - selectBtn: button to cycle through lights (ButtonSelectLight)
// - lightBtn: button to toggle current light (ButtonLight)
// - label: displays the selected light name (lightLabel)
// - mqtt: PubSubClient pointer for publishing toggle commands
void light_service_init(lv_obj_t* selectBtn, lv_obj_t* lightBtn,
                        lv_obj_t* label, PubSubClient* mqtt);

// Handle incoming MQTT message on the light topic
// Matches payload against known status strings (e.g., "cu_on", "sa_of")
void light_service_handleMQTT(const char* payload);

// Cycle to the next light (call from button handler)
void light_service_cycleLight();

// Toggle the currently selected light (publish MQTT command)
void light_service_toggleCurrent();

// Periodic processing - handles NVS debounce save (call in loop())
void light_service_loop();

#ifdef __cplusplus
extern "C" {
#endif

// Button event handlers for LVGL (called from SLS-generated C code)
void buttonSelectLight_event_handler(lv_event_t* e);
void buttonLight_event_handler(lv_event_t* e);

#ifdef __cplusplus
}
#endif
