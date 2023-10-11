#include "../memory/malloc.h"
#include "./task.h"

bool exit_process(process* proc) {
  list_del(&proc->proc_sibling);
  kfree(proc);
  return true;
}

// TODO: bug, when kill 2 times and then create
bool exit_thread(thread* th) {
  kfree(th->kernel_esp - PMM_FRAME_SIZE);
  kfree(th);
  //vmm_unmap_address(th->parent->page_directory, th->kernel_esp - PMM_FRAME_SIZE);
  
  process* parent = th->parent;
  parent->thread_count -= 1;
  list_del(&th->th_sibling);

  if (th->user_esp) {
    
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