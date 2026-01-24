#include "temperature_service.h"

#include <Preferences.h>
#include "../time/time_service.h"

// ============================================================================
// Location Configuration - Single Source of Truth
// ============================================================================
// To add/remove locations, only modify this array.
// Everything else adapts automatically.

struct LocationMeta {
    const char* label;      // Display name
    const char* mqttKey;    // JSON key in MQTT payload
};

const LocationMeta locationMeta[] = {
    { "Outside", "OutsideTemp" },
    { "Ambient", "AmbientTemp" },
    { "Kitchen", "KitchenTemp" }
};

constexpr size_t TEMP_LOC_COUNT = sizeof(locationMeta) / sizeof(locationMeta[0]);

// ============================================================================
// Temperature Sample Storage
// ============================================================================

struct TempSample {
    float temperatureC;
    char timeHHMM[6];   // "hh:mm" + null terminator
    bool valid;         // false until first MQTT update
};

// ============================================================================
// Module State
// ============================================================================

namespace {

// NVS configuration
constexpr const char* NVS_NAMESPACE = "homepanel";
constexpr const char* NVS_KEY_TEMP_LOC = "temp_loc";
constexpr unsigned long NVS_DEBOUNCE_MS = 30000;  // 30 seconds

// Temperature samples for each location
TempSample tempSamples[TEMP_LOC_COUNT] = {};

// Current selected location
size_t currentLocation = 0;

// LVGL label pointers
lv_obj_t* labelLoc = nullptr;
lv_obj_t* labelTemp = nullptr;
lv_obj_t* labelTime = nullptr;

// NVS debounce state
unsigned long lastLocationChangeTime = 0;
bool nvsSavePending = false;
size_t pendingLocation = 0;

Preferences preferences;

// ============================================================================
// Internal Functions
// ============================================================================

void loadLocationFromNVS() {
    preferences.begin(NVS_NAMESPACE, true);  // read-only
    int savedLoc = preferences.getInt(NVS_KEY_TEMP_LOC, 0);
    preferences.end();

    // Validate saved location is within bounds
    if (savedLoc >= 0 && savedLoc < static_cast<int>(TEMP_LOC_COUNT)) {
        currentLocation = static_cast<size_t>(savedLoc);
    } else {
        currentLocation = 0;
    }
    Serial.printf("Temperature service: loaded location %d (%s) from NVS\n",
                  currentLocation, locationMeta[currentLocation].label);
}

void saveLocationToNVS(size_t loc) {
    preferences.begin(NVS_NAMESPACE, false);  // read-write
    preferences.putInt(NVS_KEY_TEMP_LOC, static_cast<int>(loc));
    preferences.end();
    Serial.printf("Temperature service: saved location %d (%s) to NVS\n",
                  loc, locationMeta[loc].label);
}

lv_color_t getTemperatureColor(float temp) {
    if (temp < 0) {
        return lv_palette_main(LV_PALETTE_BLUE);
    } else if (temp > 25) {
        return lv_palette_main(LV_PALETTE_RED);
    } else {
        return lv_palette_main(LV_PALETTE_GREEN);
    }
}

void updateUI() {
    const TempSample& sample = tempSamples[currentLocation];
    const LocationMeta& meta = locationMeta[currentLocation];

    // Update location label (if available)
    if (labelLoc) {
        lv_label_set_text(labelLoc, meta.label);
    }

    // Update temperature label (if available)
    if (labelTemp) {
        if (sample.valid) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%.1f C", sample.temperatureC);
            lv_label_set_text(labelTemp, buf);
            lv_obj_set_style_text_color(labelTemp, getTemperatureColor(sample.temperatureC), 0);
        } else {
            lv_label_set_text(labelTemp, "--");
            lv_obj_set_style_text_color(labelTemp, lv_color_white(), 0);
        }
    }

    // Update time label (if available)
    if (labelTime) {
        if (sample.valid) {
            lv_label_set_text(labelTime, sample.timeHHMM);
        } else {
            lv_label_set_text(labelTime, "--:--");
        }
    }
}

}  // namespace

// ============================================================================
// Public API
// ============================================================================

void temperature_service_init(lv_obj_t* locLabel, lv_obj_t* tempLabel, lv_obj_t* timeLabel) {
    // Store label pointers
    labelLoc = locLabel;
    labelTemp = tempLabel;
    labelTime = timeLabel;

    // Initialize all samples as invalid
    for (size_t i = 0; i < TEMP_LOC_COUNT; i++) {
        tempSamples[i].temperatureC = 0.0f;
        tempSamples[i].timeHHMM[0] = '\0';
        tempSamples[i].valid = false;
    }

    // Load saved location from NVS
    loadLocationFromNVS();

    // Display initial state
    updateUI();

    Serial.println("Temperature service initialized");
}

void temperature_service_handleMQTT(const char* payload) {
    for (size_t i = 0; i < TEMP_LOC_COUNT; i++) {
        // Build the key to search for (e.g., "\"OutsideTemp\":")
        char key[32];
        snprintf(key, sizeof(key), "\"%s\":", locationMeta[i].mqttKey);

        const char* pos = strstr(payload, key);
        if (pos) {
            pos += strlen(key);

            // Skip whitespace
            while (*pos == ' ' || *pos == '\t') pos++;

            // Parse temperature value
            float temp = strtof(pos, nullptr);

            // Update sample
            tempSamples[i].temperatureC = temp;
            tempSamples[i].valid = true;

            // Get current time (hh:mm only, first 5 characters)
            String timeStr = time_service_getFormattedTime();
            if (timeStr.length() >= 5) {
                strncpy(tempSamples[i].timeHHMM, timeStr.c_str(), 5);
                tempSamples[i].timeHHMM[5] = '\0';
            } else {
                strncpy(tempSamples[i].timeHHMM, timeStr.c_str(), sizeof(tempSamples[i].timeHHMM) - 1);
                tempSamples[i].timeHHMM[sizeof(tempSamples[i].timeHHMM) - 1] = '\0';
            }

            Serial.printf("Temperature update: %s = %.1f C at %s\n",
                          locationMeta[i].label, temp, tempSamples[i].timeHHMM);

            // Update UI if this is the currently selected location
            if (i == currentLocation) {
                updateUI();
            }

            return;  // Found and processed, exit
        }
    }
}

void temperature_service_cycleLocation() {
    // Advance to next location with wrap-around
    currentLocation = (currentLocation + 1) % TEMP_LOC_COUNT;

    Serial.printf("Temperature location cycled to: %s\n", locationMeta[currentLocation].label);

    // Update UI immediately
    updateUI();

    // Start/reset NVS debounce timer
    lastLocationChangeTime = millis();
    nvsSavePending = true;
    pendingLocation = currentLocation;
}

void temperature_service_loop() {
    // Handle NVS debounce save
    if (nvsSavePending && (millis() - lastLocationChangeTime >= NVS_DEBOUNCE_MS)) {
        saveLocationToNVS(pendingLocation);
        nvsSavePending = false;
    }
}

void buttonTempLocation_event_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        temperature_service_cycleLocation();
    }
}
