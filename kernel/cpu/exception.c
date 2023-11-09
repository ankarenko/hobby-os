#include <stddef.h>

#include "kernel/util/stdio.h"
#include "kernel/cpu/exception.h"
#include "kernel/cpu/hal.h"
#include "kernel/cpu/idt.h"
#include "kernel/util/debug.h"

extern uint32_t DEBUG_LAST_TID = 0;
 
//! divide by 0 fault
void divide_by_zero_fault(interrupt_registers *registers) {
  /*
  _asm {
    cli
    add esp, 12
    pushad
  }
  */

  PANIC(
      "Divide by 0 at physical address [0x%x:0x%x] EFLAGS [0x%x] other: 0x%x",
      registers->cs,
      registers->eip,
      registers->eflags);
  for (;;)
    ;
}

//! single step
void single_step_trap(struct interrupt_registers *registers) {
  PANIC("\nException!", NULL);
}

//! non maskable trap
void nmi_trap(struct interrupt_registers *registers) {
  PANIC("NMI trap", NULL);
}

//! breakpoint hit
void breakpoint_trap(struct interrupt_registers *registers) {
  PANIC("Breakpoint trap", NULL);
}

//! overflow
void overflow_trap(struct interrupt_registers *registers) {
  PANIC("Overflow trap", NULL);
}

//! bounds check
void bounds_check_fault(struct interrupt_registers *registers) {
  PANIC("Bounds check fault", NULL);
}

//! invalid opcode / instruction
void invalid_opcode_fault(struct interrupt_registers *registers) {
  PANIC("Invalid opcode", NULL);
}

//! device not available
void no_device_fault(struct interrupt_registers *registers) {
  PANIC("Device not found", NULL);
}

//! double fault
void double_fault_abort(struct interrupt_registers *registers) {
  PANIC("Double fault", NULL);
}

//! invalid Task State Segment (TSS)
void invalid_tss_fault(struct interrupt_registers *registers) {
  PANIC("Invalid TSS", NULL);
}

//! segment not present
void no_segment_fault(struct interrupt_registers *registers) {
  PANIC("Invalid segment", NULL);
}

//! stack fault
void stack_fault(struct interrupt_registers *registers) {
  PANIC("Stack fault", NULL);
}

//! general protection fault
void general_protection_fault(struct interrupt_registers *registers) {
  PANIC("General Protection Fault", NULL);
}

//! page fault
void page_fault(interrupt_registers *registers) {
  //_asm cli _asm sub ebp, 4

  uint32_t faultAddr = 0;
	int error_code = registers->err_code;

	__asm__ __volatile__("mov %%cr2, %%eax	\n"
	 										 "mov %%eax, %0			\n"
	 										 : "=r"(faultAddr));

  PANIC("\nLast tid: %d\nPage Fault at 0x%x\nReason: %s, %s, %s%s%s",
    DEBUG_LAST_TID,
    faultAddr,
    error_code & 0b1 ? "protection violation" : "non-present page",
    error_code & 0b10 ? "write" : "read",
    error_code & 0b100 ? "user mode" : "supervisor mode",
    error_code & 0b1000 ? ", reserved" : "",
    error_code & 0b10000 ? ", instruction fetch" : ""
  );
}

//! Floating Point Unit (FPU) error
void fpu_fault(struct interrupt_registers *registers) {
  PANIC("FPU Fault", NULL);
}

void i86_default_handler(interrupt_registers *registers) {
  PANIC("*** [i86 Hal] i86_default_handler: Unhandled Exception", NULL);
}

//! alignment check
void alignment_check_fault(struct interrupt_registers *registers) {
  PANIC("Alignment Check", NULL);
}

//! machine check
void machine_check_abort(struct interrupt_registers *registers) {
  PANIC("Machine Check", NULL);
}

//! Floating Point Unit (FPU) Single Instruction Multiple Data (SIMD) error
void simd_fpu_fault(struct interrupt_registers *registers) {
  PANIC("FPU SIMD fault", NULL);
}

void exception_init() {
  for (uint32_t i = 0; i < I86_MAX_INTERRUPTS; ++i) {
    if (i == IRQ0) {  // ignore timer
      continue;
    }
    register_interrupt_handler(i, i86_default_handler);
  }

  register_interrupt_handler(0, (I86_IRQ_HANDLER)divide_by_zero_fault);
  register_interrupt_handler(1, (I86_IRQ_HANDLER)single_step_trap);
  register_interrupt_handler(2, (I86_IRQ_HANDLER)nmi_trap);
  register_interrupt_handler(3, (I86_IRQ_HANDLER)breakpoint_trap);
  register_interrupt_handler(4, (I86_IRQ_HANDLER)overflow_trap);
  register_interrupt_handler(5, (I86_IRQ_HANDLER)bounds_check_fault);
  register_interrupt_handler(6, (I86_IRQ_HANDLER)invalid_opcode_fault);
  register_interrupt_handler(7, (I86_IRQ_HANDLER)no_device_fault);
  register_interrupt_handler(8, (I86_IRQ_HANDLER)double_fault_abort);
  register_interrupt_handler(10, (I86_IRQ_HANDLER)invalid_tss_fault);
  register_interrupt_handler(11, (I86_IRQ_HANDLER)no_segment_fault);
  register_interrupt_handler(12, (I86_IRQ_HANDLER)stack_fault);
  register_interrupt_handler(13, (I86_IRQ_HANDLER)general_protection_fault);
  register_interrupt_handler(14, (I86_IRQ_HANDLER)page_fault);
  register_interrupt_handler(16, (I86_IRQ_HANDLER)fpu_fault);
  register_interrupt_handler(17, (I86_IRQ_HANDLER)alignment_check_fault);
  register_interrupt_handler(18, (I86_IRQ_HANDLER)machine_check_abort);
  register_interrupt_handler(19, (I86_IRQ_HANDLER)simd_fpu_fault);
}
