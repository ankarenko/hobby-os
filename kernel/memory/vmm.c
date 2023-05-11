#include <stdio.h>
#include "vmm.h"
#include "../devices/tty.h"
#include "../cpu/hal.h"
#include "./pmm.h"

extern uint32_t* frames;
extern uint32_t nframes;

page_directory_t* current_directory;
page_directory_t* kernel_directory;

// Defined in kheap.c
extern uint32_t placement_address;

#define PHYSICAL_MEMORY 0x1000000 //assume it is 16MB
#define PANIC(x) terminal_writestring(x)

// Function to allocate a frame.
void alloc_frame(page_t *page, int is_kernel, int is_writeable)
{
  if (page->frame != 0)
  {
    return; // Frame was already allocated, return straight away.
  }
  else
  {
    uint32_t idx = first_frame(); // idx is now the index of the first free frame.
    if (idx == UINT32_MAX)
    {
      // PANIC is just a macro that prints a message to the screen then hits an infinite loop.
      PANIC("No free frames!");
    }
    set_frame(idx * PAGE_SIZE);        // this frame is now ours!
    page->present = 1;                 // Mark it as present.
    page->rw = (is_writeable) ? 1 : 0; // Should the page be writeable?
    page->user = (is_kernel) ? 0 : 1;  // Should the page be user-mode?
    page->frame = idx;
  }
}

// Function to deallocate a frame.
void free_frame(page_t *page)
{
  uint32_t frame;
  if (!(frame = page->frame))
  {
    return; // The given page didn't actually have an allocated frame!
  }
  else
  {
    clear_frame(frame); // Frame is now free again.
    page->frame = 0x0;  // Page now doesn't have a frame.
  }
}

void initialise_paging()
{
  uint32_t mem_end_page = PHYSICAL_MEMORY;

  nframes = mem_end_page / PAGE_SIZE;
  frames = (uint32_t *)kmalloc(INDEX_FROM_BIT(nframes));
  memset(frames, 0, INDEX_FROM_BIT(nframes));

  // Let's make a page directory.
  kernel_directory = (page_directory_t *)kmalloc_a(sizeof(page_directory_t));
  memset(kernel_directory, 0, sizeof(page_directory_t));
  current_directory = kernel_directory;

  // We need to identity map (phys addr = virt addr) from
  // 0x0 to the end of used memory, so we can access this
  // transparently, as if paging wasn't enabled.
  // NOTE that we use a while loop here deliberately.
  // inside the loop body we actually change placement_address
  // by calling kmalloc(). A while loop causes this to be
  // computed on-the-fly rather than once at the start.
  int i = 0;
  while (i < placement_address)
  {
    // Kernel code is readable but not writeable from userspace.
    alloc_frame(get_page(i, 1, kernel_directory), 0, 0);
    i += PAGE_SIZE;
  }
  // Before we enable paging, we must register our page fault handler.
  register_interrupt_handler(14, page_fault);

  // Now, enable paging!
  switch_page_directory(kernel_directory);
}

void switch_page_directory(page_directory_t *dir)
{
  current_directory = dir;
  asm volatile("mov %0, %%cr3" ::"r"(&dir->tablesPhysical));
  uint32_t cr0;
  asm volatile("mov %%cr0, %0"
               : "=r"(cr0));
  cr0 |= 0x80000000; // Enable paging!
  asm volatile("mov %0, %%cr0" ::"r"(cr0));
}

page_t *get_page(uint32_t address, int make, page_directory_t *dir)
{
  // Turn the address into an index.
  address /= PAGE_SIZE;
  // Find the page table containing this address.
  uint32_t table_idx = address / PAGES_N;
  if (dir->tables[table_idx]) // If this table is already assigned
  {
    return &dir->tables[table_idx]->pages[address % PAGES_N];
  }
  else if (make)
  {
    uint32_t tmp;
    dir->tables[table_idx] = (page_table_t *)kmalloc_ap(sizeof(page_table_t), &tmp);
    memset(dir->tables[table_idx], 0, PAGE_SIZE);
    dir->tablesPhysical[table_idx] = tmp | 0x7; // PRESENT, RW, US.
    return &dir->tables[table_idx]->pages[address % PAGES_N];
  }
  else
  {
    return 0;
  }
}

void page_fault(interrupt_registers regs)
{
  // A page fault has occurred.
  // The faulting address is stored in the CR2 register.
  uint32_t faulting_address;
  __asm__ __volatile__("mov %%cr2, %%eax	\n"
											 "mov %%eax, %0			\n"
											 : "=r"(faulting_address));

  // The error code gives us details of what happened.
  int present   = !(regs.err_code & 0x1); // Page not present
  int rw = regs.err_code & 0x2;           // Write operation?
  int us = regs.err_code & 0x4;           // Processor was in user-mode?
  int reserved = regs.err_code & 0x8;     // Overwritten CPU-reserved bits of page entry?
  int id = regs.err_code & 0x10;          // Caused by an instruction fetch?

  // Output an error message.
  terminal_writestring("Page fault! ( ");
  if (present) {terminal_writestring("present ");}
  if (rw) {terminal_writestring("read-only ");}
  if (us) {terminal_writestring("user-mode ");}
  if (reserved) {terminal_writestring("reserved ");}
  printf(") at %X\n", faulting_address); // TODO: hex writestring_hex
  
  disable_interrupts();
  while (1);
  
  PANIC("Page fault");
  
  
}

