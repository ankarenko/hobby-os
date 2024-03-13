
#ifndef _FSYS_H
#define _FSYS_H

#include <stdbool.h>
#include <stdint.h>

#include "kernel/util/atomic.h"
#include "kernel/util/stat.h"
#include "kernel/util/ctype.h"
#include "kernel/fs/pipefs/pipe.h"
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

#define STDIN 0
#define STDOUT 1
#define STDERR 2
#define TERMINAL 3

/**
 * TODO: SA 2023-12-20 I suspect there are lots of memory leaks, so 
 * it needs to be taken care of at some point in the future
*/

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
  struct list_head d_cache_sibling; 
};

struct vfs_super_operations {
	struct vfs_inode* (*alloc_inode)(struct vfs_superblock *sb);
	void (*read_inode)(struct vfs_inode *);
	void (*write_inode)(struct vfs_inode *);
	void (*write_super)(struct vfs_superblock *);
};

struct kstat  {
  dev_t		st_dev;
  ino_t		st_ino;
  mode_t	st_mode;
  nlink_t	st_nlink;
  uid_t		st_uid;
  gid_t		st_gid;
  dev_t		st_rdev;
  off_t		st_size;
  struct timespec st_atim;
  struct timespec st_mtim;
  struct timespec st_ctim;
  blksize_t st_blksize;
  blkcnt_t	st_blocks;
};

// SA TODO: 2023-12-21 s_dirt flag will be useful
struct vfs_superblock {
	unsigned long s_blocksize;
	//int32_t s_dev;
	struct vfs_file_system_type* s_type;
	struct vfs_super_operations* s_op;
	unsigned long s_magic;
	struct vfs_dentry* s_root;
	char* mnt_devname;
	void* s_fs_info;
};

struct vfs_file_operations {
	int (*open)(struct vfs_inode *inode, struct vfs_file *file);
  int32_t (*read)(struct vfs_file* file, uint8_t* buffer, uint32_t length, off_t ppos);
  int (*readdir)(struct vfs_file *dir, struct dirent* dirent, uint32_t count);
  uint32_t (*write)(struct vfs_file *file, const char *buf, size_t count, off_t ppos);
  int32_t (*close)(struct vfs_file*);
  off_t (*llseek)(struct vfs_file* file, off_t ppos, int);
  int (*release)(struct vfs_inode *inode, struct vfs_file *file);
  unsigned int (*poll)(struct vfs_file *file, struct poll_table *pt);
  int (*ioctl)(struct vfs_inode *inode, struct vfs_file *file, unsigned int cmd, unsigned long arg);
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
	int32_t i_rdev;
	atomic_t i_count;
	struct timespec i_atime;
	struct timespec i_mtime;
	struct timespec i_ctime;
	uint32_t i_blocks;
	unsigned long i_blksize;
	uint32_t i_flags;
	uint32_t i_size;
	//struct semaphore i_sem;
  
  union {
		struct pipe	*i_pipe;
		void *i_cdev;
	};

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
	struct vfs_inode *(*create)(struct vfs_inode *dir, struct vfs_dentry *res, mode_t mode, int32_t dev);
	struct vfs_inode *(*lookup)(struct vfs_inode *dir, char* name);
	//int (*mkdir)(struct vfs_inode *, char *, int);
	//int (*rename)(struct vfs_inode *old_dir, struct vfs_dentry *old_dentry,
	//			  struct vfs_inode *new_dir, struct vfs_dentry *new_dentry);
	int (*unlink)(struct vfs_inode *dir, char* path);
	int (*mknod)(struct vfs_inode *, struct vfs_dentry *, int, int32_t);
	//void (*truncate)(struct vfs_inode *);
	//int (*setattr)(struct vfs_dentry *, struct iattr *);
	//int (*getattr)(struct vfs_mount *mnt, struct vfs_dentry *, struct kstat *);
};

#define MAX_FILENAME_SIZE 32

struct dirent {
	ino_t d_ino;			          /* Inode number */
	off_t d_off;			          /* Not an offset; see below */
	unsigned short d_reclen;    /* Length of this record */
	unsigned short d_type;	    /* Type of file; not supported by all filesystem types */
	char d_name[MAX_FILENAME_SIZE];			      /* Null-terminated filename */
};

struct vfs_file {
  char name[MAX_FILENAME_SIZE];
  unsigned int f_flags;
  off_t f_pos;
  mode_t f_mode;
  struct vfs_mount *f_vfsmnt;
  struct vfs_dentry* f_dentry;
  struct vfs_file_operations *f_op;
  atomic_t f_count; // to keep track of the references and release at the right time
  void *private_data;
};

// vfs.c
struct vfs_mount* do_mount(const char* fstype, int flags, const char* path);
void vfs_init(struct vfs_file_system_type* fs, char* dev_name);
void init_special_inode(struct vfs_inode* inode, mode_t mode, int32_t dev);
struct vfs_inode *init_inode();
int32_t vfs_mkdir(const char *path, mode_t mode);
int32_t vfs_ls(const char* path);
int32_t vfs_cd(const char* path);
int register_filesystem(struct vfs_file_system_type *fs);
int unregister_filesystem(struct vfs_file_system_type *fs);
int vfs_jmp(struct nameidata* nd, const char* path, int32_t flags, mode_t mode);

// open.c
int32_t vfs_close(int32_t fd);
int32_t vfs_open(const char* fname, int32_t flags, ...);
int vfs_fstat(int32_t fd, struct kstat* stat);
int vfs_stat(const char *path, struct kstat *stat);
int32_t vfs_delete(const char* fname);
struct vfs_dentry *alloc_dentry(struct vfs_dentry *parent, char *name);
struct vfs_file *alloc_vfs_file();
int generic_memory_readdir(struct vfs_file *file, struct dirent *dirent, uint32_t count);
int vfs_mknod(const char *path, int mode, int32_t dev);
struct vfs_dentry *vfs_search_virt_subdirs(struct vfs_dentry *dir, const char *name);
int32_t find_unused_fd_slot();

// read_write.c
int32_t vfs_fread(int32_t fd, char* buf, uint32_t count);
uint32_t vfs_fwrite(int32_t fd, char* buf, int32_t count);
int32_t vfs_read(const char *path, char** buf);
off_t vfs_flseek(int32_t fd, off_t offset, int whence);
off_t vfs_generic_llseek(struct vfs_file *file, off_t offset, int whence);

// namei.c
int vfs_unlink(const char *path, int flag);
void vfs_build_path_backward(struct vfs_dentry *dentry, char *path);

//cache.c
int vfs_cache(struct vfs_dentry* dentry);
int vfs_cache_remove(struct vfs_dentry* dentry);
struct vfs_dentry* vfs_cache_get(struct vfs_dentry *parent, char *name);
void vfs_cache_init();

//fcntl.c
int do_fcntl(int fd, int cmd, unsigned long arg);

#endif
