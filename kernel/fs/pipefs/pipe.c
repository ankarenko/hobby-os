#include "kernel/fs/pipefs/pipe.h"
#include "kernel/include/fcntl.h"
#include "kernel/util/debug.h"
#include "kernel/proc/task.h"

int32_t do_pipe(int32_t *fd) {
  struct vfs_inode *inode = pipe_super_operations.alloc_inode(NULL);
  struct vfs_dentry *dentry = alloc_dentry(NULL, "pipe");
  dentry->d_inode = inode;

  struct vfs_file *f1 = alloc_vfs_file();
	f1->f_flags = O_RDONLY;
	f1->f_op = &pipe_fops;
	f1->f_dentry = dentry;

	struct vfs_file *f2 = alloc_vfs_file();
	f2->f_flags = O_WRONLY;
	f2->f_op = &pipe_fops;
	f2->f_dentry = dentry;

  struct process *current_process = get_current_process();

  int32_t ufd1 = find_unused_fd_slot(0);
	current_process->files->fd[ufd1] = f1;
	fd[0] = ufd1;

	int32_t ufd2 = find_unused_fd_slot(0);
	current_process->files->fd[ufd2] = f2;
	fd[1] = ufd2;

  return 0;
}