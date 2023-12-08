#include <errno.h>

#include "kernel/fs/ext2/ext2.h"
#include "kernel/fs/vfs.h"
#include "kernel/util/math.h"
#include "kernel/util/debug.h"
#include "kernel/util/limits.h"

static void ext2_read_nth_block(
  struct vfs_superblock *sb, uint32_t block, char **iter_buf, 
  off_t ppos, uint32_t *p, size_t count, int level
) {
	KASSERT(level <= EXT2_MAX_DATA_LEVEL);
  uint32_t blocksize = sb->s_blocksize;
  KASSERT(blocksize % sizeof(uint32_t) == 0);

  if (level == 0) {
    char *block_buf = ext2_bread_block(sb, block);
		int32_t pstart = (ppos > *p) ? ppos - *p : 0;
		uint32_t pend = ((ppos + count) < (*p + blocksize)) ? (*p + blocksize - ppos - count) : 0;
		memcpy(*iter_buf, block_buf + pstart, blocksize - pstart - pend);
    kfree(block_buf);
		*p += blocksize;
		*iter_buf += blocksize - pstart - pend;
    return;
  }

  uint32_t *block_buf = (uint32_t *)ext2_bread_block(sb, block);
  for (uint32_t i = 0, times = blocksize / sizeof(uint32_t); *p < ppos + count && i < times; ++i) {
    ext2_read_nth_block(sb, block_buf[i], iter_buf, ppos, p, count, level - 1);
  }
  kfree(block_buf);
}

ssize_t ext2_read_file(struct vfs_file* file, char *buf, size_t count, off_t ppos) {
	struct vfs_inode* inode = file->f_dentry->d_inode;
  ext2_inode* ei = EXT2_INODE(inode);
	struct vfs_superblock* sb = inode->i_sb;
  ext2_mount_info* mi = EXT2_INFO(sb);

	count = min_t(size_t, ppos + count, ei->i_size) - ppos;
	uint32_t p = (ppos / sb->s_blocksize) * sb->s_blocksize;
	char *iter_buf = buf;

	while (p < ppos + count) {
		uint32_t relative_block = p / sb->s_blocksize;
		if (relative_block < mi->ino_upper_levels[0])
			ext2_read_nth_block(sb, ei->i_block[relative_block], &iter_buf, ppos, &p, count, 0);
		else if (relative_block < mi->ino_upper_levels[1])
			ext2_read_nth_block(sb, ei->i_block[12], &iter_buf, ppos, &p, count, 1);
		else if (relative_block < mi->ino_upper_levels[2])
			ext2_read_nth_block(sb, ei->i_block[13], &iter_buf, ppos, &p, count, 2);
		else if (relative_block < mi->ino_upper_levels[3])
			ext2_read_nth_block(sb, ei->i_block[14], &iter_buf, ppos, &p, count, 3);
		else
      assert_not_reached();
	}

	//file->f_pos = ppos + count;
	return count;
}

int ext2_readdir(struct vfs_file *file, struct dirent* dirent) {
  struct vfs_inode* inode = file->f_dentry->d_inode;
  
  if (!(inode->i_mode & EXT2_S_IFDIR)) { // not a directory
    return -ENOTDIR;
  }

  uint32_t count = inode->i_size;
	char *buf = kcalloc(count, sizeof(char));
	count = ext2_read_file(file, buf, count, 0);
  
  uint32_t entries_size = 0;

  struct dirent* idirent = dirent;
  for (char *ibuf = buf; ibuf - buf < count;) {
    //KASSERT(iter->rec_len % 4 == 0);
		ext2_dir_entry *entry = (struct ext2_dir_entry *)ibuf;
		idirent->d_ino = entry->ino;
		idirent->d_off = 0;
		idirent->d_reclen = sizeof(struct dirent) + entry->name_len + 1;
		idirent->d_type = entry->file_type;
		memcpy(idirent->d_name, entry->name, entry->name_len);
		idirent->d_name[entry->name_len] = 0;

		entries_size += idirent->d_reclen;
		ibuf += entry->rec_len;
		idirent = (struct dirent *)((char *)idirent + idirent->d_reclen);
	}

	return entries_size;
}

struct vfs_file_operations ext2_file_operations = {
	//.llseek = generic_file_llseek,
	.read = ext2_read_file,
	//.write = ext2_write_file,
	//.mmap = ext2_mmap_file,
};

struct vfs_file_operations ext2_dir_operations = {
	.readdir = ext2_readdir,
};