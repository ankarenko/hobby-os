#ifndef _LIBC_FILE_H
#define _LIBC_FILE_H

#include <stdint.h>

#include "kernel/include/list.h"

struct __FILE {
  int fd;
  int size;
  int _offset;
  struct list_head sibling;
  char *_IO_write_ptr, *_IO_write_base, *_IO_write_end;
};

#endif
