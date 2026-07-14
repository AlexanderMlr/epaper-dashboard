#include "commute_service.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "../config.h"
#include "commute_parser.h"

struct PsRamAllocator : ArduinoJson::Allocator {
  void* allocate(size_t size) override { return ps_malloc(size); }
  void deallocate(void* ptr) override { free(ptr); }
  void* reallocate(void* ptr, size_t new_size) override {
    return ps_realloc(ptr, new_size);
  }
};

static PsRamAllocator psRamAllocator;

namespace {
const int COMMUTE_NUM_ROUTES = 5;  // limited to 5 due to screen size

const time_t kMinValidEpoch = 1577836800;  // 2020-01-01; below = clock unset

// Transitous asks users to identify via User-Agent: https://transitous.org/api/
const char* USER_AGENT =
    "epaper-dashboard/0.1 (+https://github.com/AlexanderMlr/epaper-dashboard)";
}  // namespace

String CommuteService::buildRequestUrl() const {
  // Transitous: community-run MOTIS routing, worldwide GTFS coverage.
  String url = "https://api.transitous.org/api/v1/plan?";
  url += "fromPlace=" + String(COMMUTE_START_STOP_ID);
  url += "&toPlace=" + String(COMMUTE_DEST_STOP_ID);
  url += "&numItineraries=" + String(COMMUTE_NUM_ROUTES);

  time_t now = time(nullptr);
  if (now >= kMinValidEpoch) {  // only add time param once NTP has synced
    time_t when = now + COMMUTE_DEPARTURE_OFFSET_MIN * 60;
    struct tm whenTm;
    gmtime_r(&when, &whenTm);
    char buf[24];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &whenTm);
    url += "&time=" + String(buf);
  }

  return url;
}

std::vector<CommuteRoute> CommuteService::fetchRoutes() {
  std::vector<CommuteRoute> routes;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.useHTTP10(true);
  String url = buildRequestUrl();
  http.begin(client, url);
  http.setTimeout(15000);
  http.setUserAgent(USER_AGENT);

  Serial.println("Fetching commute data...");
  Serial.printf("URL: %s\n", url.c_str());
  int httpCode = http.GET();
  Serial.printf("Commute API response code: %d\n", httpCode);

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Commute HTTP request failed: %d\n", httpCode);
    http.end();
    return routes;
  }

  JsonDocument filter(&psRamAllocator);
  buildPlanFilter(filter);

  JsonDocument doc(&psRamAllocator);
  DeserializationError err = deserializeJson(
      doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (err) {
    Serial.printf("Commute JSON parse failed: %s\n", err.c_str());
    return routes;
  }

  routes = routesFromPlan(doc);
  Serial.printf("Parsed %zu commute routes\n", routes.size());
  return routes;
}
