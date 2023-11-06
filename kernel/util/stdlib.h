#ifndef UTIL_STDLIB_H
#define UTIL_STDLIB_H

//#include "kernel/util/printf.h"

#define EXIT_FAILURE 1
#define EXIT_SUCCESS 0

__attribute__((__noreturn__)) void abort(void);
void exit(int status);

#endif