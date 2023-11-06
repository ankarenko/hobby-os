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

int fflush(FILE* stream);
FILE *fopen(const char *filename, const char *mode);
char *fgets(char *s, int n, FILE *stream);
int fgetc(FILE *stream);
size_t fread(void * buffer, size_t size, size_t count, FILE * stream);
int fputc(int c, FILE *stream);
int fclose(FILE *stream);
int fputs(const char *s, FILE *stream);

#endif