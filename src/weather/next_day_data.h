#pragma once

#include <cmath>

#include "weather_condition.h"

struct NextDayData {
  WeatherCondition condition;
  float precipProbMax;  // max precipitation probability over the day, %
  float highTempC;      // tomorrow's high
  float tempDeltaC;     // tomorrow's high minus today's high
  bool valid;

  NextDayData()
      : condition(WeatherCondition::Unknown),
        precipProbMax(0.0f),
        highTempC(0.0f),
        tempDeltaC(0.0f),
        valid(false) {}

  bool isRainLikely(float thresholdPct) const {
    return precipProbMax >= thresholdPct;
  }

  int highTempRounded() const { return (int)std::lround(highTempC); }

  // Whole-degree day-over-day change in the high; sign = warmer(+)/colder(-).
  int tempDeltaRounded() const { return (int)std::lround(tempDeltaC); }
};
