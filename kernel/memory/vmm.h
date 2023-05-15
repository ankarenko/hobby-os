#ifndef VMM_H
#define VMM_H

#include <stdbool.h>
#include <stdint.h>

#include "pmm.h"
#include "vmm_pde.h"
#include "vmm_pte.h"

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
  | VMALLOC                 |
  |-------------------------| 0xE0000000
  |                         |
  |                         |
  | Kernel heap             |
  |                         |
  |_________________________| 0xC8000000
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
  |_________________________| 0x100000
  |                         |
  | Reversed for devices    |
  +_________________________+ 0x00000000
*/

//! virtual address
typedef uint32_t virtual_addr;

//! i86 architecture defines 1024 entries per table--do not change
#define PAGES_PER_TABLE 1024
#define PAGES_PER_DIR 1024

#define PAGE_DIRECTORY_INDEX(x) (((x) >> 22) & 0x3ff)
#define PAGE_TABLE_INDEX(x) (((x) >> 12) & 0x3ff)
#define PAGE_GET_PHYSICAL_ADDRESS(x) (*x & ~0xfff)

//! page table represents 4mb address space
#define PTABLE_ADDR_SPACE_SIZE 0x400000

//! directory table represents 4gb address space
#define DTABLE_ADDR_SPACE_SIZE 0x100000000

//! page sizes are 4k
#define PAGE_SIZE 4096

//! page table
struct ptable {
  pt_entry m_entries[PAGES_PER_TABLE];
};

//! page directory
struct pdirectory {
  pd_entry m_entries[PAGES_PER_DIR];
};

void vmm_init();

//! maps phys to virtual address
void vmm_map_page(void* phys, void* virt);

//! allocates a page in physical memory
bool vmm_alloc_page(pt_entry*);

//! frees a page in physical memory
void vmm_free_page(pt_entry* e);

//! switch to a new page directory
bool vmm_switch_pdirectory(struct pdirectory*);

//! get current page directory
struct pdirectory* vmm_get_directory();

//! flushes a cached translation lookaside buffer (TLB) entry
void vmm_flush_tlb_entry(virtual_addr addr);

//! clears a page table
void vmm_ptable_clear(struct ptable* p);

//! convert virtual address to page table index
uint32_t vmm_ptable_virt_to_index(virtual_addr addr);

//! get page entry from page table
pt_entry* vmm_ptable_lookup_entry(struct ptable* p, virtual_addr addr);

//! convert virtual address to page directory index
uint32_t vmm_pdirectory_virt_to_index(virtual_addr addr);

//! clears a page directory table
void vmm_pdirectory_clear(struct pdirectory* dir);

//! get directory entry from directory table
pd_entry* vmm_pdirectory_lookup_entry(struct pdirectory* p, virtual_addr addr);

#endif