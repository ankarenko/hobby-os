#include <string.h>

#include "../fs/fsys.h"
#include "../memory/vmm.h"
#include "../memory/malloc.h"

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

#define USER_IMAGE_START 0x400000
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

// load elf program into the user userspace 
// which is provided in params
bool elf_load_image(
  char* app_path, 
  thread* th, 
  virtual_addr* entry
) {
  FILE file = vol_open_file(app_path);
  process* parent = th->parent;

  if (file.flags == FS_INVALID) {
    printf("\n*** File not found ***\n");
    return false;
  }

  const uint8_t* elf_file = kcalloc(file.file_length, sizeof(char));

  // TODO: read the whole file, not the best approach, but simple
  uint32_t shift = 0;
  while (!file.eof) {
    vol_read_file(&file, elf_file + shift, 512);
    shift += 512;
  }

  vol_close_file(&file);
  
  struct Elf32_Ehdr *elf_header = (struct Elf32_Ehdr *)elf_file;

  if (elf_verify(elf_header) != NO_ERROR || elf_header->e_phoff == 0) {
    return false;
  }

  // where elf wants us to put the image
  virtual_addr base = UINT32_MAX;

  // finding base address
  for (int i = 0; i < elf_header->e_phnum; ++i) {
    struct Elf32_Phdr *ph = elf_file + elf_header->e_phoff + elf_header->e_phentsize * i;
    base = min(base, ph->p_vaddr);
  }

  // figuting out, how much memory to allocate
  parent->image_size = 0;
  for (int i = 0; i < elf_header->e_phnum; ++i) {
    struct Elf32_Phdr *ph = elf_file + elf_header->e_phoff + elf_header->e_phentsize * i;
    uint32_t segment_end = ph->p_vaddr - (uint32_t)base + ph->p_memsz;
    parent->image_size = max(parent->image_size, segment_end);
  }

  // allocating segments and mapping to virtual addresses
  // *image_base = base; for absolute
  parent->image_base = USER_IMAGE_START; // for PIC (or malloc(image_size))
  parent->mm->heap_start = parent->image_base;
  parent->mm->brk = parent->mm->heap_start;
  parent->mm->heap_end = parent->mm->heap_start + USER_HEAP_SIZE;
  parent->mm->remaning = 0;
  
  virtual_addr* image = sbrk(
    parent->image_size, 
    &parent->mm->brk, 
    &parent->mm->remaning,
    I86_PDE_USER
  );

  // iterating trough segments and copying them to our address space
  for (int i = 0; i < elf_header->e_phnum; ++i) {
    struct Elf32_Phdr *ph = elf_file + elf_header->e_phoff + elf_header->e_phentsize * i;
    
    if (ph->p_type != PT_LOAD || ph->p_filesz == 0)
			continue;

    virtual_addr vaddr = parent->image_base + ph->p_vaddr - base;

    // the ELF specification states that you should zero the BSS area. In bochs, everything's zeroed by default, 
    // but on real computers and virtual machines it isnt.
    memset(vaddr, 0, ph->p_memsz);
    memcpy(vaddr, elf_file + ph->p_offset, ph->p_filesz);
  }

  /* create stack space for main thread at the end of the program */
  // dont forget that the stack grows up from down
  th->user_ss = USER_DATA;
  th->virt_ustack_bottom = parent->mm->heap_end;
  
  if (!create_user_stack(
    parent->va_dir, 
    &th->user_esp, 
    &th->phys_ustack_bottom, 
    th->virt_ustack_bottom
  )) {
    return false;
  }

  *entry = parent->image_base + elf_header->e_entry - base;

  kfree(elf_file);
  return true;
}

void elf_unload() {
}