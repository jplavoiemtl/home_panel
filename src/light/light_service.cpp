#include "light_service.h"

#include <Preferences.h>

// ============================================================================
// MQTT Payload Constants
// ============================================================================

// Toggle command payloads (published to toggle a light)
static const char PAYLOAD_CUI[] = "cuisine";
static const char PAYLOAD_SAL[] = "salon";
static const char PAYLOAD_STA[] = "statue";
static const char PAYLOAD_GAL[] = "galerie";
static const char PAYLOAD_PIS[] = "piscine";
static const char PAYLOAD_BJP[] = "bureaujp";  // 4 lights all following this pattern in Node-RED
static const char PAYLOAD_STATUS[] = "status";
static const char PAYLOAD_CJP[] = "chambrejp";

// Status payloads (received from Node-RED)
static const char CUISINE_ON[]  = "cu_on";
static const char CUISINE_OFF[] = "cu_of";
static const char SALON_ON[]    = "sa_on";
static const char SALON_OFF[]   = "sa_of";
static const char STATUE_ON[]   = "st_on";
static const char STATUE_OFF[]  = "st_of";
static const char GALERIE_ON[]  = "ga_on";
static const char GALERIE_OFF[] = "ga_of";
static const char PISCINE_ON[]  = "pi_on";
static const char PISCINE_OFF[] = "pi_of";
static const char BURJP_ON[]  = "bj_on";
static const char BURJP_OFF[] = "bj_of";
static const char CHAMBREJP_ON[]  = "cj_on";
static const char CHAMBREJP_OFF[] = "cj_of";

// MQTT topic for light commands and status
static const char TOPIC_LIGHT[] = "m18toggle";

// ============================================================================
// Light Configuration - Single Source of Truth
// ============================================================================
// To add/remove lights, only modify this array and add the payload constants.
// Everything else adapts automatically.

enum class LightState : uint8_t {
    UNKNOWN,
    ON,
    OFF
};

struct LightMeta {
    const char* description;       // Display name
    const char* togglePayload;     // Sent to toggle
    const char* statusOnPayload;   // Received when ON
    const char* statusOffPayload;  // Received when OFF
};

static const LightMeta lightMeta[] = {
    { "Cuisine",  PAYLOAD_CUI, CUISINE_ON,  CUISINE_OFF  },
    { "Salon",    PAYLOAD_SAL, SALON_ON,    SALON_OFF    },
    { "Statue",   PAYLOAD_STA, STATUE_ON,   STATUE_OFF   },
    { "Galerie",  PAYLOAD_GAL, GALERIE_ON,  GALERIE_OFF  },
    { "Piscine",  PAYLOAD_PIS, PISCINE_ON,  PISCINE_OFF  },
    { "Bureau JP",PAYLOAD_BJP, BURJP_ON,    BURJP_OFF    },   
    { "Chambre JP",PAYLOAD_CJP, CHAMBREJP_ON, CHAMBREJP_OFF  }, 
};

static constexpr size_t LIGHT_COUNT = sizeof(lightMeta) / sizeof(lightMeta[0]);

// ============================================================================
// Module State
// ============================================================================

namespace {

// NVS configuration
constexpr const char* NVS_NAMESPACE = "homepanel";
constexpr const char* NVS_KEY_LIGHT_IDX = "light_idx";
constexpr unsigned long NVS_DEBOUNCE_MS = 30000;  // 30 seconds

// Light states (derived from MQTT, not persisted)
LightState lightStates[LIGHT_COUNT] = {};  // All UNKNOWN (0)

// Current selected light index
size_t currentLight = 0;

// LVGL object pointers
lv_obj_t* btnSelect = nullptr;
lv_obj_t* btnLight = nullptr;
lv_obj_t* labelLight = nullptr;
lv_obj_t* imgOn = nullptr;
lv_obj_t* imgOff = nullptr;

// MQTT client pointer (for publishing)
PubSubClient* mqtt = nullptr;

// NVS debounce state
unsigned long lastSelectionChangeTime = 0;
bool nvsSavePending = false;
size_t pendingLight = 0;
size_t lastSavedLight = 0;

Preferences preferences;

// ============================================================================
// Internal Functions
// ============================================================================

void loadLightFromNVS() {
    preferences.begin(NVS_NAMESPACE, true);  // read-only
    int savedIdx = preferences.getInt(NVS_KEY_LIGHT_IDX, 0);
    preferences.end();

    if (savedIdx >= 0 && savedIdx < static_cast<int>(LIGHT_COUNT)) {
        currentLight = static_cast<size_t>(savedIdx);
    } else {
        currentLight = 0;
    }
    lastSavedLight = currentLight;
    Serial.printf("Light service: loaded index %d (%s) from NVS\n",
                  currentLight, lightMeta[currentLight].description);
}

void saveLightToNVS(size_t idx) {
    preferences.begin(NVS_NAMESPACE, false);  // read-write
    preferences.putInt(NVS_KEY_LIGHT_IDX, static_cast<int>(idx));
    preferences.end();
    Serial.printf("Light service: saved index %d (%s) to NVS\n",
                  idx, lightMeta[idx].description);
}

lv_color_t getLightColor(LightState state) {
    switch (state) {
        case LightState::ON:      return lv_color_hex(0xc6b033);  // Yellow
        case LightState::OFF:     return lv_color_hex(0x2D2D2D);  // Dark Grey
        case LightState::UNKNOWN:
        default:                  return lv_color_hex(0x800080);  // Purple
    }
}

void updateUI() {
    const LightMeta& meta = lightMeta[currentLight];
    LightState state = lightStates[currentLight];

    // Update light name label
    if (labelLight) {
        lv_label_set_text(labelLight, meta.description);
    }

    // Update button background color based on light state
    if (btnLight) {
        lv_obj_set_style_bg_color(btnLight, getLightColor(state), LV_PART_MAIN);
    }

    // Update ON/OFF images based on light state
    switch (state) {
        case LightState::ON:
            if (imgOn) lv_obj_clear_flag(imgOn, LV_OBJ_FLAG_HIDDEN);
            if (imgOff) lv_obj_add_flag(imgOff, LV_OBJ_FLAG_HIDDEN);
            break;
        case LightState::OFF:
            if (imgOn) lv_obj_add_flag(imgOn, LV_OBJ_FLAG_HIDDEN);
            if (imgOff) lv_obj_clear_flag(imgOff, LV_OBJ_FLAG_HIDDEN);
            break;
        case LightState::UNKNOWN:
        default:
            if (imgOn) lv_obj_add_flag(imgOn, LV_OBJ_FLAG_HIDDEN);
            if (imgOff) lv_obj_add_flag(imgOff, LV_OBJ_FLAG_HIDDEN);
            break;
    }
}

}  // namespace

// ============================================================================
// Public API
// ============================================================================

void light_service_init(lv_obj_t* selectBtn, lv_obj_t* lightBtn,
                        lv_obj_t* label, lv_obj_t* imageOn, lv_obj_t* imageOff,
                        PubSubClient* mqttClient) {
    btnSelect = selectBtn;
    btnLight = lightBtn;
    labelLight = label;
    imgOn = imageOn;
    imgOff = imageOff;
    mqtt = mqttClient;

    // Initialize all light states to UNKNOWN
    for (size_t i = 0; i < LIGHT_COUNT; i++) {
        lightStates[i] = LightState::UNKNOWN;
    }

    // Load saved light index from NVS
    loadLightFromNVS();

    // Display initial state
    updateUI();

    // Request current light status from Node-RED
    light_service_requestStatus();

    Serial.println("Light service initialized");
}

void light_service_requestStatus() {
    if (mqtt && mqtt->connected()) {
        mqtt->publish(TOPIC_LIGHT, PAYLOAD_STATUS);
        Serial.println("Light service: requested status from Node-RED");
    }
}

void light_service_handleMQTT(const char* payload) {
    for (size_t i = 0; i < LIGHT_COUNT; i++) {
        if (strcmp(payload, lightMeta[i].statusOnPayload) == 0) {
            lightStates[i] = LightState::ON;
            Serial.printf("Light status: %s = ON\n", lightMeta[i].description);
            if (i == currentLight) {
                updateUI();
            }
            return;
        }
        if (strcmp(payload, lightMeta[i].statusOffPayload) == 0) {
            lightStates[i] = LightState::OFF;
            Serial.printf("Light status: %s = OFF\n", lightMeta[i].description);
            if (i == currentLight) {
                updateUI();
            }
            return;
        }
    }
}

void light_service_cycleLight() {
    currentLight = (currentLight + 1) % LIGHT_COUNT;

    Serial.printf("Light selection cycled to: %s\n", lightMeta[currentLight].description);

    // Update UI immediately
    updateUI();

    // Start/reset NVS debounce timer
    lastSelectionChangeTime = millis();
    nvsSavePending = true;
    pendingLight = currentLight;
}

void light_service_toggleCurrent() {
    if (!mqtt) {
        Serial.println("Light service: MQTT client not available");
        return;
    }

    const char* payload = lightMeta[currentLight].togglePayload;
    bool success = mqtt->publish(TOPIC_LIGHT, payload);

    Serial.printf("Light toggle: %s (%s) - %s\n",
                  lightMeta[currentLight].description, payload,
                  success ? "sent" : "failed");
}

void light_service_loop() {
    // Handle NVS debounce save
    if (nvsSavePending && (millis() - lastSelectionChangeTime >= NVS_DEBOUNCE_MS)) {
        if (pendingLight != lastSavedLight) {
            saveLightToNVS(pendingLight);
            lastSavedLight = pendingLight;
        }
        nvsSavePending = false;
    }
}

void buttonSelectLight_event_handler(lv_event_t* e) {
    static unsigned long lastClickTime = 0;
    constexpr unsigned long CLICK_DEBOUNCE_MS = 500;

    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        unsigned long now = millis();
        if (now - lastClickTime >= CLICK_DEBOUNCE_MS) {
            lastClickTime = now;
            light_service_cycleLight();
        }
    }
}

void buttonLight_event_handler(lv_event_t* e) {
    static unsigned long lastClickTime = 0;
    constexpr unsigned long CLICK_DEBOUNCE_MS = 500;

    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        unsigned long now = millis();
        if (now - lastClickTime >= CLICK_DEBOUNCE_MS) {
            lastClickTime = now;
            light_service_toggleCurrent();
        }
    }
}
