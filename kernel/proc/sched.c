#include <stdint.h>
#include "../include/list.h"
#include "../cpu/hal.h"
#include "./task.h"

struct list_head process_list;
struct list_head thread_list;
thread*  _current_task;

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

static struct thread* get_next_thread_from_list(struct list_head* list) {
	if (list_empty(list))
		return NULL;

	return list_entry(list->next, thread, sched_sibling);
}

void queue_thread(thread *th) {
	struct list_head *h = get_thread_list();

	if (h)
		list_add_tail(&th->sched_sibling, h);
}

static void remove_thread(thread *th) {
	struct list_head *h = &thread_list;

	if (h)
		list_del(&th->sched_sibling);
}

static struct thread *get_next_thread_to_run() {
	struct thread *nt = get_next_thread_from_list(&thread_list);
	return nt;
}

static struct thread *pop_next_thread_from_list(struct list_head *list) {
	if (list_empty(list))
		return NULL;

	thread* th = list_entry(list->next, thread, sched_sibling);
	list_del(&th->sched_sibling);
	return th;
}

thread *pop_next_thread_to_run() {
	struct thread *nt = pop_next_thread_from_list(&thread_list);
	return nt;
}

struct list_head* get_thread_list() {
  return &thread_list;
}

/* gets called for each clock tick. */
void scheduler_tick() {
	make_schedule();
}

/* remove thread state flags. */
void thread_remove_state(thread* t, uint32_t flags) {
	/* remove flags. */
	t->state &= ~flags;
}

/* schedule new task to run. */
void schedule() {

	/* force a task switch. */
	__asm volatile("int $32");
}

/* set thread state flags. */
void thread_set_state(thread* t, uint32_t flags) {

	/* set flags. */
	t->state |= flags;
}
/* PUBLIC definition. */
void thread_sleep(uint32_t ms) {

	/* go to sleep. */
	thread_set_state(_current_task, THREAD_BLOCK_SLEEP);
	_current_task->sleep_time_delta = ms;
	schedule();
}

/* PUBLIC definition. */
void thread_wake() {

	/* wake up. */
	thread_remove_state(_current_task, THREAD_BLOCK_SLEEP);
	_current_task->sleep_time_delta = 0;
}


void make_schedule() {
next_thread:
  thread* th = pop_next_thread_to_run();
  queue_thread(th);
  /*
  if (th->parent == ORPHAN_THREAD) {
    goto next_thread;
  }
  */
	
  /* make sure this thread is not blocked. */
	if (th->state & THREAD_BLOCK_STATE) {

		/* adjust time delta. */
		if (th->sleep_time_delta > 0)
			th->sleep_time_delta--;

		/* should we wake thread? */
		if (th->sleep_time_delta == 0) {
			thread_wake();
			goto exec_thread;
		}

		/* not yet, go to next thread. */
		goto next_thread;
	}
exec_thread:
  switch_to_task(th);
}

void sched_init()
{
	INIT_LIST_HEAD(&process_list);
  INIT_LIST_HEAD(&thread_list);
}
