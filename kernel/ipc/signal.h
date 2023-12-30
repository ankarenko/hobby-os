#ifndef KERNEL_IPC_H
#define KERNEL_IPC_H

#include <stdbool.h>

#include "kernel/util/types.h"
#include "kernel/cpu/idt.h"

#define NSIG 32

#define SIGHUP	1	    // Terminal line hangup
#define SIGINT	2	    // Interrupt program
#define SIGQUIT	3	    // Quit program
#define SIGILL	4	    // Illegal instruction
#define SIGTRAP	5	    // Trace trap
#define SIGABRT	6	    // Abort
#define SIGEMT	7	    // Emulate instruction executed
#define SIGFPE	8	    // Floating-point exception
#define SIGKILL	9	    // Kill program
#define SIGBUS	10	  // Bus error
#define SIGSEGV	11	  // Segmentation violation
#define SIGSYS	12	  // Bad argument to system call
#define SIGPIPE	13	  // Write on a pipe with no one to read it
#define SIGALRM	14	  // Real-time timer expired
#define SIGTERM	15	  // Software termination signal
#define SIGURG	16	  // Urgent condition on I/O channel
#define SIGSTOP	17	  // Stop signal not from terminal
#define SIGTSTP	18	  // Stop signal from terminal
#define SIGCONT	19	  // A stopped process is being continued
#define SIGCHLD	20	  // Notification to parent on child stop or exit
#define SIGTTIN	21	  // Read on terminal by background process
#define SIGTTOU	22	  // Write to terminal by background process
#define SIGIO	23	    // I/O possible on a descriptor
#define SIGXCPU	24	  // CPU time limit exceeded
#define SIGXFSZ	25	  // File-size limit exceeded
#define SIGVTALRM	26	// Virtual timer expired
#define SIGPROF	27	  // Profiling timer expired
#define SIGWINCH	28	// Window size changed
#define SIGINFO	29	  // Information request
#define SIGUSR1	30	  // User-defined signal 1
#define SIGUSR2	31	  // User-defined signal 2
#define SIGRTMIN 32 

#define sigmask(sig) (1UL << ((sig)-1))
#define siginmask(sig, mask) ((sig) > 0 && (sig) < SIGRTMIN && (sigmask(sig) & (mask)))

#define SIG_KERNEL_ONLY_MASK (sigmask(SIGKILL) | sigmask(SIGSTOP))
#define SIG_KERNEL_IGNORE_MASK ( sigmask(SIGCONT) | sigmask(SIGCHLD) | sigmask(SIGWINCH) | sigmask(SIGURG))

typedef void (*__sighandler_t)(int);

#define SIG_DFL ((__sighandler_t)0)	 /* default signal handling */
#define SIG_IGN ((__sighandler_t)1)	 /* ignore signal */
#define SIG_ERR ((__sighandler_t)-1) /* error return from signal */

#define sig_kernel_only(sig) (((sig) < SIGRTMIN) && siginmask(sig, SIG_KERNEL_ONLY_MASK))
#define sig_kernel_ignore(sig) (((sig) < SIGRTMIN) && siginmask(sig, SIG_KERNEL_IGNORE_MASK))

static inline bool valid_signal(unsigned long sig) {
	return sig <= NSIG;
}

struct sigaction {
	__sighandler_t sa_handler;
	uint32_t sa_flags;
	sigset_t sa_mask;
};

void signal_handler(interrupt_registers *regs);
void handle_signal(interrupt_registers *regs, sigset_t restored_sig);
void sigreturn(interrupt_registers *regs);
int do_sigaction(int signum, const struct sigaction *action, struct sigaction *old_action);

#endif