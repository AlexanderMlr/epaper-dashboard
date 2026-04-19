#pragma once

#include <Arduino.h>

void setPixel4bit(uint8_t* framebuffer, int x, int y, uint8_t grayValue);
void drawGrayscaleBitmap(uint8_t* framebuffer, const uint8_t* bitmap, int x,
                         int y, int width, int height);

class DisplayManager {
 public:
  DisplayManager();
  ~DisplayManager();

  bool initialize();
  void clear();
  void refresh();
  uint8_t* getFramebuffer() { return framebuffer_; }

 private:
  uint8_t* framebuffer_;
};
