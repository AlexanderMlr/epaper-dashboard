#pragma once

#include <Arduino.h>

#include <vector>

#include "commute_data.h"

class CommuteService {
 public:
  std::vector<CommuteRoute> fetchRoutes();

 private:
  String buildRequestUrl() const;
};
