//****************************************************************************
//**
//**    bpb.h
//**		-Bios Paramater Block
//**
//****************************************************************************

#ifndef BPB_H
#define BPB_H

#include <stdint.h>

/**
 *	bios paramater block
 */
#pragma pack(1)

// there is a bug with __attribute__((packed)) 
// so I use pragma instead 
//https://stackoverflow.com/questions/24887459/c-c-struct-packing-not-working

typedef struct _BIOS_PARAMATER_BLOCK {
  uint8_t OEM_name[8];
  uint16_t bytes_per_sector;
  uint8_t sectors_per_cluster;
  uint16_t reserved_sectors;
  uint8_t number_of_fats;
  uint16_t num_dir_entries;
  uint16_t num_sectors;
  uint8_t media;
  uint16_t sectors_per_fat;
  uint16_t sectors_per_track;
  uint16_t heads_per_cyl;
  uint32_t hidden_sectors;
  uint32_t long_sectors;
} BIOSPARAMATERBLOCK, *PBIOSPARAMATERBLOCK;

/**
 *	bios paramater block extended attributes
 */
typedef struct _BIOS_PARAMATER_BLOCK_EXT {
  uint32_t sectors_per_fat32;
  uint16_t flags;
  uint16_t version;
  uint32_t root_cluster;
  uint16_t info_cluster;
  uint16_t backup_boot;
  uint16_t reserved[6];

} BIOSPARAMATERBLOCKEXT, *PBIOSPARAMATERBLOCKEXT;

/**
 *	boot sector
 */
typedef struct _BOOT_SECTOR {
  uint8_t ignore[3];  // first 3 bytes are ignored
  BIOSPARAMATERBLOCK bpb;
  BIOSPARAMATERBLOCKEXT bpb_ext;
  uint8_t filler[448];  // needed to make struct 512 bytes
} BOOTSECTOR, *PBOOTSECTOR;

#pragma pack(0)

#endif
