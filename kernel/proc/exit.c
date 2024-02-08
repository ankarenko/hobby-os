#include "kernel/fs/vfs.h"
#include "kernel/memory/malloc.h"
#include "kernel/memory/vmm.h"
#include "kernel/proc/task.h"
#include "kernel/util/errno.h"
#include "kernel/util/debug.h"
#include "kernel/util/math.h"
#include "kernel/util/types.h"

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
        vaddr += PMM_FRAME_SIZE) {
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

  // kfree(proc->name);
  kfree(proc->fs);
  proc->fs = NULL;

  proc->flags |= SIGNAL_TERMINATED; 
  proc->flags &= ~(SIGNAL_CONTINUED | SIGNAL_STOPED); // ?
  proc->state |= EXIT_ZOMBIE;

  /*
    For a process to be completely released it needs to be 'waited' by its parent,
    othewise a process descriptor will stay in the memory with ZOMBIE_STATE assigned to it

    if a parent exits, childs becomes inherited from init process (pid 0) to make sure
    they are 'waited'
  */
  struct process *iter, *next = NULL;
  struct process *init_proc = get_init_proc();
  list_for_each_entry_safe(iter, next, &proc->childrens, child) {
    iter->parent = init_proc;
    list_del(&iter->child);
    list_add(&iter->child, &init_proc->childrens);
    iter->parent = init_proc;
  }
  log("done exit process");
}

// TODO: bug, when kill 2 times and then create
// NOTE: threads are not allowed to create threads right now.
int exit_thread(struct thread *th) {
  log("exit thread");
  lock_scheduler();

  struct process *parent = th->proc;

  assert(th->state == THREAD_TERMINATED);
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

  //thread_update(th, THREAD_TERMINATED);
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
  if (parent->parent != NULL) {
    log("waking up parent with id %d", parent->parent->pid);
    //parent->parent->tty->pgrp = parent->parent->gid;
    wake_up(&parent->parent->wait_chld);
  } else  {
    log("no parent to wake up");
  }

  return true;
}

struct process *garbage_worker = NULL;

struct process *get_garbage_worker() {
  return garbage_worker;
}

void garbage_worker_task() {
  garbage_worker = get_current_process();
  
  while (1) {
    struct thread *th = pop_next_thread_to_terminate();

    if (th == NULL) {
      wait_until_wakeup(&get_current_process()->wait_chld);
      continue;
    }
    
    /*
    assert(th->state == THREAD_TERMINATED);
    if (atomic_read(&th->lock_counter) != 0) {
      th->postpone_kill = true;
    }
    */

    exit_thread(th);
  }
}

void do_exit(int code) {
  lock_scheduler();
  
  thread_signal(get_current_thread()->tid, SIGKILL);

  unlock_scheduler();
  schedule();
}

/*
 * Return:
 * - 1 if found a child process which status is available
 * - 0 if found at least one child process which status is unavailable and WNOHANG in options
 * - < 0 otherwise
 */
int32_t do_wait(id_type_t idtype, id_t id, struct infop *infop, int options) {
  struct process *current_process = get_current_process();
  struct process *current_thread = get_current_thread();

  DEFINE_WAIT(wait);
  list_add_tail(&wait.sibling, &current_process->wait_chld.list);
  log("Process: Wait %s(p%d) with idtype=%d id=%d options=%d", current_process->name, current_process->pid, idtype, id, options);
  struct process *pchild = NULL;
  bool child_exist = false;

  while (true) {
    struct process *iter, *tmp;
    // NOTE: careful, other routine can change current_process->childrens
    // maybe it makes sense to disable schedule
    lock_scheduler();
    list_for_each_entry_safe(iter, tmp, &current_process->childrens, child) {
      if (!(
        (idtype == P_PID && iter->pid == id) ||
        (idtype == P_PGID && iter->gid == id) ||
        (idtype == P_ALL))
      ) {
        continue;
      }
        
      child_exist = true;
      if ((options & WEXITED && (iter->flags & SIGNAL_TERMINATED || iter->flags & EXIT_TERMINATED)) ||
          (options & WSTOPPED && iter->flags & SIGNAL_STOPED) ||
          (options & WCONTINUED && iter->flags & SIGNAL_CONTINUED)) {
        pchild = iter;
        break;
      }
    }
    unlock_scheduler();

    if (pchild || options & WNOHANG)
      break;
    
    thread_update(current_thread, THREAD_WAITING);
    schedule();
  }
  list_del(&wait.sibling);

  int32_t ret = -1;
	if (pchild) {
		infop->si_pid = pchild->pid;
		infop->si_signo = SIGCHLD;
		infop->si_status = pchild->caused_signal;
		if (pchild->flags & SIGNAL_STOPED)
			infop->si_code = CLD_STOPPED;
		else if (pchild->flags & SIGNAL_CONTINUED)
			infop->si_code = CLD_CONTINUED;
		else if (pchild->flags & SIGNAL_TERMINATED)
			infop->si_code = sig_kernel_coredump(pchild->caused_signal) ? CLD_DUMPED : CLD_KILLED;
		else if (pchild->flags & EXIT_TERMINATED) {
			infop->si_code = CLD_EXITED;
			infop->si_status = pchild->exit_code;
		}
		// NOTE: MQ 2020-11-25
		// After waiting for terminated child, we remove it from parent
		// the next waiting time, we don't find the same one again
		if (pchild->state & EXIT_ZOMBIE != 0) {
      list_del(&pchild->sibling);
      list_del(&pchild->child);
      kfree(pchild); // delete zombie process descriptor
    }
		ret = 1;
	}
	else
		ret = child_exist && (options & WNOHANG) ? 0 : -ECHILD;

  log("Process: Wait %s(p%d) done", current_process->name, current_process->pid);
  return ret;
}
