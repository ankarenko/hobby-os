#include <errno.h>

#include "kernel/fs/ext2/ext2.h"
#include "kernel/util/math.h"
#include "kernel/util/debug.h"
#include "kernel/util/limits.h"

static void ext2_read_nth_block(
  ext2_mount_info* mi, uint32_t block, char **iter_buf, 
  off_t ppos, uint32_t *p, size_t count, int level
) {
	KASSERT(level <= EXT2_MAX_DATA_LEVEL);
  uint32_t blocksize = minfo->blocksize;
  KASSERT(blocksize % sizeof(uint32_t) == 0);

  if (level == 0) {
    char *block_buf = ext2_bread_block(mi, block);
		int32_t pstart = (ppos > *p) ? ppos - *p : 0;
		uint32_t pend = ((ppos + count) < (*p + blocksize)) ? (*p + blocksize - ppos - count) : 0;
		memcpy(*iter_buf, block_buf + pstart, blocksize - pstart - pend);
    kfree(block_buf);
		*p += blocksize;
		*iter_buf += blocksize - pstart - pend;
    return;
  }

  uint32_t *block_buf = (uint32_t *)ext2_bread_block(mi, block);
  for (uint32_t i = 0, times = blocksize / sizeof(uint32_t); *p < ppos + count && i < times; ++i) {
    ext2_read_nth_block(mi, block_buf[i], iter_buf, ppos, p, count, level - 1);
  }
  kfree(block_buf);
}

ssize_t ext2_read_file(
  ext2_mount_info* minfo, ext2_inode* inode, 
  char *buf, size_t count, off_t ppos
) {
	
	ext2_inode* ei = inode;
	ext2_superblock* sb = minfo->sb;

	count = min_t(size_t, ppos + count, ei->i_size) - ppos;
	uint32_t p = (ppos / minfo->blocksize) * minfo->blocksize;
	char *iter_buf = buf;

	while (p < ppos + count) {
		uint32_t relative_block = p / minfo->blocksize;
		if (relative_block < minfo->ino_upper_levels[0])
			ext2_read_nth_block(sb, ei->i_block[relative_block], &iter_buf, ppos, &p, count, 0);
		else if (relative_block < minfo->ino_upper_levels[1])
			ext2_read_nth_block(sb, ei->i_block[12], &iter_buf, ppos, &p, count, 1);
		else if (relative_block < minfo->ino_upper_levels[2])
			ext2_read_nth_block(sb, ei->i_block[13], &iter_buf, ppos, &p, count, 2);
		else if (relative_block < minfo->ino_upper_levels[3])
			ext2_read_nth_block(sb, ei->i_block[14], &iter_buf, ppos, &p, count, 3);
		else
      assert_not_reached();
	}

	//file->f_pos = ppos + count;
	return count;
}

int ext2_readdir(ext2_inode* inode, ext2_dir_entry** dirs) {
  if (!(inode->i_mode & EXT2_S_IFDIR)) { // not a directory
    return -ENOTDIR;
  }

  uint32_t count = inode->i_size;
	char *buf = kcalloc(count, sizeof(char));
	count = ext2_read_file(minfo, inode, buf, count, 0);
  
  *dirs = buf;
  uint32_t entries_size = 0;
  for (char *ibuf = buf; ibuf - buf < count;) {
    ext2_dir_entry* iter = ibuf;
    KASSERT(iter->rec_len % 4 == 0);
		entries_size += iter->rec_len;
		ibuf += iter->rec_len;
	}

	return entries_size;
}