#include <stdint.h>
#include "../include/list.h"
#include "../cpu/hal.h"
#include "./task.h"

struct list_head process_list;

struct list_head ready_threads;
struct list_head waiting_threads;
struct list_head terminated_threads;

// TODO: put it in th kernel stack, it's is more efficient
thread* _current_task;

extern void switch_to_task(thread* next_task);
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

static struct list_head* get_list_from_state(enum thread_state state) {
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
  struct list_head* list = get_list_from_state(state);

	if (list_empty(list))
		return NULL;

	return list_entry(list->next, thread, sched_sibling);
}

void sched_push_queue(thread *th, enum thread_state state) {
  struct list_head *h = get_list_from_state(state);

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
  struct list_head* list = get_list_from_state(state);

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
	thread_set_state(_current_task, THREAD_WAITING);
	_current_task->sleep_time_delta = ms;
	schedule();
}

void thread_wake() {
  thread_set_state(_current_task, THREAD_READY);
	//thread_remove_state(_current_task, THREAD_WAITING);
	_current_task->sleep_time_delta = 0;
}

bool thread_kill(uint32_t id) {
  thread* th = NULL;
  list_for_each_entry(th, get_list_from_state(THREAD_READY), sched_sibling) {
    if (id == th->tid) {
      thread_set_state(th, THREAD_TERMINATED);
      return true;
    }
  }
  
  return false;
}

void make_schedule() {
next_thread:
  uint32_t a = list_count(get_list_from_state(THREAD_READY));
  thread* th = pop_next_thread_to_run();
  a = list_count(get_list_from_state(THREAD_READY));

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
  switch_to_task(th);
}

void sched_init()
{
  INIT_LIST_HEAD(&waiting_threads);
  INIT_LIST_HEAD(&ready_threads);
  INIT_LIST_HEAD(&terminated_threads);
  INIT_LIST_HEAD(&process_list);
}
