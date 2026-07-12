#include "commute_renderer.h"

#include "../config.h"
#include "firasans.h"

CommuteRenderer::CommuteRenderer(uint8_t* framebuffer, int originX,
                                 int originY, int width, int height)
    : framebuffer_(framebuffer),
      originX_(originX),
      originY_(originY),
      width_(width),
      height_(height) {}

namespace {
constexpr int TITLE_Y = 50;
constexpr int ROUTE_START_Y = 105;
constexpr int INNER_LINE_DIST = 35;
constexpr int ROUTE_SPACING = 80;

// Fixed columns on line 1 keep "->" aligned across rows regardless of delay.
constexpr int COL_DEP = 10;
constexpr int COL_DELAY = 130;
constexpr int COL_ARROW = 195;
constexpr int COL_ARR = 255;
constexpr int COL_DUR = 365;
}  // namespace

void CommuteRenderer::drawHeader() {
  int32_t x = originX_ + 10;
  int32_t y = originY_ + TITLE_Y;
  write_string((GFXfont*)&FiraSans, "Commute", &x, &y, framebuffer_);
}

void CommuteRenderer::drawRoute(const CommuteRoute& route, int y) {
  const int32_t baseY = originY_ + y;
  int32_t x, textY;

  x = originX_ + COL_DEP;
  textY = baseY;
  write_string((GFXfont*)&FiraSans, route.departureTime.c_str(), &x, &textY,
               framebuffer_);

  if (route.departureDelay > 0) {
    x = originX_ + COL_DELAY;
    textY = baseY;
    String delay = "+" + String(route.departureDelay);
    write_string((GFXfont*)&FiraSans, delay.c_str(), &x, &textY, framebuffer_);
  }

  x = originX_ + COL_ARROW;
  textY = baseY;
  write_string((GFXfont*)&FiraSans, "->", &x, &textY, framebuffer_);

  x = originX_ + COL_ARR;
  textY = baseY;
  write_string((GFXfont*)&FiraSans, route.arrivalTime.c_str(), &x, &textY,
               framebuffer_);

  x = originX_ + COL_DUR;
  textY = baseY;
  String dur = String(route.durationMinutes) + "m";
  write_string((GFXfont*)&FiraSans, dur.c_str(), &x, &textY, framebuffer_);

  String lines;
  if (route.cancelled) lines = "X ";
  for (size_t i = 0; i < route.segments.size(); i++) {
    if (i > 0) lines += " + ";
    lines += route.segments[i].trainLine.c_str();
  }
  x = originX_ + 30;
  textY = baseY + INNER_LINE_DIST;
  write_string((GFXfont*)&FiraSans, lines.c_str(), &x, &textY, framebuffer_);
}

void CommuteRenderer::draw(const std::vector<CommuteRoute>& routes,
                           bool inQuietHours) {
  if (inQuietHours) {
    int32_t x = originX_ + 10;
    int32_t y = originY_ + TITLE_Y;
    write_string((GFXfont*)&FiraSans, "Commute", &x, &y, framebuffer_);

    x = originX_ + 10;
    y = originY_ + ROUTE_START_Y;
    write_string((GFXfont*)&FiraSans, "Outside commute hours", &x, &y,
                 framebuffer_);
    return;
  }

  if (routes.empty()) {
    int32_t x = originX_ + 10;
    int32_t y = originY_ + 100;
    write_string((GFXfont*)&FiraSans, "Commute data unavailable", &x, &y,
                 framebuffer_);
    return;
  }

  drawHeader();

  for (size_t i = 0; i < routes.size(); i++) {
    drawRoute(routes[i], ROUTE_START_Y + (int)i * ROUTE_SPACING);
  }
}
