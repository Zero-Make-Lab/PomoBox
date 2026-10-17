// Host tests for the USB mirror protocol (docs/specs/usb-mirror.md): pio test -e native
#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "mirror_proto.h"

void setUp() {}
void tearDown() {}

// mirror/golden.hex: hex bytes, '#' comments. The tests run from the project folder.
static size_t readGolden(uint8_t *out, size_t cap) {
  FILE *f = fopen("mirror/golden.hex", "r");
  if (!f) return 0;
  size_t n = 0;
  char line[256];
  while (fgets(line, sizeof line, f)) {
    if (line[0] == '#') continue;
    for (char *p = line; *p;) {
      unsigned v;
      int used;
      if (sscanf(p, " %2x%n", &v, &used) != 1) break;
      if (n < cap) out[n++] = (uint8_t)v;
      p += used;
    }
  }
  fclose(f);
  return n;
}

// ---------------------------------------------------------------------------------------------------------------
// PomoBox to PC: frames

void test_encoder_produces_the_golden_stream() {
  uint8_t golden[256];
  size_t gn = readGolden(golden, sizeof golden);
  TEST_ASSERT_TRUE_MESSAGE(gn > 0, "mirror/golden.hex not found from the test's working folder");

  uint8_t got[256];
  size_t n = 0;
  n += mirrorFrameInfo(got + n, 320, 240);
  n += mirrorFrameRect(got + n, 10, 20, 30, 40, 0xF800);
  n += mirrorFramePower(got + n, true);
  n += mirrorFrameRect(got + n, -5, 0, 1, 1, 0x07E0);
  memcpy(got + n, "Hi\n", 3);
  n += 3;
  n += mirrorFrameRect(got + n, 319, 239, 1, 1, 0xFFFF);

  TEST_ASSERT_EQUAL(gn, n);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(golden, got, n);
}

void test_frame_sizes_match_the_buffer_constant() {
  uint8_t b[MIRROR_FRAME_MAX];
  TEST_ASSERT_EQUAL(14, mirrorFrameRect(b, 0, 0, 0, 0, 0));
  TEST_ASSERT_EQUAL(9, mirrorFrameInfo(b, 0, 0));
  TEST_ASSERT_EQUAL(5, mirrorFramePower(b, false));
  TEST_ASSERT_TRUE(MIRROR_FRAME_MAX >= 14);
}

void test_power_off_frame() {
  uint8_t b[MIRROR_FRAME_MAX];
  mirrorFramePower(b, false);
  const uint8_t want[] = {0xA5, 'P', 1, 0, 'P' ^ 1 ^ 0};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(want, b, 5);
}

// ---------------------------------------------------------------------------------------------------------------
// PC to PomoBox: lines

static MirrorCmd feed(MirrorLine &l, const char *s, int32_t &turn) {
  MirrorCmd last = MirrorCmd::NONE;
  for (; *s; s++) {
    MirrorCmd c = mirrorFeed(l, *s, turn);
    if (c != MirrorCmd::NONE) last = c;
  }
  return last;
}

void test_each_command_line() {
  MirrorLine l{};
  int32_t t = 0;
  TEST_ASSERT_TRUE(feed(l, "hello\n", t) == MirrorCmd::HELLO);
  TEST_ASSERT_TRUE(feed(l, "ping\n", t) == MirrorCmd::PING);
  TEST_ASSERT_TRUE(feed(l, "click\n", t) == MirrorCmd::CLICK);
  TEST_ASSERT_TRUE(feed(l, "hold\n", t) == MirrorCmd::HOLD);
  TEST_ASSERT_TRUE(feed(l, "bye\n", t) == MirrorCmd::BYE);
  TEST_ASSERT_TRUE(feed(l, "turn -3\n", t) == MirrorCmd::TURN);
  TEST_ASSERT_EQUAL(-3, t);
  TEST_ASSERT_TRUE(feed(l, "turn 1\r\n", t) == MirrorCmd::TURN);  // CRLF from a terminal is fine
  TEST_ASSERT_EQUAL(1, t);
}

void test_junk_and_out_of_range_lines_do_nothing() {
  MirrorLine l{};
  int32_t t = 99;
  TEST_ASSERT_TRUE(feed(l, "helloo\n", t) == MirrorCmd::NONE);
  TEST_ASSERT_TRUE(feed(l, "turn\n", t) == MirrorCmd::NONE);
  TEST_ASSERT_TRUE(feed(l, "turn 0\n", t) == MirrorCmd::NONE);
  TEST_ASSERT_TRUE(feed(l, "turn 21\n", t) == MirrorCmd::NONE);
  TEST_ASSERT_TRUE(feed(l, "turn -21\n", t) == MirrorCmd::NONE);
  TEST_ASSERT_TRUE(feed(l, "turn 2x\n", t) == MirrorCmd::NONE);
  TEST_ASSERT_TRUE(feed(l, "\n", t) == MirrorCmd::NONE);
  TEST_ASSERT_EQUAL(99, t);
}

void test_an_overlong_line_is_dropped_whole() {
  MirrorLine l{};
  int32_t t = 0;
  // 70 characters ending in "click": must not be read as a click
  TEST_ASSERT_TRUE(feed(l, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxclick\n", t) == MirrorCmd::NONE);
  TEST_ASSERT_TRUE(feed(l, "click\n", t) == MirrorCmd::CLICK);  // and the next line is read normally
}

// ---------------------------------------------------------------------------------------------------------------
// Session

void test_session_runs_until_three_seconds_of_silence_or_bye() {
  MirrorSession s{};
  TEST_ASSERT_FALSE(s.active);
  mirrorHeard(s, MirrorCmd::HELLO, 1000);
  TEST_ASSERT_TRUE(s.active);
  mirrorHeard(s, MirrorCmd::PING, 3500);
  TEST_ASSERT_FALSE(mirrorExpired(s, 6500));
  TEST_ASSERT_TRUE(mirrorExpired(s, 6501));
  mirrorHeard(s, MirrorCmd::BYE, 4000);
  TEST_ASSERT_FALSE(s.active);
  TEST_ASSERT_FALSE(mirrorExpired(s, 99999));  // nothing to expire
}

void test_session_survives_millis_wrapping() {
  MirrorSession s{};
  mirrorHeard(s, MirrorCmd::HELLO, 0xFFFFFF00u);
  TEST_ASSERT_FALSE(mirrorExpired(s, 0x00000100u));  // 512 ms later
}

void test_input_lines_do_not_start_a_session() {
  MirrorSession s{};
  mirrorHeard(s, MirrorCmd::CLICK, 100);
  mirrorHeard(s, MirrorCmd::PING, 100);
  TEST_ASSERT_FALSE(s.active);  // only hello starts one
}

// ---------------------------------------------------------------------------------------------------------------
// PC Mirror screen: asking the PC helper to open the page

void test_open_request_is_one_plain_log_line() {
  TEST_ASSERT_EQUAL_STRING("@pomobox open-mirror", MIRROR_OPEN_REQUEST);
  for (const char *c = MIRROR_OPEN_REQUEST; *c; c++) {
    TEST_ASSERT_TRUE(*c >= 0x20 && *c < 0x7F);  // printable ASCII: never mistaken for a frame (0xA5)
  }
}

void test_version_line_is_plain_text_zeroflash_can_read() {
  TEST_ASSERT_EQUAL_STRING("@pomobox version ", MIRROR_VERSION_LINE);
  for (const char *c = MIRROR_VERSION_LINE; *c; c++) TEST_ASSERT_TRUE(*c >= 0x20 && *c < 0x7F);
}

void test_link_state_on_the_pc_mirror_screen() {
  TEST_ASSERT_TRUE(mirrorLinkState(false, false, 0, 5000) == MirrorLink::IDLE);
  TEST_ASSERT_TRUE(mirrorLinkState(false, true, 1000, 1000 + MIRROR_OPEN_WAIT_MS - 1) == MirrorLink::OPENING);
  TEST_ASSERT_TRUE(mirrorLinkState(false, true, 1000, 1000 + MIRROR_OPEN_WAIT_MS) == MirrorLink::NO_ANSWER);
  TEST_ASSERT_TRUE(mirrorLinkState(true, true, 1000, 1000 + MIRROR_OPEN_WAIT_MS * 9) == MirrorLink::CONNECTED);
  TEST_ASSERT_TRUE(mirrorLinkState(true, false, 0, 0) == MirrorLink::CONNECTED);
  TEST_ASSERT_TRUE(mirrorLinkState(false, true, 0xFFFFFF00u, 0x00000100u) == MirrorLink::OPENING);  // millis wraps
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_encoder_produces_the_golden_stream);
  RUN_TEST(test_frame_sizes_match_the_buffer_constant);
  RUN_TEST(test_power_off_frame);
  RUN_TEST(test_each_command_line);
  RUN_TEST(test_junk_and_out_of_range_lines_do_nothing);
  RUN_TEST(test_an_overlong_line_is_dropped_whole);
  RUN_TEST(test_session_runs_until_three_seconds_of_silence_or_bye);
  RUN_TEST(test_session_survives_millis_wrapping);
  RUN_TEST(test_input_lines_do_not_start_a_session);
  RUN_TEST(test_open_request_is_one_plain_log_line);
  RUN_TEST(test_link_state_on_the_pc_mirror_screen);
  RUN_TEST(test_version_line_is_plain_text_zeroflash_can_read);
  return UNITY_END();
}
