#include <fcntl.h>
#include <stdio.h>
#include "_syscall.h"

extern void exit(int code);
extern int main(int argc, char** argv, char **envp);

void _start(int argc, char** argv, char **envp) {
  environ = envp;

  setbuf(stdout, 0);
  setbuf(stdin, 0);
  setbuf(stderr, 0);
  
  int ex = main(argc, argv, envp);
  exit(ex);
}