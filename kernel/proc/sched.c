#include <stdint.h>

#include "kernel/util/list.h"
#include "kernel/cpu/hal.h"
#include "kernel/cpu/tss.h"
#include "kernel/cpu/gdt.h"
#include "kernel/ipc/signal.h"
#include "kernel/system/timer.h"
#include "kernel/proc/task.h"
#include "kernel/util/debug.h"

struct list_head process_list;

extern uint32_t DEBUG_LAST_TID;

struct list_head ready_threads;
struct list_head waiting_threads;
struct list_head terminated_threads;

// TODO: put it in th kernel stack, it's is more efficient
thread* _current_thread = NULL;

extern void switch_to_thread(thread* next_task);
extern void scheduler_tick();

static volatile uint32_t scheduler_lock_counter = 0;

void lock_scheduler() {
	disable_interrupts();
	scheduler_lock_counter++;
}

void unlock_scheduler() {
	scheduler_lock_counter--;
	if (scheduler_lock_counter == 0)
		enable_interrupts();
}

static struct list_head* sched_get_list(enum thread_state state) {
  switch (state) {
    case THREAD_TERMINATED:
      return &terminated_threads;
    case THREAD_READY:
      return &ready_threads;
    case THREAD_WAITING:
      return &waiting_threads;
    default:
      return NULL;
  }
}

static struct thread* get_next_thread_from_list(enum thread_state state) {
  struct list_head* list = sched_get_list(state);

	if (list_empty(list))
		return NULL;

	return list_entry(list->next, thread, sched_sibling);
}

void sched_push_queue(thread *th) {
  struct list_head *h = sched_get_list(th->state);

	if (h)
		list_add_tail(&th->sched_sibling, h);
}

void sched_remove_queue(thread* th) {
  struct list_head *h = &ready_threads;

	if (h)
		list_del(&th->sched_sibling);
}

static struct thread *get_next_thread_to_run() {
	return get_next_thread_from_list(THREAD_READY);
}

static struct thread *pop_next_thread_from_list(enum thread_state state) {
  struct list_head* list = sched_get_list(state);

	if (list_empty(list))
		return NULL;

	thread* th = list_entry(list->next, thread, sched_sibling);
	list_del(&th->sched_sibling);
	return th;
}

static thread *pop_next_thread_to_run() {
	return pop_next_thread_from_list(THREAD_READY);
}

thread *pop_next_thread_to_terminate() {
  return pop_next_thread_from_list(THREAD_TERMINATED);
}

struct list_head* get_ready_threads() {
  return &ready_threads;
}

struct list_head* get_waiting_threads() {
  return &waiting_threads;
}

void scheduler_tick() {
	make_schedule();
}

void thread_remove_state(thread* t, enum thread_state state) {
	t->state &= ~state;
}

/* schedule new task to run. */
void schedule() {
	__asm volatile("int $32");
}

void thread_set_state(thread* t, enum thread_state state) {
	t->state = state;
}

void thread_update(thread *t, enum thread_state state) {
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

void thread_wake(thread *th) {
  thread_update(th, THREAD_READY);
}

bool thread_signal(uint32_t tid, int32_t signum) {
  thread* th = NULL;
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

bool thread_kill(uint32_t tid) {
  thread* th = NULL;
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

extern uint32_t address_end_of_switch_to_task = 0;

void make_schedule() {
next_thread:
  //log("Interrupt\n");
  
  thread* th = pop_next_thread_to_run();
  if (th->tid == 8 || th->tid == 6) {
    int bn = 0;
  }

  if (th->state == THREAD_TERMINATED) {
    // put it in a queue for a worker thread to purge
    sched_push_queue(th);
    goto next_thread;
  }

  sched_push_queue(th);

do_switch:
  // INFO: SA switch to trhead invokes tss_set_stack implicitly
  // tss_set_stack(KERNEL_DATA, th->kernel_esp);
  switch_to_thread(th);

  if (_current_thread->pending /*&& !(_current_thread->flags & TIF_SIGNAL_MANUAL)*/) {
    //_current_thread->handles_signal = true;
    // allocate space for signal handler trap  
    // kernel_esp always points to the start of kernel stack for a thread
		interrupt_registers *regs = (interrupt_registers *)(_current_thread->kernel_esp - sizeof(interrupt_registers));
		handle_signal(regs, _current_thread->blocked);
	}
}

void sched_init() {
  INIT_LIST_HEAD(&waiting_threads);
  INIT_LIST_HEAD(&ready_threads);
  INIT_LIST_HEAD(&terminated_threads);
  INIT_LIST_HEAD(&process_list);
}
