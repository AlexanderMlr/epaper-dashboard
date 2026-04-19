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
constexpr const char *COMMUTE_START_NAME = "";
constexpr const char *COMMUTE_DEST_NAME = "";
const int COMMUTE_NUM_ROUTES = 3;

// Timing
const int UPDATE_INTERVAL_MS = 20000;

// Display
const int DISPLAY_WIDTH = EPD_WIDTH;
const int DISPLAY_HEIGHT = EPD_HEIGHT;
const int FRAMEBUFFER_SIZE = DISPLAY_WIDTH * DISPLAY_HEIGHT / 2;
const int ICON_SIZE = 64;
const int TEXT_LINE_DISTANCE = 40;

}  // namespace Config

using namespace Config;
