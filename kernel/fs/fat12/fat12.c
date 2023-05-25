
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
  for (i = 0; i < strlen(filename) - 1 && i < fname_length; i++) {
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
    //! starting physical sector
    uint32_t phys_sector = 32 + (file->current_cluster - 1);

    //! read in sector
    uint8_t* sector = (uint8_t*)flpydsk_read_sector(phys_sector);

    //! copy block of memory
    memcpy(buffer, sector, 512);

    //! locate FAT sector
    uint32_t FAT_Offset = file->current_cluster + (file->current_cluster / 2);  // multiply by 1.5
    uint32_t FAT_Sector = 1 + (FAT_Offset / SECTOR_SIZE);
    uint32_t entry_offset = FAT_Offset % SECTOR_SIZE;

    //! read 1st FAT sector
    sector = (uint8_t*)flpydsk_read_sector(FAT_Sector);
    memcpy(FAT, sector, 512);

    //! read 2nd FAT sector
    sector = (uint8_t*)flpydsk_read_sector(FAT_Sector + 1);
    memcpy(FAT + SECTOR_SIZE, sector, 512);

    //! read entry for next cluster
    uint16_t next_cluster = *(uint16_t*)&FAT[entry_offset];

    //! test if entry is odd or even
    if (file->current_cluster & 0x0001)
      next_cluster >>= 4;  // grab high 12 bits
    else
      next_cluster &= 0x0FFF;  // grab low 12 bits

    //! test for end of file
    if (next_cluster >= 0xff8) {
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
 *	Locates file or directory in root directory
 */
FILE fat12_find_file_or_directory(const char* name) {
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
    for (int32_t i = 0; i < NUM_DIRECTORY_ENTRIES; i++) {
      // check if file is not hidden
      if (!(directory->attrib & FAT12_HIDDEN)) {
        //! get current filename
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
          file.file_length = directory->file_size;

          //! set file type
          if (directory->attrib == FAT12_DIRECTORY)
            file.flags = FS_DIRECTORY;
          else
            file.flags = FS_FILE;

          //! return file
          return file;
        }
      }
      //! go to next directory
      directory++;
    }
  }

  //! unable to find file
  file.flags = FS_INVALID;
  return file;
}

FILE fat12_open_subdir(FILE kFile, const char* filename) {
  FILE file;

  //! get 8.3 directory name
  char dos_filename[FAT12_MAX_FILE_SIZE + 1];
  to_dos_filename(filename, dos_filename, FAT12_MAX_FILE_SIZE);
  dos_filename[FAT12_MAX_FILE_SIZE] = 0;

  //! read directory
  while (!kFile.eof) {
    //! read directory
    uint8_t buf[SECTOR_SIZE];
    fat12_read(&file, buf, SECTOR_SIZE);

    //! set directort
    PDIRECTORY pkDir = (PDIRECTORY)buf;

    //! 16 entries in buffer
    for (uint32_t i = 0; i < DIRECTORY_ENTRY_SIZE; i++) {
      //! get current filename
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
        file.file_length = pkDir->file_size;

        //! set file type
        if (pkDir->attrib == FAT12_DIRECTORY)
          file.flags = FS_DIRECTORY;
        else
          file.flags = FS_FILE;

        //! return file
        return file;
      }

      //! go to next entry
      pkDir++;
    }
  }

  //! unable to find file
  file.flags = FS_INVALID;
  return file;
}

FILE fat12_open(const char* filename) {
  FILE file;
  char* p = 0;
  bool rootDir = true;
  char* path = (char*)filename;

  //! any '\'s in path?
  p = strchr(path, '\\');

  if (!p) {
    //! nope, must be in root directory, search it
    file = fat12_find_file_or_directory(path);

    //! found file?
    if (file.flags == FS_FILE)
      return file;

    //! unable to find
    FILE ret;
    ret.flags = FS_INVALID;
    return ret;
  }

  //! go to next character after first '\'

  p++;

  while (p) {
    //! get pathname
    char pathname[NUM_DIRECTORY_ENTRIES];
    int i = 0;
    for (i = 0; i < NUM_DIRECTORY_ENTRIES; i++) {
      //! if another '\' or end of line is reached, we are done
      if (p[i] == '\\' || p[i] == '\0')
        break;

      //! copy character
      pathname[i] = p[i];
    }
    pathname[i] = 0;  // null terminate

    //! open subdirectory or file
    if (rootDir) {
      //! search root directory - open pathname
      file = fat12_find_file_or_directory(pathname);
      rootDir = false;
    } else {
      //! search a subdirectory instead for pathname
      file = fat12_open_subdir(file, pathname);
    }

    //! found directory or file?
    if (file.flags == FS_INVALID)
      break;

    //! found file?
    if (file.flags == FS_FILE)
      return file;

    //! find next '\'
    p = strchr(p + 1, '\\');
    if (p)
      p++;
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

  //! store mount info
  _mount_info.num_sectors = bootsector->bpb.num_sectors;
  _mount_info.fat_offset = 1;
  _mount_info.fat_size = bootsector->bpb.sectors_per_fat;
  _mount_info.fat_entry_size = 12;  //?
  _mount_info.num_root_entries = bootsector->bpb.num_dir_entries;
  _mount_info.root_offset = bootsector->bpb.reserved_sectors + bootsector->bpb.number_of_fats * bootsector->bpb.sectors_per_fat;
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

  //! register ourself to volume manager
  vol_register_file_system(&_fsys_fat, 0);

  //! mounr filesystem
  fat12_mount();
}
