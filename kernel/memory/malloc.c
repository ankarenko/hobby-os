#include "malloc.h"

#include <math.h>
#include <stdint.h>

#include "sbrk.h"

#define BLOCK_MAGIC 0x464E

extern uint32_t heap_current;
static struct block_meta *_kblocklist = NULL;

void assert_kblock_valid(struct block_meta *block) {
  // NOTE: MQ 2020-06-06 if a block's size > 32 MiB -> might be an corrupted block
  if (block->magic != BLOCK_MAGIC || block->size > 0x2000000) {
    // todo: kernel panic
    // assert_not_reached();
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

void kfree(void *ptr) {
  if (!ptr)
    return;

  struct block_meta *block = get_block_ptr(ptr);
  assert_kblock_valid(block);
  block->free = true;
}

void *kmalloc(size_t size) {
  if (size <= 0)
    return NULL;

  struct block_meta *block;

  size = ALIGN_UP(size, 4);

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

void *kcalloc(size_t n, size_t size) {
  void *block = kmalloc(n * size);
  if (block)
    memset(block, 0, n * size);
  return block;
}