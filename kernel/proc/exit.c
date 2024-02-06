#include "kernel/memory/malloc.h"
#include "kernel/proc/task.h"
#include "kernel/util/math.h"
#include "kernel/memory/vmm.h"
#include "kernel/util/debug.h"
#include "kernel/fs/vfs.h"

static void exit_mm(struct process *proc) {
	mm_struct_mos *iter, *next;
	/*
  list_for_each_entry_safe(iter, next, &proc->mm->mmap, vm_sibling) {
		if (!iter->vm_file)
			vmm_unmap_range(proc->pdir, iter->vm_start, iter->vm_end);

		list_del(&iter->vm_sibling);
		kfree(iter);
	}
  */
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
  kfree(proc->mm_mos);
  proc->mm_mos = NULL;
}

static void exit_files(struct process *proc) {
	for (int i = 0; i < MAX_FD; ++i) {
		struct vfs_file *file = proc->files->fd[i];

		if (file && atomic_read(&file->f_count) == 1) {
			
      if (file->f_op->release) {
        file->f_op->release(file->f_dentry->d_inode, file);
      }

			kfree(file);
			proc->files->fd[i] = 0;
		}
	}
  kfree(proc->files);
  proc->files = NULL;
}

static void exit_notify(struct process *proc) {
}

void exit_process(struct process *proc) {
  log("exit process %d", proc->pid);
  exit_mm(proc);
  exit_files(proc);
  exit_notify(proc);

  //kfree(proc->name);
  kfree(proc->fs);
  proc->fs = NULL;

  proc->state |= EXIT_ZOMBIE;

  /*
    For a process to be completely released it needs to be 'waited' by its parent,
    othewise a process descriptor will stay in the memory with ZOMBIE_STATE assigned to it
    
    if a parent exits, childs becomes inherited from init process (pid 0) to make sure
    they are 'waited'
  */
 /*
  struct process *iter, *next = NULL;
  struct process *init_proc = get_init_proc();
  list_for_each_entry_safe(iter, next, &proc->childrens, child) {
    iter->parent = init_proc;
    list_del(&iter->child);
    list_add(&iter->child, &init_proc->childrens);
  }
  */
  //kfree(proc); 
}

// TODO: bug, when kill 2 times and then create
// NOTE: threads are not allowed to create threads right now.
int exit_thread(struct thread* th) {
  log("exit thread");
  lock_scheduler();
  
  struct process* parent = th->proc;

  assert(atomic_read(&th->lock_counter) == 0, "freeing thread which holds locks is forbidden");
  log("Killing thread tid: %d that belongs to process with pid:%d", th->tid, parent->pid);
  assert(parent != NULL, "zombie threads are not allowed!");
  
  bool is_user = !vmm_is_kernel_directory(parent->va_dir);
  
  // to be able to deduct physical address from a virtual one
  // we need to switch to a different address space
  // propably it's better to make recursive trick for 1022 index 
  // of a page directory
  /*
  if (is_user) {
    pmm_load_PDBR(parent->pa_dir);
  }
  */

	thread_update(th, THREAD_TERMINATED);
	del_timer(&th->s_timer);
	
  kfree(th->kernel_esp - KERNEL_STACK_SIZE);
  atomic_dec(&parent->thread_count);
  list_del(&th->child);
  list_del(&th->sibling);
  
  // clear userstack
  if (is_user) {
    // parent->mm->heap_elock_schedund - beggining of the stack
    uint32_t frames = div_ceil(USER_STACK_SIZE, PMM_FRAME_SIZE);
    physical_addr paddr = vmm_get_physical_address(th->virt_ustack_bottom, false);
    pmm_free_frames(paddr, frames);
    
    for (int i = 0; i < frames; ++i) {
      vmm_unmap_address(th->virt_ustack_bottom + i * PAGE_SIZE);
    }
  }

  kfree(th);

  if (atomic_read(&parent->thread_count) == 0) {
    log("All threads are killed, killing process with pid: %d", parent->pid);
    exit_process(parent);
  }
  
  unlock_scheduler();

  parent->parent->tty->pgrp = parent->parent->gid;
  wake_up(&parent->parent->wait_chld);
  
  return true;
}

void garbage_worker_task() {
  while (1) {
    struct thread* th = pop_next_thread_to_terminate();

    if (th == NULL) {
      thread_sleep(1000);
      continue;
    }

    exit_thread(th);
  }
}

void do_exit(int code) {
  lock_scheduler();
  struct process *current_process = get_current_process();
  struct thread *current_thread = get_current_thread();
  
  log("process: exit %s(p%d)", current_process->name, current_process->pid);
	 
  if (!exit_thread(current_thread)) {
    err("exit: unable to exit trhead");
  }

  current_process->exit_code = code;

  unlock_scheduler();
  schedule();
}