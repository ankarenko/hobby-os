
#ifndef _FSYS_H
#define _FSYS_H

#include <stdint.h>
#include <stdbool.h>
#include "kernel/include/types.h"
#include "kernel/system/time.h"

typedef struct _vfs_file {
  char name[100];
  uint32_t flags;
  uint32_t file_length;
  uint32_t id;
  bool eof;
  uint32_t position;
  uint32_t current_cluster;
  uint32_t device_id;
  struct time created;
  struct time modified;
} vfs_file;

/**
 *	Filesystem interface
 */
typedef struct _vfs_filesystem {
  char name[8];
  vfs_file(*directory)(const char* directory_name);
  void (*mount)();
  int32_t (*read)(vfs_file* file, uint8_t* buffer, uint32_t length);
  void (*close)(vfs_file*);
  vfs_file (*root)();
  bool (*ls)(const char* path);
  bool (*cd)(const char* path);
  vfs_file(*open)(const char* filename);
} vfs_filesystem;

/**
 *	File flags
 */
#define FS_FILE 0
#define FS_DIRECTORY 1
#define FS_INVALID 2
#define FS_ROOT_DIRECTORY 3
#define FS_NOT_FOUND 4


int32_t vfs_open(const char* fname, int32_t flags, ...);
int32_t vfs_read_file(vfs_file* file, uint8_t* buffer, uint32_t length);
void vfs_close_file(vfs_file* file);
bool vfs_ls(const char* path);
bool vfs_cd(const char* path);
vfs_file vfs_get_root(uint32_t device_id);
void vfs_register_file_system(vfs_filesystem*, uint32_t device_id);
void vfs_unregister_file_system(vfs_filesystem*);
void vfs_unregister_file_system_by_id(uint32_t device_id);

int32_t vfs_fread(int32_t fd, char *buf, int32_t count);
char* vfs_read(const char *path);

#endif
