#ifndef DIRENT_H
#define DIRENT_H

#include <stdint.h>
#include <sys/types.h>
#include <stdbool.h>

#define MAX_FILENAME_LENGTH 256

struct dirent {
  ino_t d_ino;             /* Inode number */
  off_t d_off;             /* Not an offset; see below */
  unsigned short d_reclen; /* Length of this record */
  unsigned short d_type;   /* Type of file; not supported by all filesystem types */
  char d_name[MAX_FILENAME_LENGTH];         /* Null-terminated filename */
};

struct __DIR {
  int fd;
  bool owned_fd;
  int len;   /* buffer length */
  int size;  /* amount of data in buffer */
  char *buf; /* buffer */
  off_t pos; /* position in buffer */
};

typedef struct __DIR DIR;

int closedir(DIR *dirp);
int dirfd(DIR *dirp);
DIR *opendir(const char *name);
DIR *fdopendir(int fd);
struct dirent *readdir(DIR *dirp);
int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result);
void rewinddir(DIR *dirp);
void seekdir(DIR *dirp, long int loc);
long int telldir(DIR *dirp);

#endif