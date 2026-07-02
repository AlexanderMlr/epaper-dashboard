#pragma once

#include <Arduino.h>

#include <optional>

#include "weather_condition.h"

struct WeatherData {
  int datetime;
  String datetime_str;
  WeatherCondition condition;
  String description;
  float absolute_temperature;
  float felt_temperature;
  float rain_probability;

  WeatherData();
  bool isValid() const;
  std::optional<String> getFormattedTime() const;
  String getTemperatureString() const;
};
