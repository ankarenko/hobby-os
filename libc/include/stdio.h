#ifndef _STDIO_H
#define _STDIO_H 1

#include <sys/cdefs.h>
#include <stdint.h>

#define EOF (-1)

struct FILE {

};

#define stdin 0
#define stdout 0
#define stderr 0

int printf(const char* __restrict, ...);
int fprintf(struct FILE *stream, const char *format, ...);
int putchar(int);
int fflush(struct FILE* stream);
int puts(const char*);
int32_t atoi(const char* str);

#endif