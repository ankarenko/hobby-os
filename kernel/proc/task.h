
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

typedef struct _trap_frame {
  uint32_t esp;
  uint32_t ebp;
  uint32_t eip;
  uint32_t edi;
  uint32_t esi;
  uint32_t eax;
  uint32_t ebx;
  uint32_t ecx;
  uint32_t edx;
  uint32_t flags;
} trap_frame;

struct _process;

typedef struct _thread {
  struct _process* parent;
  void* initial_stack; /* virtual address */
  void* stack_limit;
  void* kernel_stack;
  uint32_t priority;
  int32_t state;
  trap_frame frame;
  uint32_t image_base;
  uint32_t image_size;
} thread;

typedef struct _process {
  int32_t id;
  int32_t priority;
  struct pdirectory* page_directory;
  int32_t state;
  /* threadCount will always be 1 */
  int32_t thread_count;
  struct _thread threads[MAX_THREAD];
} process;

//extern int create_thread(int (*entry)(void), uint32_t stackBase);
//extern int terminate_thread(thread* handle);

int32_t create_process(char* appname);
void execute_process();
process* get_current_process();

// extern "C" void TerminateProcess ();

#endif