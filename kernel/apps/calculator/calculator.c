#include <unistd.h>
#include <string.h>

static char* str = "Privet Bro! \n";

int _start() {
  char a[20];

  memcpy(&a, str, 15);

  while (1) {
    sys_sleep(200);
    sys_printf(a);
  }

  //terminate
  sys_terminate();
  
  for (;;);
}
