#pragma once

#include <Arduino.h>

#include <ctime>

struct CalendarEvent {
  String summary;
  time_t start = 0;        // unix epoch in UTC; 0 if unparsed
  time_t end = 0;
  bool allDay = false;
};
