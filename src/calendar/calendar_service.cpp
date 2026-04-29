#include "calendar_service.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <algorithm>

#include "../config.h"

namespace {

// Howard Hinnant's days_from_civil — converts a UTC (Y,M,D) to days since
// 1970-01-01, valid for any proleptic Gregorian date.
long daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153u * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097L + (long)doe - 719468L;
}

int parseInt2(const char* s) { return (s[0] - '0') * 10 + (s[1] - '0'); }
int parseInt4(const char* s) {
  return (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 +
         (s[3] - '0');
}

// Parse "YYYYMMDD" or "YYYYMMDDTHHMMSS[Z]".
// `isUtc` true => Z-suffixed UTC; false => floating/TZID — treated as local.
// `dateOnly` true => allDay, returns midnight local.
bool parseIcsDateTime(const String& v, bool isUtc, bool dateOnly,
                      time_t* out) {
  if (dateOnly) {
    if (v.length() < 8) return false;
    struct tm tm{};
    tm.tm_year = parseInt4(v.c_str()) - 1900;
    tm.tm_mon = parseInt2(v.c_str() + 4) - 1;
    tm.tm_mday = parseInt2(v.c_str() + 6);
    tm.tm_isdst = -1;
    *out = mktime(&tm);
    return *out != (time_t)-1;
  }
  if (v.length() < 15 || v[8] != 'T') return false;
  int Y = parseInt4(v.c_str());
  int M = parseInt2(v.c_str() + 4);
  int D = parseInt2(v.c_str() + 6);
  int h = parseInt2(v.c_str() + 9);
  int m = parseInt2(v.c_str() + 11);
  int s = parseInt2(v.c_str() + 13);
  if (isUtc) {
    long days = daysFromCivil(Y, M, D);
    *out = (time_t)(days * 86400L + h * 3600 + m * 60 + s);
  } else {
    struct tm tm{};
    tm.tm_year = Y - 1900;
    tm.tm_mon = M - 1;
    tm.tm_mday = D;
    tm.tm_hour = h;
    tm.tm_min = m;
    tm.tm_sec = s;
    tm.tm_isdst = -1;
    *out = mktime(&tm);
    if (*out == (time_t)-1) return false;
  }
  return true;
}

// Reverses iCal text escapes: \\, \,, \;, \n, \N.
String unescapeText(const String& s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '\\' && i + 1 < s.length()) {
      char n = s[++i];
      if (n == 'n' || n == 'N') out += ' ';
      else out += n;
    } else {
      out += c;
    }
  }
  return out;
}

struct EventDraft {
  String summary;
  String dtstartValue;
  bool dtstartUtc = false;
  bool dtstartDate = false;
  String dtendValue;
  bool dtendUtc = false;
  bool dtendDate = false;
  bool valid = false;
};

void applyProperty(EventDraft& ev, const String& nameAndParams,
                   const String& value) {
  // Split "NAME;PARAM=...;PARAM=..." into name and params.
  int sc = nameAndParams.indexOf(';');
  String name = sc < 0 ? nameAndParams : nameAndParams.substring(0, sc);
  String params = sc < 0 ? String() : nameAndParams.substring(sc + 1);
  name.toUpperCase();

  if (name == "SUMMARY") {
    ev.summary = unescapeText(value);
  } else if (name == "DTSTART" || name == "DTEND") {
    bool isDate = params.indexOf("VALUE=DATE") >= 0 &&
                  params.indexOf("VALUE=DATE-TIME") < 0;
    bool isUtc = value.endsWith("Z");
    if (name == "DTSTART") {
      ev.dtstartValue = value;
      ev.dtstartUtc = isUtc;
      ev.dtstartDate = isDate;
    } else {
      ev.dtendValue = value;
      ev.dtendUtc = isUtc;
      ev.dtendDate = isDate;
    }
  }
}

// Reads one logical (unfolded) iCal line from the stream. Returns false at
// EOF. Continuation lines (leading space/tab) are merged into the previous.
bool readLogicalLine(Client& s, String& carry, String& out) {
  while (true) {
    // Block briefly for more bytes; bail when the connection is fully drained.
    while (!s.available()) {
      if (!s.connected()) {
        if (carry.length()) {
          out = carry;
          carry = "";
          return true;
        }
        return false;
      }
      delay(2);
    }
    int peeked = s.peek();
    if (peeked == ' ' || peeked == '\t') {
      s.read();  // consume the fold indicator
      String cont = s.readStringUntil('\n');
      cont.replace("\r", "");
      carry += cont;
      continue;
    }
    if (carry.length()) {
      out = carry;
      carry = s.readStringUntil('\n');
      carry.replace("\r", "");
      return true;
    }
    carry = s.readStringUntil('\n');
    carry.replace("\r", "");
  }
}

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

  Serial.println("Fetching calendar ICS...");
  int code = http.GET();
  Serial.printf("Calendar response code: %d\n", code);
  if (code != HTTP_CODE_OK) {
    http.end();
    return events;
  }

  time_t now = time(nullptr);
  time_t windowEnd = now + (time_t)CALENDAR_LOOKAHEAD_DAYS * 86400;

  WiFiClient& stream = http.getStream();
  String carry;
  String line;
  EventDraft draft;
  bool inEvent = false;

  while (readLogicalLine(stream, carry, line)) {
    int colon = line.indexOf(':');
    if (colon < 0) continue;
    String left = line.substring(0, colon);
    String value = line.substring(colon + 1);

    String upper = left;
    upper.toUpperCase();

    if (upper == "BEGIN" && value == "VEVENT") {
      draft = EventDraft{};
      inEvent = true;
      continue;
    }
    if (upper == "END" && value == "VEVENT") {
      inEvent = false;
      // TODO: expand RRULE (FREQ=DAILY|WEEKLY|MONTHLY|YEARLY with INTERVAL,
      // COUNT, UNTIL, BYDAY) and apply EXDATE so recurring events show on
      // every occurrence within the lookahead window, not just the first.
      if (draft.dtstartValue.length()) {
        CalendarEvent ev;
        ev.summary = draft.summary;
        ev.allDay = draft.dtstartDate;
        if (parseIcsDateTime(draft.dtstartValue, draft.dtstartUtc,
                             draft.dtstartDate, &ev.start)) {
          if (draft.dtendValue.length()) {
            parseIcsDateTime(draft.dtendValue, draft.dtendUtc,
                             draft.dtendDate, &ev.end);
          }
          // Keep events whose start is within [now-1h, now+lookahead). The
          // 1h grace lets an in-progress meeting still appear.
          if (ev.start >= now - 3600 && ev.start < windowEnd) {
            events.push_back(ev);
          }
        }
      }
      continue;
    }
    if (!inEvent) continue;
    applyProperty(draft, left, value);
  }

  http.end();

  std::sort(events.begin(), events.end(),
            [](const CalendarEvent& a, const CalendarEvent& b) {
              return a.start < b.start;
            });
  if ((int)events.size() > CALENDAR_NUM_EVENTS) {
    events.resize(CALENDAR_NUM_EVENTS);
  }

  Serial.printf("Parsed %u upcoming events\n", (unsigned)events.size());
  return events;
}
