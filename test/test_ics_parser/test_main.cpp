#include <unity.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>

#include "calendar/ics_parser.h"

#ifndef FIXTURE_DIR
#define FIXTURE_DIR "test/test_ics_parser/fixtures"
#endif

namespace {

constexpr time_t kNow = 1777680000;  // 2026-05-02 00:00:00 UTC

std::string wrap(const std::string& vevents) {
  return "BEGIN:VCALENDAR\r\nVERSION:2.0\r\n" + vevents + "END:VCALENDAR\r\n";
}

std::string vevent(const std::string& summary, const std::string& dtstart,
                   const std::string& dtend = "",
                   const std::string& extra = "") {
  std::string s = "BEGIN:VEVENT\r\n";
  s += "UID:" + summary + "@test\r\n";
  s += "SUMMARY:" + summary + "\r\n";
  s += "DTSTART:" + dtstart + "\r\n";
  if (!dtend.empty()) s += "DTEND:" + dtend + "\r\n";
  if (!extra.empty()) s += extra;
  s += "END:VEVENT\r\n";
  return s;
}

}  // namespace

void setUp() {
  setenv("TZ", "UTC", 1);
  tzset();
}
void tearDown() {}

void test_empty_body() {
  auto out = parseIcs("", 0, kNow, 14, 6);
  TEST_ASSERT_EQUAL(0, (int)out.size());
}

void test_single_future_utc_event() {
  std::string ics = wrap(vevent("Standup", "20260503T080000Z",
                                "20260503T083000Z"));
  IcsParseStats stats;
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 6, &stats);
  TEST_ASSERT_EQUAL(1, stats.beginVevent);
  TEST_ASSERT_EQUAL(1, stats.endVevent);
  TEST_ASSERT_EQUAL(1, (int)out.size());
  TEST_ASSERT_EQUAL_STRING("Standup", out[0].summary.c_str());
  TEST_ASSERT_FALSE(out[0].allDay);
}

void test_all_day_event() {
  std::string ics = wrap("BEGIN:VEVENT\r\nUID:a@t\r\nSUMMARY:Holiday\r\n"
                         "DTSTART;VALUE=DATE:20260504\r\n"
                         "DTEND;VALUE=DATE:20260505\r\nEND:VEVENT\r\n");
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 6);
  TEST_ASSERT_EQUAL(1, (int)out.size());
  TEST_ASSERT_TRUE(out[0].allDay);
}

void test_past_event_filtered() {
  std::string ics = wrap(vevent("Old", "20240101T100000Z"));
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 6);
  TEST_ASSERT_EQUAL(0, (int)out.size());
}

void test_event_beyond_window_filtered() {
  std::string ics = wrap(vevent("Far", "20270101T100000Z"));
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 6);
  TEST_ASSERT_EQUAL(0, (int)out.size());
}

void test_in_progress_grace_window() {
  // 30 minutes ago should still appear (1h grace).
  time_t halfHourAgo = kNow - 1800;
  struct tm tm = *gmtime(&halfHourAgo);
  char buf[20];
  strftime(buf, sizeof(buf), "%Y%m%dT%H%M%SZ", &tm);
  std::string ics = wrap(vevent("Now", buf));
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 6);
  TEST_ASSERT_EQUAL(1, (int)out.size());
}

void test_max_events_trimmed_keeps_earliest() {
  std::string body;
  for (int i = 0; i < 20; i++) {
    char dt[20];
    snprintf(dt, sizeof(dt), "202605%02dT100000Z", 3 + i);
    char name[16];
    snprintf(name, sizeof(name), "E%02d", i);
    body += vevent(name, dt);
  }
  std::string ics = wrap(body);
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 30, 6);
  TEST_ASSERT_EQUAL(6, (int)out.size());
  TEST_ASSERT_EQUAL_STRING("E00", out[0].summary.c_str());
  TEST_ASSERT_EQUAL_STRING("E05", out[5].summary.c_str());
}

void test_events_sorted_by_start() {
  std::string body;
  body += vevent("C", "20260510T100000Z");
  body += vevent("A", "20260503T100000Z");
  body += vevent("B", "20260507T100000Z");
  std::string ics = wrap(body);
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 6);
  TEST_ASSERT_EQUAL(3, (int)out.size());
  TEST_ASSERT_EQUAL_STRING("A", out[0].summary.c_str());
  TEST_ASSERT_EQUAL_STRING("B", out[1].summary.c_str());
  TEST_ASSERT_EQUAL_STRING("C", out[2].summary.c_str());
}

// This is the regression test for the production bug: 506 VEVENT blocks in
// the body, only 187 reaching the END handler. Build a synthetic body with
// many blocks and assert the parser sees every one.
void test_many_vevents_all_seen() {
  std::string body;
  for (int i = 0; i < 506; i++) {
    char dt[20];
    // Spread across years so most fall outside the 14-day window — we only
    // care that the parser SEES every BEGIN/END:VEVENT, not that they're
    // kept.
    snprintf(dt, sizeof(dt), "20%02d0101T100000Z", 20 + (i % 30));
    char name[16];
    snprintf(name, sizeof(name), "E%04d", i);
    body += vevent(name, dt);
  }
  std::string ics = wrap(body);
  IcsParseStats stats;
  parseIcs(ics.c_str(), ics.size(), kNow, 14, 1000, &stats);
  TEST_ASSERT_EQUAL(506, stats.beginVevent);
  TEST_ASSERT_EQUAL(506, stats.endVevent);
}

void test_folded_continuation_lines() {
  std::string ics =
      "BEGIN:VCALENDAR\r\nBEGIN:VEVENT\r\nUID:f@t\r\n"
      "SUMMARY:Hello\r\n World\r\n"
      "DTSTART:20260503T100000Z\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n";
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 6);
  TEST_ASSERT_EQUAL(1, (int)out.size());
  TEST_ASSERT_EQUAL_STRING("HelloWorld", out[0].summary.c_str());
}

void test_lf_only_line_endings() {
  std::string ics =
      "BEGIN:VCALENDAR\nBEGIN:VEVENT\nUID:x@t\nSUMMARY:LF\n"
      "DTSTART:20260503T100000Z\nEND:VEVENT\nEND:VCALENDAR\n";
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 6);
  TEST_ASSERT_EQUAL(1, (int)out.size());
}

void test_valarm_inside_vevent_does_not_swallow_end() {
  std::string ics =
      "BEGIN:VCALENDAR\r\nBEGIN:VEVENT\r\nUID:a@t\r\nSUMMARY:WithAlarm\r\n"
      "DTSTART:20260503T100000Z\r\n"
      "BEGIN:VALARM\r\nACTION:DISPLAY\r\nTRIGGER:-PT15M\r\nEND:VALARM\r\n"
      "END:VEVENT\r\nEND:VCALENDAR\r\n";
  IcsParseStats stats;
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 6, &stats);
  TEST_ASSERT_EQUAL(1, stats.beginVevent);
  TEST_ASSERT_EQUAL(1, stats.endVevent);
  TEST_ASSERT_EQUAL(1, (int)out.size());
}

std::string slurpFile(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return std::string();
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

// Mirrors what calendar_service.cpp does on-device: lookahead=14, max=6,
// "now" set to today. This proves the parser would return events for the
// real ICS body with production config. If this passes but the device shows
// "No upcoming events", the divergence is in the HTTP fetch or in
// time(nullptr) on-device, not in the parser.
void test_real_fixture_with_production_config() {
  std::string path = std::string(FIXTURE_DIR) + "/real.ics";
  std::string body = slurpFile(path);
  if (body.empty()) {
    TEST_IGNORE_MESSAGE("real.ics fixture not present; skipping");
    return;
  }
  auto out = parseIcs(body.c_str(), body.size(), kNow, 14, 6);
  printf("[real.ics @ now=%ld lookahead=14 max=6] returned %zu events\n",
         (long)kNow, out.size());
  for (auto& e : out) {
    char buf[32];
    time_t s = e.start;
    struct tm tm = *gmtime(&s);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
    printf("  %s UTC  allDay=%d  %s\n", buf, e.allDay ? 1 : 0,
           e.summary.c_str());
  }
  TEST_ASSERT_GREATER_THAN_MESSAGE(
      0, out.size(),
      "production config should yield events from real fixture");
}

void test_real_fixture_all_vevents_seen() {
  std::string path = std::string(FIXTURE_DIR) + "/real.ics";
  std::string body = slurpFile(path);
  if (body.empty()) {
    TEST_IGNORE_MESSAGE("real.ics fixture not present; skipping");
    return;
  }
  IcsParseStats stats;
  parseIcs(body.c_str(), body.size(), kNow, 14, 1000, &stats);
  printf("[real.ics] bytes=%zu lines=%d BEGIN=%d END=%d "
         "no-dtstart=%d parse-fail=%d outside-window=%d\n",
         body.size(), stats.logicalLines, stats.beginVevent, stats.endVevent,
         stats.noDtstart, stats.parseFail, stats.outsideWindow);
  TEST_ASSERT_EQUAL_MESSAGE(506, stats.beginVevent,
                            "parser should see every BEGIN:VEVENT");
  TEST_ASSERT_EQUAL_MESSAGE(stats.beginVevent, stats.endVevent,
                            "every BEGIN:VEVENT should have a matching END");
}

void test_dtstart_with_tzid_parsed_as_local() {
  // TZ=UTC in setUp, so a "floating" 10:00 maps to 10:00 UTC.
  std::string ics = wrap(
      "BEGIN:VEVENT\r\nUID:tz@t\r\nSUMMARY:Tz\r\n"
      "DTSTART;TZID=Europe/Berlin:20260503T100000\r\nEND:VEVENT\r\n");
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 6);
  TEST_ASSERT_EQUAL(1, (int)out.size());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_empty_body);
  RUN_TEST(test_single_future_utc_event);
  RUN_TEST(test_all_day_event);
  RUN_TEST(test_past_event_filtered);
  RUN_TEST(test_event_beyond_window_filtered);
  RUN_TEST(test_in_progress_grace_window);
  RUN_TEST(test_max_events_trimmed_keeps_earliest);
  RUN_TEST(test_events_sorted_by_start);
  RUN_TEST(test_many_vevents_all_seen);
  RUN_TEST(test_folded_continuation_lines);
  RUN_TEST(test_lf_only_line_endings);
  RUN_TEST(test_valarm_inside_vevent_does_not_swallow_end);
  RUN_TEST(test_dtstart_with_tzid_parsed_as_local);
  RUN_TEST(test_real_fixture_all_vevents_seen);
  RUN_TEST(test_real_fixture_with_production_config);
  return UNITY_END();
}
