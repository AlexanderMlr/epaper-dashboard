#pragma once

#include <Arduino.h>

enum class WeatherCondition {
  Unknown,
  Clear,
  PartlyCloudy,
  Clouds,
  Fog,
  LightRain,
  Rain,
  Snow,
  Hail,
  Thunderstorm,
};

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
  String getFormattedTime() const;
  String getTemperatureString() const;
};
