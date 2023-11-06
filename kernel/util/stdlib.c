#include "kernel/util/stdlib.h"

__attribute__((__noreturn__)) void abort(void) {
  //printf("kernel: PANIC: abort()\n");
  asm volatile("hlt");
  while (1) {}
  //__builtin_unreachable();
}

void exit(int status) {}