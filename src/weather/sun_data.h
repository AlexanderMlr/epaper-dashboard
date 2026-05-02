#pragma once

#include <Arduino.h>

struct SunData {
  float uvIndexMax;
  String sunrise;  // "HH:MM" local
  String sunset;   // "HH:MM" local

  SunData() : uvIndexMax(-1.0f) {}
  bool isValid() const { return !sunrise.isEmpty() && !sunset.isEmpty(); }
};
