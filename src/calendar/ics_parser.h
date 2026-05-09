#pragma once

#include <cstddef>
#include <ctime>
#include <string>
#include <vector>

struct ParsedIcsEvent {
  std::string summary;
  time_t start = 0;
  time_t end = 0;
  bool allDay = false;
};

struct IcsParseStats {
  int logicalLines = 0;
  int beginVevent = 0;
  int endVevent = 0;
  int parseFail = 0;
  int noDtstart = 0;
  // Non-recurring VEVENTs whose start falls outside the lookahead window.
  // Recurring events that produce zero in-window occurrences are NOT counted
  // here; they show up only in `recurrenceExpanded`.
  int outsideWindow = 0;
  // VEVENTs that had a parseable RRULE and were expanded into occurrences.
  int recurrenceExpanded = 0;
  // RRULE expansions that hit the per-event iteration safety cap. A non-zero
  // value means at least one recurrence was silently truncated.
  int recurrenceCapped = 0;
};

// Parses an ICS body and emits events whose start falls in
// [now - 1h, now + lookaheadDays). Recurring events (RRULE) are expanded
// into individual occurrences within that window. Sorted by start, trimmed
// to maxEvents. `stats` is optional and receives parse counters when non-null.
std::vector<ParsedIcsEvent> parseIcs(const char* body, std::size_t len,
                                     time_t now, int lookaheadDays,
                                     int maxEvents,
                                     IcsParseStats* stats = nullptr);
