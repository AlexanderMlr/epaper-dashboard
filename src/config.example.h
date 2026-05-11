#pragma once

namespace Config {

// WiFi
constexpr const char *WIFI_SSID = "";
constexpr const char *WIFI_PASSWORD = "";

// Weather (Open-Meteo)
constexpr const char *LOCATION_LATITUDE = "";
constexpr const char *LOCATION_LONGITUDE = "";

// Bike-vs-train thresholds
const float BIKE_MIN_TEMP_C = 10.0f;
const float BIKE_MAX_RAIN_PCT = 50.0f;

// Commute (bahn.expert)
constexpr const char *COMMUTE_START_EVA = "";
constexpr const char *COMMUTE_DEST_EVA = "";
const int COMMUTE_DEPARTURE_OFFSET_MIN = 10;  // query journeys departing now + offset
const int COMMUTE_HOURS_START = 7;            // local hour [0-23], inclusive
const int COMMUTE_HOURS_END = 9;              // local hour [0-23], exclusive

// Calendar (Google Calendar private ICS link)
// get private link in iCal format from your google calendar settings
constexpr const char *CALENDAR_ICS_URL = "";
const int CALENDAR_LOOKAHEAD_DAYS = 14;

// Timing
// POSIX TZ string with DST rules. Default is Central European Time (Germany).
constexpr const char *TIME_ZONE = "CET-1CEST,M3.5.0,M10.5.0/3";
const int COMMUTE_UPDATE_INTERVAL_MIN = 10;    // commute-hours refresh interval
const int OFF_HOURS_UPDATE_INTERVAL_MIN = 120; // refresh interval outside commute hours
const int COLD_BOOT_HOLDOFF_SEC = 20;  // reflash window on power-on / RST; skipped on timer wake

// Battery
// factor to align board voltage reading with ground truth measurement for higher battery accuracy
const float BATTERY_VOLTAGE_CALIBRATION = 1.0f; // measured / displayed

}  // namespace Config

using namespace Config;
