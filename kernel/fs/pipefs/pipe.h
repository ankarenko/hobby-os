#ifndef KERNEL_FS_PIPE_H
#define KERNEL_FS_PIPE_H

#include "kernel/fs/vfs.h"
#include "kernel/locking/semaphore.h"
#include "kernel/util/circular_buffer.h"

#define PIPE_SIZE 256

struct pipe {
	struct circular_buf_t *buf;
	struct semaphore *mutex;

  uint32_t files;
	uint32_t readers;
	uint32_t writers;
};

extern struct vfs_file_operations pipe_fops;
extern struct vfs_super_operations pipe_super_operations;

int32_t do_pipe(int32_t *fd);

#endif
