#include <math.h>
#include <string.h>

#include "malloc.h"
#include "pmm.h"
#include "vmm.h"

static uint32_t _kernel_heap_current = KERNEL_HEAP_BOTTOM;
static uint32_t _kernel_remaining_from_last_used = 0;

void *sbrk(size_t n) {
  if (n == 0)
    return (char *)_kernel_heap_current;

  char *heap_base = (char *)_kernel_heap_current;

  if (n <= _kernel_remaining_from_last_used) {
    _kernel_remaining_from_last_used -= n;
  } else {
    uint32_t phyiscal_addr = (uint32_t)pmm_alloc_frames(div_ceil(n - _kernel_remaining_from_last_used, PMM_FRAME_SIZE));
    uint32_t page_addr = div_ceil(_kernel_heap_current, PMM_FRAME_SIZE) * PMM_FRAME_SIZE;
    for (; page_addr < _kernel_heap_current + n; page_addr += PMM_FRAME_SIZE, phyiscal_addr += PMM_FRAME_SIZE) {
      vmm_map_address(vmm_get_directory(), page_addr, phyiscal_addr, I86_PTE_PRESENT | I86_PTE_WRITABLE);
    }
    
    _kernel_remaining_from_last_used = page_addr - (_kernel_heap_current + n);
  }

  _kernel_heap_current += n;
  memset(heap_base, 0, n);
  return heap_base;
}
