#include <stdio.h>

void main(int argc, char** argv) {
  char str[100];
  printf("\n[PRINT] waiting for input\n");
  gets(&str);
  printf("\n[PRINT] input: %s", str);
}