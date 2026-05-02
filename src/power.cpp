#include "power.h"

#include <Arduino.h>

#include "config.h"
#include "epd_driver.h"
#include "utilities.h"

float readBatteryVoltage() {
  epd_poweron();
  delay(10);
  uint16_t raw = analogRead(BATT_PIN);
  epd_poweroff_all();
  return ((float)raw / 4095.0f) * 2.0f * 3.3f * BATTERY_VOLTAGE_CALIBRATION;
}

int batteryPercent(float voltage) {
  // Piecewise Li-Po discharge curve (voltage, percent) under light load.
  // 4.20V = full charge, 3.30V = empty (cutoff before protection trips ~3.0V).
  static const float curve[][2] = {
      {4.20f, 100.0f}, {4.10f, 90.0f}, {4.00f, 80.0f}, {3.92f, 70.0f},
      {3.85f, 60.0f},  {3.80f, 50.0f}, {3.76f, 40.0f}, {3.72f, 30.0f},
      {3.68f, 20.0f},  {3.60f, 10.0f}, {3.50f, 5.0f},  {3.30f, 0.0f},
  };
  const size_t n = sizeof(curve) / sizeof(curve[0]);
  if (voltage >= curve[0][0]) return 100;
  if (voltage <= curve[n - 1][0]) return 0;
  for (size_t i = 1; i < n; ++i) {
    if (voltage >= curve[i][0]) {
      float vHi = curve[i - 1][0], vLo = curve[i][0];
      float pHi = curve[i - 1][1], pLo = curve[i][1];
      float pct = pLo + (voltage - vLo) * (pHi - pLo) / (vHi - vLo);
      return (int)constrain(pct, 0.0f, 100.0f);
    }
  }
  return 0;
}
