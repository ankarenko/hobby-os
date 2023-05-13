#include "pmm.h"

#include <stdbool.h>
#include <string.h>

#define INDEX_FROM_BIT(a) (a / 4 * PMM_BLOCKS_PER_BYTE)
#define OFFSET_FROM_BIT(a) (a % 4 * PMM_BLOCKS_PER_BYTE)

// size of physical memory
static uint32_t _memory_size = 0;
// number of blocks currently in use
static uint32_t _used_blocks = 0;
// maximum number of available memory blocks
static uint32_t _max_blocks = 0;
// memory map bit array. Each bit represents a memory block
static uint32_t* _memory_map = 0;

void memory_bitmap_set(uint32_t bit) {
  _memory_map[INDEX_FROM_BIT(bit)] |= (1 << (OFFSET_FROM_BIT(bit)));
}

void memory_bitmap_unset(uint32_t bit) {
  _memory_map[INDEX_FROM_BIT(bit)] &= ~(1 << (OFFSET_FROM_BIT(bit)));
}

bool memory_bitmap_test(uint32_t bit) {
  return _memory_map[INDEX_FROM_BIT(bit)] & (1 << (OFFSET_FROM_BIT(bit)));
}

int32_t memory_bitmap_first_free() {
  //! find the first free bit
  for (uint32_t i = 0; i < pmm_get_block_count() / 32; i++)
    if (_memory_map[i] != 0xffffffff)
      for (uint32_t j = 0; j < 32; j++) {  //! test each bit in the dword

        uint32_t bit = 1 << j;
        if (!(_memory_map[i] & bit))
          return i * 4 * PMM_BLOCKS_PER_BYTE + j;
      }

  return -1;
}

int32_t memory_bitmap_first_free_s(uint32_t size) {
  if (size == 0)
    return -1;

  if (size == 1)
    return memory_bitmap_first_free();

  for (uint32_t i = 0; i < pmm_get_block_count() / 32; i++)
    if (_memory_map[i] != 0xffffffff)
      for (uint32_t j = 0; j < 32; j++) {  //! test each bit in the dword

        uint32_t bit = 1 << j;
        if (!(_memory_map[i] & bit)) {
          uint32_t startingBit = i * 32;
          startingBit += bit;  // get the free bit in the dword at index i

          uint32_t free = 0;  // loop through each bit to see if its enough space
          for (uint32_t count = 0; count <= size; count++) {
            if (!memory_bitmap_test(startingBit + count))
              free++;  // this bit is clear (free frame)

            if (free == size)
              return i * 4 * PMM_BLOCKS_PER_BYTE + j;  // free count==size needed; return index
          }
        }
      }

  return -1;
}

void pmm_init(uint32_t memSize, physical_addr bitmap) {
  _memory_size = memSize;
  _memory_map = (uint32_t*)bitmap;
  _max_blocks = pmm_get_memory_size() / PMM_BLOCK_SIZE;
  _used_blocks = pmm_get_block_count();

  //! By default, all of memory is in use
  memset(_memory_map, 0xf, pmm_get_block_count() / PMM_BLOCKS_PER_BYTE);
}

void pmm_init_region(physical_addr base, uint32_t size) {
  int align = base / PMM_BLOCK_SIZE;
  int blocks = size / PMM_BLOCK_SIZE;

  for (; blocks > 0; blocks--) {
    memory_bitmap_unset(align++);
    _used_blocks--;
  }

  memory_bitmap_set(0);  // first block is always set. This insures allocs cant be 0
}

void pmm_deinit_region(physical_addr base, uint32_t size) {
  int align = base / PMM_BLOCK_SIZE;
  int blocks = size / PMM_BLOCK_SIZE;

  for (; blocks > 0; blocks--) {
    memory_bitmap_set(align++);
    _used_blocks++;
  }
}

void* pmm_alloc_block() {
  if (pmm_get_free_block_count() <= 0)
    return 0;  // out of memory

  int frame = memory_bitmap_first_free();

  if (frame == -1)
    return 0;  // out of memory

  memory_bitmap_set(frame);

  physical_addr addr = frame * PMM_BLOCK_SIZE;
  _used_blocks++;

  return (void*)addr;
}

void pmm_paging_enable(bool b) {
  uint32_t cr0 = 0;
  asm volatile("mov %%cr0, %0"
               : "=r"(cr0));
  cr0 |= b ? 0x80000000 : 0x7FFFFFFF;  // Enable paging!
  asm volatile("mov %0, %%cr0" ::"r"(cr0));
}

bool pmm_is_paging() {
  uint32_t cr0;
  asm volatile("mov %%cr0, %0"
               : "=r"(cr0));
  return (cr0 & 0x80000000) ? true : false;
}

void pmm_load_PDBR(physical_addr addr) {
  asm volatile("mov %0, %%cr3" ::"r"(addr));
}

physical_addr pmm_get_PDBR() {
  uint32_t cr3;
  asm volatile("mov %%cr3, %0"
               : "=r"(cr3));
  return cr3;
}

void* pmm_alloc_blocks(uint32_t size) {
  if (pmm_get_free_block_count() <= size)
    return 0;  // not enough space

  int frame = memory_bitmap_first_free_s(size);

  if (frame == -1)
    return 0;  // not enough space

  for (uint32_t i = 0; i < size; i++)
    memory_bitmap_set(frame + i);

  physical_addr addr = frame * PMM_BLOCK_SIZE;
  _used_blocks += size;

  return (void*)addr;
}

void pmm_free_block(void* p) {
  physical_addr addr = (physical_addr)p;
  int frame = addr / PMM_BLOCK_SIZE;

  memory_bitmap_unset(frame);

  _used_blocks--;
}

uint32_t pmm_get_memory_size() {
  return _memory_size;
}

uint32_t pmm_get_block_count() {
  return _max_blocks;
}

uint32_t pmm_get_use_block_count() {
  return _used_blocks;
}

uint32_t pmm_get_free_block_count() {
  return _max_blocks - _used_blocks;
}

uint32_t pmm_get_block_size() {
  return PMM_BLOCK_SIZE;
}
