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

  if ((status = vfs_fwrite(0, &buf, i)) < 0) {
    err("kprintf: unable to write to file");
  }
}

static void _kreadline(char *buf, uint32_t size, int fd) {
  char *iter = buf;
  int read = 0;
  int left = size;

  while (left > 0 && (read = vfs_fread(fd, iter, left)) == left) {
    if (read < 0) {
      //err("Error while reading command");
      assert_not_reached("kreadline: error while reading command");
    }

    iter = iter + read;
    //iter[0] = '\0';
    
    left -= read;
  }
}

void kreadline(char *buf, uint32_t size) {
  _kreadline(buf, size, 1);
}

void kreadterminal(char *buf, uint32_t size) {
  _kreadline(buf, size, 0);
}

char kreadchar() {
  char *buf[2];
  
  _kreadline(buf, 1, 1);

  return buf[0];
}