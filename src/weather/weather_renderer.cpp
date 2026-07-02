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

namespace {

// Layout (all relative to renderer origin)
const int MARGIN_X = 20;             // left padding for text and icons
const int HEADER_Y = 50;             // baseline of the "Weather Forecast" title
const int CURRENT_WEATHER_Y = 100;   // baseline of the first current-weather text row
const int FORECAST_Y = 210;          // top of the forecast grid
const int ICON_SIZE = 64;            // weather icons are 64x64
const int TEXT_LINE_DISTANCE = 40;   // vertical spacing for FiraSans body text
const int ICON_TEXT_GAP = 16;        // horizontal gap between an icon and its text
const int BOTTOM_LINE_GAP = 20;      // padding above the footer lines
const int BOTTOM_LINE_Y =            // baseline of the sun-info row (first footer line)
    FORECAST_Y + ICON_SIZE + 3 * TEXT_LINE_DISTANCE + BOTTOM_LINE_GAP;
const int SECOND_LINE_Y =            // baseline of the commute / next-day row
    BOTTOM_LINE_Y + TEXT_LINE_DISTANCE;

// Next-day footer threshold
const float RAIN_LIKELY_PCT = BIKE_MAX_RAIN_PCT;  // label tomorrow "rain" at/above
                                                  // this chance; tied to bike cutoff

// Forecast
const size_t MAX_FORECAST_ITEMS = 4; // entries shown in the grid (incl. current)

bool isNightTime(const WeatherData& w, const SunData& sun) {
  if (!sun.isValid()) return false;
  auto t = w.getFormattedTime();
  if (!t) return false;
  return *t < sun.sunrise || *t >= sun.sunset;
}

}  // namespace

void WeatherRenderer::drawWeatherIcon(WeatherCondition condition, int x, int y,
                                      bool isNight) {
  const uint8_t* icon = WeatherIcons::CLOUD_ICON;

  switch (condition) {
    case WeatherCondition::Clear:
      icon = isNight ? WeatherIcons::NIGHT_ICON : WeatherIcons::SUN_ICON;
      break;
    case WeatherCondition::PartlyCloudy:
      icon = isNight ? WeatherIcons::NIGHT_ICON
                     : WeatherIcons::SUN_WITH_CLOUD_ICON;
      break;
    case WeatherCondition::Clouds:
      icon = WeatherIcons::CLOUD_ICON;
      break;
    case WeatherCondition::Fog:
      icon = WeatherIcons::FOG_ICON;
      break;
    case WeatherCondition::LightRain:
      icon = WeatherIcons::LIGHT_RAIN_ICON;
      break;
    case WeatherCondition::Rain:
      icon = WeatherIcons::RAIN_ICON;
      break;
    case WeatherCondition::Snow:
      icon = WeatherIcons::SNOW_ICON;
      break;
    case WeatherCondition::Hail:
      icon = WeatherIcons::HAIL_ICON;
      break;
    case WeatherCondition::Thunderstorm:
      icon = WeatherIcons::RAIN_WITH_THUNDER_ICON;
      break;
    case WeatherCondition::Unknown:
      icon = WeatherIcons::CLOUD_ICON;
      break;
  }

  drawGrayscaleBitmap(framebuffer_, icon, originX_ + x, originY_ + y,
                      ICON_SIZE, ICON_SIZE);
}

void WeatherRenderer::drawHeader() {
  int32_t x = originX_ + MARGIN_X;
  int32_t y = originY_ + HEADER_Y;
  write_string((GFXfont*)&FiraSans, "Weather Forecast", &x, &y, framebuffer_);
}

void WeatherRenderer::drawCurrentWeather(const WeatherData& weather,
                                         const SunData& sun) {
  if (!weather.isValid()) return;

  drawWeatherIcon(weather.condition, MARGIN_X, CURRENT_WEATHER_Y - 8,
                  isNightTime(weather, sun));

  const int32_t textX = originX_ + MARGIN_X + ICON_SIZE + ICON_TEXT_GAP;

  int32_t x = textX;
  int32_t y = originY_ + CURRENT_WEATHER_Y;
  String description = weather.description;
  if (description.length() > 0) {
    description.setCharAt(0, toupper(description.charAt(0)));
  }
  write_string((GFXfont*)&FiraSans, description.c_str(), &x, &y, framebuffer_);

  x = textX;
  y = originY_ + CURRENT_WEATHER_Y + TEXT_LINE_DISTANCE;
  String tempStr = weather.getTemperatureString() + " (felt as " +
                   String((int)(weather.felt_temperature + 0.5f)) + "°C)";
  write_string((GFXfont*)&FiraSans, tempStr.c_str(), &x, &y, framebuffer_);

  x = textX;
  y = originY_ + CURRENT_WEATHER_Y + 2 * TEXT_LINE_DISTANCE;
  String rainStr =
      "Rain: " + String((int)(weather.rain_probability)) + "%";
  write_string((GFXfont*)&FiraSans, rainStr.c_str(), &x, &y, framebuffer_);
}

void WeatherRenderer::drawForecastGrid(
    const std::vector<WeatherData>& forecast, const SunData& sun) {
  if (forecast.size() < 2) return;
  const int itemWidth = width_ / (forecast.size() - 1);

  for (size_t i = 1; i < forecast.size() && i < MAX_FORECAST_ITEMS; i++) {
    const WeatherData& weather = forecast[i];
    if (!weather.isValid()) continue;

    const int x = static_cast<int>(i - 1) * itemWidth + MARGIN_X;

    drawWeatherIcon(weather.condition, x + ICON_TEXT_GAP, FORECAST_Y,
                    isNightTime(weather, sun));

    int32_t textX = originX_ + x;
    int32_t textY = originY_ + FORECAST_Y + ICON_SIZE + TEXT_LINE_DISTANCE;
    write_string((GFXfont*)&FiraSans, weather.getFormattedTime().value_or("").c_str(),
                 &textX, &textY, framebuffer_);

    textX = originX_ + x;
    textY = originY_ + FORECAST_Y + ICON_SIZE + 2 * TEXT_LINE_DISTANCE;
    write_string((GFXfont*)&FiraSans, weather.getTemperatureString().c_str(),
                 &textX, &textY, framebuffer_);
  }
}

void WeatherRenderer::drawCommuteRecommendation(
    const std::vector<WeatherData>& forecast) {
  bool bikeable = true;
  for (size_t i = 0; i < forecast.size() && i < MAX_FORECAST_ITEMS; i++) {
    const WeatherData& w = forecast[i];
    if (!w.isValid()) continue;
    if (w.absolute_temperature < BIKE_MIN_TEMP_C ||
        w.rain_probability >= BIKE_MAX_RAIN_PCT) {
      bikeable = false;
      break;
    }
  }

  const char* recommendation = bikeable
                                   ? "A good day for biking!"
                                   : "Better take the train.";

  int32_t x = originX_ + MARGIN_X;
  int32_t y = originY_ + SECOND_LINE_Y;
  write_string((GFXfont*)&FiraSans, recommendation, &x, &y, framebuffer_);
}

void WeatherRenderer::drawSunInfo(const SunData& sun, bool isNight) {
  if (!sun.isValid()) return;

  int32_t x = originX_ + MARGIN_X;
  int32_t y = originY_ + BOTTOM_LINE_Y;
  String line;
  if (isNight) {
    line = "Sunrise " + sun.sunrise;
  } else {
    line = "Sunset " + sun.sunset;
    if (sun.uvIndexMax >= 0.0f) {
      line += " | UV " + String((int)(sun.uvIndexMax + 0.5f));
    }
  }
  write_string((GFXfont*)&FiraSans, line.c_str(), &x, &y, framebuffer_);
}

void WeatherRenderer::drawNextDay(const NextDayData& nextDay) {
  if (!nextDay.valid) return;

  String sky;
  if (nextDay.isRainLikely(RAIN_LIKELY_PCT)) {
    switch (nextDay.condition) {
      case WeatherCondition::Snow: sky = "snow"; break;
      case WeatherCondition::Thunderstorm:
      case WeatherCondition::Hail: sky = "storms"; break;
      default: sky = "rain"; break;
    }
  } else {
    switch (nextDay.condition) {
      case WeatherCondition::Clear: sky = "clear"; break;
      case WeatherCondition::Fog: sky = "fog"; break;
      default: sky = "cloudy"; break;
    }
  }

  int deg = nextDay.tempDeltaRounded();
  String trend;
  if (deg > 0) {
    trend = "+" + String(deg) + "°";
  } else if (deg < 0) {
    trend = String(deg) + "°";  // String() already prefixes the minus sign
  } else {
    trend = "±0°";
  }

  int32_t x = originX_ + MARGIN_X;
  int32_t y = originY_ + SECOND_LINE_Y;
  String line = "Tmrw: " + sky + ", " + trend;
  write_string((GFXfont*)&FiraSans, line.c_str(), &x, &y, framebuffer_);
}

void WeatherRenderer::draw(const std::vector<WeatherData>& forecast,
                           bool showCommute, const SunData& sun,
                           const NextDayData& nextDay) {
  if (forecast.empty()) {
    int32_t x = originX_ + MARGIN_X;
    int32_t y = originY_ + CURRENT_WEATHER_Y;
    write_string((GFXfont*)&FiraSans, "No weather data available", &x, &y,
                 framebuffer_);
    return;
  }

  drawHeader();
  drawCurrentWeather(forecast[0], sun);

  if (forecast.size() > 1) {
    drawForecastGrid(forecast, sun);
  }

  drawSunInfo(sun, isNightTime(forecast[0], sun));

  if (showCommute) {
    drawCommuteRecommendation(forecast);
  } else {
    drawNextDay(nextDay);
  }
}
