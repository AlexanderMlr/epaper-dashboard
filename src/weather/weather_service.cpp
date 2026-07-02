#include "weather_service.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <time.h>

#include "../config.h"

namespace {

const int NUM_FORECASTS = 4;

// WMO weather code -> (condition enum, short description).
struct CodeMap {
  WeatherCondition condition;
  const char* description;
};

CodeMap mapWeatherCode(int code) {
  if (code == 0) return {WeatherCondition::Clear, "clear sky"};
  if (code == 1) return {WeatherCondition::PartlyCloudy, "mostly clear"};
  if (code == 2) return {WeatherCondition::PartlyCloudy, "partly cloudy"};
  if (code == 3) return {WeatherCondition::Clouds, "overcast"};
  if (code == 45 || code == 48) return {WeatherCondition::Fog, "fog"};
  if (code >= 51 && code <= 57) return {WeatherCondition::LightRain, "drizzle"};
  if (code == 61 || code == 80)
    return {WeatherCondition::LightRain, "light rain"};
  if ((code >= 63 && code <= 67) || code == 81 || code == 82)
    return {WeatherCondition::Rain, "rain"};
  if ((code >= 71 && code <= 77) || code == 85 || code == 86)
    return {WeatherCondition::Snow, "snow"};
  if (code == 96 || code == 99)
    return {WeatherCondition::Hail, "thunderstorm with hail"};
  if (code >= 95) return {WeatherCondition::Thunderstorm, "thunderstorm"};
  return {WeatherCondition::Unknown, "unknown"};
}

String isoToDisplay(const String& iso) {
  // "2026-05-03T14:00" -> "2026-05-03 14:00:00"
  String s = iso;
  int t = s.indexOf('T');
  if (t >= 0) s.setCharAt(t, ' ');
  if (s.length() == 16) s += ":00";
  return s;
}

String extractClockTime(const String& iso) {
  int t = iso.indexOf('T');
  if (t < 0 || t + 6 > (int)iso.length()) return String();
  return iso.substring(t + 1, t + 6);
}

int findStartIndex(JsonArray timeArr) {
  time_t now = time(nullptr);
  struct tm lt;
  localtime_r(&now, &lt);
  char prefix[14];
  // Match "YYYY-MM-DDTHH" against ISO entries.
  strftime(prefix, sizeof(prefix), "%Y-%m-%dT%H", &lt);
  for (size_t i = 0; i < timeArr.size(); i++) {
    String s = timeArr[i].as<String>();
    if (s.startsWith(prefix)) return (int)i;
  }
  return 0;
}

}  // namespace

String WeatherService::buildRequestUrl() const {
  return String("https://api.open-meteo.com/v1/forecast?") +
         "latitude=" + LOCATION_LATITUDE +
         "&longitude=" + LOCATION_LONGITUDE +
         "&hourly=temperature_2m,apparent_temperature,precipitation_probability,"
         "weathercode" +
         "&daily=uv_index_max,sunrise,sunset,weathercode,temperature_2m_max,"
         "precipitation_probability_max" +
         "&timezone=auto&forecast_days=2";
}

WeatherForecast WeatherService::fetch() {
  WeatherForecast result;

  HTTPClient http;
  http.begin(buildRequestUrl());
  http.setTimeout(10000);

  const int httpCode = http.GET();
  Serial.printf("Weather API response code: %d\n", httpCode);

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("HTTP request failed with code: %d\n", httpCode);
    http.end();
    return result;
  }

  const String payload = http.getString();
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  http.end();

  if (err != DeserializationError::Ok) {
    Serial.printf("JSON parsing failed: %s\n", err.c_str());
    return result;
  }

  JsonObject hourly = doc["hourly"].as<JsonObject>();
  JsonArray times = hourly["time"].as<JsonArray>();
  JsonArray temps = hourly["temperature_2m"].as<JsonArray>();
  JsonArray feels = hourly["apparent_temperature"].as<JsonArray>();
  JsonArray pops = hourly["precipitation_probability"].as<JsonArray>();
  JsonArray codes = hourly["weathercode"].as<JsonArray>();

  result.entries.reserve(NUM_FORECASTS);
  const int start = findStartIndex(times);
  for (int n = 0; n < NUM_FORECASTS; n++) {
    const int idx = start + n * 3;
    if (idx >= (int)times.size()) break;
    WeatherData w;
    String iso = times[idx].as<String>();
    w.datetime_str = isoToDisplay(iso);
    w.absolute_temperature = temps[idx].as<float>();
    w.felt_temperature = feels[idx].as<float>();
    w.rain_probability = pops[idx].as<float>();
    CodeMap m = mapWeatherCode(codes[idx].as<int>());
    w.condition = m.condition;
    w.description = m.description;
    result.entries.push_back(w);
  }

  JsonObject daily = doc["daily"].as<JsonObject>();
  result.sun.uvIndexMax = daily["uv_index_max"][0].as<float>();
  result.sun.sunrise = extractClockTime(daily["sunrise"][0].as<String>());
  result.sun.sunset = extractClockTime(daily["sunset"][0].as<String>());

  JsonArray dailyMax = daily["temperature_2m_max"].as<JsonArray>();
  if (dailyMax.size() >= 2) {
    result.nextDay.condition =
        mapWeatherCode(daily["weathercode"][1].as<int>()).condition;
    result.nextDay.precipProbMax =
        daily["precipitation_probability_max"][1].as<float>();
    result.nextDay.tempDeltaC = dailyMax[1].as<float>() - dailyMax[0].as<float>();
    result.nextDay.valid = true;
  }

  return result;
}
