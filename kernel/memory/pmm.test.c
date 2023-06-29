#include <test/greatest.h>

TEST TEST_ONE_IS_ONE(void) {
  ASSERT_EQ(1, 1);
  PASS();
}

SUITE(SUITE_MEMORY_PMM) {
  RUN_TEST(TEST_ONE_IS_ONE);
}
