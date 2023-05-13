
#include <stdio.h>

#include "hal.h"

//! something is wrong--bail out
void kernel_panic(const char* fmt, ...) {
  disable_interrupts();

  printf(fmt);
  
  for (;;)
    ;
}