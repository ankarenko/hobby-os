#ifndef KERNEL_IPC_H
#define KERNEL_IPC_H

#include <stdbool.h>

#include "kernel/util/types.h"
#include "kernel/cpu/idt.h"

#define NSIG 32

#define SIGHUP 1
#define SIGINT 2
#define SIGQUIT 3
#define SIGILL 4
#define SIGTRAP 5
#define SIGABRT 6
#define SIGIOT 6
#define SIGBUS 7
#define SIGFPE 8
#define SIGKILL 9
#define SIGUSR1 10
#define SIGSEGV 11
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGSTKFLT 16
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGTTIN 21
#define SIGTTOU 22
#define SIGURG 23
#define SIGXCPU 24
#define SIGXFSZ 25
#define SIGVTALRM 26
#define SIGPROF 27
#define SIGWINCH 28
#define SIGIO 29
#define SIGPOLL SIGIO
#define SIGLOST		29
#define SIGPWR 30
#define SIGSYS 31
#define SIGUNUSED 31

typedef void (*__sighandler_t)(int);

#define SIG_DFL ((__sighandler_t)0)	 /* default signal handling */
#define SIG_IGN ((__sighandler_t)1)	 /* ignore signal */
#define SIG_ERR ((__sighandler_t)-1) /* error return from signal */

#define sigmask(sig) (1UL << ((sig)-1))

struct sigaction {
	__sighandler_t sa_handler;
	uint32_t sa_flags;
	sigset_t sa_mask;
};

void signal_handler(interrupt_registers *regs);
void handle_signal(interrupt_registers *regs, sigset_t restored_sig);
void sigreturn(interrupt_registers *regs);

#endif