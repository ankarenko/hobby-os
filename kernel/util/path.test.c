#include <test/greatest.h>

#include "kernel/util/path.h"

TEST TEST_PATH(void) {
  char* out;
  simplify_path("/", &out);
  ASSERT_EQ(strcmp(out, "/"), 0);
  kfree(out);

  simplify_path("", &out);
  ASSERT_EQ(strcmp(out, ""), 0);
  kfree(out);

  simplify_path("a", &out);
  ASSERT_EQ(strcmp(out, "a/"), 0);
  kfree(out);

  simplify_path("./", &out);
  ASSERT_EQ(strcmp(out, ""), 0);
  kfree(out);

  simplify_path("/a/", &out);
  ASSERT_EQ(strcmp(out, "/a/"), 0);
  kfree(out);

  simplify_path("/a/../../", &out);
  ASSERT_EQ(strcmp(out, "/../"), 0);
  kfree(out);

  simplify_path("/a/../b/c/../", &out);
  ASSERT_EQ(strcmp(out, "/b/"), 0);
  kfree(out);

  simplify_path("/../../../", &out);
  ASSERT_EQ(strcmp(out, "/../../../"), 0);
  kfree(out);
  
  simplify_path("/././../.././", &out);
  ASSERT_EQ(strcmp(out, "/../../"), 0);
  kfree(out);

  simplify_path("/diary/", &out);
  ASSERT_EQ(strcmp(out, "/diary/"), 0);
  kfree(out);

  simplify_path("././../.././", &out);
  ASSERT_EQ(strcmp(out, "../../"), 0);
  kfree(out);

  simplify_path("diary/./", &out);
  ASSERT_EQ(strcmp(out, "diary/"), 0);
  kfree(out);

  PASS();
}

SUITE(SUITE_PATH) {
  RUN_TEST(TEST_PATH);
}
