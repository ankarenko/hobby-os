// #include <stdio.h>
// #include <stdint.h>
// #include <string.h>
// #include <timer.h>
#include <ctype.h>
#include <math.h>

#include "../test/greatest.h"
#include "./devices/tty.h"
#include "./kernel/cpu/exception.h"
#include "./kernel/cpu/gdt.h"
#include "./kernel/cpu/hal.h"
#include "./kernel/cpu/idt.h"
#include "./kernel/cpu/tss.h"
#include "./kernel/devices/kybrd.h"
#include "./kernel/fs/fat12/fat12.h"
#include "./kernel/fs/flpydsk.h"
#include "./kernel/fs/fsys.h"
#include "./kernel/memory/kernel_info.h"
#include "./kernel/memory/malloc.h"
#include "./kernel/memory/pmm.h"
#include "./kernel/memory/vmm.h"
#include "./kernel/proc/elf.h"
#include "./kernel/proc/task.h"
#include "./kernel/system/sysapi.h"
#include "./multiboot.h"

TEST x_should_equal_1(void) {
  int x = 1;
  /* Compare, with an automatic "1 != x" failure message */
  ASSERT_EQ(1, x);

  /* Compare, with a custom failure message */
  ASSERT_EQm("Yikes, x doesn't equal 1", 1, x);
  PASS();
}

TEST true_is_true(void) {
  ASSERT_EQ(true, true);
  PASS();
}

SUITE(suite) {
  RUN_TEST(x_should_equal_1);
  RUN_TEST(true_is_true);
}

GREATEST_MAIN_DEFS();

void kernel_main(multiboot_info_t* mbd, uint32_t magic) {
  char** argv = 0;
  int argc = 0;

  hal_initialize();
  terminal_initialize();
  kkybrd_install(IRQ1);
  pmm_init(mbd);

  GREATEST_MAIN_BEGIN();

  RUN_SUITE(suite);

  GREATEST_MAIN_END();

  return 0;
}