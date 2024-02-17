#include "kernel/devices/char/tty.h"

#include "kernel/devices/char/termios.h"
#include "kernel/fs/char_dev.h"
#include "kernel/locking/semaphore.h"
#include "kernel/memory/malloc.h"
#include "kernel/proc/task.h"
#include "kernel/util/debug.h"
#include "kernel/util/errno.h"
#include "kernel/util/fcntl.h"
#include "kernel/util/ioctls.h"
#include "kernel/util/string/string.h"
#include "kernel/util/vsprintf.h"

static struct list_head tty_drivers;
struct termios tty_std_termios = {
    .c_iflag = ICRNL | IXON,
    .c_oflag = OPOST | ONLCR,
    .c_cflag = B38400 | CS8 | CREAD | HUPCL,
    .c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE | IEXTEN,
    .c_cc = INIT_C_CC};

struct tty_driver *alloc_tty_driver(int32_t lines) {
  struct tty_driver *driver = kcalloc(1, sizeof(struct tty_driver));
  driver->magic = TTY_DRIVER_MAGIC;
  driver->num = lines;

  return driver;
}

static void tty_default_put_char(struct tty_struct *tty, const char ch) {
  tty->driver->tops->write(tty, &ch, 1);
}

static struct tty_struct *find_tty_from_driver(struct tty_driver *driver, int idx) {
  struct tty_struct *iter;
  list_for_each_entry(iter, &driver->ttys, sibling) {
    if (iter->index == idx)
      return iter;
  }

  return NULL;
}

static struct tty_struct *create_tty_struct(struct tty_driver *driver, int idx) {
  struct tty_struct *tty = kcalloc(1, sizeof(struct tty_struct));
  tty->magic = TTY_MAGIC;
  tty->index = idx;
  tty->driver = driver;
  tty->termios = &driver->init_termios;
  // TODO: not the best design, but lets leave it like this
  // when ICANON is changed it needs to be closed and opened again
  tty->ldisc = L_ICANON(tty) ? &tty_ldisc_N_TTY_canon : &tty_ldisc_N_TTY_raw;

  // sprintf(tty->name, "%s/%d", driver->name, idx);

  list_add_tail(&tty->sibling, &driver->ttys);

  if (tty->ldisc->open)
    tty->ldisc->open(tty);

  return tty;
}

static unsigned int tty_poll(struct vfs_file *file, struct poll_table *pt) {
	struct tty_struct *tty = (struct tty_struct *)file->private_data;
	struct tty_ldisc *ld = tty->ldisc;

	if (!tty || !ld->poll)
		return -EIO;

	return ld->poll(tty, file, pt);
}

static int tiocsctty(struct tty_struct *tty, int arg) {
  struct process *current_process = get_current_process();

  if (current_process->pid == current_process->sid && current_process->sid == tty->session)
    return 0;

  if (current_process->pid != current_process->sid || current_process->tty)
    return -EPERM;

  if (tty->session) {
    if (arg != 1)
      return -EPERM;

    struct process *proc;
    process_for_each_entry(proc) {
      if (proc->tty == tty)
        proc->tty = NULL;
    }
  }

  current_process->tty = tty;
  tty->session = current_process->sid;
  tty->pgrp = current_process->gid;
  return 0;
}

static int ptmx_open(struct vfs_inode *inode, struct vfs_file *file) {
  int index = get_next_pty_number();
  // master
  struct tty_struct *ttym = create_tty_struct(ptm_driver, index);
  file->private_data = ttym;
  ttym->driver->tops->open(ttym, file);
  assert(tiocsctty(ttym, 0) == 0);

  // slave
  struct tty_struct *ttys = create_tty_struct(pts_driver, index);
  ttym->link = ttys;
  ttys->link = ttym;

  char path[sizeof(PATH_DEV) + SPECNAMELEN] = {0};
  sprintf(path, "/dev/%s", ttys->name);
  int dev = MKDEV(UNIX98_PTY_SLAVE_MAJOR, index);
  vfs_mknod(path, S_IFCHR, dev);

  return 0;
}

static int tiocgpgrp(struct tty_struct *tty, int arg) {
  pid_t *pgrp = (pid_t *)arg;

  if (get_current_process()->tty != tty)
    return -ENOTTY;

  if (pgrp)
    *pgrp = tty->pgrp;
  return 0;
}

static int tiocspgrp(struct tty_struct *tty, int arg) {
  pid_t pgrp = *(uint32_t *)arg;
  struct process *p = find_process_by_pid(pgrp);

  if (!p)
    return -EINVAL;
  if (tty->session != p->sid)
    return -EPERM;
  tty->pgrp = pgrp;
  return 0;
}

/*
  https://man7.org/linux/man-pages/man2/ioctl_tty.2.html
*/
static int tty_ioctl(struct vfs_inode *inode, struct vfs_file *file, unsigned int cmd, unsigned long arg) {
  struct tty_struct *tty = (struct tty_struct *)file->private_data;

  if (!tty)
    return -ENODEV;

  switch (cmd) {
    // case TCGETS:
    //   return tcgets(tty, arg);
    //  TODO: 2020-11-26 Implement different actions based on TCSETS, TCSETSW and TCSETSF
    /*
    case TCSETS:
    case TCSETSW:
    case TCSETSF:
      return tcsets(tty, arg);
    */
    case TIOCSCTTY:
      return tiocsctty(tty, arg);

    case TIOCGPGRP:
      return tiocgpgrp(tty, arg);
    
    case TIOCSPGRP:
      return tiocspgrp(tty, arg);

    /*
    case FIONREAD:
      return fionread(tty, arg);
    case TIOCGWINSZ:
      return tiocgwinsz(tty, arg);
    case TIOCSWINSZ:
      return tiocswinsz(tty, arg);
    */
    default:
      assert_not_implemented("cmd %d is not supported", cmd);
      break;
  }
  return 0;
}

static int tty_open(struct vfs_inode *inode, struct vfs_file *file) {
  struct tty_struct *tty = NULL;
  int32_t dev = inode->i_rdev;
  struct process *current_process = get_current_process();

  if (MAJOR(dev) == UNIX98_PTY_SLAVE_MAJOR)
    tty = find_tty_from_driver(pts_driver, MINOR(dev));
  else if (MAJOR(dev) == TTYAUX_MAJOR && MINOR(dev) == 0)
    tty = current_process->tty;
  else if (MAJOR(dev) == TTY_MAJOR && MINOR(dev) >= SERIAL_MINOR_BASE) {
    tty = find_tty_from_driver(serial_driver, MINOR(dev));
    if (!tty) {
      tty = create_tty_struct(serial_driver, MINOR(dev) - SERIAL_MINOR_BASE);
    }
  }

  if (tty && tty->driver->tops->open)
    tty->driver->tops->open(tty, file);

  file->private_data = tty;
  tiocsctty(tty, 0);

  return 0;
}

int32_t tty_read(struct vfs_file *file, uint8_t *buffer, uint32_t length, off_t ppos) {
  struct tty_struct *tty = (struct tty_struct *)file->private_data;
  struct tty_ldisc *ld = tty->ldisc;

  if (!tty || !ld->read)
    return -EIO;

  return ld->read(tty, file, buffer, length);
}

static uint32_t tty_write(struct vfs_file *file, const char *buf, size_t count, off_t ppos) {
  struct tty_struct *tty = (struct tty_struct *)file->private_data;

  if (!tty || !tty->driver->tops->write || !tty->ldisc->write)
    return -EIO;

  struct tty_ldisc *ld = tty->ldisc;

  return ld->write(tty, file, buf, count);
}

static struct vfs_file_operations tty_fops = {
  .open = tty_open,
  .read = tty_read,
  .write = tty_write,
  .ioctl = tty_ioctl,
  .poll = tty_poll
};

static struct vfs_file_operations ptmx_fops = {
  .open = ptmx_open,
  .read = tty_read,
  .write = tty_write,
  .ioctl = tty_ioctl,
  .poll = tty_poll
};

int tty_register_driver(struct tty_driver *driver) {
  struct char_device *cdev = alloc_chrdev(driver->name, driver->major, driver->minor_start, driver->num, &tty_fops);
  register_chrdev(cdev);
  driver->cdev = cdev;

  if (!(driver->flags & TTY_DRIVER_NO_DEVFS)) {
    char name[sizeof(PATH_DEV) + SPECNAMELEN] = {0};
    for (int i = 0; i < driver->num; ++i) {
      memset(&name, 0, sizeof(name));
      sprintf(name, "/dev/%s%d", driver->name, i);

      vfs_mknod(name, S_IFCHR, MKDEV(driver->major, driver->minor_start + i));
    }
  }
  if (!driver->tops->put_char)
    driver->tops->put_char = tty_default_put_char;

  list_add_tail(&driver->sibling, &tty_drivers);
  return 0;
}

void tty_init() {
  INIT_LIST_HEAD(&tty_drivers);

  vfs_mknod("/dev/pts", S_IFDIR, O_CREAT);

  log("TTY: Mount ptmx");
  struct char_device *ptmx_cdev = alloc_chrdev("ptmx", TTYAUX_MAJOR, 2, 1, &ptmx_fops);
  register_chrdev(ptmx_cdev);
  vfs_mknod("/dev/ptmx", S_IFCHR, ptmx_cdev->dev);

  log("TTY: Mount tty");
  struct char_device *tty_cdev = alloc_chrdev("tty", TTYAUX_MAJOR, 0, 1, &tty_fops);
  register_chrdev(tty_cdev);
  // i don't see any sense
  // vfs_mknod("/dev/tty", S_IFCHR, tty_cdev->dev);

  log("TTY: Init pty");
  pty_init();

  log("TTY: Init serial");
  serial_init();
}