#pragma once

#include <string>
#include <vector>

struct CommuteSegment {
  std::string trainLine;
  std::string trainCategory;
  std::string departureTime;
  std::string arrivalTime;
  std::string platform;
  int delayMinutes = 0;
  std::string origin;
  std::string destination;
};

struct CommuteRoute {
  int transfers = 0;
  int durationMinutes = 0;
  std::string departureTime;
  std::string arrivalTime;
  int departureDelay = 0;
  bool cancelled = false;
  std::vector<CommuteSegment> segments;
};
