#include "net_module.h"
#include "secrets_private.h"

namespace {

NetConfig cfg{};

// MQTT reconnect state
bool mqttSuccess = false;
unsigned long lastMqttAttempt = 0;
constexpr unsigned long MQTT_RECONNECT_INTERVAL = 15000;  // 15s between reconnection attempts

// Current MQTT server (1=LOCAL, 2=REMOTE)
int currentMqttServer = MQTT_SERVER_LOCAL;

// NVS constants
constexpr const char* NVS_NAMESPACE = "homepanel";
constexpr const char* NVS_KEY_MQTT_SERVER = "mqtt_server";

// Preferences object for NVS
Preferences preferences;

// Helper to decide if port is secure
bool isSecurePort(uint16_t port) {
  return port == 9735 || port == 8883;
}

// Helper to attempt MQTT connection with current server config
bool tryMqttConnect() {
  if (!cfg.mqttClient) return false;

  cfg.mqttClient->disconnect();
  delay(100);

  if (cfg.mqttClient->connect(CLIENT_ID, USERNAME, KEY)) {
    // Subscribe to topics
    if (cfg.topics.image) {
      cfg.mqttClient->subscribe(cfg.topics.image, 1);
    }
    if (cfg.topics.power) {
      cfg.mqttClient->subscribe(cfg.topics.power, 1);
    }
    if (cfg.topics.energy) {
      cfg.mqttClient->subscribe(cfg.topics.energy, 1);
    }
    return true;
  }
  return false;
}

}  // namespace

void netInit(const NetConfig& c) {
  cfg = c;
  if (cfg.mqttClient) {
    // mqttClient is owned by the sketch; we just configure it here.
    cfg.mqttClient->setBufferSize(512);
    if (cfg.mqttCallback) {
      cfg.mqttClient->setCallback(cfg.mqttCallback);
    }
  }
}

void netConfigureMqttClient(int connection) {
  if (!cfg.mqttClient || !cfg.wifiClient || !cfg.secureClient) return;

  if (connection == 1) {
    if (isSecurePort(cfg.serverPort1)) {
      cfg.secureClient->setCACert(cfg.caCert);
      cfg.mqttClient->setClient(*cfg.secureClient);
    } else {
      cfg.mqttClient->setClient(*cfg.wifiClient);
    }
    cfg.mqttClient->setServer(cfg.server1, cfg.serverPort1);
  } else {
    if (isSecurePort(cfg.serverPort2)) {
      cfg.secureClient->setCACert(cfg.caCert);
      cfg.mqttClient->setClient(*cfg.secureClient);
    } else {
      cfg.mqttClient->setClient(*cfg.wifiClient);
    }
    cfg.mqttClient->setServer(cfg.server2, cfg.serverPort2);
  }
}

void netCheckMqtt(bool bypassRateLimit) {
  if (!cfg.mqttClient) return;

  if (!cfg.mqttClient->connected()) {
    unsigned long currentTime = millis();
    if (!bypassRateLimit && currentTime - lastMqttAttempt < MQTT_RECONNECT_INTERVAL) {
      return;
    }
    lastMqttAttempt = currentTime;

    Serial.printf("MQTT: Reconnecting to %s server...\n", netGetMqttServerName());

    cfg.mqttClient->disconnect();  // clean stale state
    delay(100);

    if (cfg.mqttClient->connect(CLIENT_ID, USERNAME, KEY)) {
      mqttSuccess = true;
      Serial.println("MQTT: Reconnected successfully");
      // Subscriptions
      if (cfg.topics.image) {
        cfg.mqttClient->subscribe(cfg.topics.image, 1);
      }
      if (cfg.topics.power) {
        cfg.mqttClient->subscribe(cfg.topics.power, 1);
      }
      if (cfg.topics.energy) {
        cfg.mqttClient->subscribe(cfg.topics.energy, 1);
      }
    }
  }
}

bool netIsMqttConnected() {
  return cfg.mqttClient && cfg.mqttClient->connected();
}

bool netHasInitialMqttSuccess() {
  return mqttSuccess;
}

int netGetCurrentMqttServer() {
  return currentMqttServer;
}

const char* netGetMqttServerName() {
  return (currentMqttServer == MQTT_SERVER_LOCAL) ? "Local" : "Remote";
}

void netLoadMqttServerFromNVS() {
  preferences.begin(NVS_NAMESPACE, true);  // read-only
  currentMqttServer = preferences.getInt(NVS_KEY_MQTT_SERVER, MQTT_SERVER_LOCAL);
  preferences.end();

  // Validate value
  if (currentMqttServer != MQTT_SERVER_LOCAL && currentMqttServer != MQTT_SERVER_REMOTE) {
    currentMqttServer = MQTT_SERVER_LOCAL;
  }

  Serial.printf("NVS: Loaded MQTT server preference: %s\n", netGetMqttServerName());
}

void netSaveMqttServerToNVS() {
  preferences.begin(NVS_NAMESPACE, false);  // read-write
  preferences.putInt(NVS_KEY_MQTT_SERVER, currentMqttServer);
  preferences.end();

  Serial.printf("NVS: Saved MQTT server preference: %s\n", netGetMqttServerName());
}

bool netConnectMqttWithFallback() {
  if (!cfg.mqttClient) return false;

  // Try the stored/current server first
  Serial.printf("MQTT: Trying %s server...\n", netGetMqttServerName());
  netConfigureMqttClient(currentMqttServer);

  if (tryMqttConnect()) {
    mqttSuccess = true;
    Serial.printf("MQTT: Connected to %s server\n", netGetMqttServerName());
    return true;
  }

  Serial.printf("MQTT: %s server failed, trying fallback...\n", netGetMqttServerName());

  // Try the fallback server
  int fallbackServer = (currentMqttServer == MQTT_SERVER_LOCAL)
                        ? MQTT_SERVER_REMOTE
                        : MQTT_SERVER_LOCAL;

  currentMqttServer = fallbackServer;
  netConfigureMqttClient(currentMqttServer);

  if (tryMqttConnect()) {
    mqttSuccess = true;
    // Save new preference to NVS since fallback succeeded
    netSaveMqttServerToNVS();
    Serial.printf("MQTT: Connected to %s server (fallback)\n", netGetMqttServerName());
    return true;
  }

  Serial.println("MQTT: Both servers failed");
  return false;
}
