#include "power.h"

#include <Arduino.h>

#include "epd_driver.h"
#include "utilities.h"

float readBatteryVoltage() {
  epd_poweron();
  delay(10);
  uint16_t raw = analogRead(BATT_PIN);
  epd_poweroff_all();
  return ((float)raw / 4095.0f) * 2.0f * 3.3f * (1100.0f / 1000.0f);
}

int batteryPercent(float voltage) {
  const float vMin = 3.3f;
  const float vMax = 4.2f;
  float pct = (voltage - vMin) / (vMax - vMin) * 100.0f;
  return (int)constrain(pct, 0.0f, 100.0f);
}
