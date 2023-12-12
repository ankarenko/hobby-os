
#include "kernel/util/string/string.h"
#include "kernel/util/errno.h"
#include "kernel/util/fcntl.h"
#include "kernel/proc/task.h"
#include "kernel/memory/malloc.h"
#include "kernel/util/debug.h"

#include "kernel/fs/vfs.h"

static struct vfs_file_system_type* file_systems;
struct list_head vfsmntlist;


bool vfs_ls(const char* path) {
  /*
  unsigned char device = 'a';

  if (file_systems[device - 'a'])
    return file_systems[device - 'a']->ls(path);
    */
}

bool vfs_cd(const char* path) {
  /*
  unsigned char device = 'a';

  if (file_systems[device - 'a'])
    return file_systems[device - 'a']->cd(path);
    */
}

struct vfs_file vfs_get_root(uint32_t device_id) {
  /*
  if (file_systems[device_id - 'a']) {
    vfs_file file = file_systems[device_id - 'a']->root();
    file.device_id = device_id;
    return file;
  }
  */
}

static struct vfs_file_system_type** find_filesystem(const char* name) {
	struct vfs_file_system_type **p;
	for (p = &file_systems; *p; p = &(*p)->next) {
		if (strcmp((*p)->name, name) == 0)
			break;
	}
	return p; 
}

int register_filesystem(struct vfs_file_system_type *fs) {
	struct vfs_file_system_type** p = find_filesystem(fs->name);

	if (*p)
		return -EBUSY;
	else
		*p = fs;

	return 0;
}

int unregister_filesystem(struct vfs_file_system_type *fs) {
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
		if (iter->mnt_mountpoint == d)
			return iter;
	}

	return NULL;
}

static void init_rootfs(struct vfs_file_system_type* fs_type, char *dev_name) {
	init_ext2_fs();

	struct vfs_mount *mnt = fs_type->mount(fs_type, dev_name, "/");
  list_add_tail(&mnt->sibling, &vfsmntlist);
	
	//current_process->fs->d_root = mnt->mnt_root;
	//current_process->fs->mnt_root = mnt;
}

struct vfs_mount* do_mount(const char* fstype, int flags, const char* path) {
	/*
  char* dir = NULL;
	char* name = NULL;
	strlsplat(path, strliof(path, "/"), &dir, &name);
	if (!dir) {
		dir = "/";
  }

	struct vfs_file_system_type* fs = *find_filesystem(fstype);
	struct vfs_mount* mnt = fs->mount(fs, fstype, name);
	struct nameidata nd;

	path_walk(&nd, dir, O_RDONLY, S_IFDIR);

	struct vfs_dentry *iter, *next;
  list_for_each_entry_safe(iter, next, &nd.dentry->d_subdirs, d_sibling) {
		if (!strcmp(iter->d_name, name)) {
			// TODO: MQ 2020-10-24 Make sure path is empty folder
			list_del(&iter->d_sibling);
			kfree(iter);
		}
	}

	mnt->mnt_mountpoint->d_parent = nd.dentry;
	list_add_tail(&mnt->mnt_mountpoint->d_sibling, &nd.dentry->d_subdirs);
	list_add_tail(&mnt->sibling, &vfsmntlist);

	return mnt;
  */
}


void vfs_init(struct vfs_file_system_type* fs, char* dev_name) {
	//log("VFS: Initializing");

	INIT_LIST_HEAD(&vfsmntlist);

	//log("VFS: Mount ext2");
	init_rootfs(fs, dev_name);
}