#pragma once

#include <Arduino.h>

// ============================================================================
// Time Service Module
// ============================================================================
// Provides NTP-based time synchronization and LVGL label updates.
// Based on reference: ntp_reference_code/ntp.ino
//
// Features:
// - NTP sync on boot and every 6 hours
// - 1-second LVGL timer for label updates
// - Updates labelTimeDate with date: "18 Jan 2026"
// - Updates labelTime with time: "21:34:10"
// ============================================================================

// Initialize time service (call after WiFi is connected)
// - Syncs with NTP server
// - Creates 1-second LVGL timer for label updates
// - Creates 6-hour timer for periodic NTP re-sync
void time_service_init();

// Force manual NTP re-sync (optional)
// Call this when WiFi reconnects after being disconnected
void time_service_sync();

// Get formatted date string
// Returns: "18 Jan 2026" or error string if not initialized
String time_service_getFormattedDate();

// Get formatted time string
// Returns: "21:34:10" or error string if not initialized
String time_service_getFormattedTime();

// Check if time has been successfully initialized
bool time_service_isInitialized();

// Pause/resume label updates (for screen transitions)
// Call pause when leaving Screen1, resume when returning
void time_service_pause();
void time_service_resume();
