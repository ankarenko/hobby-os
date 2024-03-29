
#include <stdint.h>

#include "kernel/util/math.h"
#include "kernel/util/debug.h"
#include "malloc.h"

#define BLOCK_MAGIC 0x464E
#define BLOCK_ALIGNMENT 4
#define NO_ALIGNMENT -1

extern uint32_t heap_current;
static struct block_meta *_kblocklist = NULL;

void assert_kblock_valid(struct block_meta *block) {
  // NOTE: MQ 2020-06-06 if a block's size > 32 MiB -> might be an corrupted block
  if (block->magic != BLOCK_MAGIC || block->size > 0x2000000) {
    //int i = 1;
    assert_not_reached("malloc: invalid block");
  }
}

struct block_meta *find_free_block(
  struct block_meta **last, 
  size_t size, 
  uint32_t alignment
) {
  struct block_meta *current = (struct block_meta *)_kblocklist;
  while (current && !(current->free && current->size >= size)) {
    assert_kblock_valid(current);
    *last = current;
    current = current->next;
    if (current)
      assert_kblock_valid(current);
  }
  return current;
}

void split_block(struct block_meta *block, size_t size) {
  if (block->size > size + sizeof(struct block_meta)) {
    struct block_meta *splited_block = (struct block_meta *)((char *)block + sizeof(struct block_meta) + size);
    splited_block->free = true;
    splited_block->magic = BLOCK_MAGIC;
    splited_block->size = block->size - size - sizeof(struct block_meta);
    //assert(splited_block->size <= 100000);
    splited_block->next = block->next;

    block->size = size;
    block->next = splited_block;
  }
}

struct block_meta *request_space(struct block_meta *last, size_t size) {
  struct block_meta *block = sbrk(size + sizeof(struct block_meta), NULL);

  if (last)
    last->next = block;
  
  block->size = size;
  block->next = NULL;
  block->free = false;
  block->magic = BLOCK_MAGIC;
  
  assert_kblock_valid(block);
  return block;
}

// NOTE: MQ 2019-11-24
// next object in heap space can be any number
// align heap so the next object's addr will be started at size * n
// ------------------- 0xE0000000
// |                 |
// |                 |
// |                 | new object address m = (size * n)
// ------------------- m - sizeof(struct block_meta)
// |                 | empty object (>= 1)
// ------------------- padding - sizeof(struct block_meta)
void *kalign_heap(size_t size, bool with_meta) {
	uint32_t heap_addr = (uint32_t)sbrk(0, NULL);
  
	if ((heap_addr + (with_meta? sizeof(struct block_meta) : 0)) % size == 0)
		return NULL;

  uint32_t required_size = sizeof(struct block_meta) * (with_meta? 2 : 1);
	uint32_t padding_size = div_ceil(heap_addr, size) * size - heap_addr;

	while (padding_size <= KERNEL_HEAP_TOP)
	{
    //log("padding size: %d", padding_size);
		if (padding_size > required_size)
		{
			struct block_meta *last = _kblocklist;
			while (last->next)
				last = last->next;
			struct block_meta *block = request_space(last, padding_size - required_size);

			return block + 1;
		}
		padding_size += size;
	}

  assert_not_reached("kernel heap cannot be exceeded");
}

struct block_meta *get_block_ptr(void *ptr) {
  return (struct block_meta *)ptr - 1;
}

// todo: probably not the best
void kfree(void *ptr) {
  if (!ptr)
    return;
  
  struct block_meta *block = get_block_ptr(ptr);
  assert_kblock_valid(block);
  block->free = true;
  //log("free: 0x%x", ptr);
}

// TODO: make it more efficient, try to find an appropriate block among
// kblocklist blocks first
void* kmalloc_aligned(size_t size, uint32_t alignment) {
  void* aligned = kalign_heap(alignment, true);

  uint32_t heap_addr = (uint32_t)sbrk(0, NULL);
  assert((heap_addr + sizeof(struct block_meta)) % alignment == 0);
  
  size = ALIGN_UP(size, BLOCK_ALIGNMENT);
  struct block_meta *last = _kblocklist;
  while (last->next)
    last = last->next;

  struct block_meta* block = request_space(last, size);
  
  if (aligned) {
    kfree(aligned);
  }

  return block? block + 1 : NULL;
}

void* kcalloc_aligned(size_t n, size_t size, uint32_t alignment) {
  void *block = kmalloc_aligned(n * size, alignment);
  if (block)
    memset(block, 0, n * size);

  assert((uint32_t)block % alignment == 0);
  assert(block != NULL);
  return block;
}

// malloc for kernel
void *kmalloc(size_t size) {
  if (size >= 0x2000000) {
    err("Allocating too much: %d", size);
  }

  if (size <= 0)
    return NULL;

  struct block_meta *block;

  size = ALIGN_UP(size, BLOCK_ALIGNMENT);

  if (_kblocklist) {
    struct block_meta *last = _kblocklist;
    block = find_free_block(&last, size, NO_ALIGNMENT);
    if (block) {
      block->free = false;
      split_block(block, size);
    } else
      block = request_space(last, size);
  } else {
    block = request_space(NULL, size);
    _kblocklist = block;
  }

  assert_kblock_valid(block);

  //log("alloc: 0x%x", block + 1);
  return block ? block + 1 : NULL;
}

void *kcalloc(size_t n, size_t size) {
  void *block = kmalloc(n * size);
  if (block)
    memset(block, 0, n * size);
  return block;
}

void *krealloc(void *ptr, size_t size)
{
	if (!ptr && size == 0)
	{
		kfree(ptr);
		return NULL;
	}
	else if (!ptr)
		return kcalloc(size, sizeof(char));

	void *newptr = kcalloc(size, sizeof(char));
	memcpy(newptr, ptr, size);
	return newptr;
}