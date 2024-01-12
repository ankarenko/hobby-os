#ifndef LOCKING_SEMAPHORE_H
#define LOCKING_SEMAPHORE_H

#include "kernel/util/list.h"

struct semaphore {
  uint32_t count;  
  uint32_t capacity;
  struct list_head wait_list;
  // uint32_t lock; for multiprocess
};

#define __SEMAPHORE_INITIALIZER(name, n)         \
{                                                \
  .count = n,                                    \
  .capacity = n,                                 \
  .wait_list = LIST_HEAD_INIT((name).wait_list), \
}

#define DEFINE_SEMAPHORE(name) \
	struct semaphore name = __SEMAPHORE_INITIALIZER(name, 1)

void semaphore_up(struct semaphore *sem);
void semaphore_down(struct semaphore *sem);

#endif