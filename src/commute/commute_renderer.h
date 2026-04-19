#pragma once

#include <Arduino.h>

#include <vector>

#include "commute_data.h"

class CommuteRenderer {
 public:
  CommuteRenderer(uint8_t* framebuffer, int originX, int originY, int width,
                  int height);
  void draw(const std::vector<CommuteRoute>& routes, bool inQuietHours);

 private:
  uint8_t* framebuffer_;
  int originX_;
  int originY_;
  int width_;
  int height_;

  void drawHeader();
  void drawRoute(const CommuteRoute& route, int y);
};