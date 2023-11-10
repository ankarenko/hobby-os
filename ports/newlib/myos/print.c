#include <sys/errno.h>
#include <stdarg.h>
#include <stdint.h>

#include "_syscall.h"
#include "print.h"

_syscall2(print, char*, va_list);
void print(char* format, ...) {
  va_list list;
  va_start(list, format);
  SYSCALL_RETURN(syscall_print(format, list));
  va_end(list);
}