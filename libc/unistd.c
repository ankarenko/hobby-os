#include <unistd.h>
#include <stdarg.h>
#include <time.h>

_syscall3(read, int, char *, size_t);
ssize_t read(int fd, char *buf, size_t size) {
	SYSCALL_RETURN_ORIGINAL(syscall_read(fd, buf, size));
}

_syscall1(sbrk, intptr_t);
void* sbrk(intptr_t increment) {
  SYSCALL_RETURN_POINTER(syscall_sbrk(increment));
}

_syscall3(write, int, const char *, size_t);
int write(int fd, const char *buf, size_t size) {
	SYSCALL_RETURN_ORIGINAL(syscall_write(fd, buf, size));
}

_syscall1(close, int);
int close(int fd) {
	SYSCALL_RETURN(syscall_close(fd));
}

_syscall2(print, char*, va_list);
void print(char* format, ...) {
  va_list list;
  va_start(list, format);
  SYSCALL_RETURN(syscall_print(format, list));
  va_end(list);
}

_syscall3(lseek, int, off_t, int);
int lseek(int fd, off_t offset, int whence) {
	SYSCALL_RETURN_ORIGINAL(syscall_lseek(fd, offset, whence));
}

_syscall1(exit, int);
void _exit(int32_t status) {
  SYSCALL_RETURN(syscall_exit(status));
}

_syscall0(fork);
int fork() {
	SYSCALL_RETURN_ORIGINAL(syscall_fork());
}

int usleep(useconds_t usec) {
	struct timespec req = {.tv_sec = usec / 1000, .tv_nsec = usec * 1000};
	return nanosleep(&req, NULL);
}

int sleep(unsigned int sec) {
	return usleep(sec * 1000);
}

_syscall2(nanosleep, const struct timespec *, struct timespec *);
int nanosleep(const struct timespec *req, struct timespec *rem) {
	SYSCALL_RETURN(syscall_nanosleep(req, rem));
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

_syscall3(execve, const char *, char *const *, char *const *);
int execve(const char *pathname, char *const argv[], char *const envp[]) {
	_clear_on_exit();
	SYSCALL_RETURN_ORIGINAL(syscall_execve(pathname, argv, envp));
}

_syscall0(getpid);
int getpid() {
	SYSCALL_RETURN_ORIGINAL(syscall_getpid());
}