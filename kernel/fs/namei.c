#include "kernel/fs/vfs.h"
#include "kernel/proc/task.h"
#include "kernel/include/errno.h"
#include "kernel/include/fcntl.h"
#include "kernel/memory/malloc.h"
#include "kernel/include/limits.h"
#include "kernel/util/path.h"
#include "kernel/include/list.h"

static void vfs_build_path_backward_internal(struct vfs_dentry *dentry, char *path) {
  if (dentry->d_parent) {
    vfs_build_path_backward_internal(dentry->d_parent, path);
    int len = strlen(path);
    int dlen = strlen(dentry->d_name);

    if (len == 0 || path[len - 1] != '/') {
      memcpy(path + len, "/", 1);
      memcpy(path + len + 1, dentry->d_name, dlen);
      path[len + 1 + dlen] = 0;
    } else {
      memcpy(path + len, dentry->d_name, dlen);
      path[len + dlen] = 0;
    }
  } else
    strcpy(path, "/");
}
void vfs_build_path_backward(struct vfs_dentry *dentry, char *path) {
  vfs_build_path_backward_internal(dentry, path);
  char *tmp;
  if (!simplify_path(path, &tmp)) {
    warn("unable to simplify path");
  }
  int tmp_len = strlen(tmp);
  memcpy(path, tmp, tmp_len);
  path[tmp_len] = '\0';
  kfree(tmp);
}

static void absolutize_path_from_process(const char *path, char **abs_path) {
  if (path[0] != '/') {
    struct process *current_process = get_current_process();
    *abs_path = kcalloc(MAXPATHLEN, sizeof(char));
    vfs_build_path_backward(current_process->fs->d_root, *abs_path);
    strcat(*abs_path, "/");
    strcat(*abs_path, path);
  } else
    *abs_path = path;
}

int vfs_unlink(const char *path, int flag) {
  struct process *cur_proc = get_current_process();

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