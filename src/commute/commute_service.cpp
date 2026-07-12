#include "commute_service.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "../config.h"

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

// Transitous asks API users to identify themselves via User-Agent:
// https://transitous.org/api/
const char* USER_AGENT = "epaper-dashboard/1.0";

time_t parseUtcIso(const char* iso) {
  // "2026-07-12T19:58:00Z" (UTC) -> unix epoch. newlib has no timegm(), so
  // convert tm via days-from-civil
  // (https://howardhinnant.github.io/date_algorithms.html)
  struct tm t = {};
  if (!iso || !strptime(iso, "%Y-%m-%dT%H:%M:%S", &t)) return 0;
  int y = t.tm_year + 1900;
  int mo = t.tm_mon + 1;
  if (y < 1970) return 0;
  int a = (mo <= 2) ? y - 1 : y;
  int era = a / 400;
  int yoe = a - era * 400;
  int doy = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + t.tm_mday - 1;
  int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  long days = (long)era * 146097 + doe - 719468;
  return (time_t)days * 86400 + t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
}

String toLocalHHMM(time_t t) {
  if (t <= 0) return "";
  struct tm tmLocal;
  localtime_r(&t, &tmLocal);
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", tmLocal.tm_hour, tmLocal.tm_min);
  return String(buf);
}
}  // namespace

String CommuteService::buildRequestUrl() const {
  // Transitous: community-run MOTIS routing, worldwide GTFS coverage.
  // All timestamps in the API are UTC.
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

  // Filter restricts parsing to just the fields the renderer uses; drops
  // legGeometry and intermediateStops, which dominate the response size.
  JsonDocument filter(&psRamAllocator);
  JsonObject itineraryFilter = filter["itineraries"][0].to<JsonObject>();
  itineraryFilter["duration"] = true;
  itineraryFilter["transfers"] = true;
  JsonObject legFilter = itineraryFilter["legs"][0].to<JsonObject>();
  legFilter["mode"] = true;
  legFilter["routeShortName"] = true;
  legFilter["startTime"] = true;
  legFilter["scheduledStartTime"] = true;
  legFilter["endTime"] = true;
  legFilter["scheduledEndTime"] = true;
  legFilter["cancelled"] = true;
  legFilter["from"]["name"] = true;
  legFilter["from"]["track"] = true;
  legFilter["from"]["scheduledTrack"] = true;
  legFilter["to"]["name"] = true;

  JsonDocument doc(&psRamAllocator);
  DeserializationError err = deserializeJson(
      doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (err) {
    Serial.printf("Commute JSON parse failed: %s\n", err.c_str());
    return routes;
  }

  JsonArray itineraries = doc["itineraries"].as<JsonArray>();
  Serial.printf("Received %u itineraries\n", (unsigned)itineraries.size());

  for (JsonObject itinerary : itineraries) {
    CommuteRoute route;

    for (JsonObject legObj : itinerary["legs"].as<JsonArray>()) {
      const char* mode = legObj["mode"] | "";
      if (strcmp(mode, "WALK") == 0) continue;

      CommuteSegment seg;
      time_t dep = parseUtcIso(legObj["startTime"] | "");
      time_t depPlanned = parseUtcIso(legObj["scheduledStartTime"] | "");
      time_t arr = parseUtcIso(legObj["endTime"] | "");
      seg.departureTime = toLocalHHMM(dep);
      seg.arrivalTime = toLocalHHMM(arr);
      if (dep > 0 && depPlanned > 0) {
        seg.delayMinutes = (int)((dep - depPlanned) / 60);
      }

      seg.trainLine = legObj["routeShortName"] | "";
      seg.trainCategory = mode;
      seg.origin = legObj["from"]["name"] | "";
      seg.destination = legObj["to"]["name"] | "";
      seg.platform =
          legObj["from"]["track"] | (legObj["from"]["scheduledTrack"] | "");

      if (legObj["cancelled"] | false) route.cancelled = true;

      route.segments.push_back(seg);
    }

    if (route.segments.empty()) continue;

    route.transfers = itinerary["transfers"] | 0;
    route.durationMinutes = (itinerary["duration"] | 0) / 60;
    route.departureTime = route.segments.front().departureTime;
    route.arrivalTime = route.segments.back().arrivalTime;
    route.departureDelay = route.segments.front().delayMinutes;

    routes.push_back(route);
  }

  Serial.printf("Parsed %zu commute routes\n", routes.size());
  return routes;
}
