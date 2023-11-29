// common.c -- Defines some global functions.
// From JamesM's kernel development tutorials.

#include "kernel/cpu/hal.h"
#include "kernel/cpu/gdt.h"
#include "kernel/cpu/idt.h"
#include "kernel/cpu/pic.h"
#include "kernel/cpu/pit.h"
#include "kernel/cpu/rtc.h"

uint32_t _sel = 0x8;

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

void interruptdone(uint32_t intno) {
  //! insure its a valid hardware irq
  if (intno < IRQ0 || intno > IRQ15)
    return;

  //! test if we need to send end-of-interrupt to second pic
  if (intno >= IRQ8)
    i86_pic_send_command(I86_PIC_OCW2_MASK_EOI, 1);

  //! always send end-of-interrupt to primary pic
  i86_pic_send_command(I86_PIC_OCW2_MASK_EOI, 0);
}

//! shutdown hardware devices
int32_t hal_shutdown () {
	return 0;
}

uint32_t hal_initialize() {
  //! disable hardware interrupts
  disable_interrupts();

  i86_gdt_initialize();
  i86_idt_initialize(_sel);
  i86_pic_initialize(0x20, 0x28);
  rtc_init();
  i86_pit_initialize();
  
  i86_pit_start_counter(100, I86_PIT_OCW_COUNTER_0, I86_PIT_OCW_MODE_SQUAREWAVEGEN);

  //! enable interrupts
  enable_interrupts();

  return 0;
}

uint32_t get_tick_count() {
  return i86_pit_get_tick_count();
}

//! sets new interrupt vector
void setvect(int intno, void (vect)(), int flags) {
	//! install interrupt handler! This overwrites prev interrupt descriptor
	i86_install_ir(intno, vect, _sel, I86_IDT_DESC_PRESENT | I86_IDT_DESC_BIT32 | flags);
}

//! returns current interrupt vector
void (*getvect(int intno))() {
	//! get the descriptor from the idt
	idt_entry_t* desc = i86_get_ir(intno);
	if (!desc)
		return 0;

	//! get address of interrupt handler
	uint32_t addr = desc->base_lo | (desc->base_hi << 16);

	//! return interrupt handler
	I86_IRQ_HANDLER irq = (I86_IRQ_HANDLER)addr;
	return irq;
}