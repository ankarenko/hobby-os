#ifndef _STDLIB_H
#define _STDLIB_H 1

#include <sys/cdefs.h>
#include <stddef.h>

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0


void exit(int status);

__attribute__((__noreturn__)) void abort(void);
void *malloc(size_t size);
void *realloc(void *ptr, size_t size);
void *calloc(size_t n, size_t size);
void free(void *ptr);

#endif