#pragma once

#include <Arduino.h>

#include <vector>

#include "calendar_data.h"

class CalendarRenderer {
 public:
  CalendarRenderer(uint8_t* framebuffer, int originX, int originY, int width,
                   int height);
  void draw(const std::vector<CalendarEvent>& events);

 private:
  uint8_t* framebuffer_;
  int originX_;
  int originY_;
  int width_;
  int height_;

  void drawHeader();
  void drawEvent(const CalendarEvent& ev, int y, bool sameDayAsPrev);
};
