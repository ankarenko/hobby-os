#ifndef SYSTEM_SYSAPI_H
#define SYSTEM_SYSAPI_H
#define MAX_SYSCALL 1

#include <stdint.h>

void syscall_init();
int32_t syscall_printf(char *);

#endif