#if defined(__is_libk)
#include <stdio.h>

int puts(const char* string) {
  return printf("%s\n", string);
}

#endif