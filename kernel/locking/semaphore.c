#include "kernel/cpu/hal.h"
#include "kernel/proc/task.h"
#include "kernel/util/debug.h"
#include "kernel/memory/malloc.h"

#include "kernel/locking/semaphore.h"

struct semaphore {
  uint32_t count;  
  uint32_t capacity;
  struct list_head wait_list;
  // uint32_t lock; for multiprocess
};

struct sem_waiter {
  struct thread* task;
  struct list_head sibling;
};


static inline void sema_init(struct semaphore *sem, int val) {
	*sem = (struct semaphore)__SEMAPHORE_INITIALIZER(*sem, val);
}

// disabling interrupts are sufficient for one core systems
// othewise spin locks needs to be used
void semaphore_down(struct semaphore *sem) {
  lock_scheduler();
  struct thread *cur_thread = get_current_thread();
  atomic_inc(&cur_thread->lock_counter);

  if (sem->count > 0) {
    sem->count -= 1;
    unlock_scheduler();
  } else {
    struct sem_waiter *sw = kcalloc(1, sizeof(struct sem_waiter));
    sw->task = cur_thread;
    list_add_tail(&sw->sibling, &sem->wait_list);
    thread_update(cur_thread, THREAD_WAITING);
    unlock_scheduler();
    schedule();
  }
}

int semaphore_free(struct semaphore *sem) {
  struct sem_waiter *iter = NULL;
  int freed = 0;
  list_for_each_entry(iter, &sem->wait_list, sibling) {
    kfree(iter);
    ++freed;
  }
  kfree((void *)sem);
  return freed;
}

void semaphore_up(struct semaphore *sem) {
  lock_scheduler();
  struct thread *cur_thread = get_current_thread();
  atomic_dec(&cur_thread->lock_counter);

  if (list_empty(&sem->wait_list)) {
    sem->count = min(sem->capacity, sem->count + 1);
    unlock_scheduler();
  } else {
    struct sem_waiter *next = list_first_entry(
      &sem->wait_list, struct sem_waiter, sibling
    );

    list_del(&next->sibling);
    thread_update(next->task, THREAD_READY);
    kfree(next);
    unlock_scheduler();
    //schedule();
  }

}

uint32_t semaphore_get_val(struct semaphore *sem) {
  return sem->count;
}

struct semaphore* semaphore_alloc(int capacity, int init_value) {
  assert(init_value <= capacity);
	struct semaphore *sem = (struct semaphore *)kcalloc(1, sizeof(struct semaphore)); 
  sem->capacity = capacity;
  sem->count = init_value;
  INIT_LIST_HEAD(&sem->wait_list);
  return sem;
}