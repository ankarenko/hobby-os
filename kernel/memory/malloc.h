#ifndef MALLOC_H
#define MALLOC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define KERNEL_HEAP_TOP 0xF0000000
#define KERNEL_HEAP_BOTTOM 0xD0000000
#define USER_HEAP_TOP 0x40000000

struct block_meta
{
	size_t size;
	struct block_meta *next;
	bool free;
	uint32_t magic;
};

void *kmalloc(size_t size);
void *kcalloc(size_t n, size_t size);
void kfree(void *ptr);

#endif