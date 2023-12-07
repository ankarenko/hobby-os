#include "kernel/fs/ext2/ext2.h"
#include "kernel/fs/buffer.h"
#include "kernel/memory/malloc.h"
#include "kernel/util/string/string.h"
#include "kernel/util/debug.h"
#include "kernel/memory/vmm.h"
#include "kernel/util/limits.h"

#define DEV_NAME "/dev/hda"

#define SUPERBLOCK_SIZE_BLOCKS 1

#define get_group_from_inode(sb, ino) ((ino - EXT2_STARTING_INO) / sb->s_inodes_per_group)
#define get_relative_inode_in_group(sb, ino) ((ino - EXT2_STARTING_INO) % sb->s_inodes_per_group)
#define get_relative_block_in_group(sb, block) ((block - sb->s_first_data_block) % sb->s_blocks_per_group)
#define get_group_from_block(sb, block) ((block - sb->s_first_data_block) / sb->s_blocks_per_group)

ext2_mount_info* minfo = NULL;

char *ext2_bread(ext2_mount_info* mi, uint32_t block, uint32_t size) {
	return bread(DEV_NAME, block * (minfo->blocksize / BYTES_PER_SECTOR), size);
}

char *ext2_bread_block(ext2_mount_info* mi, uint32_t block) {
	return ext2_bread(mi, block, mi->blocksize);
}

void ext2_bwrite(ext2_mount_info* mi, uint32_t block, char *buf, uint32_t size) {
	return bwrite(DEV_NAME, block * (mi->blocksize / BYTES_PER_SECTOR), buf, size);
}

void ext2_bwrite_block(ext2_mount_info* mi, uint32_t block, char *buf) {
	return ext2_bwrite(mi, block, buf, mi->blocksize);
}

ext2_group_desc *ext2_get_group_desc(uint32_t group) {
	ext2_group_desc* gdp = kcalloc(1, sizeof(ext2_group_desc));
	ext2_superblock* ext2_sb = minfo->sb;
	uint32_t block = ext2_sb->s_first_data_block + SUPERBLOCK_SIZE_BLOCKS + (group / EXT2_GROUP_DESC_PER_BLOCK(ext2_sb));
	char *group_block_buf = ext2_bread_block(ext2_sb, block);
	uint32_t offset = group % EXT2_GROUP_DESC_PER_BLOCK(ext2_sb) * sizeof(ext2_group_desc);
	memcpy(gdp, group_block_buf + offset, sizeof(ext2_group_desc));
  kfree(group_block_buf);
	return gdp;
}

void ext2_write_group_desc(ext2_group_desc *gdp) {
	ext2_superblock *ext2_sb = minfo->sb;
  // works because bg_block_bitmap is always within its block group
	uint32_t group = get_group_from_block(ext2_sb, gdp->bg_block_bitmap);
	uint32_t block = ext2_sb->s_first_data_block + SUPERBLOCK_SIZE_BLOCKS + group / EXT2_GROUP_DESC_PER_BLOCK(ext2_sb);
	uint32_t offset = group % EXT2_GROUP_DESC_PER_BLOCK(ext2_sb) * sizeof(ext2_group_desc);
	char *group_block_buf = ext2_bread_block(ext2_sb, block);
	memcpy(group_block_buf + offset, gdp, sizeof(ext2_group_desc));
	ext2_bwrite_block(ext2_sb, block, group_block_buf);
  kfree(group_block_buf);
}

ext2_inode *ext2_get_inode(ino_t ino) {
	ext2_superblock *ext2_sb = minfo->sb;
	uint32_t group = get_group_from_inode(ext2_sb, ino);
	ext2_group_desc *gdp = ext2_get_group_desc(group);
  ino_t rel_inode = get_relative_inode_in_group(ext2_sb, ino);
	uint32_t block = gdp->bg_inode_table + rel_inode / EXT2_INODES_PER_BLOCK(ext2_sb);
	kfree(gdp);
  uint32_t offset = (rel_inode % EXT2_INODES_PER_BLOCK(ext2_sb)) * minfo->inode_size;
	char *table_buf = ext2_bread_block(ext2_sb, block);
  ext2_inode* inode = kcalloc(1, minfo->inode_size);
  memcpy(inode, table_buf + offset, minfo->inode_size);
  kfree(table_buf);
	return inode;
}

void ext2_write_super() {
  ext2_superblock *ext2_sb = minfo->sb;
  bwrite(DEV_NAME, EXT2_STARTING_INO, (char *)ext2_sb, sizeof(ext2_superblock));
}

void ext2_write_inode(ext2_inode* ei, ino_t ino) {
	ext2_superblock *ext2_sb = minfo->sb;
	uint32_t group = get_group_from_inode(ext2_sb, ino);
  ext2_group_desc *gdp = ext2_get_group_desc(group);
	uint32_t block = gdp->bg_inode_table + get_relative_inode_in_group(ext2_sb, ino) / EXT2_INODES_PER_BLOCK(ext2_sb);
	kfree(gdp);
  uint32_t offset = (get_relative_inode_in_group(ext2_sb, ino) % EXT2_INODES_PER_BLOCK(ext2_sb)) * sizeof(struct ext2_inode);
	char *buf = ext2_bread_block(minfo, block);
	memcpy(buf + offset, ei, sizeof(struct ext2_inode));
	ext2_bwrite_block(minfo, block, buf);
  kfree(buf);
}

void assert_superblock(ext2_superblock* sb) {
  KASSERT(sb != NULL);
  KASSERT(sb->s_inodes_count <= sb->s_inodes_per_group * EXT2_MAX_BLOCK_GROUP_SIZE(sb));
  KASSERT(sb->s_blocks_count <= sb->s_blocks_per_group * EXT2_MAX_BLOCK_GROUP_SIZE(sb));
  KASSERT(sb->s_inodes_per_group % EXT2_INODES_PER_BLOCK(sb) == 0);
  KASSERT(sb->s_magic == EXT2_SUPER_MAGIC);
  KASSERT(sb->s_state == EXT2_VALID_FS);

  KASSERT(!(sb->s_feature_incompat & EXT2_FEATURE_INCOMPAT_COMPRESSION));
  KASSERT(sb->s_first_data_block == (EXT2_BLOCK_SIZE(sb) == EXT2_MIN_BLOCK_SIZE? 1 : 0));

  if (sb->s_rev_level == EXT2_GOOD_OLD_REV) {
    KASSERT(sb->s_inode_size == 128);
  } else {
    KASSERT(sb->s_inode_size % 2 == 0);
    KASSERT(sb->s_inode_size <= EXT2_BLOCK_SIZE(sb));
  } 

  // optional
  KASSERT(sb->s_creator_os == EXT2_OS_LINUX); 
}

void init_ext2_fs() {
  minfo = kcalloc(1, sizeof(ext2_mount_info));

  minfo->sb = bread(DEV_NAME, EXT2_SUPERRBLOCK_POS, sizeof(ext2_superblock));
  minfo->blocksize = EXT2_BLOCK_SIZE(minfo->sb);
  minfo->inode_reserved = minfo->sb->s_rev_level == EXT2_GOOD_OLD_REV? 
    11 : minfo->sb->s_first_ino;
  uint32_t num_block_groups = div_ceil(minfo->sb->s_blocks_count, minfo->sb->s_blocks_per_group);
  minfo->gdt_size_blocks = div_ceil((num_block_groups * sizeof(ext2_group_desc)), minfo->blocksize);
  
  uint64_t bp_count = minfo->blocksize / sizeof(uint32_t);
  
  KASSERT(sizeof(ext2_inode) == 128);

  minfo->inode_size = EXT2_GOOD_OLD_REV? 128 : minfo->sb->s_inode_size;

  KASSERT(bp_count <= (1024 << 2));

  // SA:6.11.2023 realistically we can't address more than 512 * 2^32 sectors (2 TB)
  // because of the limitations of our block device (512 bytes sector)
  minfo->ino_upper_levels[0] = 12; // 12 direct blocks, see standart
  minfo->ino_upper_levels[1] = bp_count + minfo->ino_upper_levels[0]; 
  minfo->ino_upper_levels[2] = bp_count * bp_count + minfo->ino_upper_levels[1];
  minfo->ino_upper_levels[3] = bp_count * bp_count * bp_count + minfo->ino_upper_levels[2];

  KASSERT(minfo->blocksize <= PAGE_SIZE);
  KASSERT(minfo->blocksize >= BYTES_PER_SECTOR);
  KASSERT(minfo->blocksize % BYTES_PER_SECTOR == 0);

  if ((
    EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER | 
    EXT2_FEATURE_RO_COMPAT_LARGE_FILE | 
    EXT2_FEATURE_RO_COMPAT_BTREE_DIR) & minfo->sb->s_feature_ro_compat
  ) {
    minfo->is_readonly = true;
  }

  assert_superblock(minfo->sb);

  // get root directory 
  minfo->root_dir = ext2_get_inode(EXT2_ROOT_INO);
  KASSERT(minfo->root_dir->i_mode & EXT2_S_IFDIR != 0); // check if directory

  ext2_inode* file = NULL;
  if (ext2_jmp(NULL, &file, "greande.txt") >= 0) {
    char c;
    uint32_t read = 0;

    uint32_t pos = 0;
    while ((read = ext2_read_file(minfo, file, &c, 1, pos)) > 0) {
      printf("%c", c);
      pos += read;
    }
  }

  vfs_register_file_system(&fsys_fat, 0);

  fat_mount();
}

void exit_ext2_fs() {
  if (minfo != NULL && minfo->sb != NULL) {
    kfree(minfo->sb);
    kfree(minfo->root_dir);
    kfree(minfo);
    minfo = NULL;
  }
}

vfs_file_system_type ext2_fs_type = {
	.name = "ext2",
	.mount = ext2_mount,
};