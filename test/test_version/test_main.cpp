// Host tests for the version number: pio test -e native
#include <unity.h>

#include "version.h"

void setUp() {}
void tearDown() {}

void test_version_code_packs_each_part() {
  TEST_ASSERT_EQUAL_UINT32(10100, versionCode("1.1.0"));
  TEST_ASSERT_EQUAL_UINT32(10001, versionCode("1.0.1"));
  TEST_ASSERT_EQUAL_UINT32(11003, versionCode("1.10.3"));
  TEST_ASSERT_EQUAL_UINT32(20000, versionCode("2.0.0"));
}

void test_newer_versions_have_bigger_codes() {
  TEST_ASSERT_TRUE(versionCode("1.0.9") < versionCode("1.1.0"));
  TEST_ASSERT_TRUE(versionCode("1.9.9") < versionCode("1.10.0"));
}

void test_the_built_version_is_compiled_in() {
  static_assert(POMOBOX_VERSION_CODE == versionCode(POMOBOX_VERSION), "computed at compile time");
  TEST_ASSERT_TRUE(POMOBOX_VERSION_CODE > 10000);  // 1.0.0 was the first numbered build
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_version_code_packs_each_part);
  RUN_TEST(test_newer_versions_have_bigger_codes);
  RUN_TEST(test_the_built_version_is_compiled_in);
  return UNITY_END();
}
