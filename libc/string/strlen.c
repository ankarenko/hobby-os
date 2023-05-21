#include "../include/string.h"

size_t strlen(const char* str) {
  size_t len = 0;
  while (str[len])
    len++;
  return len;
}

int32_t strcmp(const char* str1, const char* str2) {
  int32_t res = 0;
  while (!(res = *(unsigned char*)str1 - *(unsigned char*)str2) && *str2)
    ++str1, ++str2;

  if (res < 0)
    res = -1;

  if (res > 0)
    res = 1;

  return res;
}