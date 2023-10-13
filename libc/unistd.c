#include <unistd.h>

void sys_sleep(unsigned int a) {
  __asm__ __volatile__(
    "mov $200, %%ebx;			  \n"
    "mov $2, %%eax;			  \n"
    "int $0x80;           \n"
    : "=a"(a)
  );
}

void sys_printf(char* str) {
  __asm__ __volatile__(
    "mov %0, %%ebx;			  \n"
    "mov $512, %%eax;	      \n"
    "int $0x80;           \n"
    : "=m"(str)
  );
}

void sys_terminate() {
  __asm__ __volatile__(
      "mov $1, %%eax;			  \n"
      "int $0x80;           \n"
      :::
  );
}
