#include "kernel/ipc/signal.h"

#include "kernel/cpu/gdt.h"
#include "kernel/cpu/idt.h"
#include "kernel/cpu/tss.h"
#include "kernel/include/errno.h"
#include "kernel/memory/vmm.h"
#include "kernel/proc/task.h"
#include "kernel/util/debug.h"
#include "kernel/util/string/string.h"

extern void enter_usermode(
    virtual_addr entry,
    virtual_addr user_esp,
    virtual_addr return_addr  // TODO: now it's ignored
);

struct signal_frame {
  void (*sigreturn)(struct interrupt_registers *);
  int32_t signum;
  // bool signaling;
  sig_t blocked;
  interrupt_registers uregs;
};

void signal_handler(interrupt_registers *regs) {
  struct thread *current_thread = get_current_thread();
  if (!current_thread || !current_thread->pending || current_thread->signaling ||
      ((uint32_t)regs + sizeof(interrupt_registers) != current_thread->kernel_esp))
    return;

  handle_signal(regs, current_thread->blocked);
}

int do_sigaction(int signum, const struct sigaction *action, struct sigaction *old_action) {
  struct process *current_process = get_current_process();

  if (!valid_signal(signum) || signum < 1 || sig_kernel_only(signum))
    return -EINVAL;

  if (old_action)
    memcpy(old_action, &current_process->sighand[signum - 1], sizeof(struct sigaction));

  if (action)
    memcpy(&current_process->sighand[signum - 1], action, sizeof(struct sigaction));

  return 0;
}

int do_sigprocmask(int how, const sig_t *set, sig_t *oldset) {
  struct thread *current_thread = get_current_thread();
  // 4294967295
  if (oldset)
    *oldset = current_thread->blocked;

  if (set) {
    if (how == SIG_BLOCK)
      current_thread->blocked |= *set;
    else if (how == SIG_UNBLOCK)
      current_thread->blocked &= ~*set;
    else if (how == SIG_SETMASK)
      current_thread->blocked = *set;
    else
      return -EINVAL;

    // cannot block SIGKILL and SIGSTOP
    current_thread->blocked &= ~SIG_KERNEL_ONLY_MASK;
  }

  return 0;
}

static int next_signal(sig_t pending, sig_t blocked) {
  sig_t mask = pending & ~blocked;
  uint32_t signum = 0;

  for (int i = 0; i < NSIG; ++i) {
    if ((mask >> i) & sigmask(1)) {
      return i + 1;
    }
  }
  return 0;

  /*
    in case i want to make priorities?
    if (mask & SIG_KERNEL_COREDUMP_MASK)
      signum = ffz(~(mask & SIG_KERNEL_COREDUMP_MASK)) + 1;
    else if (mask & ~sigmask(SIGCONT))
      signum = ffz(~(mask & ~sigmask(SIGCONT))) + 1;
    else if (mask & sigmask(SIGCONT))
      signum = SIGCONT;

    return signum;
  */
}

bool sig_ignored(struct thread *th, int sig) {
  if (sigismember(&th->blocked, sig))
    return false;

  __sighandler_t handler = th->proc->sighand[sig - 1].sa_handler;
  return handler == SIG_IGN || (handler == SIG_DFL && sig_kernel_ignore(sig));
}

void handle_signal(interrupt_registers *regs, sig_t restored_sig) {
  struct thread *current_thread = get_current_thread();
  struct process *current_process = get_current_process();

  int signum;
  if ((signum = next_signal(current_thread->pending, current_thread->blocked)) == 0) {
    log("Signal is blocked for struct thread: %d", signum, current_thread->tid);
    return;
  }

  bool from_syscall = false;
  if (regs->int_no == DISPATCHER_ISR) {
    from_syscall = true;
  }

  assert(!sig_ignored(current_thread, signum));
  if (sig_default_action(current_process, signum)) {
    assert(sig_fatal(current_process, signum));
    current_process->caused_signal = signum;
    current_process->flags |= SIGNAL_TERMINATED;
    current_process->flags &= ~(SIGNAL_CONTINUED | SIGNAL_STOPED);  // ?
    current_thread->signaling = false;
    sigemptyset(&current_thread->pending);
    do_exit(signum);
  } else if (sig_user_defined(current_process, signum)) {
    // TODO: too complicated, can be simplified, simply can edit regs
    struct signal_frame *signal_frame = (char *)regs->useresp - sizeof(struct signal_frame);
    signal_frame->sigreturn = &sigreturn;
    signal_frame->signum = signum;
    signal_frame->blocked = current_thread->blocked;
    signal_frame->uregs = *regs;

    struct sigaction *sigaction = &current_process->sighand[signum - 1];
    current_thread->blocked |= sigmask(signum) | sigaction->sa_mask;

    if (from_syscall) {
      tss_set_stack(KERNEL_DATA, current_thread->kernel_esp);
      // NOTE: we don't need to worry about restoring general purpose registers
      enter_usermode(sigaction->sa_handler, signal_frame, NULL);
    }
  }
}

int do_signal(pid_t pid, int32_t signum) {
  log("Signal: Kill with pid=%d signum=%d", pid, signum);
  if (!valid_signal(signum) || signum < 0)
    return -EINVAL;

  if (!signum)
    return 0;

  struct process* current_process = get_current_process();
  struct thread* current_thread = get_current_thread();

  if (pid > 0) {
    struct process *proc = find_process_by_pid(pid);
    struct thread *th = list_first_entry(&proc->threads, struct thread, child);
    

    if (signum == SIGCONT) {
      current_process->flags |= SIGNAL_CONTINUED;
      current_process->flags &= ~SIGNAL_STOPED;
      sigdelsetmask(&current_thread->pending, SIG_KERNEL_STOP_MASK);

      if (th != current_thread)
        thread_update(th, THREAD_READY);

      do_signal(current_process->parent->pid, SIGCHLD);
      wake_up(&current_process->parent->wait_chld);
    } else if (sig_kernel_stop(signum)) {
      current_process->flags |= SIGNAL_STOPED;
      current_process->flags &= ~SIGNAL_CONTINUED;
      sigdelset(&current_thread->pending, SIGCONT);

      thread_update(th, THREAD_WAITING);

      do_signal(current_process->parent->pid, SIGCHLD);
      wake_up(&current_process->parent->wait_chld);

      if (th == current_thread)
        schedule();
    } else if (!sig_ignored(th, signum)) {
      th->pending |= sigmask(signum);

      if ((signum == SIGKILL || signum == SIGQUIT) && th != current_thread)
        thread_update(th, THREAD_READY);
    }
  } else if (pid == 0) {
    struct process *proc;
    process_for_each_entry(proc) {
      if (proc->gid == current_process->gid)
        do_signal(proc->gid, signum);
    }
  } else if (pid == -1) {
    struct process *proc;
    process_for_each_entry(proc) {
      // TODO: MQ 2020-08-20 Make sure calling process has permission to send signals
      if (proc->pid > 1)
        do_signal(proc->gid, signum);
    }
  } else {
    struct process *proc;
    
    process_for_each_entry(proc) {
      if (proc->gid == -pid)
        do_signal(proc->gid, signum);
    }
  }

  return 0;
}

void sigreturn(interrupt_registers *regs) {
  // NOTE: MQ 2020-08-26
  /*
  |_________________________| <- original esp
  | struct thread->uregs           |
  |-------------------------|
  | struct thread->blocked         | - is important in case of a signal being invoked while invoking invoking another signal
  |-------------------------|
  | signum                  |
  |-------------------------| <- after exiting from user defined signal handler
  | sigreturn               |
  |-------------------------| <- trap frame esp
  */

  struct process *current_process = get_current_process();
  struct thread *current_thread = get_current_thread();
  log("Signal: Return from signal handler %s(p%d)", current_process->name, current_process->pid);

  struct signal_frame *signal_frame = (char *)regs->useresp - 4;
  if (signal_frame->uregs.int_no == DISPATCHER_ISR) {
    // 100% this code was invoked by page_fault
    signal_frame->uregs.int_no = 14;
  }

  int signum = signal_frame->signum;
  struct sigaction *sigaction = &current_process->sighand[signum - 1];
  current_thread->blocked = signal_frame->blocked;
  current_thread->pending &= ~sigmask(signum);  // unmask signal

  memcpy(
      (char *)current_thread->kernel_esp - sizeof(interrupt_registers),
      &signal_frame->uregs,
      sizeof(interrupt_registers));
}