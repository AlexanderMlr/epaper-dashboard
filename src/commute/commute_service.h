#pragma once

#include <ArduinoJson.h>

#include <vector>

#include "commute_data.h"

class CommuteService {
 public:
  std::vector<CommuteRoute> fetchRoutes();

 private:
  String buildRequestUrl() const;
  static String extractTime(const String& isoDatetime);
};
