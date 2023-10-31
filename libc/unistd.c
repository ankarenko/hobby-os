#include <unistd.h>
#include <stdarg.h>

_syscall3(read, int, char *, size_t);
int read(int fd, char *buf, size_t size) {
	SYSCALL_RETURN_ORIGINAL(syscall_read(fd, buf, size));
}

_syscall1(sbrk, unsigned int);
void* usbrk(unsigned int n) {
  SYSCALL_RETURN_POINTER(syscall_sbrk(n));
}

_syscall1(sleep, int32_t);
void sleep(unsigned int a) {
  SYSCALL_RETURN(syscall_sleep(a));
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
