#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// Updating the firmware over USB without the BOOT button (docs/specs/usb-mirror.md). The running firmware takes the
// new app over the mirror's serial link and writes it to the other OTA slot (the ESP32 Update library), then
// restarts into it. No Arduino here, so the host tests (test/test_serial_update) cover it.
//
// PC: "update <size> <md5>"      PomoBox: "@pomobox update ready"
// PC: the image, in blocks of UPDATE_BLOCK bytes, each after the previous one's ack
// PomoBox: "@pomobox update ack <received>" per block, then "@pomobox update done" (and restarts)
//          or "@pomobox update error <reason>" at any point
// =============================================================================

static constexpr uint32_t UPDATE_MAX_SIZE = 0x140000;  // one app slot in the default partition table (1.25 MB)
static constexpr uint32_t UPDATE_BLOCK    = 4096;
static constexpr uint32_t UPDATE_IDLE_MS  = 10000;     // no bytes for this long: give up, keep the running firmware

struct UpdateRequest {
  uint32_t size;
  char     md5[33];  // lowercase hex
};

// "update <size> <md5>": the size within one slot, the MD5 as 32 hex digits (stored lowercase)
inline bool parseUpdateCommand(const char *line, UpdateRequest &out) {
  if (strncmp(line, "update ", 7) != 0) return false;
  const char *p = line + 7;
  if (*p < '0' || *p > '9') return false;
  char *end;
  unsigned long size = strtoul(p, &end, 10);
  if (*end != ' ' || size == 0 || size > UPDATE_MAX_SIZE) return false;
  p = end + 1;
  if (strlen(p) != 32) return false;
  for (int i = 0; i < 32; i++) {
    char c = p[i];
    if (c >= 'A' && c <= 'F') c = (char)(c - 'A' + 'a');
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    out.md5[i] = c;
  }
  out.md5[32] = '\0';
  out.size = (uint32_t)size;
  return true;
}

// Bytes to read now: what has arrived, no more than the buffer, never past the image
inline uint32_t updateTake(uint32_t size, uint32_t received, uint32_t available, uint32_t buffer) {
  uint32_t left = size - received;
  uint32_t n = available < buffer ? available : buffer;
  return n < left ? n : left;
}

// An ack after every whole block, and after the last bytes
inline bool updateShouldAck(uint32_t received, uint32_t size) {
  return received > 0 && (received % UPDATE_BLOCK == 0 || received == size);
}

inline uint8_t updatePercent(uint32_t received, uint32_t size) {
  return (uint8_t)((uint64_t)received * 100 / size);
}
