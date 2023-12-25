#include <errno.h>

#include "kernel/fs/ext2/ext2.h"
#include "kernel/fs/vfs.h"
#include "kernel/util/math.h"
#include "kernel/util/debug.h"
#include "kernel/util/string/string.h"
#include "kernel/util/limits.h"
#include "kernel/util/fcntl.h"

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
// need 
ssize_t ext2_write_file(struct vfs_file *file, const char *buf, size_t count, off_t ppos) {
	struct vfs_inode *inode = file->f_dentry->d_inode;
	struct ext2_inode *ei = EXT2_INODE(inode);
	struct vfs_superblock *sb = inode->i_sb;
  ext2_fs_info* mi = EXT2_INFO(sb);

	if (ppos + count > inode->i_size) {
		inode->i_size = ppos + count;
    ei->i_size = inode->i_size;
		inode->i_blocks = div_ceil(ppos + count, sb->s_blocksize /*BYTES_PER_SECTOR*/);
		sb->s_op->write_inode(inode);
	}

	uint32_t p = (ppos / sb->s_blocksize) * sb->s_blocksize;
	char *iter_buf = buf;
	while (p < ppos + count) {
		uint32_t relative_block = p / sb->s_blocksize;
		uint32_t block = 0;
		// FIXME: MQ 2019-07-18 Only support direct blocks
		if (relative_block < mi->ino_upper_levels[0]) {
			block = ei->i_block[relative_block];
			if (!block) {
				block = ext2_create_block(sb);
				ei->i_block[relative_block] = block;
				inode->i_mtime.tv_sec = get_seconds(NULL);
				sb->s_op->write_inode(inode);
			}
		} else {
			PANIC("Only support direct blocks, fail writing at %d-nth block", relative_block);
    }

		char *block_buf = ext2_bread_block(sb, block);
		uint32_t pstart = (ppos > p) ? ppos - p : 0;
		uint32_t pend = ((ppos + count) < (p + sb->s_blocksize)) ? (p + sb->s_blocksize - ppos - count) : 0;
		memcpy(block_buf + pstart, iter_buf, sb->s_blocksize - pstart - pend);
		ext2_bwrite_block(sb, block, block_buf);
		p += sb->s_blocksize;
		iter_buf += sb->s_blocksize - pstart - pend;
	}

	file->f_pos = ppos + count;
	return count;
}

ssize_t ext2_read_file(struct vfs_file* file, char *buf, size_t count, off_t ppos) {
	struct vfs_inode* inode = file->f_dentry->d_inode;
  ext2_inode* ei = EXT2_INODE(inode);
	struct vfs_superblock* sb = inode->i_sb;
  ext2_fs_info* mi = EXT2_INFO(sb);

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

	file->f_pos = ppos + count;
	return count;
}

static int ext2_mode_to_vfs(int ft) {
  switch (ft) {
    case EXT2_FT_REG_FILE:
      return S_IFREG;
    case EXT2_FT_CHRDEV:
      return S_IFCHR;
    case EXT2_FT_SOCK:
      return S_IFSOCK;
    case EXT2_FT_DIR:
      return S_IFDIR;
    case EXT2_FT_BLKDEV:
      return S_IFBLK;
    case EXT2_FT_FIFO:
      return S_IFBLK;
    case EXT2_FT_SYMLINK:
      return S_IFLNK;
    default:
      return S_IFMT;
  }
}

int ext2_readdir(struct vfs_file *file, struct dirent** dirent) {
  struct vfs_inode* inode = file->f_dentry->d_inode;
  
  if (!(S_ISDIR(inode->i_mode))) { // not a directory
    return -ENOTDIR;
  }

  uint32_t count = inode->i_size;
	char *buf = kcalloc(count, sizeof(char));
	count = ext2_read_file(file, buf, count, 0);
  
  uint32_t size = 0;

  for (char *ibuf = buf; ibuf - buf < count;) {
		ext2_dir_entry *entry = (struct ext2_dir_entry *)ibuf;
		size += sizeof(struct dirent);
		ibuf += entry->rec_len;
	}

  *dirent = kcalloc(size, sizeof(char));

  struct dirent* idirent = *dirent;
  for (char *ibuf = buf; ibuf - buf < count;) {
    //KASSERT(iter->rec_len % 4 == 0);
		ext2_dir_entry *entry = (struct ext2_dir_entry *)ibuf;
		idirent->d_ino = entry->ino;
		idirent->d_off = 0;
		idirent->d_reclen = sizeof(struct dirent);
		idirent->d_type = ext2_mode_to_vfs(entry->file_type);
		memcpy(idirent->d_name, entry->name, entry->name_len);
		idirent->d_name[entry->name_len] = 0;
		ibuf += entry->rec_len;
		idirent = (struct dirent *)((char *)idirent + idirent->d_reclen);
	}

  
  kfree(buf);
  KASSERT(size % sizeof(struct dirent) == 0);
  int ext2_size = size / sizeof(struct dirent);
  return ext2_size;
  /*
  // check if it can be extended
  struct dirent* virtual = 0;
  int virtual_size = generic_memory_readdir(file, &virtual);
  int final_size = virtual_size + ext2_size;

  idirent = *dirent;
  for (int i = 0; i < virtual_size; ++i) {
    for (int j = 0; j < ext2_size; ++j) {
      if (!strcmp(idirent[j].d_name, virtual[i].d_name) || !S_ISDIR(idirent[j].d_type)) {
        virtual[i].d_reclen = 0; // ignore
        --final_size;
        break;
      }
    }
  }

  struct dirent* final = kcalloc(final_size, sizeof(struct dirent));
  memcpy(final, *dirent, ext2_size * sizeof(struct dirent));
  for (int i = ext2_size; i < final_size;) {
    if (virtual[i - ext2_size].d_reclen != 0) {
      memcpy(&final[i], &virtual[i - ext2_size], sizeof(struct dirent));
      ++i;
    }
  }

  kfree(*dirent);
  kfree(virtual);
  *dirent = final;

	return final_size;
  */
}

struct vfs_file_operations ext2_file_operations = {
	.llseek = vfs_generic_llseek,
	.read = ext2_read_file,
	.write = ext2_write_file,
	//.mmap = ext2_mmap_file,
};

struct vfs_file_operations ext2_dir_operations = {
	.readdir = ext2_readdir,
};