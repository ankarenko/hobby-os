#include <stdio.h>

#if defined(__is_libk)
#include <kernel/devices/terminal.h>
#endif

int popchar() {
#if defined(__is_libk)
  terminal_popchar();
#else
  // TODO: Implement stdio and the write system call.
#endif
}

int putchar(int ic) {
#if defined(__is_libk)
  char c = (char)ic;
  terminal_write(&c, sizeof(c));
#else
  // TODO: Implement stdio and the write system call.
#endif
  return ic;
}