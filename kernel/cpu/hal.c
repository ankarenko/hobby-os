// common.c -- Defines some global functions.
// From JamesM's kernel development tutorials.

#include "hal.h"

#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "pit.h"

char *i86_cpu_get_vender() {
  static char vender[32] = {0};
  __asm__ __volatile__(
      "mov $0, %%eax;				\n"
      "cpuid;								\n"
      "lea (%0), %%eax;			\n"
      "mov %%ebx, (%%eax);		\n"
      "mov %%edx, 0x4(%%eax);\n"
      "mov %%ecx, 0x8(%%eax)	\n"
      : "=m"(vender));
  return vender;
}

void cpuid(int code, uint32_t *a, uint32_t *d) {
  asm volatile("cpuid"
               : "=a"(*a), "=d"(*d)
               : "a"(code)
               : "ecx", "ebx");
}

uint32_t hal_initialize() {
  //! disable hardware interrupts
  disable_interrupts();

  i86_gdt_initialize();
  i86_idt_initialize(0x8);
  i86_pic_initialize(0x20, 0x28);
  i86_pit_initialize();
  i86_pit_start_counter(100, I86_PIT_OCW_COUNTER_0, I86_PIT_OCW_MODE_SQUAREWAVEGEN);

  //! enable interrupts
  enable_interrupts();

  return 0;
}

uint32_t get_tick_count() {
  return i86_pit_get_tick_count();
}