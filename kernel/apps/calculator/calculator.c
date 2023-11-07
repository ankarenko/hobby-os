#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>

static char* str = "Hello, world! \n";
static int len = 16;

void _start(int argc, char** argv) {

  int32_t id = -1;
  
  if (argc == 2) {
    id = (int32_t)*argv[1];

    print("File: %s\n", argv[0]);
    print("Process id: %d\n", id);
  }

  while (1) {
    sleep(3);
    print("\nHello from userprocess: %d", id);
  }

  FILE* stream = fopen("test.txt", "a");
  int32_t size = 100;
  
  if (stream == NULL) {
    print("Not stream");
    _exit(0);
  }

  fputs("Hello, world\n!", stream);

  fflush(stream); // write to file

  char* buf = calloc(size + 1, sizeof(char));
  lseek(stream->fd, 0, SEEK_SET);
  
  // Read the content and print it
  while(fgets(buf, size, stream)) {
    buf[size] = '\0';
    print("%s", buf);
  }

  free(buf);
  fclose(stream);

  /*
  while (1) {
    sleep(3);
    print("\nHello from userprocess: %d", id);
  }
  */

  _exit(0);
  while (1) {};
  
}
