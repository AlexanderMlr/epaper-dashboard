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

String CommuteService::extractTime(const String& isoDatetime) {
  // "2026-04-18T08:58:00+02:00" -> "08:58"
  int tPos = isoDatetime.indexOf('T');
  if (tPos < 0 || tPos + 6 > (int)isoDatetime.length()) return isoDatetime;
  return isoDatetime.substring(tPos + 1, tPos + 6);
}

static int minutesFromIso(const char* iso) {
  // Extract minute-of-day from "YYYY-MM-DDTHH:MM:..."
  if (!iso || strlen(iso) < 16) return -1;
  int h = (iso[11] - '0') * 10 + (iso[12] - '0');
  int m = (iso[14] - '0') * 10 + (iso[15] - '0');
  if (h < 0 || h > 23 || m < 0 || m > 59) return -1;
  return h * 60 + m;
}

String CommuteService::buildRequestUrl() const {
  // transport.rest v6: clean REST wrapper around DB HAFAS.
  // Query flags strip stopovers, tickets, polylines, remarks and related
  // decorations to keep the response small enough for the ESP32.
  String url = "https://v6.db.transport.rest/journeys?";
  url += "from=" + String(COMMUTE_START_EVA);
  url += "&to=" + String(COMMUTE_DEST_EVA);
  url += "&results=" + String(COMMUTE_NUM_ROUTES);
  url += "&stopovers=false&tickets=false&polylines=false";
  url += "&remarks=false&subStops=false&entrances=false&linesOfStops=false";
  url += "&pretty=false";
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

  Serial.println("Fetching commute data...");
  Serial.printf("URL: %s\n", url.c_str());
  int httpCode = http.GET();
  Serial.printf("Commute API response code: %d\n", httpCode);

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Commute HTTP request failed: %d\n", httpCode);
    http.end();
    return routes;
  }

  // Filter restricts parsing to just the leg fields the renderer uses.
  JsonDocument filter(&psRamAllocator);
  JsonObject legFilter =
      filter["journeys"][0]["legs"][0].to<JsonObject>();
  legFilter["departure"] = true;
  legFilter["plannedDeparture"] = true;
  legFilter["arrival"] = true;
  legFilter["plannedArrival"] = true;
  legFilter["departurePlatform"] = true;
  legFilter["plannedDeparturePlatform"] = true;
  legFilter["cancelled"] = true;
  legFilter["walking"] = true;
  JsonObject lineFilter = legFilter["line"].to<JsonObject>();
  lineFilter["name"] = true;
  lineFilter["productName"] = true;
  legFilter["origin"]["name"] = true;
  legFilter["destination"]["name"] = true;

  JsonDocument doc(&psRamAllocator);
  DeserializationError err = deserializeJson(
      doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();

  if (err) {
    Serial.printf("Commute JSON parse failed: %s\n", err.c_str());
    return routes;
  }

  JsonArray journeys = doc["journeys"].as<JsonArray>();
  Serial.printf("Received %u journeys\n", (unsigned)journeys.size());

  for (JsonObject journey : journeys) {
    JsonArray legs = journey["legs"].as<JsonArray>();
    if (legs.size() == 0) continue;

    CommuteRoute route;

    for (JsonObject legObj : legs) {
      if (legObj["walking"] | false) continue;  // skip footpath legs

      CommuteSegment seg;
      const char* dep = legObj["departure"] | "";
      const char* depPlan = legObj["plannedDeparture"] | "";
      const char* arr = legObj["arrival"] | "";
      const char* arrPlan = legObj["plannedArrival"] | "";
      seg.departureTime = extractTime(dep[0] ? dep : depPlan);
      seg.arrivalTime = extractTime(arr[0] ? arr : arrPlan);

      int depMin = minutesFromIso(dep);
      int planMin = minutesFromIso(depPlan);
      if (depMin >= 0 && planMin >= 0) {
        seg.delayMinutes = depMin - planMin;
      }

      seg.trainLine = legObj["line"]["name"] | "";
      seg.trainCategory = legObj["line"]["productName"] | "";
      seg.origin = legObj["origin"]["name"] | "";
      seg.destination = legObj["destination"]["name"] | "";
      const char* platform =
          legObj["departurePlatform"]
              | (legObj["plannedDeparturePlatform"] | "");
      seg.platform = platform;

      if (legObj["cancelled"] | false) route.cancelled = true;

      route.segments.push_back(seg);
    }

    if (route.segments.empty()) continue;

    route.transfers = (int)route.segments.size() - 1;
    route.departureTime = route.segments.front().departureTime;
    route.arrivalTime = route.segments.back().arrivalTime;
    route.departureDelay = route.segments.front().delayMinutes;

    const char* firstDep =
        legs[0]["departure"] | (legs[0]["plannedDeparture"] | "");
    int lastIdx = (int)legs.size() - 1;
    const char* lastArr = legs[lastIdx]["arrival"]
                              | (legs[lastIdx]["plannedArrival"] | "");
    int start = minutesFromIso(firstDep);
    int end = minutesFromIso(lastArr);
    if (start >= 0 && end >= 0) {
      int dur = end - start;
      if (dur < 0) dur += 24 * 60;  // route crosses midnight
      route.durationMinutes = dur;
    }

    routes.push_back(route);
  }

  Serial.printf("Parsed %zu commute routes\n", routes.size());
  return routes;
}
