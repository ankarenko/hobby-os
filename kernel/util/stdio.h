#ifndef UTIL_STDIO_H
#define UTIL_STDIO_H

#include "kernel/util/list.h"

#define	EOF	(-1)
#define stdin 0
#define stdout 0
#define stderr 0

typedef struct __FILE {
  int fd;
  int size;
  int _offset;
  struct list_head sibling;
  char *_IO_write_ptr, *_IO_write_base, *_IO_write_end;
} FILE;


#endif