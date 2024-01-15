#include "kernel/devices/char/tty.h"

#include "kernel/fs/char_dev.h"
#include "kernel/proc/task.h"
#include "kernel/memory/malloc.h"
#include "kernel/util/debug.h"
#include "kernel/util/errno.h"
#include "kernel/util/fcntl.h"
#include "kernel/util/string/string.h"
#include "kernel/util/vsprintf.h"

static struct list_head tty_drivers;

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
  tty->ldisc = &tty_ldisc_N_TTY;
  //tty->termios = &driver->init_termios;
  //tty->winsize.ws_col = 80;
  //tty->winsize.ws_row = 40;
  sprintf(tty->name, "%s/%d", driver->name, idx);

  //INIT_LIST_HEAD(&tty->read_wait.list);
  //INIT_LIST_HEAD(&tty->write_wait.list);
  list_add_tail(&tty->sibling, &driver->ttys);

  //if (tty->ldisc->open)
  //  tty->ldisc->open(tty);

  return tty;
}

static int ptmx_open(struct vfs_inode *inode, struct vfs_file *file) {
	int index = get_next_pty_number();
  // master
	struct tty_struct *ttym = create_tty_struct(ptm_driver, index);
	file->private_data = ttym;
	ttym->driver->tops->open(ttym, file);
  // Make the given terminal the controlling terminal of the
  // calling process.  The calling process must be a session
  // https://man7.org/linux/man-pages/man2/ioctl_tty.2.html
	// tiocsctty(ttym, 0);

  // slave
	struct tty_struct *ttys = create_tty_struct(pts_driver, index);
	ttym->link = ttys;
	ttys->link = ttym;

	char path[sizeof(PATH_DEV) + SPECNAMELEN] = {0};
	sprintf(path, "/dev/%s", ttys->name);
	vfs_mknod(path, S_IFCHR, MKDEV(UNIX98_PTY_SLAVE_MAJOR, index));

	return 0;
}

static int tty_open(struct vfs_inode *inode, struct vfs_file *file) {
  struct tty_struct *tty = NULL;
  dev_t dev = inode->i_rdev;
  process *current_process = get_current_process();

  if (MAJOR(dev) == UNIX98_PTY_SLAVE_MAJOR)
    tty = find_tty_from_driver(pts_driver, MINOR(dev));
  else if (MAJOR(dev) == TTYAUX_MAJOR && MINOR(dev) == 0)
    tty = current_process->tty;
  else if (MAJOR(dev) == TTY_MAJOR && MINOR(dev) >= SERIAL_MINOR_BASE) {
    tty = find_tty_from_driver(serial_driver, MINOR(dev));
    if (!tty)
      tty = create_tty_struct(serial_driver, MINOR(dev) - SERIAL_MINOR_BASE);
  } 

  if (tty && tty->driver->tops->open)
    tty->driver->tops->open(tty, file);

  file->private_data = tty;
  //tiocsctty(tty, 0);
  
  return 0;
}

int32_t tty_read(struct vfs_file* file, uint8_t* buffer, uint32_t length, off_t ppos) {
  struct tty_struct *tty = (struct tty_struct *)file->private_data;
  struct tty_ldisc *ld = tty->ldisc;

  if (!tty || !tty->driver->tops->write || !ld->write)
    return -EIO;
  
  char c = tty->driver->tops->read(tty);
  buffer[0] = c;  
  // don't use ld, move directly to uart 

  // check if it's a '\n'
  return (int)c == 13? 0 : 1; // ld->write(tty, file, buf, count);
}

static ssize_t tty_write(struct vfs_file *file, const char *buf, size_t count, off_t ppos) {
  struct tty_struct *tty = (struct tty_struct *)file->private_data;

  if (!tty || !tty->driver->tops->write || !tty->ldisc->write)
    return -EIO;

  struct tty_ldisc *ld = tty->ldisc;

  // don't use ld, move directly to uart 
  return tty->driver->tops->write(tty, buf, count); // ld->write(tty, file, buf, count);
}

static struct vfs_file_operations tty_fops = {
  .open = tty_open,
  .read = tty_read,
  .write = tty_write
};

static struct vfs_file_operations ptmx_fops = {
  .open = ptmx_open,
  //.read = tty_read,
  //.write = tty_write
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