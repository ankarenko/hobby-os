#include "kernel/util/string/string.h"
#include "kernel/util/math.h"
#include "kernel/util/list.h"
#include "kernel/memory/vmm.h"
#include "kernel/cpu/hal.h"
#include "kernel/cpu/tss.h"
#include "kernel/memory/malloc.h"
#include "kernel/util/debug.h"
#include "kernel/cpu/gdt.h"
#include "kernel/proc/elf.h"

#include "kernel/proc/task.h"

extern void enter_usermode(
  virtual_addr entry,
  virtual_addr user_esp, 
  virtual_addr return_addr
);

extern struct thread *_current_thread;
struct list_head all_threads;
LIST_HEAD(_proc_list);

extern void scheduler_end();


struct list_head* get_proc_list() {
  return &_proc_list;
}

void start_kernel_task(struct thread* th);

static uint32_t next_pid = 0;
static uint32_t next_tid = 0;
static uint32_t next_sid = 0;

void scheduler_isr();
void (*old_pic_isr)();
void idle_task();

/* create a new kernel space stack. */
bool create_kernel_stack(virtual_addr* kernel_stack) {
	void* ret;

  /* allocate a 4k frame for the stack. */
  // https://forum.osdev.org/viewtopic.php?f=1&t=22014
  // stack is better to be aligned 16byte
  log("allocating kernel stack");
  *kernel_stack = kcalloc_aligned(KERNEL_STACK_SIZE, sizeof(char), 16);
  log("allocated kernel stack");

  /*
  virtual_addr aligned = kalign_heap(16); 
	*kernel_stack = kcalloc(KERNEL_STACK_SIZE, sizeof(char));
  if (aligned) {
    kfree(aligned);
  }
  */

  if (!(*kernel_stack)) {
    return false;
  }

	/* moving pointer to the top of the stack */
	*kernel_stack += KERNEL_STACK_SIZE;

	return true;
}

/* creates user stack for main struct thread. */
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
    assert_not_reached("User stack size is not %d aligned", PMM_FRAME_SIZE);
  }

  if (addr % PMM_FRAME_SIZE != 0) {
    assert_not_reached("User stack address is not %d aligned", PMM_FRAME_SIZE);
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

void kernel_thread_entry(struct thread *th, void *flow()) {
  // we are not returning from PIT interrupt, 
  // so we need to enable interrupts manually
  enable_interrupts();
  interruptdone(IRQ0);

  flow();
  schedule();
}

static void thread_wakeup_timer(struct sleep_timer *timer) {
	struct thread *th = from_timer(th, timer, s_timer);
	list_del(&timer->sibling);
  if (th->state == THREAD_WAITING) {
    thread_update(th, THREAD_READY);
  }
}

struct process *find_process_by_pid(pid_t pid) {
	struct process *iter;
  list_for_each_entry(iter, get_proc_list(), sibling) {
    if (iter->pid == pid)
      return iter;
  }

  return NULL;
}

static struct thread* thread_create(
  struct process* parent, 
  virtual_addr eip,
  virtual_addr entry
) {
  lock_scheduler();

  struct thread *th = kcalloc(1, sizeof(struct thread));
  virtual_addr kernel_stack;

  if (!create_kernel_stack(&kernel_stack)) {
    err("Unable to create kernel stack");
  }

  th->proc = parent;
  th->tid = ++next_tid;
	th->state = THREAD_READY;
  th->kernel_esp = kernel_stack;
  th->user_esp = NULL;
  th->kernel_ss = KERNEL_DATA;
  th->user_ss = USER_DATA;
  atomic_set(&th->lock_counter, 0);
  INIT_LIST_HEAD(&th->sched_sibling);
  //INIT_LIST_HEAD(&th->sibling);
  th->s_timer = (struct sleep_timer)TIMER_INITIALIZER(thread_wakeup_timer, UINT32_MAX);

  /* adjust stack. We are about to push data on it. */
  th->esp = th->kernel_esp - sizeof(trap_frame); // TODO: ????ALIGN_UP(sizeof(trap_frame), 16);
  trap_frame* frame = ((trap_frame*) th->esp);
  memset(frame, 0, sizeof(trap_frame));
  
  frame->parameter2 = eip;
	frame->parameter1 = (uint32_t)th;
	frame->return_address = PROCESS_TRAPPED_PAGE_FAULT;
	frame->eip = entry;

  list_add(&th->child, &parent->threads);
  list_add(&th->sibling, &all_threads);
  
  atomic_inc(&parent->thread_count);
  
  unlock_scheduler();
  return th;
}

bool empack_params(struct thread* th) {
  struct process* parent = th->proc;
  char* path = parent->name;

  uint32_t argc = 2;
  int32_t id = parent->pid; 
  char** argv = (char**)sbrk(
    sizeof(uint32_t) * argc, 
    parent->mm_mos
  );
  uint32_t len = strlen(path);
  char* arg_path = sbrk(
    len, 
    parent->mm_mos
  );
  memcpy(arg_path, path, len);

  char* arg_id = sbrk(
    sizeof(uint32_t), 
    parent->mm_mos
  );
  memcpy(arg_id, &id, sizeof(uint32_t));
  argv[0] = arg_path;
  argv[1] = arg_id;

  uint32_t params[3] = { 
    PROCESS_TRAPPED_PAGE_FAULT, 
    argc, 
    argv
  };
  memcpy(th->user_esp - 12, params, 12);
  th->user_esp -= 12;
  return true;
}

static void user_thread_elf_entry(struct thread *th) {
  enable_interrupts(); // not sure if it's needed because enter_usermode enables the flag
  interruptdone(IRQ0);

  struct process* parent = th->proc;
  char* path = parent->name;
  virtual_addr entry = NULL;

  // try to load image into our address space
  if (!elf_load_image(path, th, &entry)) {
    assert_not_reached("ELF is not loaded properly");
  }

  if (!empack_params(th)) {
    assert_not_reached("Cannot empack params");
  }

  tss_set_stack(KERNEL_DATA, th->kernel_esp);
  enter_usermode(entry, th->user_esp, PROCESS_TRAPPED_PAGE_FAULT);
}

struct thread* user_thread_create(struct process* parent) {
  struct thread* th = thread_create(parent, NULL, user_thread_elf_entry);
  return th;
}

struct thread* kernel_thread_create(struct process* parent, virtual_addr eip) {
  struct thread* th = thread_create(parent, eip, kernel_thread_entry);
  return th;
}

static files_struct* create_files_descriptors() {
  files_struct* files = kcalloc(1, sizeof(files_struct));
  for (int i = 0; i < MAX_FD; ++i) {
    files->fd[i] = NULL;
  }
  
  files->lock = semaphore_alloc(1);
  return files;
}

/*
  NOTE: Process that always stays in the system, it is also responsible for
  killing zombie processes 
*/
struct process *_init_proc = NULL;
struct process *get_init_proc() {
  assert(_init_proc != NULL, "init proc is not defined yet");
  return _init_proc;
}

static struct process* create_process(struct process* parent, char* name, struct pdirectory* pdir) {
  lock_scheduler();

  struct process* proc = kcalloc(1, sizeof(struct process));
  
  list_add(&proc->sibling, get_proc_list());
  proc->pid = next_pid++;
  proc->gid = proc->pid;
  
  atomic_set(&proc->thread_count, 0);
  proc->files = create_files_descriptors();
  proc->name = strdup(name);
  proc->exit_code = 0;
  proc->mm_mos = kcalloc(1, sizeof(mm_struct_mos));
  
  INIT_LIST_HEAD(&proc->childrens);
  INIT_LIST_HEAD(&proc->threads);
  INIT_LIST_HEAD(&proc->wait_chld.list);

  for (int i = 0; i < NSIG; ++i)
	  proc->sighand[i].sa_handler = sig_kernel_ignore(i + 1) ? SIG_IGN : SIG_DFL;

  proc->image_base = NULL;
  proc->image_size = 0;
  proc->va_dir = pdir? pdir : vmm_create_address_space();
  proc->pa_dir = vmm_get_physical_address(proc->va_dir, false);

  proc->fs = kcalloc(1, sizeof(fs_struct));
  if (parent) {
    proc->gid = parent->gid;
		proc->sid = parent->sid;
		
		memcpy(proc->fs, parent->fs, sizeof(fs_struct));
		list_add_tail(&proc->child, &parent->childrens);
	}
  
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
  struct thread* th = NULL;
  list_for_each_entry(th, &cur->threads, sibling) {
    vmm_unmap_address(cur->va_dir, th->kernel_esp - PMM_FRAME_SIZE);
  }
  */
  
  /*
  for (uint32_t i = 0; i < cur->thread_count; ++i) {
    struct thread* pThread = &cur->threads[i];
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

struct process* create_system_process(virtual_addr entry, char* name) {
  struct process *cur_process = get_current_process();
  struct process *proc = create_process(cur_process, name == NULL? "system" : name, vmm_get_directory());
  struct thread* th = kernel_thread_create(proc, entry);
  
  if (!th) {
    return false;
  }

  sched_push_queue(th);
  return proc;
}

struct process* create_elf_process(struct process* parent, char* path) {
  struct process* proc = create_process(parent, path, NULL);
  struct thread* th = user_thread_create(proc);

  if (!th || !proc) {
    assert_not_reached("Thread or struct processwere not created properly", NULL);
  }

  sched_push_queue(th);
  return proc;
}

struct thread* get_current_thread() {
  return _current_thread;
}

struct process* get_current_process() {
  if (_current_thread == NULL)
    return NULL;
  return _current_thread->proc;
}

static mm_struct_mos* clone_mm_struct(mm_struct_mos* mm_parent) {
  mm_struct_mos* mm = kcalloc(1, sizeof(mm_struct_mos));
  memcpy(mm, mm_parent, sizeof(mm_struct_mos));
  return mm;
}

static files_struct *clone_file_descriptor_table(files_struct *fs_src) {
  files_struct *fs = kcalloc(1, sizeof(files_struct));
  
	memcpy(fs, fs_src, sizeof(files_struct));
  // NOTE: MQ 2019-12-30 Increasing file description usage when forking because child refers to the same one
  
  
  for (int i = 0; i < MAX_FD; ++i)
    if (fs_src->fd[i])
      atomic_inc(&fs_src->fd[i]->f_count);
      
  return fs;
}

extern void load_trap_frame(trap_frame *);
extern void set_eax(int32_t v);

int32_t dup2(int oldfd, int newfd) {
  struct process *current_process = get_current_process();
  current_process->files->fd[newfd] = current_process->files->fd[oldfd];
  return newfd;
}

static void child_return_fork() {
  enable_interrupts();
  interruptdone(IRQ0);
  // needs to be last as previous funcion invokacions can spoil eax
  set_eax(0);
  return;
}

// probably it's better to rename the kernel fork to spawn
pid_t process_fork(struct process* parent) {
  lock_scheduler();

  log("Task: Fork from %s(p%d)", parent->name, parent->pid);
  trap_frame stf;
  load_trap_frame(&stf);

  bool is_kernel = vmm_is_kernel_directory(parent->va_dir);
  
  struct process* proc = kcalloc(1, sizeof(struct process));
  proc->pid = next_pid++;
  proc->gid = parent->gid;
  proc->sid = parent->sid;
  proc->parent = parent;
  proc->tty = parent->tty;
  proc->name = strdup(parent->name);
  proc->mm_mos = clone_mm_struct(parent->mm_mos);
  memcpy(&proc->sighand, &parent->sighand, sizeof(parent->sighand));
  
  INIT_LIST_HEAD(&proc->wait_chld.list);
  INIT_LIST_HEAD(&proc->childrens);
  INIT_LIST_HEAD(&proc->threads);

  list_add(&proc->child, &parent->childrens);
  list_add(&proc->sibling, get_proc_list());

	proc->fs = kcalloc(1, sizeof(fs_struct));
	memcpy(proc->fs, parent->fs, sizeof(fs_struct));

  proc->files = clone_file_descriptor_table(parent->files);
  proc->va_dir = is_kernel? parent->va_dir : vmm_fork(parent->va_dir);
  proc->pa_dir = vmm_get_physical_address(proc->va_dir, false);
  
  struct thread *parent_thread = get_current_thread();
  assert(
    parent_thread->proc->pid == parent->pid, 
    "Current thread doesn't belong to the process being forked"
  );

  // penging and blocked signals are not inherited
  struct thread *th = thread_create(proc, NULL, 0);

  /*
    NOTE: our goal is to make a separate struct thread that returns from process_fork method
    with same registers and stack (but different return value %eax = 0)
    
    To do this, we clone the parents stack and adjust its values a little bit so it can be 
    picked up and run by the scheduler
  */
  // ebp + 8 means that we skip return address, it will be put to switch stack frame
  int stack_size = parent_thread->kernel_esp - (stf.ebp + 8);
  th->esp = th->kernel_esp - stack_size;
  memcpy(th->esp, stf.ebp + 8, stack_size);
  
  th->esp = th->esp - sizeof(trap_frame);
  int prev_frame_size = parent_thread->kernel_esp - *((uint32_t*)stf.ebp);
  stf.ebp = th->kernel_esp - prev_frame_size;
  stf.return_address = stf.eip;
  stf.eip = child_return_fork;

  memcpy((char*)th->esp, &stf, sizeof(trap_frame));
  
  // NOTE: equal virtual address, but different physical!
	th->user_esp = parent_thread->user_esp; 

  sched_push_queue(th);
  unlock_scheduler();

  return proc->pid;
}

int32_t setpgid(pid_t pid, pid_t pgid) {
  struct process *current_process = get_current_process();
  struct process *p = !pid ? current_process : find_process_by_pid(pid);
  struct process *l = !pgid ? p : find_process_by_pid(pgid);

  if (l->sid != p->sid)
    return -1;

  p->gid = l->pid;
  return 0;
}

int count_array_of_pointers(void *arr) {
	if (!arr)
		return 0;

	const int32_t *a = arr;
	for (; *a; a++)
		;
	return a - (int32_t *)arr;
}

int32_t process_execve(const char *path, char *const argv[], char *const envp[]) {
  log("Task: Exec %s", path);

  int argv_length = count_array_of_pointers(argv); //?
	char **kernel_argv = kcalloc(argv_length, sizeof(char *));
	for (int i = 0; i < argv_length; ++i) {
		int ilength = strlen(argv[i]);
		kernel_argv[i] = kcalloc(ilength + 1, sizeof(char));
		memcpy(kernel_argv[i], argv[i], ilength);
	}
}

bool initialise_multitasking(virtual_addr entry) {
  INIT_LIST_HEAD(&all_threads);

  sched_init();

  struct process* parent = create_process(NULL, "init",  vmm_get_directory());
  assert(parent->pid == 0);
  _init_proc = parent;

  _current_thread = kernel_thread_create(parent, entry);
  sched_push_queue(_current_thread);

  /* register isr */
  old_pic_isr = getvect(IRQ0);
  /* 
    TODO: SA: scheduler and other IRQ are hanled differently and differnt values 
    could be put on stack it lead to a problem with imterpteting register values
    so probably it's better to unify it
  */
  setvect(IRQ0, scheduler_isr, I86_IDT_DESC_PRESENT | I86_IDT_DESC_BIT32);

  start_kernel_task(_current_thread);
  assert_not_reached();
}