#include <test/greatest.h>

#include "kernel/util/path.h"

TEST TEST_PATH(void) {
  unsigned char out[100];
  simplify_path("/a/\0", out);
  ASSERT_EQ(strcmp(out, "/a/"), 0);

  simplify_path("/a/../../\0", out);
  ASSERT_EQ(strcmp(out, "/\0"), 0);

  simplify_path("/a/../b/c/../\0", out);
  ASSERT_EQ(strcmp(out, "/b/\0"), 0);
  simplify_path("/../../../", out);
  ASSERT_EQ(strcmp(out, "/\0"), 0);
  
  simplify_path("/././../.././", out);
  ASSERT_EQ(strcmp(out, "/\0"), 0);

  simplify_path("/diary/", out);
  ASSERT_EQ(strcmp(out, "/diary/"), 0);

  PASS();
}

SUITE(SUITE_PATH) {
  RUN_TEST(TEST_PATH);
}
