// this does not work!

//static char* str = "Privet! \n";
/*
void sleep(unsigned int a) {
  __asm__ __volatile__(
    "mov $200, %%ebx;			  \n"
    "mov $2, %%eax;			  \n"
    "int $0x80;           \n"
    : "=a"(a)
  );
}
*/

void printf(char* str) {
  __asm__ __volatile__(
    "mov %0, %%ebx;			  \n"
    "mov $512, %%eax;	      \n"
    "int $0x80;           \n"
    : "=m"(str)
  );
}
/*
void terminate() {
  __asm__ __volatile__(
      "mov $1, %%eax;			  \n"
      "int $0x80;           \n"
      :::
  );
}

int _start() {
  //char* str = "Hello from user process!\n";

  // print message
  while (1) {
    sleep(200);
    printf(str);
  }

  //terminate
  terminate();

  for (;;);
}
*/

static char* str = "Privet! \n";

int _start() {
  printf(str);
}