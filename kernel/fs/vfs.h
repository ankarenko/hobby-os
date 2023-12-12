
#ifndef _FSYS_H
#define _FSYS_H

#include <stdbool.h>
#include <stdint.h>

#include "kernel/util/stat.h"
#include "kernel/util/ctype.h"
#include "kernel/system/time.h"
#include "kernel/util/list.h"

#define DEVICE_MAX 26
#define BYTES_PER_SECTOR 512 
/**
 *	File flags
 */
#define FS_FILE 0
#define FS_DIRECTORY 1
#define FS_INVALID 2
#define FS_ROOT_DIRECTORY 3
#define FS_NOT_FOUND 4

typedef int32_t sect_t;

typedef struct {
  size_t index;
  sect_t dir_sector;
} table_entry;

struct vfs_dentry {
	struct vfs_inode* d_inode;
	struct vfs_dentry* d_parent;
	char *d_name;
	struct vfs_superblock* d_sb;
	struct list_head d_subdirs;
	struct list_head d_sibling;
};

struct vfs_super_operations {
	struct vfs_inode* (*alloc_inode)(struct vfs_superblock *sb);
	void (*read_inode)(struct vfs_inode *);
	void (*write_inode)(struct vfs_inode *);
	void (*write_super)(struct vfs_superblock *);
};

struct vfs_superblock {
	unsigned long s_blocksize;
	//dev_t s_dev;
	struct vfs_file_system_type* s_type;
	struct vfs_super_operations* s_op;
	unsigned long s_magic;
	struct vfs_dentry* s_root;
	char* mnt_devname;
	void* s_fs_info;
};

struct vfs_file_operations {
	//vfs_file (*open)(const char* filename, mode_t mode);
  int32_t (*read)(struct vfs_file* file, uint8_t* buffer, uint32_t length, off_t ppos);
  int (*readdir)(struct vfs_file *dir, struct dirent *dirent, unsigned int count);
  ssize_t (*write)(struct vfs_file *file, const char *buf, size_t count, off_t ppos);
  int32_t (*close)(struct vfs_file*);
  off_t (*llseek)(struct vfs_file* file, off_t ppos, int);
};

struct vfs_filesystem {
  char name[8];
  struct vfs_file_operations fop;
  //vfs_file (*root)();
  int32_t (*delete)(const char* path);
  void (*mount)();
  bool (*ls)(const char* path);
  bool (*cd)(const char* path);
  int32_t (*mkdir)(const char* path);
};

struct vfs_inode {
	unsigned long i_ino;
	mode_t i_mode;
	unsigned int i_nlink;
	uid_t i_uid;
	gid_t i_gid;
	dev_t i_rdev;
	//atomic_t i_count;
	//struct timespec i_atime;
	//struct timespec i_mtime;
	//struct timespec i_ctime;
	uint32_t i_blocks;
	unsigned long i_blksize;
	uint32_t i_flags;
	uint32_t i_size;
	//struct semaphore i_sem;
	//struct pipe *i_pipe;
	//struct address_space i_data;
	struct vfs_inode_operations *i_op;
	struct vfs_file_operations *i_fop;
	struct vfs_superblock *i_sb;
	void *i_fs_info;
};

struct nameidata {
	struct vfs_dentry* dentry;
	struct vfs_mount* mnt;
};

struct vfs_mount {
	struct vfs_dentry *mnt_root;
	struct vfs_dentry *mnt_mountpoint;
	struct vfs_superblock *mnt_sb;
	char *mnt_devname;
	struct list_head sibling;
};

struct vfs_file_system_type {
	const char* name;
  struct vfs_mount* (*mount)(struct vfs_file_system_type *, char *, char *);
	//void (*unmount)(struct vfs_superblock *);
	struct vfs_file_system_type* next;
};

struct vfs_inode_operations {
	//struct vfs_inode *(*create)(struct vfs_inode *dir, struct vfs_dentry *dentry, mode_t mode);
	struct vfs_inode *(*lookup)(struct vfs_inode *dir, struct vfs_dentry *dentry);
	//int (*mkdir)(struct vfs_inode *, char *, int);
	//int (*rename)(struct vfs_inode *old_dir, struct vfs_dentry *old_dentry,
	//			  struct vfs_inode *new_dir, struct vfs_dentry *new_dentry);
	//int (*unlink)(struct vfs_inode *dir, struct vfs_dentry *dentry);
	//int (*mknod)(struct vfs_inode *, struct vfs_dentry *, int, dev_t);
	//void (*truncate)(struct vfs_inode *);
	//int (*setattr)(struct vfs_dentry *, struct iattr *);
	//int (*getattr)(struct vfs_mount *mnt, struct vfs_dentry *, struct kstat *);
};

struct dirent {
	ino_t d_ino;			        /* Inode number */
	off_t d_off;			        /* Not an offset; see below */
	unsigned short d_reclen;  /* Length of this record */
	unsigned short d_type;	  /* Type of file; not supported by all filesystem types */
	char d_name[];			      /* Null-terminated filename */
};

struct vfs_file {
  char name[12];
  uint32_t flags;
  uint32_t file_length;
  uint32_t id;
  bool eof;
  uint32_t first_cluster;
  uint32_t device_id;
  struct time created;
  struct time modified;
  off_t f_pos;
  mode_t f_mode;
  table_entry p_table_entry;
  struct vfs_dentry* f_dentry;
  struct vfs_file_operations *f_op;
};

// vfs.c
struct vfs_mount* do_mount(const char* fstype, int flags, const char* path);
void vfs_init(struct vfs_file_system_type* fs, char* dev_name);
void init_ext2_fs();
void exit_ext2_fs();

//bool vfs_ls(const char* path);
//bool vfs_cd(const char* path);
//vfs_file vfs_get_root(uint32_t device_id);
int register_filesystem(struct vfs_file_system_type *fs);
int unregister_filesystem(struct vfs_file_system_type *fs);

// open.c
int32_t vfs_close(int32_t fd);
int32_t vfs_open(const char* fname, int32_t flags, ...);
int vfs_fstat(int32_t fd, struct stat* stat);
int32_t vfs_delete(const char* fname);
int32_t vfs_mkdir(const char* dir_path);
struct vfs_dentry *alloc_dentry(struct vfs_dentry *parent, char *name);

// read_write.c
int32_t vfs_fread(int32_t fd, char* buf, int32_t count);
ssize_t vfs_fwrite(int32_t fd, char* buf, int32_t count);
char* vfs_read(const char* path);
off_t vfs_flseek(int32_t fd, off_t offset, int whence);

#endif
