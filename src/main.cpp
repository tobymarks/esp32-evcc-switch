#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#if __has_include("Config.h")
#include "Config.h"
#endif

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD ""
#endif
#ifndef EVCC_HOST
#define EVCC_HOST "evcc.local"
#endif
#ifndef EVCC_PORT
#define EVCC_PORT 7070
#endif
#ifndef EVCC_LOADPOINT_ID
#define EVCC_LOADPOINT_ID 1
#endif
#ifndef WALLBOX_LABEL
#define WALLBOX_LABEL "Garage"
#endif
#ifndef APP_VERSION
#define APP_VERSION "0.1.0-dev"
#endif

namespace {

constexpr uint16_t ScreenW = 320;
constexpr uint16_t ScreenH = 240;

constexpr uint8_t TouchIrq = 36;
constexpr uint8_t TouchMosi = 32;
constexpr uint8_t TouchMiso = 39;
constexpr uint8_t TouchClk = 25;
constexpr uint8_t TouchCs = 33;

constexpr int TouchMinX = 200;
constexpr int TouchMaxX = 3700;
constexpr int TouchMinY = 240;
constexpr int TouchMaxY = 3800;

constexpr uint32_t PollIntervalMs = 5000;
constexpr uint32_t WifiRetryMs = 10000;
constexpr uint32_t TouchDebounceMs = 450;

constexpr uint16_t ColorBg = TFT_BLACK;
constexpr uint16_t ColorPanel = 0x18E3;
constexpr uint16_t ColorPanel2 = 0x2945;
constexpr uint16_t ColorText = TFT_WHITE;
constexpr uint16_t ColorMuted = 0xBDF7;
constexpr uint16_t ColorOk = 0x45E7;
constexpr uint16_t ColorWarn = 0xFDC0;
constexpr uint16_t ColorError = 0xF986;
constexpr uint16_t ColorBlue = 0x2D7F;

struct EvccState {
  String title = WALLBOX_LABEL;
  String mode = "-";
  String vehicle = "";
  bool connected = false;
  bool charging = false;
  bool enabled = false;
  int vehicleSoc = -1;
  float chargePower = 0;
  float pvPower = 0;
  float gridPower = 0;
  bool evccOk = false;
  String error = "Noch kein EVCC-Status";
  uint32_t lastUpdate = 0;
};

struct AppConfig {
  char evccHost[64] = EVCC_HOST;
  uint16_t evccPort = EVCC_PORT;
  uint8_t loadpointId = EVCC_LOADPOINT_ID;
  char label[32] = WALLBOX_LABEL;
};

struct ModeButton {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
  const char *label;
  const char *mode;
  uint16_t color;
};

TFT_eSPI tft;
SPIClass touchSpi(VSPI);
XPT2046_Touchscreen touch(TouchCs, TouchIrq);
Preferences preferences;
AppConfig appConfig;
EvccState state;

ModeButton buttons[] = {
    {8, 136, 148, 42, "Aus", "off", ColorError},
    {164, 136, 148, 42, "PV", "pv", ColorOk},
    {8, 188, 148, 42, "Min+PV", "minpv", ColorWarn},
    {164, 188, 148, 42, "Schnell", "now", ColorBlue},
};

uint32_t lastPoll = 0;
uint32_t lastWifiAttempt = 0;
uint32_t lastTouch = 0;

String evccBaseUrl() {
  return String("http://") + appConfig.evccHost + ":" + String(appConfig.evccPort);
}

String modeLabel(const String &mode) {
  if (mode == "off") {
    return "Aus";
  }
  if (mode == "pv") {
    return "PV";
  }
  if (mode == "minpv") {
    return "Min+PV";
  }
  if (mode == "now") {
    return "Schnell";
  }
  return mode.length() ? mode : "-";
}

String boolLabel(bool value) {
  return value ? "ja" : "nein";
}

String formatPower(float watts) {
  char buffer[16];
  const float absWatts = fabs(watts);
  if (absWatts >= 1000.0f) {
    snprintf(buffer, sizeof(buffer), "%.1f kW", watts / 1000.0f);
  } else {
    snprintf(buffer, sizeof(buffer), "%.0f W", watts);
  }
  return String(buffer);
}

void drawText(int16_t x, int16_t y, const String &text, uint16_t color, uint8_t size = 1) {
  tft.setTextColor(color, ColorBg);
  tft.setTextSize(size);
  tft.setCursor(x, y);
  tft.print(text);
}

void drawStatusLine(int16_t y, const String &label, const String &value, uint16_t valueColor = ColorText) {
  tft.setTextSize(1);
  tft.setTextColor(ColorMuted, ColorBg);
  tft.setCursor(12, y);
  tft.print(label);
  tft.setTextColor(valueColor, ColorBg);
  tft.setCursor(112, y);
  tft.print(value);
}

void drawButton(const ModeButton &button) {
  const bool active = state.mode == button.mode;
  const uint16_t fill = active ? button.color : ColorPanel;
  const uint16_t border = active ? TFT_WHITE : ColorPanel2;
  const uint16_t text = active ? TFT_BLACK : ColorText;

  tft.fillRoundRect(button.x, button.y, button.w, button.h, 6, fill);
  tft.drawRoundRect(button.x, button.y, button.w, button.h, 6, border);
  tft.setTextColor(text, fill);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(button.label, button.x + button.w / 2, button.y + button.h / 2);
  tft.setTextDatum(TL_DATUM);
}

void render() {
  tft.fillScreen(ColorBg);

  tft.fillRect(0, 0, ScreenW, 34, ColorPanel);
  tft.setTextColor(ColorText, ColorPanel);
  tft.setTextSize(2);
  tft.setCursor(10, 9);
  tft.print(state.title);

  const bool wifiOk = WiFi.status() == WL_CONNECTED;
  tft.setTextSize(1);
  tft.setTextColor(wifiOk && state.evccOk ? ColorOk : ColorWarn, ColorPanel);
  tft.setCursor(228, 12);
  tft.print(wifiOk ? (state.evccOk ? "online" : "wifi") : "offline");

  drawStatusLine(44, "Modus", modeLabel(state.mode), ColorOk);
  drawStatusLine(60, "Auto", state.connected ? "verbunden" : "nicht verbunden", state.connected ? ColorOk : ColorMuted);
  drawStatusLine(76, "Laedt", boolLabel(state.charging), state.charging ? ColorOk : ColorMuted);
  drawStatusLine(92, "Leistung", formatPower(state.chargePower));

  String energyLine = "PV " + formatPower(state.pvPower) + "  Netz " + formatPower(state.gridPower);
  drawText(12, 112, energyLine, ColorMuted);

  if (state.vehicleSoc >= 0) {
    String socLine = "SoC " + String(state.vehicleSoc) + "%";
    if (state.vehicle.length()) {
      socLine += "  " + state.vehicle;
    }
    drawText(190, 44, socLine, ColorMuted);
  }

  for (const auto &button : buttons) {
    drawButton(button);
  }

  if (!state.evccOk && state.error.length()) {
    tft.fillRect(0, 231, ScreenW, 9, ColorBg);
    drawText(8, 231, state.error.substring(0, 44), ColorError);
  }
}

void loadAppConfig() {
  preferences.begin("evccswitch", true);
  String host = preferences.getString("evccHost", EVCC_HOST);
  String label = preferences.getString("label", WALLBOX_LABEL);
  appConfig.evccPort = preferences.getUShort("evccPort", EVCC_PORT);
  appConfig.loadpointId = preferences.getUChar("loadpointId", EVCC_LOADPOINT_ID);
  preferences.end();

  host.toCharArray(appConfig.evccHost, sizeof(appConfig.evccHost));
  label.toCharArray(appConfig.label, sizeof(appConfig.label));
  if (appConfig.loadpointId == 0) {
    appConfig.loadpointId = 1;
  }
  state.title = appConfig.label;
}

void saveAppConfig() {
  preferences.begin("evccswitch", false);
  preferences.putString("evccHost", appConfig.evccHost);
  preferences.putUShort("evccPort", appConfig.evccPort);
  preferences.putUChar("loadpointId", appConfig.loadpointId);
  preferences.putString("label", appConfig.label);
  preferences.end();
}

void showSetupPortalInfo() {
  tft.fillScreen(ColorBg);
  drawText(12, 20, "Setup", ColorText, 2);
  drawText(12, 56, "Mit WLAN verbinden:", ColorMuted);
  drawText(12, 76, "EVCC-Switch", ColorOk, 2);
  drawText(12, 108, "Passwort:", ColorMuted);
  drawText(12, 128, "evccswitch", ColorOk, 2);
  drawText(12, 164, "Dann EVCC-IP und", ColorMuted);
  drawText(12, 180, "Loadpoint eintragen.", ColorMuted);
  drawText(12, 216, String("Version ") + APP_VERSION, ColorMuted);
}

void resetConfigurationIfRequested() {
  pinMode(0, INPUT_PULLUP);
  delay(50);
  if (digitalRead(0) == HIGH) {
    return;
  }

  tft.fillScreen(ColorBg);
  drawText(12, 40, "Reset gedrueckt", ColorWarn, 2);
  drawText(12, 80, "Konfiguration wird geloescht", ColorMuted);

  WiFiManager wm;
  wm.resetSettings();
  preferences.begin("evccswitch", false);
  preferences.clear();
  preferences.end();
  delay(1000);
  ESP.restart();
}

void setupNetwork() {
  WiFi.mode(WIFI_STA);

  char portBuffer[8];
  char loadpointBuffer[4];
  snprintf(portBuffer, sizeof(portBuffer), "%u", appConfig.evccPort);
  snprintf(loadpointBuffer, sizeof(loadpointBuffer), "%u", appConfig.loadpointId);

  WiFiManagerParameter evccHostParam("evcc_host", "EVCC Host/IP", appConfig.evccHost, sizeof(appConfig.evccHost));
  WiFiManagerParameter evccPortParam("evcc_port", "EVCC Port", portBuffer, sizeof(portBuffer));
  WiFiManagerParameter loadpointParam("loadpoint_id", "EVCC Loadpoint ID", loadpointBuffer, sizeof(loadpointBuffer));
  WiFiManagerParameter labelParam("label", "Display Label", appConfig.label, sizeof(appConfig.label));

  WiFiManager wm;
  wm.setTitle("ESP32 EVCC Switch");
  wm.setConfigPortalBlocking(true);
  wm.setConnectTimeout(20);
  wm.addParameter(&evccHostParam);
  wm.addParameter(&evccPortParam);
  wm.addParameter(&loadpointParam);
  wm.addParameter(&labelParam);

  showSetupPortalInfo();
  const bool connected = wm.autoConnect("EVCC-Switch", "evccswitch");
  if (!connected) {
    ESP.restart();
  }

  strlcpy(appConfig.evccHost, evccHostParam.getValue(), sizeof(appConfig.evccHost));
  strlcpy(appConfig.label, labelParam.getValue(), sizeof(appConfig.label));
  appConfig.evccPort = constrain(atoi(evccPortParam.getValue()), 1, 65535);
  appConfig.loadpointId = constrain(atoi(loadpointParam.getValue()), 1, 9);
  state.title = appConfig.label;
  saveAppConfig();
}

bool parseStatePayload(const String &payload) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    state.error = String("JSON Fehler: ") + error.c_str();
    return false;
  }

  JsonVariant data = doc.as<JsonVariant>();
  if (!data["result"].isNull()) {
    data = data["result"];
  }

  JsonArray loadpoints = data["loadpoints"].as<JsonArray>();
  if (loadpoints.isNull()) {
    state.error = "Keine loadpoints in /api/state";
    return false;
  }

  const int index = appConfig.loadpointId - 1;
  if (index < 0 || index >= static_cast<int>(loadpoints.size())) {
    state.error = "Loadpoint-ID nicht gefunden";
    return false;
  }

  JsonVariant loadpoint = loadpoints[index];
  state.title = loadpoint["title"] | appConfig.label;
  state.mode = loadpoint["mode"] | "-";
  state.vehicle = loadpoint["vehicleTitle"] | "";
  state.connected = loadpoint["connected"] | false;
  state.charging = loadpoint["charging"] | false;
  state.enabled = loadpoint["enabled"] | false;
  state.vehicleSoc = loadpoint["vehicleSoc"] | -1;
  state.chargePower = loadpoint["chargePower"] | 0.0f;
  state.pvPower = data["pvPower"] | (data["site"]["pvPower"] | 0.0f);
  state.gridPower = data["gridPower"] | (data["grid"]["power"] | 0.0f);
  state.evccOk = true;
  state.error = "";
  state.lastUpdate = millis();
  return true;
}

bool fetchEvccState() {
  if (WiFi.status() != WL_CONNECTED) {
    state.evccOk = false;
    state.error = "WLAN nicht verbunden";
    return false;
  }

  HTTPClient http;
  http.setTimeout(2500);
  const String url = evccBaseUrl() + "/api/state";
  if (!http.begin(url)) {
    state.evccOk = false;
    state.error = "EVCC URL ungueltig";
    return false;
  }

  const int statusCode = http.GET();
  if (statusCode != HTTP_CODE_OK) {
    state.evccOk = false;
    state.error = "EVCC HTTP " + String(statusCode);
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();
  const bool ok = parseStatePayload(payload);
  state.evccOk = ok;
  return ok;
}

bool setEvccMode(const char *mode) {
  if (WiFi.status() != WL_CONNECTED) {
    state.error = "WLAN nicht verbunden";
    state.evccOk = false;
    return false;
  }

  HTTPClient http;
  http.setTimeout(2500);
  const String url = evccBaseUrl() + "/api/loadpoints/" + String(appConfig.loadpointId) + "/mode/" + mode;
  if (!http.begin(url)) {
    state.error = "EVCC URL ungueltig";
    return false;
  }

  const int statusCode = http.POST("");
  http.end();

  if (statusCode < 200 || statusCode >= 300) {
    state.error = "Setzen fehlgeschlagen: " + String(statusCode);
    state.evccOk = false;
    return false;
  }

  state.mode = mode;
  state.error = "";
  return true;
}

bool readTouchPoint(int16_t &x, int16_t &y) {
  if (!touch.touched()) {
    return false;
  }

  TS_Point point = touch.getPoint();
  x = map(point.x, TouchMinX, TouchMaxX, 0, ScreenW);
  y = map(point.y, TouchMinY, TouchMaxY, 0, ScreenH);
  x = constrain(x, 0, ScreenW - 1);
  y = constrain(y, 0, ScreenH - 1);
  return true;
}

void handleTouch() {
  const uint32_t now = millis();
  if (now - lastTouch < TouchDebounceMs) {
    return;
  }

  int16_t x;
  int16_t y;
  if (!readTouchPoint(x, y)) {
    return;
  }

  lastTouch = now;
  for (const auto &button : buttons) {
    const bool inside = x >= button.x && x <= button.x + button.w && y >= button.y && y <= button.y + button.h;
    if (!inside) {
      continue;
    }

    tft.fillRect(0, 112, ScreenW, 16, ColorBg);
    drawText(12, 112, String("Setze Modus: ") + button.label, ColorWarn);
    setEvccMode(button.mode);
    fetchEvccState();
    render();
    return;
  }
}

} // namespace

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(ColorBg);

  touchSpi.begin(TouchClk, TouchMiso, TouchMosi, TouchCs);
  touch.begin(touchSpi);
  touch.setRotation(1);

  loadAppConfig();
  resetConfigurationIfRequested();
  setupNetwork();
  fetchEvccState();
  render();
}

void loop() {
  handleTouch();

  if (WiFi.status() != WL_CONNECTED) {
    const uint32_t now = millis();
    if (now - lastWifiAttempt > WifiRetryMs) {
      lastWifiAttempt = now;
      WiFi.reconnect();
    }
  }

  const uint32_t now = millis();
  if (now - lastPoll >= PollIntervalMs) {
    lastPoll = now;
    fetchEvccState();
    render();
  }

  delay(20);
}
