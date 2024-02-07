#include "kernel/util/debug.h"
#include "kernel/util/math.h"
#include "kernel/util/string/string.h"
#include "kernel/locking/semaphore.h"
#include "kernel/memory/malloc.h"
#include "kernel/devices/char/tty.h"

static int ntty_open(struct tty_struct *tty) {
  tty->buffer = kcalloc(1, N_TTY_BUF_SIZE);

  tty->mutex = semaphore_alloc(1);
  tty->to_read = semaphore_alloc(N_TTY_BUF_SIZE);
  tty->to_write = semaphore_alloc(N_TTY_BUF_SIZE);

  semaphore_down_val(tty->to_read, N_TTY_BUF_SIZE);

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

static uint32_t ntty_get_room(struct tty_struct *tty) {
  return max(1, tty->to_read->count);
}

static uint32_t ntty_read(struct tty_struct *tty, struct vfs_file *file, char *buf, uint32_t nr) {
  int room = min(ntty_get_room(tty), nr);
  //log("room %d", room);int room = min(ntty_get_room(tty), nr);
  bool read_all = false;

  //log("current to_read: %d", tty->to_read->count);
  semaphore_down_val(tty->to_read, room);
  semaphore_down(tty->mutex);
  /*
  log("reading %d", room);
  log("left to read: %d", tty->to_read->count);
  log("start read: [%d, %d]", tty->read_head, tty->read_tail);
  */
  int i = 0;
  for (i = 0; i < room; ++i) {
    buf[i] = tty->buffer[tty->read_head];
    //log("raw read buf[%d] = %c", tty->read_head, buf[i]);

    tty->read_head = N_TTY_BUF_ALIGN(tty->read_head + 1);
    if (tty->read_head == tty->read_tail) {
      //read_all = true;
      ++i;
      break;
    }
  }

  buf[i] = __DISABLED_CHAR;
  //log("end read: [%d, %d]", tty->read_head, tty->read_tail);

  semaphore_up(tty->mutex);
  semaphore_up_val(tty->to_write, room);

  //log("update to write to %d", tty->to_write->count);
  return room;
}

static uint32_t echo_buf(struct tty_struct *tty, const char *buf, uint32_t nr) {
  tty->driver->tops->write(tty, buf, nr);
  return nr;
}

static uint32_t push_buf(struct tty_struct *tty, char c) {
  semaphore_down(tty->to_write);
  semaphore_down(tty->mutex);
  
  //log("raw write buf[%d] = %c", tty->read_tail, c);

  tty->buffer[tty->read_tail] = c;
  tty->read_tail = N_TTY_BUF_ALIGN(tty->read_tail + 1);
  //log("new read tail %d", tty->read_tail);
  if (tty->read_head == tty->read_tail) {
    //log("overflow: move head head: %d, tail: %d");
    //tty->read_head = N_TTY_BUF_ALIGN(tty->read_head + 1);
  }

  //log("added: [%d : %d]", tty->read_head, tty->read_tail);


  //log("[%d, %d]", tty->read_head, tty->read_tail);
  
  //log("writing");
  semaphore_up(tty->mutex);
  semaphore_up(tty->to_read);
}

static void ntty_receive_buf(struct tty_struct *tty, const char *buf, int nr) {
  for (int i = 0; i < nr; ++i) {
    push_buf(tty, buf[i]);
  }
  //log("%d", tty->to_write->count);

  if (L_ECHO(tty)) 
    echo_buf(tty, buf, nr);
}

static uint32_t ntty_write(struct tty_struct *tty, struct vfs_file *file, const char *buf, size_t nr) {
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
