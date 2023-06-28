#ifndef _STDLIB_H
#define _STDLIB_H 1

#include <sys/cdefs.h>

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0


void exit(int status);

__attribute__((__noreturn__)) void abort(void);

#endif