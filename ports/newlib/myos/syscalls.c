/* note these headers are all provided by newlib - you don't need to provide them */
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/times.h>
#include <stdarg.h>
#include <sys/signal.h>
#include <stddef.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <stdio.h>

#include "_syscall.h"
#include "print.h"

char **environ; /* pointer to array of char * strings that define the current environment variables */

_syscall2(print, char*, va_list);
void kernel_print(char* format, ...) {
  va_list list;
  va_start(list, format);
  SYSCALL_RETURN(syscall_print(format, list));
  va_end(list);
}

_syscall3(read, int, char *, size_t);
uint32_t read(int fd, char *buf, size_t size) {
  //kernel_print("\nread");
	SYSCALL_RETURN_ORIGINAL(syscall_read(fd, buf, size));
}

_syscall2(fstat, int32_t, struct stat*);
int fstat(int fildes, struct stat* buf) {
  //kernel_print("\nfstat");
	SYSCALL_RETURN(syscall_fstat(fildes, buf));
}

_syscall1(exit, int);
void _exit(int32_t status) {
  //kernel_print("\nexit");
  SYSCALL_RETURN(syscall_exit(status));
}

_syscall1(close, int);
int close(int fd) {
  //kernel_print("\nclose");
	SYSCALL_RETURN(syscall_close(fd));
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

//int execve(char *name, char **argv, char **env);
_syscall3(execve, const char *, char *const *, char *const *);
int execve(const char *pathname, char *const argv[], char *const envp[]) {
  //kernel_print("\nexecve");
	_clear_on_exit();
	SYSCALL_RETURN_ORIGINAL(syscall_execve(pathname, argv, envp));
}

_syscall0(fork);
int fork() {
  //kernel_print("\nfork");
	SYSCALL_RETURN_ORIGINAL(syscall_fork());
}

_syscall0(getpid);
int getpid() {
  //kernel_print("\ngetpid");
	SYSCALL_RETURN_ORIGINAL(syscall_getpid());
}

int isatty(int file) {
  kernel_print("\nisatty");
  return 1;
}

int kill(int pid, int sig) {
  kernel_print("\nkill");
}

int link(char *old, char *new) {
  kernel_print("\nlink");
}

//int lseek(int file, int ptr, int dir); original
_syscall3(lseek, int, off_t, int);
int lseek(int fd, off_t offset, int whence) {
  //kernel_print("\nlseek offset: %d type: %d", offset, whence);
	SYSCALL_RETURN_ORIGINAL(syscall_lseek(fd, offset, whence));
}

_syscall3(open, const char*, int32_t, mode_t);
int open(const char *name, int flags, ...) {
  
  mode_t mode = 0;
  //kernel_print("\nmode: %d", mode);
  
  if (flags & O_CREAT) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }

  //kernel_print("\nmode: %d", mode);

  SYSCALL_RETURN_ORIGINAL(syscall_open(name, flags, mode));
}

_syscall1(sbrk, intptr_t);
caddr_t sbrk(intptr_t increment) {
  //kernel_print("\nsbrk");
  SYSCALL_RETURN_POINTER(syscall_sbrk(increment));
}

int stat(const char *file, struct stat *st) {
  kernel_print("Invoke stat");
}

clock_t times(struct tms *buf) {
  kernel_print("\ntimes");
}

int unlink(char *name) {
  kernel_print("\nunlink");
}

int wait(int *status) {
  kernel_print("\nwait");
}

_syscall3(write, int, const char *, size_t);
int write(int fd, const char *buf, size_t size) {
  //kernel_print("\nwrite");
	SYSCALL_RETURN_ORIGINAL(syscall_write(fd, buf, size));
}

_syscall3(sigaction, int, const struct sigaction *, struct sigaction *);
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
	SYSCALL_RETURN(syscall_sigaction(signum, act, oldact));
}

_syscall3(sigprocmask, int, const sigset_t *, sigset_t *);
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
	SYSCALL_RETURN(syscall_sigprocmask(how, set, oldset));
}

_syscall2(nanosleep, const struct timespec *, struct timespec *);
int nanosleep(const struct timespec *req, struct timespec *rem) {
  //kernel_print("\nananosleep");
	SYSCALL_RETURN(syscall_nanosleep(req, rem));
}

int usleep(useconds_t usec) {
	struct timespec req = {.tv_sec = usec / 1000, .tv_nsec = usec * 1000};
	return nanosleep(&req, NULL);
}

int sleep(unsigned int sec) {
	return usleep(sec * 1000);
}
