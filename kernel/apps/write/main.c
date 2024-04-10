#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "../../../ports/newlib/myos/msyscalls.h"


void main(int argc, char** argv) {
  dbg_log("START WRITE PROGRAM");

  if (argc == 1)
    return 0;

  while (1) {
    printf("\n[WRITE]: %s", argv[1]);
    sleep(1);
  }

}