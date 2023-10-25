#include <string.h>

//! copies string s2 to s1
char *strcpy(char *s1, const char *s2) {
  char *s1_p = s1;
  while (*s1++ = *s2++)
    ;
  return s1_p;
}

//! locates first occurance of character in string
char *strchr(char *str, int32_t character) {
  do {
    if (*str == character)
      return (char *)str;
  } while (*str++);

  return 0;
}

//! locates last occurance of character in string
char *last_strchr(char *str, int32_t character) {
  uint32_t len = strlen(str);
  char* cur = &str[len - 1];
  do {
    if (*cur == character)
      return (char *)cur;
  } while (*cur--);

  return 0;
}