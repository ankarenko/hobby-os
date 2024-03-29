#ifndef LOCKING_SEMAPHORE_H
#define LOCKING_SEMAPHORE_H

#include "kernel/include/list.h"

struct semaphore; // opaque

#define __SEMAPHORE_INITIALIZER(name, n) {       \
  .count = n,                                    \
  .capacity = n,                                 \
  .wait_list = LIST_HEAD_INIT((name).wait_list), \
}

#define INIT_SEMAPHORE(name, n) \
	struct semaphore name = __SEMAPHORE_INITIALIZER(name, n)

#define INIT_MUTEX(name) \
	struct semaphore name = __SEMAPHORE_INITIALIZER(name, 1)

struct semaphore* semaphore_alloc(int capacity, int init_value);

void semaphore_up(struct semaphore *sem);
int semaphore_free(struct semaphore *sem);
void semaphore_down(struct semaphore *sem);
uint32_t semaphore_get_val(struct semaphore *sem);

#endif