#ifndef MALLOC_H
#define MALLOC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "./kernel_info.h"
#include "./pmm.h"

#include "kernel/util/math.h"

#define KERNEL_HEAP_TOP    0xE0000000
#define KERNEL_HEAP_BOTTOM 0xC8000000 // we can do as well KERNEL_AND_BITMAP_END + 4096
#define USER_HEAP_TOP 0x40000000

struct block_meta
{
	size_t size;
	struct block_meta *next;
	bool free;
	uint32_t magic;
};

void* kmalloc(size_t size);
void* kcalloc(size_t n, size_t size);
void* krealloc(void *ptr, size_t size);
void kfree(void *ptr);
void* kcalloc_aligned(size_t n, size_t size, uint32_t alignment);
void* kmalloc_aligned(size_t size, uint32_t alignment);
void *kalign_heap(size_t size);

#endif