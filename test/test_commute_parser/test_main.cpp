#include <unity.h>

#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>

#include "commute/commute_parser.h"

#ifndef COMMUTE_FIXTURE_DIR
#define COMMUTE_FIXTURE_DIR "test/test_commute_parser/fixtures"
#endif

namespace {

std::string readFixture(const char* name) {
  std::ifstream f(std::string(COMMUTE_FIXTURE_DIR) + "/" + name);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

std::vector<CommuteRoute> parsePlan(const std::string& json) {
  JsonDocument filter;
  buildPlanFilter(filter);
  JsonDocument doc;
  DeserializationError err =
      deserializeJson(doc, json, DeserializationOption::Filter(filter));
  TEST_ASSERT_EQUAL_STRING("Ok", err.c_str());
  return routesFromPlan(doc);
}

}  // namespace

void setUp() {}
void tearDown() {}

// --- parseUtcIso ---

void test_parse_utc_iso_epoch_values() {
  TEST_ASSERT_EQUAL(0, parseUtcIso("1970-01-01T00:00:00Z"));
  TEST_ASSERT_EQUAL(1783886280, parseUtcIso("2026-07-12T19:58:00Z"));
  TEST_ASSERT_EQUAL(951827696, parseUtcIso("2000-02-29T12:34:56Z"));  // leap
  TEST_ASSERT_EQUAL(1768464300, parseUtcIso("2026-01-15T08:05:00Z"));
}

void test_parse_utc_iso_invalid() {
  TEST_ASSERT_EQUAL(0, parseUtcIso(nullptr));
  TEST_ASSERT_EQUAL(0, parseUtcIso(""));
  TEST_ASSERT_EQUAL(0, parseUtcIso("garbage"));
  TEST_ASSERT_EQUAL(0, parseUtcIso("2026-07-12"));  // date only
  TEST_ASSERT_EQUAL(0, parseUtcIso("1969-12-31T23:59:59Z"));
}

// --- toLocalHHMM (TZ fixed to Europe/Berlin in main) ---

void test_to_local_hhmm_dst() {
  // 19:58 UTC in July = 21:58 CEST
  TEST_ASSERT_EQUAL_STRING("21:58",
                           toLocalHHMM(parseUtcIso("2026-07-12T19:58:00Z")).c_str());
}

void test_to_local_hhmm_winter() {
  // 08:05 UTC in January = 09:05 CET
  TEST_ASSERT_EQUAL_STRING("09:05",
                           toLocalHHMM(parseUtcIso("2026-01-15T08:05:00Z")).c_str());
}

void test_to_local_hhmm_invalid() {
  TEST_ASSERT_EQUAL_STRING("", toLocalHHMM(0).c_str());
  TEST_ASSERT_EQUAL_STRING("", toLocalHHMM(-1).c_str());
}

// --- routesFromPlan with captured live response ---

void test_fixture_route_count_and_shape() {
  auto routes = parsePlan(readFixture("plan.json"));
  TEST_ASSERT_EQUAL(5, routes.size());
  const size_t expectedSegments[] = {2, 1, 2, 2, 2};  // walk legs dropped
  const int expectedTransfers[] = {1, 0, 1, 1, 1};
  for (size_t i = 0; i < routes.size(); i++) {
    TEST_ASSERT_EQUAL(expectedSegments[i], routes[i].segments.size());
    TEST_ASSERT_EQUAL(expectedTransfers[i], routes[i].transfers);
    TEST_ASSERT_FALSE(routes[i].cancelled);
  }
}

void test_fixture_first_route() {
  auto routes = parsePlan(readFixture("plan.json"));
  const CommuteRoute& r = routes[0];
  // 22:41Z on Jul 12 = 00:41 CEST on Jul 13: crosses the date boundary
  TEST_ASSERT_EQUAL_STRING("00:41", r.departureTime.c_str());
  TEST_ASSERT_EQUAL_STRING("01:00", r.arrivalTime.c_str());  // 23:00Z
  TEST_ASSERT_EQUAL(19, r.durationMinutes);
  TEST_ASSERT_EQUAL(0, r.departureDelay);

  TEST_ASSERT_EQUAL_STRING("S5", r.segments[0].trainLine.c_str());
  TEST_ASSERT_EQUAL_STRING("METRO", r.segments[0].trainCategory.c_str());
  TEST_ASSERT_EQUAL_STRING("15", r.segments[0].platform.c_str());
  TEST_ASSERT_EQUAL_STRING("U8", r.segments[1].trainLine.c_str());
  TEST_ASSERT_EQUAL_STRING("1 (U8)", r.segments[1].platform.c_str());
  TEST_ASSERT_EQUAL(0, r.segments[1].delayMinutes);
}

void test_fixture_missing_track() {
  auto routes = parsePlan(readFixture("plan.json"));
  // itinerary 1's M41 bus leg has no track field in the captured response
  TEST_ASSERT_EQUAL_STRING("", routes[1].segments[0].platform.c_str());
}

// --- routesFromPlan edge cases ---

void test_empty_response() {
  TEST_ASSERT_EQUAL(0, parsePlan("{}").size());
  TEST_ASSERT_EQUAL(0, parsePlan("{\"itineraries\":[]}").size());
}

void test_walk_only_itinerary_dropped() {
  const char* json = R"({"itineraries":[{"duration":600,"transfers":0,"legs":[
      {"mode":"WALK","startTime":"2026-07-12T10:00:00Z",
       "endTime":"2026-07-12T10:10:00Z"}]}]})";
  TEST_ASSERT_EQUAL(0, parsePlan(json).size());
}

void test_cancelled_leg_marks_route() {
  const char* json = R"({"itineraries":[{"duration":1200,"transfers":0,"legs":[
      {"mode":"METRO","routeShortName":"S1","cancelled":true,
       "startTime":"2026-07-12T10:00:00Z",
       "scheduledStartTime":"2026-07-12T10:00:00Z",
       "endTime":"2026-07-12T10:20:00Z",
       "from":{"name":"A","track":"1"},"to":{"name":"B"}}]}]})";
  auto routes = parsePlan(json);
  TEST_ASSERT_EQUAL(1, routes.size());
  TEST_ASSERT_TRUE(routes[0].cancelled);
}

void test_negative_delay() {
  const char* json = R"({"itineraries":[{"duration":1200,"transfers":0,"legs":[
      {"mode":"BUS","routeShortName":"103",
       "startTime":"2026-07-12T09:58:00Z",
       "scheduledStartTime":"2026-07-12T10:00:00Z",
       "endTime":"2026-07-12T10:20:00Z",
       "from":{"name":"A"},"to":{"name":"B"}}]}]})";
  auto routes = parsePlan(json);
  TEST_ASSERT_EQUAL(1, routes.size());
  TEST_ASSERT_EQUAL(-2, routes[0].departureDelay);
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);  // Europe/Berlin, no tzdata dep
  tzset();

  UNITY_BEGIN();
  RUN_TEST(test_parse_utc_iso_epoch_values);
  RUN_TEST(test_parse_utc_iso_invalid);
  RUN_TEST(test_to_local_hhmm_dst);
  RUN_TEST(test_to_local_hhmm_winter);
  RUN_TEST(test_to_local_hhmm_invalid);
  RUN_TEST(test_fixture_route_count_and_shape);
  RUN_TEST(test_fixture_first_route);
  RUN_TEST(test_fixture_missing_track);
  RUN_TEST(test_empty_response);
  RUN_TEST(test_walk_only_itinerary_dropped);
  RUN_TEST(test_cancelled_leg_marks_route);
  RUN_TEST(test_negative_delay);
  return UNITY_END();
}
