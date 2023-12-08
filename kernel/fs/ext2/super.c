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

char *ext2_bread(vfs_superblock* sb, uint32_t block, uint32_t size) {
	return bread(sb->mnt_devname, block * (sb->s_blocksize / BYTES_PER_SECTOR), size);
}

char *ext2_bread_block(vfs_superblock* sb, uint32_t block) {
	return ext2_bread(sb, block, sb->s_blocksize);
}

void ext2_bwrite(vfs_superblock* sb, uint32_t block, char *buf, uint32_t size) {
	return bwrite(sb->mnt_devname, block * (sb->s_blocksize / BYTES_PER_SECTOR), buf, size);
}

void ext2_bwrite_block(vfs_superblock* sb, uint32_t block, char *buf) {
	return ext2_bwrite(sb, block, buf, sb->s_blocksize);
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

void ext2_write_group_desc(vfs_superblock *sb, ext2_group_desc *gdp) {
	ext2_superblock *ext2_sb = EXT2_SB(sb);
  // works because bg_block_bitmap is always within its block group
	uint32_t group = get_group_from_block(ext2_sb, gdp->bg_block_bitmap);
	uint32_t block = ext2_sb->s_first_data_block + SUPERBLOCK_SIZE_BLOCKS + group / EXT2_GROUP_DESC_PER_BLOCK(ext2_sb);
	uint32_t offset = group % EXT2_GROUP_DESC_PER_BLOCK(ext2_sb) * sizeof(ext2_group_desc);
	char *group_block_buf = ext2_bread_block(ext2_sb, block);
	memcpy(group_block_buf + offset, gdp, sizeof(ext2_group_desc));
	ext2_bwrite_block(ext2_sb, block, group_block_buf);
  kfree(group_block_buf);
}

ext2_inode *ext2_get_inode(vfs_superblock* sb, ino_t ino) {
	ext2_superblock *ext2_sb = EXT2_SB(sb);
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

void ext2_write_super(vfs_superblock *sb) {
  ext2_superblock *ext2_sb = EXT2_SB(sb);
  bwrite(sb->mnt_devname, EXT2_STARTING_INO, (char *)ext2_sb, sizeof(ext2_superblock));
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

vfs_inode* init_inode() {
	vfs_inode *i = kcalloc(1, sizeof(vfs_inode));
	i->i_blocks = 0;
	i->i_size = 0;
	//sema_init(&i->i_sem, 1);
	return i;
}

vfs_inode* ext2_alloc_inode(vfs_superblock* sb) {
	vfs_inode *i = init_inode();
	i->i_sb = sb;
	//atomic_set(&i->i_count, 0);
	return i;
}

void ext2_fill_super(vfs_superblock* vsb) {
  ext2_superblock* sb = bread(DEV_NAME, EXT2_SUPERRBLOCK_POS, sizeof(ext2_superblock));
  assert_superblock(sb);
  vsb->s_blocksize = EXT2_BLOCK_SIZE(minfo->sb);
  vsb->s_magic = sb->s_magic;
  vsb->mnt_devname = strdup(DEV_NAME);

  KASSERT(sizeof(ext2_inode) == 128);
  sb->s_inode_size = sb->s_rev_level == EXT2_GOOD_OLD_REV? 128 : sb->s_inode_size;
  
  uint64_t bp_count = vsb->s_blocksize / sizeof(uint32_t);
  KASSERT(bp_count <= (EXT2_MIN_BLOCK_SIZE << 2));

  ext2_fs_info* fs_info = kcalloc(1, sizeof(ext2_fs_info));
  // SA:6.11.2023 realistically we can't address more than 512 * 2^32 sectors (2 TB)
  // because of the limitations of our block device (512 bytes sector)
  fs_info->ino_upper_levels[0] = 12; // 12 direct blocks, see standart
  fs_info->ino_upper_levels[1] = bp_count + fs_info->ino_upper_levels[0]; 
  fs_info->ino_upper_levels[2] = bp_count * bp_count + fs_info->ino_upper_levels[1];
  fs_info->ino_upper_levels[3] = bp_count * bp_count * bp_count + fs_info->ino_upper_levels[2];
  fs_info->sb = sb;

  KASSERT(vsb->s_blocksize <= PAGE_SIZE);
  KASSERT(vsb->s_blocksize >= BYTES_PER_SECTOR);
  KASSERT(vsb->s_blocksize % BYTES_PER_SECTOR == 0);

  if ((
    EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER | 
    EXT2_FEATURE_RO_COMPAT_LARGE_FILE | 
    EXT2_FEATURE_RO_COMPAT_BTREE_DIR) & minfo->sb->s_feature_ro_compat
  ) {
    fs_info->is_readonly = true;
  }

  vsb->s_fs_info = fs_info;
}

void ext2_read_inode(vfs_inode* i) {
	ext2_inode* raw_node = ext2_get_inode(i->i_sb, i->i_ino);

	i->i_mode = raw_node->i_mode;
	i->i_gid = raw_node->i_gid;
	i->i_uid = raw_node->i_uid;

	i->i_nlink = raw_node->i_links_count;
	i->i_size = raw_node->i_size;
	//i->i_atime.tv_sec = raw_node->i_atime;
	//i->i_ctime.tv_sec = raw_node->i_ctime;
	//i->i_mtime.tv_sec = raw_node->i_mtime;
	//i->i_atime.tv_nsec = i->i_ctime.tv_nsec = i->i_mtime.tv_nsec = 0;

	i->i_blksize = PMM_FRAME_SIZE; /* This is the optimal IO size (for stat), not the fs block size */
	i->i_blocks = raw_node->i_blocks;
	i->i_flags = raw_node->i_flags;
	i->i_fs_info = raw_node;

  /*
	if (S_ISREG(i->i_mode))
	{
		i->i_op = &ext2_file_inode_operations;
		i->i_fop = &ext2_file_operations;
	}
	else if (S_ISDIR(i->i_mode))
	{
		i->i_op = &ext2_dir_inode_operations;
		i->i_fop = &ext2_dir_operations;
	}
	else
	{
		i->i_op = &ext2_special_inode_operations;
		init_special_inode(i, i->i_mode, raw_node->i_block[0]);
	}
  */
}

vfs_dentry* alloc_dentry(vfs_dentry* parent, char *name) {
	vfs_dentry *d = kcalloc(1, sizeof(vfs_dentry));
	d->d_name = strdup(name);
	d->d_parent = parent;
	INIT_LIST_HEAD(&d->d_subdirs);

	if (parent) {
		d->d_sb = parent->d_sb;
  }

	return d;
}

vfs_mount* ext2_mount(vfs_file_system_type *fs_type, char *dev_name, char *dir_name) {
  vfs_superblock *sb = (vfs_superblock *)kcalloc(1, sizeof(vfs_superblock));
	sb->mnt_devname = strdup(dev_name);
	ext2_fill_super(sb);

  vfs_inode* i_root = ext2_alloc_inode(sb);
	i_root->i_ino = EXT2_ROOT_INO;
	ext2_read_inode(i_root);
  KASSERT(i_root->i_mode & EXT2_S_IFDIR != 0); // check if directory

	vfs_dentry* d_root = alloc_dentry(NULL, dir_name);
	d_root->d_inode = i_root;
	d_root->d_sb = sb;

	sb->s_root = d_root;

	vfs_mount *mnt = kcalloc(1, sizeof(vfs_mount));
	mnt->mnt_sb = sb;
	mnt->mnt_mountpoint = mnt->mnt_root = sb->s_root;
	mnt->mnt_devname = sb->mnt_devname;
  return mnt;
  
  // get root directory 
  /*
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
  */
}

void init_ext2_fs() {
	register_filesystem(&ext2_fs_type);
}

void exit_ext2_fs() {
	unregister_filesystem(&ext2_fs_type);
}

vfs_super_operations ext2_super_operations = {
	.alloc_inode = ext2_alloc_inode,
	.read_inode = ext2_read_inode,
	.write_inode = ext2_write_inode,
	.write_super = ext2_write_super,
};

vfs_file_system_type ext2_fs_type = {
	.name = "ext2",
	.mount = ext2_mount,
};

