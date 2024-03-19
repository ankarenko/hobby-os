#include <stdint.h>

#include "kernel/cpu/gdt.h"
#include "kernel/cpu/hal.h"
#include "kernel/cpu/tss.h"
#include "kernel/ipc/signal.h"
#include "kernel/proc/task.h"
#include "kernel/system/timer.h"
#include "kernel/util/debug.h"
#include "kernel/include/list.h"
#include "kernel/util/math.h"

struct list_head process_list;

extern uint32_t DEBUG_LAST_TID;

struct list_head ready_threads;
//struct list_head waiting_threads;
//struct list_head terminated_threads;

// TODO: put it in th kernel stack, it's is more efficient
struct thread* _current_thread = NULL;

extern void switch_to_thread(struct thread* next_task);
extern void scheduler_tick();

static int32_t scheduler_lock_counter = 0;

void lock_scheduler() {
  disable_interrupts();
  scheduler_lock_counter++;
}

void unlock_scheduler() {
  assert(scheduler_lock_counter > 0, "scheduler lock cannt be < 0");

  scheduler_lock_counter--;
  if (scheduler_lock_counter == 0)
    enable_interrupts();
}

static struct list_head* sched_get_list(enum thread_state state) {
  return &ready_threads;
}

static struct thread* get_next_thread_from_list(enum thread_state state) {
  struct list_head* list = sched_get_list(state);

  if (list_empty(list))
    return NULL;

  return list_entry(list->next, struct thread, sched_sibling);
}

void sched_push_queue(struct thread* th) {
  struct list_head* h = sched_get_list(th->state);

  if (h)
    list_add_tail(&th->sched_sibling, h);
}

void sched_remove_queue(struct thread* th) {
  struct list_head* h = &ready_threads;

  if (h)
    list_del(&th->sched_sibling);
}

static struct thread* get_next_thread_to_run() {
  return get_next_thread_from_list(THREAD_READY);
}

static struct thread* pop_next_thread_from_list(enum thread_state state) {
  struct list_head* list = sched_get_list(state);

  if (list_empty(list))
    return NULL;

  struct thread* th = list_entry(list->next, struct thread, sched_sibling);
  list_del(&th->sched_sibling);
  return th;
}

static struct thread* pop_next_thread_to_run() {
  return pop_next_thread_from_list(THREAD_READY);
}

struct thread* pop_next_thread_to_terminate() {
  return pop_next_thread_from_list(THREAD_TERMINATED);
}

struct list_head* get_ready_threads() {
  return &ready_threads;
}

/*
struct list_head* get_waiting_threads() {
  return &waiting_threads;
}
*/

void scheduler_tick() {
  make_schedule();
}

void thread_remove_state(struct thread* t, enum thread_state state) {
  t->state &= ~state;
}

/* schedule new task to run. */
void schedule() {
  __asm volatile("int $32");
}

void thread_set_state(struct thread* t, enum thread_state state) {
  t->state = state;
}

void thread_remove(struct thread* t) {
  lock_scheduler();

  sched_remove_queue(t);

  unlock_scheduler();
}

void thread_update(struct thread* t, enum thread_state state) {
  if (t->state == state)
    return;

  lock_scheduler();

  sched_remove_queue(t);
  t->state = state;
  sched_push_queue(t);

  unlock_scheduler();
}

void thread_sleep(uint32_t ms) {
  uint32_t exp = get_seconds(NULL) * 1000 + ms;
  mod_timer(&_current_thread->s_timer, exp);
  thread_update(_current_thread, THREAD_WAITING);
  schedule();
}

void thread_wake(struct thread* th) {
  thread_update(th, THREAD_READY);
}

void thread_wait(struct thread* th) {
  thread_update(th, THREAD_WAITING);
}

bool thread_signal(uint32_t tid, int32_t signum) {
  struct thread* th = NULL;
  list_for_each_entry(th, sched_get_list(THREAD_READY), sched_sibling) {
    if (tid == th->tid) {
      th->pending |= sigmask(signum);
      return true;
    }
  }

  list_for_each_entry(th, sched_get_list(THREAD_WAITING), sched_sibling) {
    if (tid == th->tid) {
      th->pending |= sigmask(signum);
      return true;
    }
  }

  return false;
}

/*
bool thread_kill(uint32_t tid) {
  struct thread* th = NULL;
  list_for_each_entry(th, sched_get_list(THREAD_READY), sched_sibling) {
    if (tid == th->tid) {
      thread_update(th, THREAD_TERMINATED);
      return true;
    }
  }

  list_for_each_entry(th, sched_get_list(THREAD_WAITING), sched_sibling) {
    if (tid == th->tid) {
      thread_update(th, THREAD_TERMINATED);
      return true;
    }
  }

  return false;
}
*/

extern uint32_t address_end_of_switch_to_task = 0;

void thread_mark_dead(struct thread* th) {
  th->dead_mark = true;
}

void make_schedule() {
next_thread:
  struct thread* th = pop_next_thread_to_run();
  
  if (th->state != THREAD_READY)
    goto next_thread;

  sched_push_queue(th);

do_switch:

  // INFO: SA switch to trhead invokes tss_set_stack implicitly
  // tss_set_stack(KERNEL_DATA, th->kernel_esp);
  // log("sched: tid: %d, name: %s", th->tid, th->proc->name);
  switch_to_thread(th);
  
  // if thread has acuired some resources, it needs to release it first
  if (siginmask(_current_thread->pending, SIG_KERNEL_ONLY_MASK)) {
    if (atomic_read(&_current_thread->lock_counter) > 0)
      _current_thread->blocked |= SIG_KERNEL_ONLY_MASK;
    else
      _current_thread->blocked &= ~SIG_KERNEL_ONLY_MASK;
  }

  if (_current_thread->pending /*&& !(_current_thread->flags & TIF_SIGNAL_MANUAL)*/) {
    //_current_thread->handles_signal = true;
    // allocate space for signal handler trap
    // kernel_esp always points to the start of kernel stack for a struct thread
    interrupt_registers* regs = (interrupt_registers*)(_current_thread->kernel_esp - sizeof(interrupt_registers));
    handle_signal(regs, _current_thread->blocked);
  }
}

void sched_init() {
  //INIT_LIST_HEAD(&waiting_threads);
  INIT_LIST_HEAD(&ready_threads);
  //INIT_LIST_HEAD(&terminated_threads);
  INIT_LIST_HEAD(&process_list);
}
