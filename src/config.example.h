#pragma once

#include "epd_driver.h"

namespace Config {

// WiFi
constexpr const char *WIFI_SSID = "";
constexpr const char *WIFI_PASSWORD = "";

// Weather (OpenWeatherMap)
constexpr const char *WEATHER_API_KEY = "";
constexpr const char *LOCATION_LATITUDE = "";
constexpr const char *LOCATION_LONGITUDE = "";
const int NUM_FORECASTS = 4;

// Commute (bahn.expert)
constexpr const char *COMMUTE_START_EVA = "";
constexpr const char *COMMUTE_DEST_EVA = "";
const int COMMUTE_NUM_ROUTES = 5;
const int COMMUTE_DEPARTURE_OFFSET_MIN = 10;  // query journeys departing now + offset

// Timing
const int UPDATE_INTERVAL_MIN = 5;             // active-hours refresh interval
const int QUIET_UPDATE_INTERVAL_MIN = 120;     // quiet-hours refresh interval
const int QUIET_HOURS_START = 23;              // local hour [0-23], inclusive
const int QUIET_HOURS_END = 6;                 // local hour [0-23], exclusive
const int WIFI_RETRY_SLEEP_SEC = 30;   // deep-sleep duration after WiFi failure
const int COLD_BOOT_HOLDOFF_SEC = 30;  // reflash window on power-on / RST; skipped on timer wake

// Display
const int DISPLAY_WIDTH = EPD_WIDTH;
const int DISPLAY_HEIGHT = EPD_HEIGHT;
const int FRAMEBUFFER_SIZE = DISPLAY_WIDTH * DISPLAY_HEIGHT / 2;
const int ICON_SIZE = 64;
const int TEXT_LINE_DISTANCE = 40;

}  // namespace Config

using namespace Config;
