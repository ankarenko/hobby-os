#ifndef FAT32_H_INCLUDED
#define FAT32_H_INCLUDED

#include "kernel/fs/vfs.h"

#define FAT_ATTR_EOF        0x0ffffff8
#define FAT_ATTR_FREE       0x00000000
#define FAT_ATTR_ALLOCATED  0x00000002
#define FAT_ATTR_RESERVED   0xfffffff6
#define FAT_ATTR_DAMAGED    0xfffffff7

#define DIR_ATTR_READ_ONLY    0x01 // Indicates that writes to the file should fail.
#define DIR_ATTR_HIDDEN       0x02 // Indicates that normal directory listings should not show this file
#define DIR_ATTR_SYSTEM       0x04 // Indicates that this is an operating system file.
#define DIR_ATTR_VOLUME_ID    0x08 
#define DIR_ATTR_DIRECTORY    0x10 // Indicates that this file is actually a container for other files
#define DIR_ATTR_ARCHIVE      0x20 // This attribute supports backup utilities
#define DIR_ATTR_LONG_NAME    DIR_ATTR_READ_ONLY | DIR_ATTR_HIDDEN | DIR_ATTR_SYSTEM | DIR_ATTR_VOLUME_ID

#define FAT_ENTRY_SIZE       (sizeof(uint32_t))

//
//  The first byte of a dirent describes the dirent.  There is also a routine
//  to help in deciding how to interpret the dirent.
//
#define FAT_DIRENT_NEVER_USED            0x00
#define FAT_DIRENT_REALLY_0E5            0x05
#define FAT_DIRENT_DIRECTORY_ALIAS       0x2e
#define FAT_DIRENT_DELETED               0xe5

#pragma pack(1)

typedef struct _dir_item {
  uint8_t filename[8];
  uint8_t ext[3];
  uint8_t attrib;
  uint8_t reserved;
  uint8_t time_created_ms;
  uint16_t time_created;
  uint16_t date_created;
  uint16_t date_last_accessed;
  uint16_t first_cluster_hi_bytes;
  uint16_t last_mod_time;
  uint16_t last_mod_date;
  uint16_t first_cluster;
  uint32_t file_size;
} dir_item;

/*
  Filesystem mount info
*/
typedef struct _mount_info {
  uint32_t fat_offset;
  uint32_t fat_copy_offset;
  uint32_t root_offset;
  uint32_t fat_entry_size;
  uint32_t sect_per_cluster;
  uint32_t fat_size_sect;
  uint32_t fat_size_clusters;
  uint32_t data_offset;
  uint32_t bytes_per_sect;
  uint32_t sector_entries_count;
  uint32_t fat_entries_count;
} mount_info;

#pragma pack(0)

void fat32_init();

#endif