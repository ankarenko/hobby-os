#include "vmm.h"

#include <stdio.h>
#include <string.h>

#include "./kernel_info.h"

//! current directory table (global)
struct pdirectory* _current_dir = 0;
//! current page directory base register
physical_addr _cur_pdbr = 0;

bool vmm_switch_pdirectory(struct pdirectory* dir) {
  if (!dir)
    return false;

  _current_dir = dir;
  pmm_load_PDBR(_cur_pdbr);
  return true;
}

bool vmm_alloc_page(pt_entry* e) {
  //! allocate a free physical frame
  void* p = pmm_alloc_block();
  if (!p)
    return false;

  //! map it to the page
  pt_entry_set_frame(e, (physical_addr)p);
  pt_entry_add_attrib(e, I86_PTE_PRESENT);

  return true;
}

void vmm_free_page(pt_entry* e) {
  void* p = (void*)pt_entry_pfn(*e);
  if (p)
    pmm_free_block(p);

  pt_entry_del_attrib(e, I86_PTE_PRESENT);
}

pt_entry* vmm_ptable_lookup_entry(struct ptable* p, virtual_addr addr) {
  return p ? &p->m_entries[PAGE_TABLE_INDEX(addr)] : 0;
}

pd_entry* vmm_pdirectory_lookup_entry(struct pdirectory* p, virtual_addr addr) {
  return p ? &p->m_entries[PAGE_TABLE_INDEX(addr)] : 0;
}

struct pdirectory* vmm_get_directory() {
  return _current_dir;
}

void vmm_flush_tlb_entry(virtual_addr addr) {
  __asm__ __volatile__(
      "cli        \n\t"
      "invlpg %0  \n\t"
      "sti        \n\t"
      :
      : "m"(addr)
      :);
}

void vmm_map_page(void* phys, void* virt) {
  //! get page directory
  struct pdirectory* pageDirectory = vmm_get_directory();

  //! get page table
  pd_entry* e = &pageDirectory->m_entries[PAGE_DIRECTORY_INDEX((uint32_t)virt)];
  if ((*e & I86_PTE_PRESENT) != I86_PTE_PRESENT) {
    //! page table not present, allocate it
    struct ptable* table = (struct ptable*)pmm_alloc_block();
    if (!table)
      return;

    //! clear page table
    memset(table, 0, sizeof(struct ptable));

    //! create a new entry
    pd_entry* entry = &pageDirectory->m_entries[PAGE_DIRECTORY_INDEX((uint32_t)virt)];

    //! map in the table (Can also just do *entry |= 3) to enable these bits
    pd_entry_add_attrib(entry, I86_PDE_PRESENT);
    pd_entry_add_attrib(entry, I86_PDE_WRITABLE);
    pd_entry_set_frame(entry, (physical_addr)table);
  }
  //! get table
  struct ptable* table = (struct ptable*)PAGE_GET_PHYSICAL_ADDRESS(e);

  //! get page
  pt_entry* page = &table->m_entries[PAGE_TABLE_INDEX((uint32_t)virt)];

  //! map it in (Can also do (*page |= 3 to enable..)
  pt_entry_set_frame(page, (physical_addr)phys);
  pt_entry_add_attrib(page, I86_PTE_PRESENT);
}

void vmm_init_and_map(struct pdirectory* va_dir, uint32_t vaddr, uint32_t paddr) {
  uint32_t pa_table = (uint32_t)pmm_alloc_block();
  struct ptable* va_table = (struct ptable*)(pa_table + KERNEL_HIGHER_HALF);
  memset(va_table, 0, sizeof(struct ptable));

  uint32_t ivirtual = vaddr;
  uint32_t iframe = paddr;

  for (int i = 0; i < PAGES_PER_TABLE; ++i, ivirtual += PMM_BLOCK_SIZE, iframe += PMM_BLOCK_SIZE) {
    pt_entry* entry = &va_table->m_entries[PAGE_TABLE_INDEX(ivirtual)];
    pt_entry_set_frame(entry, iframe);
    pt_entry_add_attrib(entry, I86_PTE_PRESENT);
    pt_entry_add_attrib(entry, I86_PTE_WRITABLE);
    pmm_mark_used_addr(iframe);
  }

  pd_entry* entry = &va_dir->m_entries[PAGE_DIRECTORY_INDEX((virtual_addr)va_table)];
  pd_entry_set_frame(entry, pa_table);
  pd_entry_add_attrib(entry, I86_PTE_PRESENT);
  pd_entry_add_attrib(entry, I86_PTE_WRITABLE);
}

void vmm_init() {
  uint32_t pa_dir = (uint32_t)pmm_alloc_block();
  struct pdirectory* va_dir = (struct pdirectory*)(pa_dir + KERNEL_HIGHER_HALF);
  memset(va_dir, 0, sizeof(struct pdirectory));

  vmm_init_and_map(va_dir, KERNEL_HIGHER_HALF, 0x00000000);

  // NOTE: MQ 2019-11-21 Preallocate ptable for higher half kernel
  for (int i = PAGE_DIRECTORY_INDEX(KERNEL_HIGHER_HALF) + 1; i < PAGES_PER_DIR; ++i)
    vmm_alloc_ptable(va_dir, i);

  // NOTE: MQ 2019-05-08 Using the recursive page directory trick when paging (map last entry to directory)
  pd_entry* entry = &va_dir->m_entries[PAGES_PER_DIR - 1];
  pd_entry_set_frame(entry, pa_dir & 0xFFFFF000);
  pd_entry_add_attrib(entry, I86_PTE_PRESENT);
  pd_entry_add_attrib(entry, I86_PTE_WRITABLE);

  vmm_paging(va_dir, pa_dir);
}

void vmm_alloc_ptable(struct pdirectory* va_dir, uint32_t index) {
  pd_entry entry = &va_dir->m_entries[index];

  if (pd_entry_is_present(entry))
    return;

  uint32_t pa_table = (uint32_t)pmm_alloc_block();
  pd_entry_set_frame(entry, pa_table);
  pd_entry_add_attrib(entry, I86_PTE_PRESENT);
  pd_entry_add_attrib(entry, I86_PTE_WRITABLE);
}

void vmm_paging(struct pdirectory* va_dir, uint32_t pa_dir) {
  _current_dir = va_dir;

  __asm__ __volatile__(
      "mov %0, %%cr3           \n"
      "mov %%cr4, %%ecx        \n"
      "and $~0x00000010, %%ecx \n"
      "mov %%ecx, %%cr4        \n"
      "mov %%cr0, %%ecx        \n"
      "or $0x80000000, %%ecx   \n"
      "mov %%ecx, %%cr0        \n" ::"r"(pa_dir));
}