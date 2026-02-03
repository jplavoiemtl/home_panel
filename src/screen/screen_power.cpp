// Screen Power Manager Implementation
// Handles automatic screen dimming after inactivity timeout
// Day/night aware: uses different dim brightness based on time of day

#include "screen_power.h"
#include <Arduino.h>
#include "../../esp_bsp.h"

// State variables
static ScreenPowerState currentState = SCREEN_STATE_ON;
static unsigned long lastActivityTime = 0;
static int lastAppliedBrightness = -1;

//***************************************************************************************************
static int getDimBrightness() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 0)) {
        return SCREEN_DIM_DAY_BRIGHTNESS;  // NTP not synced, default to day
    }
    int now = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    bool isNight = (now >= NIGHT_START_MINUTES || now < DAY_START_MINUTES);
    return isNight ? SCREEN_DIM_NIGHT_BRIGHTNESS : SCREEN_DIM_DAY_BRIGHTNESS;
}

//***************************************************************************************************
void screenPowerInit(void) {
    currentState = SCREEN_STATE_DIM;
    lastActivityTime = millis();
    lastAppliedBrightness = SCREEN_DIM_DAY_BRIGHTNESS;
    bsp_display_brightness_set(SCREEN_DIM_DAY_BRIGHTNESS);
    Serial.printf("Screen power manager initialized - dim (%d%% brightness)\n", SCREEN_DIM_DAY_BRIGHTNESS);
}

//***************************************************************************************************
void screenPowerLoop(void) {
    if (currentState == SCREEN_STATE_ON) {
        if (millis() - lastActivityTime > SCREEN_ON_TIMEOUT_MS) {
            int brightness = getDimBrightness();
            bsp_display_brightness_set(brightness);
            lastAppliedBrightness = brightness;
            currentState = SCREEN_STATE_DIM;
            Serial.printf("Screen dim (%d%% brightness)\n", brightness);
        }
    } else if (currentState == SCREEN_STATE_DIM) {
        // Check for day/night boundary crossing
        int brightness = getDimBrightness();
        if (brightness != lastAppliedBrightness) {
            bsp_display_brightness_set(brightness);
            lastAppliedBrightness = brightness;
            Serial.printf("Screen dim adjusted (%d%% brightness)\n", brightness);
        }
    }
}

//***************************************************************************************************
void screenPowerActivity(void) {
    // Always reset the activity timer
    lastActivityTime = millis();

    // If screen is dimmed, wake it up
    if (currentState == SCREEN_STATE_DIM) {
        bsp_display_backlight_on();
        currentState = SCREEN_STATE_ON;
        Serial.println("Screen wake - activity detected");
    }
}

//***************************************************************************************************
ScreenPowerState screenPowerGetState(void) {
    return currentState;
}
