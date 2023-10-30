
#ifndef _FSYS_H
#define _FSYS_H

#include <stdint.h>
#include "kernel/include/types.h"
#include "kernel/system/time.h"

typedef struct _FILE {
  char name[32];
  uint32_t flags;
  uint32_t file_length;
  uint32_t id;
  uint32_t eof;
  uint32_t position;
  uint32_t current_cluster;
  uint32_t device_id;
  struct time created;
  struct time modified;
} FILE, *PFILE;

/**
 *	Filesystem interface
 */
typedef struct _FILE_SYSTEM {
  char name[8];
  FILE(*directory)(const char* directory_name);
  void (*mount)();
  void (*read)(PFILE file, uint8_t* buffer, uint32_t length);
  void (*close)(PFILE);
  FILE (*root)();
  void (*ls)(const char* path);
  void (*cd)(const char* path);
  FILE(*open)(const char* filename);
} FILESYSTEM, *PFILESYSTEM;

/**
 *	File flags
 */
#define FS_FILE 0
#define FS_DIRECTORY 1
#define FS_INVALID 2
#define FS_ROOT_DIRECTORY 3

FILE vol_open_file(const char* fname);
void vol_read_file(PFILE file, uint8_t* buffer, uint32_t length);
void vol_close_file(PFILE file);
void vol_ls(const char* path);
void vol_cd(const char* path);
FILE vol_get_root(uint32_t device_id);
void vol_register_file_system(PFILESYSTEM, uint32_t device_id);
void vol_unregister_file_system(PFILESYSTEM);
void vol_unregister_file_system_by_id(uint32_t device_id);

#endif
