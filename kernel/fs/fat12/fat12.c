
#include <stdbool.h>
#include "kernel/util/string/string.h"

#include "kernel/util/ctype.h"
#include "kernel/fs/bpb.h"
#include "kernel/fs/flpydsk.h"
#include "kernel/util/math.h"

#include "./fat12.h"

//! bytes per sector
#define FAT12_MAX_FILE_SIZE 11  // 8.3
#define DIRECTORY_ENTRY_SIZE 32
#define END_OF_FILE_MARK 0xff8
#define NUM_DIRECTORY_ENTRIES (FLPYDSK_SECTOR_SIZE / DIRECTORY_ENTRY_SIZE)  // 16
//! FAT FileSystem
static vfs_filesystem _fsys_fat;

//! Mount info
MOUNT_INFO _mount_info;

//! File Allocation Table (FAT)
uint8_t FAT[FLPYDSK_SECTOR_SIZE * 2];

void fat12_print_record(PDIRECTORY pkDir) {
  uint8_t attr = pkDir->attrib;
  
  // check file is not SWP 
  if (attr && !(attr & (FAT12_HIDDEN)) && strcmp(pkDir->ext, "SWP ") != 0) {
    char* type = attr & FAT12_DIRECTORY? "d" : "f";
    printf("%s %s : %d (bytes)\n", pkDir->filename, type, pkDir->file_size);
  }
}

bool fat12_check(PDIRECTORY pkDir, const char* dos_filename, const char* filename, vfs_file* file) {
  if (pkDir->attrib && !(pkDir->attrib & FAT12_HIDDEN)) {
    char name[FAT12_MAX_FILE_SIZE + 1];
    memcpy(name, pkDir->filename, FAT12_MAX_FILE_SIZE);
    name[FAT12_MAX_FILE_SIZE] = 0;

    if (strcmp(name, dos_filename) == 0) {
      strcpy(file->name, filename);
      file->id = 0;
      file->first_cluster = pkDir->first_cluster;
      file->file_length = pkDir->file_size;
      file->eof = 0;
      file->flags = pkDir->attrib == FAT12_DIRECTORY ? FS_DIRECTORY : FS_FILE;

      return true;
    }
  }

  return false;
}

void to_dos_filename(const char* filename, char* fname, uint32_t fname_length) {
  uint32_t i = 0;

  if (fname_length > FAT12_MAX_FILE_SIZE || !fname || !filename)
    return;

  memset(fname, ' ', fname_length);

  //! 8.3 filename
  uint32_t len = strlen(filename);
  for (i = 0; i < len && i < fname_length; i++) {
    if (filename[i] == '.' || i == 8)
      break;

    //! capitalize character and copy it over (we dont handle LFN format)
    fname[i] = toupper(filename[i]);
  }

  //! add extension if needed
  if (filename[i] == '.') {
    //! note: cant just copy over-extension might not be 3 chars
    for (int32_t k = 0; k < 3; k++) {
      ++i;
      if (filename[i])
        fname[8 + k] = filename[i];
    }
  }

  //! extension must be uppercase (we dont handle LFNs)
  for (i = 0; i < 3; i++)
    fname[8 + i] = toupper(fname[8 + i]);
}

void fat12_read(vfs_file* file, uint8_t* buffer, uint32_t Length) {
  if (file) {
    // in fat12 two FAT tables + root directory table = 9*2 + (244 * 32 / 512) = 18 + 14 = 32
    uint32_t data_offset = _mount_info.fat_size + _mount_info.root_size;

    //! starting physical sector
    uint32_t phys_sector = data_offset + (file->first_cluster - 1);

    //! read in sector
    uint8_t* sector = (uint8_t*)flpydsk_read_sector(phys_sector);

    //! copy block of memory
    memcpy(buffer, sector, FLPYDSK_SECTOR_SIZE);

    //! locate FAT sector
    // multiply by 1.5, because each fat entry is 12bit, for FAT16 it would be 2, for FAT32 - 4
    uint32_t FAT_Offset = file->first_cluster + file->first_cluster / 2;
    uint32_t FAT_Sector = 1 + (FAT_Offset / FLPYDSK_SECTOR_SIZE);
    uint32_t entry_offset = FAT_Offset % FLPYDSK_SECTOR_SIZE;

    //! read 1st FAT sector
    sector = (uint8_t*)flpydsk_read_sector(FAT_Sector);
    memcpy(FAT, sector, FLPYDSK_SECTOR_SIZE);

    //! read 2nd FAT sector
    sector = (uint8_t*)flpydsk_read_sector(FAT_Sector + 1);
    memcpy(FAT + FLPYDSK_SECTOR_SIZE, sector, FLPYDSK_SECTOR_SIZE);

    //! read entry for next cluster
    uint16_t next_cluster = *(uint16_t*)&FAT[entry_offset];

    //! test if entry is odd or even
    // FAT12 record takes 12bit, but we can read only 16
    // so we need to filter out extra bits
    // there is a pattern
    if (file->first_cluster & 0x0001)
      next_cluster >>= 4;  // grab high 12 bits
    else
      next_cluster &= 0x0FFF;  // grab low 12 bits

    if (next_cluster >= END_OF_FILE_MARK) {
      file->eof = 1;
      return;
    }

    //! test for file corruption
    if (next_cluster == 0) {
      file->eof = 1;
      return;
    }

    //! set next cluster
    file->first_cluster = next_cluster;
  }
}

/**
 *	Closes filestr
 */
void fat12_close(vfs_file* file) {
  if (file)
    file->flags = FS_INVALID;
}

/**
 *	Locates file or directory in root directory
 */
vfs_file fat12_look_root_directory(const char* filename) {
  vfs_file file;

  char dos_filename[FAT12_MAX_FILE_SIZE + 1];
  to_dos_filename(filename, dos_filename, FAT12_MAX_FILE_SIZE);
  dos_filename[FAT12_MAX_FILE_SIZE] = 0;

  for (int32_t sector = 0; sector < _mount_info.root_size; sector++) {
    PDIRECTORY directory = (PDIRECTORY)flpydsk_read_sector(_mount_info.root_offset + sector);

    for (int32_t i = 0; i < NUM_DIRECTORY_ENTRIES; i++, directory++) {
      if (fat12_check(directory, dos_filename, filename, &file)) {
        return file;
      }
    }
  }

  file.flags = FS_INVALID;
  return file;
}

vfs_file fat12_look_subdir(vfs_file file, const char* filename) {
  char dos_filename[FAT12_MAX_FILE_SIZE + 1];
  to_dos_filename(filename, dos_filename, FAT12_MAX_FILE_SIZE);
  dos_filename[FAT12_MAX_FILE_SIZE] = 0;

  while (!file.eof) {
    uint8_t buf[FLPYDSK_SECTOR_SIZE];
    fat12_read(&file, buf, FLPYDSK_SECTOR_SIZE);
    PDIRECTORY pkDir = (PDIRECTORY)buf;

    for (uint32_t i = 0; i < NUM_DIRECTORY_ENTRIES; i++, pkDir++) {
      if (fat12_check(pkDir, dos_filename, filename, &file)) {
        return file;
      }
    }
  }

  //! unable to find file
  file.flags = FS_INVALID;
  return file;
}

void fat12_ls_rootdir() {
  for (int32_t sector = 0; sector < _mount_info.root_size; sector++) {
    PDIRECTORY directory = (PDIRECTORY)flpydsk_read_sector(_mount_info.root_offset + sector);

    for (int32_t i = 0; i < NUM_DIRECTORY_ENTRIES; i++, directory++) {
      fat12_print_record(directory);
    }
  }
}

void fat12_ls_subdir(vfs_file file) {
  while (!file.eof) {
    uint8_t buf[FLPYDSK_SECTOR_SIZE];
    fat12_read(&file, buf, FLPYDSK_SECTOR_SIZE);
    PDIRECTORY pkDir = (PDIRECTORY)buf;

    for (uint32_t i = 0; i < NUM_DIRECTORY_ENTRIES; i++, pkDir++) {
      fat12_print_record(pkDir);
    }
  }
}


vfs_file fat12_get_rootdir() {  
  vfs_file rootdir;
  rootdir.eof = 0;
  rootdir.device_id = 0;
  rootdir.flags = FS_ROOT_DIRECTORY;
  return rootdir;
}

bool fat12_ls(vfs_file* pfile) {
  if (pfile && !(pfile->flags & (FS_DIRECTORY | FS_ROOT_DIRECTORY))) {
    return false;
  }

  pfile->flags == FS_ROOT_DIRECTORY ? 
    fat12_ls_rootdir() : fat12_ls_subdir(*pfile);
}

vfs_file fat12_open(const char* filename) {
  vfs_file file;
  const char* p = 0;
  bool rootDir = true;
  const char* path = (char*)filename;

  //! any '/'s in path?
  p = strchr(path, '\/');
  p = !p ? path : p + 1;

  while (p) {
    //! get pathname
    char pathname[FAT12_MAX_FILE_SIZE + 1];
    int32_t i = 0;
    while (p[i] != '\/' && p[i] != '\0') {
      pathname[i] = p[i];
      ++i;
    }
    pathname[i] = 0;  // null terminate

    //! open subdirectory or file
    if (rootDir) {
      file = fat12_look_root_directory(pathname);
      rootDir = false;
    } else {
      file = fat12_look_subdir(file, pathname);
    }

    //! found directory or file?
    if (file.flags == FS_INVALID)
      break;

    //! found file?
    if (file.flags == FS_FILE)
      return file;

    //! find next '\'
    p = strchr(p + 1, '\/');
    if (p)
      p++;
  }

  if (file.flags == FS_DIRECTORY) {
    return file;
  }

  //! unable to find
  vfs_file ret;
  ret.flags = FS_INVALID;
  return ret;
}

bool check_fat12(uint16_t sectorsize) {
  return true;
}

void fat12_mount() {
  //! Boot sector info
  p_bootsector bootsector;

  //! read boot sector
  bootsector = (p_bootsector)flpydsk_read_sector(0);

  if (!check_fat12(bootsector->bpb.num_sectors)) {
    // PANIC
  }

  // FAT12 STRUCTURE
  //
  // | Boot Sector                                 |
  // | Extra Reserved Sectors	                     |
  // | File Allocation Table 1                     |
  // | File Allocation Table 2	                   |
  // | Root Directory (FAT12/FAT16 Only)           |
  // | Data Region containng files and directories |

  //! store mount info
  _mount_info.num_sectors = bootsector->bpb.num_sectors;
  _mount_info.fat_offset = 1;
  _mount_info.fat_size = bootsector->bpb.number_of_fats * bootsector->bpb.sectors_per_fat;
  _mount_info.fat_entry_size = 12;  //?
  _mount_info.num_root_entries = bootsector->bpb.num_dir_entries;
  _mount_info.root_offset = bootsector->bpb.reserved_sectors + _mount_info.fat_size;
  _mount_info.root_size = div_ceil(bootsector->bpb.num_dir_entries << 5, bootsector->bpb.bytes_per_sector);
}

/**
 *	Initialize vfs_filesystem
 */
void fat12_initialize() {
  //! initialize vfs_filesystem struct
  strcpy(_fsys_fat.name, "FAT12");
  _fsys_fat.mount = fat12_mount;
  _fsys_fat.fop.open = fat12_open;
  _fsys_fat.fop.read = fat12_read;
  _fsys_fat.fop.close = fat12_close;
  _fsys_fat.root = fat12_get_rootdir;
  _fsys_fat.ls = fat12_ls;

  //! register ourself to volume manager
  vfs_register_file_system(&_fsys_fat, 0);

  //! mounr vfs_filesystem
  fat12_mount();
}
