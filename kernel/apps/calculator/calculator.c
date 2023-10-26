#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char* str = "Hello, world! \n\0";
static int len = 16;

void _start(int argc, char** argv) {
  int32_t id = -1;
  
  if (argc == 2) {
    id = (int32_t)*argv[1];

    print("File: %s\n", argv[0]);
    print("Process id: %d\n", id);
  }
  

  while (1) {
    //char* s = (char*)malloc(len);
    //memcpy(s, str, len);
    
    //print("\n0x%x", s);
    sleep(200);
    print("Hello from userprocess: %d\n", id);
    
    //free(s);
  }

  terminate();
}
