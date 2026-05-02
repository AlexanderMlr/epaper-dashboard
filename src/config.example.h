#pragma once

#include "epd_driver.h"

namespace Config {

// WiFi
constexpr const char *WIFI_SSID = "";
constexpr const char *WIFI_PASSWORD = "";

// Weather (Open-Meteo)
constexpr const char *LOCATION_LATITUDE = "";
constexpr const char *LOCATION_LONGITUDE = "";
const int NUM_FORECASTS = 4;

// Commute (bahn.expert)
constexpr const char *COMMUTE_START_EVA = "";
constexpr const char *COMMUTE_DEST_EVA = "";
const int COMMUTE_NUM_ROUTES = 5;
const int COMMUTE_DEPARTURE_OFFSET_MIN = 10;  // query journeys departing now + offset
const int COMMUTE_HOURS_START = 7;            // local hour [0-23], inclusive
const int COMMUTE_HOURS_END = 9;              // local hour [0-23], exclusive

// Calendar (Google Calendar private ICS link)
// get private link in iCal format from your google calendar settings
constexpr const char *CALENDAR_ICS_URL = "";
const int CALENDAR_NUM_EVENTS = 6;
const int CALENDAR_LOOKAHEAD_DAYS = 14;

// Timing
const int COMMUTE_UPDATE_INTERVAL_MIN = 10;    // commute-hours refresh interval
const int OFF_HOURS_UPDATE_INTERVAL_MIN = 120; // refresh interval outside commute hours
const int WIFI_RETRY_SLEEP_SEC = 30;   // deep-sleep duration after WiFi failure
const int COLD_BOOT_HOLDOFF_SEC = 20;  // reflash window on power-on / RST; skipped on timer wake

// Battery
// factor to align board voltage reading with ground truth measurement
const float BATTERY_VOLTAGE_CALIBRATION = 1.1f;

// Display
const int DISPLAY_WIDTH = EPD_WIDTH;
const int DISPLAY_HEIGHT = EPD_HEIGHT;
const int FRAMEBUFFER_SIZE = DISPLAY_WIDTH * DISPLAY_HEIGHT / 2;
const int ICON_SIZE = 64;
const int TEXT_LINE_DISTANCE = 40;

}  // namespace Config

using namespace Config;
