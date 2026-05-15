#include "power.h"

#include <Arduino.h>
#include <Wire.h>

#include "config.h"
#include "epd_driver.h"
#include "utilities.h"

float readBatteryVoltage() {
  constexpr int kSamples = 16;
  epd_poweron();
  delay(10);
  uint32_t sumMv = 0;
  for (int i = 0; i < kSamples; ++i) {
    sumMv += analogReadMilliVolts(BATT_PIN);
  }
  epd_poweroff_all();
  const float pinVolts = (sumMv / static_cast<float>(kSamples)) / 1000.0f;
  return pinVolts * 2.0f * BATTERY_VOLTAGE_CALIBRATION;
}

// Sleeps the GT911 touch controller (~3 mA awake → ~5 µA asleep). The INT
// pulse re-addresses the chip if it was already asleep from a prior cycle.
void sleepTouchController() {
  // GT911 picks one of two I2C addresses at power-on based on INT/RST timing,
  // so we probe both. Registers are 16-bit, sent high byte first.
  constexpr uint8_t kAddrCandidates[] = {0x14, 0x5D};
  constexpr uint16_t kRegCommand = 0x8040;
  constexpr uint8_t kCmdEnterSleep = 0x05;

  pinMode(TOUCH_INT, OUTPUT);
  digitalWrite(TOUCH_INT, HIGH);
  delay(10);

  Wire.begin(BOARD_SDA, BOARD_SCL);
  uint8_t addr = 0;
  for (uint8_t candidate : kAddrCandidates) {
    Wire.beginTransmission(candidate);
    if (Wire.endTransmission() == 0) {
      addr = candidate;
      break;
    }
  }
  if (addr) {
    Wire.beginTransmission(addr);
    Wire.write(static_cast<uint8_t>(kRegCommand >> 8));
    Wire.write(static_cast<uint8_t>(kRegCommand & 0xFF));
    Wire.write(kCmdEnterSleep);
    Wire.endTransmission();
    delay(5);
  }
  Wire.end();

  // Open-drain so we don't drive the chip's pins during deep sleep.
  pinMode(BOARD_SDA, OPEN_DRAIN);
  pinMode(BOARD_SCL, OPEN_DRAIN);
  pinMode(TOUCH_INT, OPEN_DRAIN);
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
