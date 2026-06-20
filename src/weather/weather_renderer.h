#pragma once

#include <Arduino.h>

#include <vector>

#include "sun_data.h"
#include "weather_data.h"

class WeatherRenderer {
 public:
  WeatherRenderer(uint8_t* framebuffer, int originX, int originY, int width,
                  int height);
  void draw(const std::vector<WeatherData>& forecast, bool showCommute,
            const SunData& sun);

 private:
  uint8_t* framebuffer_;
  int originX_;
  int originY_;
  int width_;
  int height_;

  void drawWeatherIcon(WeatherCondition condition, int x, int y, bool isNight);
  void drawHeader();
  void drawCurrentWeather(const WeatherData& weather, const SunData& sun);
  void drawForecastGrid(const std::vector<WeatherData>& forecast,
                        const SunData& sun);
  void drawCommuteRecommendation(const std::vector<WeatherData>& forecast);
  void drawSunInfo(const SunData& sun, bool isNight);
};
