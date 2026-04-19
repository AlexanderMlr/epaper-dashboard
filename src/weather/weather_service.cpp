#include "weather_service.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "../config.h"
#include "weather_data.h"

String WeatherService::buildRequestUrl() const {
  return String("http://api.openweathermap.org/data/2.5/forecast?") +
         "appid=" + WEATHER_API_KEY + "&units=metric&cnt=" + NUM_FORECASTS +
         "&lat=" + LOCATION_LATITUDE + "&lon=" + LOCATION_LONGITUDE;
}

WeatherData WeatherService::parseWeatherEntry(const JsonObject& entry) const {
  WeatherData weather;
  weather.characterization = entry["weather"][0]["main"].as<String>();
  weather.description = entry["weather"][0]["description"].as<String>();
  weather.absolute_temperature = entry["main"]["temp"].as<float>();
  weather.felt_temperature = entry["main"]["feels_like"].as<float>();
  weather.rain_probability = entry["pop"].as<float>() * 100.0f;
  weather.datetime = entry["dt"].as<int>();
  weather.datetime_str = entry["dt_txt"].as<String>();
  return weather;
}

std::vector<WeatherData> WeatherService::fetchForecast() {
  std::vector<WeatherData> forecasts;
  forecasts.reserve(NUM_FORECASTS);

  HTTPClient http;
  http.begin(buildRequestUrl());
  http.setTimeout(10000);

  const int httpCode = http.GET();
  Serial.printf("Weather API response code: %d\n", httpCode);

  if (httpCode == HTTP_CODE_OK) {
    const String payload = http.getString();
    JsonDocument apiResponse;

    DeserializationError error = deserializeJson(apiResponse, payload);
    if (error == DeserializationError::Ok) {
      JsonArray list = apiResponse["list"].as<JsonArray>();
      for (JsonVariant item : list) {
        forecasts.push_back(parseWeatherEntry(item.as<JsonObject>()));
      }
    } else {
      Serial.printf("JSON parsing failed: %s\n", error.c_str());
    }
  } else {
    Serial.printf("HTTP request failed with code: %d\n", httpCode);
  }

  http.end();
  return forecasts;
}
