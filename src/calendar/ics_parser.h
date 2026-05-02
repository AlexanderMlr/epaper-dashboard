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
  int outsideWindow = 0;
};

// Parses an ICS body and returns events whose DTSTART falls in
// [now - 1h, now + lookaheadDays). Sorted by start, trimmed to maxEvents.
// `stats` is optional and receives parse counters when non-null.
std::vector<ParsedIcsEvent> parseIcs(const char* body, std::size_t len,
                                     time_t now, int lookaheadDays,
                                     int maxEvents,
                                     IcsParseStats* stats = nullptr);
