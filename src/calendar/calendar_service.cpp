#include "calendar_service.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "../config.h"
#include "ics_parser.h"

std::vector<CalendarEvent> CalendarService::fetchEvents() {
  std::vector<CalendarEvent> events;

  if (!CALENDAR_ICS_URL || CALENDAR_ICS_URL[0] == '\0') {
    Serial.println("CALENDAR_ICS_URL not configured");
    return events;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, CALENDAR_ICS_URL);
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  Serial.println("Fetching calendar ICS...");
  int code = http.GET();
  Serial.printf("Calendar response code: %d (size=%d)\n", code,
                http.getSize());
  if (code != HTTP_CODE_OK) {
    http.end();
    return events;
  }

  // getString() decodes chunked Transfer-Encoding (which Google's ICS
  // endpoint uses); reading the raw stream would block on chunk metadata.
  String body = http.getString();
  http.end();
  Serial.printf("ICS body length: %u, free heap: %u\n",
                (unsigned)body.length(), (unsigned)ESP.getFreeHeap());

  IcsParseStats stats;
  auto parsed = parseIcs(body.c_str(), body.length(), time(nullptr),
                         CALENDAR_LOOKAHEAD_DAYS, CALENDAR_NUM_EVENTS, &stats);

  events.reserve(parsed.size());
  for (auto& p : parsed) {
    CalendarEvent e;
    e.summary = String(p.summary.c_str());
    e.start = p.start;
    e.end = p.end;
    e.allDay = p.allDay;
    events.push_back(std::move(e));
  }

  Serial.printf(
      "Parsed %u events (lines=%d, BEGIN:VEVENT=%d, END:VEVENT=%d, "
      "no-dtstart=%d, parse-fail=%d, outside-window=%d)\n",
      (unsigned)events.size(), stats.logicalLines, stats.beginVevent,
      stats.endVevent, stats.noDtstart, stats.parseFail, stats.outsideWindow);
  return events;
}
