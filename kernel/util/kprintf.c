#include <stdarg.h>

#include "kernel/util/vsprintf.h"
#include "kernel/util/stdio.h"
#include "kernel/fs/vfs.h"
#include "kernel/util/string/string.h"
#include "kernel/util/math.h"
#include "kernel/util/debug.h"
#include "kernel/devices/char/tty.h"

#define BUF_SIZE (max(N_TTY_BUF_SIZE, 100))

void kprintf(char *fmt, ...) {
  
  char buf[BUF_SIZE];
  va_list args;
  va_start(args, fmt);
	int i = vsnprintf(&buf, BUF_SIZE, fmt, args);
	va_end(args);
  buf[i] = '\0';
  int status = 0;

  if ((status = vfs_fwrite(stdout, &buf, i)) < 0) {
    err("kprintf: unable to write to file");
  }
}

void kprintformat(char *fmt, int size, char *color, ...) {
  
  char buf[BUF_SIZE];
  va_list args;
  va_start(args, fmt);
	int i = vsnprintf(&buf, BUF_SIZE, fmt, args);
	va_end(args);
  int status = 0;

  memset(&buf[i], ' ', size - i);
  buf[size] = '\0';

  if (color) {
    vfs_fwrite(stdout, color, strlen(color));
  }
  if ((status = vfs_fwrite(stdout, &buf, size)) < 0) {
    err("kprintf: unable to write to file");
  }
  if (color) {
    vfs_fwrite(stdout, COLOR_RESET, strlen(COLOR_RESET));
  }
}

static void _kreadline(int fd, char *buf, uint32_t size) {
  int read = vfs_fread(fd, buf, size);
  if (read <= 0) {
    assert_not_reached("kreadline: error while reading command");
  }

  if (buf[read - 1] == '\n') {
    buf[read - 1] = '\0';  
  }
  buf[read] = '\0';
}

void kreadline(char *buf, uint32_t size) {
  _kreadline(stdin, buf, size);
}

char kreadchar() {
  char *buf[2];
  
  _kreadline(buf, 1, stdin);

  return buf[0];
}