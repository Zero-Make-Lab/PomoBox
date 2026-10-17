#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// =============================================================================
// Quick Timer duration rules: hours and minutes are set separately, up to 12 hours.
// No Arduino here, so the host tests (test/test_quick_timer) cover it.
// =============================================================================

static constexpr int32_t QT_MAX_HOURS = 12;
static constexpr int32_t QT_MAX_SEC   = QT_MAX_HOURS * 3600;

// Hours stop at 0 and 12. 12 hours is the most, so reaching it zeroes the minutes.
inline void qtStepHours(int32_t &hours, int32_t &mins, int32_t step) {
  hours += step;
  if (hours < 0) hours = 0;
  if (hours > QT_MAX_HOURS) hours = QT_MAX_HOURS;
  if (hours == QT_MAX_HOURS) mins = 0;
}

// Minutes wrap 0..59 without carrying into the hours, and stay at 0 at 12 hours.
inline void qtStepMinutes(int32_t hours, int32_t &mins, int32_t step) {
  if (hours >= QT_MAX_HOURS) { mins = 0; return; }
  mins = ((mins + step) % 60 + 60) % 60;
}

// Seconds the set time runs for: at least a minute, at most 12 hours.
inline int32_t qtSetTotal(int32_t hours, int32_t mins) {
  int32_t total = hours * 3600 + mins * 60;
  if (total < 60) total = 60;
  if (total > QT_MAX_SEC) total = QT_MAX_SEC;
  return total;
}

// Paused, the knob moves the time left by whole minutes.
inline int32_t qtNudgeRemaining(int32_t remaining, int32_t stepMin) {
  int32_t r = remaining + stepMin * 60;
  if (r < 1) r = 1;
  if (r > QT_MAX_SEC) r = QT_MAX_SEC;
  return r;
}

// Big digits while setting: "HH:MM".
inline void qtFormatSetting(char *out, size_t n, int32_t hours, int32_t mins) {
  snprintf(out, n, "%02ld:%02ld", (long)hours, (long)mins);
}

// Big digits while counting: "MM:SS" under an hour, "H:MM:SS" from an hour up.
inline void qtFormatRemaining(char *out, size_t n, int32_t sec) {
  long h = sec / 3600, m = (sec % 3600) / 60, s = sec % 60;
  if (h > 0) snprintf(out, n, "%ld:%02ld:%02ld", h, m, s);
  else       snprintf(out, n, "%02ld:%02ld", m, s);
}

// The mode card's "<length> timer": "25:00" under an hour, "1h 30m" from an hour up.
inline void qtFormatTotal(char *out, size_t n, int32_t sec) {
  long h = sec / 3600, m = (sec % 3600) / 60, s = sec % 60;
  if (h == 0)      snprintf(out, n, "%ld:%02ld", m, s);
  else if (m == 0) snprintf(out, n, "%ldh", h);
  else             snprintf(out, n, "%ldh %ldm", h, m);
}
