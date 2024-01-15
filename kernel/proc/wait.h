#ifndef KERNEL_PROC_WAIT_H
#define KERNEL_PROC_WAIT_H

#include "kernel/proc/task.h"
#include "kernel/util/debug.h"
#include "kernel/util/list.h"

typedef void (*wait_queue_callback)(thread *);

struct wait_queue_head {
	struct list_head list;
};

struct wait_queue_entry {
	thread *thread;
	wait_queue_callback callback;
	struct list_head sibling;
};

extern volatile thread *_current_thread;
extern void schedule();

#define DEFINE_WAIT(name)               \
	struct wait_queue_entry name = {      \
		.thread = _current_thread,          \
		.callback = thread_wake,            \
	}
  
#define wait_until(cond) ({                           \
  for (; !(cond);) {                                  \
		thread_update(_current_thread, THREAD_WAITING);    \
		schedule();                                       \
	}                                                   \
})

#define wait_event(wh, cond) ({                   \
  DEFINE_WAIT(__wait);                            \
  list_add_tail(&__wait.sibling, &(wh)->list);    \
  wait_until(cond);                               \
  list_del(&__wait.sibling);                      \  
})                                                 \


void wake_up(struct wait_queue_head *hq) {
	struct wait_queue_entry *iter, *next;
	list_for_each_entry_safe(iter, next, &hq->list, sibling) {
		iter->callback(iter->thread);
	}
}

#endif