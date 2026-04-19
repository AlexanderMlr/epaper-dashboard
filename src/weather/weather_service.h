#pragma once

#include <ArduinoJson.h>
#include <Arduino.h>

#include <vector>

#include "weather_data.h"

class WeatherService {
 private:
  String buildRequestUrl() const;
  WeatherData parseWeatherEntry(const JsonObject& entry) const;

 public:
  std::vector<WeatherData> fetchForecast();
};