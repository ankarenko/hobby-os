
#ifndef _PROC_TASK_H
#define _PROC_TASK_H

#include <stdint.h>

#include "kernel/util/types.h"
#include "kernel/ipc/signal.h"
#include "kernel/locking/semaphore.h"
#include "kernel/util/list.h"
#include "kernel/devices/char/tty.h"
#include "kernel/proc/wait.h"
#include "kernel/memory/vmm.h"
#include "kernel/system/timer.h"
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
  uint32_t ebp, edi, esi, ebx;  // Pushed by pusha.
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
  //struct list_head mmap;
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

typedef struct process;

struct thread {
  uint32_t kernel_esp;
  uint32_t kernel_ss;
  uint32_t user_esp;  // user stack
  uint32_t user_ss;   // user stack segment
  uint32_t esp;

  struct process *proc;
  uint32_t tid;

  physical_addr phys_ustack_bottom;  // TODO: needs to be put in mm_struct 
  virtual_addr virt_ustack_bottom;
  enum thread_state state;

  struct list_head sched_sibling;
  struct list_head sibling;
  struct list_head child;

  // used by UNIX signals
  sig_t pending;
	sig_t blocked;
	bool signaling;
  
  struct sleep_timer s_timer;
};

typedef struct _files_struct {
  // TODO: what to do if two threads from different processes try to access a file
	struct semaphore *lock;   // used to synchronized threads and childs of a given process
	struct vfs_file *fd[MAX_FD];
} files_struct;


typedef struct _fs_struct {
	struct vfs_dentry* d_root;
	struct vfs_mount* mnt_root;
} fs_struct;

/* 
  be very careful with modifying this as it's used by assembler code 
  if you happen to change that it is better to add new fields 
  to the end of the structure
*/

struct wait_queue_head;

struct process {
  int32_t pid;

  int32_t priority;
  struct pdirectory* va_dir;
  physical_addr pa_dir;
  mm_struct_mos* mm_mos;
  int32_t state;
  uint32_t image_base;
  uint32_t image_size;
  atomic_t thread_count;
  char* name;

  struct list_head threads;
  struct list_head sibling; // used for a list of all processes
  
  files_struct* files; 
  fs_struct* fs;

  struct tty_struct *tty;
  struct sigaction sighand[NSIG];
  struct wait_queue_head wait_chld;

  int32_t gid;  // group id
  int32_t sid;  // session id
  int32_t exit_code;
  struct process *parent;
  struct list_head childrens;
  struct list_head child; // used for a list of childs
};

// task.c
struct thread* get_current_thread();
struct process* get_current_process();
void thread_sleep(uint32_t ms);
bool initialise_multitasking(virtual_addr entry);
struct thread* kernel_thread_create(struct process* parent, virtual_addr eip);
struct process* create_system_process(virtual_addr entry, char* name);
struct list_head* get_proc_list();
struct process* create_elf_process(struct process* parent, char* path);
pid_t process_fork(struct process* parent);
int32_t dup2(int oldfd, int newfd);
struct process *find_process_by_pid(pid_t pid);
int32_t setpgid(pid_t pid, pid_t pgid);

// sched.c
void lock_scheduler();
void unlock_scheduler();
void make_schedule();
void sched_init();
void schedule();
struct list_head* get_ready_threads();
void sched_push_queue(struct thread* th);
struct thread* pop_next_thread_to_terminate();
bool thread_kill(uint32_t id);
void thread_wake(struct thread *th);
bool thread_signal(uint32_t tid, int32_t signal);
void thread_wait(struct thread *th);
void thread_update(struct thread *t, enum thread_state state);
struct list_head* get_waiting_threads();

// exit.c
void garbage_worker_task();
void do_exit(int code);
bool exit_thread(struct thread* th);

#endif