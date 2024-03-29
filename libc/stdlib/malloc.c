#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include "kernel/util/debug.h"
#include "kernel/util/math.h"

#define BLOCK_MAGIC 0x464E
#define BLOCK_ALIGNMENT 4

struct block_meta
{
	size_t size;
	struct block_meta *next;
	bool free;
	uint32_t magic;
};

uint32_t heap_current;
static struct block_meta *_kblocklist = NULL;

void assert_kblock_valid(struct block_meta *block) {
  // NOTE: MQ 2020-06-06 if a block's size > 32 MiB -> might be an corrupted block
  if (block->magic != BLOCK_MAGIC || block->size > 0x2000000) {
    // todo: kernel PANIC
    // 99% that I accidently overwriten the block
    assert_not_reached("malloc: invalid block, most probably buffer overflow");
  }
}

struct block_meta *find_free_block(struct block_meta **last, size_t size) {
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
    splited_block->next = block->next;

    block->size = size;
    block->next = splited_block;
  }
}

struct block_meta *request_space(struct block_meta *last, size_t size) {
  struct block_meta *block = sbrk(size + sizeof(struct block_meta));

  if (last)
    last->next = block;

  block->size = size;
  block->next = NULL;
  block->free = false;
  block->magic = BLOCK_MAGIC;
  return block;
}

struct block_meta *get_block_ptr(void *ptr) {
  return (struct block_meta *)ptr - 1;
}

// todo: probably not the best
void free(void *ptr) {
  if (!ptr)
    return;
  
  struct block_meta *block = get_block_ptr(ptr);
  assert_kblock_valid(block);
  block->free = true;
}

// malloc for kernel
void *malloc(size_t size) {

  if (size <= 0)
    return NULL;

  struct block_meta *block;

  size = ALIGN_UP(size, BLOCK_ALIGNMENT);
  
  if (_kblocklist) {
    struct block_meta *last = _kblocklist;
    block = find_free_block(&last, size);
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

  return block ? block + 1 : NULL;
}

void *calloc(size_t n, size_t size) {
  void *block = malloc(n * size);
  if (block)
    memset(block, 0, n * size);
  return block;
}

void *realloc(void *ptr, size_t size)
{
	if (!ptr && size == 0)
	{
		free(ptr);
		return NULL;
	}
	else if (!ptr)
		return calloc(size, sizeof(char));

	void *newptr = calloc(size, sizeof(char));
	memcpy(newptr, ptr, size);
	return newptr;
}