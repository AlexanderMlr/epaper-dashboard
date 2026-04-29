#include "footer_renderer.h"

#include "epd_driver.h"
#include "firasans.h"

FooterRenderer::FooterRenderer(uint8_t* framebuffer)
    : framebuffer_(framebuffer) {}

void FooterRenderer::draw(const struct tm& now, uint64_t sleepUs,
                          float batteryVoltage, int batteryPercent) {
  struct tm next = now;
  next.tm_sec += (int)(sleepUs / 1000000ULL);
  mktime(&next);

  char footer[96];
  snprintf(footer, sizeof(footer),
           "Updated %02d:%02d | Next %02d:%02d | %d%% (%.2fV)",
           now.tm_hour, now.tm_min, next.tm_hour, next.tm_min, batteryPercent,
           batteryVoltage);

  int32_t x = 20;
  int32_t y = EPD_HEIGHT - 20;
  write_string((GFXfont*)&FiraSans, footer, &x, &y, framebuffer_);
}
