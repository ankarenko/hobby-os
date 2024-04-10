#include <stdio.h>
#include <string.h>

void main(int argc, char** argv) {
  dbg_log("START PRINT PROGRAM");
  char str[100];

  while (1) {
    memset(str, 0, 100);
    printf("\n[PRINT] waiting for input\n");
    gets(str);
    printf("\n[]: %s", str);
  }
}