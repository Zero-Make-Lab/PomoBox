// Host tests for updating the firmware over USB without the BOOT button (docs/specs/usb-mirror.md): pio test -e native
#include <string.h>
#include <unity.h>

#include "mirror_proto.h"
#include "serial_update.h"

void setUp() {}
void tearDown() {}

static const char *MD5 = "0123456789abcdef0123456789abcdef";

void test_a_valid_update_command() {
  UpdateRequest r{};
  char line[80] = "update 334992 ";
  strcat(line, MD5);
  TEST_ASSERT_TRUE(parseUpdateCommand(line, r));
  TEST_ASSERT_EQUAL_UINT32(334992, r.size);
  TEST_ASSERT_EQUAL_STRING(MD5, r.md5);
}

void test_update_commands_that_are_refused() {
  UpdateRequest r{};
  TEST_ASSERT_FALSE(parseUpdateCommand("update 0 0123456789abcdef0123456789abcdef", r));         // empty image
  TEST_ASSERT_FALSE(parseUpdateCommand("update 9999999 0123456789abcdef0123456789abcdef", r));   // past the slot
  TEST_ASSERT_FALSE(parseUpdateCommand("update 1000 0123456789abcdef0123456789abcde", r));       // MD5 too short
  TEST_ASSERT_FALSE(parseUpdateCommand("update 1000 0123456789abcdef0123456789abcdeff", r));     // too long
  TEST_ASSERT_FALSE(parseUpdateCommand("update 1000 0123456789ABCDEF0123456789abcdeg", r));      // not hex
  TEST_ASSERT_FALSE(parseUpdateCommand("update 1000x 0123456789abcdef0123456789abcdef", r));     // size not a number
  TEST_ASSERT_FALSE(parseUpdateCommand("update -5 0123456789abcdef0123456789abcdef", r));
  TEST_ASSERT_FALSE(parseUpdateCommand("update 1000", r));
  TEST_ASSERT_FALSE(parseUpdateCommand("updates 1000 0123456789abcdef0123456789abcdef", r));
}

void test_uppercase_md5_is_accepted_as_lowercase() {
  UpdateRequest r{};
  TEST_ASSERT_TRUE(parseUpdateCommand("update 1000 0123456789ABCDEF0123456789ABCDEF", r));
  TEST_ASSERT_EQUAL_STRING(MD5, r.md5);  // the ESP32 Update library compares lowercase
}

void test_the_line_reader_holds_an_update_command() {
  // "update <size> <md5>" is 47 characters: past the old 31-character line limit
  MirrorLine l{};
  int32_t turn = 0;
  const char *s = "update 334992 0123456789abcdef0123456789abcdef\n";
  MirrorCmd cmd = MirrorCmd::NONE;
  for (; *s; s++) {
    MirrorCmd c = mirrorFeed(l, *s, turn);
    if (c != MirrorCmd::NONE) cmd = c;
  }
  TEST_ASSERT_TRUE(cmd == MirrorCmd::UPDATE);
  UpdateRequest r{};
  TEST_ASSERT_TRUE(parseUpdateCommand(l.buf, r));  // the completed line stays in the buffer until the next byte
  TEST_ASSERT_EQUAL_UINT32(334992, r.size);
}

void test_how_much_to_take_from_the_link() {
  TEST_ASSERT_EQUAL_UINT32(1024, updateTake(10000, 0, 5000, 1024));  // capped by the buffer
  TEST_ASSERT_EQUAL_UINT32(300, updateTake(1000, 0, 300, 1024));     // what has arrived
  TEST_ASSERT_EQUAL_UINT32(10, updateTake(1000, 990, 500, 1024));    // never past the image
  TEST_ASSERT_EQUAL_UINT32(0, updateTake(1000, 1000, 500, 1024));
}

void test_acks_every_block_and_at_the_end() {
  TEST_ASSERT_FALSE(updateShouldAck(4095, 10000));
  TEST_ASSERT_TRUE(updateShouldAck(4096, 10000));
  TEST_ASSERT_TRUE(updateShouldAck(8192, 10000));
  TEST_ASSERT_TRUE(updateShouldAck(10000, 10000));  // the last, short block
  TEST_ASSERT_FALSE(updateShouldAck(0, 10000));
}

void test_percent_for_the_progress_bar() {
  TEST_ASSERT_EQUAL(0, updatePercent(0, 334992));
  TEST_ASSERT_EQUAL(50, updatePercent(167496, 334992));
  TEST_ASSERT_EQUAL(100, updatePercent(334992, 334992));
  TEST_ASSERT_EQUAL(100, updatePercent(UPDATE_MAX_SIZE, UPDATE_MAX_SIZE));  // no overflow at the largest image
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_a_valid_update_command);
  RUN_TEST(test_update_commands_that_are_refused);
  RUN_TEST(test_uppercase_md5_is_accepted_as_lowercase);
  RUN_TEST(test_the_line_reader_holds_an_update_command);
  RUN_TEST(test_how_much_to_take_from_the_link);
  RUN_TEST(test_acks_every_block_and_at_the_end);
  RUN_TEST(test_percent_for_the_progress_bar);
  return UNITY_END();
}
