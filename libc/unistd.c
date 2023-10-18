#include <unistd.h>

_syscall1(sbrk, unsigned int);
void* usbrk(unsigned int n) {
  SYSCALL_RETURN_POINTER(syscall_sbrk(n));
}

_syscall1(sleep, int32_t);
void sleep(unsigned int a) {
  SYSCALL_RETURN(syscall_sleep(a));
}

_syscall1(print, char*);
void print(char* str) {
  SYSCALL_RETURN(syscall_print(str));
}

_syscall0(terminate);
void terminate() {
  SYSCALL_RETURN(syscall_terminate());
}
