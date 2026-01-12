#include "net_module.h"
#include "secrets_private.h"

namespace {

NetConfig cfg{};

// MQTT reconnect state
bool mqttSuccess = false;
unsigned long lastMqttAttempt = 0;
constexpr unsigned long MQTT_RECONNECT_INTERVAL = 15000;  // 15s between reconnection attempts

// Helper to decide if port is secure
bool isSecurePort(uint16_t port) {
  return port == 9735 || port == 8883;
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
