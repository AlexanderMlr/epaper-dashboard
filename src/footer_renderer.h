#pragma once

#include <Arduino.h>

#include <time.h>

class FooterRenderer {
 public:
  explicit FooterRenderer(uint8_t* framebuffer);
  void draw(const struct tm& now, uint64_t sleepUs, float batteryVoltage,
            int batteryPercent);

 private:
  uint8_t* framebuffer_;
};
