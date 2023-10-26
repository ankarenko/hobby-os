#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "kernel/fs/bpb.h"
#include "kernel/fs/flpydsk.h"
#include "kernel/util/debug.h"
#include "kernel/util/path.h"

#include "./fat32.h"

#define FAT_LEGACY_FILENAME_SIZE      11  // 8.3
#define FAT_ENTRIES_COUNT             (SECTOR_SIZE / sizeof(uint32_t))
#define MAX_FILE_PATH                 100
typedef int32_t sect_t;

//! FAT FileSystem
static FILESYSTEM _fsys_fat;

/*
  fat[0] // 0xfffffff8 hard, 0xfffffff0 floppy, 0xfffffffa ram
  fat[1] // must be 0xffffffff
*/
uint32_t fat[FAT_ENTRIES_COUNT]; 

//! Mount info
static mount_info _minfo;
static bool fat32 = true;
static sect_t cur_dir = 0;
static unsigned char cur_path[MAX_FILE_PATH];

static void print_dir_record(p_directory pkDir) {
  uint8_t attr = pkDir->attrib;
  
  // check file is not SWP 
  if (attr && !(attr & (FAT_ENTRY_HIDDEN)) && strcmp(pkDir->ext, "SWP ") != 0) {
    char* type = attr & FAT_ENTRY_DIRECTORY? "d" : "f";
    printf("%s %s : %db\n", pkDir->filename, type, pkDir->file_size);
  }
}

static void ls_dir(uint32_t offset) {
  p_directory p_dir = (p_directory)flpydsk_read_sector(offset);

  for (int32_t i = 0; i < DIR_ENTRIES; i++, p_dir++) {
    print_dir_record(p_dir);
  }
}

static void to_dos_name(const char* name, char* fname) {
  uint32_t len = strlen(name);

  memset(fname, ' ', FAT_LEGACY_FILENAME_SIZE + 1);
  char* ext = strchr(name, '.');
  
  if (ext == name) {
    memcpy(fname, ext, len);
    goto finish;
  }

  uint32_t count = ext? ext - name : len;
  memcpy(fname, name, count);
  if (ext) {
    memcpy(&fname[8], ext + 1, 3);
  }

  for (int i = 0; i < FAT_LEGACY_FILENAME_SIZE; ++i) {
    fname[i] = toupper(fname[i]);
  }

finish:
  fname[FAT_LEGACY_FILENAME_SIZE] = '\0';
}

static bool is_fat32(p_bootsector bootsector) {
  return true;
}

static bool is_fat_eof(uint32_t entry) {
  return entry >= FAT_MARK_EOF;
}

// TODO: bad code, fix of invalid root dir (to parse folder/..)  
static bool is_bug_root_dir(sect_t* dir_sector) {
  return _minfo.root_offset - _minfo.fat_num_clusters * _minfo.sectors_per_cluster == *dir_sector;
}

static sect_t cluster_to_sector(uint32_t cluster_num) {
  sect_t sect = _minfo.data_offset + 
    (cluster_num - _minfo.fat_num_clusters) * _minfo.sectors_per_cluster;

  if (is_bug_root_dir(&sect)) {
    sect = _minfo.root_offset;
  }

  return sect;
}

// in: dir sector, out: cluster
static bool find_subdir(char* name, const sect_t dir_sector, PFILE p_file) {
  char dos_name[FAT_LEGACY_FILENAME_SIZE + 1];
  to_dos_name(name, dos_name);
  p_directory p_dir = (p_directory)flpydsk_read_sector(dir_sector);

  for (int32_t i = 0; i < DIR_ENTRIES; i++, p_dir++) {
    if (p_dir->attrib && !(p_dir->attrib & FAT_ENTRY_HIDDEN)) {
      char entry_name[FAT_LEGACY_FILENAME_SIZE + 1];
      memcpy(entry_name, p_dir->filename, FAT_LEGACY_FILENAME_SIZE);
      entry_name[FAT_LEGACY_FILENAME_SIZE] = 0;

      if (strcmp(entry_name, dos_name) == 0) {
        strcpy(p_file->name, name);
        p_file->id = 0;
        p_file->current_cluster = p_dir->first_cluster | (p_dir->first_cluster_hi_bytes << 16);
        p_file->file_length = p_dir->file_size;
        p_file->eof = 0;
        p_file->flags = p_dir->attrib == FAT_ENTRY_DIRECTORY ? FS_DIRECTORY : FS_FILE;

        return true;
      }
    }
  }

  return false;
}

static FILE _open_file(const char* name, sect_t dir_sector) {
  FILE file;
  //! open subdirectory or file
  uint32_t cluster = 0;
  if (!find_subdir(name, dir_sector, &file)) {
    PANIC("Cannot find file: %s", name);
  }

  if (cluster > SECTOR_SIZE) {
    PANIC("Cluster number is too big: %d", cluster);
  }

  return file;
}

static bool jmp_dir(const char* path, sect_t* dir_sector) {
  FILE file;
  const char* cur = path;
  
  // check if root dir
  if (cur[0] == '\/') {
    *dir_sector = _minfo.root_offset;
    ++cur;
  }
  
  uint32_t len = strlen(path);
  if (!(len == 1 && path[0] == '.')) { // check if '.'

    while (*cur) {
      char pathname[FAT_LEGACY_FILENAME_SIZE + 1];
      int32_t i = 0;
      while (cur[i] != '\/' && cur[i] != '\0') {
        pathname[i] = cur[i];
        ++i;
      }

      pathname[i] = '\0';  // null terminate

      //! open subdirectory or file
      FILE file;
      file.current_cluster = 0;
      if (!find_subdir(pathname, *dir_sector, &file)) {
        PANIC("Cannot find directory: %s", pathname);
      }

      if (cur[i] == '\/') {
        ++i;
      }

      cur = &cur[i];
      *dir_sector = cluster_to_sector(file.current_cluster);
    }
  }

  return true;
}

bool fat_cd(const char* path) {
  uint32_t len_path = strlen(path);
  unsigned char norm_path[MAX_FILE_PATH];
  memcpy(norm_path, path, len_path);
  norm_path[len_path] = '\0';

  if (len_path >= MAX_FILE_PATH) {
    PANIC("Too big path: %d", len_path);
  }

  if (norm_path[len_path - 1] != '\/') {
    norm_path[len_path] = '\/';  
    len_path++;
    norm_path[len_path] = '\0';  
  }

  sect_t dir_sector = cur_dir;
  if (!jmp_dir(norm_path, &dir_sector)) {
    PANIC("Not found directory %s", path);
  }
  
  if (norm_path[0] == '\/') {
    cur_path[0] = '\0';
  }

  cur_dir = dir_sector;
  uint32_t len = strlen(cur_path);
  memcpy(&cur_path[len], norm_path, len_path);
  cur_path[len + len_path] = '\0';

  unsigned char out[MAX_FILE_PATH];
  out[0] = '\0';
  simplify_path(cur_path, out);
  len = strlen(out);
  memcpy(cur_path, out, len);
  cur_path[len] = '\0';
  //memcpy(cur_path + strlen(cur_path), postfix, strlen(postfix));
  return true;
}

char* pwd() {
  return cur_path;
}

void fat_ls(const char* path) {
  if (path == NULL) {
    ls_dir(cur_dir);
  } else {
    sect_t dir_sector = cur_dir;
    if (!jmp_dir(path, &dir_sector)) {
      PANIC("Not found directory %s", path);
    }

    ls_dir(dir_sector);
  }
}

void fat_read_file(PFILE file, uint8_t* buffer, uint32_t length) {
  if (file) {
    sect_t sect_num = cluster_to_sector(file->current_cluster);
    uint8_t* sector = flpydsk_read_sector(sect_num);
    //! copy block of memory
    memcpy(buffer, sector, SECTOR_SIZE);
    file->current_cluster = fat[file->current_cluster];

    if (file->current_cluster >= FAT_MARK_EOF) {
      file->eof = 1;
      return;
    }

    //! test for file corruption
    if (file->current_cluster == 0) {
      file->eof = 1;
      return;
    }
  }
}

static void fat_close(PFILE file) {
  if (file)
    file->flags = FS_INVALID;
}

FILE fat_open_file(const char* path) {
  char* filename = last_strchr(path, '\/');
  bool no_dir_jmp = false;
  if (filename) {
    *filename = '\0';
    ++filename; // ignore '/'
  } else {
    filename = path;
    no_dir_jmp = true;
  }

  sect_t dir_sector = cur_dir;
  if (!no_dir_jmp && !jmp_dir(path, &dir_sector)) {
    PANIC("Not found directory %s", path);
  }

  FILE file = _open_file(filename, dir_sector);

  /*
  printf("___________________________________________________________\n\n");
  uint32_t cluster = file.current_cluster;
  do {
    sect_t sect = cluster_to_sector(cluster);
    uint8_t* ptr = flpydsk_read_sector(sect);
    printf(ptr);
    cluster = fat[cluster];
  } while (cluster < FAT_MARK_EOF);
  printf("___________________________________________________________\n");
  */

  return file;
}

void fat_mount() {
  p_bootsector bootsector = (p_bootsector)flpydsk_read_sector(0);
  
  if (!is_fat32(bootsector)) {
    PANIC("The disk is not fat32 formatted", "");
  }

  _minfo.fat_size = fat32? 
    bootsector->bpb_ext.sectors_per_fat32 : (uint32_t)bootsector->bpb.sectors_per_fat;

  uint32_t bytes_per_sector = bootsector->bpb.bytes_per_sector;
  _minfo.fat_offset = bootsector->bpb.reserved_sectors;
  _minfo.fat_copy_offset = _minfo.fat_size + _minfo.fat_offset;
  _minfo.sectors_per_cluster = bootsector->bpb.sectors_per_cluster;
  _minfo.data_offset = bootsector->bpb.reserved_sectors + bootsector->bpb.number_of_fats * _minfo.fat_size;

  _minfo.fat_num_clusters = bootsector->bpb.number_of_fats;

  _minfo.fat_entry_size = ENTRY_SIZE;
  
  uint32_t offset_in_data_cluster = (bootsector->bpb_ext.root_cluster - bootsector->bpb.number_of_fats) * bootsector->bpb.sectors_per_cluster;
  _minfo.root_offset = bootsector->bpb.reserved_sectors + _minfo.fat_size * bootsector->bpb.number_of_fats - offset_in_data_cluster;
  
  // read fat table
  uint32_t* fat_ptr = (uint32_t*)flpydsk_read_sector(_minfo.fat_offset);
  if (fat_ptr[1] != 0xfffffff) {
    PANIC("Invalid fat table at sector: %d", _minfo.fat_offset);
  }

  memcpy(&fat, fat_ptr, FAT_ENTRIES_COUNT * sizeof(uint32_t));

  cur_dir = _minfo.root_offset;
}

FILE fat_get_rootdir() {  
  FILE rootdir;
  rootdir.eof = 0;
  rootdir.device_id = 0;
  rootdir.flags = FS_ROOT_DIRECTORY;
  return rootdir;
}

void fat32_init() {
    //! initialize filesystem struct
  strcpy(_fsys_fat.name, "FAT32");
  _fsys_fat.mount = fat_mount;
  _fsys_fat.open = fat_open_file;
  _fsys_fat.read = fat_read_file;
  _fsys_fat.close = fat_close;
  _fsys_fat.root = fat_get_rootdir;
  _fsys_fat.ls = fat_ls;
  _fsys_fat.cd = fat_cd;
  
  //! register ourself to volume manager
  vol_register_file_system(&_fsys_fat, 0);

  fat_mount();
}