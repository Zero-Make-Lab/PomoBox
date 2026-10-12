// Settings storage. ESP32: Preferences (NVS). RP2040: the part of the same API that PomoBox uses, kept in LittleFS
// as one small file per namespace, so saveSettings()/loadSettings() are the same on both.
#pragma once

#if defined(ARDUINO_ARCH_RP2040)
#include <Arduino.h>

#include "platform/pref_entries.h"

class Preferences {
 public:
  // A read-only begin on a namespace that was never saved returns false and every get returns its default,
  // as on the ESP32
  bool begin(const char *name, bool readOnly = false);
  void end();  // writes the file if anything changed

  uint32_t getUInt(const char *key, uint32_t defaultValue = 0);
  int32_t getInt(const char *key, int32_t defaultValue = 0);
  uint8_t getUChar(const char *key, uint8_t defaultValue = 0);
  bool getBool(const char *key, bool defaultValue = false);

  // Return the bytes stored (the ESP32 value size), or 0 when not open for writing
  size_t putUInt(const char *key, uint32_t value);
  size_t putInt(const char *key, int32_t value);
  size_t putUChar(const char *key, uint8_t value);
  size_t putBool(const char *key, bool value);

 private:
  bool get(const char *key, int64_t &value) const;
  size_t put(const char *key, int64_t value, size_t size);

  char path_[32] = "";
  bool open_ = false, readOnly_ = true, dirty_ = false;
  PrefEntries entries_;
};

#else
#include <Preferences.h>
#endif
