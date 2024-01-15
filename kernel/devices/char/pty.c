#include "kernel/devices/char/tty.h"

#define NR_PTY_MAX (1 << MINORBITS)

struct tty_driver *ptm_driver, *pts_driver;
volatile int next_pty_number = 0;

int get_next_pty_number() {
  return next_pty_number++;
}

static int pty_open(struct tty_struct *tty, struct vfs_file *filp) {
  return 0;
}

static int pty_write(struct tty_struct *tty, const char *buf, int count) {
  struct tty_struct *to = tty->link;

  if (!to)
    return 0;

  int c = to->ldisc->receive_room(to);

  if (c > count)
    c = count;

  to->ldisc->receive_buf(to, buf, c);
  return c;
}

static int pty_write_room(struct tty_struct *tty) {
  struct tty_struct *to = tty->link;

  if (!to)
    return 0;

  return to->ldisc->receive_room(to);
}

static struct tty_operations pty_ops = {
  .open = pty_open,
  .write = pty_write,
  .write_room = pty_write_room,
};

void pty_init() {
  
}