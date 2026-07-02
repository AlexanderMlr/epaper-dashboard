#pragma once

#include <ArduinoJson.h>
#include <Arduino.h>

#include <vector>

#include "next_day_data.h"
#include "sun_data.h"
#include "weather_data.h"

struct WeatherForecast {
  std::vector<WeatherData> entries;
  SunData sun;
  NextDayData nextDay;
};

class WeatherService {
 private:
  String buildRequestUrl() const;

 public:
  WeatherForecast fetch();
};
