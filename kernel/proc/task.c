#include <math.h>
#include "task.h"
#include <string.h>
#include "../memory/vmm.h"
#include "../cpu/hal.h"
#include "../cpu/tss.h"
#include "../memory/malloc.h"
#include "../cpu/gdt.h"
#include "elf.h"

#define THREAD_MAX 10
#define PROC_MAX 10

process _kernel_proc = { .id = 0 };

thread   _ready_queue  [THREAD_MAX];
int32_t  _queue_last, _queue_first;
thread   _idle_thread;
thread*  _current_task;
thread   _current_thread_local;
process  _process_list [PROC_MAX];

static uint32_t next_pid = 0;
static uint32_t next_tid = 0;

void scheduler_isr();
void (*old_pic_isr)();
void idle_task();

extern void jump_to_process(virtual_addr entry, virtual_addr stack);

static process _proc = {
	PROC_INVALID_ID,0,0,0,0
};

/*** PROCESS LIST ***********************************/

/* add process to list. */
process* add_process(process p) {
	for (int c = 0; c < PROC_MAX; c++) {
		/* id's of -1 are free. */
		if (_process_list[c].id == PROC_INVALID_ID) {
			_process_list[c] = p;
			return &_process_list[c];
		}
	}
	return 0;
}

process* get_process(uint32_t id) {
  for (int c = 0; c < PROC_MAX; c++) {
		if (_process_list[c].id == id) {
			return &_process_list[c];
		}
	}
	return 0;
}

/* remove process from list. */
void remove_process(process p) {
	for (int c = 0; c < PROC_MAX; c++) {
		if (_process_list[c].id == p.id) {
			/* id's of -1 are free. */
			_process_list[c].id = PROC_INVALID_ID;
			return;
		}
	}
}

/* init process list. */
void init_process_list() {
	for (int c = 0; c < PROC_MAX; c++)
		_process_list[c].id = PROC_INVALID_ID;
}

/*** READY QUEUE ************************************/

/* clear queue. */
void clear_queue() {
	_queue_first = 0;
	_queue_last  = 0;
}

/* insert thread. */
bool queue_insert(thread t) {
	_ready_queue[_queue_last % THREAD_MAX] = t;
	_queue_last++;
	return true;
}

/* remove thread. */
thread queue_remove() {
	thread t;
	t = _ready_queue[_queue_first % THREAD_MAX];
	_queue_first++;
	return t;
}

/* get top of queue. */
thread queue_get() {
	return _ready_queue[_queue_first % THREAD_MAX];
}

/* create new address space. */
struct pdirectory* create_address_space() {
	struct pdirectory* space;

	/* we need our page directory to be aligned, 0-11 bits should be zero */
  virtual_addr aligned_object = kalign_heap(PMM_FRAME_SIZE);
	space = kmalloc(PAGE_SIZE); // (struct pdirectory*)pmm_alloc_frame();
  if (aligned_object)
		kfree(aligned_object);

	/* clear page directory and clone kernel space. */
	vmm_pdirectory_clear(space);
	vmm_clone_kernel_space(space);

  // recursive trick
  
  space->m_entries[PAGES_PER_DIR - 1] = 
    vmm_get_physical_address(space, true) | 
    I86_PDE_PRESENT | 
    I86_PDE_WRITABLE | 
    I86_PDE_USER;

	return space;
}

/* create a new kernel space stack. */
int _kernel_stack_index = 0;
bool create_kernel_stack_static(virtual_addr* kernel_stack) {

	physical_addr       p;
	virtual_addr        location;
	void*               ret;

	/* we are reserving this area for 4k kernel stacks. */
#define KERNEL_STACK_ALLOC_BASE 0xe0000000

	/* allocate a 4k frame for the stack. */
	p = (physical_addr)pmm_alloc_frame();
	if (!p)
		return false;

	/* next free 4k memory block. */
	location = KERNEL_STACK_ALLOC_BASE + _kernel_stack_index * PAGE_SIZE;

	/* map it into kernel space. */
	vmm_map_address(
    vmm_get_directory(), location, 
    p, I86_PTE_PRESENT | I86_PTE_WRITABLE
  );

	/* we are returning top of stack. */
	*kernel_stack = (void*) (location + PAGE_SIZE);

	/* prepare to allocate next 4k if we get called again. */
	_kernel_stack_index++;

	/* and return top of stack. */
	return true;
}


/* create a new kernel space stack. */
bool create_kernel_stack(virtual_addr* kernel_stack) {
  uint32_t stack_size = PMM_FRAME_SIZE;
	void* ret;

  /* allocate a 4k frame for the stack. */
  // https://forum.osdev.org/viewtopic.php?f=1&t=22014
  // stack is better to be aligned 16byte
  virtual_addr aligned = kalign_heap(16); 
	*kernel_stack = kmalloc(stack_size);
  if (aligned) {
    kfree(aligned);
  }

  if (!*kernel_stack) {
    return false;
  }

	/* moving pointer to the top of the stack */
	*kernel_stack += stack_size;

	return true;
}


/* creates user stack for main thread. */
bool create_user_stack(
  struct pdirectory* space, 
  virtual_addr* user_esp, 
  virtual_addr addr
) {
	/* this will call our address space allocator
	to reserve area in user space. Until then,
	return error. */
  
  physical_addr user_stack = pmm_alloc_frame();
  
  vmm_map_address(
    space, 
    addr, 
    user_stack, 
    I86_PTE_PRESENT | I86_PTE_WRITABLE | I86_PTE_USER
  );

  *user_esp = addr + PMM_FRAME_SIZE;

  return true;
}

/* 
  executes thread, remember that switching thread is just a matter 
  of switching stack pointer 
*/
void thread_execute(thread t) {
	__asm__ __volatile__("mov %0, %%esp \n"
											 "pop %%gs			\n"
											 "pop %%fs			\n"
											 "pop %%es			\n"
											 "pop %%ds			\n"
											 "popal					\n"
											 "iret					\n" ::"r"(t.kernel_esp));
}

/* execute idle thread. */
void execute_idle() {
	/* just run idle thread. */
	thread_execute(_idle_thread);
}

/* gets called for each clock tick. */
void scheduler_tick() {
	/* just run dispatcher. */
	dispatch();
}

/* schedule next task. */
void dispatch () {
	/* We do Round Robin here, just remove and insert. */
next_thread:
	queue_remove();
	thread prev = _current_thread_local;
  queue_insert(_current_thread_local);
  _current_thread_local = queue_get();

	/* make sure this thread is not blocked. */
	if (_current_thread_local.state & THREAD_BLOCK_STATE) {

		/* adjust time delta. */
		if (_current_thread_local.sleep_time_delta > 0)
			_current_thread_local.sleep_time_delta--;

		/* should we wake thread? */
		if (_current_thread_local.sleep_time_delta == 0) {
			thread_wake();
			goto exec_thread;
		}

		/* not yet, go to next thread. */
		goto next_thread;
	}
exec_thread:
  // check if we need to change address space
  if (prev.parent->id != _current_thread_local.parent->id) {
    struct pgdirectory* space = _current_thread_local.parent->page_directory;
    vmm_switch_pdirectory(space);
  }
}

/* remove thread state flags. */
void thread_remove_state(thread* t, uint32_t flags) {
	/* remove flags. */
	t->state &= ~flags;
}

/* schedule new task to run. */
void schedule() {

	/* force a task switch. */
	__asm volatile("int $32");
}

/* set thread state flags. */
void thread_set_state(thread* t, uint32_t flags) {

	/* set flags. */
	t->state |= flags;
}
/* PUBLIC definition. */
void thread_sleep(uint32_t ms) {

	/* go to sleep. */
	thread_set_state(_current_task, THREAD_BLOCK_SLEEP);
	_current_task->sleep_time_delta = ms;
	schedule();
}

/* PUBLIC definition. */
void thread_wake() {

	/* wake up. */
	thread_remove_state(_current_task, THREAD_BLOCK_SLEEP);
	_current_task->sleep_time_delta = 0;
}

/* creates thread. */
thread thread_create(
  process *parent, 
  virtual_addr eip, 
  virtual_addr user_esp,
  virtual_addr kernel_esp,
  bool is_kernel
) {
	lock_scheduler();

  trap_frame* frame;
	thread t;

	/* adjust stack. We are about to push data on it. */
  kernel_esp -= sizeof(trap_frame); // ????ALIGN_UP(sizeof(trap_frame), 16);

	/* initialize task frame. */
	frame = ((trap_frame*) kernel_esp);
	// makes sure when doing iret, interrupts are enabled, 2th bit is just a parity bit
	// https://stackoverflow.com/questions/25707130/what-is-the-purpose-of-the-parity-flag-on-a-cpu
  frame->flags = 0x202;  
  frame->eip = eip;
	frame->ebp = 0;
	frame->esp = 0;
	frame->edi = 0;
	frame->esi = 0;
	frame->edx = 0;
	frame->ecx = 0;
	frame->ebx = 0;
	frame->eax = 0;

	/* set up segment selectors. */
	frame->cs = is_kernel? KERNEL_CODE : USER_CODE;
	frame->ds = is_kernel? KERNEL_DATA : USER_DATA;
	frame->es = is_kernel? KERNEL_DATA : USER_DATA;
	frame->fs = is_kernel? KERNEL_DATA : USER_DATA;
	frame->gs = is_kernel? KERNEL_DATA : USER_DATA;
	
  
	t.parent = parent == 0? &_kernel_proc : parent;
	t.priority = 0;
	t.state = THREAD_RUN;
	t.sleep_time_delta = 0;
  t.kernel_esp = kernel_esp;
  t.kernel_ss = KERNEL_DATA;
  t.user_ss = USER_DATA;
  t.user_esp = user_esp;

  unlock_scheduler();
	return t;
}


bool create_process(char* app_path, uint32_t* proc_id) {
  process pcb;
  struct pdirectory* address_space = 0;
  struct pdirectory* kernel_space = vmm_get_directory();
  virtual_addr image_base;
  uint32_t image_size;
  virtual_addr user_esp;
  virtual_addr entry;
  virtual_addr image_end = ALIGN_UP(image_base + image_size, PMM_FRAME_SIZE);
      
  // create userspace
  address_space = create_address_space();
  
  vmm_switch_pdirectory(address_space);
  {
    // try to load image into our address space
    // TODO: mos make it differently check it out
    if (!elf_load_image(app_path, address_space, &image_base, &image_size, &entry)) {
      return false;
    }

    /* create stack space for main thread at the end of the program */
    // dont forget that the stack grows up from down
    if (!create_user_stack(address_space, &user_esp, image_end)) {
      return false;
    }

    virtual_addr* kernel_esp;
    if (!create_kernel_stack(kernel_esp)) {
      return false;
    }

    /* create process. */
    pcb.id = ++next_pid;
    *proc_id = pcb.id;
    pcb.image_base = image_base;
    pcb.page_directory = address_space;

    /* create main thread. */
    pcb.thread_count = 1;
    pcb.threads[0] = thread_create(
      &pcb, (virtual_addr)entry, 
      user_esp, kernel_esp, false
    );
    add_process(pcb);
  }
  vmm_switch_back();

	return true;

/*
  proc->id = 1;
  proc->page_directory = layout.address_space;
  proc->priority = 1;
  proc->state = PROCESS_STATE_ACTIVE;
  proc->thread_count = 1;

  mthread = &proc->threads[0];
  mthread->kernel_stack = 0;
  mthread->parent = proc;
  mthread->priority = 1;
  mthread->state = PROCESS_STATE_ACTIVE;
  mthread->initial_stack = layout.stack;
  mthread->stack_limit = (void*) ((uint32_t) mthread->initial_stack + 4096);
  mthread->image_base = layout.image_start;
  mthread->image_size = layout.image_size;
  //memset (&mthread->frame, 0, sizeof (trapFrame));
  mthread->frame.eip = layout.entry;
  mthread->frame.flags = 0x200;  
  mthread->frame.esp = layout.stack;
  mthread->frame.ebp = mthread->frame.esp;
*/
}

bool process_load(char* app_path) {
  lock_scheduler();
  uint32_t proc_id;
  create_process(app_path, &proc_id);
  process* proc = get_process(proc_id);

  if (!proc) {
    return false;
  }

  /* schedule thread to run. */
	queue_insert(proc->threads[0]);
  unlock_scheduler();
}

void execute_process() {
  /*
  process* proc = 0;
  virtual_addr entry_point = 0;
  virtual_addr proc_stack = 0;

  /* get running process */
  /*
  proc = get_current_process();
  if (proc->id == PROC_INVALID_ID || !proc->page_directory)
    return;

  /* get esp and eip of main thread */
  /*
  entry_point = proc->threads[0].frame.eip;
  proc_stack  = proc->threads[0].frame.esp;

  /* switch to process address space */
  /*
  physical_addr p_dir = vmm_get_physical_address(proc->page_directory, false);
  
  //vmm_switch_pdirectory(p_dir, proc->page_directory);
  //pmm_load_PDBR(p_dir);
  
  uint8_t* addr = 0xffd00008;
  uint32_t i = PAGE_TABLE_INDEX((uint32_t)addr);
  pt_entry e = proc->page_directory->m_entries[i];
  
  int32_t esp;
  __asm__ __volatile__("mov %%esp, %0" : "=r"(esp));

  // setting up esp y ss, probably the only two fields we need to be bothered with
  tss_set_stack(0x10, esp);
  jump_to_process(entry_point, proc_stack);
  */
}

/**
* Return current process
* \ret Process
*/
process* get_current_process() {
	return &_proc;
}

/* idle thread. */

void idle_task() {
	/* loop forever and yield to cpu. */
	while(1) 
    __asm volatile ("pause" ::: "memory");
}

/* initialize scheduler. */
bool scheduler_initialize(void) {
  /* clear ready queue. */
  clear_queue();

  /* clear process list. */
  init_process_list();

  virtual_addr kernel_stack;
  if (!create_kernel_stack(&kernel_stack)) {
    return false;
  }

  /* create idle thread and add it. */
  _idle_thread = thread_create(
    0, 
    (virtual_addr)idle_task, 
    (virtual_addr)kernel_stack,
    (virtual_addr)kernel_stack, 
    true
  );

  /* set current thread to idle task and add it. */
  _current_thread_local = _idle_thread;
  _current_task = &_current_thread_local;
  queue_insert(_idle_thread);

  /* register isr */
  old_pic_isr = getvect(IRQ0);
  setvect(IRQ0, scheduler_isr, I86_IDT_DESC_PRESENT | I86_IDT_DESC_BIT32);
}

extern void cmd_init();
extern void restore_kernel();

void terminate_process() {
  /*
  process* cur = get_current_process();
	if (cur->id == PROC_INVALID_ID)
		return;

	/* release threads */
  /*
	uint32_t i = 0;
	thread* pThread = &cur->threads[i];

  uint8_t* addr = pThread->initial_stack;

  //*addr = 123;
  vmm_unmap_address(cur->page_directory, pThread->initial_stack);

  /* unmap and release image memory */
  /*
	for (uint32_t page = 0; page < div_ceil(pThread->image_size, PAGE_SIZE); page++) {
		
    physical_addr phys = 0;
		virtual_addr virt = 0;

		/* get virtual address of page */
  /*
		virt = pThread->image_base + (page * PAGE_SIZE);
		vmm_unmap_address(cur->page_directory, virt);
	}

  disable_interrupts();
  restore_kernel();
  enable_interrupts();

	/* return to kernel command shell */
  /*
	cmd_init();

	for (;;);
  */
}