#if defined(ARDUINO_ARCH_RP2040)
#include "platform/prefs.h"

#include <LittleFS.h>

namespace {
bool mounted = false;
bool mount() {
  if (!mounted) mounted = LittleFS.begin();  // formats an empty filesystem on first use
  return mounted;
}

constexpr size_t FILE_CAP = PrefEntries::MAX_KEYS * PrefEntries::LINE_CAP;
}  // namespace

bool Preferences::begin(const char *name, bool readOnly) {
  open_ = false;
  dirty_ = false;
  entries_.count = 0;
  if (!mount()) return false;
  snprintf(path_, sizeof(path_), "/prefs_%s", name);
  readOnly_ = readOnly;
  File f = LittleFS.open(path_, "r");
  if (!f) {
    open_ = !readOnly;  // a write session creates the namespace
    return open_;
  }
  char text[FILE_CAP + 1];
  const size_t n = f.readBytes(text, FILE_CAP);
  f.close();
  text[n] = '\0';
  entries_.parse(text);
  open_ = true;
  return true;
}

void Preferences::end() {
  if (open_ && !readOnly_ && dirty_) {
    char text[FILE_CAP + 1];
    const size_t n = entries_.format(text, sizeof(text));
    File f = LittleFS.open(path_, "w");
    if (f) {
      f.write((const uint8_t *)text, n);
      f.close();
    }
  }
  open_ = false;
  dirty_ = false;
}

bool Preferences::get(const char *key, int64_t &value) const {
  if (!open_) return false;
  const int i = entries_.find(key);
  if (i < 0) return false;
  value = entries_.values[i];
  return true;
}

size_t Preferences::put(const char *key, int64_t value, size_t size) {
  if (!open_ || readOnly_) return 0;
  const int i = entries_.find(key);
  if (i >= 0 && entries_.values[i] == value) return size;  // unchanged: no flash write
  if (!entries_.set(key, value)) return 0;
  dirty_ = true;
  return size;
}

uint32_t Preferences::getUInt(const char *key, uint32_t defaultValue) {
  int64_t v;
  return get(key, v) ? (uint32_t)v : defaultValue;
}

int32_t Preferences::getInt(const char *key, int32_t defaultValue) {
  int64_t v;
  return get(key, v) ? (int32_t)v : defaultValue;
}

uint8_t Preferences::getUChar(const char *key, uint8_t defaultValue) {
  int64_t v;
  return get(key, v) ? (uint8_t)v : defaultValue;
}

bool Preferences::getBool(const char *key, bool defaultValue) {
  int64_t v;
  return get(key, v) ? v != 0 : defaultValue;
}

size_t Preferences::putUInt(const char *key, uint32_t value) { return put(key, value, sizeof(uint32_t)); }
size_t Preferences::putInt(const char *key, int32_t value) { return put(key, value, sizeof(int32_t)); }
size_t Preferences::putUChar(const char *key, uint8_t value) { return put(key, value, sizeof(uint8_t)); }
size_t Preferences::putBool(const char *key, bool value) { return put(key, value ? 1 : 0, sizeof(bool)); }
#endif
