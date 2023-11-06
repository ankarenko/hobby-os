#include <unistd.h>
#include <stdarg.h>

_syscall3(read, int, char *, size_t);
ssize_t read(int fd, char *buf, size_t size) {
	SYSCALL_RETURN_ORIGINAL(syscall_read(fd, buf, size));
}

_syscall1(sleep, unsigned int);
unsigned int sleep(unsigned int seconds) {
  SYSCALL_RETURN(syscall_sleep(seconds));
}

_syscall1(sbrk, intptr_t);
void* usbrk(intptr_t increment) {
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

_syscall0(terminate);
void terminate() {
  SYSCALL_RETURN(syscall_terminate());
}

_syscall3(lseek, int, off_t, int);
int lseek(int fd, off_t offset, int whence) {
	SYSCALL_RETURN_ORIGINAL(syscall_lseek(fd, offset, whence));
}