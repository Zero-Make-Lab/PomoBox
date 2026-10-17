#pragma once
#include <stdint.h>

// The firmware version, shown in the general Settings (hold the knob on the app list) and printed at boot.
// Bump it for every build that leaves this repo (zeroFlash, a USB update).
#define POMOBOX_VERSION "1.1.2"

// "major.minor.patch" as one number (1.1.0 -> 10100). Saved in the settings, so the first start of a new version
// notices it: it switches on and says so.
constexpr uint32_t versionCode(const char *s, uint32_t done = 0, uint32_t part = 0) {
  return *s == '\0' ? done * 100 + part
       : *s == '.' ? versionCode(s + 1, done * 100 + part, 0)
       : versionCode(s + 1, done, part * 10 + (uint32_t)(*s - '0'));
}
static constexpr uint32_t POMOBOX_VERSION_CODE = versionCode(POMOBOX_VERSION);
