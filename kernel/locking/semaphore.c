#include "kernel/cpu/hal.h"
#include "kernel/proc/task.h"
#include "kernel/util/debug.h"
#include "kernel/memory/malloc.h"

#include "kernel/locking/semaphore.h"

struct sem_waiter {
  struct thread* task;
  struct list_head sibling;
};

// disabling interrupts are sufficient for one core systems
// othewise spin locks needs to be used
void semaphore_down_val(struct semaphore *sem, int val) {
  disable_interrupts();
  struct thread *cur_thread = get_current_thread();

  if (sem->count >= val) {
    sem->count -= val;
    enable_interrupts();
  } else {
    struct sem_waiter *sw = kcalloc(1, sizeof(struct sem_waiter));
    sw->task = cur_thread;
    list_add_tail(&sw->sibling, &sem->wait_list);
    thread_update(cur_thread, THREAD_WAITING);
    enable_interrupts();  // make_schedule instead??
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

void semaphore_up_val(struct semaphore *sem, int val) {
  if (val == 0)
    return;

  disable_interrupts();
  struct thread *cur_thread = get_current_thread();

  if (list_empty(&sem->wait_list)) {
    sem->count = min(sem->capacity, sem->count + val);
  } else {
    struct sem_waiter *next = list_first_entry(
      &sem->wait_list, struct sem_waiter, sibling
    );
    list_del(&next->sibling);
    thread_update(next->task, THREAD_READY);
    kfree(next);
    schedule();
  }

  enable_interrupts();
}

void semaphore_down(struct semaphore *sem) {
  return semaphore_down_val(sem, 1);
}

struct semaphore* semaphore_alloc(int val) {
	struct semaphore *sem = (struct semaphore *)kcalloc(1, sizeof(struct semaphore)); 
  sem->capacity = val;
  sem->count = val;
  INIT_LIST_HEAD(&sem->wait_list);
  return sem;
}

void semaphore_up(struct semaphore *sem) {
  return semaphore_up_val(sem, 1);
}
