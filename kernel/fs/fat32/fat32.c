#include "./fat32.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "kernel/fs/bpb.h"
#include "kernel/fs/flpydsk.h"
#include "kernel/fs/vfs.h"
#include "kernel/memory/malloc.h"
#include "kernel/system/time.h"
#include "kernel/util/debug.h"
#include "kernel/util/path.h"

#define FAT_LEGACY_FILENAME_SIZE 11  // 8.3
#define MAX_FILE_PATH 100

/*
  fat[0] // 0xfffffff8 hard, 0xfffffff0 floppy, 0xfffffffa ram
  fat[1] // must be 0xffffffff
*/
uint32_t* fat = NULL;
typedef int32_t sect_t;
static vfs_filesystem fsys_fat;
static mount_info minfo;
static bool fat32 = true;
static sect_t cur_dir = 0;
static unsigned char cur_path[MAX_FILE_PATH];
static dir_item empty_dir_entry[sizeof(dir_item)];

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
    "dec"};

static void print_dir_record(dir_item* p_dir) {
  uint8_t attr = p_dir->attrib;

  // check file is not SWP
  // attr && !(attr & (DIR_ATTR_HIDDEN)) && strcmp(p_dir->ext, "SWP ") != 0
  uint8_t c = p_dir->filename[0];
  if (c != FAT_DIRENT_NEVER_USED && c != FAT_DIRENT_DELETED && !(p_dir->attrib & DIR_ATTR_HIDDEN)) {
    char* type = attr & DIR_ATTR_DIRECTORY ? "d" : "f";
    printf("%s %s", p_dir->filename, type);

    if (p_dir->attrib != DIR_ATTR_DIRECTORY) {
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

static void update_fat_sect(uint32_t index) {
  uint32_t sector = index / minfo.sector_entries_count;
  uint8_t* addr = &fat[sector * minfo.sector_entries_count];
  flpydsk_set_buffer(addr);
  flpydsk_write_sector(sector + minfo.fat_offset);
}

static void update_fat() {
  for (int i = 0; i < minfo.fat_size_sect; ++i) {
    update_fat_sect(i * minfo.sector_entries_count);
  }
}

static void ls_dir(uint32_t offset) {
  dir_item* p_dir = (dir_item*)flpydsk_read_sector(offset);

  printf("\n");
  for (int32_t i = 0; i < minfo.sector_entries_count; i++, p_dir++) {
    if (p_dir->filename[0] == FAT_DIRENT_NEVER_USED)
      break;
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

  uint32_t count = ext ? ext - name : len;
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
  return entry >= FAT_ATTR_EOF;
}

// TODO: bad code, fix of invalid root dir (to parse folder/..)
static bool is_bug_root_dir(sect_t* dir_sector) {
  return minfo.root_offset - minfo.fat_size_clusters * minfo.sect_per_cluster == *dir_sector;
}

static sect_t cluster_to_sector(uint32_t cluster_num) {
  sect_t sect = minfo.data_offset +
                (cluster_num - minfo.fat_size_clusters) * minfo.sect_per_cluster;

  if (is_bug_root_dir(&sect)) {
    sect = minfo.root_offset;
  }

  return sect;
}

static dir_item* find_and_create_dir_entry(const char* filename, sect_t dir_sector, dir_item* item) {
  for (int32_t i = 0; i < minfo.sect_per_cluster; ++i) {
    dir_item* p_dir = (dir_item*)flpydsk_read_sector(dir_sector + i);
    dir_item* iter = p_dir;
    for (int32_t j = 0; j < minfo.sector_entries_count; j++, iter++) {
      uint8_t c = iter->filename[0];
      if (c == FAT_DIRENT_NEVER_USED || c == FAT_DIRENT_DELETED) {  // empty space
        memcpy(iter, item, sizeof(dir_item));
        flpydsk_set_buffer(p_dir);
        flpydsk_write_sector(dir_sector + i);
        return iter;
      }
    }
  }

  return NULL;
}

static bool is_equal_name(const char* dos_name, const char* fname) {
  char entry_name[FAT_LEGACY_FILENAME_SIZE + 1];
  memcpy(entry_name, fname, FAT_LEGACY_FILENAME_SIZE);
  entry_name[FAT_LEGACY_FILENAME_SIZE] = 0;
  return strcmp(entry_name, dos_name) == 0;
}

static uint32_t pack_cluster(dir_item* p_dir) {
  uint32_t cluster = ((uint32_t)p_dir->first_cluster) | (((uint32_t)p_dir->first_cluster_hi_bytes) << 16);
  KASSERT(cluster <= minfo.fat_entries_count);
  return cluster;
}

static bool clear_dir_entry(char* name, const sect_t dir_sector) {
  char dos_name[FAT_LEGACY_FILENAME_SIZE + 1];
  to_dos_name(name, dos_name);

  for (int32_t i = 0; i < minfo.sect_per_cluster; ++i) {
    dir_item* p_dir = (dir_item*)flpydsk_read_sector(dir_sector + i);
    dir_item* iter = p_dir;

    for (int32_t j = 0; j < minfo.sector_entries_count; j++, iter++) {
      uint8_t c = iter->filename[0];
      
      if (c == FAT_DIRENT_NEVER_USED) {
        break;
      }

      if (iter->attrib & DIR_ATTR_HIDDEN || c == FAT_DIRENT_DELETED) {
        continue;
      }

      if (is_equal_name(dos_name, iter->filename)) {
        iter->filename[0] = FAT_DIRENT_DELETED;
        flpydsk_set_buffer(p_dir);
        flpydsk_write_sector(dir_sector + i);

        uint32_t cluster = pack_cluster(p_dir);

        // clear fat table
        do {
          uint32_t tmp = cluster; 
          cluster = fat[cluster];
          fat[tmp] = 0;
        } while (cluster < FAT_ATTR_EOF);

        update_fat();
        return true;
      }
    }
  }

  return false;
}

// in: dir sector, out: cluster
static bool find_subdir(char* name, const sect_t dir_sector, vfs_file* p_file) {
  char dos_name[FAT_LEGACY_FILENAME_SIZE + 1];
  to_dos_name(name, dos_name);

  for (int32_t i = 0; i < minfo.sect_per_cluster; ++i) {
    dir_item* p_dir = (dir_item*)flpydsk_read_sector(dir_sector + i);

    for (int32_t j = 0; j < minfo.sector_entries_count; j++, p_dir++) {
      uint8_t c = p_dir->filename[0];

      if (c == FAT_DIRENT_NEVER_USED) {
        break;
      }

      if (p_dir->attrib & DIR_ATTR_HIDDEN || c == FAT_DIRENT_DELETED) {
        continue;
      }

      if (is_equal_name(dos_name, p_dir->filename)) {
        strcpy(p_file->name, name);
        p_file->id = 0;
        p_file->first_cluster = pack_cluster(p_dir);
        p_file->file_length = p_dir->file_size;
        p_file->eof = false;
        p_file->f_pos = 0;
        p_file->flags = p_dir->attrib == DIR_ATTR_DIRECTORY ? FS_DIRECTORY : FS_FILE;

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

static bool jmp_dir(const char* path, sect_t* dir_sector) {
  vfs_file file;
  const char* cur = path;

  // check if root dir
  if (cur[0] == '\/') {
    *dir_sector = minfo.root_offset;
    ++cur;
  }

  uint32_t len = strlen(path);
  if (!(len == 1 && path[0] == '.')) {  // check if '.'

    while (*cur) {
      char pathname[FAT_LEGACY_FILENAME_SIZE + 1];
      int32_t i = 0;
      while (cur[i] != '\/' && cur[i] != '\0') {
        pathname[i] = cur[i];
        ++i;
      }

      pathname[i] = '\0';  // null terminate

      //! open subdir_item or file
      vfs_file file;
      file.first_cluster = 0;
      if (!find_subdir(pathname, *dir_sector, &file)) {
        // PANIC("Cannot find dir_item: %s", pathname);
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

static bool path_deconstruct(const char* path, sect_t* dir_sector, char** filename) {
  char* fname = last_strchr(path, '\/');
  bool no_dir_jmp = false;
  if (fname) {
    *fname = '\0';
    ++fname;  // ignore '/'
  } else {
    fname = path;
    no_dir_jmp = true;
  }

  *filename = fname;
  *dir_sector = cur_dir;

  bool ret = !no_dir_jmp && !jmp_dir(path, dir_sector);
  return !ret;
}

static int32_t find_free_cluster(int32_t* index) {
  for (uint32_t i = 0; i < minfo.fat_entries_count; ++i) {
    if (fat[i] == 0) {
      *index = i;
      return 0;
    }
  }

  return -ENOMEM;
}

static bool init_new_file(const char* filename, dir_item* item) {
  memset(item, 0, sizeof(dir_item));

  char dos_name[FAT_LEGACY_FILENAME_SIZE + 1];
  to_dos_name(filename, dos_name);
  memcpy(item->filename, &dos_name, FAT_LEGACY_FILENAME_SIZE - 3);
  memcpy(item->ext, &dos_name[8], 3);

  // for now hardcode it as I can't get current time
  struct time t = {
      .year = 2023,
      .month = 11,
      .day = 2,
      .hour = 10,
      .minute = 20,
      .second = 36};

  uint16_t date = 0;
  uint16_t time = 0;
  time_to_fat(&t, &date, &time);

  item->file_size = 1;
  item->attrib = DIR_ATTR_ARCHIVE;

  item->date_created = date;
  item->last_mod_date = date;
  item->date_last_accessed = date;
  item->time_created = time;
  item->last_mod_time = time;

    // find free cluster
  uint32_t cluster = 0;
  if (find_free_cluster(&cluster) < 0) {
    PANIC("Unable to find free cluster in fat32, NO SPACE?", NULL);
  }

  fat[cluster] = FAT_ATTR_EOF;
  item->first_cluster_hi_bytes = cluster >> 16;
  item->first_cluster = (cluster << 16) >> 16;

  update_fat_sect(cluster);
  return true;
}

static bool fat_create(
  const char* filename,
  sect_t dir_sector,
  vfs_file* file,
  int32_t flags
) {
  dir_item item;
  if (!init_new_file(filename, &item)) {
    PANIC("Unable to init file for dir item", NULL);
  }

  if (find_and_create_dir_entry(filename, dir_sector, &item) == NULL) {
    PANIC("Directory entry creation failed", NULL)
  }
  return true;
}

static vfs_file open_file(const char* name, sect_t dir_sector, uint32_t flags) {
  vfs_file file;
  
  if (!find_subdir(name, dir_sector, &file)) {
    if (flags & O_CREAT) {  // not found, let's create
      if (!fat_create(name, dir_sector, &file, flags)) {
        file.flags = FS_INVALID;
        return file;
      } else if (find_subdir(name, dir_sector, &file)) {
        return file;
      } else {
        PANIC("Can't find a file after creation", NULL);
      }
    } else {
      file.flags = FS_NOT_FOUND;
      return file;
    }
    // PANIC("Cannot find file: %s", name);
  }
  
  return file;
}

static int32_t delete_file(const char* name, sect_t dir_sector) {
  vfs_file file;
  if (!find_subdir(name, dir_sector, &file)) {
    file.flags = FS_NOT_FOUND;
    return -ENOENT;
  }

  if (!clear_dir_entry(name, dir_sector)) {
    PANIC("Unable to delete file which is found", NULL);
  }

  return 1;
}

static uint32_t get_start_cluster(vfs_file* file, loff_t ppos) {
  if (file->file_length <= ppos) {
    return FAT_ATTR_EOF;
  }

  uint32_t cur_cluster = file->first_cluster;
  uint32_t ppos_cluster = ppos / (minfo.sect_per_cluster * minfo.bytes_per_sect);

  while (ppos_cluster > 0) {
    if (cur_cluster >= FAT_ATTR_EOF) {
      return EOF;
    }
    cur_cluster = fat[cur_cluster];
    ppos_cluster--;
  }

  return cur_cluster;
}

// Public
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
    // PANIC("Not found dir_item %s", path);
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
  // memcpy(cur_path + strlen(cur_path), postfix, strlen(postfix));
  return true;
}

int32_t fat_delete(const char* path) {
  char* filename = 0;
  sect_t dir_sector = 0;

  if (!path_deconstruct(path, &dir_sector, &filename)) {
    return -ENOENT;
  }

  return delete_file(filename, dir_sector);
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
      // PANIC("Not found dir_item %s", path);
    }

    ls_dir(dir_sector);
  }
  return true;
}

int32_t fat_write(vfs_file* file, uint8_t* buffer, size_t length, loff_t ppos) {
  return 1;
}

int32_t fat_read(vfs_file* file, uint8_t* buffer, uint32_t length, loff_t ppos) {
  if (!file) {
    return -ENOENT;
  }

  if (length == 0) {
    return length;
  }

  uint8_t* cur = buffer;
  uint32_t cur_cluster = get_start_cluster(file, ppos);
  loff_t offset = ppos % (minfo.bytes_per_sect * minfo.sect_per_cluster);
  uint32_t left_len = length;

  do {
    if (cur_cluster >= FAT_ATTR_EOF) {
      file->eof = true;
      break;
    }

    sect_t sect_num = cluster_to_sector(cur_cluster);
    for (uint32_t i = 0; i < minfo.sect_per_cluster; ++i) {
      if (left_len > 0) {
        uint8_t* sector = flpydsk_read_sector(sect_num + i);
        uint32_t to_read = min(left_len, minfo.bytes_per_sect - offset);
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

vfs_file fat_open(const char* path, int32_t flags) {
  char* filename = 0;
  sect_t dir_sector = 0;
  if (!path_deconstruct(path, &dir_sector, &filename)) {
    vfs_file file;
    file.flags = FS_NOT_FOUND;
    return file;
  }
  return open_file(filename, dir_sector, flags);
}

void fat_mount() {
  p_bootsector bootsector = (p_bootsector)flpydsk_read_sector(0);

  if (!is_fat32(bootsector)) {
    PANIC("The disk is not fat32 formatted", "");
  }

  minfo.fat_size_clusters = bootsector->bpb.number_of_fats;
  minfo.fat_size_sect =
      (fat32 ? bootsector->bpb_ext.sectors_per_fat32 : (uint32_t)bootsector->bpb.sectors_per_fat) *
      (minfo.fat_size_clusters >> 1);  // without copies
  uint32_t bytes_per_sector = bootsector->bpb.bytes_per_sector;
  minfo.fat_offset = bootsector->bpb.reserved_sectors;
  minfo.fat_copy_offset = minfo.fat_size_sect + minfo.fat_offset;
  minfo.sect_per_cluster = bootsector->bpb.sect_per_cluster;
  minfo.data_offset = bootsector->bpb.reserved_sectors + bootsector->bpb.number_of_fats * minfo.fat_size_sect;
  minfo.bytes_per_sect = bootsector->bpb.bytes_per_sector;
  KASSERT(FLPYDSK_SECTOR_SIZE == minfo.bytes_per_sect);
  minfo.sector_entries_count = minfo.bytes_per_sect / sizeof(uint32_t);
  minfo.fat_entry_size = FAT_ENTRY_SIZE;
  uint32_t offset_in_data_cluster = (bootsector->bpb_ext.root_cluster - bootsector->bpb.number_of_fats) * bootsector->bpb.sect_per_cluster;
  minfo.root_offset = bootsector->bpb.reserved_sectors + minfo.fat_size_sect * bootsector->bpb.number_of_fats - offset_in_data_cluster;
  minfo.fat_entries_count = (minfo.fat_size_sect * minfo.bytes_per_sect) / minfo.fat_entry_size;
  fat = kcalloc(minfo.fat_entries_count, minfo.fat_entry_size);

  for (int i = 0; i < minfo.fat_size_sect; ++i) {
    uint32_t* fat_ptr = (uint32_t*)flpydsk_read_sector(minfo.fat_offset + i);
    memcpy((uint8_t*)fat + i * minfo.bytes_per_sect, fat_ptr, minfo.bytes_per_sect);
  }

  memset(&empty_dir_entry, 0, sizeof(dir_item));

  // read fat table
  uint32_t* fat_ptr = (uint32_t*)flpydsk_read_sector(minfo.fat_offset);
  if (fat[1] != 0xfffffff) {
    PANIC("Invalid fat table at sector: %d", minfo.fat_offset);
  }

  cur_dir = minfo.root_offset;
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
  strcpy(fsys_fat.name, "FAT32");
  fsys_fat.mount = fat_mount;
  fsys_fat.open = fat_open;
  fsys_fat.read = fat_read;
  fsys_fat.close = fat_close;
  fsys_fat.root = fat_get_rootdir;  // TODO: needs to be on the same level with VFS
  fsys_fat.ls = fat_ls;
  fsys_fat.cd = fat_cd;
  fsys_fat.delete = fat_delete;

  //! register ourself to volume manager
  vfs_register_file_system(&fsys_fat, 0);

  fat_mount();
}