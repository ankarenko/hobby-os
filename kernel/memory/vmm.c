#include <string.h>
#include <stdio.h>

#include "vmm.h"

//! current directory table (global)
struct pdirectory* _cur_directory = 0;
//! current page directory base register
physical_addr _cur_pdbr = 0;

bool vmm_switch_pdirectory(struct pdirectory* dir) {
  if (!dir)
    return false;

  _cur_directory = dir;
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
  return _cur_directory;
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

void vmm_initialize() {
  //! allocate default page table
  struct ptable* table = (struct ptable*)pmm_alloc_block();
  if (!table)
    return;

  //! allocates 3gb page table
  struct ptable* table2 = (struct ptable*)pmm_alloc_block();
  if (!table2)
    return;

  //! clear page table
  memset(table, 0, sizeof(struct ptable));

  //! 1st 4mb are idenitity mapped
  for (
    uint32_t i = 0, frame = 0x0, virt = 0x00000000; 
    i < PAGES_PER_TABLE; 
    i++, frame += PAGE_SIZE, virt += PAGE_SIZE
  ) {
    //! create a new page
    pt_entry page = 0;
    pt_entry_add_attrib(&page, I86_PTE_PRESENT);
    pt_entry_set_frame(&page, frame);

    //! ...and add it to the page table
    table2->m_entries[PAGE_TABLE_INDEX(virt)] = page;

    if (i == PAGES_PER_TABLE) {
      printf("Index: %d : [%x]\n", PAGE_TABLE_INDEX(virt), frame + PAGE_SIZE);
    }
    
  }

  //! map 1mb to 3gb (where we are at)
  for (
    uint32_t i = 0, frame = 0x100000, virt = 0xc0000000; 
    i < PAGES_PER_TABLE; 
    i++, frame += PAGE_SIZE, virt += PAGE_SIZE
  ) {
    //! create a new page
    pt_entry page = 0;
    pt_entry_add_attrib(&page, I86_PTE_PRESENT);
    pt_entry_set_frame(&page, frame);

    //! ...and add it to the page table
    table->m_entries[PAGE_TABLE_INDEX(virt)] = page;
  }

  //! create default directory table
  struct pdirectory* dir = (struct pdirectory*)pmm_alloc_blocks(3);
  if (!dir)
    return;

  //! clear directory table and set it as current
  memset(dir, 0, sizeof(struct pdirectory));

  pd_entry* entry = &dir->m_entries[PAGE_DIRECTORY_INDEX(0xc0000000)];
  pd_entry_add_attrib(entry, I86_PDE_PRESENT);
  pd_entry_add_attrib(entry, I86_PDE_WRITABLE);
  pd_entry_set_frame(entry, (physical_addr)table);

  pd_entry* entry2 = &dir->m_entries[PAGE_DIRECTORY_INDEX(0x00000000)];
  pd_entry_add_attrib(entry2, I86_PDE_PRESENT);
  pd_entry_add_attrib(entry2, I86_PDE_WRITABLE);
  pd_entry_set_frame(entry2, (physical_addr)table2);

  //! store current PDBR
  _cur_pdbr = (physical_addr)&dir->m_entries;

  //! switch to our page directory
  vmm_switch_pdirectory(dir);

  //! enable paging
  pmm_paging_enable(true);
}