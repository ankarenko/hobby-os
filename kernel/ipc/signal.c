#include "kernel/memory/vmm.h"
#include "kernel/cpu/idt.h"
#include "kernel/proc/task.h"
#include "kernel/util/debug.h"
#include "kernel/cpu/tss.h"
#include "kernel/cpu/gdt.h"

#include "kernel/ipc/signal.h"

extern void enter_usermode(
  virtual_addr entry,
  virtual_addr user_esp, 
  virtual_addr return_addr
);

struct signal_frame {
	void (*sigreturn)(struct interrupt_registers *);
	int32_t signum;
	bool signaling;
	sigset_t blocked;
	interrupt_registers uregs;
};

void signal_handler(interrupt_registers *regs) {
  
}

void signal_handler_test(int num) {
  // it will be invoked as a signal handler in user space
  // then it returns to a sigreturn address and page fault occurs
  int i = 0; 
}

void handle_signal(interrupt_registers *regs, sigset_t restored_sig) {
  
  int signum = 1;
  int prev_signaling = 1;

  bool from_syscall = false;
  if (regs->int_no == DISPATCHER_ISR) {
		from_syscall = true;
		//regs->eax = -EINTR;
	}

  thread* current_thread = get_current_thread();
  process* current_process = get_current_process();

  struct signal_frame *signal_frame = (char *)regs->useresp - sizeof(struct signal_frame);
  signal_frame->sigreturn = &sigreturn;
  signal_frame->signum = signum;
  //frame->signaling = prev_signaling;
  //frame->blocked = restored_sig;
  signal_frame->uregs = *regs;

  //struct sigaction *sigaction = &current_process->sighand[signum - 1];
  //regs->eip = (uint32_t)sigaction->sa_handler;
  //current_thread->blocked |= sigmask(signum) | sigaction->sa_mask;
  if (from_syscall) {
    tss_set_stack(KERNEL_DATA, current_thread->kernel_esp);
    enter_usermode(&signal_handler_test, signal_frame, &sigreturn);
  }
}

void sigreturn(interrupt_registers *regs) {
  process* current_process = get_current_process();
  thread* current_thread = get_current_thread();
  log("Signal: Return from signal handler %s(p%d)", current_process->path, current_process->pid);
  current_thread->pending &= ~sigmask(0); // unmask signal
}