/* note these headers are all provided by newlib - you don't need to provide them */
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "_syscall.h"

char **environ; /* pointer to array of char * strings that define the current environment variables */

_syscall3(read, int, char *, size_t);
uint32_t read(int fd, char *buf, size_t size) {
  SYSCALL_RETURN_ORIGINAL(syscall_read(fd, buf, size));
}

_syscall2(fstat, int32_t, struct stat *);
int fstat(int fildes, struct stat *buf) {
  SYSCALL_RETURN_ORIGINAL(syscall_fstat(fildes, buf));
}

_syscall1(dup, int);
int dup(int oldfd) {
  SYSCALL_RETURN_ORIGINAL(syscall_dup(oldfd));
}

_syscall2(dup2, int, int);
int dup2(int oldfd, int newfd) {
  SYSCALL_RETURN_ORIGINAL(syscall_dup2(oldfd, newfd));
}

int wait(int *wstatus) {
  return waitpid(-1, wstatus, 0);
}

_syscall3(waitpid, pid_t, int *, int);
pid_t waitpid(pid_t pid, int *wstatus, int options) {
  SYSCALL_RETURN_ORIGINAL(syscall_waitpid(pid, wstatus, options));
}

// TODO: idtype_t is not defined, so I replaced to int
_syscall4(waitid, int, id_t, struct infop *, int);
int waitid(int idtype, id_t id, struct infop *infop, int options) {
  SYSCALL_RETURN_ORIGINAL(syscall_waitid(idtype, id, infop, options));
}

_syscall1(chdir, const char *);
int chdir(const char *path) {
	SYSCALL_RETURN_ORIGINAL(syscall_chdir(path));
}

_syscall1(fchdir, int);
int fchdir(int fildes) {
	SYSCALL_RETURN_ORIGINAL(syscall_fchdir(fildes));
}

_syscall1(exit, int);
void _exit(int32_t status) {
  SYSCALL_RETURN(syscall_exit(status));
}

_syscall1(close, int);
int close(int fd) {
  SYSCALL_RETURN_ORIGINAL(syscall_close(fd));
}

_syscall1(unlink, const char *);
int unlink(const char *path) {
	SYSCALL_RETURN_ORIGINAL(syscall_unlink(path));
}

_syscall3(unlinkat, int, const char *, int);
int unlinkat(int fd, const char *path, int flag) {
	SYSCALL_RETURN_ORIGINAL(syscall_unlinkat(fd, path, flag));
}

void _clear_on_exit() {
  /*
        struct cleanup_handler *iter, *next;
        list_for_each_entry_safe(iter, next, &lhandler, sibling)
        {
                list_del(&iter->sibling);
                free(iter);
        }
  */
}

// int execve(char *name, char **argv, char **env);
_syscall3(execve, const char *, char *const *, char *const *);
int execve(const char *pathname, char *const argv[], char *const envp[]) {
  SYSCALL_RETURN_ORIGINAL(syscall_execve(pathname, argv, envp));
}

_syscall3(getdents, unsigned int, struct dirent *, unsigned int);
int getdents(unsigned int fd, struct dirent *dirent, unsigned int count) {
	SYSCALL_RETURN_ORIGINAL(syscall_getdents(fd, dirent, count));
}


_syscall2(getcwd, char *, size_t);
char *getcwd(char *buf, size_t size) {
  /*
	if (!size)
		size = MAXPATHLEN;
	if (!buf)
		buf = calloc(size, sizeof(char));
  */
	SYSCALL_RETURN_POINTER(syscall_getcwd(buf, size));
}

_syscall3(ioctl, int, unsigned int, unsigned long);
int ioctl(int fd, unsigned int cmd, ...) {
	va_list ap;
	va_start(ap, cmd);
	unsigned long arg = va_arg(ap, unsigned long);
	va_end(ap);
	SYSCALL_RETURN_ORIGINAL(syscall_ioctl(fd, cmd, arg));
}

_syscall0(fork);
int fork() {
  SYSCALL_RETURN_ORIGINAL(syscall_fork());
}

int tcsetpgrp(int fd, pid_t pid) {
	return ioctl(fd, TIOCSPGRP, (unsigned long)&pid);
}

pid_t tcgetpgrp(int fd) {
	pid_t pgrp;

	if (ioctl(fd, TIOCGPGRP, &pgrp) < 0)
		return -1;

	return pgrp;
}


_syscall0(getpid);
int getpid() {
  SYSCALL_RETURN_ORIGINAL(syscall_getpid());
}

int isatty(int file) {
  return 1;
}

int kill(int pid, int sig) {
}

int link(char *old, char *new) {
}

// int lseek(int file, int ptr, int dir); original
_syscall3(lseek, int, off_t, int);
int lseek(int fd, off_t offset, int whence) {
  SYSCALL_RETURN_ORIGINAL(syscall_lseek(fd, offset, whence));
}

_syscall3(open, const char *, int32_t, mode_t);
int open(const char *name, int flags, ...) {
  mode_t mode = 0;
  
  if (flags & O_CREAT) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }

  SYSCALL_RETURN_ORIGINAL(syscall_open(name, flags, mode));
}

_syscall0(getppid);
int getppid() {
  SYSCALL_RETURN_ORIGINAL(syscall_getppid());
}

_syscall2(setpgid, pid_t, pid_t);
int setpgid(pid_t pid, pid_t pgid) {
  SYSCALL_RETURN_ORIGINAL(syscall_setpgid(pid, pgid));
}

_syscall0(getsid);
int getsid() {
  SYSCALL_RETURN_ORIGINAL(syscall_getsid());
}

_syscall0(setsid);
int setsid() {
  SYSCALL_RETURN_ORIGINAL(syscall_setsid());
}

_syscall1(sbrk, intptr_t);
caddr_t sbrk(intptr_t increment) {
  SYSCALL_RETURN_POINTER(syscall_sbrk(increment));
}

_syscall2(stat, const char *, struct stat *);
int stat(const char *path, struct stat *buf) {
	SYSCALL_RETURN_ORIGINAL(syscall_stat(path, buf));
}

int lstat(const char *path, struct stat *buf) {
	return stat(path, buf);
}

clock_t times(struct tms *buf) {
}

_syscall3(write, int, const char *, size_t);
int write(int fd, const char *buf, size_t size) {
  SYSCALL_RETURN_ORIGINAL(syscall_write(fd, buf, size));
}

_syscall3(sigaction, int, const struct sigaction *, struct sigaction *);
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
  SYSCALL_RETURN_ORIGINAL(syscall_sigaction(signum, act, oldact));
}

_syscall3(sigprocmask, int, const sigset_t *, sigset_t *);
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
  SYSCALL_RETURN_ORIGINAL(syscall_sigprocmask(how, set, oldset));
}

_syscall2(nanosleep, const struct timespec *, struct timespec *);
int nanosleep(const struct timespec *req, struct timespec *rem) {
  SYSCALL_RETURN_ORIGINAL(syscall_nanosleep(req, rem));
}

_syscall3(mkdirat, int, const char *, mode_t);
int mkdirat(int fd, const char *path, mode_t mode) {
	SYSCALL_RETURN_ORIGINAL(syscall_mkdirat(fd, path, mode));
}

_syscall0(getgid);
int getgid() {
	SYSCALL_RETURN_ORIGINAL(syscall_getgid());
}

_syscall0(getuid);
int getuid() {
	SYSCALL_RETURN_ORIGINAL(syscall_getuid());
}

_syscall1(setuid, uid_t);
int setuid(uid_t uid) {
	SYSCALL_RETURN(syscall_setuid(uid));
}

_syscall0(getegid);
int getegid() {
	SYSCALL_RETURN_ORIGINAL(syscall_getegid());
}

_syscall0(geteuid);
int geteuid() {
	SYSCALL_RETURN_ORIGINAL(syscall_geteuid());
}

_syscall1(pipe, int *);
int pipe(int *fildes) {
	SYSCALL_RETURN(syscall_pipe(fildes));
}

_syscall0(getpgrp);
int getpgrp() {
	SYSCALL_RETURN_ORIGINAL(syscall_getpgrp());
}

_syscall1(setgid, gid_t);
int setgid(gid_t gid) {
	SYSCALL_RETURN_ORIGINAL(syscall_setgid(gid));
}

_syscall1(umask, mode_t);
mode_t umask(mode_t cmask) {
	SYSCALL_RETURN_ORIGINAL(syscall_umask(cmask));
}

_syscall3(fcntl, int, int, unsigned long);
int fcntl(int fd, int cmd, ...) {
	va_list ap;
	va_start(ap, cmd);
	unsigned long arg = va_arg(ap, unsigned long);
	va_end(ap);

	SYSCALL_RETURN_ORIGINAL(syscall_fcntl(fd, cmd, arg));
}

_syscall2(mkdir, const char *, mode_t);
int mkdir(const char *path, mode_t mode) {
	SYSCALL_RETURN_ORIGINAL(syscall_mkdir(path, mode));
}

_syscall3(mknod, const char *, mode_t, dev_t);
int mknod(const char *path, mode_t mode, dev_t dev) {
	SYSCALL_RETURN_ORIGINAL(syscall_mknod(path, mode, dev));
}

_syscall4(mknodat, int, const char *, mode_t, dev_t);
int mknodat(int fd, const char *path, mode_t mode, dev_t dev) {
	SYSCALL_RETURN_ORIGINAL(syscall_mknodat(fd, path, mode, dev));
}

int usleep(useconds_t usec) {
  struct timespec req = {.tv_sec = usec / 1000, .tv_nsec = usec * 1000};
  return nanosleep(&req, NULL);
}

int sleep(unsigned int sec) {
  return usleep(sec * 1000);
}
