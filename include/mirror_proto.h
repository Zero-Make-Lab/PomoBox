#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// =============================================================================
// USB screen mirror, protocol v1 (docs/specs/usb-mirror.md). No Arduino here, so the host tests
// (test/test_mirror) cover it, against the hand-made byte stream in mirror/golden.hex.
// =============================================================================

static constexpr uint32_t MIRROR_BAUD      = 921600;  // UART boards; USB-CDC boards ignore it
static constexpr uint8_t  MIRROR_SYNC      = 0xA5;
static constexpr uint8_t  MIRROR_VERSION   = 1;
static constexpr size_t   MIRROR_FRAME_MAX = 14;    // the largest frame, a rect
static constexpr uint32_t MIRROR_SILENCE_MS = 3000;  // the session ends this long after the PC's last line
static constexpr int32_t  MIRROR_TURN_MAX  = 20;    // detents in one "turn" line

// --- PomoBox to PC: 0xA5 type len payload check, check = XOR of type, len and payload ---

inline size_t mirrorFrame(uint8_t *out, char type, const uint8_t *payload, uint8_t len) {
  out[0] = MIRROR_SYNC;
  out[1] = (uint8_t)type;
  out[2] = len;
  uint8_t check = (uint8_t)type ^ len;
  for (uint8_t i = 0; i < len; i++) {
    out[3 + i] = payload[i];
    check ^= payload[i];
  }
  out[3 + len] = check;
  return 4 + len;
}

inline void mirrorPut16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)(v >> 8);
}

inline size_t mirrorFrameRect(uint8_t *out, int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  uint8_t p[10];
  mirrorPut16(p + 0, (uint16_t)x);
  mirrorPut16(p + 2, (uint16_t)y);
  mirrorPut16(p + 4, (uint16_t)w);
  mirrorPut16(p + 6, (uint16_t)h);
  mirrorPut16(p + 8, color);
  return mirrorFrame(out, 'R', p, sizeof p);
}

inline size_t mirrorFrameInfo(uint8_t *out, uint16_t width, uint16_t height) {
  uint8_t p[5] = {MIRROR_VERSION};
  mirrorPut16(p + 1, width);
  mirrorPut16(p + 3, height);
  return mirrorFrame(out, 'I', p, sizeof p);
}

inline size_t mirrorFramePower(uint8_t *out, bool screenOn) {
  uint8_t p[1] = {(uint8_t)(screenOn ? 1 : 0)};
  return mirrorFrame(out, 'P', p, sizeof p);
}

// --- PC to PomoBox: newline-terminated text lines ---

enum class MirrorCmd { NONE, HELLO, PING, TURN, CLICK, HOLD, BYE, UPDATE };

struct MirrorLine {
  char    buf[64];   // a completed line stays here until the next byte (serial_update.h reads "update ..." from it)
  uint8_t len;
  bool    overflow;  // the line went past 63 characters: drop all of it
};

// One complete line (without its newline). turn is set for TURN only.
inline MirrorCmd mirrorParse(const char *s, int32_t &turn) {
  if (!strcmp(s, "hello")) return MirrorCmd::HELLO;
  if (!strcmp(s, "ping"))  return MirrorCmd::PING;
  if (!strcmp(s, "click")) return MirrorCmd::CLICK;
  if (!strcmp(s, "hold"))  return MirrorCmd::HOLD;
  if (!strcmp(s, "bye"))   return MirrorCmd::BYE;
  if (!strncmp(s, "update ", 7)) return MirrorCmd::UPDATE;  // its size and MD5: parseUpdateCommand
  if (!strncmp(s, "turn ", 5) && s[5]) {
    char *end;
    long v = strtol(s + 5, &end, 10);
    if (*end || v == 0 || v > MIRROR_TURN_MAX || v < -MIRROR_TURN_MAX) return MirrorCmd::NONE;
    turn = (int32_t)v;
    return MirrorCmd::TURN;
  }
  return MirrorCmd::NONE;
}

// Feeds one received character; returns the command when it completes a line.
inline MirrorCmd mirrorFeed(MirrorLine &l, char c, int32_t &turn) {
  if (c == '\r') return MirrorCmd::NONE;
  if (c == '\n') {
    MirrorCmd cmd = MirrorCmd::NONE;
    if (!l.overflow) {
      l.buf[l.len] = '\0';
      cmd = mirrorParse(l.buf, turn);
    }
    l.len = 0;
    l.overflow = false;
    return cmd;
  }
  if (l.len >= sizeof l.buf - 1) l.overflow = true;
  else l.buf[l.len++] = c;
  return MirrorCmd::NONE;
}

// --- Session: hello starts it, any line keeps it, bye or MIRROR_SILENCE_MS of silence ends it ---

struct MirrorSession {
  bool     active;
  uint32_t lastMs;
};

inline void mirrorHeard(MirrorSession &s, MirrorCmd cmd, uint32_t now) {
  if (cmd == MirrorCmd::NONE) return;
  if (cmd == MirrorCmd::BYE) { s.active = false; return; }
  if (cmd == MirrorCmd::HELLO) s.active = true;
  s.lastMs = now;
}

inline bool mirrorExpired(const MirrorSession &s, uint32_t now) {
  return s.active && (uint32_t)(now - s.lastMs) > MIRROR_SILENCE_MS;
}

// --- Opening the page from the PomoBox (Settings > PC Mirror) ---
// The PomoBox prints this log line; the PomoBox helper on the PC (tools/pc-helper) lets go of the port and opens the
// page, which connects by itself. Printable ASCII, so the page never reads it as a frame.

static constexpr const char *MIRROR_OPEN_REQUEST = "@pomobox open-mirror";
// Printed in reply to hello, before the frames: zeroFlash reads which version a PomoBox runs from it
static constexpr const char *MIRROR_VERSION_LINE = "@pomobox version ";
static constexpr uint32_t    MIRROR_OPEN_WAIT_MS = 15000;  // asked this long ago with no page: say nobody answered

enum class MirrorLink { IDLE, OPENING, NO_ANSWER, CONNECTED };

inline MirrorLink mirrorLinkState(bool connected, bool asked, uint32_t askedMs, uint32_t now) {
  if (connected) return MirrorLink::CONNECTED;
  if (!asked) return MirrorLink::IDLE;
  return (uint32_t)(now - askedMs) < MIRROR_OPEN_WAIT_MS ? MirrorLink::OPENING : MirrorLink::NO_ANSWER;
}
