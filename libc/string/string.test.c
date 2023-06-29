#include <test/greatest.h>
#include <string.h>

TEST TEST_STRLEN(void) {
  ASSERT_EQ(strlen("hello"), 5);
  ASSERT_EQ(strlen(""), 0);

  PASS();
}

SUITE(SUITE_LIBC_STRING) {
  RUN_TEST(TEST_STRLEN);
}
