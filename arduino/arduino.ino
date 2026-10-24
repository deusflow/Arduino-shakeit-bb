
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
  STATE_LOADING,
  STATE_COUNTRY_SHOWN,
  STATE_SAVING,
  STATE_SAVED,
  STATE_ERROR
};

enum RegionChoice {
  REGION_NONE = 0,
  REGION_EUROPE,
  REGION_ASIA,
  REGION_AFRICA
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
bool hasCurrentCountry = false;
bool isSaving = false;

unsigned long lastShakeTime = 0;
unsigned long savedStateEnterTime = 0;
bool randomSeeded = false;

extern "C" char* sbrk(int incr);
int getFreeRam() {
  char top;
  return &top - reinterpret_cast<char*>(sbrk(0));
}

// Stream wrapper that tracks bytes read by ArduinoJson
class CountingStream : public Stream {
private:
  Client& client;
  size_t bytesRead;
public:
  CountingStream(Client& c) : client(c), bytesRead(0) {}
  size_t getBytesRead() const { return bytesRead; }
  int available() override { return client.available(); }
  int read() override {
    int c = client.read();
    if (c >= 0) {
      bytesRead++;
    }
    return c;
  }
  int peek() override { return client.peek(); }
  void flush() override { client.flush(); }
  size_t write(uint8_t b) override { return client.write(b); }
};

void seedRandomOnce() {
  if (!randomSeeded) {
    randomSeed(micros() + millis());
    randomSeeded = true;
    Serial.println("Random generator seeded.");
  }
}

const char* pickRandomCountry(RegionChoice region) {
  int idx = 0;
  switch (region) {
    case REGION_EUROPE:
      if (europeCount == 0) return nullptr;
      idx = random(0, europeCount);
      return europeCountries[idx];
    case REGION_ASIA:
      if (asiaCount == 0) return nullptr;
      idx = random(0, asiaCount);
      return asiaCountries[idx];
    case REGION_AFRICA:
      if (africaCount == 0) return nullptr;
      idx = random(0, africaCount);
      return africaCountries[idx];
    default:
      return nullptr;
  }
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

  if (DEBUG_PRINT_ACCEL) {
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 250) {
      Serial.print("IMU: diff=");
      Serial.println(diff, 2);
      lastPrint = millis();
    }
  }

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

void drawChooseRegionScreen() {
  carrier.display.fillScreen(ST77XX_BLACK);
  carrier.display.setTextColor(ST77XX_WHITE);
  carrier.display.setTextWrap(false);

  carrier.display.setTextSize(2);
  carrier.display.setCursor(30, 35);
  carrier.display.println("Choose Region");

  carrier.display.drawFastHLine(30, 58, 180, 0x4208);

  carrier.display.setTextSize(1);
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
  carrier.display.fillScreen(ST77XX_BLACK);
  carrier.display.setTextColor(ST77XX_WHITE);
  carrier.display.setTextWrap(false);

  carrier.display.setTextSize(2);
  carrier.display.setCursor(30, 40);
  carrier.display.println(regionName);

  carrier.display.drawFastHLine(30, 65, 180, 0x4208);

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
  carrier.display.fillScreen(ST77XX_BLACK);
  carrier.display.setTextColor(ST77XX_WHITE);
  carrier.display.setTextWrap(false);

  carrier.display.setTextSize(2);
  carrier.display.setCursor(30, 45);
  carrier.display.println("Traveling...");

  carrier.display.drawFastHLine(30, 70, 180, 0x4208);

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
  carrier.display.fillScreen(ST77XX_BLACK);
  carrier.display.setTextColor(ST77XX_WHITE);
  carrier.display.setTextWrap(false);

  carrier.display.setTextSize(2);
  carrier.display.setCursor(30, 35);
  carrier.display.println(c.name);

  carrier.display.drawFastHLine(30, 58, 180, 0x4208);

  carrier.display.setTextSize(1);
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
  carrier.display.fillScreen(ST77XX_BLACK);
  carrier.display.setTextColor(ST77XX_WHITE);
  carrier.display.setTextWrap(false);

  carrier.display.setTextSize(2);
  carrier.display.setCursor(30, 60);
  carrier.display.println("Saving...");

  carrier.display.drawFastHLine(30, 90, 180, 0x4208);

  carrier.display.setTextSize(1);
  carrier.display.setCursor(30, 115);
  carrier.display.println(countryName);

  carrier.display.setCursor(30, 145);
  carrier.display.setTextColor(0x7BEF);
  carrier.display.println("Sending to PC backend");
}

void drawSavedScreen(const char* countryName) {
  carrier.display.fillScreen(ST77XX_BLACK);
  carrier.display.setTextColor(ST77XX_GREEN);
  carrier.display.setTextWrap(false);

  carrier.display.setTextSize(2);
  carrier.display.setCursor(30, 50);
  carrier.display.println("Saved!");

  carrier.display.drawFastHLine(30, 75, 180, 0x4208);

  carrier.display.setTextColor(ST77XX_WHITE);
  carrier.display.setTextSize(1);
  carrier.display.setCursor(30, 100);
  carrier.display.println(countryName);

  carrier.display.setCursor(30, 130);
  carrier.display.setTextColor(0x7BEF);
  carrier.display.println("Confirmed by PC (201)");

  carrier.display.setCursor(30, 170);
  carrier.display.setTextColor(ST77XX_WHITE);
  carrier.display.println("Shake: Reroll");
  carrier.display.setCursor(30, 190);
  carrier.display.setTextColor(0x7BEF);
  carrier.display.println("4: Back to menu");
}

void drawErrorScreen(const char* shortMsg) {
  carrier.display.fillScreen(ST77XX_BLACK);
  carrier.display.setTextColor(ST77XX_RED);
  carrier.display.setTextWrap(false);

  carrier.display.setTextSize(2);
  carrier.display.setCursor(30, 45);
  carrier.display.println("ERROR");

  carrier.display.drawFastHLine(30, 70, 180, 0x4208);

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
  hasCurrentCountry = false;

  Serial.print("ERROR: ");
  Serial.print(shortMsg);
  Serial.print(" - ");
  Serial.println(detailMsg);

  currentState = STATE_ERROR;
  drawErrorScreen(shortMsg);
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
  while (WiFi.status() != WL_CONNECTED && attempts < 15) {
    WiFi.begin(SECRET_SSID, SECRET_PASS);
    delay(1000);
    attempts++;
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    triggerError("WiFi Failed", "Could not connect to WiFi");
    return false;
  }

  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

bool fetchCountry(const char* countryName) {
  hasCurrentCountry = false;

  unsigned long startTime = millis();
  int ramBefore = getFreeRam();

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

  unsigned long waitStart = millis();
  while (!sslClient.available()) {
    if (millis() - waitStart > HTTP_TIMEOUT_MS) {
      sslClient.stop();
      triggerError("Timeout", "Server response timeout");
      return false;
    }
    delay(10);
  }

  String statusLine = sslClient.readStringUntil('\n');
  int statusCode = 0;
  int firstSpace = statusLine.indexOf(' ');
  if (firstSpace > 0) {
    statusCode = statusLine.substring(firstSpace + 1, firstSpace + 4).toInt();
  }

  if (statusCode != 200) {
    sslClient.stop();
    char codeBuf[32];
    snprintf(codeBuf, sizeof(codeBuf), "HTTP %d", statusCode);
    triggerError(codeBuf, statusLine.c_str());
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

  CountingStream stream(sslClient);

  StaticJsonDocument<256> filter;
  filter["name"]["common"] = true;
  filter["capital"][0] = true;
  filter["cca3"] = true;
  filter["region"] = true;
  filter["population"] = true;

  StaticJsonDocument<768> doc;
  DeserializationError err = deserializeJson(doc, stream, DeserializationOption::Filter(filter));
  sslClient.stop();

  int ramAfter = getFreeRam();
  unsigned long duration = millis() - startTime;
  size_t bytesRead = stream.getBytesRead();

  Serial.print("DEBUG: status=");
  Serial.print(statusCode);
  Serial.print(", bytes=");
  Serial.print(bytesRead);
  Serial.print(", ramBefore=");
  Serial.print(ramBefore);
  Serial.print(", ramAfter=");
  Serial.print(ramAfter);
  Serial.print(", timeMs=");
  Serial.println(duration);

  if (err) {
    char errBuf[32];
    snprintf(errBuf, sizeof(errBuf), "JSON: %s", err.c_str());
    triggerError(errBuf, err.c_str());
    return false;
  }

  const char* common = doc["name"]["common"];
  if (!common || strlen(common) == 0) {
    triggerError("Missing name", "name.common missing or empty");
    return false;
  }

  const char* cap = doc["capital"][0];
  if (!cap || strlen(cap) == 0) {
    triggerError("Missing cap", "capital[0] missing or empty");
    return false;
  }

  const char* c3 = doc["cca3"];
  if (!c3 || strlen(c3) != 3) {
    triggerError("Missing cca3", "cca3 missing or not 3 chars");
    return false;
  }

  const char* reg = doc["region"];
  if (!reg || strlen(reg) == 0) {
    triggerError("Missing reg", "region missing or empty");
    return false;
  }

  if (!doc.containsKey("population") || doc["population"].as<long>() < 0) {
    triggerError("Missing pop", "population missing or negative");
    return false;
  }

  strncpy(currentCountry.name, common, sizeof(currentCountry.name) - 1);
  currentCountry.name[sizeof(currentCountry.name) - 1] = '\0';

  strncpy(currentCountry.capital, cap, sizeof(currentCountry.capital) - 1);
  currentCountry.capital[sizeof(currentCountry.capital) - 1] = '\0';

  strncpy(currentCountry.cca3, c3, sizeof(currentCountry.cca3) - 1);
  currentCountry.cca3[sizeof(currentCountry.cca3) - 1] = '\0';

  strncpy(currentCountry.region, reg, sizeof(currentCountry.region) - 1);
  currentCountry.region[sizeof(currentCountry.region) - 1] = '\0';

  currentCountry.population = doc["population"].as<long>();
  hasCurrentCountry = true;

  return true;
}

bool saveCountryToBackend(const Country& c) {
  if (!hasCurrentCountry || isSaving) {
    return false;
  }

  isSaving = true;

  Serial.print("Connecting to backend: ");
  Serial.print(BACKEND_HOST);
  Serial.print(":");
  Serial.println(BACKEND_PORT);

  WiFiClient client;
  client.setTimeout(HTTP_TIMEOUT_MS);

  if (!client.connect(BACKEND_HOST, BACKEND_PORT)) {
    isSaving = false;
    triggerError("Backend Fail", "Could not connect to PC backend");
    return false;
  }

  StaticJsonDocument<256> doc;
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
  client.print(" HTTP/1.1\r\n");
  client.print("Host: ");
  client.print(BACKEND_HOST);
  client.print("\r\n");
  client.print("Content-Type: application/json\r\n");
  client.print("Content-Length: ");
  client.print(body.length());
  client.print("\r\n");
  client.print("Connection: close\r\n\r\n");
  client.print(body);

  unsigned long waitStart = millis();
  while (!client.available()) {
    if (millis() - waitStart > HTTP_TIMEOUT_MS) {
      client.stop();
      isSaving = false;
      triggerError("Save Timeout", "Backend response timeout");
      return false;
    }
    delay(10);
  }

  String statusLine = client.readStringUntil('\n');
  int statusCode = 0;
  int firstSpace = statusLine.indexOf(' ');
  if (firstSpace > 0) {
    statusCode = statusLine.substring(firstSpace + 1, firstSpace + 4).toInt();
  }

  client.stop();
  isSaving = false;

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

void handleChooseRegion() {
  if (carrier.Buttons.onTouchDown(TOUCH1)) {
    seedRandomOnce();
    selectedRegion = REGION_EUROPE;
    currentState = STATE_WAIT_SHAKE;
    drawWaitShakeScreen(getRegionName(selectedRegion));
  } else if (carrier.Buttons.onTouchDown(TOUCH2)) {
    seedRandomOnce();
    selectedRegion = REGION_ASIA;
    currentState = STATE_WAIT_SHAKE;
    drawWaitShakeScreen(getRegionName(selectedRegion));
  } else if (carrier.Buttons.onTouchDown(TOUCH3)) {
    seedRandomOnce();
    selectedRegion = REGION_AFRICA;
    currentState = STATE_WAIT_SHAKE;
    drawWaitShakeScreen(getRegionName(selectedRegion));
  }
}

void handleWaitShake() {
  if (carrier.Buttons.onTouchDown(TOUCH4)) {
    selectedRegion = REGION_NONE;
    hasCurrentCountry = false;
    currentState = STATE_CHOOSE_REGION;
    drawChooseRegionScreen();
    return;
  }

  if (checkShake()) {
    const char* picked = pickRandomCountry(selectedRegion);
    if (!picked) {
      triggerError("No Countries", "Empty country list for region");
      return;
    }

    currentState = STATE_LOADING;
    drawLoadingScreen(picked);

    if (fetchCountry(picked)) {
      currentState = STATE_COUNTRY_SHOWN;
      drawCountryScreen(currentCountry);
    }
  }
}

void handleLoading() {
}

void handleCountryShown() {
  if (isSaving) {
    return;
  }

  if (carrier.Buttons.onTouchDown(TOUCH4)) {
    hasCurrentCountry = false;
    selectedRegion = REGION_NONE;
    currentState = STATE_CHOOSE_REGION;
    drawChooseRegionScreen();
    return;
  }

  if (carrier.Buttons.onTouchDown(TOUCH0)) {
    if (hasCurrentCountry) {
      currentState = STATE_SAVING;
      drawSavingScreen(currentCountry.name);

      if (saveCountryToBackend(currentCountry)) {
        currentState = STATE_SAVED;
        savedStateEnterTime = millis();
        drawSavedScreen(currentCountry.name);
      } else if (currentState == STATE_SAVING) {
        currentState = STATE_COUNTRY_SHOWN;
      }
    }
    return;
  }

  if (checkShake()) {
    const char* picked = pickRandomCountry(selectedRegion);
    if (!picked) {
      triggerError("No Countries", "Empty country list for region");
      return;
    }

    currentState = STATE_LOADING;
    drawLoadingScreen(picked);

    if (fetchCountry(picked)) {
      currentState = STATE_COUNTRY_SHOWN;
      drawCountryScreen(currentCountry);
    }
  }
}

void handleSaving() {
}

void handleSaved() {
  if (carrier.Buttons.onTouchDown(TOUCH4)) {
    hasCurrentCountry = false;
    selectedRegion = REGION_NONE;
    currentState = STATE_CHOOSE_REGION;
    drawChooseRegionScreen();
    return;
  }

  if (checkShake()) {
    const char* picked = pickRandomCountry(selectedRegion);
    if (!picked) {
      triggerError("No Countries", "Empty country list for region");
      return;
    }

    currentState = STATE_LOADING;
    drawLoadingScreen(picked);

    if (fetchCountry(picked)) {
      currentState = STATE_COUNTRY_SHOWN;
      drawCountryScreen(currentCountry);
    }
    return;
  }

  if (millis() - savedStateEnterTime > SAVED_SCREEN_DURATION_MS) {
    if (hasCurrentCountry) {
      currentState = STATE_COUNTRY_SHOWN;
      drawCountryScreen(currentCountry);
    } else {
      currentState = STATE_CHOOSE_REGION;
      drawChooseRegionScreen();
    }
  }
}

void handleError() {
  if (carrier.Buttons.onTouchDown(TOUCH4) || carrier.Buttons.onTouchDown(TOUCH1) ||
      carrier.Buttons.onTouchDown(TOUCH2) || carrier.Buttons.onTouchDown(TOUCH3)) {
    hasCurrentCountry = false;
    selectedRegion = REGION_NONE;
    currentState = STATE_CHOOSE_REGION;
    drawChooseRegionScreen();
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
    case STATE_LOADING:
      handleLoading();
      break;
    case STATE_COUNTRY_SHOWN:
      handleCountryShown();
      break;
    case STATE_SAVING:
      handleSaving();
      break;
    case STATE_SAVED:
      handleSaved();
      break;
    case STATE_ERROR:
      handleError();
      break;
  }
}
