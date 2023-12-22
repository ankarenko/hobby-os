#include "kernel/fs/vfs.h"

#include "kernel/fs/devfs/devfs.h"

static int devfs_mknod(struct vfs_inode *dir, struct vfs_dentry *dentry, int mode, dev_t dev) {
  struct vfs_inode *i = devfs_get_inode(dir->i_sb, mode);
  uint32_t current_seconds = get_seconds(NULL);
  i->i_ctime.tv_sec = current_seconds;
  i->i_atime.tv_sec = current_seconds;
  i->i_mtime.tv_sec = current_seconds;
  i->i_rdev = dev;

  dentry->d_inode = i;
  dir->i_mtime.tv_sec = current_seconds;
  return 0;
}

static struct vfs_inode *devfs_create_inode(struct vfs_inode *dir, struct vfs_dentry *dentry, mode_t mode) {
  return devfs_get_inode(dir->i_sb, mode);
}

struct vfs_inode_operations devfs_file_inode_operations = {};

struct vfs_inode_operations devfs_dir_inode_operations = {
  .create = devfs_create_inode,
  .mknod = devfs_mknod,
  //.rename = generic_memory_rename,
};

struct vfs_inode_operations devfs_special_inode_operations = {};