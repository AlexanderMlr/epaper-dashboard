#pragma once

#include <Arduino.h>

#include <vector>

#include "weather_data.h"

class WeatherRenderer {
 public:
  WeatherRenderer(uint8_t* framebuffer, int originX, int originY, int width,
                  int height);
  void draw(const std::vector<WeatherData>& forecast);

 private:
  uint8_t* framebuffer_;
  int originX_;
  int originY_;
  int width_;
  int height_;

  void drawWeatherIcon(const String& condition, int x, int y);
  void drawHeader();
  void drawCurrentWeather(const WeatherData& weather);
  void drawForecastGrid(const std::vector<WeatherData>& forecast);
};
