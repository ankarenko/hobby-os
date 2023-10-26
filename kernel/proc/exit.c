#include "kernel/memory/malloc.h"
#include "kernel/proc/task.h"
#include "kernel/memory/vmm.h"

bool free_heap(mm_struct_mos* mm) {
  uint32_t frames = div_ceil(mm->brk, PMM_FRAME_SIZE);
  //pmm_free_frames();
}

bool exit_process(process* proc) {
  // free image
  if (proc->mm_mos) {
    for (
      virtual_addr vaddr = proc->mm_mos->heap_start; 
      vaddr < proc->mm_mos->brk; 
      vaddr += PMM_FRAME_SIZE
    ) {
      physical_addr paddr = vmm_get_physical_address(vaddr, false);
      pmm_free_frame(paddr);
      vmm_unmap_address(vaddr);
    }
  }

  list_del(&proc->proc_sibling);
  
  kfree(proc->mm_mos);
  kfree(proc->path);
  kfree(proc);

  return true;
}

// TODO: bug, when kill 2 times and then create
bool exit_thread(thread* th) {
  bool is_user = th->user_esp;
  process* parent = th->parent;
  //vmm_load_usertable(parent->pa_dir);
  
  
  // to be able to deduct physical address from a virtual one
  // we need to switch to a different address space
  // propably it's better to make recursive trick for 1022 index 
  // of a page directory
  if (is_user) {
    pmm_load_PDBR(parent->pa_dir);
  }

  
  kfree(th->kernel_esp - KERNEL_STACK_SIZE);
  kfree(th);
  //vmm_unmap_address(th->parent->page_directory, th->kernel_esp - PMM_FRAME_SIZE);
  
  parent->thread_count -= 1;
  list_del(&th->th_sibling);

  // clear userstack
  if (th->user_esp) {
    // parent->mm->heap_end - beggining of the stack
    uint32_t frames = div_ceil(USER_STACK_SIZE, PMM_FRAME_SIZE);
    physical_addr paddr = vmm_get_physical_address(th->virt_ustack_bottom, false);
    pmm_free_frames(paddr, frames);
    
    for (int i = 0; i < frames; ++i) {
      vmm_unmap_address(th->virt_ustack_bottom + i * PAGE_SIZE);
    }
  }
  
  // if there are no threads, then exit process
  if (parent->thread_count == 0) {
    exit_process(parent);
  }
  return true;
}

void garbage_worker_task() {
  while (1) {
    thread* th = pop_next_thread_to_terminate();

    if (th == NULL) {
      thread_sleep(300);
      continue;
    }

    exit_thread(th);
  }
}