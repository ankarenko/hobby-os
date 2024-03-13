#ifndef UTIL_STDIO_H
#define UTIL_STDIO_H

#include <stdint.h>

#include "kernel/include/list.h"

#define	EOF	(-1)
#define stdin 0
#define stdout 1
#define stderr 2

typedef struct __FILE {
  int fd;
  int size;
  int _offset;
  struct list_head sibling;
  char *_IO_write_ptr, *_IO_write_base, *_IO_write_end;
} FILE;

void kprintf(char *fmt, ...);
void kprintformat(char *fmt, int size, char *color, ...);
void kreadline(char *buf, uint32_t size);
char kreadchar();

#endif