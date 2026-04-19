#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, Arduino IDE -> tools -> PSRAM -> OPI !!!"
#endif

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "commute/commute_renderer.h"
#include "commute/commute_service.h"
#include "config.h"
#include "display_manager.h"
#include "epd_driver.h"
#include "utilities.h"
#include "weather/weather_renderer.h"
#include "weather/weather_service.h"

DisplayManager* display = nullptr;
WeatherService* weatherService = nullptr;
CommuteService* commuteService = nullptr;

bool connectToWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");

  int attempts = 0;
  const int maxAttempts = 20;

  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
    return true;
  } else {
    Serial.println("\nWiFi connection failed!");
    return false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Initializing display...");

  display = new DisplayManager();
  if (!display->initialize()) {
    Serial.println("Display initialization failed!");
    while (true) {
      delay(1000);
    }
  }

  weatherService = new WeatherService();
  commuteService = new CommuteService();

  if (!connectToWiFi()) {
    Serial.println("Cannot proceed without WiFi connection!");
    while (true) {
      delay(1000);
    }
  }

  // Sync time via NTP (needed for commute API requests)
  configTime(3600, 3600, "pool.ntp.org");
  Serial.println("Waiting for NTP time sync...");
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10000)) {
    Serial.println("Time synced.");
  } else {
    Serial.println("NTP sync failed, commute times may be wrong.");
  }

  Serial.println("Setup complete!");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, attempting to reconnect...");
    connectToWiFi();
    delay(UPDATE_INTERVAL_MS);
    return;
  }

  Serial.println("Fetching data...");
  std::vector<WeatherData> forecast = weatherService->fetchForecast();
  std::vector<CommuteRoute> routes = commuteService->fetchRoutes();

  display->clear();

  const int halfWidth = EPD_WIDTH / 2;
  WeatherRenderer weatherUI(display->getFramebuffer(), 0, 0, halfWidth,
                            EPD_HEIGHT);
  weatherUI.draw(forecast);

  CommuteRenderer commuteUI(display->getFramebuffer(), halfWidth, 0, halfWidth,
                            EPD_HEIGHT);
  commuteUI.draw(routes);

  display->refresh();

  delay(UPDATE_INTERVAL_MS);
}
