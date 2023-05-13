#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stdbool.h>

//! 8 blocks per byte
#define PMM_BLOCKS_PER_BYTE 8

//! block size (4k)
#define PMM_BLOCK_SIZE 0x1000

//! block alignment
#define PMM_BLOCK_ALIGN PMM_BLOCK_SIZE

//! physical address
typedef uint32_t physical_addr;

void pmm_init(uint32_t memSize, physical_addr bitmap);
void pmm_init_region(physical_addr base, uint32_t size);
void pmm_deinit_region(physical_addr base, uint32_t size);
void* pmm_alloc_block();
void* pmm_alloc_blocks(uint32_t size);
void pmm_paging_enable(bool b);
bool pmm_is_paging();
void pmm_load_PDBR(physical_addr addr);
physical_addr pmm_get_PDBR();

uint32_t pmm_get_memory_size();
uint32_t pmm_get_block_count();
uint32_t pmm_get_use_block_count();
uint32_t pmm_get_free_block_count();
uint32_t pmm_get_block_size();
void pmm_free_block(void* p);

#endif