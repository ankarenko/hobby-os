#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/*
 * min()/max() macros that also do
 * strict type-checking.. See the
 * "unnecessary" pointer comparison.
 */
#define min(x, y) ({ \
	typeof(x) _x = (x);	\
	typeof(y) _y = (y);	\
	(void) (&_x == &_y);		\
	_x < _y ? _x : _y; })

#define max(x, y) ({ \
	typeof(x) _x = (x);	\
	typeof(y) _y = (y);	\
	(void) (&_x == &_y);		\
	_x > _y ? _x : _y; })

DIR *opendir(const char *name) {
  int fd = open(name, O_RDONLY);
  DIR *dirp = fdopendir(fd);
  dirp->owned_fd = true;
  return dirp;
}

DIR *fdopendir(int fd) {
  DIR *dirp = calloc(1, sizeof(DIR));
  dirp->fd = fd;
  dirp->owned_fd = false;
  dirp->size = 0;
  dirp->pos = 0;

  struct stat stat;
  
  if (fstat(fd, &stat) != 0)
    goto free_dirp;

  if (!S_ISDIR(stat.st_mode)) {
    errno = -ENOTDIR;
    goto free_dirp;
  }

  dirp->len = stat.st_blksize;
  dirp->buf = calloc(dirp->len, sizeof(char));

  return dirp;

free_dirp:
  free(dirp);
  return NULL;
}

struct dirent *readdir(DIR *dirp) {
  if (dirp->fd < 0)
    return errno = EBADF, NULL;

  if (!dirp->size || dirp->pos == dirp->size) {
    memset(dirp->buf, 0, dirp->len);
    dirp->size = getdents(dirp->fd, (struct dirent *)dirp->buf, dirp->len);
    dirp->pos = 0;
  }
  if (!dirp->size)
    return NULL;

  struct dirent *entry = (struct dirent *)((char *)dirp->buf + dirp->pos);
  dirp->pos += entry->d_reclen;
  return entry;
}

int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result) {
  if (dirp->fd < 0)
    return errno = EBADF, -1;

  struct dirent *pdir = readdir(dirp);
  memcpy(entry, pdir, min(entry->d_reclen, pdir->d_reclen));

  *result = pdir ? entry : NULL;
  return 0;
}

int closedir(DIR *dirp) {
  if (dirp->fd < 0)
    return errno = EBADF, -1;

  if (dirp->owned_fd)
    close(dirp->fd);

  dirp->fd = -1;
  free(dirp->buf);
  return 0;
}

void rewinddir(DIR *dirp) {
  lseek(dirp->fd, 0, SEEK_SET);
  dirp->pos = 0;
  dirp->size = 0;
}

long telldir(DIR *dirp) {
  if (dirp->fd < 0)
    return errno = EBADF, -1;

  return dirp->pos;
}

void seekdir(DIR *dirp, long loc) {
  dirp->pos = loc;
}

int dirfd(DIR *dirp) {
  if (dirp->fd < 0)
    return errno = EINVAL, -1;

  return dirp->fd;
}
