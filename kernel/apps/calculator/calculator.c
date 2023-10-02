// this does not work!
// static char* str = "Priver!";
/*
void sleep(unsigned int a) {
  __asm__ __volatile__(
    "mov $200, %%ebx;			  \n"
    "mov $2, %%eax;			  \n"
    "int $0x80;           \n"
    : "=a"(a)
  );
}

void printf(char* str) {
  __asm__ __volatile__(
    "mov %0, %%ebx;			  \n"
    "mov $512, %%eax;	      \n"
    "int $0x80;           \n"
    : "=m"(str)
  );
}

void terminate() {
  __asm__ __volatile__(
      "mov $1, %%eax;			  \n"
      "int $0x80;           \n"
      :::
  );
}
*/

int main()
{
  int a = 0;
  char* str = "Hello from user process!\n";

  // print message
  //while (1) {
    // sleep
    a = 200;
    __asm__ __volatile__(
      "mov $200, %%ebx;			  \n"
      "mov $2, %%eax;			  \n"
      "int $0x80;           \n"
      :::
    );

    a = 512;
    __asm__ __volatile__(
        "mov %0, %%ebx;			  \n"
        "mov %1, %%eax;	      \n"
        "int $0x80;           \n"
        : "=m"(str)
        : "a"(a)
    );
  //}

  //terminate
  a = 1;
  __asm__ __volatile__(
      "mov %0, %%eax;			  \n"
      "int $0x80;           \n"
      : "=a"(a)
  );

  for (;;);
}
