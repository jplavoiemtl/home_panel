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
// - Format: "18 Jan 2026, 14:31:10"
// ============================================================================

// Initialize time service (call after WiFi is connected)
// - Syncs with NTP server
// - Creates 1-second LVGL timer for label updates
// - Creates 6-hour timer for periodic NTP re-sync
void time_service_init();

// Force manual NTP re-sync (optional)
// Call this when WiFi reconnects after being disconnected
void time_service_sync();

// Get formatted time string
// Returns: "18 Jan 2026, 14:31:10" or error string if not initialized
String time_service_getFormatted();

// Check if time has been successfully initialized
bool time_service_isInitialized();
