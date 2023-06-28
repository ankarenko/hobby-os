#ifndef _STRING_H
#define _STRING_H 1

#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>

int memcmp(const void*, const void*, size_t);
void* memcpy(void* __restrict, const void* __restrict, size_t);
void* memmove(void*, const void*, size_t);
void* memset(void*, int, size_t);
size_t strlen(const char*);
int strncmp(const char *s1, const char *s2, size_t n);
int32_t strcmp (const char* str1, const char* str2);
char *strcpy(char *s1, const char *s2);
char *strchr(char *str, int32_t character);
char *strncat(char *s1, const char *s2, size_t n);
size_t strnlen(const char *str, size_t maxlen);

#endif