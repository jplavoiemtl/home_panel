// Screen Power Manager Implementation
// Handles automatic screen dimming after inactivity timeout

#include "screen_power.h"
#include <Arduino.h>
#include "../../esp_bsp.h"

// State variables
static ScreenPowerState currentState = SCREEN_STATE_ON;
static unsigned long lastActivityTime = 0;

//***************************************************************************************************
void screenPowerInit(void) {
    currentState = SCREEN_STATE_ON;
    lastActivityTime = millis();
    bsp_display_backlight_on();
    Serial.println("Screen power manager initialized - screen ON");
}

//***************************************************************************************************
void screenPowerLoop(void) {
    if (currentState == SCREEN_STATE_ON) {
        // Check if timeout expired
        if (millis() - lastActivityTime > SCREEN_ON_TIMEOUT_MS) {
            bsp_display_brightness_set(SCREEN_DIM_BRIGHTNESS);
            currentState = SCREEN_STATE_DIM;
            Serial.printf("Screen dim - timeout expired (%d%% brightness)\n", SCREEN_DIM_BRIGHTNESS);
        }
    }
    // When in DIM state, just wait for activity signal
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
