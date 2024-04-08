#ifndef NEWLIB_MYOS_MSYSCALLS_H 
#define NEWLIB_MYOS_MSYSCALLS_H 1

#include "_syscall.h"

/*
  CUSTOM SYSCALLS, NOT SUPPORTED BY POSIX
*/

int dbg_ps();
int dbg_log(char* fmt, ...);

#endif