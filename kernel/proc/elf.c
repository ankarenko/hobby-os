#include "kernel/util/string/string.h"

#include "kernel/fs/vfs.h"
#include "kernel/memory/vmm.h"
#include "kernel/memory/malloc.h"
#include "kernel/util/errno.h"
#include "kernel/util/debug.h"
#include "kernel/util/math.h"
#include "kernel/proc/task.h"

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

int32_t elf_load(
  char* app_path, 
  struct ELF32_Layout* layout
) {
  
  uint8_t* elf_file = NULL;
  
  int ret = 0;
  if ((ret = vfs_read(app_path, &elf_file)) < 0)
    return ret;

  struct process* parent = get_current_process();
  struct Elf32_Ehdr *elf_header = (struct Elf32_Ehdr *)elf_file;

  if (elf_verify(elf_header) != NO_ERROR || elf_header->e_phoff == 0)
    return -EINVAL;

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

  parent->image_base = USER_IMAGE_START; // for PIC (or malloc(image_size))
  assert(USER_HEAP_SIZE % PMM_FRAME_SIZE == 0);
  
  mm_struct_mos* mm = parent->mm_mos;
  mm->heap_start = parent->image_base;
  mm->brk = mm->heap_start;
  mm->heap_end = HEAP_END(mm->heap_start);
  mm->remaning = 0;

  virtual_addr* image = sbrk(
    parent->image_size, 
    mm
  );
  
  for (int i = 0; i < elf_header->e_phnum; ++i) {
    struct Elf32_Phdr *ph = elf_file + elf_header->e_phoff + elf_header->e_phentsize * i;
    
    if (ph->p_type != PT_LOAD || ph->p_filesz == 0)
			continue;

    virtual_addr vaddr = parent->image_base + ph->p_vaddr - base;

    // text segment
		if ((ph->p_flags & PF_X) != 0 && (ph->p_flags & PF_R) != 0) {
			mm->start_code = vaddr;
			mm->end_code = vaddr + ph->p_memsz;
		}
		// data segment
		else if ((ph->p_flags & PF_W) != 0 && (ph->p_flags & PF_R) != 0) {
			mm->start_data = vaddr;
			mm->end_data = vaddr + ph->p_memsz;
		}

    // the ELF specification states that you should zero the BSS area. In bochs, everything's zeroed by default, 
    // but on real computers and virtual machines it isnt.
    memset(vaddr, 0, ph->p_memsz);
    memcpy(vaddr, elf_file + ph->p_offset, ph->p_filesz);
  }

  if (!create_user_stack(
    parent->va_dir, 
    &layout->stack_bottom,
    mm->heap_end
  )) {
    return false;
  }

  layout->entry = parent->image_base + elf_header->e_entry - base;
  layout->heap_start = mm->heap_start;
  layout->heap_current = sbrk(0, mm);
  
  kfree(elf_file);
  return 0;
}

int32_t elf_unload(struct process* _proc) {
  // caught signals are reset
	// sigemptyset(&get_current_process()->thread->pending); ??
  struct process* proc = _proc == NULL? get_current_process() : _proc;
  virtual_addr start = proc->mm_mos->heap_start;
  virtual_addr end = sbrk(0, proc->mm_mos);

  assert(start % PMM_FRAME_SIZE == 0);

  for (virtual_addr virt = start; virt < end; virt+= PMM_FRAME_SIZE) {
    physical_addr phys = vmm_get_physical_address(virt, false);
    vmm_unmap_address(virt);
    pmm_free_frame(phys);
  }

  start = proc->mm_mos->heap_end;
  end = start + USER_STACK_SIZE;
  
  for (virtual_addr virt = start; virt < end; virt+= PMM_FRAME_SIZE) {
    physical_addr phys = vmm_get_physical_address(virt, false);
    vmm_unmap_address(virt);
    pmm_free_frame(phys);
  }

  memset(proc->mm_mos, 0, sizeof(mm_struct_mos));
  return 0;
}