#include "vfs.h"

#include "kernel/util/string/string.h"

#include "kernel/util/errno.h"
#include "kernel/util/fcntl.h"
#include "kernel/proc/task.h"
#include "kernel/memory/malloc.h"
#include "kernel/util/debug.h"

vfs_filesystem* file_systems[DEVICE_MAX];

bool vfs_ls(const char* path) {
  unsigned char device = 'a';

  if (file_systems[device - 'a'])
    return file_systems[device - 'a']->ls(path);
}

bool vfs_cd(const char* path) {
  unsigned char device = 'a';

  if (file_systems[device - 'a'])
    return file_systems[device - 'a']->cd(path);
}

vfs_file vfs_get_root(uint32_t device_id) {
  if (file_systems[device_id - 'a']) {
    vfs_file file = file_systems[device_id - 'a']->root();
    file.device_id = device_id;
    return file;
  }
}

void vfs_register_file_system(vfs_filesystem* fsys, uint32_t device_id) {
  static int32_t i = 0;

  if (i < DEVICE_MAX)
    if (fsys) {
      file_systems[device_id] = fsys;
      i++;
    }
}

void vfs_unregister_file_system(vfs_filesystem* fsys) {
  for (int i = 0; i < DEVICE_MAX; i++)
    if (file_systems[i] == fsys)
      file_systems[i] = 0;
}

void vfs_unregister_file_system_by_id(uint32_t device_id) {
  if (device_id < DEVICE_MAX)
    file_systems[device_id] = 0;
}
