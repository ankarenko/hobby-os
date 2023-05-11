#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#define PAGE_SIZE 0x1000 // 4KB
// Macros used in the bitset algorithms.
#define INDEX_FROM_BIT(a) (a / 32)
#define OFFSET_FROM_BIT(a) (a % 32)

// Static function to set a bit in the frames bitset
void set_frame(uint32_t frame_addr);

// Static function to clear a bit in the frames bitset
void clear_frame(uint32_t frame_addr);

// Static function to test if a bit is set.
uint32_t test_frame(uint32_t frame_addr);

// Static function to find the first free frame.
uint32_t first_frame();

#endif