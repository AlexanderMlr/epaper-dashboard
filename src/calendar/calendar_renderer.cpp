#include "calendar_renderer.h"

#include <algorithm>

#include "../config.h"
#include "firasans.h"

CalendarRenderer::CalendarRenderer(uint8_t* framebuffer, int originX,
                                   int originY, int width, int height)
    : framebuffer_(framebuffer),
      originX_(originX),
      originY_(originY),
      width_(width),
      height_(height) {}

namespace {
constexpr int TITLE_Y = 50;
constexpr int EVENT_START_Y = 105;
constexpr int INNER_LINE_DIST = 35;
constexpr int EVENT_SPACING = 80;
constexpr size_t MAX_DISPLAYED_EVENTS = 5;
constexpr int COL_DATE = 10;
constexpr int COL_TIME = 180;
constexpr size_t SUMMARY_MAX_CHARS = 28;

String formatDate(const struct tm& t) {
  // "Mi 29.04"
  static const char* DAYS[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
  char buf[16];
  snprintf(buf, sizeof(buf), "%s %02d.%02d", DAYS[t.tm_wday], t.tm_mday,
           t.tm_mon + 1);
  return String(buf);
}

String formatTime(const struct tm& t) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
  return String(buf);
}
}  // namespace

void CalendarRenderer::drawHeader() {
  int32_t x = originX_ + 10;
  int32_t y = originY_ + TITLE_Y;
  write_string((GFXfont*)&FiraSans, "Calendar", &x, &y, framebuffer_);
}

void CalendarRenderer::drawEvent(const CalendarEvent& ev, int y,
                                 bool sameDayAsPrev) {
  const int32_t baseY = originY_ + y;
  int32_t x, textY;

  struct tm local;
  localtime_r(&ev.start, &local);

  if (!sameDayAsPrev) {
    x = originX_ + COL_DATE;
    textY = baseY;
    String date = formatDate(local);
    write_string((GFXfont*)&FiraSans, date.c_str(), &x, &textY, framebuffer_);
  }

  x = originX_ + COL_TIME;
  textY = baseY;
  String timeStr = ev.allDay ? String("all-day") : formatTime(local);
  write_string((GFXfont*)&FiraSans, timeStr.c_str(), &x, &textY, framebuffer_);

  String summary = ev.summary.length() ? ev.summary : String("(no title)");
  if (summary.length() > SUMMARY_MAX_CHARS) {
    summary = summary.substring(0, SUMMARY_MAX_CHARS - 1) + String("…");
  }
  x = originX_ + COL_DATE;
  textY = baseY + INNER_LINE_DIST;
  write_string((GFXfont*)&FiraSans, summary.c_str(), &x, &textY, framebuffer_);
}

void CalendarRenderer::draw(const std::vector<CalendarEvent>& events) {
  drawHeader();

  if (events.empty()) {
    int32_t x = originX_ + 10;
    int32_t y = originY_ + EVENT_START_Y;
    write_string((GFXfont*)&FiraSans, "No upcoming events", &x, &y,
                 framebuffer_);
    return;
  }

  const size_t n = std::min(events.size(), MAX_DISPLAYED_EVENTS);
  int prevYday = -1, prevYear = -1;
  for (size_t i = 0; i < n; i++) {
    struct tm local;
    localtime_r(&events[i].start, &local);
    bool sameDay =
        (local.tm_yday == prevYday) && (local.tm_year == prevYear);
    drawEvent(events[i], EVENT_START_Y + (int)i * EVENT_SPACING, sameDay);
    prevYday = local.tm_yday;
    prevYear = local.tm_year;
  }
}
