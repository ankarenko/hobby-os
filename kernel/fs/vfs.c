#include "vfs.h"

#include <string.h>
#include <errno.h>

#include "kernel/proc/task.h"
#include "kernel/memory/malloc.h"
#include "kernel/util/debug.h"

#define DEVICE_MAX 26

//! File system list
vfs_filesystem* _file_systems[DEVICE_MAX];

vfs_file *get_empty_file() {
	vfs_file *file = kcalloc(1, sizeof(vfs_file));
	
  //atomic_set(&file->f_count, 1);

	return file;
}

int32_t find_unused_fd_slot() {
  process* proc = get_current_process();

	for (uint32_t i = 0; i < MAX_FD; ++i)
		if (!proc->files->fd[i])
			return i;

	return -1;
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
    if (_file_systems[device - 'a']) {
      vfs_file ret_file = _file_systems[device - 'a']->open(filename);
      
      if (ret_file.flags == FS_INVALID || ret_file.flags == FS_NOT_FOUND) {
        return -1;
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

/**
 *	Reads file
 */
int32_t vfs_read_file(vfs_file* file, uint8_t* buffer, uint32_t length) {
  if (file) {
    if (_file_systems[file->device_id - 'a']) {
      return _file_systems[file->device_id - 'a']->read(file, buffer, length);
    }
  }
  return -1;
}

int32_t vfs_fread(int32_t fd, char *buf, int32_t count) {
  process* proc = get_current_process();
  vfs_file* file = proc->files->fd[fd];
  	
  if (fd < 0 || !file)
		return -EBADF;

	if (file /*&& file->f_mode & FMODE_CAN_READ*/) {
    return vfs_read_file(file, buf, count);
  }

	return -EINVAL;
}

int vfs_fstat(int32_t fd, struct stat* stat) {
  process* proc = get_current_process();
  vfs_file* file = proc->files->fd[fd];
  stat->st_size = file->file_length;
  return 1;
}

char* vfs_read(const char *path) {
  #define O_RDWR 1
  int32_t fd = vfs_open(path, O_RDWR);

	if (fd < 0)
		return NULL;

	//struct kstat *stat = kcalloc(1, sizeof(struct kstat));
	//vfs_fstat(fd, stat);
  process* proc = get_current_process();
  int32_t size = proc->files->fd[fd]->file_length;
	char *buf = kcalloc(size, sizeof(char));
	vfs_fread(fd, buf, size);
	return buf;
}

/**
 *	Close file
 */
void vfs_close_file(vfs_file* file) {
  if (file)
    if (_file_systems[file->device_id - 'a'])
      _file_systems[file->device_id - 'a']->close(file);
}

bool vfs_ls(const char* path) {
  unsigned char device = 'a';

  if (_file_systems[device - 'a'])
    return _file_systems[device - 'a']->ls(path);
}

bool vfs_cd(const char* path) {
  unsigned char device = 'a';

  if (_file_systems[device - 'a'])
    return _file_systems[device - 'a']->cd(path);
}

vfs_file vfs_get_root(uint32_t device_id) {
  if (_file_systems[device_id - 'a']) {
    vfs_file file = _file_systems[device_id - 'a']->root();
    file.device_id = device_id;
    return file;
  }
}

/**
 *	Registers a vfs_filesystem
 */
void vfs_register_file_system(vfs_filesystem* fsys, uint32_t device_id) {
  static int32_t i = 0;

  if (i < DEVICE_MAX)
    if (fsys) {
      _file_systems[device_id] = fsys;
      i++;
    }
}

/**
 *	Unregister file system
 */
void vfs_unregister_file_system(vfs_filesystem* fsys) {
  for (int i = 0; i < DEVICE_MAX; i++)
    if (_file_systems[i] == fsys)
      _file_systems[i] = 0;
}

/**
 *	Unregister file system
 */
void vfs_unregister_file_system_by_id(uint32_t device_id) {
  if (device_id < DEVICE_MAX)
    _file_systems[device_id] = 0;
}
