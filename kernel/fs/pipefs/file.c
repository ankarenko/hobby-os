#include "kernel/fs/pipefs/pipe.h"

#include "kernel/util/debug.h"
#include "kernel/util/fcntl.h"
#include "kernel/util/errno.h"

static int32_t pipe_read(struct vfs_file* file, uint8_t* buffer, uint32_t length, off_t ppos) {
  if (file->f_flags & O_WRONLY)
		return -EINVAL;

	struct pipe *p = file->f_dentry->d_inode->i_pipe;

  int i = 0;
	semaphore_up(p->mutex);
  for (int i = 0; i < length; ++i) {
    if (circular_buf_get(p->buf, buffer + i) < 0) {
      break;
    }
  }
	semaphore_down(p->mutex);

  return i;
}

static uint32_t pipe_write(struct vfs_file *file, const char *buf, size_t count, off_t ppos) {
  if (file->f_flags & O_RDONLY)
		return -EINVAL;

	struct pipe *p = file->f_dentry->d_inode->i_pipe;

  semaphore_up(p->mutex);
  for (int i = 0; i < count; ++i) {
    circular_buf_put(p->buf, buf[i]);
  }
  semaphore_down(p->mutex);
  return count;
}

static int pipe_release(struct vfs_inode *inode, struct vfs_file *file) {
  struct pipe *p = inode->i_pipe;

  semaphore_down(p->mutex);
  p->files--;
  switch (file->f_flags) {
    case O_RDONLY:
      p->readers--;
      break;

    case O_WRONLY:
      p->writers--;
      break;

    default:
      assert_not_implemented();
      break;
  }
  semaphore_up(p->mutex);

  if (!p->files && !p->writers && !p->readers) {
		inode->i_pipe = NULL;
		circular_buf_free(p->buf);
    semaphore_free(p->mutex);
		kfree(p);
	}

  return 0;
}

static int pipe_open(struct vfs_inode *inode, struct vfs_file *file) {
  struct pipe *p = inode->i_pipe;

  semaphore_down(p->mutex);
  switch (file->f_flags) {
    case O_RDONLY:
      p->readers++;
      break;

    case O_WRONLY:
      p->writers++;
      break;

    default:
      assert_not_implemented();
      break;
  }
  semaphore_up(p->mutex);
  return 0;
}

struct vfs_file_operations pipe_fops = {
  .read = pipe_read,
  .write = pipe_write,
  .open = pipe_open,
  .release = pipe_release,
};