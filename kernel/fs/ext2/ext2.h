#ifndef KERNEL_FS_EXT2_H
#define KERNEL_FS_EXT2_H

/**
 * The implementation is based on this file
 * https://www.nongnu.org/ext2-doc/ext2.html#bg-block-bitmap
*/

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#include "kernel/fs/vfs.h"

#define EXT2_SUPER_MAGIC 0xEF53
#define EXT2_STARTING_INO 1
#define EXT2_MAX_DATA_LEVEL 3
#define EXT2_SUPERRBLOCK_POS (1024 / BYTES_PER_SECTOR)

#define EXT2_MIN_BLOCK_SIZE 1024
#define EXT2_MAX_BLOCK_SIZE 4096

#define EXT2_BLOCK_SIZE(sb) (EXT2_MIN_BLOCK_SIZE << sb->s_log_block_size)
#define EXT2_INODES_PER_BLOCK(sb) (EXT2_BLOCK_SIZE(sb) / sb->s_inode_size)
#define EXT2_GROUP_DESC_PER_BLOCK(sb) (EXT2_BLOCK_SIZE(sb) / sizeof(ext2_group_desc))
#define EXT2_MAX_BLOCK_GROUP_SIZE(sb) (EXT2_BLOCK_SIZE(sb) << 3)

#define EXT2_VALID_FS 1       /* Unmounted cleanly */
#define EXT2_ERROR_FS 2       /* Errors detected */

#define EXT2_OS_LINUX	0	  // Linux
#define EXT2_OS_HURD	1	  // GNU HURD
#define EXT2_OS_MASIX	2	  // MASIX
#define EXT2_OS_FREEBSD	3	// FreeBSD
#define EXT2_OS_LITES	4	  // Lites

#define EXT2_GOOD_OLD_REV	0	 // Revision 0
#define EXT2_DYNAMIC_REV	1	 // Revision 1 with variable inode sizes, extended attributes, etc.

// features compatible
#define EXT2_FEATURE_COMPAT_DIR_PREALLOC    0x0001	 // Block pre-allocation for new directories
#define EXT2_FEATURE_COMPAT_IMAGIC_INODES   0x0002	 
#define EXT3_FEATURE_COMPAT_HAS_JOURNAL     0x0004	// An Ext3 journal exists
#define EXT2_FEATURE_COMPAT_EXT_ATTR        0x0008	 // Extended inode attributes are present
#define EXT2_FEATURE_COMPAT_RESIZE_INO      0x0010	// Non-standard inode size used
#define EXT2_FEATURE_COMPAT_DIR_INDEX       0x0020	// Directory indexing (HTree)

// features incompatible, should refuse if not implemented
#define EXT2_FEATURE_INCOMPAT_COMPRESSION   0x0001	// Disk/File compression is used
#define EXT2_FEATURE_INCOMPAT_FILETYPE      0x0002	 
#define EXT3_FEATURE_INCOMPAT_RECOVER       0x0004	 
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV   0x0008	 
#define EXT2_FEATURE_INCOMPAT_META_BG       0x0010	 

#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER 0x0001	// Sparse Superblock
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE   0x0002	// Large file support, 64-bit file size
#define EXT2_FEATURE_RO_COMPAT_BTREE_DIR    0x0004	// Binary tree sorted directory files

// known reserved inode entries
#define EXT2_BAD_INO	                  1	// bad blocks inode
#define EXT2_ROOT_INO	                  2	// root directory inode
#define EXT2_ACL_IDX_INO	              3	// ACL index inode (deprecated?)
#define EXT2_ACL_DATA_INO	              4	// ACL data inode (deprecated?)
#define EXT2_BOOT_LOADER_INO	          5	// boot loader inode
#define EXT2_UNDEL_DIR_INO	            6	// undelete directory inode

// access rights --
#define EXT2_S_IRUSR	0x0100	// user read
#define EXT2_S_IWUSR	0x0080	// user write
#define EXT2_S_IXUSR	0x0040	// user execute
#define EXT2_S_IRGRP	0x0020	// group read
#define EXT2_S_IWGRP	0x0010	// group write
#define EXT2_S_IXGRP	0x0008	// group execute
#define EXT2_S_IROTH	0x0004	// others read
#define EXT2_S_IWOTH	0x0002	// others write
#define EXT2_S_IXOTH	0x0001	// others execute

// possible values for i_flags
#define EXT2_SECRM_FL	        0x00000001	// secure deletion
#define EXT2_UNRM_FL	        0x00000002	// record for undelete
#define EXT2_COMPR_FL	        0x00000004	// compressed file
#define EXT2_SYNC_FL	        0x00000008	// synchronous updates
#define EXT2_IMMUTABLE_FL	    0x00000010	// immutable file
#define EXT2_APPEND_FL	      0x00000020	// append only
#define EXT2_NODUMP_FL	      0x00000040	// do not dump/delete file
#define EXT2_NOATIME_FL	      0x00000080	// do not update .i_atime
// Reserved for compression usage --
#define EXT2_DIRTY_FL	        0x00000100	// Dirty (modified)
#define EXT2_COMPRBLK_FL	    0x00000200	// compressed blocks
#define EXT2_NOCOMPR_FL	      0x00000400	// access raw compressed data
#define EXT2_ECOMPR_FL	      0x00000800	// compression error
// End of compression flags --
#define EXT2_BTREE_FL	        0x00001000	// b-tree format directory
#define EXT2_INDEX_FL	        0x00001000	// hash indexed directory
#define EXT2_IMAGIC_FL	      0x00002000	// AFS directory
#define EXT3_JOURNAL_DATA_FL	0x00004000	// journal file data
#define EXT2_RESERVED_FL	    0x80000000	// reserved for ext2 library

// File type
#define EXT2_FT_UNKNOWN	      0	// Unknown File Type
#define EXT2_FT_REG_FILE	    1	// Regular File
#define EXT2_FT_DIR	          2 // Directory File
#define EXT2_FT_CHRDEV	      3	// Character Device
#define EXT2_FT_BLKDEV	      4	// Block Device
#define EXT2_FT_FIFO	        5	// Buffer File
#define EXT2_FT_SOCK	        6	// Socket File
#define EXT2_FT_SYMLINK	      7	// Symbolic Link

typedef struct {
	uint32_t s_inodes_count;	  /* Inodes count */
	uint32_t s_blocks_count;	  /* Blocks count */
	uint32_t s_r_blocks_count;	  /* Reserved blocks count */
	uint32_t s_free_blocks_count; /* Free blocks count */
	uint32_t s_free_inodes_count; /* Free inodes count */
	uint32_t s_first_data_block;  /* First Data Block */
	uint32_t s_log_block_size;	  /* Block size */
	uint32_t s_log_frag_size;	  /* Fragment size */
	uint32_t s_blocks_per_group;  /* # Blocks per group */
	uint32_t s_frags_per_group;	  /* # Fragments per group */
	uint32_t s_inodes_per_group;  /* # Inodes per group */
	uint32_t s_mtime;			  /* Mount time */
	uint32_t s_wtime;			  /* Write time */
	uint16_t s_mnt_count;		  /* Mount count */
	uint16_t s_max_mnt_count;	  /* Maximal mount count */
	uint16_t s_magic;			  /* Magic signature */
	uint16_t s_state;			  /* File system state */
	uint16_t s_errors;			  /* Behaviour when detecting errors */
	uint16_t s_minor_rev_level;	  /* minor revision level */
	uint32_t s_lastcheck;		  /* time of last check */
	uint32_t s_checkinterval;	  /* max. time between checks */
	uint32_t s_creator_os;		  /* OS */
	uint32_t s_rev_level;		  /* Revision level */
	uint16_t s_def_resuid;		  /* Default uid for reserved blocks */
	uint16_t s_def_resgid;		  /* Default gid for reserved blocks */

  /*
    SA:5.11.2023: 
    The variables below are probably not needed for the very basic ext2 filesystem
  */
  uint32_t s_first_ino;			   /* First non-reserved inode */
	uint16_t s_inode_size;			   /* size of inode structure */
	uint16_t s_block_group_nr;		   /* block group # of this superblock */
	uint32_t s_feature_compat;		   /* compatible feature set */
	uint32_t s_feature_incompat;	   /* incompatible feature set */
	uint32_t s_feature_ro_compat;	   /* readonly-compatible feature set */
	uint8_t s_uuid[16];				   /* 128-bit uuid for volume */
	char s_volume_name[16];			   /* volume name */
	char s_last_mounted[64];		   /* directory where last mounted */
	uint32_t s_algorithm_usage_bitmap; /* For compression */

	uint8_t s_prealloc_blocks;	   /* Nr of blocks to try to preallocate*/
	uint8_t s_prealloc_dir_blocks; /* Nr to preallocate for dirs */
	uint16_t s_padding1;

	uint8_t s_journal_uuid[16]; /* uuid of journal superblock */
	uint32_t s_journal_inum;	/* inode number of journal file */
	uint32_t s_journal_dev;		/* device number of journal file */
	uint32_t s_last_orphan;		/* start of list of inodes to delete */
	uint32_t s_hash_seed[4];	/* HTREE hash seed */
	uint8_t s_def_hash_version; /* Default hash version to use */
	uint8_t s_reserved_char_pad;
	uint16_t s_reserved_word_pad;
	uint32_t s_default_mount_opts;
	uint32_t s_first_meta_bg; /* First metablock block group */
	uint32_t s_reserved[190]; /* Padding to the end of the block */
} ext2_superblock;

typedef struct {
	uint32_t bg_block_bitmap;
	uint32_t bg_inode_bitmap;
	uint32_t bg_inode_table;
	uint16_t bg_free_blocks_count;
	uint16_t bg_free_inodes_count;
	uint16_t bg_used_dirs_count;
	uint16_t bg_pad;
	uint32_t bg_reserved[3];
} ext2_group_desc;

typedef struct ext2_inode {
	uint16_t i_mode;		/* File mode */
	uint16_t i_uid;			/* Low 16 bits of Owner Uid */
	uint32_t i_size;		/* Size in bytes */
	uint32_t i_atime;		/* Access time */
	uint32_t i_ctime;		/* Creation time */
	uint32_t i_mtime;		/* Modification time */
	uint32_t i_dtime;		/* Deletion Time */
	uint16_t i_gid;			/* Low 16 bits of Group Id */
	uint16_t i_links_count; /* Links count */
	uint32_t i_blocks;		/* Blocks count */
	uint32_t i_flags;		/* File flags */
	union
	{
		struct
		{
			uint32_t l_i_reserved1;
		} linux1;
		struct
		{
			uint32_t h_i_translator;
		} hurd1;
		struct
		{
			uint32_t m_i_reserved1;
		} masix1;
	} osd1;				   /* OS dependent 1 */
	uint32_t i_block[15];  /* Pointers to blocks */
	uint32_t i_generation; /* File version (for NFS) */
	uint32_t i_file_acl;   /* File ACL */
	uint32_t i_dir_acl;	   /* Directory ACL */
	uint32_t i_faddr;	   /* Fragment address */
	union
	{
		struct linux2
		{
			uint8_t l_i_frag;  /* Fragment number */
			uint8_t l_i_fsize; /* Fragment size */
			uint16_t i_pad1;
			uint16_t l_i_uid_high; /* these 2 fields    */
			uint16_t l_i_gid_high; /* were reserved2[0] */
			uint32_t l_i_reserved2;
		} linux2;
		struct
		{
			uint8_t h_i_frag;  /* Fragment number */
			uint8_t h_i_fsize; /* Fragment size */
			uint16_t h_i_mode_high;
			uint16_t h_i_uid_high;
			uint16_t h_i_gid_high;
			uint32_t h_i_author;
		} hurd2;
		struct
		{
			uint8_t m_i_frag;  /* Fragment number */
			uint8_t m_i_fsize; /* Fragment size */
			uint16_t m_pad1;
			uint32_t m_i_reserved2[2];
		} masix2;
	} osd2; /* OS dependent 2 */
} ext2_inode;

#define EXT2_NAME_LEN 255

typedef struct {
	uint32_t ino;	    /* Inode number */
	uint16_t rec_len; /* Directory entry length */
	uint8_t name_len; /* Name length */
	uint8_t file_type;
	char name[];
} ext2_dir_entry;

typedef struct {
  ext2_superblock* sb;
  //uint32_t blocksize;
  bool is_readonly;
  uint32_t inode_reserved;
  uint32_t gdt_size_blocks; // amount of global descriptor tables
  uint64_t ino_upper_levels[4];
  //uint32_t inode_size;
  ext2_inode* root_dir;
} ext2_mount_info;

typedef struct {
  ext2_superblock* sb;
  uint64_t ino_upper_levels[4];
  bool is_readonly;
} ext2_fs_info;

static inline ext2_fs_info* EXT2_INFO(struct vfs_superblock *sb) {
  return sb->s_fs_info;
}

static inline ext2_superblock* EXT2_SB(struct vfs_superblock *sb) {
	return ((ext2_fs_info*)sb->s_fs_info)->sb;
}

static inline ext2_inode* EXT2_INODE(struct vfs_inode *inode) {
	return (ext2_inode*)inode->i_fs_info;
}

// super.c
void init_ext2_fs();
void exit_ext2_fs();
char *ext2_bread_block(struct vfs_superblock *sb, uint32_t iblock);
char *ext2_bread(struct vfs_superblock *sb, uint32_t iblock, uint32_t size);
void ext2_bwrite_block(struct vfs_superblock *sb, uint32_t iblock, char *buf);
void ext2_bwrite(struct vfs_superblock *sb, uint32_t iblock, char *buf, uint32_t size);
uint32_t ext2_create_block(struct vfs_superblock *sb);

void ext2_read_inode(struct vfs_inode* i);
void ext2_write_inode(struct vfs_inode* i);
struct ext2_inode* ext2_get_inode(struct vfs_superblock* sb, ino_t ino);
ext2_group_desc *ext2_get_group_desc(struct vfs_superblock* sb, uint32_t group);
void ext2_write_group_desc(struct vfs_superblock *sb, ext2_group_desc *gdp);

// file.c
ssize_t ext2_read_file(struct vfs_file* file, char *buf, size_t count, off_t ppos);
struct vfs_inode* ext2_alloc_inode(struct vfs_superblock* sb);

// inode.c
extern struct vfs_inode_operations ext2_dir_inode_operations;
extern struct vfs_inode_operations ext2_file_inode_operations;
extern struct vfs_inode_operations ext2_special_inode_operations;

extern struct vfs_file_operations ext2_file_operations;
extern struct vfs_file_operations ext2_dir_operations;
extern struct vfs_super_operations ext2_super_operations;
#endif