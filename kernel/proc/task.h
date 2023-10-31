
#ifndef _PROC_TASK_H
#define _PROC_TASK_H

#include <stdint.h>
#include <list.h>

#include "kernel/memory/vmm.h"
#include "kernel/fs/vfs.h"

/*
0x00000000-0x00100000 � Kernel reserved
0x00100000-0xC0000000 � User land
0xC0000000-0xffffffff � Kernel reserved
*/

#define PROCESS_TRAPPED_PAGE_FAULT 0xFFFFFFFF

#define KE_USER_START 0x00100000
#define KE_KERNEL_START 0xC0000000

#define MAX_FD 256

#define PROCESS_STATE_SLEEP 0
#define PROCESS_STATE_ACTIVE 1
#define PROC_INVALID_ID -1

/* be very careful with modifying this as it's used by assembler code */
typedef struct _trap_frame {
  uint32_t epb, edi, esi, ebx;  // Pushed by pusha.
  uint32_t eip;                 // eip is saved on stack by the caller's "CALL" instruction
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

typedef struct _vm_area_struct
{
	struct mm_struct *vm_mm;
	uint32_t vm_start;
	uint32_t vm_end;
	uint32_t vm_flags;

	struct list_head vm_sibling;
} vm_area_struct;

typedef struct _mm_struct_mos {
  struct list_head mmap;
	uint32_t free_area_cache; // not used now
	uint32_t start_code, end_code, start_data, end_data; 
	// NOTE: MQ 2020-01-30
	// end_brk is marked as the end of heap section, brk is end but in range start_brk<->end_brk and expand later
	// better way is only mapping start_brk->brk and handling page fault brk->end_brk
  uint32_t start_brk, brk, end_brk, start_stack; // not used now

  virtual_addr heap_start;
  //virtual_addr brk;  // current pointer
  uint32_t remaning;
  virtual_addr heap_end;
} mm_struct_mos;

typedef struct _thread {
  uint32_t kernel_esp;
  uint32_t kernel_ss;
  uint32_t user_esp;  // user stack
  uint32_t user_ss;   // user stack segment
  uint32_t esp;

  struct _process* parent;
  uint32_t tid;

  physical_addr phys_ustack_bottom;
  virtual_addr virt_ustack_bottom;
  uint32_t priority;
  // uint32_t state;
  enum thread_state state;
  ktime_t sleep_time_delta;
  struct list_head sched_sibling;
  struct list_head th_sibling;
} thread;

typedef struct _files_struct {
	//struct semaphore lock;
	vfs_file *fd[MAX_FD];
} files_struct;

/* 
  be very careful with modifying this as it's used by assembler code 
  if you happen to change that it is better to add new fields 
  to the end of the structure
*/
typedef struct _process {
  int32_t id;
  int32_t priority;
  struct pdirectory* va_dir;
  physical_addr pa_dir;
  mm_struct_mos* mm_mos;
  int32_t state;
  uint32_t image_base;
  uint32_t image_size;
  int32_t thread_count;
  char* path;

  struct list_head threads;
  struct list_head proc_sibling;
  files_struct* files; 
} process;

thread* get_current_thread();
process* get_current_process();
void thread_sleep(uint32_t ms);
bool initialise_multitasking(virtual_addr entry);
thread* kernel_thread_create(process* parent, virtual_addr eip);
process* create_system_process(virtual_addr entry);
struct list_head* get_proc_list();
process* create_elf_process(char* path);

// sched.c
void lock_scheduler();
void unlock_scheduler();
void make_schedule();
void sched_init();
struct list_head* get_ready_threads();
void sched_push_queue(thread* th, enum thread_state state);
thread* pop_next_thread_to_terminate();
bool thread_kill(uint32_t id);

// exit.c
void garbage_worker_task();

#endif