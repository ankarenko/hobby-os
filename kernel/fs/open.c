#include <stddef.h>

#include "kernel/util/errno.h"
#include "kernel/proc/task.h"
#include "kernel/util/debug.h"
#include "kernel/util/string/string.h"

#include "kernel/fs/vfs.h"

struct vfs_dentry *alloc_dentry(struct vfs_dentry *parent, char *name) {
	struct vfs_dentry *d = kcalloc(1, sizeof(struct vfs_dentry));
	d->d_name = strdup(name);
	d->d_parent = parent;
	INIT_LIST_HEAD(&d->d_subdirs);

	if (parent)
		d->d_sb = parent->d_sb;

	return d;
}

static int32_t find_unused_fd_slot() {
  process* proc = get_current_process();

	for (uint32_t i = 0; i < MAX_FD; ++i)
		if (!proc->files->fd[i])
			return i;

	return -1;
}

static struct vfs_file *get_empty_file() {
	struct vfs_file *file = kcalloc(1, sizeof(struct vfs_file));
  
  //file->f_maxcount = INT_MAX;
	//atomic_set(&file->f_count, 1);
	return file;
}

int32_t vfs_close(int32_t fd) {
  /*
  process* proc = get_current_process();
  vfs_file* file = proc->files->fd[fd];

  if (file)
    if (file_systems[file->device_id - 'a'])
      return file_systems[file->device_id - 'a']->fop.close(file);
      */
}

int vfs_fstat(int32_t fd, struct stat* stat) {
  /*
  process* proc = get_current_process();
  vfs_file* file = proc->files->fd[fd];
  stat->st_size = file->file_length;
  stat->st_mode = S_IFCHR;
  stat->st_blksize = 512;
  //stat->st_blocks = 1;
  */
  return 1;
}

int32_t vfs_delete(const char* fname) {
  /*
  if (!fname)
    return -ENOENT;

  unsigned char device = 'a';
  if (file_systems[device - 'a']) {
    return file_systems[device - 'a']->delete(fname);
  }
  */
}

int32_t vfs_mkdir(const char* dir_path) {
  /*
  if (!dir_path)
    return -ENOENT;

  unsigned char device = 'a';
  if (file_systems[device - 'a']) {
    return file_systems[device - 'a']->mkdir(dir_path);
  }
  */
}

int32_t vfs_open(const char* path, int32_t flags, ...) {
  int fd = find_unused_fd_slot(0);
	mode_t mode = 0;
  /*
	if (flags & O_CREAT)
	{
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}
  */

	struct nameidata nd;
	int ret = vfs_jmp(&nd, path/*, flags, mode*/);
	if (ret < 0)
		return ret;

	struct vfs_file *file = get_empty_file();
	file->f_dentry = nd.dentry;
	//file->f_vfsmnt = nd.mnt;
	//file->f_flags = flags;
	//file->f_mode = OPEN_FMODE(flags);
	file->f_op = nd.dentry->d_inode->i_fop;
  /*
	if (file->f_mode & FMODE_READ)
		file->f_mode |= FMODE_CAN_READ;
	if (file->f_mode & FMODE_WRITE)
		file->f_mode |= FMODE_CAN_WRITE;
  */
	if (file->f_op && file->f_op->open) {
		ret = file->f_op->open(nd.dentry->d_inode, file);
		if (ret < 0) {
			kfree(file);
			return ret;
		}
	}

	//atomic_inc(&file->f_dentry->d_inode->i_count);
  process* cur_proc = get_current_process();
	cur_proc->files->fd[fd] = file;
	return fd;
}