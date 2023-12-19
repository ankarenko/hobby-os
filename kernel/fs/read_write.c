#include "kernel/util/errno.h"
#include "kernel/util/fcntl.h"
#include "kernel/proc/task.h"
#include "kernel/util/debug.h"

#include "./vfs.h"

char *vfs_read(const char *path) {
  int32_t fd = vfs_open(path, O_RDWR);
	if (fd < 0)
		return NULL;

  process* proc = get_current_process();
  struct vfs_file* file = proc->files->fd[fd];
  int32_t size = file->f_dentry->d_inode->i_size;
  file->f_pos = 0; // read from the beggining

	struct kstat *stat = kcalloc(1, sizeof(struct kstat));
  vfs_fstat(fd, stat);
	char *buf = kcalloc(stat->st_size + 1, sizeof(char));
	vfs_fread(fd, buf, stat->st_size);
  buf[stat->st_size] = '\0';
	return buf;
}

int32_t vfs_fread(int32_t fd, char *buf, int32_t count) {
  process* cur_proc = get_current_process();
	struct vfs_file* file = cur_proc->files->fd[fd];
	if (fd < 0 || !file)
		return -EBADF;

	if (file /*&& file->f_mode & FMODE_CAN_READ*/)
		return file->f_op->read(file, buf, count, file->f_pos);

	return -EINVAL;
}

off_t vfs_flseek(int32_t fd, off_t offset, int whence) {
  process* proc = get_current_process();
  struct vfs_file* file = proc->files->fd[fd];

  if (!file)
    return -ENOENT;
  
  switch (whence) {
  case SEEK_SET:
    file->f_pos = offset;
    break;

  case SEEK_CUR:
    file->f_pos += offset;
    break;

  case SEEK_END:
    file->f_pos = file->f_dentry->d_inode->i_size + offset;
    break;
  
  default:
    return -EINVAL;
  }
  
  return file->f_pos;
}

ssize_t vfs_fwrite(int32_t fd, char* buf, int32_t count) {
  process* cur_proc = get_current_process();
  struct vfs_file *file = cur_proc->files->fd[fd];
	if (fd < 0 || !file)
		return -EBADF;

	off_t ppos = file->f_pos;
  
	if (file->f_flags & O_APPEND)
		ppos = file->f_dentry->d_inode->i_size;
  
  return file->f_op->write(file, buf, count, ppos);
  
  /*
	if (file->f_mode & FMODE_CAN_WRITE)
		return file->f_op->write(file, buf, count, ppos);
  */
	return -EINVAL;
}