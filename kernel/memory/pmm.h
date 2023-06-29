#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "../multiboot.h"

#define PMM_DEBUG() printf("Available: \n Blocks: %d \n Bytes: %d \n Used: \n Blocks: %d \n Bytes: %d \n", pmm_get_free_frame_count(), pmm_get_free_frame_count() * pmm_get_frame_size(), pmm_get_use_frame_count(), pmm_get_use_frame_count() * pmm_get_frame_size());
  
  

//! 8 blocks per byte
#define PMM_FRAMES_PER_BYTE 8
#define PMM_FRAMES_PER_4BYTES 32
//! block size (4k)
#define PMM_FRAME_SIZE 0x1000 // 4096
#define PMM_FRAME_ALIGN PMM_FRAME_SIZE
#define PAGE_MASK (~(PMM_FRAME_SIZE - 1))
#define PAGE_ALIGN(addr) (((addr) + PMM_FRAME_SIZE - 1) & PAGE_MASK)
// it is 32bit system, so the maximum MEMORY_BITMAP size is 128Kbyte
// for simplicity we reserve it statically 
#define MAX_MEMORY_BITMAP_BYTES 0x400 * 128 // 1024 * 128

//! physical address
typedef uint32_t physical_addr;

void pmm_init(multiboot_info_t* mbd);
void pmm_init_region(physical_addr base, uint32_t size);
void pmm_deinit_region(physical_addr base, uint32_t size);
void* pmm_alloc_frame();
void* pmm_alloc_frames(uint32_t size);
void pmm_paging_enable(bool b);
bool pmm_is_paging();
void pmm_load_PDBR(physical_addr addr);
void pmm_mark_used_addr(uint32_t paddr);
void pmm_free_frame(void* p);
void pmm_free_frames(void* p, uint32_t size);

physical_addr pmm_get_PDBR();
uint32_t pmm_get_memory_size();
uint32_t pmm_get_frame_count();
uint32_t pmm_get_use_frame_count();
uint32_t pmm_get_free_frame_count();
uint32_t pmm_get_frame_size();

#endif