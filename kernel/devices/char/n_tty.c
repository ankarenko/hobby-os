#include "kernel/util/debug.h"
#include "kernel/util/math.h"
#include "kernel/util/string/string.h"
#include "kernel/locking/semaphore.h"
#include "kernel/devices/char/tty.h"

int ntty_open(struct tty_struct *tty) {
  return 0;
}

ssize_t ntty_read(struct tty_struct *tty, struct vfs_file *file, char *buf, size_t nr) {
  semaphore_down_val(tty->reserved, nr);
  semaphore_down(tty->mutex);

  int len = min(tty->free->count, nr);
  for (int i = 0; i < len; ++i) {
    buf[i] = tty->buf[(tty->buf_head + i) % N_TTY_BUF_SIZE]; 
  }
  tty->buf_tail = tty->buf_tail - nr;
  
  semaphore_up(tty->mutex);
  semaphore_up_val(tty->free, nr);
  return len;
}

int ntty_receive_room(struct tty_struct *tty) {
  return tty->free->count;
}

ssize_t ntty_write(struct tty_struct *tty, struct vfs_file *file, const char *buf, size_t nr) {
  semaphore_down_val(tty->free, nr);
  semaphore_down(tty->mutex);

  int room = ntty_receive_room(tty);

  if (nr > room) {
    nr = room;
  }

  for (int i = 0; i < nr; ++i) {
    tty->buf[(tty->buf_tail + i) % N_TTY_BUF_SIZE] = buf[i]; 
  }

  tty->buf_tail = (tty->buf_tail + nr) % N_TTY_BUF_SIZE;

  semaphore_up(tty->mutex);
  semaphore_up_val(tty->reserved, nr);

  return nr;
}

struct tty_ldisc tty_ldisc_N_TTY = {
	.magic = TTY_LDISC_MAGIC,
	.open = ntty_open,
  .read = ntty_read,
	.write = ntty_write,
  .receive_room = ntty_receive_room
};
