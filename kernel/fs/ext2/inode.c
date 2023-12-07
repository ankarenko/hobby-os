#include <errno.h>
#include "kernel/util/debug.h"
#include "kernel/util/limits.h"
#include "kernel/util/string/string.h"
#include "kernel/util/path.h"
#include "kernel/fs/vfs.h"

#include "kernel/fs/ext2/ext2.h"

static int ext2_recursive_block_action(
  ext2_mount_info* minfo,
	int level, uint32_t block, void *arg,
	int (*action)(ext2_mount_info*, uint32_t, void *)) {

	KASSERT(level <= EXT2_MAX_DATA_LEVEL);
  ext2_superblock* sb = minfo->sb;

  if (level == 0) 
    return action(minfo, block, arg);

  int ret = -ENOENT;
  uint32_t *block_buf = (uint32_t *)ext2_bread_block(sb, block);
  for (uint32_t i = 0, nblocks = minfo->blocksize / sizeof(uint32_t); i < nblocks; ++i)
    if ((ret = ext2_recursive_block_action(sb, level - 1, block_buf[i], arg, action)) >= 0)
      break;
  kfree(block_buf);
  return ret;
}

static int ext2_find_ino(ext2_mount_info* minfo, uint32_t block, void *arg) {
	const char *name = arg;
	char *block_buf = ext2_bread_block(minfo, block);

	char tmpname[NAME_MAX];
	uint32_t size = 0;
	ext2_dir_entry *entry = (ext2_dir_entry *)block_buf;

  int ret = -ENOENT;
	while (size < minfo->blocksize) {
		memcpy(tmpname, entry->name, entry->name_len);
		tmpname[entry->name_len] = '\0';

		if (strcmp(tmpname, name) == 0) {
      ret = entry->ino;
      break;
    }

		size = size + entry->rec_len;
		entry = (ext2_dir_entry *)((char *)entry + entry->rec_len);
	}
  kfree(block_buf);
	return ret;
}

ext2_inode* ext2_lookup_inode(ext2_inode* dir, char* name) {
	ext2_inode *ei = dir;

	for (uint32_t i = 0, ino = 0; i < ei->i_blocks; ++i) {
		if (!ei->i_block[i])
			continue;

		if (
      ((minfo->ino_upper_levels[0] > i) && (ino = ext2_recursive_block_action(minfo, 0, ei->i_block[i], name, ext2_find_ino)) > 0) ||
			((minfo->ino_upper_levels[0] <= i && i < minfo->ino_upper_levels[1]) && (ino = ext2_recursive_block_action(minfo, 1, ei->i_block[12], name, ext2_find_ino)) > 0) ||
			((minfo->ino_upper_levels[1] <= i && i < minfo->ino_upper_levels[2]) && (ino = ext2_recursive_block_action(minfo, 2, ei->i_block[13], name, ext2_find_ino)) > 0) ||
			((minfo->ino_upper_levels[2] <= i && i < minfo->ino_upper_levels[3]) && (ino = ext2_recursive_block_action(minfo, 3, ei->i_block[14], name, ext2_find_ino)) > 0))
		{
			return ext2_get_inode(ino);
		}
	}
	return NULL;
}

int ext2_jmp(ext2_inode* dir, ext2_inode** res, char* path) {
  char* simplified = NULL;
  if (!simplify_path(path, &simplified)) {
    return -EINVAL;
  }

  if (simplified[0] == '\0') { // if empty return current directory
    return dir;
  }

  const char* cur = simplified;
  
  if (dir == NULL) {
    dir = minfo->root_dir;
  }

  // check if root dir
  if (simplified[0] == '\/') {
    dir = minfo->root_dir;
    cur++;
  }
  
  int ret = 0;
  ext2_inode* cur_node = dir;

  while (*cur) {
    char pathname[NAME_MAX + 1];
    int32_t i = 0;
    while (cur[i] != '\/' && cur[i] != '\0') {
      pathname[i] = cur[i];
      ++i;
    }

    pathname[i] = '\0';  // null terminate

    if ((cur_node = ext2_lookup_inode(cur_node, pathname)) == NULL) {
      return -ENOENT;
    }

    if (cur[i] == '\/') {
      ++i;
    }

    cur = &cur[i];
  }

  *res = cur_node;
  return ret;
}

vfs_inode_operations ext2_file_inode_operations = {
	//.truncate = ext2_truncate_inode,
};

vfs_inode_operations ext2_dir_inode_operations = {
	//.create = ext2_create_inode,
	.lookup = ext2_lookup_inode,
	//.mknod = ext2_mknod,
	//.rename = ext2_rename,
	//.unlink = ext2_unlink,

};

vfs_inode_operations ext2_special_inode_operations = {};