#include <test/greatest.h>

#include "kernel/util/ctype.h"
#include "kernel/util/math.h"
#include "kernel/devices/tty.h"
#include "kernel/cpu/exception.h"
#include "kernel/cpu/gdt.h"
#include "kernel/cpu/hal.h"
#include "kernel/cpu/idt.h"
#include "kernel/cpu/tss.h"
#include "kernel/devices/kybrd.h"
#include "kernel/fs/flpydsk.h"
#include "kernel/memory/kernel_info.h"
#include "kernel/memory/malloc.h"
#include "kernel/memory/pmm.h"
#include "kernel/memory/vmm.h"
#include "kernel/proc/elf.h"
#include "kernel/proc/task.h"
#include "kernel/system/sysapi.h"
#include "./multiboot.h"


SUITE_EXTERN(SUITE_PMM);
SUITE_EXTERN(SUITE_MALLOC);
SUITE_EXTERN(SUITE_LIST);
SUITE_EXTERN(SUITE_PATH);

//! sleeps a little bit. This uses the HALs get_tick_count() which in turn uses the PIT
void sleep(uint32_t ms) {
  int32_t ticks = ms + get_tick_count();
  while (ticks > get_tick_count())
    ;
}

GREATEST_MAIN_DEFS();

void kernel_main(multiboot_info_t* mbd, uint32_t magic) {
  char** argv = 0;
  int argc = 0;
  GREATEST_MAIN_BEGIN();

  hal_initialize();
  terminal_initialize();
  kkybrd_install(IRQ1);
  pmm_init(mbd);

  
  RUN_SUITE(SUITE_LIST);
  RUN_SUITE(SUITE_PMM);

  vmm_init();
  RUN_SUITE(SUITE_MALLOC);
  RUN_SUITE(SUITE_PATH);
  
  
  GREATEST_MAIN_END();
  return 0;
}