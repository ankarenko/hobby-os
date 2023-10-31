#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#include "kernel/proc/task.h"
#include "kernel/cpu/idt.h"
#include "kernel/system/sysapi.h"

static int32_t sys_debug_printf(const char *format, va_list args) {
  int i = vprintf(format, args);
  return i;
}

static int32_t sys_read(uint32_t fd, char *buf, size_t count) {
	return vfs_fread(fd, buf, count);
}

static int32_t sys_open(const char *path, int32_t flags, mode_t mode) {
  return vfs_open(path, flags, mode);
}

static int32_t sys_fstat(int32_t fd, struct kstat *stat) {
	return vfs_fstat(fd, stat);
}

static void* sys_sbrk(size_t n) {
  thread* th = get_current_thread();
  process* parent = th->parent;
  virtual_addr addr = sbrk(
    n, parent->mm_mos
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
  [__NR_open] = sys_open,
  [__NR_read] = sys_read,
  [__NR_fstat] = sys_fstat,
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