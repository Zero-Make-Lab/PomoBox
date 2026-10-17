// Host tests for the Quick Timer's duration rules: pio test -e native
#include <string.h>
#include <unity.h>

#include "qt_time.h"

void setUp() {}
void tearDown() {}

static const QTUnits MS = QTUnits::MIN_SEC;
static const QTUnits HM = QTUnits::HOUR_MIN;

// ---------------------------------------------------------------------------------------------------------------
// Two fields, left then right: MM:SS (up to 60:00) or HH:MM (up to 12:00), picked in Settings

void test_left_field_stops_at_zero_and_the_mode_maximum() {
  int32_t l = 0, r = 5;
  qtStepLeft(HM, l, r, -1);
  TEST_ASSERT_EQUAL(0, l);
  qtStepLeft(HM, l, r, +30);
  TEST_ASSERT_EQUAL(12, l);
  l = 0;
  qtStepLeft(MS, l, r, +90);
  TEST_ASSERT_EQUAL(60, l);
}

void test_at_the_maximum_the_right_field_is_zero_and_locked() {
  int32_t l = 11, r = 45;
  qtStepLeft(HM, l, r, +1);
  TEST_ASSERT_EQUAL(12, l);
  TEST_ASSERT_EQUAL(0, r);
  qtStepRight(HM, l, r, +1);
  TEST_ASSERT_EQUAL(0, r);

  l = 59, r = 30;
  qtStepLeft(MS, l, r, +1);
  TEST_ASSERT_EQUAL(60, l);
  TEST_ASSERT_EQUAL(0, r);
  qtStepRight(MS, l, r, -1);
  TEST_ASSERT_EQUAL(0, r);
}

void test_right_field_wraps_without_carrying() {
  int32_t l = 0, r = 58;
  qtStepRight(HM, l, r, +1);
  TEST_ASSERT_EQUAL(59, r);
  qtStepRight(HM, l, r, +1);
  TEST_ASSERT_EQUAL(0, r);
  qtStepRight(MS, l, r, -1);
  TEST_ASSERT_EQUAL(59, r);
  qtStepRight(MS, l, r, +125);  // a fast spin still lands inside 0..59
  TEST_ASSERT_EQUAL(4, r);
  TEST_ASSERT_EQUAL(0, l);
}

void test_set_total_per_mode() {
  TEST_ASSERT_EQUAL(5 * 60 + 30, qtSetTotal(MS, 5, 30));
  TEST_ASSERT_EQUAL(1, qtSetTotal(MS, 0, 0));  // MM:SS can start a one second timer, as before
  TEST_ASSERT_EQUAL(3600, qtSetTotal(MS, 60, 0));
  TEST_ASSERT_EQUAL(90 * 60, qtSetTotal(HM, 1, 30));
  TEST_ASSERT_EQUAL(60, qtSetTotal(HM, 0, 0));  // HH:MM starts at least a minute
  TEST_ASSERT_EQUAL(12 * 3600, qtSetTotal(HM, 12, 0));
}

void test_switching_mode_keeps_the_time_when_it_fits() {
  int32_t l, r;
  qtSplitTotal(HM, 90 * 60, l, r);  // 90:00 does not fit MM:SS's 60:00...
  TEST_ASSERT_EQUAL(1, l);
  TEST_ASSERT_EQUAL(30, r);
  qtSplitTotal(MS, 90 * 60, l, r);  // ...so it stops at the maximum
  TEST_ASSERT_EQUAL(60, l);
  TEST_ASSERT_EQUAL(0, r);
  qtSplitTotal(MS, 5 * 60 + 30, l, r);
  TEST_ASSERT_EQUAL(5, l);
  TEST_ASSERT_EQUAL(30, r);
  qtSplitTotal(HM, 5 * 60 + 30, l, r);  // HH:MM drops the seconds
  TEST_ASSERT_EQUAL(0, l);
  TEST_ASSERT_EQUAL(5, r);
}

// ---------------------------------------------------------------------------------------------------------------
// Paused: the knob nudges the time left by whole minutes, up to the mode's maximum

void test_paused_nudge_stays_inside_the_mode() {
  TEST_ASSERT_EQUAL(5 * 60 + 30, qtNudgeRemaining(MS, 4 * 60 + 30, +1));
  TEST_ASSERT_EQUAL(1, qtNudgeRemaining(MS, 30, -1));
  TEST_ASSERT_EQUAL(3600, qtNudgeRemaining(MS, 3600 - 30, +5));
  TEST_ASSERT_EQUAL(2 * 3600, qtNudgeRemaining(HM, 3600, +60));
  TEST_ASSERT_EQUAL(QT_MAX_SEC, qtNudgeRemaining(HM, QT_MAX_SEC - 30, +1));
  // A 3 hour timer paused, then the mode switched to MM:SS: a turn never cuts it to 60:00
  TEST_ASSERT_EQUAL(3 * 3600, qtNudgeRemaining(MS, 3 * 3600, +1));
  TEST_ASSERT_EQUAL(3 * 3600 - 60, qtNudgeRemaining(MS, 3 * 3600, -1));
}

// ---------------------------------------------------------------------------------------------------------------
// The big digits

void test_setting_shows_both_fields() {
  char s[12];
  qtFormatSetting(s, sizeof s, 5, 0);
  TEST_ASSERT_EQUAL_STRING("05:00", s);
  qtFormatSetting(s, sizeof s, 12, 0);
  TEST_ASSERT_EQUAL_STRING("12:00", s);
}

void test_countdown_shows_hours_only_past_an_hour() {
  char s[12];
  qtFormatRemaining(s, sizeof s, 3600);  // a full MM:SS timer still reads 60:00
  TEST_ASSERT_EQUAL_STRING("60:00", s);
  qtFormatRemaining(s, sizeof s, 59 * 60 + 59);
  TEST_ASSERT_EQUAL_STRING("59:59", s);
  qtFormatRemaining(s, sizeof s, 5);
  TEST_ASSERT_EQUAL_STRING("00:05", s);
  qtFormatRemaining(s, sizeof s, 3601);
  TEST_ASSERT_EQUAL_STRING("1:00:01", s);
  qtFormatRemaining(s, sizeof s, 12 * 3600);
  TEST_ASSERT_EQUAL_STRING("12:00:00", s);
}

void test_card_label_names_the_timer_length() {
  char s[16];
  qtFormatTotal(s, sizeof s, 25 * 60);
  TEST_ASSERT_EQUAL_STRING("25:00", s);
  qtFormatTotal(s, sizeof s, 5 * 60 + 30);
  TEST_ASSERT_EQUAL_STRING("5:30", s);
  qtFormatTotal(s, sizeof s, 3600);
  TEST_ASSERT_EQUAL_STRING("60:00", s);
  qtFormatTotal(s, sizeof s, 90 * 60);
  TEST_ASSERT_EQUAL_STRING("1h 30m", s);
  qtFormatTotal(s, sizeof s, 12 * 3600);
  TEST_ASSERT_EQUAL_STRING("12h", s);
}

void test_stored_mode_falls_back_to_minutes() {
  TEST_ASSERT_TRUE(qtUnitsFromStored(0) == MS);
  TEST_ASSERT_TRUE(qtUnitsFromStored(1) == HM);
  TEST_ASSERT_TRUE(qtUnitsFromStored(7) == MS);  // a damaged value
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_left_field_stops_at_zero_and_the_mode_maximum);
  RUN_TEST(test_at_the_maximum_the_right_field_is_zero_and_locked);
  RUN_TEST(test_right_field_wraps_without_carrying);
  RUN_TEST(test_set_total_per_mode);
  RUN_TEST(test_switching_mode_keeps_the_time_when_it_fits);
  RUN_TEST(test_paused_nudge_stays_inside_the_mode);
  RUN_TEST(test_setting_shows_both_fields);
  RUN_TEST(test_countdown_shows_hours_only_past_an_hour);
  RUN_TEST(test_card_label_names_the_timer_length);
  RUN_TEST(test_stored_mode_falls_back_to_minutes);
  return UNITY_END();
}
