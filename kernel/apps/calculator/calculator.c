#include <unistd.h>
#include <string.h>
#include "../../../libc/include/stdlib.h"

static char* str = "Hello, world! \n";

int _start() {
  //char a[20];

  

  

  while (1) {
    char* s = (char*)malloc(20);
    memcpy(s, str, 15);
    s[16] = '\0';

    sleep(100);
    print(s);
    
    free(s);
  }

  //terminate
  terminate();
  
  for (;;);
}
