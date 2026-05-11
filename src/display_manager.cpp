#include "display_manager.h"

#include "config.h"
#include "epd_driver.h"

namespace {
const int kFramebufferSize = EPD_WIDTH * EPD_HEIGHT / 2;
}

void setPixel4bit(uint8_t* framebuffer, int x, int y, uint8_t grayValue) {
  if (x < 0 || x >= EPD_WIDTH || y < 0 || y >= EPD_HEIGHT) {
    return;
  }

  const int pixelIndex = y * EPD_WIDTH + x;
  const int byteIndex = pixelIndex / 2;
  const bool isOddPixel = (pixelIndex % 2) == 1;

  grayValue = constrain(grayValue, 0, 15);

  if (isOddPixel) {
    framebuffer[byteIndex] = (framebuffer[byteIndex] & 0xF0) | grayValue;
  } else {
    framebuffer[byteIndex] = (framebuffer[byteIndex] & 0x0F) | (grayValue << 4);
  }
}

void drawGrayscaleBitmap(uint8_t* framebuffer, const uint8_t* bitmap, int x,
                         int y, int width, int height) {
  for (int row = 0; row < height; row++) {
    for (int col = 0; col < width; col++) {
      const int srcIndex = (row * width + col) / 2;
      const bool isOddPixel = ((row * width + col) % 2) == 1;

      uint8_t grayValue;
      if (isOddPixel) {
        grayValue = bitmap[srcIndex] & 0x0F;
      } else {
        grayValue = (bitmap[srcIndex] & 0xF0) >> 4;
      }

      setPixel4bit(framebuffer, x + col, y + row, grayValue);
    }
  }
}

DisplayManager::DisplayManager() : framebuffer_(nullptr) {}

DisplayManager::~DisplayManager() {
  if (framebuffer_) {
    free(framebuffer_);
  }
}

bool DisplayManager::initialize() {
  framebuffer_ = (uint8_t*)ps_calloc(sizeof(uint8_t), kFramebufferSize);

  if (!framebuffer_) {
    Serial.println("Failed to allocate framebuffer memory!");
    return false;
  }

  epd_init();

  return true;
}

void DisplayManager::clear() {
  if (framebuffer_) {
    memset(framebuffer_, 0xFF, kFramebufferSize);
  }
}

void DisplayManager::refresh() {
  epd_poweron();
  epd_clear();
  epd_draw_grayscale_image(epd_full_screen(), framebuffer_);
  epd_poweroff_all();
}
