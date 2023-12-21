#include <stdint.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>

#include "../../../ports/newlib/myos/print.h"

void main(int argc, char** argv) {
  FILE* stream = fopen("test.txt", "ab+");
  
  if (fputs("Hello, world\n!", stream) == 0) {
    fflush(stream);
    int32_t size = 100;
    char* buf = calloc(size + 1, sizeof(char));
    memset(buf, 0, size);
    
    if (fseek(stream, 0, SEEK_SET) == 0) {
      print("\n_______________________________\n");
      while (fgets(buf, size, stream) != NULL) {
        print("%s", buf);
      }
      print("\n________________________________\n");
    }
    fclose(stream);
    free(buf);
  }
  
  /*
  int32_t id = -1;
  
  if (argc == 2) {
    id = (int32_t)*argv[1];

    print("File: %s\n", argv[0]);
    print("Process id: %d\n", id);
  }
  */
  
  //FILE* stream = fopen("test.txt", "a");
  /*
  int32_t size = 100;
  
  if (stream == NULL) {
    //print("Not stream");
    _exit(0);
  }

  fputs("Hello, world\n!", stream);

  fflush(stream); // write to file

  char* buf = calloc(size + 1, sizeof(char));
  lseek(stream->_file, 0, SEEK_SET);
  
  // Read the content and print it
  while(fgets(buf, size, stream)) {
    buf[size] = '\0';
    //print("%s", buf);
  }

  free(buf);
  fclose(stream);

  /*
  while (1) {
    sleep(3);
    print("\nHello from userprocess: %d", id);
  }
  */

  //_exit(0);
  while (1) {};
}
