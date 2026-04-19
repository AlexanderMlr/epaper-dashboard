#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, Arduino IDE -> tools -> PSRAM -> OPI !!!"
#endif

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <time.h>

#include "commute/commute_renderer.h"
#include "commute/commute_service.h"
#include "config.h"
#include "display_manager.h"
#include "epd_driver.h"
#include "utilities.h"
#include "weather/weather_renderer.h"
#include "weather/weather_service.h"

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

uint64_t computeSleepMicros() {
  struct tm t;
  if (!getLocalTime(&t, 1000)) {
    return (uint64_t)UPDATE_INTERVAL_MS * 1000ULL;
  }

  const bool inQuiet = (QUIET_HOURS_START < QUIET_HOURS_END)
      ? (t.tm_hour >= QUIET_HOURS_START && t.tm_hour < QUIET_HOURS_END)
      : (t.tm_hour >= QUIET_HOURS_START || t.tm_hour < QUIET_HOURS_END);

  if (!inQuiet) {
    return (uint64_t)UPDATE_INTERVAL_MS * 1000ULL;
  }

  struct tm wake = t;
  wake.tm_hour = QUIET_HOURS_END;
  wake.tm_min = 0;
  wake.tm_sec = 0;
  time_t wakeTs = mktime(&wake);
  time_t nowTs = mktime(&t);
  if (wakeTs <= nowTs) {
    wakeTs += 24 * 3600;
  }
  return (uint64_t)(wakeTs - nowTs) * 1000000ULL;
}

void deepSleep(uint64_t sleepUs) {
  Serial.printf("Sleeping %llu seconds\n", sleepUs / 1000000ULL);
  Serial.flush();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_sleep_enable_timer_wakeup(sleepUs);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TIMER) {
    Serial.printf("Cold boot: holding %d ms for reflash window...\n",
                  COLD_BOOT_HOLDOFF_MS);
    Serial.flush();
    delay(COLD_BOOT_HOLDOFF_MS);
  }

  Serial.println("Initializing display...");

  DisplayManager display;
  if (!display.initialize()) {
    Serial.println("Display initialization failed!");
    deepSleep((uint64_t)WIFI_RETRY_SLEEP_MS * 1000ULL);
  }

  if (!connectToWiFi()) {
    Serial.println("Cannot proceed without WiFi; sleeping before retry.");
    deepSleep((uint64_t)WIFI_RETRY_SLEEP_MS * 1000ULL);
  }

  configTime(3600, 3600, "pool.ntp.org");
  Serial.println("Waiting for NTP time sync...");
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10000)) {
    Serial.println("Time synced.");
  } else {
    Serial.println("NTP sync failed, commute times may be wrong.");
  }

  Serial.println("Fetching data...");
  WeatherService weatherService;
  CommuteService commuteService;
  std::vector<WeatherData> forecast = weatherService.fetchForecast();
  std::vector<CommuteRoute> routes = commuteService.fetchRoutes();
  Serial.printf("Fetched %u forecasts, %u routes\n",
                (unsigned)forecast.size(), (unsigned)routes.size());

  display.clear();

  const int halfWidth = EPD_WIDTH / 2;
  WeatherRenderer weatherUI(display.getFramebuffer(), 0, 0, halfWidth,
                            EPD_HEIGHT);
  weatherUI.draw(forecast);

  CommuteRenderer commuteUI(display.getFramebuffer(), halfWidth, 0, halfWidth,
                            EPD_HEIGHT);
  commuteUI.draw(routes);

  display.refresh();

  deepSleep(computeSleepMicros());
}

void loop() {}
