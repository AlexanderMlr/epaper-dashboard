#include "weather_renderer.h"

#include "../config.h"
#include "../display_manager.h"
#include "firasans.h"
#include "weather_icons.h"

WeatherRenderer::WeatherRenderer(uint8_t* framebuffer, int originX,
                                 int originY, int width, int height)
    : framebuffer_(framebuffer),
      originX_(originX),
      originY_(originY),
      width_(width),
      height_(height) {}

void WeatherRenderer::drawWeatherIcon(const String& condition, int x, int y) {
  const uint8_t* icon = WeatherIcons::CLOUD_ICON;

  if (condition.equalsIgnoreCase("Clear") ||
      condition.equalsIgnoreCase("Sunny")) {
    icon = WeatherIcons::SUN_ICON;
  } else if (condition.equalsIgnoreCase("Clouds") ||
             condition.equalsIgnoreCase("Cloudy")) {
    icon = WeatherIcons::CLOUD_ICON;
  } else if (condition.equalsIgnoreCase("Rain") ||
             condition.equalsIgnoreCase("Drizzle")) {
    icon = WeatherIcons::RAIN_ICON;
  }

  drawGrayscaleBitmap(framebuffer_, icon, originX_ + x, originY_ + y, 64, 64);
}

void WeatherRenderer::drawHeader() {
  int32_t x = originX_ + 20;
  int32_t y = originY_ + 50;
  write_string((GFXfont*)&FiraSans, "Weather Forecast", &x, &y, framebuffer_);
}

void WeatherRenderer::drawCurrentWeather(const WeatherData& weather) {
  if (!weather.isValid()) return;

  const int iconX = 20;
  const int iconY = 110;
  drawWeatherIcon(weather.characterization, iconX, iconY);

  int32_t x = originX_ + iconX + ICON_SIZE;
  int32_t y = originY_ + iconY;
  write_string((GFXfont*)&FiraSans, weather.description.c_str(), &x, &y,
               framebuffer_);

  x = originX_ + iconX + ICON_SIZE;
  y = originY_ + iconY + TEXT_LINE_DISTANCE;
  String tempStr = weather.getTemperatureString() + " (felt as " +
                   String((int)(weather.felt_temperature + 0.5f)) + "°C)";
  write_string((GFXfont*)&FiraSans, tempStr.c_str(), &x, &y, framebuffer_);

  x = originX_ + iconX + ICON_SIZE;
  y = originY_ + iconY + 2 * TEXT_LINE_DISTANCE;
  String rainStr =
      "Rain: " + String((int)(weather.rain_probability)) + "%";
  write_string((GFXfont*)&FiraSans, rainStr.c_str(), &x, &y, framebuffer_);
}

void WeatherRenderer::drawForecastGrid(
    const std::vector<WeatherData>& forecast) {
  const int startY = 200;
  const int itemWidth = width_ / (forecast.size() - 1);

  for (size_t i = 1; i < forecast.size() && i < 4; i++) {
    const WeatherData& weather = forecast[i];
    if (!weather.isValid()) continue;

    const int x = (int)(i - 1) * itemWidth + 20;

    drawWeatherIcon(weather.characterization, x, startY);

    int32_t textX = originX_ + x;
    int32_t textY = originY_ + startY + ICON_SIZE + TEXT_LINE_DISTANCE;
    write_string((GFXfont*)&FiraSans, weather.getFormattedTime().c_str(),
                 &textX, &textY, framebuffer_);

    textX = originX_ + x;
    textY = originY_ + startY + ICON_SIZE + 2 * TEXT_LINE_DISTANCE;
    write_string((GFXfont*)&FiraSans, weather.getTemperatureString().c_str(),
                 &textX, &textY, framebuffer_);
  }
}

void WeatherRenderer::drawCommuteRecommendation(
    const std::vector<WeatherData>& forecast) {
  bool bikeable = true;
  for (size_t i = 0; i < forecast.size() && i < 4; i++) {
    const WeatherData& w = forecast[i];
    if (!w.isValid()) continue;
    if (w.absolute_temperature < 10.0f || w.rain_probability >= 20.0f) {
      bikeable = false;
      break;
    }
  }

  const char* recommendation = bikeable
                                   ? "A good day for biking!"
                                   : "Better take the train.";

  int32_t x = originX_ + 20;
  int32_t y = originY_ + 200 + ICON_SIZE + 3 * TEXT_LINE_DISTANCE + 20;
  write_string((GFXfont*)&FiraSans, recommendation, &x, &y, framebuffer_);
}

void WeatherRenderer::drawSunInfo(const SunData& sun) {
  if (!sun.isValid()) return;

  int32_t x = originX_ + 20;
  int32_t y = originY_ + 200 + ICON_SIZE + 3 * TEXT_LINE_DISTANCE + 20;
  String line = "Sun: " + sun.sunrise + " - " + sun.sunset;
  if (sun.uvIndexMax >= 0.0f) {
    line += "  |  UV " + String((int)(sun.uvIndexMax + 0.5f));
  }
  write_string((GFXfont*)&FiraSans, line.c_str(), &x, &y, framebuffer_);
}

void WeatherRenderer::draw(const std::vector<WeatherData>& forecast,
                           bool showCommute, const SunData& sun) {
  if (forecast.empty()) {
    int32_t x = originX_ + 20;
    int32_t y = originY_ + 100;
    write_string((GFXfont*)&FiraSans, "No weather data available", &x, &y,
                 framebuffer_);
    return;
  }

  drawHeader();
  drawCurrentWeather(forecast[0]);

  if (forecast.size() > 1) {
    drawForecastGrid(forecast);
  }

  if (showCommute) {
    drawCommuteRecommendation(forecast);
  } else {
    drawSunInfo(sun);
  }
}
