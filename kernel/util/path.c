#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include "kernel/util/path.h"

void simplify_path(const char* path, char* out) {
  char* cur = path;

  int32_t i = -1;
  int32_t j = 0;
  int32_t dot_count = 0;

  while (cur[j]) {
    if ((i <= 0 || out[i] == '\/') && cur[j] == '.') {
      ++dot_count;
      goto next;
    } else if ((!cur[j] || cur[j] == '\/') && dot_count == 1) {
      
    } else if ((!cur[j] || cur[j] == '\/') && dot_count == 2) {
      do {
        --i;
      } while(i >= 0 && out[i] != '\/');
      i = max(0, i);
      out[i] = '\/';

    } else {
      ++i;
      out[i] = cur[j];
    }
    dot_count = 0;
next:
    ++j;
  }
  out[i + 1] = '\0';
}