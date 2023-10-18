#include <stdint.h>
#include "kernel/util/list.h"
#include "kernel/cpu/hal.h"
#include "kernel/proc/task.h"

struct list_head process_list;

extern uint32_t DEBUG_LAST_TID;

struct list_head ready_threads;
struct list_head waiting_threads;
struct list_head terminated_threads;

// TODO: put it in th kernel stack, it's is more efficient
thread* _current_thread;

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

void sched_push_queue(thread *th, enum thread_state state) {
  struct list_head *h = sched_get_list(state);

	if (h)
		list_add_tail(&th->sched_sibling, h);
}

void sched_remove_queue(thread* th, enum thread_state state) {
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

void thread_sleep(uint32_t ms) {
	thread_set_state(_current_thread, THREAD_WAITING);
	_current_thread->sleep_time_delta = ms;
	schedule();
}

void thread_wake() {
  thread_set_state(_current_thread, THREAD_READY);
	//thread_remove_state(_current_thread, THREAD_WAITING);
	_current_thread->sleep_time_delta = 0;
}

bool thread_kill(uint32_t tid) {
  thread* th = NULL;
  list_for_each_entry(th, sched_get_list(THREAD_READY), sched_sibling) {
    if (tid == th->tid) {
      thread_set_state(th, THREAD_TERMINATED);
      return true;
    }
  }
  
  return false;
}

void make_schedule() {
next_thread:
  thread* th = pop_next_thread_to_run();

  if (th->state == THREAD_TERMINATED) {
    // put it in a queue for a worker thread to purge
    sched_push_queue(th, THREAD_TERMINATED);
    goto next_thread;
  }

  sched_push_queue(th, THREAD_READY);
  
	if (th->state == THREAD_WAITING) {
		if (th->sleep_time_delta > 0)
			th->sleep_time_delta--;

		if (th->sleep_time_delta == 0) {
			thread_wake();
			goto do_switch;
		}

		goto next_thread;
	}
do_switch:
  DEBUG_LAST_TID = th->tid;
  if (DEBUG_LAST_TID == 6) {
    uint32_t a = 1;
  } 
  switch_to_thread(th);
}

void sched_init()
{
  INIT_LIST_HEAD(&waiting_threads);
  INIT_LIST_HEAD(&ready_threads);
  INIT_LIST_HEAD(&terminated_threads);
  INIT_LIST_HEAD(&process_list);
}
