#include "kernel/fs/vfs.h"

#include "kernel/devices/terminal.h"
#include "kernel/devices/vga.h"
#include "kernel/fs/char_dev.h"
#include "kernel/fs/ext2/ext2.h"
#include "kernel/memory/malloc.h"
#include "kernel/proc/task.h"
#include "kernel/system/time.h"
#include "kernel/util/debug.h"
#include "kernel/util/errno.h"
#include "kernel/util/fcntl.h"
#include "kernel/util/limits.h"
#include "kernel/util/string/string.h"
#include "kernel/fs/devfs/devfs.h"

static struct vfs_file_system_type* file_systems;
struct list_head vfsmntlist;

char* months[12] = {
    "jan",
    "feb",
    "mar",
    "apr",
    "may",
    "jun",
    "jul",
    "aug",
    "sep",
    "oct",
    "nov",
    "dec"};

int32_t vfs_ls(const char* path) {
  struct nameidata nd = {
      .dentry = 0,
      .mnt = 0};
  int32_t ret = 0;
  if ((ret = vfs_jmp(&nd, path, 0, 0)) != 0) {
    return ret;
  }

  //  check if dir
  if (!S_ISDIR(nd.dentry->d_inode->i_mode)) {
    return -ENOTDIR;
  }

  struct vfs_file* file = alloc_vfs_file();
  file->f_dentry = nd.dentry;

  struct dirent* dirs = NULL;

  uint32_t count = nd.dentry->d_inode->i_fop->readdir(file, &dirs);

  struct vfs_inode inode;

  for (int i = 0; i < count; ++i) {
    
    char name[12] = "           ";
    struct dirent* iter = &dirs[i];

    inode.i_ino = iter->d_ino;
    inode.i_sb = nd.dentry->d_inode->i_sb;

    if (nd.dentry->d_inode->i_sb->s_op->read_inode && inode.i_ino != 0) {
      nd.dentry->d_inode->i_sb->s_op->read_inode(&inode);

      memcpy(&name, iter->d_name, strlen(iter->d_name));
      if (!S_ISDIR(iter->d_type)) {
        bool is_char_dev = S_ISCHR(iter->d_type);
        if (is_char_dev) {
          kprintf(BLU"\n%s", name);
        } else {
          kprintf(RED"\n%s", name);
        }
        
        if (!is_char_dev) {
          struct time* created = get_time(inode.i_ctime.tv_sec);
          
          kprintf("   %d %s %d%d:%d%d",
              created->day,
              months[created->month - 1],
              created->hour / 10, created->hour % 10,
              created->minute / 10, created->minute % 10);

        
          kprintf("   %u bytes", inode.i_size);
        
          kfree(created);
        }
        kprintf(COLOR_RESET);
      } else {
        kprintf("\n%s", name);
      }
    } else {
      memcpy(&name, iter->d_name, strlen(iter->d_name));
      if (!S_ISDIR(iter->d_type)) {
        kprintf("\n%s", name);
      } else {
        kprintf(RED"\n%s"COLOR_RESET, name);
      }
    }
    
  }
  kfree(dirs);
  return count;
}

struct vfs_inode* init_inode() {
	struct vfs_inode *i = kcalloc(1, sizeof(struct vfs_inode));
	i->i_blocks = 0;
	i->i_size = 0;
	//semaphore_alloc(&i->i_sem, 1);
	return i;
}

int32_t vfs_cd(const char* path) {
  struct nameidata nd;
  int32_t ret = 0;
  if ((ret = vfs_jmp(&nd, path, 0, 0)) != 0) {
    return ret;
  }

  if (!S_ISDIR(nd.dentry->d_inode->i_mode)) {
    return -ENOTDIR;
  }

  struct process* cur_proc = get_current_process();
  cur_proc->fs->d_root = nd.dentry;
  cur_proc->fs->mnt_root = nd.mnt;
  return 0;
}

static struct vfs_file_system_type** find_filesystem(const char* name) {
  struct vfs_file_system_type** p;
  for (p = &file_systems; *p; p = &(*p)->next) {
    if (strcmp((*p)->name, name) == 0)
      break;
  }
  return p;
}

int register_filesystem(struct vfs_file_system_type* fs) {
  struct vfs_file_system_type** p = find_filesystem(fs->name);

  if (*p)
    return -EBUSY;
  else
    *p = fs;

  return 0;
}

int unregister_filesystem(struct vfs_file_system_type* fs) {
  struct vfs_file_system_type** p;
  for (p = &file_systems; *p; p = &(*p)->next) {
    if (strcmp((*p)->name, fs->name) == 0) {
      *p = (*p)->next;
      return 0;
    }
  }
  return -EINVAL;
}

struct vfs_mount* lookup_mnt(struct vfs_dentry* d) {
  struct vfs_mount* iter;
  list_for_each_entry(iter, &vfsmntlist, sibling) {
    if (!strcmp(iter->mnt_devname, d->d_sb->mnt_devname))
      return iter;
  }

  return NULL;
}

static void init_rootfs(struct vfs_file_system_type* fs_type, char* dev_name) {
  ext2_init_fs();

  struct vfs_mount* mnt = fs_type->mount(fs_type, dev_name, "/");
  list_add_tail(&mnt->sibling, &vfsmntlist);

  struct process* cur = get_current_process();

  cur->fs->d_root = mnt->mnt_root;
  cur->fs->mnt_root = mnt;
}

int vfs_jmp(struct nameidata* nd, const char* path, int32_t flags, mode_t mode) {
  char* simplified = NULL;
  if (!simplify_path(path, &simplified)) {
    return -EINVAL;
  }

  const char* cur = simplified;

  struct process* cur_proc = get_current_process();
  nd->mnt = cur_proc->fs->mnt_root;

  // check if root dir
  if (simplified[0] == '\/') {
    nd->dentry = cur_proc->fs->mnt_root->mnt_root;
    while ((char)(*cur) == '\/') {
      cur++;
    }
  } else {
    nd->dentry = cur_proc->fs->d_root;
  }

  int ret = 0;
  

  while (*cur) {
    char name[NAME_MAX + 1];
    int32_t i = 0;
    int32_t length = strlen(cur);

    while (cur[i] != '\/' && cur[i] != '\0') {
      name[i] = cur[i];
      ++i;
    }

    name[i] = '\0';  // null terminate

    while (cur[i] == '\/') {
      ++i;
    }

    struct vfs_dentry* d_child = NULL;

    if ((d_child = vfs_search_virt_subdirs(nd->dentry, name)) != NULL) {
      nd->dentry = d_child;

      if (i == length && flags & O_CREAT && flags & O_EXCL) {
        ret = -EEXIST;
        goto clean;
      }

    } else {
      d_child = alloc_dentry(nd->dentry, name);

      struct vfs_inode* inode = NULL;
      if (inode = nd->dentry->d_inode->i_op->lookup) {
        inode = nd->dentry->d_inode->i_op->lookup(nd->dentry->d_inode, name);
      }

      if (inode == NULL) {
        if (i == length && flags & O_CREAT) {
          inode = nd->dentry->d_inode->i_op->create(nd->dentry->d_inode, d_child, mode, 0);
        } else {
          ret = -ENOENT;
          kfree(d_child);
          goto clean;
        }
      } else if (i == length && flags & O_CREAT && flags & O_EXCL) {
        ret = -EEXIST;
        kfree(d_child);
        goto clean;
      }

      d_child->d_inode = inode;
      list_add_tail(&d_child->d_sibling, &nd->dentry->d_subdirs);
      nd->dentry = d_child;
      vfs_cache(d_child);
    }

    cur = &cur[i];
  }

  // find what mnt we are in
  struct vfs_mount* mnt = lookup_mnt(nd->dentry);
  if (mnt) {
    nd->mnt = mnt;
  }

clean:
  kfree(simplified);
  return ret;
}

void init_special_inode(struct vfs_inode* inode, mode_t mode, int32_t dev) {
  inode->i_mode = mode;
  if (S_ISCHR(mode)) {
    inode->i_fop = &def_chr_fops;
    inode->i_rdev = dev;
  }
}

// O_RDONLY means that it won't create all the folders that are missing 
struct vfs_mount* do_mount(const char* fstype, int flags, const char* path) {
  char* dir = NULL;
  char* name = NULL;
  strlsplat(path, strliof(path, "/"), &dir, &name);
  if (!dir) {
    dir = "/";
  }

  struct vfs_file_system_type* fs = *find_filesystem(fstype);
  struct vfs_mount* mnt = fs->mount(fs, fstype, name);
  struct nameidata nd;

  if (vfs_jmp(&nd, dir, flags, S_IFDIR) < 0) {
    return NULL;
  }

  /*
  if (!nd.dentry->d_inode->i_op->mknod) {
    assert_not_implemented();
  }
  */

  //nd.dentry->d_inode->i_op->mknod(nd.dentry->d_inode, mnt->mnt_root, S_IFDIR, 0);

  struct vfs_dentry *iter, *next;
  list_for_each_entry_safe(iter, next, &nd.dentry->d_subdirs, d_sibling) {
    if (!strcmp(iter->d_name, name)) {
      // TODO: MQ 2020-10-24 Make sure path is empty folder
      list_del(&iter->d_sibling);
      // TODO: SA 2023-12-22 kfree properies of the iter?
      kfree(iter);
    }
  }

  struct vfs_dentry* current = alloc_dentry(mnt->mnt_root, ".");
  current->d_inode = mnt->mnt_root->d_inode;
  list_add_tail(&current->d_sibling, &mnt->mnt_root->d_subdirs);

  struct vfs_dentry* back = alloc_dentry(nd.dentry, "..");
  back->d_inode = nd.dentry->d_inode;
  list_add_tail(&back->d_sibling, &mnt->mnt_root->d_subdirs);

  mnt->mnt_mountpoint->d_parent = nd.dentry;
  list_add_tail(&mnt->mnt_mountpoint->d_sibling, &nd.dentry->d_subdirs);
  list_add_tail(&mnt->sibling, &vfsmntlist);

  return mnt;
}

int32_t vfs_mkdir(const char* path, mode_t mode) {
  int ret = vfs_open(path, O_RDONLY);
  if (ret < 0 && ret != -ENOENT)
    return ret;
  else if (ret >= 0)
    return -EEXIST;

  struct nameidata nd;
  return vfs_jmp(&nd, path, O_CREAT, mode | S_IFDIR);
}

void vfs_init(struct vfs_file_system_type* fs, char* dev_name) {
  INIT_LIST_HEAD(&vfsmntlist);
  vfs_cache_init();
  
  init_rootfs(fs, dev_name);

  init_devfs();
}