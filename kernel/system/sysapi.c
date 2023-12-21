#include <stdarg.h>

#include "kernel/util/ctype.h"
#include "kernel/util/stdio.h"
#include "kernel/util/errno.h"
#include "kernel/proc/task.h"
#include "kernel/cpu/idt.h"
#include "kernel/system/sysapi.h"
#include "kernel/system/time.h"
#include "kernel/cpu/hal.h"

#define __NR_exit 1
#define __NR_fork 2
#define __NR_read 3
#define __NR_write 4
#define __NR_open 5
#define __NR_close 6
#define __NR_sbrk 10
#define __NR_execve 11
#define __NR_time 13
#define __NR_lseek 19
#define __NR_getpid 20
#define __NR_kill 37
#define __NR_fstat 108
#define __NR_nanosleep 162
#define __NR_print 0


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

static int32_t sys_write(uint32_t fd, char *buf, size_t count) {
	return vfs_fwrite(fd, buf, count);
}

static void* sys_sbrk(size_t n) {
  thread* th = get_current_thread();
  process* parent = th->parent;
  virtual_addr addr = sbrk(
    n, parent->mm_mos
  );
  return addr;
}

static int32_t sys_getpid() {
	process* proc = get_current_process();
  return proc->pid;
}

static int32_t sys_exit() {
  thread* cur_thread = get_current_thread();
  thread_kill(cur_thread->tid);
  
  while (true) { // wait until terminated
    make_schedule();
  }; 
}

static int32_t sys_execve(const char *pathname, char *const argv[], char *const envp[]) {
	//return process_execve(pathname, argv, envp);
  return -1;
}

static pid_t sys_fork() {
  return -1;
	/*
  struct process *child = process_fork(current_process);
	queue_thread(child->thread);

	return child->pid;
  */
}
 
static int32_t sys_nanosleep(const struct timespec *req, struct timespec *rem) {
	thread_sleep(req->tv_nsec / 10000);
	return 0;
}

static int32_t sys_lseek(int fd, off_t offset, int whence) {
	return vfs_flseek(fd, offset, whence);
}

static int32_t sys_close(uint32_t fd) {
	return vfs_close(fd);
}

static int32_t sys_kill(pid_t pid, int sig) {
  return -1;
}

static int32_t sys_time(time_t *tloc) {
	time_t t = get_seconds(NULL);
	if (tloc)
		*tloc = t;
	return t;
}

static void *syscalls[] = {
  [__NR_exit] = sys_exit,
  [__NR_nanosleep] = sys_nanosleep,
  [__NR_read] = sys_read,
  [__NR_write] = sys_write,
  [__NR_open] = sys_open,
  [__NR_sbrk] = sys_sbrk,
  [__NR_execve] = sys_execve,
  [__NR_fork] = sys_fork,
  [__NR_time] = sys_time,
  [__NR_fstat] = sys_fstat,
  [__NR_print] = sys_debug_printf,
  [__NR_lseek] = sys_lseek,
  [__NR_close] = sys_close,
  [__NR_getpid] = sys_getpid,
  [__NR_kill] = sys_kill,
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