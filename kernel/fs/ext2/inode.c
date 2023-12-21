#include <errno.h>
#include "kernel/util/debug.h"
#include "kernel/util/limits.h"
#include "kernel/util/string/string.h"
#include "kernel/util/path.h"
#include "kernel/fs/vfs.h"
#include "kernel/util/math.h"
#include "kernel/memory/malloc.h"

#include "kernel/fs/ext2/ext2.h"

static int ext2_recursive_block_action(
  struct vfs_superblock* sb,
	int level, uint32_t block, void* arg,
	int (*action)(struct vfs_superblock *, uint32_t, void *)) {

	KASSERT(level <= EXT2_MAX_DATA_LEVEL);
  //ext2_superblock* sb = EXT2_SB(vsb);

  if (level == 0) 
    return action(sb, block, arg);

  int ret = -ENOENT;
  uint32_t *block_buf = (uint32_t *)ext2_bread_block(sb, block);
  for (uint32_t i = 0, nblocks = sb->s_blocksize / sizeof(uint32_t); i < nblocks; ++i)
    if ((ret = ext2_recursive_block_action(sb, level - 1, block_buf[i], arg, action)) >= 0)
      break;
  kfree(block_buf);
  return ret;
}

static int ext2_find_ino(struct vfs_superblock *sb, uint32_t block, void *arg) {
	const char *name = arg;
	char *block_buf = ext2_bread_block(sb, block);

	char tmpname[NAME_MAX];
	uint32_t size = 0;
	ext2_dir_entry *entry = (ext2_dir_entry *)block_buf;

  int ret = -ENOENT;
	while (size < sb->s_blocksize) {
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

static int ext2_delete_entry(struct vfs_superblock *sb, uint32_t block, void *arg) {
	const char *name = arg;
	char *block_buf = ext2_bread_block(sb, block);

	char tmpname[NAME_MAX];
	uint32_t size = 0;
	ext2_dir_entry *prev = NULL;
	ext2_dir_entry *entry = (ext2_dir_entry *)block_buf;

	while (size < sb->s_blocksize) {
		memcpy(tmpname, entry->name, entry->name_len);
		tmpname[entry->name_len] = 0;

		if (strcmp(tmpname, name) == 0) {
			int ino = entry->ino;
			entry->ino = 0;

			if (prev)
				prev->rec_len += entry->rec_len;

			ext2_bwrite_block(sb, block, block_buf);
			return ino;
		}

		prev = entry;
		size += entry->rec_len;
		entry = (ext2_dir_entry *)((char *)entry + entry->rec_len);
	}

	return -ENOENT;
}

struct vfs_inode* ext2_lookup_inode(struct vfs_inode *dir, char* name) {
  ext2_inode *ei = EXT2_INODE(dir);
	struct vfs_superblock *sb = dir->i_sb;
  ext2_fs_info* mi = EXT2_INFO(sb);
  
	for (int32_t i = 0, ino = 0; i < ei->i_blocks; ++i) {
		if (!ei->i_block[i])
			continue;

		if (
      ((mi->ino_upper_levels[0] > i) && (ino = ext2_recursive_block_action(sb, 0, ei->i_block[i], name, ext2_find_ino)) > 0) ||
			((mi->ino_upper_levels[0] <= i && i < mi->ino_upper_levels[1]) && (ino = ext2_recursive_block_action(mi, 1, ei->i_block[12], name, ext2_find_ino)) > 0) ||
			((mi->ino_upper_levels[1] <= i && i < mi->ino_upper_levels[2]) && (ino = ext2_recursive_block_action(mi, 2, ei->i_block[13], name, ext2_find_ino)) > 0) ||
			((mi->ino_upper_levels[2] <= i && i < mi->ino_upper_levels[3]) && (ino = ext2_recursive_block_action(mi, 3, ei->i_block[14], name, ext2_find_ino)) > 0))
		{
      struct vfs_inode *inode = dir->i_sb->s_op->alloc_inode(dir->i_sb);
			inode->i_ino = ino;
			ext2_read_inode(inode);
			return inode;
		}
	}
	return NULL;
}

static int ext2_unlink(struct vfs_inode *dir, char* name) {
	ext2_inode *ei = EXT2_INODE(dir);
	struct vfs_superblock *sb = dir->i_sb;
  ext2_fs_info* mi = EXT2_INFO(sb);

	for (int32_t i = 0, ino = 0; i < ei->i_blocks; ++i) {
		if (!ei->i_block[i])
			continue;

		if (
      ((mi->ino_upper_levels[0] > i) && (ino = ext2_recursive_block_action(sb, 0, ei->i_block[i], name, ext2_delete_entry)) > 0) ||
			((mi->ino_upper_levels[0] <= i && i < mi->ino_upper_levels[1]) && (ino = ext2_recursive_block_action(mi, 1, ei->i_block[12], name, ext2_delete_entry)) > 0) ||
			((mi->ino_upper_levels[1] <= i && i < mi->ino_upper_levels[2]) && (ino = ext2_recursive_block_action(mi, 2, ei->i_block[13], name, ext2_delete_entry)) > 0) ||
			((mi->ino_upper_levels[2] <= i && i < mi->ino_upper_levels[3]) && (ino = ext2_recursive_block_action(mi, 3, ei->i_block[14], name, ext2_delete_entry)) > 0))
		{
			struct vfs_inode *inode = dir->i_sb->s_op->alloc_inode(dir->i_sb);
			inode->i_ino = ino;
			ext2_read_inode(inode);

			inode->i_nlink -= 1;
			ext2_write_inode(inode);
			// TODO: SA 2023-12-20 If i_nlink == 0, we delete ext2 inode
      kfree(inode);
			break;
		}
	}
	return 0;
}

static int ext2_add_entry(struct vfs_superblock *sb, uint32_t block, void *arg) {
	struct vfs_dentry *dentry = arg;
	int filename_length = strlen(dentry->d_name);

	char *block_buf = ext2_bread_block(sb, block);
	int size = 0;
  int new_rec_len = 0;
  int ret = -ENOENT;

	ext2_dir_entry *entry = (struct ext2_dir_entry *)block_buf;
	while (size < sb->s_blocksize && (char *)entry < block_buf + sb->s_blocksize) {
    // TODO: SA 2023-19-11 needs review
		// NOTE: MQ 2020-12-01 some ext2 tools mark entry with zero indicate an unused entry
		if (!entry->ino && (!entry->rec_len || entry->rec_len >= EXT2_DIR_REC_LEN(filename_length))) {
			entry->ino = dentry->d_inode->i_ino;
			if (S_ISREG(dentry->d_inode->i_mode))
				entry->file_type = EXT2_FT_REG_FILE;
			else if (S_ISDIR(dentry->d_inode->i_mode))
				entry->file_type = EXT2_FT_DIR;
			else
				assert_not_implemented();

			entry->name_len = filename_length;
			memcpy(entry->name, dentry->d_name, entry->name_len);
			entry->rec_len = max_t(uint16_t, new_rec_len, entry->rec_len);

			ext2_bwrite_block(sb, block, block_buf);

      ret = 0;
			goto clean;
		}

    // how do we find an unused entry in a directory?
    // https://stackoverflow.com/questions/29213455/ext2-directory-entry-list-where-is-the-end
		if (EXT2_DIR_REC_LEN(filename_length) + EXT2_DIR_REC_LEN(entry->name_len) < entry->rec_len) {
			new_rec_len = entry->rec_len - EXT2_DIR_REC_LEN(entry->name_len);
			entry->rec_len = EXT2_DIR_REC_LEN(entry->name_len);
			size += entry->rec_len;
			entry = (ext2_dir_entry *)((char *)entry + entry->rec_len);
			memset(entry, 0, new_rec_len);

		} else {
			size += entry->rec_len;
			new_rec_len = sb->s_blocksize - size;
			entry = (ext2_dir_entry *)((char *)entry + entry->rec_len);
		}
	}

clean:
  kfree(block_buf);
	return ret;
}

static int ext2_create_entry(struct vfs_superblock *sb, struct vfs_inode *dir, struct vfs_dentry *dentry) {
	ext2_inode* ei = EXT2_INODE(dir);
  ext2_fs_info* mi = EXT2_INFO(sb);

	// NOTE: MQ 2020-11-19 support nth-level block
	for (int i = 0; i < dir->i_blocks; ++i) {
		if (i >= mi->ino_upper_levels[0])
			assert_not_reached();

		uint32_t block = ei->i_block[i];
		if (!block) {
			block = ext2_create_block(dir->i_sb);
			ei->i_block[i] = block;
			dir->i_blocks += 1;
			dir->i_size += sb->s_blocksize;
			ext2_write_inode(dir);
		}

		if (ext2_add_entry(sb, block, dentry) >= 0)
			return 0;
	}
	return -ENOENT;
}

static int find_unused_inode_number(struct vfs_superblock *sb) {
	ext2_superblock *ext2_sb = EXT2_SB(sb);

	uint32_t number_of_groups = div_ceil(ext2_sb->s_blocks_count, ext2_sb->s_blocks_per_group);
	for (uint32_t group = 0; group < number_of_groups; group += 1) {
		ext2_group_desc *gdp = ext2_get_group_desc(sb, group);
		unsigned char *inode_bitmap = (unsigned char *)ext2_bread_block(sb, gdp->bg_inode_bitmap);

		for (uint32_t i = 0; i < sb->s_blocksize; ++i)
			if (inode_bitmap[i] != 0xff)
				for (uint8_t j = 0; j < 8; ++j)
					if (!(inode_bitmap[i] & (1 << j)))
						return group * ext2_sb->s_inodes_per_group + i * 8 + j + EXT2_STARTING_INO;
	}
	return -ENOSPC;
}

static struct vfs_inode *ext2_create_inode(struct vfs_inode *dir, struct vfs_dentry *dentry, mode_t mode) {
	struct vfs_superblock *sb = dir->i_sb;
	ext2_superblock *ext2_sb = EXT2_SB(sb);
	uint32_t ino = find_unused_inode_number(sb);
	ext2_group_desc *gdp = ext2_get_group_desc(sb, get_group_from_inode(ext2_sb, ino));

	// superblock
	ext2_sb->s_free_inodes_count -= 1;
	sb->s_op->write_super(sb);

	// group descriptor
	gdp->bg_free_inodes_count -= 1;
	if (S_ISDIR(mode))
		gdp->bg_used_dirs_count += 1;
	ext2_write_group_desc(sb, gdp);

	// inode bitmap
	char *inode_bitmap_buf = ext2_bread_block(sb, gdp->bg_inode_bitmap);
	uint32_t relative_inode = get_relative_inode_in_group(ext2_sb, ino);
  // each bit represents one block, 1 byte = 8 bit
	inode_bitmap_buf[relative_inode / 8] |= 1 << (relative_inode % 8);
	ext2_bwrite_block(sb, gdp->bg_inode_bitmap, inode_bitmap_buf);

	// inode table
	struct ext2_inode *ei_new = kcalloc(1, sizeof(struct ext2_inode));
	ei_new->i_links_count = 1;
	struct vfs_inode *inode = sb->s_op->alloc_inode(sb);
	inode->i_ino = ino;
	inode->i_mode = mode;
	inode->i_size = 0;
	inode->i_fs_info = ei_new;
	inode->i_sb = sb;
	inode->i_atime.tv_sec = get_seconds(NULL);
	inode->i_ctime.tv_sec = get_seconds(NULL);
	inode->i_mtime.tv_sec = get_seconds(NULL);
	inode->i_flags = 0;
	inode->i_blocks = 0;
	// NOTE: MQ 2020-11-18 When creating inode, it is safe to assume that it is linked to dir entry?
	inode->i_nlink = 1;
  
  ext2_inode *ei = EXT2_INODE(inode);
	uint32_t block = ext2_create_block(inode->i_sb);
	ei->i_block[0] = block;
	inode->i_blocks += 1;
	
	if (S_ISREG(mode)) {
		inode->i_op = &ext2_file_inode_operations;
		inode->i_fop = &ext2_file_operations;
    ext2_write_inode(inode);
    
	} else if (S_ISDIR(mode)) {
		inode->i_op = &ext2_dir_inode_operations;
		inode->i_fop = &ext2_dir_operations;
    inode->i_size += sb->s_blocksize;
		ext2_write_inode(inode);

		char *block_buf = ext2_bread_block(inode->i_sb, block);

		ext2_dir_entry *c_entry = (ext2_dir_entry *)block_buf;
		c_entry->ino = inode->i_ino;
		memcpy(c_entry->name, ".", 1);
		c_entry->name_len = 1;
		c_entry->rec_len = EXT2_DIR_REC_LEN(1);
		c_entry->file_type = EXT2_FT_DIR;

		ext2_dir_entry *p_entry = (ext2_dir_entry *)(block_buf + c_entry->rec_len);
		p_entry->ino = dir->i_ino;
		memcpy(p_entry->name, "..", 2);
		p_entry->name_len = 2;
		p_entry->rec_len = sb->s_blocksize - c_entry->rec_len;
		p_entry->file_type = EXT2_FT_DIR;

		ext2_bwrite_block(inode->i_sb, block, block_buf);
    kfree(block_buf);
	}
	else
		assert_not_reached();

	dentry->d_inode = inode;

	if (ext2_create_entry(sb, dir, dentry) >= 0)
		return inode;
	return NULL;
}

struct vfs_inode_operations ext2_file_inode_operations = {
	//.truncate = ext2_truncate_inode,
};

struct vfs_inode_operations ext2_dir_inode_operations = {
  .create = ext2_create_inode,
	.lookup = ext2_lookup_inode,
  .unlink = ext2_unlink
	//.mknod = ext2_mknod,
	//.rename = ext2_rename,
};

struct vfs_inode_operations ext2_special_inode_operations = {};