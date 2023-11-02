#ifndef BPB_H
#define BPB_H

#include <stdint.h>

/*
  make sure 2 bytes field doesn't get aligned to 4 bytes
  there is a bug with __attribute__((packed)) though
  so I use the pragma directive instead 
  https://stackoverflow.com/questions/24887459/c-c-struct-packing-not-working
*/
#pragma pack(1) 

typedef struct _bios_parameter_block {
  uint8_t oem_name[8];
  uint16_t bytes_per_sector;
  uint8_t sect_per_cluster;
  uint16_t reserved_sectors;
  uint8_t number_of_fats;
  uint16_t num_dir_entries;
  uint16_t num_sectors;
  uint8_t media; // 0xf8 hard drive, 0xf0 usb flash
  uint16_t sectors_per_fat;
  uint16_t sectors_per_track;
  uint16_t heads_per_cyl;
  uint32_t hidden_sectors;
  uint32_t num_sectors_fat32;
} bios_parameter_block, *p_bios_parameter_block;

/*
  bios paramater block extended attributes
*/
typedef struct _bios_parameter_block_ext {
  uint32_t sectors_per_fat32;
  uint16_t flags;
  uint16_t version;
  uint32_t root_cluster;
  uint16_t fsinfo_cluster;
  uint16_t backup_boot;
  uint16_t reserved[6];
} bios_parameter_block_ext, *p_bios_parameter_block_ext;

/*
  boot sector
*/
typedef struct _boot_sector {
  uint8_t jump[3];      // zeros or jmp command, ignore it
  bios_parameter_block bpb;
  bios_parameter_block_ext bpb_ext;
  uint8_t code[446];    // code to jmp to
  uint8_t signature[2] // check if it is a loading sector
} bootsector, *p_bootsector;

#pragma pack(0)

#endif
