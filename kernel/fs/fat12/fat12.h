#ifndef FAT12_H_INCLUDED
#define FAT12_H_INCLUDED

#include <stdint.h>
#include "kernel/fs/vfs.h"

/**
 *	Directory entry
 */


#define FAT12_READ_ONLY 0x01
#define FAT12_HIDDEN 0x02
#define FAT12_SYSTEM 0x04
#define FAT12_VOLUME_ID 0x08
#define FAT12_DIRECTORY 0x10
#define FAT12_ARCHIVE 0x20

#pragma pack(1)
typedef struct _DIRECTORY {
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
} DIRECTORY, *PDIRECTORY;

/**
 *	Filesystem mount info
 */
typedef struct _MOUNT_INFO {
  uint32_t num_sectors;
  uint32_t fat_offset;
  uint32_t num_root_entries;
  uint32_t root_offset;
  uint32_t root_size;
  uint32_t fat_size;
  //uint32_t sector_size;
  uint32_t fat_entry_size;
} MOUNT_INFO, *PMOUNT_INFO;

#pragma pack(0)

//vfs_file fat12_directory(const char* directory_name);
//void fat12_read(vfs_file* file, uint8_t* buffer, uint32_t length);
//vfs_file fat12_open(const char* filename);
//void fat12_mount();

void fat12_initialize();

#endif
