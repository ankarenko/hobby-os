#ifndef FAT32_H_INCLUDED
#define FAT32_H_INCLUDED

#include "kernel/fs/fsys.h"

#define FAT_MARK_EOF        0x0ffffff8
#define FAT_MARK_FREE       0x00000000
#define FAT_MARK_ALLOCATED  0x00000002
#define FAT_MARK_RESERVED   0xfffffff6
#define FAT_MARK_DAMAGED    0xfffffff7

#define FAT_ENTRY_READ_ONLY    0x01
#define FAT_ENTRY_HIDDEN       0x02
#define FAT_ENTRY_SYSTEM       0x04
#define FAT_ENTRY_VOLUME_ID    0x08
#define FAT_ENTRY_DIRECTORY    0x10
#define FAT_ENTRY_ARCHIVE      0x20

#define ENTRY_SIZE              32
#define SECTOR_SIZE             512
#define DIR_ENTRIES             (SECTOR_SIZE / ENTRY_SIZE) 

#pragma pack(1)

typedef struct _directory {
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
} directory, *p_directory;

/*
  Filesystem mount info
*/
typedef struct _mount_info {
  uint32_t fat_offset;
  uint32_t fat_copy_offset;
  uint32_t root_offset;
  uint32_t fat_entry_size;
  uint32_t sectors_per_cluster;
  uint32_t fat_size;
  uint32_t fat_num_clusters;
  uint32_t data_offset;
} mount_info, *p_mount_info;

#pragma pack(0)

void fat32_init();

#endif