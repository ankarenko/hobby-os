
#ifndef _FSYS_H
#define _FSYS_H

#include <stdbool.h>
#include <stdint.h>

#include "kernel/util/stat.h"
#include "kernel/util/ctype.h"
#include "kernel/system/time.h"

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

typedef struct {
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
} vfs_file;

typedef struct {
	vfs_file (*open)(const char* filename, mode_t mode);
  int32_t (*read)(vfs_file* file, uint8_t* buffer, uint32_t length, off_t ppos);
  int (*readdir)(struct vfs_file *dir, struct dirent *dirent, unsigned int count);
  ssize_t (*write)(vfs_file *file, const char *buf, size_t count, off_t ppos);
  int32_t (*close)(vfs_file*);
  off_t (*llseek)(vfs_file* file, off_t ppos, int);
} vfs_file_operations;

typedef struct {
  char name[8];
  vfs_file_operations fop;
  vfs_file (*root)();
  int32_t (*delete)(const char* path);
  void (*mount)();
  bool (*ls)(const char* path);
  bool (*cd)(const char* path);
  int32_t (*mkdir)(const char* path);
} vfs_filesystem;

typedef struct {
	const char *name;
	void (*mount)();
	//void (*unmount)(struct vfs_superblock *);
	//struct vfs_file_system_type *next;
} vfs_file_system_type;

typedef struct {
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
} vfs_inode_operations;

// vfs.c
bool vfs_ls(const char* path);
bool vfs_cd(const char* path);
vfs_file vfs_get_root(uint32_t device_id);
void vfs_register_file_system(vfs_filesystem*, uint32_t device_id);
void vfs_unregister_file_system(vfs_filesystem*);
void vfs_unregister_file_system_by_id(uint32_t device_id);

// open.c
int32_t vfs_close(int32_t fd);
int32_t vfs_open(const char* fname, int32_t flags, ...);
int vfs_fstat(int32_t fd, struct stat* stat);
int32_t vfs_delete(const char* fname);
int32_t vfs_mkdir(const char* dir_path);

// read_write.c
int32_t vfs_fread(int32_t fd, char* buf, int32_t count);
ssize_t vfs_fwrite(int32_t fd, char* buf, int32_t count);
char* vfs_read(const char* path);
off_t vfs_flseek(int32_t fd, off_t offset, int whence);

#endif
