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
static const uint32_t NTP_RETRY_INTERVAL_MS = 30000;  // 30 seconds retry on failure

// ============================================================================
// Private State
// ============================================================================

static bool timeInitialized = false;
static unsigned long lastNtpSync = 0;
static lv_timer_t* labelTimer = nullptr;
static lv_timer_t* syncTimer = nullptr;
static lv_timer_t* retryTimer = nullptr;

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

// Forward declaration for retry callback
static void retryTimerCallback(lv_timer_t* timer);

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

        // Schedule a retry if not already scheduled
        if (!retryTimer && WiFi.status() == WL_CONNECTED) {
            Serial.println("Time: Scheduling retry in 30 seconds...");
            retryTimer = lv_timer_create(retryTimerCallback, NTP_RETRY_INTERVAL_MS, nullptr);
        }
        return;
    }

    Serial.println("Time: Got time from NTP");

    // Cancel retry timer if it exists (sync succeeded)
    if (retryTimer) {
        lv_timer_del(retryTimer);
        retryTimer = nullptr;
        Serial.println("Time: Cancelled retry timer (sync successful)");
    }

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

// LVGL timer callback - retry NTP sync after failure
static void retryTimerCallback(lv_timer_t* timer) {
    if (timeInitialized) {
        // Already synced, cancel retry timer
        if (retryTimer) {
            lv_timer_del(retryTimer);
            retryTimer = nullptr;
        }
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Time: Retrying NTP sync...");
        initTime(TIMEZONE);
    }
}

// Format date string: "18 Jan 2026"
static String formatDateString(struct tm* timeinfo) {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d %s %04d",
             timeinfo->tm_mday,
             MONTHS[timeinfo->tm_mon],
             timeinfo->tm_year + 1900);
    return String(buffer);
}

// Format time string: "21:34:10"
static String formatTimeOnlyString(struct tm* timeinfo) {
    char buffer[12];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d",
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    return String(buffer);
}

// LVGL timer callback - updates labels once per second
static void labelTimerCallback(lv_timer_t* timer) {
    struct tm timeinfo;

    if (!timeInitialized) {
        if (ui_labelTimeDate) lv_label_set_text(ui_labelTimeDate, "Syncing...");
        if (ui_labelTime) lv_label_set_text(ui_labelTime, "--:--:--");
        return;
    }

    if (!getLocalTime(&timeinfo)) {
        if (ui_labelTimeDate) lv_label_set_text(ui_labelTimeDate, "-- --- ----");
        if (ui_labelTime) lv_label_set_text(ui_labelTime, "--:--:--");
        return;
    }

    String dateStr = formatDateString(&timeinfo);
    String timeStr = formatTimeOnlyString(&timeinfo);

    if (ui_labelTimeDate) lv_label_set_text(ui_labelTimeDate, dateStr.c_str());
    if (ui_labelTime) lv_label_set_text(ui_labelTime, timeStr.c_str());
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

String time_service_getFormattedDate() {
    struct tm timeinfo;

    if (!timeInitialized) {
        return "Syncing...";
    }

    if (!getLocalTime(&timeinfo)) {
        return "-- --- ----";
    }

    return formatDateString(&timeinfo);
}

String time_service_getFormattedTime() {
    struct tm timeinfo;

    if (!timeInitialized) {
        return "--:--:--";
    }

    if (!getLocalTime(&timeinfo)) {
        return "--:--:--";
    }

    return formatTimeOnlyString(&timeinfo);
}

bool time_service_isInitialized() {
    return timeInitialized;
}

void time_service_pause() {
    if (labelTimer) {
        lv_timer_pause(labelTimer);
    }
}

void time_service_resume() {
    if (labelTimer) {
        lv_timer_resume(labelTimer);
    }
}
