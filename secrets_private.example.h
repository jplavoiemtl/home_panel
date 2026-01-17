#pragma once
// Copy this file to secrets_private.h and fill in your real values.
// Do NOT commit the populated secrets_private.h/.cpp files.

// ====================================================================
// MQTT SERVER CONFIGURATION
// ====================================================================
// SERVER1 = LOCAL (non-TLS, typically port 1883)
// SERVER2 = REMOTE (TLS, typically port 8883)
// ====================================================================

#define SERVERPORT1 1883   // Local MQTT (non-secure)
#define SERVERPORT2 8883   // Remote MQTT (TLS secure) - change to your broker's port

// MQTT Servers (defined in secrets_private.cpp)
extern const char* SERVER1;  // Local server IP
extern const char* SERVER2;  // Remote server hostname

// Image servers (defined in secrets_private.cpp)
extern const char* IMAGE_SERVER_BASE;    // Local image server
extern const char* IMAGE_SERVER_REMOTE;  // Remote image server (HTTPS)

// --- COMMON CONFIGURATION ---

extern const char* API_TOKEN;

// MQTT Credentials
#define USERNAME    "your_mqtt_username"
#define KEY         "your_mqtt_password"
#define CLIENT_ID   "HOMEPANEL"

// OTA Credentials
#define OTA_USERNAME "your_ota_username"
#define OTA_PASSWORD "your_ota_password"

// --- CERTIFICATES ---

// Certificate for secure MQTT connection (port 9735)
extern const char* ca_cert;

// Certificate for the HTTPS Remote Image Server (IMAGE_SERVER_REMOTE)
extern const char* remote_server_ca_cert;

// ------------------------------------------------------------------------
// Populate the corresponding secrets_private.cpp with real values.
// ------------------------------------------------------------------------
