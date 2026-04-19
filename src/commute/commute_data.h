#pragma once

#include <Arduino.h>

#include <vector>

struct CommuteSegment {
  String trainLine;
  String trainCategory;
  String departureTime;
  String arrivalTime;
  String platform;
  int delayMinutes = 0;
  String origin;
  String destination;
};

struct CommuteRoute {
  int transfers = 0;
  int durationMinutes = 0;
  String departureTime;
  String arrivalTime;
  int departureDelay = 0;
  bool cancelled = false;
  std::vector<CommuteSegment> segments;
};
