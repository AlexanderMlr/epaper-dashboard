#include "ics_parser.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

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
bool parseIcsDateTime(const std::string& v, bool isUtc, bool dateOnly,
                      time_t* out) {
  if (dateOnly) {
    if (v.size() < 8) return false;
    struct tm tm{};
    tm.tm_year = parseInt4(v.c_str()) - 1900;
    tm.tm_mon = parseInt2(v.c_str() + 4) - 1;
    tm.tm_mday = parseInt2(v.c_str() + 6);
    tm.tm_isdst = -1;
    *out = mktime(&tm);
    return *out != (time_t)-1;
  }
  if (v.size() < 15 || v[8] != 'T') return false;
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

std::string unescapeText(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); i++) {
    char c = s[i];
    if (c == '\\' && i + 1 < s.size()) {
      char n = s[++i];
      if (n == 'n' || n == 'N') out += ' ';
      else out += n;
    } else {
      out += c;
    }
  }
  return out;
}

std::string toUpper(const std::string& s) {
  std::string out;
  out.resize(s.size());
  for (size_t i = 0; i < s.size(); i++) {
    out[i] = (char)std::toupper((unsigned char)s[i]);
  }
  return out;
}

// Strips trailing CR / spaces / tabs.
void rtrim(std::string& s) {
  while (!s.empty()) {
    char c = s.back();
    if (c == '\r' || c == ' ' || c == '\t') s.pop_back();
    else break;
  }
}

struct Recurrence {
  enum Freq { NONE, DAILY, WEEKLY, MONTHLY, YEARLY };
  Freq freq = NONE;
  int interval = 1;
  int count = 0;          // 0 = unlimited
  time_t until = 0;
  bool untilSet = false;
  uint8_t byDayMask = 0;  // bit i set => weekday i (0=Sun..6=Sat) is allowed
};

// Parses an RRULE value (the part after "RRULE:"). Only the subset used by
// typical Google/Apple calendars is honored: FREQ, INTERVAL, COUNT, UNTIL,
// and BYDAY (as a plain weekday list — positional prefixes like "1MO" are
// stripped). WKST defaults to MO. Returns true if FREQ was recognized.
bool parseRrule(const std::string& v, Recurrence* r) {
  *r = Recurrence{};
  size_t i = 0;
  while (i < v.size()) {
    size_t sc = v.find(';', i);
    std::string part =
        v.substr(i, sc == std::string::npos ? std::string::npos : sc - i);
    i = (sc == std::string::npos) ? v.size() : sc + 1;
    size_t eq = part.find('=');
    if (eq == std::string::npos) continue;
    std::string key = toUpper(part.substr(0, eq));
    std::string val = part.substr(eq + 1);
    if (key == "FREQ") {
      std::string fu = toUpper(val);
      if (fu == "DAILY") r->freq = Recurrence::DAILY;
      else if (fu == "WEEKLY") r->freq = Recurrence::WEEKLY;
      else if (fu == "MONTHLY") r->freq = Recurrence::MONTHLY;
      else if (fu == "YEARLY") r->freq = Recurrence::YEARLY;
    } else if (key == "INTERVAL") {
      int n = std::atoi(val.c_str());
      r->interval = n > 0 ? n : 1;
    } else if (key == "COUNT") {
      r->count = std::atoi(val.c_str());
    } else if (key == "UNTIL") {
      bool untilUtc = !val.empty() && val.back() == 'Z';
      bool untilDate = (val.find('T') == std::string::npos);
      time_t u;
      if (parseIcsDateTime(val, untilUtc, untilDate, &u)) {
        r->until = u;
        r->untilSet = true;
      }
    } else if (key == "BYDAY") {
      size_t j = 0;
      while (j < val.size()) {
        size_t comma = val.find(',', j);
        std::string dayPart = val.substr(
            j, comma == std::string::npos ? std::string::npos : comma - j);
        j = (comma == std::string::npos) ? val.size() : comma + 1;
        size_t k = 0;
        if (k < dayPart.size() && (dayPart[k] == '+' || dayPart[k] == '-')) k++;
        while (k < dayPart.size() && std::isdigit((unsigned char)dayPart[k])) k++;
        std::string code = toUpper(dayPart.substr(k));
        int wday = -1;
        if (code == "SU") wday = 0;
        else if (code == "MO") wday = 1;
        else if (code == "TU") wday = 2;
        else if (code == "WE") wday = 3;
        else if (code == "TH") wday = 4;
        else if (code == "FR") wday = 5;
        else if (code == "SA") wday = 6;
        if (wday >= 0) r->byDayMask |= (uint8_t)(1u << wday);
      }
    }
  }
  return r->freq != Recurrence::NONE;
}

// useUtc: step by raw seconds. Floating-local / date-only: round-trip
// through tm so wall-clock time is preserved across DST transitions.
// Same convention applies to addMonths / addYears below.
time_t addDays(time_t t, int days, bool useUtc) {
  if (useUtc) return t + (time_t)days * 86400;
  struct tm tm;
  localtime_r(&t, &tm);
  tm.tm_mday += days;
  tm.tm_isdst = -1;
  return mktime(&tm);
}

time_t addMonths(time_t t, int months, bool useUtc) {
  struct tm tm;
  if (useUtc) {
    gmtime_r(&t, &tm);
    int Y = tm.tm_year + 1900;
    int M = tm.tm_mon + 1 + months;
    while (M > 12) { M -= 12; Y++; }
    while (M < 1)  { M += 12; Y--; }
    long days = daysFromCivil(Y, (unsigned)M, (unsigned)tm.tm_mday);
    return (time_t)(days * 86400L + tm.tm_hour * 3600 +
                    tm.tm_min * 60 + tm.tm_sec);
  }
  localtime_r(&t, &tm);
  tm.tm_mon += months;
  tm.tm_isdst = -1;
  return mktime(&tm);
}

time_t addYears(time_t t, int years, bool useUtc) {
  return addMonths(t, years * 12, useUtc);
}

int weekdayOf(time_t t, bool useUtc) {
  struct tm tm;
  if (useUtc) gmtime_r(&t, &tm);
  else        localtime_r(&t, &tm);
  return tm.tm_wday;
}

// How far the expansion loop advances per iteration. WEEKLY+BYDAY uses a
// 1-day stride and filters in `isRealOccurrence`; everything else strides
// by the freq's natural unit times the user's INTERVAL.
struct RecurrenceStep {
  enum Kind { DAY, MONTH, YEAR } kind;
  int n;
};

RecurrenceStep stepFor(const Recurrence& r) {
  switch (r.freq) {
    case Recurrence::DAILY:   return {RecurrenceStep::DAY, r.interval};
    case Recurrence::WEEKLY:
      return {RecurrenceStep::DAY, r.byDayMask ? 1 : 7 * r.interval};
    case Recurrence::MONTHLY: return {RecurrenceStep::MONTH, r.interval};
    case Recurrence::YEARLY:  return {RecurrenceStep::YEAR, r.interval};
    case Recurrence::NONE:
    default:                  return {RecurrenceStep::DAY, 1};
  }
}

time_t applyStep(time_t cur, RecurrenceStep step, bool useUtc) {
  switch (step.kind) {
    case RecurrenceStep::DAY:   return addDays(cur, step.n, useUtc);
    case RecurrenceStep::MONTH: return addMonths(cur, step.n, useUtc);
    case RecurrenceStep::YEAR:  return addYears(cur, step.n, useUtc);
  }
  return cur;
}

// Mon-anchored start of the week containing `t`. Used by WEEKLY+INTERVAL>1
// to decide whether a candidate day belongs to an "active" cycle week.
// Assumes WKST=MO (RFC 5545 default).
time_t weekAnchorOf(time_t t, bool useUtc) {
  int wday = weekdayOf(t, useUtc);
  int daysFromWkst = ((wday - 1) % 7 + 7) % 7;
  return t - (time_t)daysFromWkst * 86400;
}

// True if `cur` is a real recurrence instance after applying BYDAY and
// INTERVAL-week filters. Non-WEEKLY freqs treat every step as an instance.
bool isRealOccurrence(time_t cur, time_t weekAnchor, const Recurrence& r,
                      bool useUtc) {
  if (r.freq != Recurrence::WEEKLY || r.byDayMask == 0) return true;
  int wday = weekdayOf(cur, useUtc);
  if (!(r.byDayMask & (1u << wday))) return false;
  if (r.interval > 1) {
    long daysSinceAnchor = (long)((cur - weekAnchor) / 86400);
    if (daysSinceAnchor < 0) daysSinceAnchor = 0;
    if ((daysSinceAnchor / 7) % r.interval != 0) return false;
  }
  return true;
}

bool isExcluded(time_t cur, const std::vector<time_t>& exdates) {
  for (time_t ex : exdates) {
    if (ex == cur) return true;
  }
  return false;
}

struct EventDraft {
  std::string summary;
  std::string dtstartValue;
  bool dtstartUtc = false;
  bool dtstartDate = false;
  std::string dtendValue;
  bool dtendUtc = false;
  bool dtendDate = false;
  std::string rruleValue;
  std::vector<time_t> exdates;
};

void applyProperty(EventDraft& ev, const std::string& nameAndParams,
                   const std::string& value) {
  size_t sc = nameAndParams.find(';');
  std::string name = sc == std::string::npos ? nameAndParams
                                              : nameAndParams.substr(0, sc);
  std::string params = sc == std::string::npos ? std::string()
                                                : nameAndParams.substr(sc + 1);
  name = toUpper(name);

  if (name == "SUMMARY") {
    ev.summary = unescapeText(value);
  } else if (name == "DTSTART" || name == "DTEND") {
    bool isDate = params.find("VALUE=DATE") != std::string::npos &&
                  params.find("VALUE=DATE-TIME") == std::string::npos;
    bool isUtc = !value.empty() && value.back() == 'Z';
    if (name == "DTSTART") {
      ev.dtstartValue = value;
      ev.dtstartUtc = isUtc;
      ev.dtstartDate = isDate;
    } else {
      ev.dtendValue = value;
      ev.dtendUtc = isUtc;
      ev.dtendDate = isDate;
    }
  } else if (name == "RRULE") {
    ev.rruleValue = value;
  } else if (name == "EXDATE") {
    bool isDate = params.find("VALUE=DATE") != std::string::npos &&
                  params.find("VALUE=DATE-TIME") == std::string::npos;
    size_t i = 0;
    while (i < value.size()) {
      size_t comma = value.find(',', i);
      std::string v = value.substr(
          i, comma == std::string::npos ? std::string::npos : comma - i);
      i = (comma == std::string::npos) ? value.size() : comma + 1;
      if (v.empty()) continue;
      bool vUtc = v.back() == 'Z';
      time_t t;
      if (parseIcsDateTime(v, vUtc, isDate, &t)) ev.exdates.push_back(t);
    }
  }
}

// Reads one physical line into `out` (appended). Skips '\r'. Consumes '\n'.
// Returns false at EOF with nothing more to read.
bool appendPhysicalLine(const char* body, size_t len, size_t& pos,
                        std::string& out) {
  if (pos >= len) return false;
  while (pos < len) {
    char c = body[pos++];
    if (c == '\n') return true;
    if (c == '\r') continue;
    out += c;
  }
  return true;
}

// Reads one logical (unfolded) iCal line. Continuation lines (leading
// space/tab) are merged into the previous.
bool readLogicalLine(const char* body, size_t len, size_t& pos,
                     std::string& out) {
  out.clear();
  if (!appendPhysicalLine(body, len, pos, out)) return false;
  while (pos < len) {
    char c = body[pos];
    if (c != ' ' && c != '\t') break;
    pos++;  // consume fold indicator
    appendPhysicalLine(body, len, pos, out);
  }
  return true;
}

// Emits the single instance or expanded recurrences for one VEVENT into
// `events`. Occurrences are clipped to [windowStart, windowEnd).
void emitOccurrences(const EventDraft& draft, time_t windowStart,
                     time_t windowEnd, std::vector<ParsedIcsEvent>& events,
                     IcsParseStats& st) {
  if (draft.dtstartValue.empty()) {
    st.noDtstart++;
    return;
  }
  time_t baseStart;
  if (!parseIcsDateTime(draft.dtstartValue, draft.dtstartUtc,
                        draft.dtstartDate, &baseStart)) {
    st.parseFail++;
    return;
  }
  time_t baseEnd = 0;
  bool haveEnd = false;
  if (!draft.dtendValue.empty()) {
    haveEnd = parseIcsDateTime(draft.dtendValue, draft.dtendUtc,
                               draft.dtendDate, &baseEnd);
  }
  time_t duration = (haveEnd && baseEnd > baseStart) ? baseEnd - baseStart : 0;

  auto pushEvent = [&](time_t s) {
    ParsedIcsEvent ev;
    ev.summary = draft.summary;
    ev.allDay = draft.dtstartDate;
    ev.start = s;
    ev.end = haveEnd ? s + duration : 0;
    events.push_back(std::move(ev));
  };

  Recurrence rrule;
  bool hasRrule = !draft.rruleValue.empty() &&
                  parseRrule(draft.rruleValue, &rrule);

  if (!hasRrule) {
    if (baseStart >= windowStart && baseStart < windowEnd) {
      pushEvent(baseStart);
    } else {
      st.outsideWindow++;
    }
    return;
  }

  st.recurrenceExpanded++;

  // Date-only DTSTART always lives in local-midnight semantics (mktime),
  // even though it has no time-of-day. Treat it as floating-local for stepping.
  const bool useUtc = draft.dtstartUtc && !draft.dtstartDate;
  const RecurrenceStep step = stepFor(rrule);
  const time_t weekAnchor = weekAnchorOf(baseStart, useUtc);

  // Safety cap: a recurrence anchored years ago with a small interval can't
  // run forever. 5000 covers ~13.7 years of daily steps. Hitting the cap
  // bumps `recurrenceCapped` so silent truncation is observable.
  const int kMaxIterations = 5000;
  int iter = 0;
  int occurrenceIndex = 0;
  time_t cur = baseStart;
  for (; iter < kMaxIterations; iter++) {
    if (rrule.untilSet && cur > rrule.until) break;
    if (cur >= windowEnd) break;

    if (isRealOccurrence(cur, weekAnchor, rrule, useUtc)) {
      // COUNT counts real occurrences (after BY* filters), pre-EXDATE,
      // including ones before the window.
      if (rrule.count > 0 && occurrenceIndex >= rrule.count) break;
      occurrenceIndex++;
      if (cur >= windowStart && !isExcluded(cur, draft.exdates)) {
        pushEvent(cur);
      }
    }

    time_t next = applyStep(cur, step, useUtc);
    if (next == (time_t)-1 || next <= cur) break;
    cur = next;
  }
  if (iter == kMaxIterations) st.recurrenceCapped++;
}

}  // namespace

std::vector<ParsedIcsEvent> parseIcs(const char* body, std::size_t len,
                                     time_t now, int lookaheadDays,
                                     int maxEvents, IcsParseStats* stats) {
  std::vector<ParsedIcsEvent> events;
  IcsParseStats local;
  IcsParseStats& st = stats ? *stats : local;

  const time_t windowStart = now - 3600;
  const time_t windowEnd = now + (time_t)lookaheadDays * 86400;

  size_t pos = 0;
  std::string line;
  line.reserve(512);
  EventDraft draft;
  bool inEvent = false;

  while (readLogicalLine(body, len, pos, line)) {
    st.logicalLines++;
    rtrim(line);
    size_t colon = line.find(':');
    if (colon == std::string::npos) continue;
    std::string left = line.substr(0, colon);
    std::string value = line.substr(colon + 1);
    std::string upper = toUpper(left);

    if (upper == "BEGIN" && value == "VEVENT") {
      st.beginVevent++;
      draft = EventDraft{};
      inEvent = true;
      continue;
    }
    if (upper == "END" && value == "VEVENT") {
      st.endVevent++;
      inEvent = false;
      emitOccurrences(draft, windowStart, windowEnd, events, st);
      continue;
    }
    if (!inEvent) continue;
    applyProperty(draft, left, value);
  }

  std::sort(events.begin(), events.end(),
            [](const ParsedIcsEvent& a, const ParsedIcsEvent& b) {
              return a.start < b.start;
            });
  if ((int)events.size() > maxEvents) {
    events.resize(maxEvents);
  }
  return events;
}
