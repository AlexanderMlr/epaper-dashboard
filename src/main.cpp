#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, Arduino IDE -> tools -> PSRAM -> OPI !!!"
#endif

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <sys/time.h>
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
RTC_DATA_ATTR time_t lastNtpSyncEpoch = 0;
RTC_DATA_ATTR time_t epochBeforeSleep = 0;
RTC_DATA_ATTR uint64_t plannedSleepUs = 0;

namespace {
const int kWifiRetrySleepSec = 30;  // deep-sleep duration after WiFi failure
const int kNtpMinResyncHours = 20;  // earliest re-sync, and only if in window
const int kNtpMaxResyncHours = 26;  // forced re-sync regardless of hour
const int kNtpSyncAttempts = 3;
const uint32_t kNtpAttemptTimeoutMs = 5000;
const time_t kMinValidEpoch = 1577836800;  // 2020-01-01; below = clock unset
}

bool clockIsValid() { return time(nullptr) >= kMinValidEpoch; }

bool shouldSyncNtp(time_t now, time_t lastSync) {
  if (lastSync == 0) return true;
  const time_t age = now - lastSync;
  if (age < kNtpMinResyncHours * 3600) return false;
  if (age > kNtpMaxResyncHours * 3600) return true;
  struct tm t;
  localtime_r(&now, &t);
  return t.tm_hour >= 2 && t.tm_hour < 5;
}

bool syncNtp() {
  struct tm timeinfo;
  for (int attempt = 0; attempt < kNtpSyncAttempts; attempt++) {
    if (attempt > 0) {
      Serial.printf("NTP retry %d/%d...\n", attempt, kNtpSyncAttempts - 1);
      configTzTime(TIME_ZONE, "pool.ntp.org");  // re-arm SNTP for a fresh request
    }
    if (getLocalTime(&timeinfo, kNtpAttemptTimeoutMs)) return true;
  }
  return false;
}

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
  epochBeforeSleep = clockIsValid() ? time(nullptr) : 0;
  plannedSleepUs = sleepUs;
  Serial.printf("Sleeping %llu seconds\n", sleepUs / 1000000ULL);
  Serial.flush();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  sleepTouchController();
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

  // Set TZ and restore wall clock from RTC cache before anything else, so
  // failure paths (display/WiFi) still preserve the time-cache continuity.
  configTzTime(TIME_ZONE, "pool.ntp.org");
  if (epochBeforeSleep >= kMinValidEpoch) {
    struct timeval tv;
    tv.tv_sec = epochBeforeSleep + (time_t)(plannedSleepUs / 1000000ULL);
    tv.tv_usec = 0;
    settimeofday(&tv, nullptr);
  }

  Serial.println("Initializing display...");

  DisplayManager display;
  if (!display.initialize()) {
    Serial.println("Display initialization failed!");
    deepSleep((uint64_t)kWifiRetrySleepSec * 1000000ULL);
  }

  if (!connectToWiFi()) {
    Serial.println("Cannot proceed without WiFi; sleeping before retry.");
    deepSleep((uint64_t)kWifiRetrySleepSec * 1000000ULL);
  }

  const time_t now = time(nullptr);
  if (!clockIsValid() || shouldSyncNtp(now, lastNtpSyncEpoch)) {
    Serial.println("Syncing NTP...");
    if (syncNtp()) {
      lastNtpSyncEpoch = time(nullptr);
      Serial.println("Time synced.");
    } else {
      Serial.println("NTP sync failed, using cached/drifted time.");
    }
  } else {
    struct tm localNow;
    localtime_r(&now, &localNow);
    Serial.printf("Skipping NTP (age=%lus, hour=%d)\n",
                  (unsigned long)(now - lastNtpSyncEpoch), localNow.tm_hour);
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

  // WiFi off before reading battery — TX sag would skew the voltage low.
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  display.clear();

  const int halfWidth = EPD_WIDTH / 2;
  WeatherRenderer weatherUI(display.getFramebuffer(), 0, 0, halfWidth,
                            EPD_HEIGHT);
  weatherUI.draw(weather.entries, inCommute, weather.sun, weather.nextDay);

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
