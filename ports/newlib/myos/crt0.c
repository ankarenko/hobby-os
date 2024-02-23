#include <fcntl.h>
#include <stdio.h>

extern void exit(int code);
extern int main(int argc, char** argv);

void _start(int argc, char** argv) {
  setbuf(stdout, 0);
  setbuf(stdin, 0);
  setbuf(stderr, 0);
  
  int ex = main(argc, argv);
  exit(ex);
}