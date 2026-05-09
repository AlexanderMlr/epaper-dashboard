#pragma once

#include <vector>

#include "calendar_data.h"

class CalendarService {
 public:
  // Fetches the configured ICS feed and returns upcoming events sorted by
  // start time. Recurring events (RRULE) are expanded into individual
  // occurrences within the lookahead window.
  std::vector<CalendarEvent> fetchEvents();
};
