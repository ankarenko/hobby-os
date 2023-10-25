#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "kernel/fs/bpb.h"
#include "kernel/fs/flpydsk.h"
#include "kernel/util/debug.h"

#include "./fat32.h"

#define FAT_LEGACY_FILENAME_SIZE      11  // 8.3
#define FAT_ENTRIES_COUNT             (SECTOR_SIZE / sizeof(uint32_t))

typedef int32_t sect_t;

/*
  fat[0] // 0xfffffff8 hard, 0xfffffff0 floppy, 0xfffffffa ram
  fat[1] // must be 0xffffffff
*/
uint32_t fat[FAT_ENTRIES_COUNT]; 

//! Mount info
static mount_info _minfo;
static bool fat32 = true;
static sect_t cur_dir = 0;

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
static bool find_subdir(char* name, const sect_t dir_sector, uint32_t* cluster) {
  char dos_name[FAT_LEGACY_FILENAME_SIZE + 1];
  to_dos_name(name, dos_name);
  p_directory p_dir = (p_directory)flpydsk_read_sector(dir_sector);

  for (int32_t i = 0; i < DIR_ENTRIES; i++, p_dir++) {
    if (p_dir->attrib && !(p_dir->attrib & FAT_ENTRY_HIDDEN)) {
      char entry_name[FAT_LEGACY_FILENAME_SIZE + 1];
      memcpy(entry_name, p_dir->filename, FAT_LEGACY_FILENAME_SIZE);
      entry_name[FAT_LEGACY_FILENAME_SIZE] = 0;

      if (strcmp(entry_name, dos_name) == 0) {
        *cluster = p_dir->first_cluster | (p_dir->first_cluster_hi_bytes << 16);
        return true;
      }
    }
  }

  return false;
}

static void ls_file(const char* name, sect_t dir_sector) {
  //! open subdirectory or file
  uint32_t cluster = 0;
  if (!find_subdir(name, dir_sector, &cluster)) {
    PANIC("Cannot find file: %s", name);
  }

  if (cluster > SECTOR_SIZE) {
    PANIC("Cluster number is too big: %d", cluster);
  }
  
  printf("__________________%s____________________\n", name);
  do {
    sect_t sect = cluster_to_sector(cluster);
    uint8_t* ptr = flpydsk_read_sector(sect);
    printf(ptr);
    cluster = fat[cluster];
  } while (cluster < FAT_MARK_EOF);
  printf("__________________END____________________\n");
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
      uint32_t cluster = 0;
      if (!find_subdir(pathname, *dir_sector, &cluster)) {
        PANIC("Cannot find directory: %s", pathname);
      }

      if (cur[i] == '\/') {
        ++i;
      }

      cur = &cur[i];
      *dir_sector = cluster_to_sector(cluster);
    }
  }

  return true;
}

static bool cd(const char* path) {
  sect_t dir_sector = cur_dir;
  if (!jmp_dir(path, &dir_sector)) {
    PANIC("Not found directory %s", path);
  }

  cur_dir = dir_sector;
  return true;
}

static void ls(const char* path) {
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

static void cat(const char* path) {
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

  ls_file(filename, dir_sector);
}

void fat32_mount() {
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

  int32_t dir_sector = 0;
  /*
  if (jmp_dir(".", &dir_sector, true)) {
    ls_dir(dir_sector);
    ls_file("grande.txt", dir_sector);
  }
  */
  cur_dir = _minfo.root_offset;
  cd("/diary");
  ls(NULL);
  cat("/diary/exec/program.exe");
}

void fat32_init() {
  
  fat32_mount();
}