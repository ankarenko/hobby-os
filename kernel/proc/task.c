#include "task.h"
#include "elf.h"

int32_t create_process(char* appname) {
  elf_load(appname);
}

void execute_process() {

}