#include "task.h"
#include "../memory/vmm.h"
#include "../cpu/hal.h"
#include "elf.h"

extern void jump_to_process(virtual_addr entry, virtual_addr stack);

static process _proc = {
	PROC_INVALID_ID,0,0,0,0
};

int32_t create_process(char* appname) {
  process* proc = get_current_process();
  thread* mthread = 0;
        
  struct Elf32_Layout layout = elf_load(appname);

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
  mthread->image_base = layout.base;
  mthread->image_size = layout.image_size;
  //memset (&mthread->frame, 0, sizeof (trapFrame));
  mthread->frame.eip = layout.entry;
  mthread->frame.flags = 0x200;  
  mthread->frame.esp = layout.stack;
  mthread->frame.ebp = mthread->frame.esp;

  return proc->id;
}

void execute_process() {
  process* proc = 0;
  virtual_addr entry_point = 0;
  virtual_addr proc_stack = 0;

  /* get running process */
  proc = get_current_process();
  if (proc->id == PROC_INVALID_ID || !proc->page_directory)
    return;

  /* get esp and eip of main thread */
  entry_point = proc->threads[0].frame.eip;
  proc_stack  = proc->threads[0].frame.esp;

  /* switch to process address space */
  disable_interrupts();
  physical_addr p_dir = vmm_get_physical_address(proc->page_directory, false);
  vmm_switch_pdirectory(p_dir);
  
  jump_to_process(entry_point, proc_stack);
}

/**
* Return current process
* \ret Process
*/
process* get_current_process() {
	return &_proc;
}
