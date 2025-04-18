#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#include <string>

#include <ArduinoJson.h>

#include <Crypto.h>
#include <SHA256.h>

#include "secrets.h"

// Crypto parameters
#define HASH_SIZE 32
#define BLOCK_SIZE 64
static const char *key = KEY; // secrets.h
static uint32_t nonce;

// Wifi
static const char *ssid = WIFI_SSID;
static const char *password = WIFI_PASSWORD;
IPAddress staticIP(192, 168, 89, 32);
IPAddress gateway(192, 168, 89, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(192, 168, 89, 1);
static const char* hostname = "esp32-hpctl";

// REST
WebServer server(80);

struct MCState {
  // Heater level 0..30
  uint8_t heating;

  // Ventilation in level 0..255
  uint8_t vent_in;

  // Ventilation out level 0..6
  uint8_t vent_out;

  // Binary state of SSR0...SSR7
  // SSR3 is determined from vent_in
  // SSR5..SSR7 are set by heating
  uint8_t ssrs;

  // Input values. Are not written to mC but read from it.
  uint8_t inputs;
};

static MCState currentState = { 0, 0, 0, 0, 0 };
static MCState nextState = { 0, 0, 0, 0, 0 };

// True iff currentState != nextState -> triggers sync over i2c
static bool i2cDirty = true;
static bool guiDirty = true;
void stateChanged() {
  // We do not allow PWM values < 30 because the ventilator cannot start then
  // Disable heating if ventilation is off
  if (nextState.vent_in < 30){
    nextState.vent_in = 0;
    nextState.heating = 0;
  }
    
  i2cDirty = true;
  guiDirty = true;
}

// True iff there was a problem accessing i2c last time
// Prevents getting stuck in i2c routine
static bool i2cError = true;

uint64_t millis64() {
  static uint32_t low32, high32;
  uint32_t new_low32 = millis();
  if (new_low32 < low32) high32++;
  low32 = new_low32;
  return (uint64_t)high32 << 32 | low32;
}

void setup(void) {
  Serial.begin(115200);
  Serial.println("");

  setupButtons();
  setupGui();
  i2cError = !setupTwi();

  WiFi.mode(WIFI_STA);
  if (WiFi.config(staticIP, gateway, subnet, dns, dns) == false) {
    Serial.println("WiFi configuration failed.");
  }
  WiFi.setHostname(hostname);
  WiFi.begin(ssid, password);

  // Wait for connection
  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 10) {
    delay(1000);
    Serial.printf("Attempting to connect to %s\n", ssid);
    handleGuiNoWifi();
    retryCount++;
  }

  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  nonce = esp_random();

  if (MDNS.begin("hpctl")) {
    Serial.println("MDNS responder started");
  }

  registerWebserverAPI();

  server.begin();
  Serial.println("HTTP server started");
}

void ensureWiFiConnection() {
  static unsigned long lastWiFiCheck = 0;
  unsigned long currentMillis = millis();
  if (currentMillis - lastWiFiCheck < 180000) {  // Check every 3 minutes
    return;
  }
  lastWiFiCheck = currentMillis;

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost. Attempting to reconnect...");
    handleGuiNoWifi();
    WiFi.disconnect();
    WiFi.begin(ssid, password);

    // Wait for connection
    int retryCount = 0;
    while (WiFi.status() != WL_CONNECTED && retryCount < 5) {
      delay(1000);
      Serial.print(".");
      retryCount++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Reconnected to WiFi!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("Failed to reconnect to WiFi.");
    }
  }
}

void loop(void) {
  handleButtons();
  server.handleClient();
  handleTwi();
  handleGui();
  ensureWiFiConnection();
}
