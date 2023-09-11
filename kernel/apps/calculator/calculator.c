// this does not work!
// static char* str = "Priver!";

int main()
{
  int a = 512;
  char* str = "Priver!";

  // TODO: not sure if it is a right way to provide params, but it works
  __asm__ __volatile__(
      "mov %0, %%ebx;			  \n"
      "mov %1, %%eax;	      \n"
      "int $0x80;           \n"
      : "=m"(str)
      : "a"(a)
  );

  return 0;
}
