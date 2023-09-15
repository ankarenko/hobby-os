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

struct Elf32_Layout elf_load(const char *path) {
  FILE file = vol_open_file(path);
  struct Elf32_Layout layout;
  //addressSpace = vmmngr_createAddressSpace ();
  struct pdirectory* address_space = vmm_get_directory();

  layout.entry = 0;
  layout.stack = 0;

  if (file.flags == FS_INVALID) {
    printf("\n*** File not found ***\n");
    return;
  }

  const uint8_t* elf_file = kmalloc(file.file_length);

  // TODO: read the whole file, not the best approach, but simple
  uint32_t shift = 0;
  while (!file.eof) {
    vol_read_file(&file, elf_file + shift, 512);
    shift += 512;
  }

  vol_close_file(&file);
  
  struct Elf32_Ehdr *elf_header = (struct Elf32_Ehdr *)elf_file;

  if (elf_verify(elf_header) != NO_ERROR || elf_header->e_phoff == 0) {
    // log("ELF: %s is not correct format", path);
    return layout;
  }

  uint8_t* base = UINT32_MAX;

  // finding base address
  for (int i = 0; i < elf_header->e_phnum; ++i) {
    struct Elf32_Phdr *ph = elf_file + elf_header->e_phoff + elf_header->e_phentsize * i;
    
    base = min(base, ph->p_vaddr);
  }

  uint32_t image_size = 0;
  // figuting out, how much memory to allocate
  for (int i = 0; i < elf_header->e_phnum; ++i) {
    struct Elf32_Phdr *ph = elf_file + elf_header->e_phoff + elf_header->e_phentsize * i;
    uint32_t segment_end = ph->p_vaddr - (uint32_t)base + ph->p_memsz;
    image_size = max(image_size, segment_end);
  }

 

  // allocating segments and mapping to virtual addresses
  uint8_t* vimage = 0x40000000;
  uint32_t frames = div_ceil(image_size, PMM_FRAME_SIZE);
  uint8_t* phys = pmm_alloc_frames(frames);
 
  for (int i = 0; i < frames; ++i) {
    vmm_map_address(
      address_space, 
      vimage + PMM_FRAME_SIZE * i, phys + PMM_FRAME_SIZE * i, 
      I86_PTE_PRESENT | I86_PTE_WRITABLE | I86_PTE_USER
    );
  }

  

  // allocate stack
  uint8_t* pstack = pmm_alloc_frame();
  uint8_t* vstack = ALIGN_UP((uint32_t)vimage + image_size, PMM_FRAME_SIZE);
  
  vmm_map_address(
    address_space, 
    vstack, pstack, 
    I86_PTE_PRESENT | I86_PTE_WRITABLE | I86_PTE_USER
  );

  // iterating trough segments
  for (int i = 0; i < elf_header->e_phnum; ++i) {
    struct Elf32_Phdr *ph = elf_file + elf_header->e_phoff + elf_header->e_phentsize * i;
    
    if (ph->p_type != PT_LOAD || ph->p_filesz == 0)
			continue;

    // data segment
		// if ((ph->p_flags & PF_W) != 0 && (ph->p_flags & PF_R) != 0) {
    // text segment
		// if ((ph->p_flags & PF_X) != 0 && (ph->p_flags & PF_R) != 0) {
    uint8_t* vaddr = vimage + ph->p_vaddr - base;

    memset(vaddr, 0, ph->p_memsz);
    memcpy(vaddr, elf_file + ph->p_offset, ph->p_filesz);
  }
  
  layout.entry = vimage + elf_header->e_entry - base;
  layout.stack = vstack;
  layout.image_size = image_size;
  layout.base = base;
  layout.image_start = vimage;
  layout.address_space = address_space;

  kfree(elf_file);
  return layout;
}

void elf_unload() {
}