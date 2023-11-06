#include <stdio.h>

#include "kernel/devices/tty.h"

int popchar() {
  terminal_popchar();
}

int putchar(int ic) {
  char c = (char)ic;
  terminal_write(&c, sizeof(c));
  return ic;
}