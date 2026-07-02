#include <unity.h>

#include "weather/next_day_data.h"

namespace {

constexpr float kRainPct = 50.0f;  // mirrors RAIN_LIKELY_PCT

NextDayData make(WeatherCondition c, float precip, float delta) {
  NextDayData d;
  d.condition = c;
  d.precipProbMax = precip;
  d.tempDeltaC = delta;
  d.valid = true;
  return d;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_rain_likely_at_and_above_threshold() {
  TEST_ASSERT_TRUE(make(WeatherCondition::Rain, 50.0f, 0).isRainLikely(kRainPct));
  TEST_ASSERT_TRUE(make(WeatherCondition::Rain, 80.0f, 0).isRainLikely(kRainPct));
}

void test_rain_unlikely_below_threshold() {
  // A rainy weather code with a low chance must not read as rain.
  TEST_ASSERT_FALSE(
      make(WeatherCondition::LightRain, 49.0f, 0).isRainLikely(kRainPct));
  TEST_ASSERT_FALSE(make(WeatherCondition::Clear, 0.0f, 0).isRainLikely(kRainPct));
}

void test_temp_delta_rounds_to_whole_degrees() {
  TEST_ASSERT_EQUAL(2, make(WeatherCondition::Clear, 0, 2.4f).tempDeltaRounded());
  TEST_ASSERT_EQUAL(3, make(WeatherCondition::Clear, 0, 2.6f).tempDeltaRounded());
  TEST_ASSERT_EQUAL(-1, make(WeatherCondition::Clear, 0, -1.4f).tempDeltaRounded());
  TEST_ASSERT_EQUAL(-2, make(WeatherCondition::Clear, 0, -1.6f).tempDeltaRounded());
}

void test_temp_delta_zero_when_sub_half_degree() {
  TEST_ASSERT_EQUAL(0, make(WeatherCondition::Clear, 0, 0.4f).tempDeltaRounded());
  TEST_ASSERT_EQUAL(0, make(WeatherCondition::Clear, 0, -0.4f).tempDeltaRounded());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_rain_likely_at_and_above_threshold);
  RUN_TEST(test_rain_unlikely_below_threshold);
  RUN_TEST(test_temp_delta_rounds_to_whole_degrees);
  RUN_TEST(test_temp_delta_zero_when_sub_half_degree);
  return UNITY_END();
}
