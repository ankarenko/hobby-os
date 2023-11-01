#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

#include "kernel/fs/bpb.h"
#include "kernel/fs/vfs.h"
#include "kernel/util/debug.h"
#include "kernel/util/path.h"
#include "kernel/system/time.h"
#include "kernel/memory/malloc.h"

#include "./fat32.h"

#define FAT_LEGACY_FILENAME_SIZE      11  // 8.3
#define FAT_ENTRIES_COUNT             (SECTOR_SIZE / sizeof(uint32_t))
#define MAX_FILE_PATH                 100
typedef int32_t sect_t;

//! FAT FileSystem
static vfs_filesystem _fsys_fat;

/*
  fat[0] // 0xfffffff8 hard, 0xfffffff0 floppy, 0xfffffffa ram
  fat[1] // must be 0xffffffff
*/
uint32_t* fat = NULL; 

//! Mount info
static mount_info _minfo;
static bool fat32 = true;
static sect_t cur_dir = 0;
static unsigned char cur_path[MAX_FILE_PATH];

char* months[12] = {
  "jan",
  "feb",
  "mar",
  "apr",
  "may",
  "jun",
  "jul",
  "aug",
  "sep",
  "oct",
  "nov",
  "dec"
};

static void print_dir_record(p_directory p_dir) {
  uint8_t attr = p_dir->attrib;
  
  // check file is not SWP 
  if (attr && !(attr & (FAT_ENTRY_HIDDEN)) && strcmp(p_dir->ext, "SWP ") != 0) {
    char* type = attr & FAT_ENTRY_DIRECTORY? "d" : "f";
    printf("%s %s", p_dir->filename, type);

    if (p_dir->attrib != FAT_ENTRY_DIRECTORY) {
      struct time created = fat_to_time(p_dir->date_created, p_dir->time_created);
      struct time modified = fat_to_time(p_dir->last_mod_date, p_dir->last_mod_time);
      printf(" %d %s %d%d:%d%d", 
        created.day, 
        months[created.month - 1], 
        created.hour / 10, created.hour % 10,
        created.minute / 10, created.minute % 10);
    } else {
      printf("         ");
    }

    if (p_dir->file_size) {
      printf(" %d bytes", p_dir->file_size);
    }

    printf("\n");
  }
}

static void ls_dir(uint32_t offset) {
  p_directory p_dir = (p_directory)flpydsk_read_sector(offset);

  printf("\n");
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
static bool find_subdir(char* name, const sect_t dir_sector, vfs_file* p_file) {
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
        p_file->first_cluster = p_dir->first_cluster | (p_dir->first_cluster_hi_bytes << 16);
        p_file->file_length = p_dir->file_size;
        p_file->eof = false;
        p_file->f_pos = 0;
        p_file->flags = p_dir->attrib == FAT_ENTRY_DIRECTORY ? FS_DIRECTORY : FS_FILE;

        if (p_file->flags == FS_FILE) {
          p_file->created = fat_to_time(p_dir->date_created, p_dir->time_created);
          p_file->modified = fat_to_time(p_dir->last_mod_date, p_dir->last_mod_time);
        }
        return true;
      }
    }
  }

  return false;
}

static vfs_file open_file(const char* name, sect_t dir_sector) {
  vfs_file file;
  //! open subdirectory or file
  uint32_t cluster = 0;
  if (!find_subdir(name, dir_sector, &file)) {
    file.flags = FS_NOT_FOUND;
    return file;
    //PANIC("Cannot find file: %s", name);
  }

  if (cluster > SECTOR_SIZE) {
    PANIC("Cluster number is too big: %d", cluster);
  }

  return file;
}

static uint32_t get_start_cluster(vfs_file* file, loff_t ppos) {
  if (file->file_length <= ppos) {
    return FAT_MARK_EOF;
  }

  uint32_t cur_cluster = file->first_cluster;
  uint32_t ppos_cluster = ppos / (_minfo.sectors_per_cluster * SECTOR_SIZE);
  
  while (ppos_cluster > 0) {
    if (cur_cluster >= FAT_MARK_EOF) {
      return EOF;
    }
    cur_cluster = fat[cur_cluster];
    ppos_cluster--;
  }

  return cur_cluster;
}

static bool jmp_dir(const char* path, sect_t* dir_sector) {
  vfs_file file;
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
      vfs_file file;
      file.first_cluster = 0;
      if (!find_subdir(pathname, *dir_sector, &file)) {
        //PANIC("Cannot find directory: %s", pathname);
        return false;
      }

      if (cur[i] == '\/') {
        ++i;
      }

      cur = &cur[i];
      *dir_sector = cluster_to_sector(file.first_cluster);
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
    return false;
    //PANIC("Not found directory %s", path);
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

bool fat_ls(const char* path) {
  if (path == NULL) {
    ls_dir(cur_dir);

  } else {
    sect_t dir_sector = cur_dir;
    if (!jmp_dir(path, &dir_sector)) {
      return false;
      //PANIC("Not found directory %s", path);
    }

    ls_dir(dir_sector);
  }
  return true;
}

int32_t fat_fread(vfs_file* file, uint8_t* buffer, uint32_t length, loff_t ppos) {
  if (!file) {
    return -ENOENT;
  }

  if (length == 0) {
    return length;
  }

  uint8_t* cur = buffer;
  uint32_t cur_cluster = get_start_cluster(file, ppos);
  loff_t offset = ppos % (SECTOR_SIZE * _minfo.sectors_per_cluster);
  uint32_t left_len = length;

  do {
    if (cur_cluster >= FAT_MARK_EOF) {
      file->eof = true;
      break;
    } 

    sect_t sect_num = cluster_to_sector(cur_cluster);
    for (uint32_t i = 0; i < _minfo.sectors_per_cluster; ++i) {
      if (left_len > 0) {
        uint8_t* sector = flpydsk_read_sector(sect_num + i);
        uint32_t to_read = min(left_len, SECTOR_SIZE - offset);
        memcpy(cur, sector + offset, to_read);
        file->f_pos += to_read;
        left_len -= to_read;
        cur += to_read;

        if (offset) {
          offset = 0;
        }
      } 
    }

    cur_cluster = fat[cur_cluster];
  } while (left_len > 0);

  return length - left_len;
}

static void fat_close(vfs_file* file) {
  if (file)
    file->flags = FS_INVALID;
}



vfs_file fat_open(const char* path) {
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
    vfs_file file;
    file.flags = FS_NOT_FOUND;
    return file;
    //PANIC("Not found directory %s", path);
  }

  return open_file(filename, dir_sector);
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
  
  uint32_t size_fat_entries = (SECTOR_SIZE / sizeof(uint32_t)) * _minfo.fat_offset;
  fat = kcalloc(size_fat_entries, sizeof(uint32_t));

  for (int i = 0; i < _minfo.fat_size; ++i) {
    uint32_t* fat_ptr = (uint32_t*)flpydsk_read_sector(_minfo.fat_offset + i);
    memcpy((uint8_t*)fat + i * SECTOR_SIZE, fat_ptr, SECTOR_SIZE);  
  }

  // read fat table
  uint32_t* fat_ptr = (uint32_t*)flpydsk_read_sector(_minfo.fat_offset);
  if (fat[1] != 0xfffffff) {
    PANIC("Invalid fat table at sector: %d", _minfo.fat_offset);
  }

  cur_dir = _minfo.root_offset;
}

void fat_unmount() {
  kfree(fat);
}

vfs_file fat_get_rootdir() {  
  vfs_file rootdir;
  rootdir.eof = false;
  rootdir.device_id = 0;
  rootdir.flags = FS_ROOT_DIRECTORY;
  return rootdir;
}

void fat32_init() {
    //! initialize vfs_filesystem struct
  strcpy(_fsys_fat.name, "FAT32");
  _fsys_fat.mount = fat_mount;
  _fsys_fat.open = fat_open;
  _fsys_fat.read = fat_fread;
  _fsys_fat.close = fat_close;
  _fsys_fat.root = fat_get_rootdir; // TODO: needs to be on the same level with VFS
  _fsys_fat.ls = fat_ls;
  _fsys_fat.cd = fat_cd;
  
  //! register ourself to volume manager
  vfs_register_file_system(&_fsys_fat, 0);

  fat_mount();
}