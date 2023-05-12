#ifndef VMM_H
#define VMM_H

#include <stdbool.h>
#include <stdint.h>

#include "pmm.h"
#include "vmm_pte.h"
#include "vmm_pde.h"

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

#endif