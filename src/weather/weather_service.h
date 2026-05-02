#pragma once

#include <ArduinoJson.h>
#include <Arduino.h>

#include <vector>

#include "sun_data.h"
#include "weather_data.h"

struct WeatherForecast {
  std::vector<WeatherData> entries;
  SunData sun;
};

class WeatherService {
 private:
  String buildRequestUrl() const;

 public:
  WeatherForecast fetch();
};
