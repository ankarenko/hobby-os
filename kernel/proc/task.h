
#ifndef _PROC_TASK_H
#define _PROC_TASK_H

#include <stdint.h>
#include "../memory/vmm.h"
#include "../include/list.h"

/*
0x00000000-0x00100000 � Kernel reserved
0x00100000-0xC0000000 � User land 
0xC0000000-0xffffffff � Kernel reserved
*/

#define KERNEL_STACK_SIZE 0x1000
#define USER_STACK_SIZE 0x1000

#define KE_USER_START 0x00100000
#define KE_KERNEL_START 0xC0000000

#define MAX_THREAD 5

#define PROCESS_STATE_SLEEP 0
#define PROCESS_STATE_ACTIVE 1
#define PROC_INVALID_ID -1

/* be very careful with modifying this as it's used by assembler code */
typedef struct _trap_frame {
	uint32_t epb, edi, esi, ebx;    // Pushed by pusha.
	uint32_t eip;									  // eip is saved on stack by the caller's "CALL" instruction
	uint32_t return_address;
	uint32_t parameter1, parameter2, parameter3;
} trap_frame;

enum thread_state {
	THREAD_NEW,
	THREAD_READY,
	THREAD_RUNNING,
	THREAD_WAITING,
	THREAD_TERMINATED,
};

struct _process;
typedef unsigned int ktime_t;

typedef struct _thread_info {

} thread_info;

union thread_union {
  thread_info info;
  uint64_t stack[1024];
};

typedef struct _mm_struct {
  struct list_head mmap;
	uint32_t start_code, end_code, start_data, end_data;
} mm_struct;

typedef struct _thread {
  uint32_t  kernel_esp;
  uint32_t  kernel_ss;
  uint32_t  user_esp; // user stack
  uint32_t  user_ss; // user stack segment
  uint32_t  esp;

  struct _process* parent;
  uint32_t tid;

  uint32_t  priority;
  //uint32_t state;
  enum thread_state state;
  ktime_t   sleep_time_delta;
  struct list_head sched_sibling;
  struct list_head th_sibling;
} thread;

/* be very careful with modifying this as it's used by assembler code */
typedef struct _process {
  int32_t id;
  int32_t priority;
  struct pdirectory* va_dir;
  physical_addr pa_dir;
  mm_struct* mm;
  int32_t state;
  uint32_t  image_base;
  uint32_t  image_size;
  int32_t thread_count;
  char *path;
  //struct _thread  threads[MAX_THREAD];
  struct list_head threads;
  struct list_head proc_sibling;
} process;

thread* get_current_thread();
void thread_sleep(uint32_t ms);
bool initialise_multitasking(virtual_addr entry);
thread* kernel_thread_create(process* parent, virtual_addr eip);
process* create_system_process(virtual_addr entry);
struct list_head* get_proc_list();
process* create_elf_process(char* path);
// extern "C" void TerminateProcess ();

// sched.c
void lock_scheduler();
void unlock_scheduler();
void make_schedule();
void sched_init();
struct list_head* get_ready_threads();
void sched_push_queue(thread *th, enum thread_state state);
thread *pop_next_thread_to_terminate();
bool thread_kill(uint32_t id);


// exit.c
void garbage_worker_task();

#endif