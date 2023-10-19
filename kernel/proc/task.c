#include <math.h>
#include <string.h>

#include "task.h"
#include "kernel/memory/vmm.h"
#include "kernel/cpu/hal.h"
#include "kernel/cpu/tss.h"
#include "kernel/memory/malloc.h"
#include "kernel/cpu/gdt.h"
#include "elf.h"
#include "kernel/util/list.h"
#include "kernel/util/debug.h"

#define PROCESS_TRAPPED_PAGE_FAULT 0xFFFFFFFF

extern void enter_usermode(
  virtual_addr entry,
  virtual_addr user_esp, 
  virtual_addr return_addr
);

extern thread*  _current_thread;

LIST_HEAD(_proc_list);
struct list_head* get_proc_list() {
  return &_proc_list;
}

void start_kernel_task(thread* th);

static uint32_t next_pid = 0;
static uint32_t next_tid = 0;

void scheduler_isr();
void (*old_pic_isr)();
void idle_task();

/* create new address space. */
struct pdirectory* create_address_space() {
	struct pdirectory* space;

	/* we need our page directory to be aligned, 0-11 bits should be zero */
  virtual_addr aligned_object = kalign_heap(PAGE_SIZE);
	space = kcalloc(PAGE_SIZE, sizeof(char)); // (struct pdirectory*)pmm_alloc_frame();
  if (aligned_object)
		kfree(aligned_object);

	/* clear page directory and clone kernel space. */
	vmm_pdirectory_clear(space);
	vmm_clone_kernel_space(space);
  physical_addr phys = vmm_get_physical_address(space, false);
  
  KASSERT(phys % PAGE_SIZE == 0); // check alignment 

  // recursive trick
  space->m_entries[TABLES_PER_DIR - 1] = 
    phys | 
    I86_PDE_PRESENT | 
    I86_PDE_WRITABLE;
  //5255168 //5263392

  //vmm_switch_pdirectory(space);
	return space;
}

/* create a new kernel space stack. */
bool create_kernel_stack(virtual_addr* kernel_stack) {
	void* ret;

  /* allocate a 4k frame for the stack. */
  // https://forum.osdev.org/viewtopic.php?f=1&t=22014
  // stack is better to be aligned 16byte
  virtual_addr aligned = kalign_heap(16); 
	*kernel_stack = kcalloc(KERNEL_STACK_SIZE, sizeof(char));
  if (aligned) {
    kfree(aligned);
  }

  if (!(*kernel_stack)) {
    return false;
  }

	/* moving pointer to the top of the stack */
	*kernel_stack += KERNEL_STACK_SIZE;

	return true;
}

/* creates user stack for main thread. */
bool create_user_stack(
  struct pdirectory* space, 
  virtual_addr* user_esp,
  physical_addr* user_stack_end, 
  virtual_addr addr
) {
	/* this will call our address space allocator
	to reserve area in user space. Until then,
	return error. */
  if (USER_STACK_SIZE % PMM_FRAME_SIZE != 0) {
    printf("User stack size is not %d aligned", PMM_FRAME_SIZE);
    return;
  }

  if (addr % PMM_FRAME_SIZE != 0) {
    printf("User stack address is not %d aligned", PMM_FRAME_SIZE);
    return;
  }

  uint32_t frames_count = USER_STACK_SIZE / PMM_FRAME_SIZE;
  physical_addr user_stack = pmm_alloc_frames(frames_count);
    
  for (int i = 0; i < frames_count; ++i) {
    vmm_map_address(
      addr + PMM_FRAME_SIZE * i, 
      user_stack + PMM_FRAME_SIZE * i, 
      I86_PTE_PRESENT | I86_PTE_WRITABLE | I86_PTE_USER
    );
  }
  
  *user_esp = addr + USER_STACK_SIZE;
  *user_stack_end = user_stack;

  return true;
}

void kernel_thread_entry(thread *th, void *flow()) {
  // we are not returning from PIT interrupt, 
  // so we need to enable interrupts manually
  enable_interrupts();
  interruptdone(IRQ0);

  flow();
  schedule();
}

extern uint32_t DEBUG_LAST_TID;

static thread* thread_create(
  process* parent, 
  virtual_addr eip,
  virtual_addr entry
) {
  lock_scheduler();

  thread *th = kcalloc(1, sizeof(thread));
  virtual_addr kernel_stack;

  if (!create_kernel_stack(&kernel_stack)) {
    return NULL;
  }

  th->parent = parent;
	th->priority = 0;
  th->tid = ++next_tid;
	th->state = THREAD_READY;
	th->sleep_time_delta = 0;
  th->kernel_esp = kernel_stack;
  th->user_esp = NULL;
  th->kernel_ss = KERNEL_DATA;
  th->user_ss = USER_DATA;
  INIT_LIST_HEAD(&th->sched_sibling);
  INIT_LIST_HEAD(&th->th_sibling);

  /* adjust stack. We are about to push data on it. */
  th->esp = th->kernel_esp - sizeof(trap_frame); // ????ALIGN_UP(sizeof(trap_frame), 16);
  trap_frame* frame = ((trap_frame*) th->esp);
  memset(frame, 0, sizeof(trap_frame));
  
  frame->parameter2 = eip;
	frame->parameter1 = (uint32_t)th;
	frame->return_address = PROCESS_TRAPPED_PAGE_FAULT;
	frame->eip = entry;

  th->parent = parent;

  list_add(&th->th_sibling, &parent->threads);
  parent->thread_count += 1;
  
  unlock_scheduler();
  return th;
}

static void user_thread_elf_entry(thread *th) {
  enable_interrupts();
  interruptdone(IRQ0);

  process* parent = th->parent;
  char* path = parent->path;
  virtual_addr entry = NULL;

  // try to load image into our address space
  // TODO: mos make it differently check it out
  if (!elf_load_image(path, th, &entry)) {
    PANIC("ELF is not loaded properly", NULL);
  }

  tss_set_stack(KERNEL_DATA, th->kernel_esp);
  enter_usermode(entry, th->user_esp, PROCESS_TRAPPED_PAGE_FAULT);
}

thread* user_thread_create(process* parent) {
  thread* th = thread_create(parent, NULL, user_thread_elf_entry);
  return th;
}

thread* kernel_thread_create(process* parent, virtual_addr eip) {
  thread* th = thread_create(parent, eip, kernel_thread_entry);
  return th;
}

static process* create_process(char* app_path, struct pdirectory* pdir) {
  lock_scheduler();

  process* proc = kcalloc(1, sizeof(process));
  INIT_LIST_HEAD(&proc->threads);
  INIT_LIST_HEAD(&proc->proc_sibling);
  list_add(&proc->proc_sibling, get_proc_list());
  proc->id = ++next_pid;
  proc->thread_count = 0;
  proc->path = app_path;
  proc->mm_mos = kcalloc(1, sizeof(mm_struct_mos));
  INIT_LIST_HEAD(&proc->mm_mos->mmap);

  proc->image_base = NULL;
  proc->image_size = 0;
  proc->va_dir = pdir? pdir : create_address_space();
  proc->pa_dir = vmm_get_physical_address(proc->va_dir, false);

  unlock_scheduler();

	return proc;
}

void idle_task() {
	while(1) {
    __asm volatile ("pause" ::: "memory");
  }
}

bool process_load(char* app_path) {
  
}

extern void cmd_init();

void terminate_process() {
	/* release threads */
  /*
  thread* th = NULL;
  list_for_each_entry(th, &cur->threads, th_sibling) {
    vmm_unmap_address(cur->va_dir, th->kernel_esp - PMM_FRAME_SIZE);
  }
  */
  
  /*
  for (uint32_t i = 0; i < cur->thread_count; ++i) {
    thread* pThread = &cur->threads[i];
    vmm_unmap_address(cur->va_dir, pThread->kernel_esp);
    vmm_unmap_address(cur->va_dir, pThread->user_esp);
    // make it orphan, so it will be deleetd from the queue
    pThread->parent = ORPHAN_THREAD;
  }
  */
  
	
  /* unmap and release image memory */
  /*
	for (uint32_t page = 0; page < div_ceil(cur->image_size, PAGE_SIZE); page++) {
		
    physical_addr phys = 0;
		virtual_addr virt = 0;

		/* get virtual address of page */
    /*
		virt = cur->image_base + (page * PAGE_SIZE);
		vmm_unmap_address(cur->va_dir, virt);
	}

  remove_process(*cur);
  */
}

process* create_system_process(virtual_addr entry) {
  process* proc = create_process("system", vmm_get_directory());
  thread* th = kernel_thread_create(proc, entry);
  
  if (!th) {
    return false;
  }

  sched_push_queue(th, THREAD_READY);
  return proc;
}

process* create_elf_process(char* path) {
  process* proc = create_process(path, NULL);
  thread* th = user_thread_create(proc);

  if (!th || !proc) {
    PANIC("Thread or process were not created properly", NULL);
  }

  sched_push_queue(th, THREAD_READY);
  return proc;
}

thread* get_current_thread() {
  return _current_thread;
}

process* get_current_process() {
  return _current_thread->parent;
}

bool initialise_multitasking(virtual_addr entry) {
  sched_init();

  process* parent = create_process("",  vmm_get_directory());
  _current_thread = kernel_thread_create(parent, entry);
  sched_push_queue(_current_thread, THREAD_READY);
  
  thread* garbage_worker = kernel_thread_create(parent, garbage_worker_task);
  sched_push_queue(garbage_worker, THREAD_READY);

  /* register isr */
  old_pic_isr = getvect(IRQ0);
  setvect(IRQ0, scheduler_isr, I86_IDT_DESC_PRESENT | I86_IDT_DESC_BIT32);

  start_kernel_task(_current_thread);
}