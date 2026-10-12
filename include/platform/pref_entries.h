// The key/value table behind the RP2040 Preferences, and its file format: one "key value" pair per line, values as
// signed decimal. No Arduino dependencies, so the host unit tests (pio test -e native) cover it.
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct PrefEntries {
  static constexpr int MAX_KEYS = 16;
  static constexpr size_t KEY_CAP = 16;  // ESP32 NVS keys are at most 15 characters

  int count = 0;
  char keys[MAX_KEYS][KEY_CAP];
  int64_t values[MAX_KEYS];

  int find(const char* key) const {
    for (int i = 0; i < count; i++)
      if (strcmp(keys[i], key) == 0) return i;
    return -1;
  }

  // false when the key is too long or the table is full
  bool set(const char* key, int64_t value) {
    int i = find(key);
    if (i < 0) {
      if (count >= MAX_KEYS || strlen(key) >= KEY_CAP || strlen(key) == 0) return false;
      i = count++;
      strcpy(keys[i], key);
    }
    values[i] = value;
    return true;
  }

  // Reads lines of the file format; lines that don't parse are skipped.
  void parse(const char* text) {
    count = 0;
    while (*text) {
      const char* end = strchr(text, '\n');
      const size_t len = end ? (size_t)(end - text) : strlen(text);
      const char* space = (const char*)memchr(text, ' ', len);
      if (space && space > text && (size_t)(space - text) < KEY_CAP) {
        char key[KEY_CAP];
        memcpy(key, text, space - text);
        key[space - text] = '\0';
        char number[24];
        const size_t numLen = len - (space + 1 - text);
        if (numLen > 0 && numLen < sizeof(number)) {
          memcpy(number, space + 1, numLen);
          number[numLen] = '\0';
          char* parsedTo = nullptr;
          const long long value = strtoll(number, &parsedTo, 10);
          if (parsedTo != number && (*parsedTo == '\0' || *parsedTo == '\r')) set(key, value);
        }
      }
      text += len + (end ? 1 : 0);
    }
  }

  // Longest line: a 15-character key, a space, a 20-character value and the newline
  static constexpr size_t LINE_CAP = KEY_CAP + 22;

  // Writes the file format into out (NUL-terminated). Returns the length, or 0 if it didn't fit.
  size_t format(char* out, size_t cap) const {
    size_t used = 0;
    for (int i = 0; i < count; i++) {
      char line[LINE_CAP];
      size_t n = strlen(keys[i]);
      memcpy(line, keys[i], n);
      line[n++] = ' ';
      n += formatInt64(values[i], line + n);
      line[n++] = '\n';
      if (used + n >= cap) return 0;
      memcpy(out + used, line, n);
      used += n;
    }
    if (cap) out[used] = '\0';
    return used;
  }

  // Decimal without printf, whose 64-bit support depends on the C library build. No terminator; at most 20 chars.
  static size_t formatInt64(int64_t v, char* out) {
    char digits[20];
    uint64_t u = v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
    size_t n = 0;
    do {
      digits[n++] = (char)('0' + u % 10);
      u /= 10;
    } while (u);
    size_t len = 0;
    if (v < 0) out[len++] = '-';
    while (n) out[len++] = digits[--n];
    return len;
  }
};
