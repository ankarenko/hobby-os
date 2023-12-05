#include <errno.h>
#include "kernel/util/debug.h"
#include "kernel/util/limits.h"
#include "kernel/util/string/string.h"

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
  for (uint32_t i = 0, nblocks = minfo->blocksize >> 2; i < nblocks; ++i)
    if ((ret = ext2_recursive_block_action(sb, level - 1, block_buf[i], arg, action)) >= 0)
      break;
  return ret;
}

static int ext2_find_ino(ext2_mount_info* minfo, uint32_t block, void *arg) {
	const char *name = arg;
	char *block_buf = ext2_bread_block(minfo, block);

	char tmpname[NAME_MAX];
	uint32_t size = 0;
	ext2_dir_entry *entry = (ext2_dir_entry *)block_buf;
	while (size < minfo->blocksize) {
		memcpy(tmpname, entry->name, entry->name_len);
		tmpname[entry->name_len] = 0;

		if (strcmp(tmpname, name) == 0)
			return entry->ino;

		size = size + entry->rec_len;
		entry = (ext2_dir_entry *)((char *)entry + entry->rec_len);
	}
	return -ENOENT;
}

static ext2_inode *ext2_lookup_inode(ext2_inode *dir, char* name) {
	ext2_inode *ei = dir;

	for (uint32_t i = 0, ino = 0; i < ei->i_blocks; ++i) {
		if (!ei->i_block[i])
			continue;

		if (
      ((EXT2_INO_UPPER_LEVEL0 > i) && (ino = ext2_recursive_block_action(minfo, 0, ei->i_block[i], name, ext2_find_ino)) > 0) ||
			((EXT2_INO_UPPER_LEVEL0 <= i && i < EXT2_INO_UPPER_LEVEL1) && (ino = ext2_recursive_block_action(minfo, 1, ei->i_block[12], name, ext2_find_ino)) > 0) ||
			((EXT2_INO_UPPER_LEVEL1 <= i && i < EXT2_INO_UPPER_LEVEL2) && (ino = ext2_recursive_block_action(minfo, 2, ei->i_block[13], name, ext2_find_ino)) > 0) ||
			((EXT2_INO_UPPER_LEVEL2 <= i && i < EXT2_INO_UPPER_LEVEL3) && (ino = ext2_recursive_block_action(minfo, 3, ei->i_block[14], name, ext2_find_ino)) > 0))
		{
			return ext2_get_inode(ino);
		}
	}
	return NULL;
}
