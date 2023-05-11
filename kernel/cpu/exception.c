#include "exception.h"

#include "./hal.h"
#include "./idt.h"
#include "../devices/tty.h"
// #include <cdefs.h>
//#include <utils/debug.h>

#define kernel_panic(fmt, ...) ({ disable_interrupts(); terminal_writestring(fmt); terminal_writestring("\n");  __asm__ __volatile__("int $0x01"); })

static int32_t divide_by_zero_fault(struct interrupt_registers *regs)
{
	kernel_panic("Divide by 0");
	return IRQ_HANDLER_STOP;
}

static int32_t single_step_trap(struct interrupt_registers *regs)
{
  while (1);
  
	kernel_panic("Single step");
	return IRQ_HANDLER_STOP;
}

static int32_t nmi_trap(struct interrupt_registers *regs)
{
	kernel_panic("NMI trap");
	return IRQ_HANDLER_STOP;
}

static int32_t breakpoint_trap(struct interrupt_registers *regs)
{
	kernel_panic("Breakpoint trap");
	return IRQ_HANDLER_STOP;
}

static int32_t overflow_trap(struct interrupt_registers *regs)
{
	kernel_panic("Overflow trap");
	return IRQ_HANDLER_STOP;
}

static int32_t bounds_check_fault(struct interrupt_registers *regs)
{
	kernel_panic("Bounds check fault");
	return IRQ_HANDLER_STOP;
}

static int32_t invalid_opcode_fault(struct interrupt_registers *regs)
{
	kernel_panic("Invalid opcode");
	return IRQ_HANDLER_STOP;
}

static int32_t no_device_fault(struct interrupt_registers *regs)
{
	kernel_panic("Device not found");
	return IRQ_HANDLER_STOP;
}

static int32_t double_fault_abort(struct interrupt_registers *regs)
{
	kernel_panic("Double fault");
	return IRQ_HANDLER_STOP;
}

static int32_t invalid_tss_fault(struct interrupt_registers *regs)
{
	kernel_panic("Invalid TSS");
	return IRQ_HANDLER_STOP;
}

static int32_t no_segment_fault(struct interrupt_registers *regs)
{
	kernel_panic("Invalid segment");
	return IRQ_HANDLER_STOP;
}

static int32_t stack_fault(struct interrupt_registers *regs)
{
	kernel_panic("Stack fault");
	return IRQ_HANDLER_STOP;
}

static int32_t general_protection_fault(struct interrupt_registers *regs)
{
	kernel_panic("General Protection Fault");
	return IRQ_HANDLER_STOP;
}

static int32_t fpu_fault(struct interrupt_registers *regs)
{
	kernel_panic("FPU Fault");
	return IRQ_HANDLER_STOP;
}

static int32_t alignment_check_fault(struct interrupt_registers *regs)
{
	kernel_panic("Alignment Check");
	return IRQ_HANDLER_STOP;
}

static int32_t machine_check_abort(struct interrupt_registers *regs)
{
	kernel_panic("Machine Check");
	return IRQ_HANDLER_STOP;
}

static int32_t simd_fpu_fault(struct interrupt_registers *regs)
{
	kernel_panic("FPU SIMD fault");
	return IRQ_HANDLER_STOP;
}

/*
static int32_t page_fault(interrupt_registers regs)
{
  // A page fault has occurred.
  // The faulting address is stored in the CR2 register.
  uint32_t faulting_address;
  asm volatile("mov %%cr2, %0" : "=r" (faulting_address));

  // The error code gives us details of what happened.
  int present   = !(regs.err_code & 0x1); // Page not present
  int rw = regs.err_code & 0x2;           // Write operation?
  int us = regs.err_code & 0x4;           // Processor was in user-mode?
  int reserved = regs.err_code & 0x8;     // Overwritten CPU-reserved bits of page entry?
  int id = regs.err_code & 0x10;          // Caused by an instruction fetch?

  // Output an error message.
  terminal_writestring("Page fault! ( ");
  if (present) {terminal_writestring("present ");}
  if (rw) {terminal_writestring("read-only ");}
  if (us) {terminal_writestring("user-mode ");}
  if (reserved) {terminal_writestring("reserved ");}
  terminal_writestring(") at 0x");
  terminal_writestring(faulting_address); // TODO: hex writestring_hex
  terminal_writestring("\n");
  terminal_writestring("Page fault");
  kernel_panic("Page Fault");
	return IRQ_HANDLER_STOP;
}
*/

void exception_init()
{
	terminal_writestring("Exception: Initializing\n");

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
	//register_interrupt_handler(14, (I86_IRQ_HANDLER)page_fault);
	register_interrupt_handler(16, (I86_IRQ_HANDLER)fpu_fault);
	register_interrupt_handler(17, (I86_IRQ_HANDLER)alignment_check_fault);
	register_interrupt_handler(18, (I86_IRQ_HANDLER)machine_check_abort);
	register_interrupt_handler(19, (I86_IRQ_HANDLER)simd_fpu_fault);

	terminal_writestring("Exception: Done\n");
}
