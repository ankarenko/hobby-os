#ifndef UTIL_STRING
#define UTIL_STRING

#include <stddef.h>
#include <stdint.h>

#define op_t unsigned long int
#define OPSIZ (sizeof(op_t))

char *strdup(const char *s);
void *memset(void *dest, char val, size_t len);
void *memcpy(void *dest, const void *src, size_t len);
int strcmp(const char *p1, const char *p2);
size_t strlen(const char *str);
char *strcpy(char *dest, const char *src);
int strcmp(const char *p1, const char *p2);
int strncmp(const char *s1, const char *s2, size_t n);
int memcmp(const void *vl, const void *vr, size_t n);
void *memmove(void *dest_ptr, const void *src_ptr, size_t n);
char *strncat(char *s1, const char *s2, size_t n);
size_t strnlen(const char *str, size_t maxlen);
int32_t strliof(const char *s1, const char *s2);
char *strrstr(char *string, char *find);

#endif