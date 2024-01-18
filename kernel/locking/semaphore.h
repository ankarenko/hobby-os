#ifndef LOCKING_SEMAPHORE_H
#define LOCKING_SEMAPHORE_H

#include "kernel/util/list.h"

struct semaphore {
  uint32_t count;  
  uint32_t capacity;
  struct list_head wait_list;
  // uint32_t lock; for multiprocess
};

#define __SEMAPHORE_INITIALIZER(name, n) {       \
  .count = n,                                    \
  .capacity = n,                                 \
  .wait_list = LIST_HEAD_INIT((name).wait_list), \
}

#define INIT_SEMAPHORE(name, n) \
	struct semaphore name = __SEMAPHORE_INITIALIZER(name, n)

#define INIT_MUTEX(name) \
	struct semaphore name = __SEMAPHORE_INITIALIZER(name, 1)

static inline void sema_init(struct semaphore *sem, int val) {
	*sem = (struct semaphore)__SEMAPHORE_INITIALIZER(*sem, val);
}

struct semaphore* semaphore_alloc(int val);

void semaphore_up(struct semaphore *sem);
int semaphore_free(struct semaphore *sem);
void semaphore_down(struct semaphore *sem);
void semaphore_up_val(struct semaphore *sem, int val);
void semaphore_down_val(struct semaphore *sem, int val);

#endif