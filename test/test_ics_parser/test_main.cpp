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
                   const std::string& dtend = "") {
  std::string s = "BEGIN:VEVENT\r\n";
  s += "UID:" + summary + "@test\r\n";
  s += "SUMMARY:" + summary + "\r\n";
  s += "DTSTART:" + dtstart + "\r\n";
  if (!dtend.empty()) s += "DTEND:" + dtend + "\r\n";
  s += "END:VEVENT\r\n";
  return s;
}

std::string slurpFile(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return std::string();
  std::ostringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

// Loads the real.ics fixture or returns empty if missing. Tests should call
// TEST_IGNORE_MESSAGE on an empty result so CI without the fixture still
// passes.
std::string loadRealFixture() {
  return slurpFile(std::string(FIXTURE_DIR) + "/real.ics");
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

// Asserts the parser handles a body with hundreds of VEVENTs without losing
// any to buffer state, off-by-ones, or accumulator bugs. Events are spread
// across years so most fall outside the lookahead window — we only check
// that every BEGIN/END pair is seen, not that they're kept.
void test_many_vevents_all_seen() {
  std::string body;
  for (int i = 0; i < 506; i++) {
    char dt[20];
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

// Runs the parser against the real fixture with the same config the device
// uses, and asserts events come out. Catches regressions where production
// config silently filters everything (e.g. window math, sort/trim bugs).
void test_real_fixture_with_production_config() {
  std::string body = loadRealFixture();
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
  std::string body = loadRealFixture();
  if (body.empty()) {
    TEST_IGNORE_MESSAGE("real.ics fixture not present; skipping");
    return;
  }
  IcsParseStats stats;
  parseIcs(body.c_str(), body.size(), kNow, 14, 1000, &stats);
  printf("[real.ics] bytes=%zu BEGIN=%d END=%d no-dtstart=%d "
         "parse-fail=%d outside-window=%d\n",
         body.size(), stats.beginVevent, stats.endVevent, stats.noDtstart,
         stats.parseFail, stats.outsideWindow);
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

// Daily standup that started a year ago should yield ~14 occurrences for the
// 14-day window, not just one.
void test_rrule_daily_expanded_in_window() {
  std::string ics = wrap(
      "BEGIN:VEVENT\r\nUID:d@t\r\nSUMMARY:Standup\r\n"
      "DTSTART:20250101T080000Z\r\nDTEND:20250101T083000Z\r\n"
      "RRULE:FREQ=DAILY\r\nEND:VEVENT\r\n");
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 100);
  TEST_ASSERT_EQUAL(14, (int)out.size());
  TEST_ASSERT_EQUAL(1800, (int)(out[0].end - out[0].start));
}

// MO/WE/FR meeting starting before the window should fire 6 times in 14 days.
void test_rrule_weekly_byday_filter() {
  // 2026-05-04 is a Monday.
  std::string ics = wrap(
      "BEGIN:VEVENT\r\nUID:w@t\r\nSUMMARY:Sync\r\n"
      "DTSTART:20260105T140000Z\r\nDTEND:20260105T143000Z\r\n"
      "RRULE:FREQ=WEEKLY;BYDAY=MO,WE,FR\r\nEND:VEVENT\r\n");
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 100);
  TEST_ASSERT_EQUAL(6, (int)out.size());
  for (auto& e : out) {
    struct tm tm = *gmtime(&e.start);
    // tm_wday: 1=Mon, 3=Wed, 5=Fri.
    TEST_ASSERT_TRUE(tm.tm_wday == 1 || tm.tm_wday == 3 || tm.tm_wday == 5);
  }
}

// Bi-weekly meeting: occurrences land in alternate weeks anchored to DTSTART.
void test_rrule_weekly_interval2() {
  // DTSTART 2026-04-27 (Mon) is the anchor; window 05-02..05-16. Active weeks
  // step by 2 from the anchor: 04-27, 05-11, 05-25, ... so only 05-11 is
  // inside the window. 05-04 and 05-18 are in skipped weeks / past window.
  std::string ics = wrap(
      "BEGIN:VEVENT\r\nUID:bw@t\r\nSUMMARY:BiWeekly\r\n"
      "DTSTART:20260427T140000Z\r\nDTEND:20260427T150000Z\r\n"
      "RRULE:FREQ=WEEKLY;INTERVAL=2;BYDAY=MO\r\nEND:VEVENT\r\n");
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 100);
  TEST_ASSERT_EQUAL(1, (int)out.size());
  struct tm tm = *gmtime(&out[0].start);
  TEST_ASSERT_EQUAL(2026, tm.tm_year + 1900);
  TEST_ASSERT_EQUAL(5, tm.tm_mon + 1);
  TEST_ASSERT_EQUAL(11, tm.tm_mday);
}

// FREQ=DAILY;INTERVAL=2 must skip every other day, not march daily. Catches
// stepFor() forgetting to multiply the day stride by INTERVAL.
void test_rrule_daily_interval2() {
  // DTSTART 2026-05-03 08:00Z; window 05-02..05-16. Expected hits: 05-03,
  // 05, 07, 09, 11, 13, 15 — seven every-other-day occurrences.
  std::string ics = wrap(
      "BEGIN:VEVENT\r\nUID:di@t\r\nSUMMARY:EveryOther\r\n"
      "DTSTART:20260503T080000Z\r\nDTEND:20260503T083000Z\r\n"
      "RRULE:FREQ=DAILY;INTERVAL=2\r\nEND:VEVENT\r\n");
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 100);
  TEST_ASSERT_EQUAL(7, (int)out.size());
  TEST_ASSERT_EQUAL(2 * 86400, (int)(out[1].start - out[0].start));
}

// UNTIL clips the recurrence mid-window.
void test_rrule_until_clips() {
  std::string ics = wrap(
      "BEGIN:VEVENT\r\nUID:u@t\r\nSUMMARY:Until\r\n"
      "DTSTART:20260503T080000Z\r\n"
      "RRULE:FREQ=DAILY;UNTIL=20260506T080000Z\r\nEND:VEVENT\r\n");
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 100);
  TEST_ASSERT_EQUAL(4, (int)out.size());
}

// EXDATE skips the named instance from the expansion.
void test_rrule_exdate_skips_instance() {
  std::string ics = wrap(
      "BEGIN:VEVENT\r\nUID:e@t\r\nSUMMARY:Daily\r\n"
      "DTSTART:20260503T080000Z\r\nDTEND:20260503T083000Z\r\n"
      "RRULE:FREQ=DAILY;COUNT=5\r\n"
      "EXDATE:20260505T080000Z\r\nEND:VEVENT\r\n");
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 100);
  TEST_ASSERT_EQUAL(4, (int)out.size());
  for (auto& e : out) {
    struct tm tm = *gmtime(&e.start);
    TEST_ASSERT_NOT_EQUAL(5, tm.tm_mday);
  }
}

// All-day weekly recurrence carries the allDay flag through to occurrences.
void test_rrule_all_day_weekly() {
  std::string ics = wrap(
      "BEGIN:VEVENT\r\nUID:ad@t\r\nSUMMARY:Trash\r\n"
      "DTSTART;VALUE=DATE:20260104\r\nDTEND;VALUE=DATE:20260105\r\n"
      "RRULE:FREQ=WEEKLY\r\nEND:VEVENT\r\n");
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 100);
  TEST_ASSERT_EQUAL(2, (int)out.size());
  TEST_ASSERT_TRUE(out[0].allDay);
  TEST_ASSERT_TRUE(out[1].allDay);
}

// Plain weekly recurrence (no BYDAY) repeats on the same weekday as DTSTART.
void test_rrule_weekly_no_byday() {
  // 2026-04-26 is a Sunday; window is 2026-05-02 .. 05-16. Expected hits:
  // 05-03 and 05-10 (both Sundays).
  std::string ics = wrap(
      "BEGIN:VEVENT\r\nUID:ws@t\r\nSUMMARY:Sunday\r\n"
      "DTSTART:20260426T120000Z\r\nDTEND:20260426T130000Z\r\n"
      "RRULE:FREQ=WEEKLY\r\nEND:VEVENT\r\n");
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 100);
  TEST_ASSERT_EQUAL(2, (int)out.size());
  for (auto& e : out) {
    struct tm tm = *gmtime(&e.start);
    TEST_ASSERT_EQUAL(0, tm.tm_wday);  // Sunday
  }
}

// Yearly recurrence from years ago must still produce the current-year hit
// when its anniversary falls inside the window.
void test_rrule_yearly_birthday_in_window() {
  // Birthday 2020-05-09; window 05-02..05-16 includes 2026-05-09.
  std::string ics = wrap(
      "BEGIN:VEVENT\r\nUID:y@t\r\nSUMMARY:Birthday\r\n"
      "DTSTART;VALUE=DATE:20200509\r\nDTEND;VALUE=DATE:20200510\r\n"
      "RRULE:FREQ=YEARLY\r\nEND:VEVENT\r\n");
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 100);
  TEST_ASSERT_EQUAL(1, (int)out.size());
  TEST_ASSERT_TRUE(out[0].allDay);
  struct tm tm = *gmtime(&out[0].start);
  TEST_ASSERT_EQUAL(2026, tm.tm_year + 1900);
  TEST_ASSERT_EQUAL(5, tm.tm_mon + 1);
  TEST_ASSERT_EQUAL(9, tm.tm_mday);
}

// Yearly recurrence whose anniversary is outside the 14-day window emits
// nothing (and doesn't run away iterating).
void test_rrule_yearly_outside_window() {
  std::string ics = wrap(
      "BEGIN:VEVENT\r\nUID:y2@t\r\nSUMMARY:Xmas\r\n"
      "DTSTART;VALUE=DATE:20201225\r\nDTEND;VALUE=DATE:20201226\r\n"
      "RRULE:FREQ=YEARLY\r\nEND:VEVENT\r\n");
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 100);
  TEST_ASSERT_EQUAL(0, (int)out.size());
}

// A daily recurrence anchored well over 13 years before kNow exhausts the
// 5000-iteration safety cap before reaching the window. The cap should be
// observable via stats rather than failing silently.
void test_rrule_iteration_cap_observable() {
  std::string ics = wrap(
      "BEGIN:VEVENT\r\nUID:cap@t\r\nSUMMARY:Old\r\n"
      "DTSTART:20100101T080000Z\r\nDTEND:20100101T083000Z\r\n"
      "RRULE:FREQ=DAILY\r\nEND:VEVENT\r\n");
  IcsParseStats stats;
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 100, &stats);
  TEST_ASSERT_EQUAL(1, stats.recurrenceExpanded);
  TEST_ASSERT_EQUAL(1, stats.recurrenceCapped);
  TEST_ASSERT_EQUAL(0, (int)out.size());
}

// outsideWindow tracks single-instance VEVENTs only. A recurring event whose
// every occurrence pre-dates the window must NOT inflate that counter — it
// appears in recurrenceExpanded instead.
void test_stats_separate_single_and_recurring() {
  std::string ics = wrap(
      vevent("Old", "20240101T100000Z") +
      "BEGIN:VEVENT\r\nUID:r@t\r\nSUMMARY:DeadDaily\r\n"
      "DTSTART:20240101T080000Z\r\nRRULE:FREQ=DAILY;COUNT=3\r\n"
      "END:VEVENT\r\n");
  IcsParseStats stats;
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 100, &stats);
  TEST_ASSERT_EQUAL(0, (int)out.size());
  TEST_ASSERT_EQUAL(1, stats.outsideWindow);
  TEST_ASSERT_EQUAL(1, stats.recurrenceExpanded);
  TEST_ASSERT_EQUAL(0, stats.recurrenceCapped);
}

// Floating-local daily recurrence anchored before a DST transition must
// preserve its 09:00 wall-clock time on every occurrence after the spring
// forward. Without DST-aware stepping, occurrences past 03-29 would drift
// by an hour.
void test_rrule_daily_floating_local_preserves_wall_clock_across_dst() {
  setenv("TZ", "Europe/Berlin", 1);
  tzset();
  std::string ics = wrap(
      "BEGIN:VEVENT\r\nUID:dst@t\r\nSUMMARY:WallClock\r\n"
      "DTSTART;TZID=Europe/Berlin:20260325T090000\r\n"
      "RRULE:FREQ=DAILY\r\nEND:VEVENT\r\n");
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 100);
  TEST_ASSERT_GREATER_THAN(0, (int)out.size());
  for (auto& e : out) {
    struct tm tm;
    localtime_r(&e.start, &tm);
    TEST_ASSERT_EQUAL(9, tm.tm_hour);
    TEST_ASSERT_EQUAL(0, tm.tm_min);
  }
}

// Monthly recurrence anchored years ago must still surface this month's hit.
void test_rrule_monthly_hits_window() {
  std::string ics = wrap(
      "BEGIN:VEVENT\r\nUID:m@t\r\nSUMMARY:Monthly\r\n"
      "DTSTART:20240510T120000Z\r\nDTEND:20240510T130000Z\r\n"
      "RRULE:FREQ=MONTHLY\r\nEND:VEVENT\r\n");
  auto out = parseIcs(ics.c_str(), ics.size(), kNow, 14, 100);
  TEST_ASSERT_EQUAL(1, (int)out.size());
  struct tm tm = *gmtime(&out[0].start);
  TEST_ASSERT_EQUAL(2026, tm.tm_year + 1900);
  TEST_ASSERT_EQUAL(5, tm.tm_mon + 1);
  TEST_ASSERT_EQUAL(10, tm.tm_mday);
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
  RUN_TEST(test_rrule_daily_expanded_in_window);
  RUN_TEST(test_rrule_weekly_byday_filter);
  RUN_TEST(test_rrule_weekly_interval2);
  RUN_TEST(test_rrule_daily_interval2);
  RUN_TEST(test_rrule_until_clips);
  RUN_TEST(test_rrule_exdate_skips_instance);
  RUN_TEST(test_rrule_all_day_weekly);
  RUN_TEST(test_rrule_weekly_no_byday);
  RUN_TEST(test_rrule_yearly_birthday_in_window);
  RUN_TEST(test_rrule_yearly_outside_window);
  RUN_TEST(test_rrule_iteration_cap_observable);
  RUN_TEST(test_stats_separate_single_and_recurring);
  RUN_TEST(test_rrule_daily_floating_local_preserves_wall_clock_across_dst);
  RUN_TEST(test_rrule_monthly_hits_window);
  RUN_TEST(test_real_fixture_all_vevents_seen);
  RUN_TEST(test_real_fixture_with_production_config);
  return UNITY_END();
}
