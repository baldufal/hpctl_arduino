#define BYTE_TO_BINARY_PATTERN "%c %c %c %c %c %c %c %c"
#define BYTE_TO_BINARY(byte) \
  ((byte)&0x80 ? '1' : '0'), \
    ((byte)&0x40 ? '1' : '0'), \
    ((byte)&0x20 ? '1' : '0'), \
    ((byte)&0x10 ? '1' : '0'), \
    ((byte)&0x08 ? '1' : '0'), \
    ((byte)&0x04 ? '1' : '0'), \
    ((byte)&0x02 ? '1' : '0'), \
    ((byte)&0x01 ? '1' : '0')

#define NIBBLE_TO_BINARY_PATTERN "%c %c %c %c"
#define NIBBLE_TO_BINARY(byte) \
  ((byte)&0x08 ? '1' : '0'), \
    ((byte)&0x04 ? '1' : '0'), \
    ((byte)&0x02 ? '1' : '0'), \
    ((byte)&0x01 ? '1' : '0')

// Returns 0 iff ok.
int checkHash(const char* packet, size_t length, char* errorMsg) {

  // Compute HMAC over all body bytes after the first 32 bytes
  char hmacResult[HASH_SIZE];
  hmac<SHA256>(hmacResult, HASH_SIZE, key, strlen(key),
               packet + HASH_SIZE * 2, length - HASH_SIZE * 2);

  char hmacResultHex[HASH_SIZE * 2];
  for (size_t i = 0; i < HASH_SIZE; ++i) {
    uint lower = hmacResult[i] % 16;
    uint higher = hmacResult[i] / 16;

    hmacResultHex[2 * i + 1] = lower < 10 ? lower + 48 : lower + 87;
    hmacResultHex[2 * i] = higher < 10 ? higher + 48 : higher + 87;
  }

  if (memcmp(hmacResultHex, packet, HASH_SIZE * 2)) {
    strcpy(errorMsg, "Wrong HMAC");
    return -1;
  }

  return 0;
}

void registerWebserverAPI() {
  server.onNotFound(handleNotFound);

  server.on("/", handleRoot);

  server.on("/hpctlip", HTTP_GET, []() {
    server.send(200, "text/plain", WiFi.localIP().toString().c_str());
  });

  server.on("/json", HTTP_GET, []() {
    StaticJsonDocument<200> doc;

    doc["ssrs"] = currentState.ssrs;
    doc["heating"] = currentState.heating;
    doc["ventin"] = currentState.vent_in;
    doc["ventout"] = currentState.vent_out;
    doc["inputs"] = currentState.inputs;
    doc["i2c_error"] = i2cError;
    doc["i2c_dirty"] = i2cDirty;
    doc["uptime"] = millis();
    doc["nonce"] = nonce;
    doc["ip"] = WiFi.localIP().toString();
    doc["rssi"] = WiFi.RSSI();

    char output[200];
    serializeJson(doc, output);

    server.send(200, "application/json", output);
  });

  server.on("/json", HTTP_POST, []() {
    String body = server.arg("plain");
    uint bodyLength = body.length();

    if (bodyLength < HASH_SIZE * 2 + 4) {
      server.send(400, "text/plain", "Body too short. Must contain 64 Bytes Hex HMAC + JSON");
      return;
    }

    char errorMsg[30];

    if (checkHash(body.c_str(), bodyLength, errorMsg)) {
      server.send(400, "text/plain", errorMsg);
      return;
    }

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, body.c_str() + HASH_SIZE * 2, bodyLength - HASH_SIZE * 2);

    // Test if parsing succeeds.
    if (error) {
      strcpy(errorMsg, "Invalid JSON.");
      server.send(400, "text/plain", errorMsg);
      return;
    }

    if (!doc["nonce"].is<unsigned int>() || doc["nonce"] != nonce) {
      strcpy(errorMsg, "Invalid or missing NONCE.");
      server.send(400, "text/plain", errorMsg);
      return;
    }

    nonce = esp_random();

    String processed = "";

    if (doc["ssrs"].is<unsigned short>() && doc["ssrs"] < 32) {
      processed += " ssrs";

      uint8_t value = doc["ssrs"];
      uint8_t mask = 127;

      if (doc["ssrmask"].is<unsigned short>()) {
        uint8_t tempMask = doc["ssrmask"];
        if ((tempMask & 0b11101000) == 0) {  // Bits 7..5 and 3 must not be set
          processed += " ssrmask";
          mask = tempMask;
          }
      }

      nextState.ssrs &= ~mask;
      nextState.ssrs |= (value & mask);
      stateChanged();
    }

    if (doc["heating"].is<unsigned short>() && doc["heating"] <= 30) {
      processed += " heating";
      uint8_t value = doc["heating"];
      nextState.heating = value;
      stateChanged();
    }

    if (doc["ventin"].is<unsigned short>() && (doc["ventin"] == 0 || doc["ventin"] >= 30)) {
      processed += " ventin";
      uint8_t value = doc["ventin"];
      nextState.vent_in = value;
      stateChanged();
    }

    if (doc["ventout"].is<unsigned short>() && doc["ventout"] <= 6) {
      processed += " ventout";
      uint8_t value = doc["ventout"];
      nextState.vent_out = value;
      stateChanged();
    }

    server.send(200, "text/plain", String("processed:") + processed);
  });

  server.on("/nonce", []() {
    char s[11];
    sprintf(s, "%u", nonce);
    server.send(200, "text/plain", s);
  });
}

void handleRoot() {
  char temp[520];
  unsigned int sec = millis64() / (uint64_t)1000;
  unsigned int min = sec / 60;
  unsigned int hr = min / 60;
  unsigned int day = hr / 24;

  snprintf(temp, 520,
           "<html>\
  <head>\
    <title>HPCtl State</title>\
    <style>\
      body { background-color: #f0cce0;  }\
    </style>\
  </head>\
  <body>\
    <h1>HPCtl State</h1>\
    <p>I2C Error: %s</p>\
    <p>I2C Dirty: %s</p>\
    <p>IP: %s</p>\
    <p>RSSI: %d dBm</p>\
    <p>Uptime: %4ud %2uh %2um %2us</p>\
    <p>Nonce: %u</p>\
    <h2>Heating</h1>\
    <p>Heater Level: %02d</p>\
    <p>Ventilation IN: %d</p>\
    <p>Ventilation OUT: %d</p>\
    <h2>SSRs</h1>\
    <p>" BYTE_TO_BINARY_PATTERN "</p>\
    <h2>Inputs</h1>\
    <p>" NIBBLE_TO_BINARY_PATTERN "</p>\
  </body>\
  </html>",
           i2cError ? "true" : "false",
           i2cDirty ? "true" : "false",
           WiFi.localIP().toString().c_str(),
           WiFi.RSSI(),
           day, hr % 24, min % 60, sec % 60,
           nonce,
           currentState.heating,
           currentState.vent_in,
           currentState.vent_out,
           BYTE_TO_BINARY(currentState.ssrs),
           NIBBLE_TO_BINARY(currentState.inputs));
  server.send(200, "text/html", temp);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}
