#include "exception.h"

#include <stdio.h>

#include "idt.h"
#include "panic.h"

//! divide by 0 fault
void divide_by_zero_fault(interrupt_registers *registers) {
  /*
  _asm {
    cli
    add esp, 12
    pushad
  }
  */

  kernel_panic(
      "Divide by 0 at physical address [0x%x:0x%x] EFLAGS [0x%x] other: 0x%x",
      registers->cs,
      registers->eip,
      registers->eflags);
  for (;;)
    ;
}

//! single step
void single_step_trap(struct interrupt_registers *registers) {
  kernel_panic("Single step");
  for (;;)
    ;
}

//! non maskable trap
void nmi_trap(struct interrupt_registers *registers) {
  kernel_panic("NMI trap");
  for (;;)
    ;
}

//! breakpoint hit
void breakpoint_trap(struct interrupt_registers *registers) {
  kernel_panic("Breakpoint trap");
  for (;;)
    ;
}

//! overflow
void overflow_trap(struct interrupt_registers *registers) {
  kernel_panic("Overflow trap");
  for (;;)
    ;
}

//! bounds check
void bounds_check_fault(struct interrupt_registers *registers) {
  kernel_panic("Bounds check fault");
  for (;;)
    ;
}

//! invalid opcode / instruction
void invalid_opcode_fault(struct interrupt_registers *registers) {
  kernel_panic("Invalid opcode");
  for (;;)
    ;
}

//! device not available
void no_device_fault(struct interrupt_registers *registers) {
  kernel_panic("Device not found");
  for (;;)
    ;
}

//! double fault
void double_fault_abort(struct interrupt_registers *registers) {
  kernel_panic("Double fault");
  for (;;)
    ;
}

//! invalid Task State Segment (TSS)
void invalid_tss_fault(struct interrupt_registers *registers) {
  kernel_panic("Invalid TSS");
  for (;;)
    ;
}

//! segment not present
void no_segment_fault(struct interrupt_registers *registers) {
  kernel_panic("Invalid segment");
  for (;;)
    ;
}

//! stack fault
void stack_fault(struct interrupt_registers *registers) {
  kernel_panic("Stack fault");
  for (;;)
    ;
}

//! general protection fault
void general_protection_fault(struct interrupt_registers *registers) {
  kernel_panic("General Protection Fault");
  for (;;)
    ;
}

//! page fault
void page_fault(struct interrupt_registers *registers) {
  //_asm cli _asm sub ebp, 4

  //	int faultAddr=0;

  //	_asm {
  //		mov eax, cr2
  //		mov [faultAddr], eax
  //	}

  //	kernel_panic ("Page Fault at 0x%x:0x%x refrenced memory at 0x%x",
  //		cs, eip, faultAddr);

  //_asm popad _asm sti _asm iretd
}

//! Floating Point Unit (FPU) error
void fpu_fault(struct interrupt_registers *registers) {
  kernel_panic("FPU Fault");
  for (;;)
    ;
}

void i86_default_handler(interrupt_registers *registers) {
  kernel_panic("*** [i86 Hal] i86_default_handler: Unhandled Exception");
  
  for (;;)
    ;
}

//! alignment check
void alignment_check_fault(struct interrupt_registers *registers) {
  kernel_panic("Alignment Check");
  for (;;)
    ;
}

//! machine check
void machine_check_abort(struct interrupt_registers *registers) {
  kernel_panic("Machine Check");
  for (;;)
    ;
}

//! Floating Point Unit (FPU) Single Instruction Multiple Data (SIMD) error
void simd_fpu_fault(struct interrupt_registers *registers) {
  kernel_panic("FPU SIMD fault");
  for (;;)
    ;
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
