#include <stdio.h>
#include <unistd.h>
#include "../proc/task.h"
#include "../cpu/idt.h"
#include "sysapi.h"

static int32_t sys_debug_printf(const char *format, va_list args) {
  int i = vprintf(format, args);
  return i;
}

static void* sys_sbrk(size_t n) {
  thread* th = get_current_thread();
  process* parent = th->parent;
  virtual_addr addr = sbrk(
    n, &parent->mm_mos->brk, 
    &parent->mm_mos->remaning, 
    I86_PTE_WRITABLE | I86_PTE_PRESENT | I86_PTE_USER
  );
  return addr;
}

static int32_t sys_debug_terminate() {
  thread* cur_thread = get_current_thread();
  thread_kill(cur_thread->tid);
  return 1;
}
 
static int32_t sys_debug_sleep_thread(uint32_t msec) {
  thread_sleep(msec);
  return 1;
}

static void *syscalls[] = {
  [__NR_print] = sys_debug_printf,
  [__NR_terminate] = sys_debug_terminate,
  [__NR_sleep] = sys_debug_sleep_thread,
  [__NR_sbrk] = sys_sbrk,
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