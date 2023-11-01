#ifndef _STDIO_H
#define _STDIO_H 1

#include <sys/cdefs.h>
#include <stdint.h>
#include <stddef.h>
#include <FILE.h>

#define EOF (-1)

typedef struct __FILE FILE;

#define stdin 0
#define stdout 0
#define stderr 0

int printf(const char* __restrict, ...);
int fprintf(FILE *stream, const char *format, ...);
int putchar(int);
int fflush(FILE* stream);
int puts(const char*);
int32_t atoi(const char* str);

FILE *fopen(const char *filename, const char *mode);
char *fgets(char *s, int n, FILE *stream);
int fgetc(FILE *stream);
size_t fread(void * buffer, size_t size, size_t count, FILE * stream);

#endif