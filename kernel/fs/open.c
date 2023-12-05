#include <stddef.h>

#include "kernel/util/errno.h"
#include "kernel/proc/task.h"
#include "kernel/util/debug.h"

#include "kernel/fs/vfs.h"

extern vfs_filesystem* file_systems[DEVICE_MAX];

static int32_t find_unused_fd_slot() {
  process* proc = get_current_process();

	for (uint32_t i = 0; i < MAX_FD; ++i)
		if (!proc->files->fd[i])
			return i;

	return -1;
}

static vfs_file *get_empty_file() {
	vfs_file *file = kcalloc(1, sizeof(vfs_file));
  
	return file;
}

int32_t vfs_close(int32_t fd) {
  process* proc = get_current_process();
  vfs_file* file = proc->files->fd[fd];

  if (file)
    if (file_systems[file->device_id - 'a'])
      return file_systems[file->device_id - 'a']->fop.close(file);
}

int vfs_fstat(int32_t fd, struct stat* stat) {
  process* proc = get_current_process();
  vfs_file* file = proc->files->fd[fd];
  stat->st_size = file->file_length;
  stat->st_mode = S_IFCHR;
  stat->st_blksize = 512;
  //stat->st_blocks = 1;
  return 1;
}

int32_t vfs_delete(const char* fname) {
  if (!fname)
    return -ENOENT;

  unsigned char device = 'a';
  if (file_systems[device - 'a']) {
    return file_systems[device - 'a']->delete(fname);
  }
}

int32_t vfs_mkdir(const char* dir_path) {
  if (!dir_path)
    return -ENOENT;

  unsigned char device = 'a';
  if (file_systems[device - 'a']) {
    return file_systems[device - 'a']->mkdir(dir_path);
  }
}

int32_t vfs_open(const char* fname, int32_t flags, ...) {
  if (fname) {
    //! default to device 'a'
    unsigned char device = 'a';

    //! filename
    char* filename = (char*)fname;

    //! in all cases, if fname[1]==':' then the first character must be device letter
    //! FIXME: Using fname[2] do to BUG 2. Please see main.cpp for info
    if (fname[2] == ':') {
      device = fname[0];
      filename += 3;  // strip it from pathname
    }

    int32_t fd = find_unused_fd_slot();
    if (fd < 0) {
      PANIC("Can't find file descriptor slot", NULL);
    }

    //! call vfs_filesystem
    if (file_systems[device - 'a']) {
      vfs_file ret_file = file_systems[device - 'a']->fop.open(filename, flags);
      
      if (ret_file.flags == FS_INVALID || ret_file.flags == FS_NOT_FOUND) {
        return -ENOENT;
      }

      vfs_file* file = get_empty_file();
      memcpy(file, &ret_file, sizeof(vfs_file));
      file->device_id = device;
      
      process* proc = get_current_process();
      proc->files->fd[fd] = file;

      return fd;
    }
  }

  return -1;
}