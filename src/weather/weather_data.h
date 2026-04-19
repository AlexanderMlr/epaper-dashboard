#pragma once

#include <Arduino.h>

struct WeatherData {
  int datetime;
  String datetime_str;
  String characterization;
  String description;
  float absolute_temperature;
  float felt_temperature;
  float rain_probability;

  WeatherData();
  bool isValid() const;
  String getFormattedTime() const;
  String getTemperatureString() const;
};
