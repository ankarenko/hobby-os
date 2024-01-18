#include "kernel/devices/char/tty.h"
#include "kernel/locking/semaphore.h"
#include "kernel/memory/malloc.h"
#include "kernel/util/debug.h"
#include "kernel/util/math.h"
#include "kernel/util/string/string.h"

static int ntty_open(struct tty_struct *tty) {
  tty->buffer = kcalloc(1, N_TTY_BUF_SIZE);

  sema_init(tty->mutex, 1);
  sema_init(tty->to_read, 1);
  sema_init(tty->to_write, 1);

  semaphore_up(tty->to_write);
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

  bool read_all = false;
  for (int i = 0; i < nr; ++i) {
    buf[i] = tty->buffer[tty->read_head];
    tty->read_head = N_TTY_BUF_ALIGN(tty->read_head + 1);

    if (tty->read_head == tty->read_tail) {
      read_all = true;
    }
  }

  semaphore_up(tty->mutex);
  semaphore_up_val(tty->to_write, nr);
  return nr;
}

static ssize_t echo_buf(struct tty_struct *tty, const char *buf, ssize_t nr) {
  char *wbuf = kcalloc(1, N_TTY_BUF_SIZE);
  char *ibuf = wbuf;
  int wlength = 0;

  for (int i = 0; i < nr; ++i) {
    char ch = buf[i];
    if (O_ONLCR(tty) && ch == '\n') {
      *ibuf++ = '\r';
      *ibuf++ = ch;
      wlength += 2;
    } else if (O_OCRNL(tty) && ch == '\r') {
      *ibuf++ = '\n';
      wlength++;
    } else {
      *ibuf++ = O_OLCUC(tty) ? toupper(ch) : ch;
      wlength++;
    }
  }

  tty->driver->tops->write(tty, wbuf, wlength);
  kfree(wbuf);
  return wlength;
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
  semaphore_up(LINE_SEPARATOR(tty, c) ? tty->read_lock : tty->to_write);
}

static void eraser(struct tty_struct *tty, char ch) {
}

static void ntty_receive_buf(struct tty_struct *tty, const char *buf, int nr) {
  for (int i = 0; i < nr; ++i) {
    char ch = buf[i];

    if (I_ISTRIP(tty))
      ch &= 0x7f;
    if (I_IUCLC(tty) && L_IEXTEN(tty))
      ch = tolower(ch);
    if (ch == '\r') {
      if (I_IGNCR(tty))
        continue;
      if (I_ICRNL(tty))
        ch = '\n';
    } else if (ch == '\n' && I_INLCR(tty))
      ch = '\r';

    if (L_ISIG(tty)) {
      /*
      int32_t sig = -1;
      if (INTR_CHAR(tty) == ch)
        sig = SIGINT;
      else if (QUIT_CHAR(tty) == ch)
        sig = SIGQUIT;
      else if (SUSP_CHAR(tty) == ch)
        sig = SIGTSTP;

      if (valid_signal(sig) && sig > 0)
      {
        if (tty->pgrp > 0)
          do_kill(-tty->pgrp, sig);
        continue;
      }
      */
    }

    if (EOF_CHAR(tty) == ch) {
      ch = __DISABLED_CHAR;
    }

    push_buf(tty, &(const char){ch});

    if (L_ECHO(tty))
      echo_buf(tty, &(const char){ch}, 1);
  }
}

static ssize_t ntty_write(struct tty_struct *tty, struct vfs_file *file, const char *buf, size_t nr) {
  ntty_receive_buf(tty, buf, nr);
  return nr;
}

struct tty_ldisc tty_ldisc_N_TTY_canon = {
  .magic = TTY_LDISC_MAGIC,
  .open = ntty_open,
  .read = ntty_read,
  .write = ntty_write,
  .receive_buf = ntty_receive_buf,
  .close = ntty_close
};
