#include "calendar_service.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "../config.h"
#include "ics_parser.h"

namespace {
constexpr size_t kInitialCapacity = 64 * 1024;
constexpr unsigned long kReadTimeoutMs = 20000;
const int CALENDAR_NUM_EVENTS = 5;  // limited to 5 due to screen size
}  // namespace

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
  int contentLength = http.getSize();
  Serial.printf("Calendar response code: %d (size=%d)\n", code, contentLength);
  if (code != HTTP_CODE_OK) {
    http.end();
    return events;
  }

  // Buffer the body in PSRAM rather than the default heap: the body is
  // ~180 KB and the internal D-RAM heap is fragmented enough after Wi-Fi/TLS
  // setup that a single contiguous allocation that large would silently
  // truncate. Capacity grows on demand because Google's ICS endpoint uses
  // chunked transfer encoding, so Content-Length is usually -1.
  size_t cap = contentLength > 0 ? (size_t)contentLength + 1 : kInitialCapacity;
  size_t got = 0;
  char* body = (char*)ps_malloc(cap);
  if (!body) {
    Serial.printf("ps_malloc(%u) failed (psram free=%u)\n", (unsigned)cap,
                  (unsigned)ESP.getFreePsram());
    http.end();
    return events;
  }

  WiFiClient* stream = http.getStreamPtr();
  const unsigned long deadline = millis() + kReadTimeoutMs;
  bool readError = false;
  bool sawEndMarker = false;
  static const char kEndMarker[] = "END:VCALENDAR";
  const size_t kEndMarkerLen = sizeof(kEndMarker) - 1;
  while (millis() < deadline) {
    int avail = stream->available();
    if (avail <= 0) {
      if (!http.connected()) break;
      delay(5);
      continue;
    }
    if (got + (size_t)avail + 1 > cap) {
      size_t newCap = cap * 2;
      while (got + (size_t)avail + 1 > newCap) newCap *= 2;
      char* grown = (char*)ps_realloc(body, newCap);
      if (!grown) {
        Serial.printf("ps_realloc(%u) failed\n", (unsigned)newCap);
        readError = true;
        break;
      }
      body = grown;
      cap = newCap;
    }
    int n = stream->readBytes(body + got, avail);
    if (n > 0) got += (size_t)n;

    // ICS body always contains exactly one "END:VCALENDAR", at the end. Break
    // once we see it so we don't wait on the keep-alive TCP connection. We
    // search a tail window rather than the very last bytes because chunked
    // transfer encoding can append metadata (e.g. "0\r\n\r\n") after the body.
    if (!sawEndMarker && got >= kEndMarkerLen) {
      const size_t windowBytes = 256;
      const size_t windowStart =
          got > windowBytes ? got - windowBytes : 0;
      for (size_t i = windowStart; i + kEndMarkerLen <= got; i++) {
        if (memcmp(body + i, kEndMarker, kEndMarkerLen) == 0) {
          sawEndMarker = true;
          break;
        }
      }
      if (sawEndMarker) break;
    }
  }
  body[got] = '\0';
  http.end();

  Serial.printf("ICS body: got=%u cap=%u psram free=%u\n", (unsigned)got,
                (unsigned)cap, (unsigned)ESP.getFreePsram());

  const bool truncated =
      (contentLength > 0 && got < (size_t)contentLength) ||
      (contentLength <= 0 && !sawEndMarker);
  if (readError || got == 0 || truncated) {
    Serial.println("Body read failed or truncated, skipping parse");
    free(body);
    return events;
  }

  IcsParseStats stats;
  auto parsed = parseIcs(body, got, time(nullptr), CALENDAR_LOOKAHEAD_DAYS,
                         CALENDAR_NUM_EVENTS, &stats);
  free(body);

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
      "Parsed %u events (BEGIN:VEVENT=%d, END:VEVENT=%d, no-dtstart=%d, "
      "parse-fail=%d, outside-window=%d, rrule-expanded=%d, "
      "rrule-capped=%d)\n",
      (unsigned)events.size(), stats.beginVevent, stats.endVevent,
      stats.noDtstart, stats.parseFail, stats.outsideWindow,
      stats.recurrenceExpanded, stats.recurrenceCapped);
  return events;
}
