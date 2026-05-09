#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, Arduino IDE -> tools -> PSRAM -> OPI !!!"
#endif

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <time.h>

#include "calendar/calendar_renderer.h"
#include "calendar/calendar_service.h"
#include "commute/commute_renderer.h"
#include "commute/commute_service.h"
#include "config.h"
#include "display_manager.h"
#include "epd_driver.h"
#include "footer_renderer.h"
#include "power.h"
#include "utilities.h"
#include "weather/weather_renderer.h"
#include "weather/weather_service.h"

RTC_DATA_ATTR uint8_t cachedBssid[6] = {0};
RTC_DATA_ATTR int32_t cachedChannel = 0;
RTC_DATA_ATTR bool cachedWifiValid = false;

bool waitForConnect(uint32_t timeoutMs) {
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(100);
    Serial.print(".");
  }
  return WiFi.status() == WL_CONNECTED;
}

bool connectToWiFi() {
  Serial.print("Connecting to WiFi");

  if (cachedWifiValid) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD, cachedChannel, cachedBssid);
    if (waitForConnect(4000)) {
      Serial.println("\nWiFi connected (fast path)!");
      Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
      return true;
    }
    Serial.println("\nFast path failed, falling back to full scan");
    WiFi.disconnect(true);
    cachedWifiValid = false;
  }

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  if (waitForConnect(10000)) {
    Serial.println("\nWiFi connected!");
    Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
    memcpy(cachedBssid, WiFi.BSSID(), 6);
    cachedChannel = WiFi.channel();
    cachedWifiValid = true;
    return true;
  }

  Serial.println("\nWiFi connection failed!");
  return false;
}

bool isInCommuteHours(const struct tm& t) {
  const bool isWeekday = (t.tm_wday >= 1 && t.tm_wday <= 5);
  if (!isWeekday) return false;
  return (COMMUTE_HOURS_START < COMMUTE_HOURS_END)
      ? (t.tm_hour >= COMMUTE_HOURS_START && t.tm_hour < COMMUTE_HOURS_END)
      : (t.tm_hour >= COMMUTE_HOURS_START || t.tm_hour < COMMUTE_HOURS_END);
}

uint64_t computeSleepMicros(bool haveTime, const struct tm& t) {
  if (!haveTime) {
    return (uint64_t)COMMUTE_UPDATE_INTERVAL_MIN * 60ULL * 1000000ULL;
  }
  const int intervalMin = isInCommuteHours(t) ? COMMUTE_UPDATE_INTERVAL_MIN
                                              : OFF_HOURS_UPDATE_INTERVAL_MIN;
  return (uint64_t)intervalMin * 60ULL * 1000000ULL;
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

  if (esp_reset_reason() != ESP_RST_DEEPSLEEP) {
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
  bool inCommute = haveTime && isInCommuteHours(nowT);
  uint64_t sleepUs = computeSleepMicros(haveTime, nowT);

  Serial.println("Fetching data...");
  WeatherService weatherService;
  WeatherForecast weather = weatherService.fetch();
  std::vector<CommuteRoute> routes;
  std::vector<CalendarEvent> events;
  if (inCommute) {
    CommuteService commuteService;
    routes = commuteService.fetchRoutes();
  } else {
    CalendarService calendarService;
    events = calendarService.fetchEvents();
  }
  Serial.printf("Fetched %u forecasts, %u routes, %u events\n",
                (unsigned)weather.entries.size(), (unsigned)routes.size(),
                (unsigned)events.size());

  display.clear();

  const int halfWidth = EPD_WIDTH / 2;
  WeatherRenderer weatherUI(display.getFramebuffer(), 0, 0, halfWidth,
                            EPD_HEIGHT);
  weatherUI.draw(weather.entries, inCommute, weather.sun);

  if (inCommute) {
    CommuteRenderer commuteUI(display.getFramebuffer(), halfWidth, 0,
                              halfWidth, EPD_HEIGHT);
    commuteUI.draw(routes, false);
  } else {
    CalendarRenderer calendarUI(display.getFramebuffer(), halfWidth, 0,
                                halfWidth, EPD_HEIGHT);
    calendarUI.draw(events);
  }

  float batteryVoltage = readBatteryVoltage();
  Serial.printf("Battery: %.2fV (%d%%)\n", batteryVoltage,
                batteryPercent(batteryVoltage));

  if (haveTime) {
    FooterRenderer footer(display.getFramebuffer());
    footer.draw(nowT, sleepUs, batteryVoltage, batteryPercent(batteryVoltage));
  }

  display.refresh();

  deepSleep(sleepUs);
}

void loop() {}
