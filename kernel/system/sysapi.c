#include <stdarg.h>

#include "kernel/util/ctype.h"
#include "kernel/util/stdio.h"
#include "kernel/util/errno.h"
#include "kernel/proc/task.h"
#include "kernel/cpu/idt.h"
#include "kernel/system/sysapi.h"
#include "kernel/system/time.h"
#include "kernel/cpu/hal.h"
#include "kernel/ipc/signal.h"
#include "kernel/fs/poll.h"

#define __NR_exit 1
#define __NR_fork 2
#define __NR_read 3
#define __NR_write 4
#define __NR_open 5
#define __NR_close 6
#define __NR_waitpid 7
#define __NR_sbrk 10
#define __NR_execve 11
#define __NR_time 13
#define __NR_lseek 19
#define __NR_getpid 20
#define __NR_kill 37
#define __NR_pipe 42
#define __NR_signal 48
#define __NR_ioctl 54
#define __NR_fcntl 55   
#define __NR_setpgid 57
#define __NR_dup2 63
#define __NR_getpgrp 65
#define __NR_setsid 66
#define __NR_sigaction 67
#define __NR_sigsuspend 72
#define __NR_sigreturn 103
#define __NR_fstat 108
#define __NR_sigprocmask 126
#define __NR_getpgid 132
#define __NR_getsid 147
#define __NR_nanosleep 162
#define __NR_poll 168
#define __NR_waitid 284
#define __NR_print 0

static int32_t sys_pipe(int32_t *fd) {
  return do_pipe(fd);
}

static int32_t sys_debug_printf(const char *format, va_list args) {
  int i = vprintf(format, args);
  return i;
}

static int32_t sys_dup2(int oldfd, int newfd) {
  return dup2(oldfd, newfd);
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

static int32_t sys_waitid(id_type_t idtype, id_t id, struct infop *infop, int options) {
  int ret = do_wait(idtype, id, infop, options);
  return ret >= 0 ? 0 : ret;
}

static void* sys_sbrk(size_t n) {
  struct thread* th = get_current_thread();
  struct process* parent = th->proc;
  virtual_addr addr = sbrk(
    n, parent->mm_mos
  );
  return addr;
}

static int32_t sys_exit() {
  assert_not_implemented("sys_exit is not implemented");
  /*
  struct thread* cur_thread = get_current_thread();
  thread_kill(cur_thread->tid);
  
  while (true) { // wait until terminated
    make_schedule();
  };
  */ 
}

static int32_t sys_execve(const char *pathname, char *const argv[], char *const envp[]) {
	//return process_execve(pathname, argv, envp);
  return -1;
}

static pid_t sys_fork() {
  return -1;
	/*
  struct struct process* child = process_fork(current_process);
	queue_thread(child->struct thread);

	return child->pid;
  */
}

static int32_t sys_waitpid(pid_t pid, int *wstatus, int options) {
  id_type_t idtype;
  struct process *current_process = get_current_process();

  if (pid < -1) {
    idtype = P_PGID;
    pid = -pid;
  } else if (pid == -1)
    idtype = P_ALL;
  else if (pid == 0) {
    idtype = P_PGID;
    pid = current_process->gid;
  } else
    idtype = P_PID;

  if (options & WUNTRACED) {
    options &= ~WUNTRACED;
    options |= /*WED |*/ WSTOPPED;
  }

  struct infop ifp;
  int ret = do_wait(idtype, pid, &ifp, options);
  if (ret <= 0)
    return ret;

  if (wstatus) {
    /*
    if (ifp.si_code & CLD_ED)
      *wstatus = WSED | (ifp.si_status << 8);
    else
    */ 
    if (ifp.si_code & CLD_KILLED || ifp.si_code & CLD_DUMPED) {
      *wstatus = WSSIGNALED;
      if (ifp.si_code & CLD_KILLED)
        *wstatus |= (ifp.si_status << 8);
      if (ifp.si_code & CLD_DUMPED)
        *wstatus |= WSCOREDUMP;
    } else if (ifp.si_code & CLD_STOPPED)
      *wstatus = WSSTOPPED | (ifp.si_status << 8);
    else if (ifp.si_code & CLD_CONTINUED)
      *wstatus = WSCONTINUED;
  }

  return ifp.si_pid;
}
 
static int32_t sys_nanosleep(const struct timespec *req, struct timespec *rem) {
	thread_sleep(req->tv_nsec / 1000);
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

static int32_t sys_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
  return do_sigaction(signum, act, oldact);
}

static int32_t sys_sigprocmask(int how, const sig_t *set, sig_t *oldset) {
  return do_sigprocmask(how, set, oldset);
}

static int32_t sys_setpgid(pid_t pid, pid_t pgid) {
  return setpgid(pid, pgid);
}

/**
 * NOTE: incorrect implementation, go to man setsid for more details
*/
static int32_t sys_setsid() {
  struct process *current_process = get_current_process();
  
  if (current_process->pid == current_process->gid)
    return -1;

  current_process->sid = current_process->gid = current_process->pid;
  current_process->tty = NULL;
  return 0;
}

static int32_t sys_getpgid(pid_t pid) {
  struct process *current_process = get_current_process();
  if (!pid)
    return current_process->gid;

  struct process *p = find_process_by_pid(pid);
  if (!p)
    return -ESRCH;

  return current_process->gid;
}

static int32_t sys_getsid() {
  return get_current_process()->sid;
}

static int32_t sys_getpgrp() {
  return get_current_process()->gid;
}

static int32_t sys_getpid() {
  return get_current_process()->pid;
}

static int32_t sys_ioctl(int fd, unsigned int cmd, unsigned long arg) {
  struct process *current_process = get_current_process();
  struct vfs_file *file = current_process->files->fd[fd];

  if (file && file->f_op->ioctl)
    return file->f_op->ioctl(file->f_dentry->d_inode, file, cmd, arg);

  return -EINVAL;
}

static int32_t sys_poll(struct pollfd *fds, uint32_t nfds, int timeout) {
  return do_poll(fds, nfds, timeout);
}

static int32_t sys_fcntl(int fd, int cmd, unsigned long arg) {
  assert_not_implemented();
  //return do_fcntl(fd, cmd, arg);
}

static void *syscalls[] = {
  [__NR_exit] = sys_exit,
  [__NR_nanosleep] = sys_nanosleep,
  [__NR_read] = sys_read,
  [__NR_write] = sys_write,
  [__NR_open] = sys_open,
  [__NR_sbrk] = sys_sbrk,
  [__NR_getpgid] = sys_getpgid,
  [__NR_execve] = sys_execve,
  [__NR_fork] = sys_fork,
  [__NR_pipe] = sys_pipe,
  [__NR_time] = sys_time,
  [__NR_fstat] = sys_fstat,
  [__NR_dup2] = sys_dup2,
  [__NR_waitpid] = sys_waitpid,
  [__NR_setpgid] = sys_setpgid,
  [__NR_print] = sys_debug_printf,
  [__NR_lseek] = sys_lseek,
  [__NR_poll] = sys_poll,
  [__NR_ioctl] = sys_ioctl,
  [__NR_close] = sys_close,
  [__NR_waitid] = sys_waitid,
  [__NR_setsid] = sys_setsid,
  [__NR_getsid] = sys_getsid,
  [__NR_getpgrp] = sys_getpgrp,
  [__NR_getpid] = sys_getpid,
  [__NR_kill] = sys_kill,
  [__NR_fcntl] = sys_fcntl,
  [__NR_sigaction] = sys_sigaction,
  [__NR_sigprocmask] = sys_sigprocmask,
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