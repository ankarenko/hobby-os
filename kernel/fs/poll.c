#include "kernel/fs/poll.h"

int do_poll(struct pollfd *fds, uint32_t nfds, int timeout) {

}

void poll_wait(struct vfs_file *file, struct wait_queue_head *wh, struct poll_table *pt) {

}

void poll_wakeup(struct thread *t) {

}