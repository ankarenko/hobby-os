#include "kernel/devices/char/tty.h"
#include "kernel/locking/semaphore.h"
#include "kernel/memory/malloc.h"
#include "kernel/proc/task.h"
#include "kernel/fs/poll.h"
#include "kernel/util/debug.h"
#include "kernel/ipc/signal.h"
#include "kernel/util/math.h"
#include "kernel/util/string/string.h"

static int ntty_open(struct tty_struct *tty) {
  tty->buffer = kcalloc(1, N_TTY_BUF_SIZE);

  tty->separators = 0;
  tty->mutex = semaphore_alloc(1, 1);
  tty->to_read = semaphore_alloc(N_TTY_BUF_SIZE, 0);
  tty->to_write = semaphore_alloc(N_TTY_BUF_SIZE, N_TTY_BUF_SIZE);
  
  INIT_LIST_HEAD(&tty->read_wait.list);
  INIT_LIST_HEAD(&tty->write_wait.list);
  INIT_LIST_HEAD(&tty->separator_wait.list);

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

static void ntty_pop_char_raw(struct tty_struct *tty, char *ch) {
  semaphore_down(tty->to_read);
  semaphore_down(tty->mutex);
  enter_critical_section();
  *ch = tty->buffer[tty->read_head];
  tty->read_head = N_TTY_BUF_ALIGN(tty->read_head + 1);
  
  if (LINE_SEPARATOR(tty, *ch)) {
    assert(tty->separators > 0);
    tty->separators--;
  }

  semaphore_up(tty->mutex);
  semaphore_up(tty->to_write);
  leave_critical_section();
}

static uint32_t ntty_read(struct tty_struct *tty, struct vfs_file *file, char *buf, size_t nr) {

  int i = 0;

  // NOTE: needs to be protected with mutex?
  if (L_ICANON(tty))
    wait_event(&tty->separator_wait, tty->separators > 0);

  do {
    ntty_pop_char_raw(tty, &buf[i]);
    wake_up(&tty->write_wait.list);

    if ((L_ICANON(tty) && LINE_SEPARATOR(tty, buf[i])) || tty->read_head == tty->read_tail)
      break;
      
    ++i;
  } while (i < nr);


  return min(nr, i + 1);
}

static uint32_t push_buf_raw(struct tty_struct *tty, char ch) {
  semaphore_down(tty->to_write);
  semaphore_down(tty->mutex);
  enter_critical_section();

  tty->buffer[tty->read_tail] = ch;
  tty->read_tail = N_TTY_BUF_ALIGN(tty->read_tail + 1);

  if (LINE_SEPARATOR(tty, ch))
    tty->separators++;

  semaphore_up(tty->mutex);
  semaphore_up(tty->to_read);
  leave_critical_section();
}

static void eraser(struct tty_struct *tty, char ch) {
  semaphore_down(tty->mutex);
  enter_critical_section();
  
  if (tty->read_tail != tty->read_head) {
    uint16_t* tmp = tty->read_tail > tty->read_head? &tty->read_tail : &tty->read_head;

    if (*tmp == 0)
      *tmp = N_TTY_BUF_SIZE;

    *tmp = *tmp - 1;
  }

  semaphore_up(tty->mutex);
  semaphore_up(tty->to_write);
  leave_critical_section();
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
          sig = SIGKILL; // SIGINT;
        else if (QUIT_CHAR(tty) == ch)
          sig = SIGQUIT;
        else if (SUSP_CHAR(tty) == ch)
          sig = SIGTSTP;

        if (valid_signal(sig) && sig > 0) {
          if (tty->pgrp > 0) {
            do_signal(-tty->pgrp, sig);
          }
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
      
      push_buf_raw(tty, ch);
      if (LINE_SEPARATOR(tty, ch)) { 
        wake_up(&tty->separator_wait);
        wake_up(&tty->read_wait.list);
      }
      
    }
  } else {
    for (int i = 0; i < nr; ++i) {
		  push_buf_raw(tty, buf[i]);
      wake_up(&tty->read_wait.list);
    }
  }
}

static unsigned int ntty_poll(struct tty_struct *tty, struct vfs_file *file, struct poll_table *ptable) {
	uint32_t mask = 0;

	poll_wait(file, &tty->read_wait, ptable);
	poll_wait(file, &tty->write_wait, ptable);
  
  bool is_canon = L_ICANON(tty);

	if (semaphore_get_val(tty->to_read)) {
		mask |= POLLIN | POLLRDNORM;

    if (L_ICANON(tty) && tty->separators == 0)
      mask = 0;
  }

	if (semaphore_get_val(tty->to_write))
		mask |= POLLOUT | POLLWRNORM;

	return mask;
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
