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
#include "firasans.h"
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

float readBatteryVoltage() {
  epd_poweron();
  delay(10);
  uint16_t raw = analogRead(BATT_PIN);
  epd_poweroff_all();
  return ((float)raw / 4095.0f) * 2.0f * 3.3f * (1100.0f / 1000.0f);
}

int batteryPercent(float voltage) {
  const float vMin = 3.3f;
  const float vMax = 4.2f;
  float pct = (voltage - vMin) / (vMax - vMin) * 100.0f;
  return (int)constrain(pct, 0.0f, 100.0f);
}

bool isInQuietHours(const struct tm& t) {
  return (QUIET_HOURS_START < QUIET_HOURS_END)
      ? (t.tm_hour >= QUIET_HOURS_START && t.tm_hour < QUIET_HOURS_END)
      : (t.tm_hour >= QUIET_HOURS_START || t.tm_hour < QUIET_HOURS_END);
}

uint64_t computeSleepMicros(bool haveTime, const struct tm& t) {
  if (!haveTime) {
    return (uint64_t)UPDATE_INTERVAL_MIN * 60ULL * 1000000ULL;
  }
  const int intervalMin =
      isInQuietHours(t) ? QUIET_UPDATE_INTERVAL_MIN : UPDATE_INTERVAL_MIN;
  return (uint64_t)intervalMin * 60ULL * 1000000ULL;
}

void drawFooter(uint8_t* framebuffer, const struct tm& now, uint64_t sleepUs,
                float batteryVoltage) {
  struct tm next = now;
  next.tm_sec += (int)(sleepUs / 1000000ULL);
  mktime(&next);

  char footer[96];
  snprintf(footer, sizeof(footer),
           "Updated %02d:%02d  |  Next update %02d:%02d  |  %d%% (%.2fV)",
           now.tm_hour, now.tm_min, next.tm_hour, next.tm_min,
           batteryPercent(batteryVoltage), batteryVoltage);

  int32_t x = 20;
  int32_t y = EPD_HEIGHT - 20;
  write_string((GFXfont*)&FiraSans, footer, &x, &y, framebuffer);
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
    Serial.printf("Cold boot: holding %d s for reflash window...\n",
                  COLD_BOOT_HOLDOFF_SEC);
    Serial.flush();
    delay((uint32_t)COLD_BOOT_HOLDOFF_SEC * 1000UL);
  }

  Serial.println("Initializing display...");

  DisplayManager display;
  if (!display.initialize()) {
    Serial.println("Display initialization failed!");
    deepSleep((uint64_t)WIFI_RETRY_SLEEP_SEC * 1000000ULL);
  }

  if (!connectToWiFi()) {
    Serial.println("Cannot proceed without WiFi; sleeping before retry.");
    deepSleep((uint64_t)WIFI_RETRY_SLEEP_SEC * 1000000ULL);
  }

  configTime(3600, 3600, "pool.ntp.org");
  Serial.println("Waiting for NTP time sync...");
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10000)) {
    Serial.println("Time synced.");
  } else {
    Serial.println("NTP sync failed, commute times may be wrong.");
  }

  struct tm nowT{};
  bool haveTime = getLocalTime(&nowT, 1000);
  bool inQuiet = haveTime && isInQuietHours(nowT);
  uint64_t sleepUs = computeSleepMicros(haveTime, nowT);

  Serial.println("Fetching data...");
  WeatherService weatherService;
  std::vector<WeatherData> forecast = weatherService.fetchForecast();
  std::vector<CommuteRoute> routes;
  if (!inQuiet) {
    CommuteService commuteService;
    routes = commuteService.fetchRoutes();
  }
  Serial.printf("Fetched %u forecasts, %u routes\n",
                (unsigned)forecast.size(), (unsigned)routes.size());

  display.clear();

  const int halfWidth = EPD_WIDTH / 2;
  WeatherRenderer weatherUI(display.getFramebuffer(), 0, 0, halfWidth,
                            EPD_HEIGHT);
  weatherUI.draw(forecast);

  CommuteRenderer commuteUI(display.getFramebuffer(), halfWidth, 0, halfWidth,
                            EPD_HEIGHT);
  commuteUI.draw(routes, inQuiet);

  float batteryVoltage = readBatteryVoltage();
  Serial.printf("Battery: %.2fV (%d%%)\n", batteryVoltage,
                batteryPercent(batteryVoltage));

  if (haveTime) {
    drawFooter(display.getFramebuffer(), nowT, sleepUs, batteryVoltage);
  }

  display.refresh();

  deepSleep(sleepUs);
}

void loop() {}
