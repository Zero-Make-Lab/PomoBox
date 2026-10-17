// Host tests for the Quick Timer's duration rules: pio test -e native
#include <string.h>
#include <unity.h>

#include "qt_time.h"

void setUp() {}
void tearDown() {}

// ---------------------------------------------------------------------------------------------------------------
// Setting: hours 0..12, minutes 0..59

void test_hours_stop_at_zero_and_twelve() {
  int32_t h = 0, m = 5;
  qtStepHours(h, m, -1);
  TEST_ASSERT_EQUAL(0, h);
  qtStepHours(h, m, +30);
  TEST_ASSERT_EQUAL(12, h);
  qtStepHours(h, m, +1);
  TEST_ASSERT_EQUAL(12, h);
}

void test_twelve_hours_is_the_most_so_minutes_go_to_zero() {
  int32_t h = 11, m = 45;
  qtStepHours(h, m, +1);
  TEST_ASSERT_EQUAL(12, h);
  TEST_ASSERT_EQUAL(0, m);
  qtStepMinutes(h, m, +1);  // locked at 12:00
  TEST_ASSERT_EQUAL(0, m);
  qtStepMinutes(h, m, -1);
  TEST_ASSERT_EQUAL(0, m);
}

void test_minutes_wrap_around_the_hour() {
  int32_t h = 0, m = 58;
  qtStepMinutes(h, m, +1);
  TEST_ASSERT_EQUAL(59, m);
  qtStepMinutes(h, m, +1);
  TEST_ASSERT_EQUAL(0, m);
  qtStepMinutes(h, m, -1);
  TEST_ASSERT_EQUAL(59, m);
  qtStepMinutes(h, m, +125);  // a fast spin still lands inside 0..59
  TEST_ASSERT_EQUAL(4, m);
  TEST_ASSERT_EQUAL(0, h);    // and never carries into the hours
}

void test_set_total_runs_from_one_minute_to_twelve_hours() {
  TEST_ASSERT_EQUAL(60, qtSetTotal(0, 0));  // 00:00 still starts something
  TEST_ASSERT_EQUAL(60, qtSetTotal(0, 1));
  TEST_ASSERT_EQUAL(3600, qtSetTotal(1, 0));
  TEST_ASSERT_EQUAL(90 * 60, qtSetTotal(1, 30));
  TEST_ASSERT_EQUAL(12 * 3600, qtSetTotal(12, 0));
  TEST_ASSERT_EQUAL(12 * 3600, qtSetTotal(12, 30));  // never past 12 hours
}

// ---------------------------------------------------------------------------------------------------------------
// Paused: the knob nudges the time left by whole minutes

void test_paused_nudge_stays_between_one_second_and_twelve_hours() {
  TEST_ASSERT_EQUAL(5 * 60 + 30, qtNudgeRemaining(4 * 60 + 30, +1));
  TEST_ASSERT_EQUAL(1, qtNudgeRemaining(30, -1));
  TEST_ASSERT_EQUAL(QT_MAX_SEC, qtNudgeRemaining(QT_MAX_SEC - 30, +1));
  TEST_ASSERT_EQUAL(2 * 3600, qtNudgeRemaining(60 * 60, +60));  // past the old 60 minute cap
}

// ---------------------------------------------------------------------------------------------------------------
// The big digits

void test_setting_reads_hours_then_minutes() {
  char s[12];
  qtFormatSetting(s, sizeof s, 0, 5);
  TEST_ASSERT_EQUAL_STRING("00:05", s);
  qtFormatSetting(s, sizeof s, 12, 0);
  TEST_ASSERT_EQUAL_STRING("12:00", s);
  qtFormatSetting(s, sizeof s, 1, 30);
  TEST_ASSERT_EQUAL_STRING("01:30", s);
}

void test_countdown_shows_hours_only_from_an_hour_up() {
  char s[12];
  qtFormatRemaining(s, sizeof s, 59 * 60 + 59);
  TEST_ASSERT_EQUAL_STRING("59:59", s);
  qtFormatRemaining(s, sizeof s, 5);
  TEST_ASSERT_EQUAL_STRING("00:05", s);
  qtFormatRemaining(s, sizeof s, 3600);
  TEST_ASSERT_EQUAL_STRING("1:00:00", s);
  qtFormatRemaining(s, sizeof s, 12 * 3600);
  TEST_ASSERT_EQUAL_STRING("12:00:00", s);
  qtFormatRemaining(s, sizeof s, 2 * 3600 + 5 * 60 + 9);
  TEST_ASSERT_EQUAL_STRING("2:05:09", s);
}

void test_card_label_names_the_timer_length() {
  char s[16];
  qtFormatTotal(s, sizeof s, 25 * 60);
  TEST_ASSERT_EQUAL_STRING("25:00", s);
  qtFormatTotal(s, sizeof s, 5 * 60 + 30);
  TEST_ASSERT_EQUAL_STRING("5:30", s);
  qtFormatTotal(s, sizeof s, 3600);
  TEST_ASSERT_EQUAL_STRING("1h", s);
  qtFormatTotal(s, sizeof s, 90 * 60);
  TEST_ASSERT_EQUAL_STRING("1h 30m", s);
  qtFormatTotal(s, sizeof s, 12 * 3600);
  TEST_ASSERT_EQUAL_STRING("12h", s);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_hours_stop_at_zero_and_twelve);
  RUN_TEST(test_twelve_hours_is_the_most_so_minutes_go_to_zero);
  RUN_TEST(test_minutes_wrap_around_the_hour);
  RUN_TEST(test_set_total_runs_from_one_minute_to_twelve_hours);
  RUN_TEST(test_paused_nudge_stays_between_one_second_and_twelve_hours);
  RUN_TEST(test_setting_reads_hours_then_minutes);
  RUN_TEST(test_countdown_shows_hours_only_from_an_hour_up);
  RUN_TEST(test_card_label_names_the_timer_length);
  return UNITY_END();
}
