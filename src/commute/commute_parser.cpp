#include "commute_parser.h"

#include <cstdio>
#include <cstring>

time_t parseUtcIso(const char* iso) {
  // newlib has no timegm(), so convert tm via days-from-civil
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

std::string toLocalHHMM(time_t t) {
  if (t <= 0) return "";
  struct tm tmLocal;
  localtime_r(&t, &tmLocal);
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", tmLocal.tm_hour, tmLocal.tm_min);
  return std::string(buf);
}

void buildPlanFilter(JsonDocument& filter) {
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
}

std::vector<CommuteRoute> routesFromPlan(JsonVariantConst doc) {
  std::vector<CommuteRoute> routes;

  for (JsonObjectConst itinerary : doc["itineraries"].as<JsonArrayConst>()) {
    CommuteRoute route;

    for (JsonObjectConst legObj : itinerary["legs"].as<JsonArrayConst>()) {
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

  return routes;
}
