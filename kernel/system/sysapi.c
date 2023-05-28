#include "sysapi.h"

#include <stdio.h>

#include "../cpu/idt.h"

#define __NR_dprintf 512

static int32_t sys_debug_printf(/*enum debug_level level,*/ const char *out) {
  printf(out);
  return 1;
}

// In x86-32 parameters for Linux system call are passed using 
// registers. %eax for syscall_number. %ebx, %ecx, %edx, %esi, 
// %edi, %ebp are used for passing 6 parameters to system calls.

// The return value is in %eax. All other registers 
// (including EFLAGS) are preserved across the int $0x80.
int32_t syscall_printf(char *str) {
  int32_t a = __NR_dprintf;

  // TODO: not sure if it is a right way to provide params, but it works
  __asm__ __volatile__(
      "mov %0, %%ebx;			  \n"
      "mov %1, %%eax;	      \n"
      "int $0x80;           \n"
      : "=m"(str)
      : "a"(a)
  );

  return a;
}

static void *syscalls[] = {
    [__NR_dprintf] = sys_debug_printf,
    0
};

static int32_t syscall_dispatcher(interrupt_registers *regs) {
  int idx = regs->eax;

  uint32_t (*func)(unsigned int, ...) = syscalls[idx];

  if (!func)
    return IRQ_HANDLER_STOP;

  // memcpy(&current_thread->uregs, regs, sizeof(struct interrupt_registers));

  uint32_t ret = func(regs->ebx, regs->ecx, regs->edx, regs->esi, regs->edi);
  regs->eax = ret;

  return IRQ_HANDLER_CONTINUE;
}

void syscall_init() {
  register_interrupt_handler(DISPATCHER_ISR, syscall_dispatcher);
}