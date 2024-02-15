#include "kernel/fs/pipefs/pipe.h"
#include "kernel/locking/semaphore.h"

static struct pipe *alloc_pipe() {
	struct pipe *p = kcalloc(1, sizeof(struct pipe));
	p->files = 0;
	p->readers = 0;
	p->writers = 0;

  p->mutex = semaphore_alloc(1);

	char *buf = kcalloc(PIPE_SIZE, sizeof(char));
	p->buf = circular_buf_init(buf, PIPE_SIZE);

	return p;
}

static struct vfs_inode *ext2_alloc_inode(struct vfs_superblock *sb) {
  struct pipe *p = alloc_pipe();
  p->readers = p->writers = 1;
	p->files = 2;

	struct vfs_inode *inode = init_inode();

	inode->i_mode = S_IFIFO;
	inode->i_atime.tv_sec = get_seconds(NULL);
	inode->i_ctime.tv_sec = get_seconds(NULL);
	inode->i_mtime.tv_sec = get_seconds(NULL);
	inode->i_pipe = p;

	//sema_init(&inode->i_sem, 1);
	
  inode->i_fop = &pipe_fops;

	return inode;
}

struct vfs_super_operations pipe_super_operations = {
	.alloc_inode = ext2_alloc_inode
};