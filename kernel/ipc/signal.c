#include "kernel/memory/vmm.h"
#include "kernel/cpu/idt.h"
#include "kernel/proc/task.h"
#include "kernel/util/debug.h"
#include "kernel/cpu/tss.h"
#include "kernel/util/string/string.h"
#include "kernel/cpu/gdt.h"
#include "kernel/util/errno.h"

#include "kernel/ipc/signal.h"

extern void enter_usermode(
  virtual_addr entry,
  virtual_addr user_esp, 
  virtual_addr return_addr // TODO: now it's ignored
);

struct signal_frame {
	void (*sigreturn)(struct interrupt_registers *);
	int32_t signum;
	bool signaling;
	sigset_t blocked;
	interrupt_registers uregs;
};

void signal_handler(interrupt_registers *regs) {
  thread* current_thread = get_current_thread();
	if (!current_thread || !current_thread->pending || current_thread->signaling ||
		((uint32_t)regs + sizeof(interrupt_registers) != current_thread->kernel_esp))
		return;

	handle_signal(regs, current_thread->blocked);
}

int do_sigaction(int signum, const struct sigaction *action, struct sigaction *old_action) {
	process* current_process = get_current_process();
  
  if (!valid_signal(signum) || signum < 1 || sig_kernel_only(signum))
		return -EINVAL;

	if (old_action)
		memcpy(old_action, &current_process->sighand[signum - 1], sizeof(struct sigaction));

	if (action)
		memcpy(&current_process->sighand[signum - 1], action, sizeof(struct sigaction));

	return 0;
}

void handle_signal(interrupt_registers *regs, sigset_t restored_sig) {
  thread* current_thread = get_current_thread();
  process* current_process = get_current_process();
  
  int signum = SIGTRAP;
  current_thread->pending &= ~sigmask(signum); // unmask signal
  
  bool from_syscall = false;
  if (regs->int_no == DISPATCHER_ISR) {
		from_syscall = true;
	}

  if (siginmask(signum, current_thread->blocked)) {
    log("Signal %d is blocked for thread: %d", signum, current_thread->tid);
    return;
  }

  struct signal_frame *signal_frame = (char *)regs->useresp - sizeof(struct signal_frame);
  signal_frame->sigreturn = &sigreturn;
  signal_frame->signum = signum;
  //frame->signaling = prev_signaling;
  //frame->blocked = restored_sig;
  signal_frame->uregs = *regs;

  struct sigaction *sigaction = &current_process->sighand[signum - 1];
  current_thread->blocked |= sigmask(signum) | sigaction->sa_mask;
  
  if (from_syscall) {
    tss_set_stack(KERNEL_DATA, current_thread->kernel_esp);
    enter_usermode(sigaction->sa_handler, signal_frame, NULL);
  }
}

void sigreturn(interrupt_registers *regs) {
  process* current_process = get_current_process();
  thread* current_thread = get_current_thread();
  log("Signal: Return from signal handler %s(p%d)", current_process->path, current_process->pid);
  
  struct signal_frame *signal_frame = (char *)regs->useresp - 4;
  if (signal_frame->uregs.int_no == DISPATCHER_ISR) {
    // 100% this code was invoked by page_fault
    signal_frame->uregs.int_no = 14; 
  }

  int signum = signal_frame->signum;
  struct sigaction *sigaction = &current_process->sighand[signum - 1];
  current_thread->blocked &= ~(sigmask(signum) | sigaction->sa_mask); // unmask

  memcpy(
    (char*)current_thread->kernel_esp - sizeof(interrupt_registers), 
    &signal_frame->uregs, 
    sizeof(interrupt_registers)
  );
}