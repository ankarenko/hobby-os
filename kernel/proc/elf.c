#include "elf.h"
#include "../fs/fsys.h"

struct Elf32_Layout *elf_load(const char *path) {
  FILE file = vol_open_file(path);

  if (file.flags == FS_INVALID) {
    printf("\n*** File not found ***\n");
    return;
  }

  const uint8_t *buf[512];

  vol_read_file(&file, buf, 512);
	struct Elf32_Ehdr *elf_header = (struct Elf32_Ehdr *)buf;
  
}

void elf_unload() {

}