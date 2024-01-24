#include <stdio.h>
#include <stdlib.h>

__attribute__((__noreturn__)) void abort(void) {
#if defined(__is_libk)
  // TODO: Add proper kernel PANIC.
  printf("kernel: PANIC: abort()\n");
  asm volatile("hlt");
#else
  // TODO: Abnormally terminate the struct processas if by SIGABRT.
  printf("abort()\n");
#endif
  while (1) {
  }
  __builtin_unreachable();
}