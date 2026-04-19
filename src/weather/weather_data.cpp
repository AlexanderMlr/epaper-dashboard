#include "weather_data.h"

#include <Arduino.h>

WeatherData::WeatherData()
    : datetime(0),
      absolute_temperature(0.0f),
      felt_temperature(0.0f),
      rain_probability(0.0f) {}

bool WeatherData::isValid() const { return !datetime_str.isEmpty(); }

String WeatherData::getFormattedTime() const {
  int spaceIndex = datetime_str.indexOf(' ');
  if (spaceIndex != -1 && spaceIndex + 6 < datetime_str.length()) {
    return datetime_str.substring(spaceIndex + 1, spaceIndex + 6);
  }
  return "99:99";
}

String WeatherData::getTemperatureString() const {
  return String((int)(absolute_temperature + 0.5f)) + "°C";
}