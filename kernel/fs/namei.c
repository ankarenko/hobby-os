#include "kernel/util/fcntl.h"
#include "kernel/util/errno.h"
#include "kernel/proc/task.h"
#include "kernel/util/list.h"

#include "kernel/fs/vfs.h"

int vfs_unlink(const char *path, int flag) {
  struct process* cur_proc = get_current_process();

	int ret = vfs_open(path, O_RDONLY);
	
  if (ret >= 0) {
		struct vfs_file *file = cur_proc->files->fd[ret];
		if (!file)
			ret = -EBADF;
		else if (flag & AT_REMOVEDIR && file->f_dentry->d_inode->i_mode & S_IFREG)
			ret = -ENOTDIR;
		else {
			struct vfs_inode *dir = file->f_dentry->d_parent->d_inode;
			if (dir->i_op && dir->i_op->unlink)
				ret = dir->i_op->unlink(dir, file->f_dentry->d_name);
			list_del(&file->f_dentry->d_sibling);
		}
		vfs_close(ret);
	}
  
	return ret;
}