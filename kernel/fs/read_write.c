#include "kernel/util/errno.h"
#include "kernel/util/fcntl.h"
#include "kernel/proc/task.h"
#include "kernel/util/debug.h"

#include "./vfs.h"

extern vfs_filesystem* file_systems[DEVICE_MAX];

static int32_t vfs_read_chunk(vfs_file* file, uint8_t* buffer, uint32_t length) {
  if (file) {
    if (file_systems[file->device_id - 'a']) {
      return file_systems[file->device_id - 'a']->read(file, buffer, length, file->f_pos);
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
    return vfs_read_chunk(file, buf, count);
  }

	return -EINVAL;
}

off_t vfs_flseek(int32_t fd, off_t offset, int whence) {
  process* proc = get_current_process();
  vfs_file* file = proc->files->fd[fd];

  if (!file) {
    return -ENOENT;
  }
  
  switch (whence)
  {
  case SEEK_SET:
    file->f_pos = offset;
    break;

  case SEEK_CUR:
    file->f_pos += offset;
    break;

  case SEEK_END:
    file->f_pos = file->file_length + offset;
    break;
  
  default:
    return -EINVAL;
  }

  return file->f_pos;
}

ssize_t vfs_fwrite(int32_t fd, char* buf, int32_t count) {
	if (fd < 0)
		return -EBADF;

  process* proc = get_current_process();
  vfs_file* file = proc->files->fd[fd];
  
  if (file) {
    if (file_systems[file->device_id - 'a']) {
      return file_systems[file->device_id - 'a']->write(file, buf, count, file->f_pos);
    }
  }
  return -ENOENT;
}

char* vfs_read(const char *path) {
  int32_t fd = vfs_open(path, O_RDWR);

	if (fd < 0)
		return NULL;

	//struct kstat *stat = kcalloc(1, sizeof(struct kstat));
	//vfs_fstat(fd, stat);
  process* proc = get_current_process();
  vfs_file* file = proc->files->fd[fd];
  int32_t size = file->file_length;
  file->f_pos = 0; // read from the beggining
	char *buf = kcalloc(size + 1, sizeof(char));
	vfs_fread(fd, buf, size);
  buf[size] = '\0';
	return buf;
}

