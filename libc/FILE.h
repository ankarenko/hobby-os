#ifndef _LIBC_FILE_H
#define _LIBC_FILE_H

#include <stdint.h>
#include <list.h>

struct __FILE {
  int fd;
  int size;
  struct list_head sibling;
};

#endif
