#include "kernel/memory/malloc.h"
#include "kernel/fs/char_dev.h"
#include "kernel/util/string/string.h"
#include "kernel/util/fcntl.h"

#include "kernel/devices/char/tty.h"

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

int tty_register_driver(struct tty_driver *driver) {
	struct char_device *cdev = alloc_chrdev(driver->name, driver->major, driver->minor_start, driver->num, NULL/*, &tty_fops*/);
	register_chrdev(cdev);
	driver->cdev = cdev;
  
	if (!(driver->flags & TTY_DRIVER_NO_DEVFS))
	{
		char name[sizeof(PATH_DEV) + SPECNAMELEN] = {0};
		for (int i = 0; i <= driver->num; ++i)
		{
			memset(&name, 0, sizeof(name));
			//sprintf(name, "/dev/%s%d", driver->name, i);
			
      vfs_open(name, O_CREAT); // ??

      //vfs_mknod(name, S_IFCHR, MKDEV(driver->major, driver->minor_start + i));
		}
	}
	if (!driver->tops->put_char)
		driver->tops->put_char = tty_default_put_char;
	list_add_tail(&driver->sibling, &tty_drivers);

	return 0;
}


void tty_init()
{
	INIT_LIST_HEAD(&tty_drivers);
  /*
	log("TTY: Mount ptmx");
	struct char_device *ptmx_cdev = alloc_chrdev("ptmx", TTYAUX_MAJOR, 2, 1, &ptmx_fops);
	register_chrdev(ptmx_cdev);
	vfs_mknod("/dev/ptmx", S_IFCHR, ptmx_cdev->dev);

	log("TTY: Mount tty");
	struct char_device *tty_cdev = alloc_chrdev("tty", TTYAUX_MAJOR, 0, 1, &tty_fops);
	register_chrdev(tty_cdev);
	vfs_mknod("/dev/tty", S_IFCHR, tty_cdev->dev);

	log("TTY: Init pty");
	pty_init();

	log("TTY: Init serial");
  */
  //vfs_mkdir(PATH_DEV);
  
	serial_init();
}