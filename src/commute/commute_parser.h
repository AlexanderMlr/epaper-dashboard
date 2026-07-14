#pragma once

#include <ArduinoJson.h>

#include <ctime>
#include <string>
#include <vector>

#include "commute_data.h"

// "2026-07-12T19:58:00Z" (UTC) -> unix epoch; 0 on parse failure.
time_t parseUtcIso(const char* iso);

// Epoch -> "HH:MM" in the local timezone (TZ must be configured).
std::string toLocalHHMM(time_t t);

void buildPlanFilter(JsonDocument& filter);

std::vector<CommuteRoute> routesFromPlan(JsonVariantConst doc);
