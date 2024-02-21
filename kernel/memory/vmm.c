#include "kernel/util/stdio.h"
#include "kernel/util/string/string.h"
#include "kernel/util/math.h"
#include "kernel/util/list.h"
#include "kernel/memory/malloc.h"
#include "kernel/memory/kernel_info.h"
#include "kernel/memory/pmm.h"
#include "kernel/util/debug.h"
#include "kernel/proc/task.h"

#include "kernel/memory/vmm.h"


//! current directory table (global)
struct pdirectory* _current_dir = 0;
struct pdirectory* _kernel_dir = 0;
physical_addr _kernel_dir_phys = 0;
physical_addr _current_dir_phys = 0;


bool vmm_switch_pdirectory(struct pdirectory* dir) {
  if (!dir)
    return false;

  _current_dir = dir;
  _current_dir_phys = vmm_get_physical_address(dir, false); // pmm_get_PDBR();

  pmm_load_PDBR(_current_dir_phys);

  return true;
}

bool vmm_switch_back() {
  _current_dir = _kernel_dir;
  _current_dir_phys = _kernel_dir_phys;

  pmm_load_PDBR(_current_dir_phys);
  return true;
}

bool vmm_alloc_page(pt_entry* e) {
  //! allocate a free physical frame
  void* p = pmm_alloc_frame();
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
    pmm_free_frame(p);

  pt_entry_del_attrib(e, I86_PTE_PRESENT);
}

/* create new address space. */
struct pdirectory* vmm_create_address_space() {
	struct pdirectory* space;

	/* we need our page directory to be aligned, 0-11 bits should be zero */
  space = kcalloc_aligned(PAGE_SIZE, sizeof(char), PAGE_SIZE);
  
	/* clear page directory and clone kernel space. */
	vmm_pdirectory_clear(space);
	vmm_clone_kernel_space(space);
  physical_addr phys = vmm_get_physical_address(space, false);
  
  assert(phys % PAGE_SIZE == 0); // check alignment 

  // recursive trick
  space->m_entries[TABLES_PER_DIR - 1] = 
    phys | 
    I86_PDE_PRESENT | 
    I86_PDE_WRITABLE;

	return space;
}


void vmm_unmap_address(virtual_addr virt) {
  struct pdirectory* va_dir = PAGE_DIRECTORY_BASE;

  if (virt != PAGE_ALIGN(virt)) {
    err("0x%x is not page aligned", virt);
  }
		//dlog("0x%x is not page aligned", virt);

	if (!pd_entry_is_present(va_dir->m_entries[PAGE_DIRECTORY_INDEX(virt)])) {
		err("PD entry not present");
    return;
  }

	struct ptable *pt = (struct ptable *)(PAGE_TABLE_VIRT_ADDRESS(virt));
  uint32_t pte = PAGE_TABLE_INDEX(virt);

	if (!pt_entry_is_present(pt->m_entries[pte])) {
    err("PT entry not present");
		return;
  }

	pt->m_entries[pte] = 0;
	vmm_flush_tlb_entry(virt);
}

void vmm_unmap_range(virtual_addr vm_start, virtual_addr vm_end) {
	assert(PAGE_ALIGN(vm_start) == vm_start);
	assert(PAGE_ALIGN(vm_end) == vm_end);

	for (uint32_t addr = vm_start; addr < vm_end; addr += PMM_FRAME_SIZE)
		vmm_unmap_address(addr);
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

struct pdirectory* vmm_get_kernel_space() {
  return _current_dir;
}

void pmmngr_load_PDBR(physical_addr addr)
{

	asm volatile("mov %0, %%cr3" ::"r"(addr));
}

void vmm_flush_tlb_entry(virtual_addr addr) {
  __asm__ __volatile__("invlpg (%0)" ::"r"(addr)
						 : "memory");
  /*
  doesn't work 
  __asm__ __volatile__(
      "cli        \n\t"
      "invlpg %0  \n\t"
      "sti        \n\t"
      :
      : "m"(addr)
      :);
  */
}

struct pdirectory *vmm_fork(struct pdirectory *va_dir) {
  lock_scheduler(); 

  struct pdirectory *forked_dir = vmm_create_address_space();
  void *aligned = kalign_heap(PMM_FRAME_ALIGN);

  uint32_t start_heap = (uint32_t)sbrk(0, NULL);

  // NOTE: make sure we are not exceeding the boundaries of the kernel heap
  assert(
    ALIGN_UP(start_heap + sizeof(struct ptable) + PMM_FRAME_SIZE, PMM_FRAME_SIZE) <= KERNEL_HEAP_TOP
  );

  assert(start_heap % PMM_FRAME_ALIGN == 0);

  for (int ipd = 0; ipd < 768; ++ipd) {
    if (pd_entry_is_present(va_dir->m_entries[ipd])) {
      struct ptable *forked_pt = (struct ptable *)start_heap;
      uint32_t heap_current = start_heap;
      void* forked_pt_paddr = pmm_alloc_frame();
      vmm_map_address((uint32_t)forked_pt, forked_pt_paddr, I86_PTE_PRESENT | I86_PTE_WRITABLE);
			memset((void*)forked_pt, 0, sizeof(struct ptable));
      heap_current += sizeof(struct ptable);
  
      struct ptable *pt = PAGE_TABLE_BASE + ipd * PMM_FRAME_SIZE;

      for (int ipt = 0; ipt < PAGES_PER_TABLE; ++ipt) {
        if (pt_entry_is_present(pt->m_entries[ipt])) {
          if (ipt == 4 && ipd == 1) {
            int asdf = 1;
          }
          uint8_t *pte = (ipd << 10 | (0b1111111111 & ipt)) << 12;  // NOTE: lowest virtual address assigned to ipd and ipt
					uint8_t *forked_pte = heap_current;
					void* forked_pte_paddr = pmm_alloc_frame();
          //6766592
          vmm_map_address((uint32_t)forked_pte, forked_pte_paddr, I86_PTE_PRESENT | I86_PTE_WRITABLE);
          //memset(forked_pte, 0, PMM_FRAME_SIZE);
          memcpy((void*)forked_pte, (void*)pte, PMM_FRAME_SIZE);
          vmm_unmap_address((uint32_t)forked_pte);

          forked_pt->m_entries[ipt] = (uint32_t)forked_pte_paddr | I86_PTE_PRESENT | I86_PTE_WRITABLE | I86_PTE_USER;
          ////ffff030000000f4cc281e7ffffff89
          /*
          forked_pt->m_entries[ipt] = pt->m_entries[ipt];
          pt_entry_set_frame(&forked_pt->m_entries[ipt], forked_pte_paddr);
          */
        }
      }
      //6746215
      vmm_unmap_address((uint32_t)forked_pt);
      forked_dir->m_entries[ipd] = (uint32_t)forked_pt_paddr | I86_PDE_PRESENT | I86_PDE_WRITABLE | I86_PDE_USER;
      /*
      forked_dir->m_entries[ipd] = va_dir->m_entries[ipd];
      pd_entry_set_frame(&forked_dir->m_entries[ipd], forked_pt_paddr);
      */
    }
  }

  if (aligned) {
    kfree(aligned);
  }

  unlock_scheduler();
  return forked_dir;
}

void vmm_init_and_map(struct pdirectory* va_dir, uint32_t vaddr, uint32_t paddr, uint32_t num_of_pages) {
  uint32_t pa_table = (uint32_t)pmm_alloc_frame();
  struct ptable* va_table = (struct ptable*)(pa_table + KERNEL_HIGHER_HALF);
  memset(va_table, 0, sizeof(struct ptable));

  uint32_t ivirtual = vaddr;
  uint32_t iframe = paddr;

  for (int i = 0; i < num_of_pages; ++i, ivirtual += PMM_FRAME_SIZE, iframe += PMM_FRAME_SIZE) {
    pt_entry* entry = &va_table->m_entries[PAGE_TABLE_INDEX(ivirtual)];
    pt_entry_set_frame(entry, iframe);
    pt_entry_add_attrib(entry, I86_PTE_PRESENT | I86_PTE_WRITABLE);
    pmm_mark_used_addr(iframe);
  }

  pd_entry* entry = &va_dir->m_entries[PAGE_DIRECTORY_INDEX((virtual_addr)vaddr)];
  pd_entry_set_frame(entry, pa_table);
  pd_entry_add_attrib(entry, I86_PTE_PRESENT | I86_PTE_WRITABLE);
}


// TODO: dereferencing zero address should though a page fault!!!
void vmm_init() {
  uint32_t pa_dir = (uint32_t)pmm_alloc_frame();
  struct pdirectory* va_dir = (struct pdirectory*)(pa_dir + KERNEL_HIGHER_HALF);
  memset(va_dir, 0, sizeof(struct pdirectory));

  //make identity map of first MB and mark it used 
  vmm_init_and_map(va_dir, 0x00000000, 0x00000000, PAGES_PER_TABLE >> 2); // 1mb
  vmm_init_and_map(va_dir, KERNEL_HIGHER_HALF, 0x00000000, PAGES_PER_TABLE); // 4mb

  // NOTE: MQ 2019-11-21 Preallocate ptable for higher halkernel
  for (int i = PAGE_DIRECTORY_INDEX(KERNEL_HIGHER_HALF) + 1; i < TABLES_PER_DIR; ++i)
    vmm_alloc_ptable(va_dir, i, I86_PTE_WRITABLE);
  
  // NOTE: MQ 2019-05-08 Using the recursive page directory trick when paging (map last entry to directory)
  pd_entry* entry = &va_dir->m_entries[TABLES_PER_DIR - 1];
  //va_dir->m_entries[1023] = (pa_dir & 0xFFFFF000) | I86_PTE_PRESENT | I86_PTE_WRITABLE;
  
  pd_entry_set_frame(entry, pa_dir);
  pd_entry_add_attrib(entry, I86_PDE_PRESENT | I86_PDE_WRITABLE);

  // for editing userspace tables
  memcpy(
    &va_dir->m_entries[TABLES_PER_DIR - 2], 
    &va_dir->m_entries[TABLES_PER_DIR - 1], 
    sizeof(pd_entry)
  );
  
  vmm_paging(va_dir, pa_dir);
}


void vmm_load_usertable(physical_addr p_dir) {
  struct pdirectory* va_dir = PAGE_DIRECTORY_BASE;
  pd_entry* entry = &va_dir->m_entries[TABLES_PER_DIR - 2];
  assert(p_dir % PAGE_SIZE == 0);
  pd_entry_set_frame(entry, p_dir);
  vmm_flush_tlb_entry(va_dir);
}

void vmm_alloc_ptable(struct pdirectory* va_dir, uint32_t index, uint32_t flags) {
  pd_entry* entry = &va_dir->m_entries[index];

  if (pd_entry_is_present(*entry))
    return;

  uint32_t pa_table = (uint32_t)pmm_alloc_frame();

  //! clear page table
  //memset(PAGE_TABLE_PHYS_ADDRESS(virt), 0, sizeof(struct ptable));

  pd_entry_set_frame(entry, pa_table);
  pd_entry_add_attrib(entry, I86_PTE_PRESENT | flags);
}

void vmm_paging(struct pdirectory* va_dir, uint32_t pa_dir) {
  _current_dir = va_dir;
  _current_dir_phys = pa_dir;
  _kernel_dir = va_dir;
  _kernel_dir_phys = pa_dir;

  __asm__ __volatile__(
      "mov %0, %%cr3           \n"
      "mov %%cr4, %%ecx        \n"
      "or $0x00000010, %%ecx   \n"  // i don't know why but "and $~0x00000010, %%ecx doesnt work (QEMU)
      "mov %%ecx, %%cr4        \n"
      "mov %%cr0, %%ecx        \n"
      "or $0x80000000, %%ecx   \n"
      "mov %%ecx, %%cr0        \n" ::"r"(pa_dir));
}

bool vmm_is_kernel_directory(struct pdirectory *dir) {
  return dir == _kernel_dir;
}

physical_addr vmm_get_physical_address(
  virtual_addr vaddr, 
  bool is_page
) {
  uint32_t* table = PAGE_TABLE_VIRT_ADDRESS(vaddr);
  uint32_t tindex = PAGE_TABLE_INDEX(vaddr);
  uint32_t paddr = table[tindex];
  return is_page ? paddr : (paddr & ~0xfff) | (vaddr & 0xfff);
}

void vmm_map_address(uint32_t virt, uint32_t phys, uint32_t flags) {
  struct pdirectory* va_dir = PAGE_DIRECTORY_BASE;

  if (virt != PAGE_ALIGN(virt)) {
    err("0x%x is not page aligned", virt);
  }

  if (!pd_entry_is_present(va_dir->m_entries[PAGE_DIRECTORY_INDEX(virt)]))
    vmm_create_page_table(va_dir, virt, flags);

  struct ptable* table = (struct ptable*)(PAGE_TABLE_VIRT_ADDRESS(virt));
  uint32_t tindex = PAGE_TABLE_INDEX(virt);

  pt_entry* entry = &table->m_entries[tindex];

  pt_entry_add_attrib(entry, flags);
  pt_entry_set_frame(entry, phys);
  vmm_flush_tlb_entry(virt);
}

void vmm_create_page_table(struct pdirectory* va_dir, uint32_t virt, uint32_t flags) {
  if (pd_entry_is_present(va_dir->m_entries[PAGE_DIRECTORY_INDEX(virt)]))
    return;

  physical_addr pa_table = (uint32_t)pmm_alloc_frame();

  pd_entry* entry = &va_dir->m_entries[PAGE_DIRECTORY_INDEX(virt)];
  pd_entry_add_attrib(entry, flags);
  pd_entry_set_frame(entry, pa_table);
  vmm_flush_tlb_entry(virt);
  virtual_addr va_table = PAGE_TABLE_VIRT_ADDRESS(virt);

  // we can't access directly, 
  // because the directory maybe is not set to cr3 yet
  // but we can always have a recursive table hack
  memset(va_table, 0, sizeof(struct ptable));
  //return va_table;
}


virtual_addr vmm_alloc_size(virtual_addr from, uint32_t size, uint32_t flags) {
  /*
  if (size == 0) {
    return; 
  }

  uint32_t phyiscal_addr = (uint32_t)pmm_alloc_frames(div_ceil(size, PMM_FRAME_SIZE));
  uint32_t page_addr = (from / PMM_FRAME_SIZE) * PMM_FRAME_SIZE;
  for (; page_addr < from + size; page_addr += PMM_FRAME_SIZE, phyiscal_addr += PMM_FRAME_SIZE) {
    vmm_map_address(vmm_get_directory(), page_addr, phyiscal_addr, flags);
  }

  return page_addr - PMM_FRAME_SIZE + size;
  */
}

void vmm_pdirectory_clear(struct pdirectory* dir) {
  if (dir) {
    memset(dir, sizeof(struct pdirectory), 0);
  }
}

void vmm_clone_kernel_space(struct pdirectory* dir) { 
  if (dir) {
    uint32_t kernel_dir_index = PAGE_DIRECTORY_INDEX(KERNEL_HIGHER_HALF);
    struct pdirectory* cur_dir = PAGE_DIRECTORY_BASE;
    pd_entry* kernel = &cur_dir->m_entries[kernel_dir_index];
    
    /* copy kernel page tables into this new page directory.
    Recall that KERNEL SPACE is 0xc0000000, which starts at
    entry 768. */
    memcpy(
      &dir->m_entries[0], 
      &cur_dir->m_entries[0],
      sizeof(pd_entry)
    );
    memcpy(
      &dir->m_entries[kernel_dir_index], 
      kernel, 
      (TABLES_PER_DIR - kernel_dir_index) * sizeof(pd_entry)
    );
    
  }
}