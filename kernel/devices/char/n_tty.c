#include "kernel/devices/char/tty.h"
#include "kernel/locking/semaphore.h"
#include "kernel/memory/malloc.h"
#include "kernel/util/debug.h"
#include "kernel/ipc/signal.h"
#include "kernel/util/math.h"
#include "kernel/util/string/string.h"

static int ntty_open(struct tty_struct *tty) {
  tty->buffer = kcalloc(1, N_TTY_BUF_SIZE);

  tty->mutex = semaphore_alloc(1);
  tty->to_read = semaphore_alloc(N_TTY_BUF_SIZE);
  tty->to_write = semaphore_alloc(N_TTY_BUF_SIZE);
  tty->to_read->count = 0;

  return 0;
}

static void ntty_close(struct tty_struct *tty) {
  semaphore_free(tty->mutex);
  semaphore_free(tty->to_read);
  semaphore_free(tty->to_write);

  tty->mutex = NULL;
  tty->to_read = NULL;
  tty->to_write = NULL;

  kfree(tty->buffer);
  tty->buffer = NULL;
}

static uint32_t ntty_available_to_read(struct tty_struct *tty) {
  if (tty->read_tail >= tty->read_head)
    return tty->read_tail - tty->read_head;

  return N_TTY_BUF_SIZE + tty->read_tail - tty->read_head;
}

static uint32_t ntty_receive_room(struct tty_struct *tty) {
  return N_TTY_BUF_SIZE - ntty_available_to_read(tty);
}

static void ntty_pop_char(struct tty_struct *tty, char *ch) {
  bool is_canon = L_ICANON(tty);

  semaphore_down(tty->to_read);
  semaphore_down(tty->mutex);

  *ch = tty->buffer[tty->read_head];
  tty->read_head = N_TTY_BUF_CANON_ALIGN(tty->read_head + 1);
  int to_write = !L_ICANON(tty)? 1 : 0;
  if (L_ICANON(tty) && tty->read_head == tty->read_tail) {
    to_write = N_TTY_BUF_SIZE;
  }
  semaphore_up(tty->mutex);
  if (to_write > 0) {
    tty->to_write->count += to_write - 1;
    semaphore_up(tty->to_write);
  }
}

static uint32_t ntty_read(struct tty_struct *tty, struct vfs_file *file, char *buf, size_t nr) {

  int i = 0;

  do {
    ntty_pop_char(tty, &buf[i]);
    if (L_ICANON(tty) && LINE_SEPARATOR(tty, buf[i])) {
      --i;  // omit LINE_SEPARATOR;
      break;
    }

    if (tty->read_head == tty->read_tail)
      break;
      
    ++i;
  } while (i < nr);

  return i + 1;
}

static uint32_t push_buf_raw(struct tty_struct *tty, char c) {
  semaphore_down(tty->to_write);
  semaphore_down(tty->mutex);

  tty->buffer[tty->read_tail] = c;
  tty->read_tail = N_TTY_BUF_ALIGN(tty->read_tail + 1);

  semaphore_up(tty->mutex);
  semaphore_up(tty->to_read);
}

static uint32_t push_buf_canon(struct tty_struct *tty, char c) {
  semaphore_down(tty->to_write);
  semaphore_down(tty->mutex);

  tty->buffer[tty->read_tail] = c;
  tty->read_tail = N_TTY_BUF_CANON_ALIGN(tty->read_tail + 1);

  if (tty->read_head == tty->read_tail) {
    tty->read_head = N_TTY_BUF_CANON_ALIGN(tty->read_head + 1);
  }

  int to_read = LINE_SEPARATOR(tty, c)? ntty_available_to_read(tty) : 0;
  if (to_read > 0) 
    tty->to_write->count = 0;

  semaphore_up(tty->mutex);
  if (to_read > 0) {
    tty->to_read->count += to_read - 1;
    semaphore_up(tty->to_read);
  }
}

static void eraser(struct tty_struct *tty, char ch) {
  semaphore_down(tty->mutex);
  if (tty->read_tail != tty->read_head) {
    tty->read_tail = N_TTY_BUF_ALIGN(tty->read_tail - 1);
  }
  semaphore_up(tty->mutex);
}

static void ntty_receive_buf(struct tty_struct *tty, const char *buf, int nr) {
  if (L_ICANON(tty)) {
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
        
        int32_t sig = -1;
        if (INTR_CHAR(tty) == ch)
          sig = SIGINT;
        else if (QUIT_CHAR(tty) == ch)
          sig = SIGQUIT;
        else if (SUSP_CHAR(tty) == ch)
          sig = SIGTSTP;

        if (valid_signal(sig) && sig > 0) {
          if (tty->pgrp > 0) 
            do_kill(-tty->pgrp, sig);
          continue;
        }
      }

      if (ch == ERASE_CHAR(tty) || ch == KILL_CHAR(tty) || 
        (ch == WERASE_CHAR(tty) && L_IEXTEN(tty))
      ) {
        eraser(tty, ch);
        continue;
      }

      if (EOF_CHAR(tty) == ch) {
        ch = __DISABLED_CHAR;
      }

      push_buf_canon(tty, ch);
    }
  } else {
    for (int i = 0; i < nr; ++i)
		  push_buf_raw(tty, buf[i]);
  }
}

static uint32_t ntty_write(struct tty_struct *tty, struct vfs_file *file, const char *buf, size_t nr) {
	if (O_OPOST(tty)) {
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
  } else {
    tty->driver->tops->write(tty, buf, nr);
		return nr;
	}
}

static unsigned int ntty_poll(struct tty_struct *tty, struct vfs_file *file, struct poll_table *ptable) {
	assert_not_implemented();
}

struct tty_ldisc tty_ldisc_N_TTY = {
  .magic = TTY_LDISC_MAGIC,
  .open = ntty_open,
  .read = ntty_read,
  .write = ntty_write,
  .receive_buf = ntty_receive_buf,
  .receive_room = ntty_receive_room,
  .close = ntty_close,
  .poll = ntty_poll
};
