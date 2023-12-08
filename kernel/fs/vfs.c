
#include "kernel/util/string/string.h"
#include "kernel/util/errno.h"
#include "kernel/util/fcntl.h"
#include "kernel/proc/task.h"
#include "kernel/memory/malloc.h"
#include "kernel/util/debug.h"

#include "kernel/fs/vfs.h"

static vfs_file_system_type* file_systems;
struct list_head vfsmntlist;

/*
bool vfs_ls(const char* path) {
  unsigned char device = 'a';

  if (file_systems[device - 'a'])
    return file_systems[device - 'a']->ls(path);
}

bool vfs_cd(const char* path) {
  unsigned char device = 'a';

  if (file_systems[device - 'a'])
    return file_systems[device - 'a']->cd(path);
}

vfs_file vfs_get_root(uint32_t device_id) {
  if (file_systems[device_id - 'a']) {
    vfs_file file = file_systems[device_id - 'a']->root();
    file.device_id = device_id;
    return file;
  }
}
*/

static vfs_file_system_type** find_filesystem(const char* name) {
	vfs_file_system_type **p;
	for (p = &file_systems; *p; p = &(*p)->next) {
		if (strcmp((*p)->name, name) == 0)
			break;
	}
	return p;
}

int register_filesystem(vfs_file_system_type *fs) {
	vfs_file_system_type** p = find_filesystem(fs->name);

	if (*p)
		return -EBUSY;
	else
		*p = fs;

	return 0;
}

int unregister_filesystem(vfs_file_system_type *fs) {
	vfs_file_system_type** p;
	for (p = &file_systems; *p; p = &(*p)->next) {
		if (strcmp((*p)->name, fs->name) == 0) {
			*p = (*p)->next;
			return 0;
		}
  }
	return -EINVAL;
}

static void init_rootfs(vfs_file_system_type* fs_type, char *dev_name) {
	init_ext2_fs();

	vfs_mount *mnt = fs_type->mount(fs_type, dev_name, "/");
  list_add_tail(&mnt->sibling, &vfsmntlist);
	
	//current_process->fs->d_root = mnt->mnt_root;
	//current_process->fs->mnt_root = mnt;
}

void vfs_init(vfs_file_system_type* fs, char* dev_name) {
	//log("VFS: Initializing");

	INIT_LIST_HEAD(&vfsmntlist);

	//log("VFS: Mount ext2");
	init_rootfs(fs, dev_name);
}