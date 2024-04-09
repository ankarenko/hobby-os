#ifndef KERNEL_PROC_WAIT_H
#define KERNEL_PROC_WAIT_H

#include "kernel/util/debug.h"
#include "kernel/include/list.h"
#include "kernel/include/types.h"


#define WNOHANG 0x00000001
#define WUNTRACED 0x00000002
#define WSTOPPED WUNTRACED
#define WEXITED 0x00000004
#define WCONTINUED 0x00000008
#define WNOWAIT 0x01000000 /* Don't reap, just poll status.  */

#define CLD_EXITED 1	/* child has exited */
#define CLD_KILLED 2	/* child was killed */
#define CLD_DUMPED 3	/* child terminated abnormally */
#define CLD_TRAPPED 4	/* traced child has trapped */
#define CLD_STOPPED 5	/* child has stopped */
#define CLD_CONTINUED 6 /* stopped child has continued */

#define P_ALL 0
#define P_PID 1
#define P_PGID 2


/* A status looks like:
      <1 byte info> <1 byte code>

      <code> == 0, child has exited, info is the exit value
      <code> == 1..7e, child has exited, info is the signal number.
      <code> == 7f, child has stopped, info was the signal number.
      <code> == 80, there was a core dump.
*/
   
   
#define WSEXITED 0x0000 // TODO: was 0x0001 which is stange
#define WSSIGNALED 0x0002
#define WSCOREDUMP 0x0004
#define WSSTOPPED 0x0008
#define WSCONTINUED 0x0010

struct thread;
void thread_wake(struct thread *th);
void thread_wait(struct thread *th);

typedef void (*wait_queue_callback)(struct thread *);

struct infop {
	pid_t si_pid;
	int32_t si_signo;
	int32_t si_status;
	int32_t si_code;
};

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