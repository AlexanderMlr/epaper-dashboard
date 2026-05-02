#include "ics_parser.h"

#include <algorithm>
#include <cctype>
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

struct EventDraft {
  std::string summary;
  std::string dtstartValue;
  bool dtstartUtc = false;
  bool dtstartDate = false;
  std::string dtendValue;
  bool dtendUtc = false;
  bool dtendDate = false;
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

}  // namespace

std::vector<ParsedIcsEvent> parseIcs(const char* body, std::size_t len,
                                     time_t now, int lookaheadDays,
                                     int maxEvents, IcsParseStats* stats) {
  std::vector<ParsedIcsEvent> events;
  IcsParseStats local;
  IcsParseStats& st = stats ? *stats : local;

  time_t windowEnd = now + (time_t)lookaheadDays * 86400;

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
      // TODO: expand RRULE / apply EXDATE for recurring events.
      if (draft.dtstartValue.empty()) {
        st.noDtstart++;
        continue;
      }
      ParsedIcsEvent ev;
      ev.summary = draft.summary;
      ev.allDay = draft.dtstartDate;
      if (!parseIcsDateTime(draft.dtstartValue, draft.dtstartUtc,
                            draft.dtstartDate, &ev.start)) {
        st.parseFail++;
        continue;
      }
      if (!draft.dtendValue.empty()) {
        parseIcsDateTime(draft.dtendValue, draft.dtendUtc, draft.dtendDate,
                         &ev.end);
      }
      if (ev.start >= now - 3600 && ev.start < windowEnd) {
        events.push_back(std::move(ev));
      } else {
        st.outsideWindow++;
      }
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
