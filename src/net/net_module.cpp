#include "net_module.h"
#include "secrets_private.h"

namespace {

NetConfig cfg{};

// MQTT reconnect state
bool mqttSuccess = false;
unsigned long lastMqttAttempt = 0;
constexpr unsigned long MQTT_RECONNECT_INTERVAL = 15000;  // 15s between reconnection attempts

// MQTT Watchdog - detect stale connections
unsigned long lastMqttMessage = 0;
constexpr unsigned long MQTT_WATCHDOG_TIMEOUT = 300000;  // 5 minutes without message = stale

// Helper to decide if port is secure
bool isSecurePort(uint16_t port) {
  return port == 9735 || port == 8883;
}

// Internal helper to force MQTT reconnect
void forceReconnect() {
  if (!cfg.mqttClient) return;

  Serial.println("=== MQTT WATCHDOG TRIGGERED ===");
  unsigned long timeSinceLastMessage = millis() - lastMqttMessage;
  Serial.printf("No MQTT message for %lu seconds\n", timeSinceLastMessage / 1000);
  Serial.println("Connection appears stale, forcing reconnect...");

  cfg.mqttClient->disconnect();
  delay(100);

  // Reset watchdog timer to avoid repeated triggers
  lastMqttMessage = millis();
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

    cfg.mqttClient->disconnect();  // clean stale state
    delay(100);

    if (cfg.mqttClient->connect(CLIENT_ID, USERNAME, KEY)) {
      mqttSuccess = true;
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

void netMqttMessageReceived() {
  // Reset watchdog timer - we received a message, connection is alive
  lastMqttMessage = millis();
}

void netCheckWatchdog() {
  // Skip if we haven't received any message yet (still initializing)
  if (lastMqttMessage == 0) return;

  // Skip if not supposed to be connected
  if (!cfg.mqttClient || !cfg.mqttClient->connected()) return;

  unsigned long timeSinceLastMessage = millis() - lastMqttMessage;

  if (timeSinceLastMessage > MQTT_WATCHDOG_TIMEOUT) {
    forceReconnect();
  }
}
