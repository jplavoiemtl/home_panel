/*
power_display
(c) Jean-Pierre Lavoie, novembre 2024
Module pour afficher la puissance électrique consommée par la maison.  Données d'Hydro-Québec par HILO
transmises à partir de Node-Red sur MQTT.

Alterner la puissance longuement affichée et en affichant l'énergie consommée à date pour la journée à 
chaque 30 secondes.
S'il n'y a pas de valeur dans les 5 dernières minutes éteindre l'affichage.
Afficher no wifi ou no MQTT en cas de déconnection du module.

UTILISATION ET FONCTIONS
Indicateur de connection wifi et mqtt.  RGB LED verte: connection mqtt active, orange: pas de mqtt mais 
connexion wfi.  Rouge: pas de connexion wifi.  Au power-up on se connecte sur le wifi configuré.
Si pas de connexion wifi on entre dans un mode de captive portal pour configurer un nouveau wifi et après ce délai
on redémarre et si le wifi réapparait, on reconnecte sur le wifi.  

************************************************
HARDWARE
ESP32S3-Mini-Waveshare
Board dans Arduino IDE: ESP32S3 Dev Module
ESP32 by Espressif Systems 3.0.7
PSRAM: QSPI (2MB)
Flash size: 4MB
Partion Scheme: No FS 2 MB x 2
Flash mode: QIO 80MHz
USB CDC on Boot: enabled

************************************************
Wifi Portal AP
WiFiManager: AP IP address: 192.168.4.1

************************************************
PROGRAMMATION OTA AVEC LA LIBRAIRIE ELEGANTOTA
https://github.com/ayushsharma82/ElegantOTA
Dans Arduino IDE faire sketch export compiled binary.
Noter que faire dans un browser 192.168.30.217 répond une page principale du module qui l'identifie et 
pour voir s'il est en ligne.
Mise à jour du firmware à 192.168.30.216/update
On demande d'entrer un username et password comme sécurité
Choisir le fichier ino.bin dans le répertoire build du projet et l'update se fera tout seul.  
En prime, pas besoin de peser aucun bouton.

*/

#include "secrets.h"
#include <ElegantOTA.h>
#include <Timer.h>
#include <PubSubClient.h>
#include <WebServer.h>
typedef WebServer WiFiWebServer;
#include <WiFiManager.h>            // https://github.com/tzapu/WiFiManager
#include <MD_Parola.h>

#define RGB_BRIGHTNESS 1 // Change brightness (max 255), 2
#ifdef RGB_BUILTIN
#undef RGB_BUILTIN
#endif
#define RGB_BUILTIN 21  

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CLK_PIN   1  // SCK
#define DATA_PIN  2  // MOSI
#define CS_PIN    3  // CS

MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// We always wait a bit between updates of the display
#define DELAYTIME 100             // Scrolling delay in milliseconds  100
#define DELAYTIME_ENERGY 40000    // Delay to show energy: 40 sec.
#define DELAYTIME_NODATA 300000   // Delay to blank display with no data update: 5 minutes

bool scrolling = false;

enum ScrollingState {     // Scrolling text instances
  INSTANCE_1,             // Puissance
  INSTANCE_2,             // Énergie
  INSTANCE_3,             // No Wifi
  INSTANCE_4,             // No MQTT  
  NO_TEXT,                // Blank screen when no updates
};

ScrollingState currentState = INSTANCE_1; // Start with the first instance    
const char* message = "";
String power = "";
String energy = "";
unsigned long lastUpdateEnergy = 0;     // To alternate energy display
unsigned long lastUpdateData = 0;       // To blank the display with no data

const char HILO_POWER[] = "ha/hilo_meter_power";       //MQTT topic de la puissance sur Hilo
const char HILO_ENERGY[] = "hilo_energie";      //MQTT topic de l'énergie de la journée en kWh 

// Create a client class to connect to the MQTT server
WiFiClient espClient;
PubSubClient client(espClient);

Timer t_mqtt;                                    //Timer MQTT retry connection
const int period_mqtt = 15000;                   //retry delay MQTT

WebServer server(80);
unsigned long ota_progress_millis = 0;


//******************************************************************************************************************
void onOTAStart() {
  // Log when OTA has started
  Serial.println();
  Serial.println("OTA update started!");
}


//******************************************************************************************************************
void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}


//******************************************************************************************************************
void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
  Serial.println();
}


//**************************************************************************************************
  void configModeCallback (WiFiManager *myWiFiManager) {
    Serial.println("Entered configuration portal mode");
    Serial.print("Connect to AP: ");
    Serial.println(myWiFiManager->getConfigPortalSSID());
    P.print("Portal");
  }


//******************************************************************************************************************
//MQTT connection every 15s interval if not connected
//Wifi and MQTT connection status indicator
void reconnectMQTT() {
  yield(); 
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      Serial.print("Attempting MQTT connection...");
      
      if (client.connect(CLIENT_ID, USERNAME, KEY)) {
        Serial.println(" connected");                                    
        Serial.print("Client state connected, rc=");                                                     
        Serial.println(client.state());  
        client.subscribe(HILO_POWER, 1);  
        client.subscribe(HILO_ENERGY, 1);                
        neopixelWrite(RGB_BUILTIN, RGB_BRIGHTNESS, 0, 0);  // MQTT connected, green LED
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 15 seconds");
        neopixelWrite(RGB_BUILTIN, 0, 0, RGB_BRIGHTNESS);  // no MQTT, blue LED
      } 
    }
  } else {
    Serial.println("No Wifi");
    neopixelWrite(RGB_BUILTIN,0,RGB_BRIGHTNESS,0);      // No wifi, red LED
    restartESP();         // If blocked in Deco wifi system needs to restart to reconnect after an unblock
  }  
}


//******************************************************************************************************************
// Handle MQTT
void checkMQTT() {
  if (!client.connected()) {   
    t_mqtt.update();
  }
  client.loop();                              //handle MQTT client
}


//******************************************************************************************************************
//Fonction appelée par le broker MQTT chaque fois qu'un message est publié sur un topic auquel le module est abonné
void callback(char* topic, byte* payload, unsigned int length) {
  String topicString = topic;
  String payloadString = (char*)payload;
  String fullPayloadString = payloadString.substring(0, length);

  if (topicString == HILO_POWER) {
    power = fullPayloadString.c_str();
    Serial.println("Power: " + power);
  } 

  if (topicString == HILO_ENERGY) {     // To test by disabling Node-Red here
    energy = fullPayloadString.c_str();
    Serial.println("Energy: " + energy);
    if (currentState == NO_TEXT) {      // If display is blank and get data show power
      currentState = INSTANCE_3;
    }
    lastUpdateData = millis();          // Update data counter
    lastUpdateEnergy = millis();        // Make sure to show power first and then energy
  } 
}


//******************************************************************************************************************
// Start MQTT
void startMQTT() {
  client.setServer(SERVERMQTT, SERVERPORT);
  client.setCallback(callback);
  reconnectMQTT();    
}


//******************************************************************************************************************
void printPower() {
    static unsigned long lastUpdate = 0;

    if (millis() - lastUpdate >= DELAYTIME) {
      lastUpdate = millis();
      P.print(power);
    }
}


//******************************************************************************************************************
// Non-blocking scrolling function using Parola
bool scrollTextNonBlocking(const char *text) {
  static bool initialized = false;  // Track if display text is set initially

  if (!initialized) {
    P.displayClear();
    P.displayText(text, PA_CENTER, 150, 50, PA_SCROLL_LEFT, PA_SCROLL_LEFT);    // 150, 50
    initialized = true;
  }

  if (P.displayAnimate()) {  // Animate the text, returns true when done
    P.displayReset();        // Reset for next scroll
    initialized = false;           // Reset flag for the next call
    return true;                   // Scrolling complete
  }

  return false;  // Scrolling in progress
}


//******************************************************************************************************************
// For a device blocked in Deco wifi needed to reconnect.  Hypotetic and non-realistic but in any case.
// If blocked in Deco wifi system needs to restart to reconnect after an unblock in Deco
void restartESP() {
  Serial.println("Failed to connect, restart in 10s");
  delay(10000);
  ESP.restart();

}


//**************************************************************************************************
void setup() {
  Serial.begin(115200);
  neopixelWrite(RGB_BUILTIN, 0, 0, 0);     // Turn OFF LED
  delay(1000);
  Serial.println("House power display module");

  P.begin();
  //P.setTextAlignment(PA_RIGHT);  // Set text alignment to right
  P.setTextAlignment(PA_CENTER);
  P.setIntensity(1);  // Set brightness level (0-15, with 1 as very dim) 4
  P.displayClear(); 

  WiFiManager wm;

  wm.setConfigPortalTimeout(180);
  wm.setAPCallback(configModeCallback);  // Set the callback function
  // reset settings - wipe stored credentials for testing
  // these are stored by the esp library
  // wm.resetSettings();

  bool res;
  res = wm.autoConnect("PowerDisp24","password"); // password protected ap

  if(!res) {
      restartESP();
  } 
  else {
      //if you get here you have connected to the WiFi    
      Serial.println("Connected to WiFi");
  }

  if (WiFi.status() == WL_CONNECTED) {
    message = "Wifi GOOD";
  } else {
    message = "No Wifi";
  }

  startMQTT();                                // Start MQTT
  t_mqtt.every(period_mqtt, reconnectMQTT);   // MQTT reconnect timer 

  ElegantOTA.begin(&server, "jp", "delphijpl");    // Start ElegantOTA with credentials
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);

  server.on("/", []() {
    server.send(200, "text/plain", "M24 power display module");
  });

  server.begin();
  Serial.println("HTTP server started");

}


//**************************************************************************************************
void loop() {

  switch (currentState) {
    case INSTANCE_1:
      scrolling = true;
      if (scrollTextNonBlocking(message)) {
        Serial.println("Instance 1 complete");
        scrolling = false;
        currentState = INSTANCE_2; // Move to the second instance
      }
      break; 

    case INSTANCE_2:
      scrolling = true;
      message =  "Power";
      if (scrollTextNonBlocking(message)) {
        Serial.println("Instance 2 complete");
        scrolling = false;
        //currentState = INSTANCE_1;
        currentState = INSTANCE_3; // Reset to the first instance
      }
      break; 

    case INSTANCE_3:
      printPower();
      break; 

    case INSTANCE_4:
      scrolling = true;
      if (scrollTextNonBlocking(energy.c_str())) {
        Serial.println("Instance 4 complete");
        scrolling = false;
        currentState = INSTANCE_3; // Move to the second instance
      }  
      break; 

    case NO_TEXT:
      P.displayClear(); 
      break; 

  }

  if (!scrolling) {     // Scrolling letters are bad if handling wifi
    if (millis() - lastUpdateEnergy >= DELAYTIME_ENERGY && energy != "" && currentState != NO_TEXT) {    // Update energy display
      lastUpdateEnergy = millis();
      currentState = INSTANCE_4;
    }  

    if (millis() - lastUpdateData >= DELAYTIME_NODATA && currentState != NO_TEXT) {    // No recent data so blank display
      currentState = NO_TEXT;
    }      

    checkMQTT();  
    server.handleClient();
    ElegantOTA.loop();
  }  

}