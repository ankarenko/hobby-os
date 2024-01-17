#include "kernel/util/debug.h"
#include "kernel/util/math.h"
#include "kernel/util/string/string.h"
#include "kernel/locking/semaphore.h"
#include "kernel/devices/char/tty.h"

int ntty_open(struct tty_struct *tty) {
  return 0;
}

ssize_t ntty_read(struct tty_struct *tty, struct vfs_file *file, char *buf, size_t nr) {
  assert(nr < N_TTY_BUF_SIZE);
  
  semaphore_down_val(tty->to_read, nr);
  semaphore_down(tty->mutex);

  bool read_all = false;
  int i = 0;
  for (; i < nr; ++i) {
    buf[i] = tty->buffer[tty->read_head];
    tty->read_head = N_TTY_BUF_ALIGN(tty->read_head + 1);
    
    if (tty->read_head == tty->read_tail) {
      read_all = true;
      break;
    }
  }

  semaphore_up(tty->mutex);
  return i;
}

void ntty_receive_buf(struct tty_struct *tty, const char *cp, int count) {
  
}

ssize_t ntty_write(struct tty_struct *tty, struct vfs_file *file, const char *buf, size_t nr) {
  assert(nr < N_TTY_BUF_SIZE);

  semaphore_down(tty->mutex);

  for (int i = 0; i < nr; ++i) {
    tty->buffer[tty->read_tail] = buf[i]; 
    tty->read_tail = N_TTY_BUF_ALIGN(tty->read_tail + 1);
    if (tty->read_head == tty->read_tail) {
      tty->read_head = N_TTY_BUF_ALIGN(tty->read_head + 1);
    }
  }

  semaphore_up(tty->mutex);
  semaphore_up_val(tty->to_read, nr);

  if (tty->driver && tty->driver->tops->write) {
    tty->driver->tops->write(tty, buf, nr);
  }

  return nr;
}

struct tty_ldisc tty_ldisc_N_TTY = {
	.magic = TTY_LDISC_MAGIC,
	.open = ntty_open,
  .read = ntty_read,
	.write = ntty_write,
  .receive_buf = ntty_receive_buf
};
