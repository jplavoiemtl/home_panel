#include "time_service.h"

#include <WiFi.h>
#include <time.h>
#include <lvgl.h>

#include "ui.h"

// ============================================================================
// Constants
// ============================================================================

// Montreal timezone: EST5EDT,M3.2.0,M11.1.0
// EST = Eastern Standard Time (UTC-5)
// EDT = Eastern Daylight Time (UTC-4)
// M3.2.0 = DST starts 2nd Sunday of March
// M11.1.0 = DST ends 1st Sunday of November
static const char* TIMEZONE = "EST5EDT,M3.2.0,M11.1.0";

// NTP server
static const char* NTP_SERVER = "pool.ntp.org";

// Update intervals
static const uint32_t LABEL_UPDATE_MS = 1000;        // 1 second
static const uint32_t NTP_SYNC_INTERVAL_MS = 21600000; // 6 hours in ms

// ============================================================================
// Private State
// ============================================================================

static bool timeInitialized = false;
static unsigned long lastNtpSync = 0;
static lv_timer_t* labelTimer = nullptr;
static lv_timer_t* syncTimer = nullptr;

// Month abbreviations
static const char* MONTHS[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

// ============================================================================
// Private Functions
// ============================================================================

// Set timezone (from reference ntp.ino)
static void setTimezone(const char* timezone) {
    Serial.printf("Time: Setting timezone to %s\n", timezone);
    setenv("TZ", timezone, 1);
    tzset();
}

// Initialize time from NTP (from reference ntp.ino)
static void initTime(const char* timezone) {
    struct tm timeinfo;

    Serial.println("Time: Syncing with NTP server...");

    // First connect to NTP server with 0 TZ offset
    configTime(0, 0, NTP_SERVER);

    // Wait for time to be set (with timeout)
    int retries = 0;
    while (!getLocalTime(&timeinfo) && retries < 10) {
        Serial.println("Time: Waiting for NTP response...");
        delay(500);
        retries++;
    }

    if (retries >= 10) {
        Serial.println("Time: Failed to obtain time from NTP");
        return;
    }

    Serial.println("Time: Got time from NTP");

    // Now set the real timezone
    setTimezone(timezone);

    timeInitialized = true;
    lastNtpSync = millis();

    // Print current time
    if (getLocalTime(&timeinfo)) {
        Serial.printf("Time: Current time: %02d:%02d:%02d\n",
                      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
}

// Format time string: "18 Jan 2026, 14:31:10"
static String formatTimeString(struct tm* timeinfo) {
    char buffer[32];

    // Day without leading zero, month abbreviation, year, time with leading zeros
    snprintf(buffer, sizeof(buffer), "%d %s %04d, %02d:%02d:%02d",
             timeinfo->tm_mday,
             MONTHS[timeinfo->tm_mon],
             timeinfo->tm_year + 1900,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);

    return String(buffer);
}

// LVGL timer callback - updates label once per second
static void labelTimerCallback(lv_timer_t* timer) {
    if (!ui_labelTimeDate) return;

    struct tm timeinfo;

    if (!timeInitialized) {
        lv_label_set_text(ui_labelTimeDate, "Syncing time...");
        return;
    }

    if (WiFi.status() != WL_CONNECTED) {
        // WiFi disconnected - still show time from internal clock
        if (getLocalTime(&timeinfo)) {
            String timeStr = formatTimeString(&timeinfo);
            lv_label_set_text(ui_labelTimeDate, timeStr.c_str());
        }
        return;
    }

    if (!getLocalTime(&timeinfo)) {
        lv_label_set_text(ui_labelTimeDate, "-- --- ----, --:--:--");
        return;
    }

    String timeStr = formatTimeString(&timeinfo);
    lv_label_set_text(ui_labelTimeDate, timeStr.c_str());
}

// LVGL timer callback - periodic NTP re-sync
static void syncTimerCallback(lv_timer_t* timer) {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Time: Periodic NTP re-sync");
        initTime(TIMEZONE);
    }
}

// ============================================================================
// Public API
// ============================================================================

void time_service_init() {
    Serial.println("Time: Initializing time service...");

    // Initial NTP sync
    if (WiFi.status() == WL_CONNECTED) {
        initTime(TIMEZONE);
    } else {
        Serial.println("Time: WiFi not connected, skipping initial sync");
    }

    // Create 1-second LVGL timer for label updates
    labelTimer = lv_timer_create(labelTimerCallback, LABEL_UPDATE_MS, nullptr);
    if (labelTimer) {
        Serial.println("Time: Created 1-second label update timer");
    }

    // Create 6-hour LVGL timer for NTP re-sync
    syncTimer = lv_timer_create(syncTimerCallback, NTP_SYNC_INTERVAL_MS, nullptr);
    if (syncTimer) {
        Serial.println("Time: Created 6-hour NTP sync timer");
    }

    Serial.println("Time: Time service initialized");
}

void time_service_sync() {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Time: Manual NTP sync requested");
        initTime(TIMEZONE);
    } else {
        Serial.println("Time: Cannot sync - WiFi not connected");
    }
}

String time_service_getFormatted() {
    struct tm timeinfo;

    if (!timeInitialized) {
        return "Syncing time...";
    }

    if (!getLocalTime(&timeinfo)) {
        return "-- --- ----, --:--:--";
    }

    return formatTimeString(&timeinfo);
}

bool time_service_isInitialized() {
    return timeInitialized;
}
