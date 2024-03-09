#include <stdarg.h>

#include "kernel/util/ctype.h"
#include "kernel/util/stdio.h"
#include "kernel/util/errno.h"
#include "kernel/proc/task.h"
#include "kernel/cpu/idt.h"
#include "kernel/memory/malloc.h"
#include "kernel/util/fcntl.h"
#include "kernel/system/sysapi.h"
#include "kernel/system/time.h"
#include "kernel/cpu/hal.h"
#include "kernel/ipc/signal.h"
#include "kernel/fs/poll.h"

/*
  https://filippo.io/linux-syscall-table/
*/

#define __NR_exit 1
#define __NR_fork 2
#define __NR_read 3
#define __NR_write 4
#define __NR_open 5
#define __NR_close 6
#define __NR_waitpid 7
#define __NR_unlink 10
#define __NR_execve 11
#define __NR_chdir 12
#define __NR_time 13
#define __NR_mknod 14
#define __NR_sbrk 18
#define __NR_lseek 19
#define __NR_getpid 20
#define __NR_setuid 23
#define __NR_getuid 24
#define __NR_kill 37
#define __NR_mkdir 39
#define __NR_dup 41
#define __NR_pipe 42
#define __NR_times 43
#define __NR_setgid 46
#define __NR_getgid 47
#define __NR_signal 48
#define __NR_geteuid 49
#define __NR_getegid 50
#define __NR_ioctl 54
#define __NR_fcntl 55   
#define __NR_setpgid 57
#define __NR_vfork 58
#define __NR_umask 60
#define __NR_dup2 63
#define __NR_getppid 64
#define __NR_getpgrp 65
#define __NR_setsid 66
#define __NR_sigaction 67
#define __NR_sigsuspend 72
#define __NR_sigreturn 103
#define __NR_stat 106
#define __NR_fstat 108
#define __NR_sigprocmask 126
#define __NR_getpgid 132
#define __NR_fchdir 133
#define __NR_getdents 141
#define __NR_getsid 147
#define __NR_nanosleep 162
#define __NR_poll 168
#define __NR_getcwd 183
#define __NR_waitid 284
#define __NR_mkdirat 296
#define __NR_mknodat 297
#define __NR_unlinkat 301
// debug
#define __NR_dbg_ps 512

static int32_t sys_pipe(int32_t *fd) {
  return do_pipe(fd);
}

extern void ps(char **argv);
static int32_t sys_dbg_ps() {
  ps(NULL);
  return 0;
}

static int32_t sys_dup2(int oldfd, int newfd) {
  log("sys_dup2");
  return dup2(oldfd, newfd);
}

static int32_t sys_dup(int oldfd) {
  log("sys_dup");
  return dup(oldfd);
}

static int32_t sys_read(uint32_t fd, char *buf, size_t count) {
  //log("sys_read");
	return vfs_fread(fd, buf, count);
}

static int32_t sys_open(const char *path, int32_t flags, mode_t mode) {
  log("sys_open");
  return vfs_open(path, flags, mode);
}

static int32_t sys_fstat(int32_t fd, struct kstat *stat) {
  log("sys_fstat");
	return vfs_fstat(fd, stat);
}

static int32_t sys_write(uint32_t fd, char *buf, int32_t count) {
  // TODO: bug; RESOLVE IT.
  if (count > 2147482620)
    return 0;

  //log("sys_write");
	return vfs_fwrite(fd, buf, count);
}

static int32_t sys_waitid(id_type_t idtype, id_t id, struct infop *infop, int options) {
  int ret = do_wait(idtype, id, infop, options);
  return ret >= 0 ? 0 : ret;
}

static void* sys_sbrk(size_t n) {
  //log("sys_sbrk");
  struct thread* th = get_current_thread();
  struct process* parent = th->proc;
  virtual_addr addr = sbrk(
    n, parent->mm_mos
  );
  return addr;
}

static int32_t sys_getdents(unsigned int fd, struct dirent *dirent, unsigned int count) {
  log("sys_getdents");
  struct process *current_process = get_current_process();
  struct vfs_file *file = current_process->files->fd[fd];

  if (!file)
    return -EBADF;

  if (file->f_op->readdir)
    return file->f_op->readdir(file, dirent, count);

  return -ENOTDIR;
}

static int32_t sys_getcwd(char *buf, size_t size) {
  log("sys_getcwd");
  if (!buf || !size)
    return -EINVAL;
  
  struct process *proc = get_current_process();
  char *path = proc->fs->d_root->d_name;

  int32_t ret = (int32_t)buf;
  int plen = strlen(path);
  if (plen < size) {
    memcpy(buf, path, plen);
    buf[plen] = '\0';
  } else
    ret = -ERANGE;

  return buf;

  /*
  char *abs_path = kcalloc(MAXPATHLEN, sizeof(char));
  vfs_build_path_backward(current_process->fs->d_root, abs_path);

  int32_t ret = (int32_t)buf;
  int plen = strlen(abs_path);
  if (plen < size)
    memcpy(buf, abs_path, plen + 1);
  else
    ret = -ERANGE;

  kfree(abs_path);
  return ret;
  */
}

static int32_t sys_exit() {
  log("sys_exit");
  do_kill(0, SIGQUIT);
  while (1) {}
  
  /*
  struct thread* cur_thread = get_current_thread();
  thread_kill(cur_thread->tid);
  
  while (true) { // wait until terminated
    make_schedule();
  };
  */ 
}

static pid_t sys_fork() {
  log("sys_fork");
  struct process *parent = get_current_process();
  return process_fork(parent);
}

static pid_t sys_vfork() {
  log("sys_vfork");
  return sys_fork();
}

static int32_t sys_waitpid(pid_t pid, int *wstatus, int options) {
  log("sys_waitpid");
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
  log("sys_lseek");
	return vfs_flseek(fd, offset, whence);
}

static int32_t sys_close(uint32_t fd) {
  log("sys_close");
	return vfs_close(fd);
}

static int32_t sys_time(time_t *tloc) {
  log("sys_time");
	time_t t = get_seconds(NULL);
	if (tloc)
		*tloc = t;
	return t;
}

static int32_t sys_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
  log("sys_sigaction");
  return do_sigaction(signum, act, oldact);
}

static int32_t sys_sigprocmask(int how, const sig_t *set, sig_t *oldset) {
  return do_sigprocmask(how, set, oldset);
}

static int32_t sys_setpgid(pid_t pid, pid_t pgid) {
  log("sys_setpid");log("sys_getcwd");
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

static int32_t sys_fchdir(int fildes) {
  struct process *current_process = get_current_process();
  struct vfs_file *filp = current_process->files->fd[fildes];
  if (!filp)
    return -EBADF;

  if (!S_ISDIR(filp->f_mode))
    return -ENOTDIR;
  
  current_process->fs->d_root = filp->f_dentry;
  current_process->fs->mnt_root = filp->f_vfsmnt;
  return 0;
}

static int32_t sys_chdir(const char *path) {
  int ret = vfs_open(path, O_RDONLY);
  if (ret < 0)
    return ret;

  return sys_fchdir(ret);
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


static int32_t sys_execve(const char *pathname, char *const argv[], char *const envp[]) {
  return process_execve(pathname, argv, envp);
}

static int32_t sys_poll(struct pollfd *fds, uint32_t nfds, int timeout) {
  return do_poll(fds, nfds, timeout);
}

static int32_t sys_kill(pid_t pid, int sig) {
  return do_kill(pid, sig);
}

static int32_t sys_times(struct tms *buffer) {
  assert_not_implemented("sys_times");
}

static int32_t sys_stat(const char *path, struct kstat *stat) {
  return vfs_stat(path, stat);
}

static int32_t sys_getppid() {
  return get_current_process()->parent->pid;
}

static int32_t sys_unlink(const char *path) {
  return vfs_unlink(path, 0);
}

static int32_t sys_unlinkat(int fd, const char *path, int flag) {
  assert_not_implemented();
  /*
  if (flag && flag & ~AT_REMOVEDIR)
    return -EINVAL;

  char *interpreted_path;
  int ret = interpret_path_from_fd(fd, path, &interpreted_path);

  if (ret >= 0)
    ret = vfs_unlink(interpreted_path, flag);

  if (interpreted_path != path)
    kfree(interpreted_path);
  return ret;
  */
}

static int32_t sys_mknodat(int fd, const char *path, mode_t mode, dev_t dev) {
  assert_not_implemented();
  /*
  char *interpreted_path;
  int ret = interpret_path_from_fd(fd, path, &interpreted_path);

  if (ret >= 0)
    ret = vfs_mknod(interpreted_path, mode, dev);

  if (interpreted_path != path)
    kfree(interpreted_path);
  return ret;
  */
}

static int32_t sys_mknod(const char *path, mode_t mode, dev_t dev) {
  return vfs_mknod(path, mode, dev);
}

static int32_t sys_mkdir(const char *path, mode_t mode) {
  return vfs_mkdir(path, mode);
}

static int32_t sys_mkdirat(int fd, const char *path, mode_t mode) {
  assert_not_implemented();
  /*
  char *interpreted_path;
  int ret = interpret_path_from_fd(fd, path, &interpreted_path);

  if (ret >= 0)
    ret = vfs_mkdir(interpreted_path, mode);

  if (interpreted_path != path)
    kfree(interpreted_path);
  return ret;
  */
}

static int32_t sys_fcntl(int fd, int cmd, unsigned long arg) {
  return do_fcntl(fd, cmd, arg);
}

static int32_t sys_getgid() {
  return get_current_process()->gid;
}

static int32_t sys_setgid(gid_t gid) {
  struct process *current_process = get_current_process();
  current_process->gid = gid;
  return 0;
}

static int32_t sys_umask(mode_t cmask) {
  // TODO: MQ 2020-11-10 Implement umask

  //assert_not_implemented();
  return 0;
}

static int32_t sys_getuid() {
  //assert_not_implemented();
  return 0;
}

static int32_t sys_setuid(uid_t uid) {
  //assert_not_implemented();
  return 0;
}

static int32_t sys_getegid() {
  //assert_not_implemented();
  return 0;
}

static int32_t sys_geteuid() {
  //assert_not_implemented();
  return 0;
}

static int32_t sys_sigsuspend(const sig_t *set) {
  return 0;
  //return do_sigsuspend(set);
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
  [__NR_times] = sys_times,
  [__NR_stat] = sys_stat,
  [__NR_fstat] = sys_fstat,
  [__NR_dup] = sys_dup,
  [__NR_dup2] = sys_dup2,
  [__NR_waitpid] = sys_waitpid,
  [__NR_setpgid] = sys_setpgid,
  [__NR_dbg_ps] = sys_dbg_ps,
  [__NR_lseek] = sys_lseek,
  [__NR_poll] = sys_poll,
  [__NR_getgid] = sys_getgid,
  [__NR_setgid] = sys_setgid,
  [__NR_mknod] = sys_mknod,
  [__NR_mknodat] = sys_mknodat,
  [__NR_ioctl] = sys_ioctl,
  [__NR_unlink] = sys_unlink,
  [__NR_close] = sys_close,
  [__NR_execve] = sys_execve,
  [__NR_waitid] = sys_waitid,
  [__NR_chdir] = sys_chdir,
  [__NR_mkdir] = sys_mkdir,
  [__NR_mkdirat] = sys_mkdirat,
  [__NR_fchdir] = sys_fchdir,
  [__NR_unlinkat] = sys_unlinkat,
  [__NR_getppid] = sys_getppid,
  [__NR_setsid] = sys_setsid,
  [__NR_umask] = sys_umask,
  [__NR_sigsuspend] = sys_sigsuspend,
  [__NR_getsid] = sys_getsid,
  [__NR_getuid] = sys_getuid,
  [__NR_vfork] = sys_vfork,
  [__NR_setuid] = sys_setuid,
  [__NR_getegid] = sys_getegid,
  [__NR_geteuid] = sys_geteuid,
  [__NR_getpgrp] = sys_getpgrp,
  [__NR_getpid] = sys_getpid,
  [__NR_getdents] = sys_getdents,
  [__NR_getcwd] = sys_getcwd,
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