#include <stdbool.h>
#include <stdint.h>

#include "kernel/util/string/string.h"
#include "kernel/util/math.h"
#include "kernel/util/path.h"
#include "kernel/util/debug.h"
#include "kernel/memory/malloc.h"

bool simplify_path(const char* path, const char** ret) {
  uint32_t len_path = strlen(path);

  if (path == NULL || len_path == 0) {
    *ret = kcalloc(1, sizeof(char));
    memset(*ret, '\0', 1);
    return true;
  }

  uint32_t len_norm = len_path + 1;

  // ensure '/' at the beggining
  bool with_slash_beg = path[0] == '\/';
  bool with_slash_end = path[len_path - 1] == '\/';
  if (!with_slash_beg) {
    ++len_norm;
  }

  if (!with_slash_end) {
    ++len_norm;
  }

  uint8_t* normalized_path = kcalloc(len_norm, sizeof(char));

  if (!with_slash_beg) {
    memcpy(&normalized_path[1], path, len_path);
    normalized_path[0] = '\/';
  } else {
    memcpy(normalized_path, path, len_path);
  }

  // ensure '/' at the end of a string
  if (!with_slash_end) {
    normalized_path[len_norm - 2] = '\/';
    normalized_path[len_norm - 1] = '\0';
  } else {
    normalized_path[len_norm - 1] = '\0';
  }

  uint8_t* out = kcalloc(len_norm, sizeof(char));
  char* cur = normalized_path;

  int32_t i = 0;
  int32_t j = 0;
  int32_t dot_count = 0;

  // ensure normalized_path is started with '/' and ended with '/'
  while (cur[j] != '\0') {
    if (cur[j] == '.') {
      if (j == 0 || cur[j - 1] == '\/' || cur[j - 1] == '.') {
        ++dot_count;
      } else { // better to simplify
        out[i] = cur[j];
        dot_count = 0;
        ++i;
      }
    } else if (cur[j] == '\/' && dot_count == 1) { // case /./
      dot_count = 0;
    } else if (cur[j] == '\/' && dot_count == 2) { // case /../
      assert(i > 0 && j >= 3);
      if (i == 1 || out[i - 2] == '.') { // nothing to reduce
        memcpy(&out[i], &cur[j - 2], 3);  // just copy '../' to output string
        i += 3;
      } else {
        --i;
        do { --i; } while(out[i] != '\/');
        ++i;
        assert(i > 0);
      }
      dot_count = 0;
    } else {
      out[i] = cur[j];
      dot_count = 0;
      ++i;
    }
    ++j;
  }
  assert(i < len_norm);
  out[i] = '\0';
  *ret = out;
  kfree(normalized_path);

  if (!with_slash_beg) {
    *ret = kcalloc(i, sizeof(uint8_t));
    memcpy(*ret, &out[1], i);
    kfree(out);
  }

  return true;
}