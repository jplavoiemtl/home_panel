// Screen Power Manager
// Handles automatic screen dimming after inactivity timeout
// Wakes screen on touch events or MQTT image updates

#ifndef SCREEN_POWER_H
#define SCREEN_POWER_H

#ifdef __cplusplus
extern "C" {
#endif

// Configuration constants
#define SCREEN_ON_TIMEOUT_MS      60000   // 1 minute before dimming
#define SCREEN_DIM_BRIGHTNESS     10      // 10% brightness when dimmed

// Screen power states
typedef enum {
    SCREEN_STATE_ON,
    SCREEN_STATE_DIM
} ScreenPowerState;

// Initialize screen power manager (call from setup)
void screenPowerInit(void);

// Process screen power state machine (call from loop)
void screenPowerLoop(void);

// Signal activity - wakes screen and resets timer
// Call this from activity_event_handler() and MQTT image handler
void screenPowerActivity(void);

// Get current state (for debugging/status)
ScreenPowerState screenPowerGetState(void);

#ifdef __cplusplus
}
#endif

#endif // SCREEN_POWER_H
