#ifndef KERNEL_PROC_WAIT_H
#define KERNEL_PROC_WAIT_H

#include "kernel/util/debug.h"
#include "kernel/util/list.h"

struct thread;
void thread_wake(struct thread *th);
void thread_wait(struct thread *th);

typedef void (*wait_queue_callback)(struct thread *);

struct wait_queue_head {
	struct list_head list;
};

struct wait_queue_entry {
	struct thread *th;
	wait_queue_callback callback;
	struct list_head sibling;
};

extern struct thread *_current_thread;
extern void schedule();

#define DEFINE_WAIT(name)               \
	struct wait_queue_entry name = {      \
		.th = _current_thread,          \
		.callback = thread_wake,            \
	}
  
#define wait_until(cond) ({                           \
  for (; !(cond);) {   \
    thread_wait(_current_thread);    \
		schedule();                                       \
	}                                                   \
})

#define wait_event(wh, cond) ({   \
  DEFINE_WAIT(__wait);                            \
  list_add_tail(&__wait.sibling, &(wh)->list);    \
  wait_until(cond);                               \
  list_del(&__wait.sibling);                      \  
})                                                 \

#define wait_until_wakeup(wh) ({   \
  DEFINE_WAIT(__wait);                            \
  list_add_tail(&__wait.sibling, &(wh)->list);    \
  thread_wait(_current_thread);    \
	schedule();                               \
  list_del(&__wait.sibling);                      \  
})

void wake_up(struct wait_queue_head *hq);

#endif