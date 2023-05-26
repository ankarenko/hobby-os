
#include "./fat12.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

#include "../bpb.h"
#include "../flpydsk.h"

//! bytes per sector
#define SECTOR_SIZE 512
#define FAT12_MAX_FILE_SIZE 11  // 8.3
#define DIRECTORY_ENTRY_SIZE 32
#define END_OF_FILE_MARK 0xff8
#define NUM_DIRECTORY_ENTRIES (SECTOR_SIZE / DIRECTORY_ENTRY_SIZE)  // 16
//! FAT FileSystem
FILESYSTEM _fsys_fat;


//! Mount info
MOUNT_INFO _mount_info;

//! File Allocation Table (FAT)
uint8_t FAT[SECTOR_SIZE * 2];

void to_dos_filename(const char* filename, char* fname, uint32_t fname_length) {
  uint32_t i = 0;

  if (fname_length > FAT12_MAX_FILE_SIZE)
    return;

  if (!fname || !filename)
    return;

  //! set all characters in output name to spaces
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

/**
 *	Reads from a file
 */
void fat12_read(PFILE file, uint8_t* buffer, uint32_t Length) {
  if (file) {
    // in fat12 two FAT tables + root directory table = 9*2 + (244 * 32 / 512) = 18 + 14 = 32
    uint32_t data_offset = _mount_info.fat_size + _mount_info.root_size;

    //! starting physical sector
    uint32_t phys_sector = data_offset + (file->current_cluster - 1);

    //! read in sector
    uint8_t* sector = (uint8_t*)flpydsk_read_sector(phys_sector);

    //! copy block of memory
    memcpy(buffer, sector, SECTOR_SIZE);

    //! locate FAT sector
    // multiply by 1.5, because each fat entry is 12bit, for FAT16 it would be 2, for FAT32 - 4
    uint32_t FAT_Offset = file->current_cluster + file->current_cluster / 2;  
    uint32_t FAT_Sector = 1 + (FAT_Offset / SECTOR_SIZE);
    uint32_t entry_offset = FAT_Offset % SECTOR_SIZE;

    //! read 1st FAT sector
    sector = (uint8_t*)flpydsk_read_sector(FAT_Sector);
    memcpy(FAT, sector, SECTOR_SIZE);

    //! read 2nd FAT sector
    sector = (uint8_t*)flpydsk_read_sector(FAT_Sector + 1);
    memcpy(FAT + SECTOR_SIZE, sector, SECTOR_SIZE);

    //! read entry for next cluster
    uint16_t next_cluster = *(uint16_t*)&FAT[entry_offset];

    //! test if entry is odd or even
    // FAT12 record takes 12bit, but we can read only 16
    // so we need to filter out extra bits
    // there is a pattern
    if (file->current_cluster & 0x0001)
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
    file->current_cluster = next_cluster;
  }
}

/**
 *	Closes filestr
 */
void fat12_close(PFILE file) {
  if (file)
    file->flags = FS_INVALID;
}

/**
 *	Locates file or directory in root directory
 */
FILE fat12_look_root_directory(const char* name) {
  FILE file;
  uint8_t* buf;
  PDIRECTORY directory;

  //! get 8.3 directory name
  char dos_filename[FAT12_MAX_FILE_SIZE + 1];
  to_dos_filename(name, dos_filename, FAT12_MAX_FILE_SIZE);
  dos_filename[FAT12_MAX_FILE_SIZE] = 0;  // end of string

  //! 14 sectors per directory
  for (int32_t sector = 0; sector < _mount_info.root_size; sector++) {
    //! read in sector of root directory
    buf = (uint8_t*)flpydsk_read_sector(_mount_info.root_offset + sector);

    //! get directory info
    directory = (PDIRECTORY)buf;

    //! 16 entries per sector
    for (int32_t i = 0; i < NUM_DIRECTORY_ENTRIES; i++, directory++) {
  
      if (directory->attrib & FAT12_HIDDEN)
        continue;
      
      char name[FAT12_MAX_FILE_SIZE + 1];
      memcpy(name, directory->filename, FAT12_MAX_FILE_SIZE);
      name[FAT12_MAX_FILE_SIZE] = 0;

      //! find a match?
      if (strcmp(dos_filename, name) == 0) {
        //! found it, set up file info
        strcpy(file.name, name);
        file.id = 0;
        file.current_cluster = directory->first_cluster;
        file.file_length = directory->file_size;
        file.eof = 0;

        //! set file type
        if (directory->attrib == FAT12_DIRECTORY)
          file.flags = FS_DIRECTORY;
        else
          file.flags = FS_FILE;

        //! return file
        return file;
      }
    }
  }

  //! unable to find file
  file.flags = FS_INVALID;
  return file;
}

FILE fat12_look_subdir(FILE file, const char* filename) {
  //! get 8.3 directory name
  char dos_filename[FAT12_MAX_FILE_SIZE + 1];
  to_dos_filename(filename, dos_filename, FAT12_MAX_FILE_SIZE);
  dos_filename[FAT12_MAX_FILE_SIZE] = 0;

  //! read directory
  while (!file.eof) {
    //! read directory
    uint8_t buf[SECTOR_SIZE];
    fat12_read(&file, buf, SECTOR_SIZE);

    //! set directort
    PDIRECTORY pkDir = (PDIRECTORY)buf;

    for (uint32_t i = 0; i < DIRECTORY_ENTRY_SIZE; i++, pkDir++) {
      if (pkDir->attrib & FAT12_HIDDEN)
        continue;

      char name[FAT12_MAX_FILE_SIZE + 1];
      memcpy(name, pkDir->filename, FAT12_MAX_FILE_SIZE);
      name[FAT12_MAX_FILE_SIZE] = 0;

      //! match?
      if (strcmp(name, dos_filename) == 0) {
        //! found it, set up file info
        strcpy(file.name, filename);
        file.id = 0;
        file.current_cluster = pkDir->first_cluster;
        file.file_length = pkDir->file_size;
        file.eof = 0;

        //! set file type
        if (pkDir->attrib == FAT12_DIRECTORY)
          file.flags = FS_DIRECTORY;
        else
          file.flags = FS_FILE;

        //! return file
        return file;
      }
    }
  }

  //! unable to find file
  file.flags = FS_INVALID;
  return file;
}

bool fat12_ls(PFILE pfile) {
  FILE file = *pfile;
  
  if (file.flags != FS_DIRECTORY) {
    return false;
  }

  uint8_t buf[SECTOR_SIZE];
  
  while (!file.eof) {
    fat12_read(&file, buf, SECTOR_SIZE);
    //! set directort
    PDIRECTORY pkDir = (PDIRECTORY)buf;

    for (uint32_t i = 0; i < DIRECTORY_ENTRY_SIZE; i++) {
      if (!(pkDir->attrib & FAT12_HIDDEN)) {
        //! get current filename
        char name[FAT12_MAX_FILE_SIZE + 1];
        memcpy(name, pkDir->filename, FAT12_MAX_FILE_SIZE);
        name[FAT12_MAX_FILE_SIZE] = 0;
        printf(name);
      }
      pkDir++;
    }
  }
  
}

FILE fat12_open(const char* filename) {
  FILE file;
  const char* p = 0;
  bool rootDir = true;
  const char* path = (char*)filename;

  //! any '/'s in path?
  p = strchr(path, '\/');
  p = !p? path : p + 1;

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
  FILE ret;
  ret.flags = FS_INVALID;
  return ret;
}

bool check_fat12(uint16_t sectorsize) {
  return true;
  /*
  if (sectorsize == 0)
{
   fat_type = ExFAT;
}
else if(total_clusters < 4085)
{
   fat_type = FAT12;
}
else if(total_clusters < 65525)
{
   fat_type = FAT16;
}
else
{
   fat_type = FAT32;
}
  */
}

/**
 *	Mounts the filesystem
 */
void fat12_mount() {
  //! Boot sector info
  PBOOTSECTOR bootsector;

  //! read boot sector
  bootsector = (PBOOTSECTOR)flpydsk_read_sector(0);

  if (!check_fat12(bootsector->bpb.num_sectors)) {
    // panic
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
 *	Initialize filesystem
 */
void fat12_initialize() {
  //! initialize filesystem struct
  strcpy(_fsys_fat.name, "FAT12");
  _fsys_fat.mount = fat12_mount;
  _fsys_fat.open = fat12_open;
  _fsys_fat.read = fat12_read;
  _fsys_fat.close = fat12_close;
  _fsys_fat.directory = fat12_look_root_directory;
  //_fsys_fat.root = fat12_get_root_dir;
  _fsys_fat.ls = fat12_ls;

  //! register ourself to volume manager
  vol_register_file_system(&_fsys_fat, 0);

  //! mounr filesystem
  fat12_mount();
}
