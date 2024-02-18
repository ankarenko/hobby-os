#include "kernel/fs/poll.h"
#include "kernel/proc/task.h"

static void poll_table_free(struct poll_table *pt) {
	struct poll_table_entry *iter, *next;

	list_for_each_entry_safe(iter, next, &pt->list, sibling) {
		list_del(&iter->wait.sibling);
		list_del(&iter->sibling);
		kfree(iter);
	}
	kfree(pt);
}

void poll_wait(struct vfs_file *file, struct wait_queue_head *wh, struct poll_table *pt) {
  struct poll_table_entry *pe = kcalloc(sizeof(struct poll_table_entry), 1);
	pe->file = file;
	pe->wait.callback = thread_wake;
	pe->wait.th = get_current_thread();
	list_add_tail(&pe->wait.sibling, &wh->list);
	list_add_tail(&pe->sibling, &pt->list);
}

int do_poll(struct pollfd *fds, uint32_t nfds, int timeout) {
  int32_t nr;

  struct thread *current_thread = get_current_thread();

  while (true) {
    // define it inside, because f_op->poll with modify it
    struct poll_table *pt = kcalloc(sizeof(struct poll_table), 1);
    INIT_LIST_HEAD(&pt->list);

    nr = 0;
    for (uint32_t i = 0; i < nfds; ++i) {
      struct pollfd *pfd = &fds[i];

      if (pfd->fd >= 0) {
        struct vfs_file *f = current_thread->proc->files->fd[pfd->fd];

        pfd->revents = f->f_op->poll(f, pt);
        if (pfd->events & pfd->revents)
          nr++;
      } else
        pfd->revents = 0;
    }

    if (nr > 0) {
      poll_table_free(pt);
      break;
    }

    thread_update(current_thread, THREAD_WAITING);
    schedule();
    
    poll_table_free(pt);
  }
  return nr;
}
