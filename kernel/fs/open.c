#include <stdarg.h>
#include <stddef.h>

#include "kernel/fs/vfs.h"
#include "kernel/proc/task.h"
#include "kernel/util/debug.h"
#include "kernel/util/errno.h"
#include "kernel/util/fcntl.h"
#include "kernel/util/string/string.h"

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
  struct process* proc = get_current_process();

  for (uint32_t i = 0; i < MAX_FD; ++i)
    if (!proc->files->fd[i])
      return i;

  return -1;
}

struct vfs_file *alloc_vfs_file() {
  struct vfs_file *file = kcalloc(1, sizeof(struct vfs_file));

  // file->f_maxcount = INT_MAX;
  atomic_set(&file->f_count, 1);
  return file;
}

int32_t vfs_close(int32_t fd) {
  struct process* cur_proc = get_current_process();
  struct vfs_file *file = cur_proc->files->fd[fd];

  semaphore_down(cur_proc->files->lock);

  int ret = 0;
  if (file) {
    atomic_dec(&file->f_count);
    cur_proc->files->fd[fd] = NULL;
    if (!atomic_read(&file->f_count)) {
			if (file->f_op && file->f_op->release)
				ret = file->f_op->release(file->f_dentry->d_inode, file);
			kfree(file);
		}
  } else {
    ret = -EBADF;
  }

  semaphore_up(cur_proc->files->lock);

  return ret;
}

static void generic_fillattr(struct vfs_inode *inode, struct kstat *stat) {
  // stat->st_dev = inode->i_sb->s_dev;
  stat->st_ino = inode->i_ino;
  stat->st_mode = inode->i_mode;
  stat->st_nlink = inode->i_nlink;
  stat->st_uid = inode->i_uid;
  stat->st_gid = inode->i_gid;
  stat->st_rdev = inode->i_rdev;
  stat->st_atim = inode->i_atime;
  stat->st_mtim = inode->i_mtime;
  stat->st_ctim = inode->i_ctime;
  stat->st_size = inode->i_size;
  stat->st_blocks = inode->i_blocks;
  stat->st_blksize = inode->i_blksize;
}

static int do_getattr(/*struct vfs_mount *mnt, */ struct vfs_dentry *dentry, struct kstat *stat) {
  struct vfs_inode *inode = dentry->d_inode;
  /*
        if (inode->i_op->getattr)
                return inode->i_op->getattr(mnt, dentry, stat);
  */

  generic_fillattr(inode, stat);
  /*
        if (!stat->st_blksize)
        {
                struct vfs_superblock *s = inode->i_sb;
                unsigned blocks;
                blocks = (stat->st_size + s->s_blocksize - 1) >> s->s_blocksize_bits;
                stat->st_blocks = (s->s_blocksize / BYTES_PER_SECTOR) * blocks;
                stat->st_blksize = s->s_blocksize;
        }
  */
  return 0;
}

int vfs_fstat(int32_t fd, struct kstat *stat) {
  struct process* cur_proc = get_current_process();
  struct vfs_file *file = cur_proc->files->fd[fd];
  if (fd < 0 || !file)
    return -EBADF;

  return do_getattr(/*file->f_vfsmnt,*/ file->f_dentry, stat);
}

int vfs_mknod(const char *path, int mode, int32_t dev) {
  char *dir = NULL;
  char *name = NULL;
  strlsplat(path, strliof(path, "/"), &dir, &name);

  struct nameidata nd;
  int ret = vfs_jmp(&nd, dir, 0, S_IFDIR);
  if (ret < 0)
    return ret;

  struct vfs_dentry *d_child = alloc_dentry(nd.dentry, name);
  ret = nd.dentry->d_inode->i_op->mknod(nd.dentry->d_inode, d_child, mode, dev);
  if (ret < 0)
    return ret;

  list_add_tail(&d_child->d_sibling, &nd.dentry->d_subdirs);

  return ret;
}

int generic_memory_readdir(struct vfs_file *file, struct dirent **dirent) {
  struct vfs_dentry *dentry = file->f_dentry;
  int entries_size = list_count(&dentry->d_subdirs);

  *dirent = kcalloc(entries_size, sizeof(struct dirent));

  struct dirent *idrent = *dirent;
  struct vfs_dentry *iter;
  list_for_each_entry(iter, &dentry->d_subdirs, d_sibling) {
    int len = strlen(iter->d_name);
    memcpy(idrent->d_name, iter->d_name, len);
    idrent->d_reclen = sizeof(struct dirent); // + len + 1;
    idrent->d_ino = iter->d_inode->i_ino;
    idrent->d_type = iter->d_inode->i_mode;
    idrent = (struct dirent *)((char *)idrent + idrent->d_reclen);
  }
  return entries_size;
}

int32_t vfs_open(const char *path, int32_t flags, ...) {
  int fd = find_unused_fd_slot(0);
  mode_t mode = 0;

  if (flags & O_CREAT) {
    // TODO: need loop?
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
  }

  struct nameidata nd;
  int ret = vfs_jmp(&nd, path, flags, mode);
  if (ret < 0)
    return ret;

  struct vfs_file *file = alloc_vfs_file();
  file->f_dentry = nd.dentry;
  file->f_vfsmnt = nd.mnt;
  file->f_flags = flags;
  // file->f_mode = OPEN_FMODE(flags);
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

  // atomic_inc(&file->f_dentry->d_inode->i_count);
  struct process* cur_proc = get_current_process();
  cur_proc->files->fd[fd] = file;
  return fd;
}

struct vfs_dentry *vfs_search_virt_subdirs(struct vfs_dentry *dir, const char *name) {
  // look in subdirectories first
  struct vfs_dentry* iter = NULL;
  
  list_for_each_entry(iter, &dir->d_subdirs, d_sibling) {
    if (!strcmp(name, iter->d_name)) {
      return iter;
    }
  }
  
  return NULL;
}