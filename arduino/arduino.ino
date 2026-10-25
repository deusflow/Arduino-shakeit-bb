#include <SPI.h>
#include <WiFiNINA.h>
#include <Arduino_MKRIoTCarrier.h>
#include <ArduinoJson.h>

#include "arduino_secrets.h"
#include "config.h"
#include "countries.h"

MKRIoTCarrier carrier;

enum AppState {
  STATE_CHOOSE_REGION,
  STATE_WAIT_SHAKE,
  STATE_COUNTRY_SHOWN,
  STATE_ERROR
};

enum RegionChoice {
  REGION_NONE = 0,
  REGION_EUROPE = 1,
  REGION_ASIA = 2,
  REGION_AFRICA = 3
};

AppState currentState = STATE_CHOOSE_REGION;
RegionChoice selectedRegion = REGION_NONE;

struct Country {
  char name[48];
  char capital[48];
  char cca3[4];
  char region[24];
  long population;
};

Country currentCountry;
unsigned long lastShakeTime = 0;

const char* pickRandomCountry(RegionChoice region) {
  if (region < REGION_EUROPE || region > REGION_AFRICA) return nullptr;
  const char* const* lists[] = { europeCountries, asiaCountries, africaCountries };
  const int counts[] = { europeCount, asiaCount, africaCount };
  int idx = (int)region - 1;
  if (counts[idx] == 0) return nullptr;
  return lists[idx][random(0, counts[idx])];
}

const char* getRegionName(RegionChoice region) {
  switch (region) {
    case REGION_EUROPE: return "Europe";
    case REGION_ASIA:   return "Asia";
    case REGION_AFRICA: return "Africa";
    default:            return "Unknown";
  }
}

bool checkShake() {
  if (!carrier.IMUmodule.accelerationAvailable()) {
    return false;
  }

  float x, y, z;
  carrier.IMUmodule.readAcceleration(x, y, z);
  float magnitude = sqrt(x * x + y * y + z * z);
  float diff = fabs(magnitude - 1.0f);

  if (diff > SHAKE_THRESHOLD) {
    unsigned long now = millis();
    if (now - lastShakeTime > SHAKE_COOLDOWN_MS) {
      lastShakeTime = now;
      Serial.print("Shake detected! Force diff: ");
      Serial.println(diff);
      return true;
    }
  }

  return false;
}

void drawHeader(const char* title, uint16_t color = ST77XX_WHITE, int y = 35) {
  carrier.display.fillScreen(ST77XX_BLACK);
  carrier.display.setTextColor(color);
  carrier.display.setTextWrap(false);
  carrier.display.setTextSize(2);
  carrier.display.setCursor(30, y);
  carrier.display.println(title);
  carrier.display.drawFastHLine(30, y + 23, 180, 0x4208);
}

void drawChooseRegionScreen() {
  drawHeader("Choose Region");

  carrier.display.setTextSize(1);
  carrier.display.setTextColor(ST77XX_WHITE);
  carrier.display.setCursor(35, 75);
  carrier.display.println("TOUCH 1: Europe");

  carrier.display.setCursor(35, 100);
  carrier.display.println("TOUCH 2: Asia");

  carrier.display.setCursor(35, 125);
  carrier.display.println("TOUCH 3: Africa");

  carrier.display.drawFastHLine(30, 150, 180, 0x4208);

  carrier.display.setCursor(30, 170);
  carrier.display.setTextColor(0x7BEF);
  carrier.display.println("Touch button 1-3");
}

void drawWaitShakeScreen(const char* regionName) {
  drawHeader(regionName, ST77XX_WHITE, 40);

  carrier.display.setTextSize(2);
  carrier.display.setCursor(30, 95);
  carrier.display.setTextColor(0xFBE0);
  carrier.display.println("SHAKE ME!");

  carrier.display.setTextSize(1);
  carrier.display.setTextColor(ST77XX_WHITE);
  carrier.display.setCursor(30, 140);
  carrier.display.println("Shake board to travel");

  carrier.display.drawFastHLine(30, 175, 180, 0x4208);
  carrier.display.setCursor(30, 190);
  carrier.display.setTextColor(0x7BEF);
  carrier.display.println("TOUCH 4: Back to menu");
}

void drawLoadingScreen(const char* countryName) {
  drawHeader("Traveling...", ST77XX_WHITE, 45);

  carrier.display.setTextSize(1);
  carrier.display.setCursor(30, 95);
  carrier.display.println("Destination:");

  carrier.display.setTextSize(2);
  carrier.display.setCursor(30, 115);
  carrier.display.setTextColor(0x7BEF);
  carrier.display.println(countryName);

  carrier.display.setTextSize(1);
  carrier.display.setCursor(30, 160);
  carrier.display.setTextColor(ST77XX_WHITE);
  carrier.display.println("Fetching facts...");
}

void drawCountryScreen(const Country& c) {
  drawHeader(c.name, ST77XX_WHITE, 35);

  carrier.display.setTextSize(1);
  carrier.display.setTextColor(ST77XX_WHITE);
  carrier.display.setCursor(30, 72);
  carrier.display.print("Capital: ");
  carrier.display.println(c.capital);

  carrier.display.setCursor(30, 95);
  carrier.display.print("Region:  ");
  carrier.display.println(c.region);

  carrier.display.setCursor(30, 118);
  carrier.display.print("Code:    ");
  carrier.display.println(c.cca3);

  carrier.display.setCursor(30, 141);
  carrier.display.print("Pop.:    ");
  carrier.display.println(c.population);

  carrier.display.drawFastHLine(30, 165, 180, 0x4208);

  carrier.display.setCursor(30, 178);
  carrier.display.setTextColor(ST77XX_GREEN);
  carrier.display.println("0: Save   Shake: Reroll");

  carrier.display.setCursor(30, 198);
  carrier.display.setTextColor(0x7BEF);
  carrier.display.println("4: Reset to menu");
}

void drawSavingScreen(const char* countryName) {
  drawHeader("Saving...", ST77XX_WHITE, 60);

  carrier.display.setTextSize(1);
  carrier.display.setTextColor(ST77XX_WHITE);
  carrier.display.setCursor(30, 115);
  carrier.display.println(countryName);

  carrier.display.setCursor(30, 145);
  carrier.display.setTextColor(0x7BEF);
  carrier.display.println("Sending to PC backend");
}

void drawSavedScreen(const char* countryName) {
  drawHeader("Saved!", ST77XX_GREEN, 50);

  carrier.display.setTextColor(ST77XX_WHITE);
  carrier.display.setTextSize(1);
  carrier.display.setCursor(30, 100);
  carrier.display.println(countryName);

  carrier.display.setCursor(30, 130);
  carrier.display.setTextColor(0x7BEF);
  carrier.display.println("Confirmed by PC (201)");
}

void drawErrorScreen(const char* shortMsg) {
  drawHeader("ERROR", ST77XX_RED, 45);

  carrier.display.setTextColor(ST77XX_WHITE);
  carrier.display.setTextSize(1);
  carrier.display.setCursor(30, 90);
  carrier.display.println(shortMsg);

  carrier.display.setTextColor(0xFBE0);
  carrier.display.setCursor(30, 130);
  carrier.display.println("No country kept.");

  carrier.display.setTextColor(0x7BEF);
  carrier.display.setCursor(30, 175);
  carrier.display.println("TOUCH 4: Reset menu");
}

void triggerError(const char* shortMsg, const char* detailMsg) {
  Serial.print("ERROR: ");
  Serial.print(shortMsg);
  Serial.print(" - ");
  Serial.println(detailMsg);

  currentState = STATE_ERROR;
  drawErrorScreen(shortMsg);
}

int readStatusCode(Client& client) {
  unsigned long waitStart = millis();
  while (!client.available()) {
    if (millis() - waitStart > HTTP_TIMEOUT_MS) {
      client.stop();
      triggerError("Timeout", "Server response timeout");
      return -1;
    }
    delay(10);
  }

  String statusLine = client.readStringUntil('\n');
  int statusCode = 0;
  int firstSpace = statusLine.indexOf(' ');
  if (firstSpace > 0) {
    statusCode = statusLine.substring(firstSpace + 1, firstSpace + 4).toInt();
  }
  return statusCode;
}

bool connectWiFi() {
  if (WiFi.status() == WL_NO_SHIELD) {
    triggerError("No WiFi", "WiFi shield not present");
    return false;
  }

  carrier.display.fillScreen(ST77XX_BLACK);
  carrier.display.setTextColor(ST77XX_WHITE);
  carrier.display.setTextSize(1);
  carrier.display.setCursor(30, 100);
  carrier.display.println("Connecting WiFi...");

  Serial.print("Connecting to WiFi: ");
  Serial.println(SECRET_SSID);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 5) {
    attempts++;
    Serial.print("Connecting to WiFi (attempt ");
    Serial.print(attempts);
    Serial.println("/5)...");
    WiFi.begin(SECRET_SSID, SECRET_PASS);

    unsigned long startWait = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startWait < 5000) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();
  }

  if (WiFi.status() != WL_CONNECTED) {
    triggerError("WiFi Failed", "Could not connect to WiFi");
    return false;
  }

  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

bool fetchCountry(const char* countryName) {
  WiFiSSLClient sslClient;
  sslClient.setTimeout(HTTP_TIMEOUT_MS);

  Serial.print("Connecting to ");
  Serial.print(API_HOST);
  Serial.print(":");
  Serial.println(API_PORT);

  if (!sslClient.connect(API_HOST, API_PORT)) {
    triggerError("TLS Failed", "SSL connect failed (check root certificate)");
    return false;
  }

  sslClient.print("GET ");
  sslClient.print(API_PATH_PREFIX);
  for (int i = 0; countryName[i] != '\0'; i++) {
    uint8_t b = (uint8_t)countryName[i];
    if ((b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z') || (b >= '0' && b <= '9') || b == '-' || b == '_' || b == '.' || b == '~') {
      sslClient.print((char)b);
    } else {
      char hexBuf[4];
      snprintf(hexBuf, sizeof(hexBuf), "%%%02X", b);
      sslClient.print(hexBuf);
    }
  }
  sslClient.print(" HTTP/1.0\r\n");
  sslClient.print("Host: ");
  sslClient.print(API_HOST);
  sslClient.print("\r\n");
  sslClient.print("User-Agent: ArduinoOpla\r\n");
  sslClient.print("Connection: close\r\n\r\n");

  int statusCode = readStatusCode(sslClient);
  if (statusCode < 0) return false;

  if (statusCode != 200) {
    sslClient.stop();
    char codeBuf[32];
    snprintf(codeBuf, sizeof(codeBuf), "HTTP %d", statusCode);
    triggerError(codeBuf, "Unexpected response status");
    return false;
  }

  bool foundHeaderEnd = false;
  while (sslClient.connected() || sslClient.available()) {
    String line = sslClient.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) {
      foundHeaderEnd = true;
      break;
    }
  }

  if (!foundHeaderEnd) {
    sslClient.stop();
    triggerError("Bad Response", "Premature end of headers");
    return false;
  }

  JsonDocument filter;
  filter["name"]["common"] = true;
  filter["capital"][0] = true;
  filter["cca3"] = true;
  filter["region"] = true;
  filter["population"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, sslClient, DeserializationOption::Filter(filter));
  sslClient.stop();

  if (err) {
    char errBuf[32];
    snprintf(errBuf, sizeof(errBuf), "JSON: %s", err.c_str());
    triggerError(errBuf, err.c_str());
    return false;
  }

  const char* common = doc["name"]["common"];
  const char* cap = doc["capital"][0];
  const char* c3 = doc["cca3"];
  const char* reg = doc["region"];

  if (!common || strlen(common) == 0 ||
      !cap || strlen(cap) == 0 ||
      !c3 || strlen(c3) != 3 ||
      !reg || strlen(reg) == 0 ||
      !doc["population"].is<long>() || doc["population"].as<long>() < 0) {
    triggerError("Bad Data", "Country payload missing required fields");
    return false;
  }

  snprintf(currentCountry.name, sizeof(currentCountry.name), "%s", common);
  snprintf(currentCountry.capital, sizeof(currentCountry.capital), "%s", cap);
  snprintf(currentCountry.cca3, sizeof(currentCountry.cca3), "%s", c3);
  snprintf(currentCountry.region, sizeof(currentCountry.region), "%s", reg);
  currentCountry.population = doc["population"].as<long>();

  return true;
}

bool saveCountryToBackend(const Country& c) {
  Serial.print("Connecting to backend: ");
  Serial.print(BACKEND_HOST);
  Serial.print(":");
  Serial.println(BACKEND_PORT);

  WiFiClient client;
  client.setTimeout(HTTP_TIMEOUT_MS);

  if (!client.connect(BACKEND_HOST, BACKEND_PORT)) {
    triggerError("Backend Fail", "Could not connect to PC backend");
    return false;
  }

  JsonDocument doc;
  doc["apiName"] = "Mercantec Countries API";
  doc["choice"] = c.name;
  doc["cca3"] = c.cca3;
  doc["capital"] = c.capital;
  doc["region"] = c.region;
  doc["population"] = c.population;

  String body;
  serializeJson(doc, body);

  client.print("POST ");
  client.print(BACKEND_PATH);
  client.print(" HTTP/1.0\r\n");
  client.print("Host: ");
  client.print(BACKEND_HOST);
  client.print("\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print("Content-Length: ");
  client.print(body.length());
  client.print("\r\n");
  client.print("Connection: close\r\n\r\n");
  client.print(body);

  int statusCode = readStatusCode(client);
  client.stop();

  Serial.print("Backend response status: ");
  Serial.println(statusCode);

  if (statusCode == 201) {
    return true;
  } else {
    char errBuf[32];
    snprintf(errBuf, sizeof(errBuf), "Save HTTP %d", statusCode);
    triggerError(errBuf, "Backend rejected country payload");
    return false;
  }
}

void resetToMenu() {
  selectedRegion = REGION_NONE;
  currentState = STATE_CHOOSE_REGION;
  drawChooseRegionScreen();
}

void rollCountry() {
  const char* picked = pickRandomCountry(selectedRegion);
  if (!picked) {
    triggerError("No Countries", "Empty country list for region");
    return;
  }

  drawLoadingScreen(picked);

  if (fetchCountry(picked)) {
    currentState = STATE_COUNTRY_SHOWN;
    drawCountryScreen(currentCountry);
  }
}

void handleChooseRegion() {
  RegionChoice choice = REGION_NONE;
  if (carrier.Buttons.onTouchDown(TOUCH1)) choice = REGION_EUROPE;
  else if (carrier.Buttons.onTouchDown(TOUCH2)) choice = REGION_ASIA;
  else if (carrier.Buttons.onTouchDown(TOUCH3)) choice = REGION_AFRICA;

  if (choice != REGION_NONE) {
    randomSeed(micros());
    selectedRegion = choice;
    currentState = STATE_WAIT_SHAKE;
    drawWaitShakeScreen(getRegionName(selectedRegion));
  }
}

void handleWaitShake() {
  if (carrier.Buttons.onTouchDown(TOUCH4)) {
    resetToMenu();
    return;
  }

  if (checkShake()) {
    rollCountry();
  }
}

void handleCountryShown() {
  if (carrier.Buttons.onTouchDown(TOUCH4)) {
    resetToMenu();
    return;
  }

  if (carrier.Buttons.onTouchDown(TOUCH0)) {
    drawSavingScreen(currentCountry.name);
    if (saveCountryToBackend(currentCountry)) {
      drawSavedScreen(currentCountry.name);
      delay(SAVED_SCREEN_DURATION_MS);
      drawCountryScreen(currentCountry);
    }
    return;
  }

  if (checkShake()) {
    rollCountry();
  }
}

void handleError() {
  if (carrier.Buttons.onTouchDown(TOUCH4)) {
    resetToMenu();
  }
}

void setup() {
  Serial.begin(SERIAL_BAUD);

  CARRIER_CASE = false;
  carrier.begin();

  carrier.display.fillScreen(ST77XX_BLACK);
  carrier.display.setTextColor(ST77XX_WHITE);
  carrier.display.setTextSize(2);
  carrier.display.setCursor(30, 80);
  carrier.display.println("ShakeTravel");
  carrier.display.setTextSize(1);
  carrier.display.setCursor(30, 120);
  carrier.display.println("Initializing...");

  if (!connectWiFi()) {
    return;
  }

  currentState = STATE_CHOOSE_REGION;
  drawChooseRegionScreen();
}

void loop() {
  carrier.Buttons.update();

  switch (currentState) {
    case STATE_CHOOSE_REGION:
      handleChooseRegion();
      break;
    case STATE_WAIT_SHAKE:
      handleWaitShake();
      break;
    case STATE_COUNTRY_SHOWN:
      handleCountryShown();
      break;
    case STATE_ERROR:
      handleError();
      break;
  }

  delay(20);
}
