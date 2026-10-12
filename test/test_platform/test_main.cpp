// Host tests for the board layer's logic: pio test -e native
#include <string.h>
#include <unity.h>

#include "platform/power_logic.h"
#include "platform/pref_entries.h"
#include "platform/pwm_timing.h"
#include "platform/quadrature.h"

void setUp() {}
void tearDown() {}

// ---------------------------------------------------------------------------------------------------------------
// Encoder: the same counts as ESP32Encoder's half-quad mode. Its pulse counter config: A rising -> decrement,
// A falling -> increment, reversed while B is high. States are (A << 1) | B.

static int run(const uint8_t *states, int n) {
  int count = 0;
  for (int i = 1; i < n; i++) count += halfQuadStep(states[i - 1], states[i]);
  return count;
}

void test_quadrature_matches_the_esp32_pulse_counter() {
  TEST_ASSERT_EQUAL(-1, halfQuadStep(0b00, 0b10));  // A rises, B low
  TEST_ASSERT_EQUAL(+1, halfQuadStep(0b01, 0b11));  // A rises, B high: reversed
  TEST_ASSERT_EQUAL(+1, halfQuadStep(0b10, 0b00));  // A falls, B low
  TEST_ASSERT_EQUAL(-1, halfQuadStep(0b11, 0b01));  // A falls, B high: reversed
}

void test_quadrature_ignores_b_alone_and_missed_edges() {
  TEST_ASSERT_EQUAL(0, halfQuadStep(0b11, 0b11));
  TEST_ASSERT_EQUAL(0, halfQuadStep(0b11, 0b10));  // B alone
  TEST_ASSERT_EQUAL(0, halfQuadStep(0b00, 0b01));  // B alone
  TEST_ASSERT_EQUAL(0, halfQuadStep(0b00, 0b11));  // both: an edge was missed
  TEST_ASSERT_EQUAL(0, halfQuadStep(0b10, 0b01));
}

void test_quadrature_counts_two_per_detent_each_way() {
  const uint8_t aLeads[] = {0b11, 0b01, 0b00, 0b10, 0b11};
  const uint8_t bLeads[] = {0b11, 0b10, 0b00, 0b01, 0b11};
  TEST_ASSERT_EQUAL(-2, run(aLeads, 5));
  TEST_ASSERT_EQUAL(+2, run(bLeads, 5));
}

void test_quadrature_bounce_cancels_out() {
  // A chatters on its first edge before settling: the net count is still one detent
  const uint8_t bounced[] = {0b11, 0b01, 0b11, 0b01, 0b11, 0b01, 0b00, 0b10, 0b11};
  TEST_ASSERT_EQUAL(-2, run(bounced, 9));
}

// ---------------------------------------------------------------------------------------------------------------
// LEDC on RP2040 PWM

void test_pwm_knob_audio_carrier_is_exact() {
  const PwmTiming t = pwmTimingFor(133000000, 62500);
  TEST_ASSERT_EQUAL_UINT32(16, t.div16);  // divider 1
  TEST_ASSERT_EQUAL_UINT32(2128, t.top);
  TEST_ASSERT_EQUAL_UINT32(62500, pwmTimingFreq(133000000, t));
}

void test_pwm_frequencies_land_within_half_a_percent() {
  const uint32_t clocks[] = {125000000, 133000000};
  for (uint32_t clock : clocks) {
    for (uint32_t f = 20; f <= 200000; f = f * 21 / 20 + 1) {
      const PwmTiming t = pwmTimingFor(clock, f);
      TEST_ASSERT_TRUE(t.top >= 2 && t.top <= 65536);
      TEST_ASSERT_TRUE(t.div16 >= 16 && t.div16 <= 4095);
      const uint32_t actual = pwmTimingFreq(clock, t);
      TEST_ASSERT_UINT32_WITHIN(f / 200 + 1, f, actual);
    }
  }
}

void test_pwm_uses_the_finest_counter_for_buzzer_tones() {
  // NOTE_C4: needs the fractional divider to stay inside the 16-bit counter
  const PwmTiming t = pwmTimingFor(133000000, 262);
  TEST_ASSERT_TRUE(t.top > 60000);
  TEST_ASSERT_UINT32_WITHIN(1, 262, pwmTimingFreq(133000000, t));
}

void test_pwm_rejects_out_of_range_frequencies() {
  TEST_ASSERT_EQUAL_UINT32(0, pwmTimingFor(133000000, 0).top);
  TEST_ASSERT_EQUAL_UINT32(0, pwmTimingFor(133000000, 1).top);         // below ~8 Hz
  TEST_ASSERT_EQUAL_UINT32(0, pwmTimingFor(133000000, 100000000).top);  // above half the clock
}

void test_pwm_level_keeps_esp32_duty_semantics() {
  // ledcWriteTone() leaves the channel at 10 bits, 0x1FF: just under half
  TEST_ASSERT_EQUAL_UINT32(998, pwmLevel(0x1FF, 10, 2000));
  // playTone() then writes 64/128/230 at 10 bits: 6.25% / 12.5% / 22.5%
  TEST_ASSERT_EQUAL_UINT32(125, pwmLevel(64, 10, 2000));
  TEST_ASSERT_EQUAL_UINT32(250, pwmLevel(128, 10, 2000));
  TEST_ASSERT_EQUAL_UINT32(449, pwmLevel(230, 10, 2000));
  TEST_ASSERT_EQUAL_UINT32(0, pwmLevel(0, 8, 2128));
  TEST_ASSERT_EQUAL_UINT32(2128, pwmLevel(256, 8, 2128));  // 1 << bits: always on
  TEST_ASSERT_EQUAL_UINT32(2128, pwmLevel(999, 8, 2128));  // clamped
}

// ---------------------------------------------------------------------------------------------------------------
// Settings file

void test_prefs_round_trip_every_value_type() {
  PrefEntries a;
  TEST_ASSERT_TRUE(a.set("focusTime", 4294967295LL));  // uint32 max
  TEST_ASSERT_TRUE(a.set("focusDur", 3000));
  TEST_ASSERT_TRUE(a.set("neg", -2147483648LL));
  TEST_ASSERT_TRUE(a.set("autoCont", 1));
  char text[256];
  TEST_ASSERT_TRUE(a.format(text, sizeof(text)) > 0);
  TEST_ASSERT_EQUAL_STRING("focusTime 4294967295\nfocusDur 3000\nneg -2147483648\nautoCont 1\n", text);

  PrefEntries b;
  b.parse(text);
  TEST_ASSERT_EQUAL(4, b.count);
  TEST_ASSERT_EQUAL_INT64(4294967295LL, b.values[b.find("focusTime")]);
  TEST_ASSERT_EQUAL_INT64(-2147483648LL, b.values[b.find("neg")]);
  TEST_ASSERT_EQUAL(-1, b.find("volume"));
}

void test_prefs_update_in_place() {
  PrefEntries a;
  a.set("volume", 3);
  a.set("volume", 0);
  TEST_ASSERT_EQUAL(1, a.count);
  TEST_ASSERT_EQUAL_INT64(0, a.values[0]);
}

void test_prefs_parse_skips_damaged_lines() {
  PrefEntries a;
  a.parse("good 7\r\nnoValue\n 5\nword abc\nhalf 12x\nthiskeyistoolong 1\nlast -3");
  TEST_ASSERT_EQUAL(2, a.count);
  TEST_ASSERT_EQUAL_INT64(7, a.values[a.find("good")]);
  TEST_ASSERT_EQUAL_INT64(-3, a.values[a.find("last")]);
}

void test_prefs_limits() {
  PrefEntries a;
  TEST_ASSERT_FALSE(a.set("sixteenCharsKey!", 1));  // NVS keys are at most 15 characters
  TEST_ASSERT_FALSE(a.set("", 1));
  char key[8];
  for (int i = 0; i < PrefEntries::MAX_KEYS; i++) {
    key[0] = 'k';
    key[1] = (char)('a' + i);
    key[2] = '\0';
    TEST_ASSERT_TRUE(a.set(key, i));
  }
  TEST_ASSERT_FALSE(a.set("oneTooMany", 1));
  char small[8];
  TEST_ASSERT_EQUAL(0, a.format(small, sizeof(small)));
}

// ---------------------------------------------------------------------------------------------------------------
// Power

void test_boot_turns_on_from_the_button_or_battery_alone() {
  TEST_ASSERT_TRUE(startedByButton(true, false));
  TEST_ASSERT_TRUE(startedByButton(false, false));  // on battery, only the button can have switched it on
  TEST_ASSERT_TRUE(startedByButton(true, true));
  TEST_ASSERT_FALSE(startedByButton(false, true));  // USB plugged in, or an upload: soft off
}

void test_battery_millivolts_from_the_divider() {
  // 4.2 V through 330k / 100k is 0.977 V: ADC 1212 of 4095 at 3.3 V
  TEST_ASSERT_UINT32_WITHIN(2, 4200, batteryMillivolts(1212, 4095, 3300, 330, 100));
  TEST_ASSERT_UINT32_WITHIN(2, 3400, batteryMillivolts(981, 4095, 3300, 330, 100));
}

void test_divider_input_from_the_tap_voltage() {
  TEST_ASSERT_EQUAL_UINT32(4300, dividerInputMillivolts(1000, 330, 100));  // the battery divider
  TEST_ASSERT_EQUAL_UINT32(3401, dividerInputMillivolts(791, 330, 100));  // 3401.3, rounded down
  TEST_ASSERT_EQUAL_UINT32(4700, dividerInputMillivolts(2350, 100, 100));  // the DIY board's USB divider
  TEST_ASSERT_EQUAL_UINT32(0, dividerInputMillivolts(0, 100, 100));
}

void test_usb_present_ignores_the_devkit_back_feed() {
  // On battery the DevKitC's own regulator leaks the 3.3 V supply back onto its 5 V pin
  TEST_ASSERT_FALSE(usbPresentFromVbus(0));
  TEST_ASSERT_FALSE(usbPresentFromVbus(2600));
  TEST_ASSERT_FALSE(usbPresentFromVbus(3300));
  TEST_ASSERT_TRUE(usbPresentFromVbus(4400));  // a weak port after the devkit's diode
  TEST_ASSERT_TRUE(usbPresentFromVbus(5100));
}

void test_low_battery_needs_readings_in_a_row_on_battery() {
  LowBatteryFilter f(3400, 3);
  TEST_ASSERT_FALSE(f.update(3390, false));
  TEST_ASSERT_FALSE(f.update(3390, false));
  TEST_ASSERT_FALSE(f.update(3410, false));  // one good reading starts over
  TEST_ASSERT_FALSE(f.update(3390, false));
  TEST_ASSERT_FALSE(f.update(3390, false));
  TEST_ASSERT_TRUE(f.update(3390, false));
  TEST_ASSERT_TRUE(f.update(3300, false));
  TEST_ASSERT_FALSE(f.update(3300, true));  // USB: charging, never critical
  TEST_ASSERT_FALSE(f.update(3300, false));
}

void test_c6_divider_and_disconnected_battery() {
  TEST_ASSERT_EQUAL_UINT32(4200, dividerInputMillivolts(2100, 1000, 1000));
  TEST_ASSERT_EQUAL_UINT32(3400, dividerInputMillivolts(1700, 1000, 1000));
  TEST_ASSERT_TRUE(c6BatteryAbsent(0));
  TEST_ASSERT_TRUE(c6BatteryAbsent(999));
  TEST_ASSERT_FALSE(c6BatteryAbsent(1000));
  TEST_ASSERT_FALSE(c6BatteryAbsent(2500));
  LowBatteryFilter f(3400, 3);
  TEST_ASSERT_FALSE(f.update(0, c6BatteryAbsent(0)));
  TEST_ASSERT_FALSE(f.update(3300, c6BatteryAbsent(3300)));
  TEST_ASSERT_FALSE(f.update(3300, c6BatteryAbsent(3300)));
  TEST_ASSERT_TRUE(f.update(3300, c6BatteryAbsent(3300)));
  TEST_ASSERT_FALSE(f.update(3400, c6BatteryAbsent(3400)));
}

// One battery check as power.cpp makes it: the burst's lowest, highest and average reading, then the filter
static bool c6Check(LowBatteryFilter& f, uint32_t& previousMv, uint32_t minMv, uint32_t maxMv) {
  const uint32_t mv = (minMv + maxMv) / 2;
  const bool skip = c6BatteryAbsent(mv) || c6BatteryUnsteady(minMv, maxMv, mv, previousMv);
  previousMv = mv;
  return f.update(mv, skip);
}

void test_c6_unsteady_readings_are_not_a_battery_level() {
  TEST_ASSERT_FALSE(c6BatteryUnsteady(3300, 3360, 3330, 0));     // first check: nothing to compare with
  TEST_ASSERT_FALSE(c6BatteryUnsteady(3300, 3360, 3330, 3380));  // a real cell under a changing load
  TEST_ASSERT_TRUE(c6BatteryUnsteady(2900, 4150, 3525, 3500));   // one burst spans the charger restarting
  TEST_ASSERT_TRUE(c6BatteryUnsteady(3250, 3300, 3275, 4150));   // a jump since the check 2 s ago
  TEST_ASSERT_TRUE(c6BatteryUnsteady(4100, 4150, 4125, 3250));   // and back
}

void test_c6_thermal_cutoff_on_usb_never_shuts_down() {
  // Rev C1: the 45 C switch can open the battery lead while USB runs the board. With no cell the charger's BAT node
  // swings as the charger restarts into its capacitor. That must never add up to "Battery low".
  // Each case below averages under 3.4 V three checks in a row, so without the steadiness test it would shut down.
  LowBatteryFilter f(3400, 3);
  uint32_t previous = 0;
  // bursts that catch the charger restarting: averaging 3.35 V, but spanning 1.1 V
  for (int i = 0; i < 6; ++i) TEST_ASSERT_FALSE(c6Check(f, previous, 2800, 3900));
  // a sawtooth sampled every 2 s: steady within each burst, far apart between checks
  LowBatteryFilter g(3400, 3);
  previous = 0;
  const uint32_t saw[] = {3300, 2700, 3350, 2650, 3300, 2700};
  for (uint32_t v : saw) TEST_ASSERT_FALSE(c6Check(g, previous, v - 20, v + 20));
}

void test_c6_steady_empty_cell_still_shuts_down() {
  LowBatteryFilter f(3400, 3);
  uint32_t previous = 0;
  TEST_ASSERT_FALSE(c6Check(f, previous, 3400, 3440));  // 3420: still above the 3400 mV threshold
  TEST_ASSERT_FALSE(c6Check(f, previous, 3330, 3370));  // 3350
  TEST_ASSERT_FALSE(c6Check(f, previous, 3320, 3360));  // 3340
  TEST_ASSERT_TRUE(c6Check(f, previous, 3300, 3340));   // third low reading in a row: save and sleep
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_quadrature_matches_the_esp32_pulse_counter);
  RUN_TEST(test_quadrature_ignores_b_alone_and_missed_edges);
  RUN_TEST(test_quadrature_counts_two_per_detent_each_way);
  RUN_TEST(test_quadrature_bounce_cancels_out);
  RUN_TEST(test_pwm_knob_audio_carrier_is_exact);
  RUN_TEST(test_pwm_frequencies_land_within_half_a_percent);
  RUN_TEST(test_pwm_uses_the_finest_counter_for_buzzer_tones);
  RUN_TEST(test_pwm_rejects_out_of_range_frequencies);
  RUN_TEST(test_pwm_level_keeps_esp32_duty_semantics);
  RUN_TEST(test_prefs_round_trip_every_value_type);
  RUN_TEST(test_prefs_update_in_place);
  RUN_TEST(test_prefs_parse_skips_damaged_lines);
  RUN_TEST(test_prefs_limits);
  RUN_TEST(test_boot_turns_on_from_the_button_or_battery_alone);
  RUN_TEST(test_battery_millivolts_from_the_divider);
  RUN_TEST(test_divider_input_from_the_tap_voltage);
  RUN_TEST(test_usb_present_ignores_the_devkit_back_feed);
  RUN_TEST(test_low_battery_needs_readings_in_a_row_on_battery);
  RUN_TEST(test_c6_divider_and_disconnected_battery);
  RUN_TEST(test_c6_unsteady_readings_are_not_a_battery_level);
  RUN_TEST(test_c6_thermal_cutoff_on_usb_never_shuts_down);
  RUN_TEST(test_c6_steady_empty_cell_still_shuts_down);
  return UNITY_END();
}
