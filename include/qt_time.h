#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// =============================================================================
// Quick Timer duration rules. The timer is set as two fields, left then right:
// MM:SS up to 60 minutes, or HH:MM up to 12 hours (Settings > Timer Mode).
// No Arduino here, so the host tests (test/test_quick_timer) cover it.
// =============================================================================

enum class QTUnits : uint8_t { MIN_SEC = 0, HOUR_MIN = 1 };

static constexpr int32_t QT_MAX_HOURS = 12;
static constexpr int32_t QT_MAX_SEC   = QT_MAX_HOURS * 3600;

// Seconds in one step of the left field, and the most it goes to.
inline int32_t qtLeftUnitSec(QTUnits u) { return u == QTUnits::HOUR_MIN ? 3600 : 60; }
inline int32_t qtLeftMax(QTUnits u)     { return u == QTUnits::HOUR_MIN ? QT_MAX_HOURS : 60; }
inline int32_t qtMaxSec(QTUnits u)      { return qtLeftMax(u) * qtLeftUnitSec(u); }

// The mode as stored in NVS; anything unknown is MM:SS.
inline QTUnits qtUnitsFromStored(uint8_t v) { return v == 1 ? QTUnits::HOUR_MIN : QTUnits::MIN_SEC; }

// The left field stops at 0 and the maximum. At the maximum the right field is 0.
inline void qtStepLeft(QTUnits u, int32_t &left, int32_t &right, int32_t step) {
  left += step;
  if (left < 0) left = 0;
  if (left > qtLeftMax(u)) left = qtLeftMax(u);
  if (left == qtLeftMax(u)) right = 0;
}

// The right field wraps 0..59 without carrying into the left, and stays at 0 at the maximum.
inline void qtStepRight(QTUnits u, int32_t left, int32_t &right, int32_t step) {
  if (left >= qtLeftMax(u)) { right = 0; return; }
  right = ((right + step) % 60 + 60) % 60;
}

// Seconds the set fields run for: MM:SS from 1 second, HH:MM from 1 minute.
inline int32_t qtSetTotal(QTUnits u, int32_t left, int32_t right) {
  int32_t rightUnit = qtLeftUnitSec(u) / 60;
  int32_t total = left * qtLeftUnitSec(u) + right * rightUnit;
  if (total < rightUnit) total = rightUnit;
  if (total > qtMaxSec(u)) total = qtMaxSec(u);
  return total;
}

// The fields for a length of time, for switching mode without losing the set time. HH:MM drops the seconds.
inline void qtSplitTotal(QTUnits u, int32_t total, int32_t &left, int32_t &right) {
  if (total > qtMaxSec(u)) total = qtMaxSec(u);
  if (total < 0) total = 0;
  int32_t rightUnit = qtLeftUnitSec(u) / 60;
  left  = total / qtLeftUnitSec(u);
  right = (total % qtLeftUnitSec(u)) / rightUnit;
}

// Paused, the knob moves the time left by whole minutes, up to the mode's maximum (or the time already left, when
// the mode changed to a shorter one mid-timer).
inline int32_t qtNudgeRemaining(QTUnits u, int32_t remaining, int32_t stepMin) {
  int32_t cap = remaining > qtMaxSec(u) ? remaining : qtMaxSec(u);
  int32_t r = remaining + stepMin * 60;
  if (r < 1) r = 1;
  if (r > cap) r = cap;
  return r;
}

// Big digits while setting: both fields, "LL:RR".
inline void qtFormatSetting(char *out, size_t n, int32_t left, int32_t right) {
  snprintf(out, n, "%02ld:%02ld", (long)left, (long)right);
}

// Big digits while counting: "MM:SS" up to an hour, "H:MM:SS" past it.
inline void qtFormatRemaining(char *out, size_t n, int32_t sec) {
  if (sec <= 3600) snprintf(out, n, "%02ld:%02ld", (long)(sec / 60), (long)(sec % 60));
  else snprintf(out, n, "%ld:%02ld:%02ld", (long)(sec / 3600), (long)((sec % 3600) / 60), (long)(sec % 60));
}

// The mode card's "<length> timer": "25:00" up to an hour, "1h 30m" past it.
inline void qtFormatTotal(char *out, size_t n, int32_t sec) {
  long h = sec / 3600, m = (sec % 3600) / 60;
  if (sec <= 3600) snprintf(out, n, "%ld:%02ld", (long)(sec / 60), (long)(sec % 60));
  else if (m == 0) snprintf(out, n, "%ldh", h);
  else             snprintf(out, n, "%ldh %ldm", h, m);
}
