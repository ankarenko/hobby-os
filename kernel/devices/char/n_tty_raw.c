#include "kernel/util/debug.h"
#include "kernel/util/math.h"
#include "kernel/util/string/string.h"
#include "kernel/locking/semaphore.h"
#include "kernel/memory/malloc.h"
#include "kernel/devices/char/tty.h"

static int ntty_open(struct tty_struct *tty) {
  tty->buffer = kcalloc(1, N_TTY_BUF_SIZE);

  sema_init(tty->mutex, 1);
  sema_init(tty->to_read, N_TTY_BUF_SIZE);
  sema_init(tty->to_write, N_TTY_BUF_SIZE);

  semaphore_up_val(tty->to_write, N_TTY_BUF_SIZE);

  return 0;
}

static void ntty_close(struct tty_struct *tty) {
  // TODO: not sure about semaphore_free
  semaphore_free(tty->mutex);
  semaphore_free(tty->to_read);
  semaphore_free(tty->to_write);

  tty->mutex = NULL;
  tty->to_read = NULL;
  tty->to_write = NULL;

  kfree(tty->buffer);
  tty->buffer = NULL;
}

static ssize_t ntty_read(struct tty_struct *tty, struct vfs_file *file, char *buf, size_t nr) {
  semaphore_down_val(tty->to_read, nr);
  semaphore_down(tty->mutex);

  for (int i = 0; i < nr; ++i) {
    buf[i] = tty->buffer[tty->read_head];
    tty->read_head = N_TTY_BUF_ALIGN(tty->read_head + 1);
  }

  semaphore_up(tty->mutex);
  semaphore_up_val(tty->to_write, nr);
  return nr;
}

static ssize_t echo_buf(struct tty_struct *tty, const char *buf, ssize_t nr) {
  tty->driver->tops->write(tty, buf, nr);
  return nr;
}

static ssize_t push_buf(struct tty_struct *tty, char c) {
  
  semaphore_down(tty->to_write);
  semaphore_down(tty->mutex);
  
  tty->buffer[tty->read_tail] = c;
  tty->read_tail = N_TTY_BUF_ALIGN(tty->read_tail + 1);
  if (tty->read_head == tty->read_tail) {
    tty->read_head = N_TTY_BUF_ALIGN(tty->read_head + 1);
  }
  
  semaphore_up(tty->mutex);
  semaphore_up(tty->to_read);
}

static void ntty_receive_buf(struct tty_struct *tty, const char *buf, int nr) {
  for (int i = 0; i < nr; ++i) {
    push_buf(tty, buf[i]);
  }

  if (L_ECHO(tty)) 
    echo_buf(tty, buf, nr);
}

static ssize_t ntty_write(struct tty_struct *tty, struct vfs_file *file, const char *buf, size_t nr) {
  ntty_receive_buf(tty, buf, nr);
  return nr;
}

struct tty_ldisc tty_ldisc_N_TTY_raw = {
	.magic = TTY_LDISC_MAGIC,
	.open = ntty_open,
  .read = ntty_read,
	.write = ntty_write,
  .receive_buf = ntty_receive_buf,
  .close = ntty_close
};
