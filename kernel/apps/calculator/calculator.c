#include <unistd.h>
#include <string.h>
#include "../../../libc/include/stdlib.h"

static char* str = "Privet Br!! \n";

int _start() {
  //char a[20];

  char* a = (char*)malloc(20);

  memcpy(a, str, 15);
  a[5] = '\0';

  while (1) {
    sleep(50);
    print(a);
  }

  //terminate
  terminate();
  
  for (;;);
}
