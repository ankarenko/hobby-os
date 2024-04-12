#include "kernel/include/errno.h"
#include "kernel/include/fcntl.h"
#include "kernel/proc/task.h"
#include "kernel/util/debug.h"

#include "kernel/fs/vfs.h"

int32_t vfs_read(const char *path, char** res) {
  int ret  = -1;
  *res = NULL;

  int32_t fd;
	if ((fd = vfs_open(path, O_RDWR)) < 0)
		return fd;

  struct process* proc = get_current_process();
  struct vfs_file* file = proc->files->fd[fd];

  int32_t size = file->f_dentry->d_inode->i_size;
  file->f_pos = 0; // read from the beggining

	struct kstat *stat = kcalloc(1, sizeof(struct kstat));
  if ((ret = vfs_fstat(fd, stat)) < 0)
    goto clean;

	char* buf = kcalloc(stat->st_size + 1, sizeof(char));

  if ((ret = vfs_fread(fd, buf, stat->st_size)) < 0) {
    kfree(buf);
    goto clean;
  }

  buf[stat->st_size] = '\0';
  *res = buf;
  
clean:
  kfree(stat);
	return 0;
}

int32_t vfs_fread(int32_t fd, char *buf, uint32_t count) {
  struct process* cur_proc = get_current_process();
	struct vfs_file* file = cur_proc->files->fd[fd];

  int ret = 0;
  if (S_ISDIR(file->f_dentry->d_inode->i_mode)) {
    ret = -EISDIR;
  } else if (fd < 0 || !file)
		ret = -EBADF;
	else if (file /*&& file->f_mode & FMODE_CAN_READ*/)
		ret = file->f_op->read(file, buf, count, file->f_pos);
  else {
    ret = -EINVAL;
  }

  return ret;
}

off_t vfs_generic_llseek(struct vfs_file *file, off_t offset, int whence) {
  struct process* cur_proc = get_current_process();
  enter_critical_section();
	struct vfs_inode *inode = file->f_dentry->d_inode;
	off_t foffset;

	if (whence == SEEK_SET)
		foffset = offset;
	else if (whence == SEEK_CUR)
		foffset = file->f_pos + offset;
	else if (whence == SEEK_END)
		foffset = inode->i_size + offset;
	else {
    return -EINVAL;
  }

	file->f_pos = foffset;

  leave_critical_section();
	return foffset;
}

off_t vfs_flseek(int32_t fd, off_t offset, int whence) {
  if (fd == -1)
    return -EBADF;

  struct process* proc = get_current_process();
  semaphore_down(proc->files->lock);
  enter_critical_section();
  struct vfs_file* file = proc->files->fd[fd];

  int ret = 0;

  if (fd < 0 || !file)
		ret = -EBADF;
	else if (file && file->f_op && file->f_op->llseek)
		ret = file->f_op->llseek(file, offset, whence);
  else 
    ret = -EINVAL;

  semaphore_up(proc->files->lock);
  leave_critical_section();
	return ret;
}

uint32_t vfs_fwrite(int32_t fd, char* buf, int32_t count) {
  if (fd < 0)
    return -EBADF;

  struct process* cur_proc = get_current_process();
  semaphore_down(cur_proc->files->lock);
  enter_critical_section();
  struct vfs_file *file = cur_proc->files->fd[fd];

  int ret = 0;

  if (!file) {
    ret = -ENOENT;
    goto exit;
  }

  if (S_ISDIR(file->f_dentry->d_inode->i_mode)) {
    ret = -EISDIR;
    goto exit;
  } else if (fd < 0 || !file) {
		ret = -EBADF;
    goto exit;
  }

	off_t ppos = file->f_pos;
  
	if (file->f_flags & O_APPEND)
		ppos = file->f_dentry->d_inode->i_size;
  
  ret = file->f_op->write(file, buf, count, ppos);
  
  /*
	if (file->f_mode & FMODE_CAN_WRITE)
		return file->f_op->write(file, buf, count, ppos);
  */
exit:
  semaphore_up(cur_proc->files->lock);
  leave_critical_section();
	return ret;
}