
#include "kernel/fs/vfs.h"

#include "kernel/fs/devfs/devfs.h"

struct vfs_file_operations devfs_file_operations = {};

struct vfs_file_operations devfs_dir_operations = {
	.readdir = generic_memory_readdir,
};