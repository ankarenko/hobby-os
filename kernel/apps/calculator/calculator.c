// this does not work!
// static char* str = "Priver!";

int main()
{
  int a = 512;
  char* str = "Hello from user process!";
  
  // print message
  __asm__ __volatile__(
      "mov %0, %%ebx;			  \n"
      "mov %1, %%eax;	      \n"
      "int $0x80;           \n"
      : "=m"(str)
      : "a"(a)
  );

  a = 1;
  //terminate
  __asm__ __volatile__(
      "mov %0, %%eax;			  \n"
      "int $0x80;           \n"
      : "=a"(a)
  );

  for (;;);
}
