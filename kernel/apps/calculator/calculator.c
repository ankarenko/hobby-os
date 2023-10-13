#include <unistd.h>
#include <string.h>
#include <stdlib.h>

static char* str = "Hello, world! \n\0";
static int len = 16;

void _start() {
  while (1) {
    char* s = (char*)malloc(len);
    memcpy(s, str, len);
    
    sleep(100);
    print(s);
  
    free(s);
  }

  terminate();
}
