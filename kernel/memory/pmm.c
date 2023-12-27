#include <stdbool.h>


#include "kernel/memory/pmm.h"
#include "kernel/util/string/string.h"
#include "kernel/util/math.h"
#include "kernel/util/debug.h"

#include "./kernel_info.h"

#define INDEX_FROM_BIT(a) (a / (4 * PMM_FRAMES_PER_BYTE))
#define OFFSET_FROM_BIT(a) (a % (4 * PMM_FRAMES_PER_BYTE))

// size of physical memory
static uint32_t _memory_size = 0;
// number of frames currently in use
static uint32_t _used_frames = 0;
// maximum number of available memory frames
static uint32_t _max_frames = 0;
// memory map bit array. Each bit represents a memory frame
static uint32_t* _memory_bitmap = 0;
static uint32_t _memory_bitmap_size = 0;  // 4294967295

void memory_bitmap_set(uint32_t bit) {
  _memory_bitmap[INDEX_FROM_BIT(bit)] |= (1 << (OFFSET_FROM_BIT(bit)));
}

void memory_bitmap_unset(uint32_t bit) {
  // uint32_t tmp = ~(1 << (OFFSET_FROM_BIT(bit)));
  // uint32_t tmp2 = _memory_bitmap[INDEX_FROM_BIT(bit)];
  // printf("%x & %x = %x\n", tmp2, tmp, tmp2 & tmp);
  _memory_bitmap[INDEX_FROM_BIT(bit)] &= ~(1 << (OFFSET_FROM_BIT(bit)));
}

bool memory_bitmap_test(uint32_t bit) {
  return _memory_bitmap[INDEX_FROM_BIT(bit)] & (1 << (OFFSET_FROM_BIT(bit)));
}

int32_t memory_bitmap_first_free() {
  //! find the first free bit
  for (uint32_t i = 0; i < div_ceil(pmm_get_frame_count(), PMM_FRAMES_PER_4BYTES); i++) {

    if (_memory_bitmap[i] != 0xffffffff) {
      for (uint32_t j = 0; j < PMM_FRAMES_PER_4BYTES; j++) {  //! test each bit in the dword

        uint32_t bit = 1 << j;
        if (!(_memory_bitmap[i] & bit))
          return i * PMM_FRAMES_PER_4BYTES + j;
      }
    }
  }

  return -1;
}

int32_t memory_bitmap_first_free_s(uint32_t size) {
  if (size == 0)
    return -1;

  if (size == 1)
    return memory_bitmap_first_free();

  for (uint32_t i = 0; i < pmm_get_frame_count() / PMM_FRAMES_PER_4BYTES; i++) {
    if (_memory_bitmap[i] != 0xffffffff) {
      for (uint32_t j = 0; j < PMM_FRAMES_PER_4BYTES; j++) {  //! test each bit in the dword

        uint32_t bit = 1 << j;
        if (!(_memory_bitmap[i] & bit)) {
          uint32_t startingBit = i * PMM_FRAMES_PER_4BYTES;
          startingBit += j;  // get the free bit in the dword at index i

          uint32_t free = 0;  // loop through each bit to see if its enough space
          for (uint32_t count = 0; count <= size; count++) {
            if (!memory_bitmap_test(startingBit + count))
              free++;  // this bit is clear (free frame)

            if (free == size)
              return i * PMM_FRAMES_PER_4BYTES + j;  // free count==size needed; return index
          }
        }
      }
    }
  }

  return -1;
}

void pmm_init(multiboot_info_t* mbd) {
  /* Check bit 6 to see if we have a valid memory map */
  if (!(mbd->flags >> 6 & 0x1)) {
    err("invalid memory map given by GRUB bootloader");
    return;
  }
  
  
  // make perfectly aligned and ignore some space to avoid mistakes in future
  _memory_size = ALIGN_DOWN((mbd->mem_lower + mbd->mem_upper) * 1024, PMM_FRAME_ALIGN);
  _memory_bitmap = (uint32_t*)KERNEL_END;
  _used_frames = _max_frames = div_ceil(_memory_size, PMM_FRAME_SIZE);

  _memory_bitmap_size = div_ceil(_max_frames, PMM_FRAMES_PER_BYTE);
  memset(_memory_bitmap, 0xff, BITMAP_SIZE /*_memory_bitmap_size*/);

  mbd->mmap_addr += KERNEL_HIGHER_HALF;
  for (uint32_t i = 0; i < mbd->mmap_length; i += sizeof(multiboot_memory_map_t)) {
    multiboot_memory_map_t* mmmt = (multiboot_memory_map_t*)(mbd->mmap_addr + i);

    if (mmmt->type >= MULTIBOOT_MEMORY_BADRAM && mmmt->addr_low == 0)
      break;

    if (mmmt->type == MULTIBOOT_MEMORY_AVAILABLE) {
      // for simplicity I align everything to PMM_FRAME_SIZE even thou it leads to a memory loss
      physical_addr start_aligned = ALIGN_UP(mmmt->addr_low, PMM_FRAME_ALIGN);
      uint32_t len_aligned = ALIGN_DOWN(mmmt->len_low, PMM_FRAME_ALIGN);

      int kb = (int)len_aligned / 1024;

      log("Start Addr: %X | Length: %d bytes | Size: %d KB",
            start_aligned, len_aligned, kb);

      pmm_init_region(start_aligned, len_aligned);
    }
  }

  if (_memory_bitmap_size > BITMAP_SIZE) {
    printf("bitmapsize is too big");
    return;
  }

  pmm_deinit_region(0x0, KERNEL_BOOT);
  pmm_deinit_region(KERNEL_BOOT, KERNEL_END + BITMAP_SIZE - KERNEL_START);
}

/*
void pmm_init(uint32_t memSize, physical_addr bitmap) {
  _memory_size = memSize;
  _memory_bitmap = (uint32_t*)bitmap;
  _max_frames = pmm_get_memory_size() / PMM_FRAME_SIZE;
  _used_frames = pmm_get_frame_count();

  //! By default, all of memory is in use
  memset(_memory_bitmap, 0xf, pmm_get_frame_count() / PMM_FRAMES_PER_BYTE);
}
*/

void pmm_init_region(physical_addr base, uint32_t size) {
  uint32_t align = base / PMM_FRAME_SIZE;
  uint32_t frames = div_ceil(size, PMM_FRAME_SIZE);

  for (uint32_t i = 0; i < frames; ++i) { 
    memory_bitmap_unset(align + i);
    _used_frames--;
  }

  memory_bitmap_set(0);  // first frame is always set. This insures allocs cant be 0
}

void pmm_deinit_region(physical_addr base, uint32_t size) {
  uint32_t align = base / PMM_FRAME_SIZE;
  uint32_t frames = div_ceil(size, PMM_FRAME_SIZE);

  for (uint32_t i = 0; i < frames; ++i) {
    memory_bitmap_set(align + i);
    _used_frames++;
  }
}

void* pmm_alloc_frame() {
  if (pmm_get_free_frame_count() <= 0)
    return 0;  // out of memory

  int32_t frame = memory_bitmap_first_free();

  if (frame == -1)
    return 0;  // out of memory

  memory_bitmap_set(frame);

  physical_addr addr = frame * PMM_FRAME_SIZE;
  _used_frames++;

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

void pmm_mark_used_addr(uint32_t paddr) {
  uint32_t frame = paddr / PMM_FRAME_SIZE;
  if (!memory_bitmap_test(frame)) {
    memory_bitmap_set(frame);
    _used_frames++;
  }
}

physical_addr pmm_get_PDBR() {
  uint32_t cr3;
  asm volatile("mov %%cr3, %0"
               : "=r"(cr3));
  return cr3;
}

void* pmm_alloc_frames(uint32_t size) {
  if (pmm_get_free_frame_count() < size)
    return 0;  // not enough space

  int32_t frame = memory_bitmap_first_free_s(size);

  if (frame == -1)
    return 0;  // not enough space

  for (uint32_t i = 0; i < size; i++)
    memory_bitmap_set(frame + i);

  physical_addr addr = frame * PMM_FRAME_SIZE;
  _used_frames += size;

  return (void*)addr;
}

void pmm_free_frame(void* p) {
  if ((physical_addr)p % PMM_FRAME_SIZE != 0) {
    printf("pmm_dealloc: addr is not %d aligned", PMM_FRAME_SIZE);
    return; // todo: make crash
  }

  physical_addr addr = (physical_addr)p;
  uint32_t frame = addr / PMM_FRAME_SIZE;

  memory_bitmap_unset(frame);

  _used_frames--;
}

void pmm_free_frames(void* p, uint32_t size) {
  for (uint32_t i = 0; i < size; ++i) {
    pmm_free_frame(p + i * PMM_FRAME_SIZE);
  }
}

uint32_t pmm_get_memory_size() {
  return _memory_size;
}

uint32_t pmm_get_frame_count() {
  return _max_frames;
}

uint32_t pmm_get_use_frame_count() {
  return _used_frames;
}

uint32_t pmm_get_free_frame_count() {
  return _max_frames - _used_frames;
}

uint32_t pmm_get_frame_size() {
  return PMM_FRAME_SIZE;
}
