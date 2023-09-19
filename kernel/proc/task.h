
#ifndef _PROC_TASK_H
#define _PROC_TASK_H

#include <stdint.h>
#include "../memory/vmm.h"

/*
0x00000000-0x00100000 � Kernel reserved
0x00100000-0xC0000000 � User land 
0xC0000000-0xffffffff � Kernel reserved
*/

#define KE_USER_START 0x00100000
#define KE_KERNEL_START 0xC0000000

#define MAX_THREAD 5

#define PROCESS_STATE_SLEEP 0
#define PROCESS_STATE_ACTIVE 1
#define PROC_INVALID_ID -1

#define THREAD_RUN 1
#define THREAD_BLOCK_SLEEP 2
/* we may have multiple block state flags. */
#define THREAD_BLOCK_STATE THREAD_BLOCK_SLEEP

/* be very careful with modifying this. Reference tss.cpp ISR. */
typedef struct _trap_frame {
	/* pushed by isr. */
	uint32_t gs;
	uint32_t fs;
	uint32_t es;
	uint32_t ds;
	/* pushed by pushf. */
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t esi;
	uint32_t edi;
	uint32_t esp;
	uint32_t ebp;
	/* pushed by cpu. */
	uint32_t eip;
	uint32_t cs;
	uint32_t flags;
	/* used only when coming from/to user mode. */
  //	uint32_t user_stack;
  //	uint32_t user_ss;
} trap_frame;

struct _process;
typedef unsigned int ktime_t;

typedef struct _thread {
  uint32_t  esp; // user stack
  uint32_t  ss; // user stack segment
  uint32_t  kernel_esp;
  uint32_t  kernel_ss;
  struct _process*  parent;
  uint32_t  priority;
  int32_t   state;
  ktime_t   sleep_time_delta;
}thread;

typedef struct _process {
  int32_t id;
  int32_t priority;
  struct pdirectory* page_directory;
  int32_t state;
  uint32_t  image_base;
  uint32_t  image_size;
  int32_t thread_count;
  struct _thread  threads[MAX_THREAD];
} process;

//extern int create_thread(int (*entry)(void), uint32_t stackBase);
//extern int terminate_thread(thread* handle);

int32_t create_process(char* appname);
void execute_process();
void terminate_process();
process* get_current_process();

// extern "C" void TerminateProcess ();

#endif