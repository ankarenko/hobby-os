#include "../fs/fsys.h"
#include "elf.h"

/*
* 	+---------------+ 	/
* 	| stack pointer | grows toward lower addresses
* 	+---------------+ ||
* 	|...............| \/
* 	|...............|
* 	|...............|
* 	|...............| /\
* 	+---------------+ ||
* 	|  brk (heap)   | grows toward higher addresses
* 	+---------------+
* 	| .bss section  |
* 	+---------------+
* 	| .data section |
* 	+---------------+
* 	| .text section |
* 	+---------------+
*/

#define NO_ERROR 0
#define ERR_NOT_ELF_FILE 1
#define ERR_NOT_SUPPORTED_PLATFORM 2
#define ERR_NOT_SUPPORTED_ENCODING 3
#define ERR_WRONG_VERSION 4
#define ERR_NOT_SUPPORTED_TYPE 5

static int elf_verify(struct Elf32_Ehdr *elf_header) {
  if (!(elf_header->e_ident[EI_MAG0] == ELFMAG0 &&
        elf_header->e_ident[EI_MAG1] == ELFMAG1 &&
        elf_header->e_ident[EI_MAG2] == ELFMAG2 &&
        elf_header->e_ident[EI_MAG3] == ELFMAG3))
    return -ERR_NOT_ELF_FILE;

  if (elf_header->e_ident[EI_CLASS] != ELFCLASS32)
    return -ERR_NOT_SUPPORTED_PLATFORM;

  if (elf_header->e_ident[EI_DATA] != ELFDATA2LSB)
    return -ERR_NOT_SUPPORTED_ENCODING;

  if (elf_header->e_ident[EI_VERSION] != EV_CURRENT)
    return -ERR_WRONG_VERSION;

  if (elf_header->e_machine != EM_386)
    return -ERR_NOT_SUPPORTED_PLATFORM;

  if (elf_header->e_type != ET_EXEC)
    return -ERR_NOT_SUPPORTED_TYPE;

  return NO_ERROR;
}

struct Elf32_Layout *elf_load(const char *path) {
  FILE file = vol_open_file(path);

  if (file.flags == FS_INVALID) {
    printf("\n*** File not found ***\n");
    return;
  }

  const uint8_t *buf[512];

  vol_read_file(&file, buf, 512);
  struct Elf32_Ehdr *elf_header = (struct Elf32_Ehdr *)buf;

  if (elf_verify(elf_header) != NO_ERROR || elf_header->e_phoff == 0) {
    // log("ELF: %s is not correct format", path);
    return 0;
  }

  struct Elf32_Layout *layout = kcalloc(1, sizeof(struct Elf32_Layout));
	layout->entry = elf_header->e_entry;

  for (struct Elf32_Phdr *ph = (struct Elf32_Phdr *)(buf + elf_header->e_phoff);
		 ph && (char *)ph < (buf + elf_header->e_phoff + elf_header->e_phentsize * elf_header->e_phnum);
		 ++ph)
	{
    if (ph->p_type != PT_LOAD)
			continue;

    // text segment
		if ((ph->p_flags & PF_X) != 0 && (ph->p_flags & PF_R) != 0) {
      /*
			do_mmap(ph->p_vaddr, ph->p_memsz, 0, 0, -1, 0);
			mm->start_code = ph->p_vaddr;
			mm->end_code = ph->p_vaddr + ph->p_memsz;
      */
		}
		// data segment
		else if ((ph->p_flags & PF_W) != 0 && (ph->p_flags & PF_R) != 0) {
      /*
			do_mmap(ph->p_vaddr, ph->p_memsz, 0, 0, -1, 0);
			mm->start_data = ph->p_vaddr;
			mm->end_data = ph->p_memsz;
      */
		}
		// eh frame
		else {

    }
			//do_mmap(ph->p_vaddr, ph->p_memsz, 0, 0, -1, 0);

		// NOTE: MQ 2019-11-26 According to elf's spec, p_memsz may be larger than p_filesz due to bss section
		memset((uint8_t *)ph->p_vaddr, 0, ph->p_memsz);
		memcpy((uint8_t *)ph->p_vaddr, buf + ph->p_offset, ph->p_filesz);
  }

  return layout;
}

void elf_unload() {
}