#pragma once

#include <vector>

#include "calendar_data.h"

class CalendarService {
 public:
  // Fetches the configured ICS feed and returns upcoming events sorted by
  // start time. Recurring events (RRULE) are not yet expanded; only their
  // first occurrence is returned.
  std::vector<CalendarEvent> fetchEvents();
};
