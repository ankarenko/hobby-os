#ifndef VMM_H
#define VMM_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "kernel/memory/vmm_pde.h"
#include "kernel/memory/vmm_pte.h"
#include "kernel/memory/vmm.h"

struct _mm_struct_mos;

/*
  Memory layout of our address space
  +-------------------------+ 0xFFFFFFFF
  | Page table mapping      |
  |_________________________| 0xFFC00000
  |                         |
  |-------------------------| 0xF0000000
  |                         |
  | Device drivers          |
  |                         |
  |-------------------------| 0xE8000000
  | VMALLOC  ???????        |
  |-------------------------| 0xE0000000
  |                         |
  |                         |
  | Kernel heap             |
  |_________________________| 0xC8000000                    
  |                         |
  | Not used                | 
  |_________________________| KERNEL_END + BITMAP_SIZE_MAX (128 Kbyte)                       
  |                         |
  | Bitmap                  |
  |_________________________| KERNEL_END
  |                         | 
  | Kernel itself           |
  |_________________________| 0xC0000000
  |                         |
  | Page for page faults    |
  |_________________________| 0xBFFFF000
  |                         |
  |                         |
  |                         |
  |                         |
  |_________________________| 0x40000000
  |                         |
  | User heap               |
  |_________________________| 0x
  |                         |
  | Elf                     |
  |_________________________| 0x200000
  |                         |
  | todo: I get error here  | 
  |_________________________| 0x100000
  |                         |
  | Reversed for devices    |
  +_________________________+ 0x00000000
*/

//! virtual address
typedef uint32_t virtual_addr;

//! i86 architecture defines 1024 entries per table--do not change
#define PAGES_PER_TABLE 1024
#define TABLES_PER_DIR 1024

#define PAGE_DIRECTORY_INDEX(x) (((x) >> 22) & 0x3ff)
#define PAGE_TABLE_INDEX(x) (((x) >> 12) & 0x3ff)
#define PAGE_PHYS_ADDR(x) (*x & ~0xfff)

#define PAGE_DIRECTORY_BASE 0xFFFFF000
#define PAGE_TABLE_BASE 0xFFC00000
#define PAGE_TABLE_VIRT_ADDRESS(virt) (PAGE_TABLE_BASE + (PAGE_DIRECTORY_INDEX(virt) * PMM_FRAME_SIZE))


//! page table represents 4mb address space
#define PTABLE_ADDR_SPACE_SIZE 0x400000

//! directory table represents 4gb address space
#define DTABLE_ADDR_SPACE_SIZE 0x100000000

#define KERNEL_STACK_SIZE (0x40000)
#define USER_STACK_SIZE 0x1000
#define USER_HEAP_SIZE 0xA00000 // 10mb TODO: increase it

//! page sizes are 4k
#define PAGE_SIZE 4096


#define MMAP_PROT_READ	0x1		/* page can be read */
#define MMAP_PROT_WRITE	0x2		/* page can be written */

#define MMAP_FIXED	0x10		/* Interpret addr exactly */
#define MMAP_ANONYMOUS	0x20		/* don't use a file */

#define MMAP_DONT_ALLOCATE 0x30


//! page table
struct ptable {
  pt_entry m_entries[PAGES_PER_TABLE];
};

//! page directory
struct pdirectory {
  pd_entry m_entries[TABLES_PER_DIR];
};

void vmm_init();

bool vmm_is_kernel_directory(struct pdirectory *dir);

//! allocates a page in physical memory
bool vmm_alloc_page(pt_entry*);

struct pdirectory *vmm_create_address_space();

//! frees a page in physical memory
void vmm_free_page(pt_entry* e);

//! switch to a new page directory
bool vmm_switch_pdirectory(struct pdirectory* dir);
bool vmm_switch_back();

//! get current page directory
struct pdirectory* vmm_get_directory();

//! flushes a cached translation lookaside buffer (TLB) entry
void vmm_flush_tlb_entry(virtual_addr addr);

//! clears a page table
void vmm_ptable_clear(struct ptable* p);

//! get page entry from page table
pt_entry* vmm_ptable_lookup_entry(struct ptable* p, virtual_addr addr);

//! clears a page directory table
void vmm_pdirectory_clear(struct pdirectory* dir);

void vmm_clone_kernel_space(struct pdirectory* dir);

//! get directory entry from directory table
pd_entry* vmm_pdirectory_lookup_entry(struct pdirectory* p, virtual_addr addr);

physical_addr vmm_get_physical_address(virtual_addr vaddr, bool is_page);

// maps address to a currently set page_directory
void vmm_map_address(uint32_t virt, uint32_t phys, uint32_t flags);

//virtual_addr vmm_alloc_size(virtual_addr from, uint32_t size, uint32_t flags);
int32_t vmm_unmap_address(virtual_addr virt);
void vmm_unmap_range(virtual_addr vm_start, virtual_addr vm_end);
struct pdirectory *vmm_fork(struct pdirectory* dir);

/* sbrk.c */
void* sbrk(size_t n, struct _mm_struct_mos* mm);


/* mmap.c */
virtual_addr do_mmap(
  uint32_t addr,
  size_t len, 
  uint32_t prot,
  uint32_t flag, 
  int32_t fd
);
virtual_addr do_brk(virtual_addr addr, size_t len);

#endif