#include <math.h>
#include <string.h>

#include "malloc.h"
#include "pmm.h"
#include "vmm.h"

static virtual_addr _kernel_heap_current = KERNEL_HEAP_BOTTOM;
static uint32_t _kernel_remaining_from_last_used = 0;

void* sbrk(size_t n, virtual_addr* brk, virtual_addr* remaning, uint32_t flags) {
  virtual_addr* kernel_heap_current = &_kernel_heap_current;
  uint32_t* kernel_remaining_from_last_used = &_kernel_remaining_from_last_used;

  // an adjustment for using sbrk with malloc in userspace
  if (brk && remaning) {
    kernel_heap_current = brk;
    kernel_remaining_from_last_used = remaning;
  }

  if (n == 0)
    return (char *)(*kernel_heap_current);

  char *heap_base = (char *)(*kernel_heap_current);

  if (n <= *kernel_remaining_from_last_used) {
    *kernel_remaining_from_last_used = *kernel_remaining_from_last_used - n;
  } else {
    uint32_t phyiscal_addr = (uint32_t)pmm_alloc_frames(
      div_ceil(n - *kernel_remaining_from_last_used, PMM_FRAME_SIZE)
    );

    if (phyiscal_addr == 0) { // error while allocating frames
      return 0;
    }

    uint32_t page_addr = div_ceil(*kernel_heap_current, PMM_FRAME_SIZE) * PMM_FRAME_SIZE;
    // todo: check if page_addr is less than KERNEL_HEAP_BOTTOM, otherwise thrown an error
    for (
      ; 
      page_addr < *kernel_heap_current + n; 
      page_addr += PMM_FRAME_SIZE, phyiscal_addr += PMM_FRAME_SIZE
    ) {
      // todo: check if page already mapped?
      vmm_map_address(page_addr, phyiscal_addr, I86_PTE_PRESENT | I86_PTE_WRITABLE | flags);
    }
    
    *kernel_remaining_from_last_used = page_addr - (*kernel_heap_current + n);
  }

  *kernel_heap_current += n;
  memset(heap_base, 0, n); // clear garbage
  return heap_base;
}
