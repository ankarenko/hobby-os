
#ifndef _FSYS_H
#define _FSYS_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "kernel/system/time.h"

#define DEVICE_MAX 26

/**
 *	File flags
 */
#define FS_FILE 0
#define FS_DIRECTORY 1
#define FS_INVALID 2
#define FS_ROOT_DIRECTORY 3
#define FS_NOT_FOUND 4

typedef struct _vfs_file {
  char name[100];
  uint32_t flags;
  uint32_t file_length;
  uint32_t id;
  bool eof;
  uint32_t first_cluster;
  uint32_t device_id;
  struct time created;
  struct time modified;
  loff_t f_pos;
  mode_t f_mode;
} vfs_file;

typedef struct _vfs_filesystem {
  char name[8];
  vfs_file (*open)(const char* filename, mode_t mode);
  int32_t (*read)(vfs_file* file, uint8_t* buffer, uint32_t length, loff_t ppos);
  void (*close)(vfs_file*);
  loff_t (*llseek)(vfs_file* file, loff_t ppos, int);
  vfs_file (*root)();
  int32_t (*delete)(const char* path);
  void (*mount)();
  bool (*ls)(const char* path);
  bool (*cd)(const char* path);
  int32_t (*mkdir)(const char* path);
} vfs_filesystem;

// vfs.c
bool vfs_ls(const char* path);
bool vfs_cd(const char* path);
vfs_file vfs_get_root(uint32_t device_id);
void vfs_register_file_system(vfs_filesystem*, uint32_t device_id);
void vfs_unregister_file_system(vfs_filesystem*);
void vfs_unregister_file_system_by_id(uint32_t device_id);

// open.c
void vfs_close_file(vfs_file* file);
int32_t vfs_open(const char* fname, int32_t flags, ...);
int vfs_fstat(int32_t fd, struct stat* stat);
int32_t vfs_delete(const char* fname);
int32_t vfs_mkdir(const char* dir_path);

// read_write.c
int32_t vfs_fread(int32_t fd, char* buf, int32_t count);
char* vfs_read(const char* path);
loff_t vfs_flseek(int32_t fd, loff_t offset, int whence);

#endif
