#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "time.h"

// WiFi setup
const char* ssid = "IgLights";
const char* password = "PolarBow";

// OpenWeatherMap API
const char* city = "Lubbock";
const char* apiKey = "fde8154ca85ad2aa561fc841b4bd4e25";

// Time settings
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -21600;
const int daylightOffset_sec = 3600;

// Screen setup
TFT_eSPI tft = TFT_eSPI();
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// State
unsigned long lastSwitch = 0;
bool showTimeScreen = true;

// Store weather info
String weatherDesc = "";
float weatherTemp = 0;
String weatherMain = "";

// Time + Weather functions

void initTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

String getTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Time Error";
  }
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%I:%M", &timeinfo);
  return String(buffer);
}

void getWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    weatherDesc = "No WiFi";
    weatherTemp = 0;
    weatherMain = "";
    return;
  }

  HTTPClient http;
  String url = "http://api.openweathermap.org/data/2.5/weather?q=" + String(city) + "&appid=" + String(apiKey) + "&units=imperial";

  http.begin(url);

  int httpCode = http.GET();
  if (httpCode > 0) {
    String payload = http.getString();
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      weatherDesc = "Parse Error";
      return;
    }
    weatherDesc = doc["weather"][0]["description"].as<String>();
    weatherMain = doc["weather"][0]["main"].as<String>();
    weatherTemp = doc["main"]["temp"].as<float>();
  } else {
    weatherDesc = "HTTP Error";
  }
  http.end();
}


void drawTimeScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(6);
  String now = getTimeString();
  tft.drawString(now, tft.width()/2, tft.height()/2);
}

void drawWeatherScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(6);
  String tempStr = String((int)weatherTemp) + "F";
  tft.drawString(tempStr, tft.width()/2, tft.height()/2 - 40);

  tft.setTextSize(3);
  tft.drawString(weatherDesc, tft.width()/2, tft.height()/2 + 40);

}

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected");

  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  initTime();
}

void loop() {
  unsigned long now = millis();

  if (now - lastSwitch > 20000) {
    lastSwitch = now;
    showTimeScreen = !showTimeScreen;

    if (showTimeScreen) {
      drawTimeScreen();
    } else {
      getWeather();
      drawWeatherScreen();
    }
  }

  if (showTimeScreen) {
    static unsigned long lastUpdate = 0;
    if (now - lastUpdate > 1000) {
      lastUpdate = now;
      drawTimeScreen();
    }
  }

  delay(100);
}
