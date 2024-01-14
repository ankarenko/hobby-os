#ifndef KERNEL_FS_CHAR_DEV_H
#define KERNEL_FS_CHAR_DEV_H

#include "kernel/util/types.h"

#include "kernel/fs/vfs.h"

// cdev
// 16 bit was not enough and it was extended to 32 bits by linux 
#define MINORBITS 20
#define MINORMASK ((1U << MINORBITS) - 1)

#define MAJOR(dev) ((unsigned int)((dev) >> MINORBITS))
#define MINOR(dev) ((unsigned int)((dev)&MINORMASK))
#define MKDEV(ma, mi) (((ma) << MINORBITS) | (mi))

#define PATH_DEV "/dev/"
#define SPECNAMELEN 255 /* max length of devicename */

struct char_device {
	const char *name;
	uint32_t major;
	uint32_t baseminor;
	int32_t minorct;

	dev_t dev;
	struct list_head sibling;
	struct vfs_file_operations *f_ops;
};

#define DECLARE_CHRDEV(_name, _major, _baseminor, _minorct, _ops) \
{                                                           \
  .name = _name,                                            \
  .major = _major,                                          \
  .baseminor = _baseminor,                                  \
  .minorct = _minorct,                                      \
  .dev = MKDEV(_major, _baseminor),                         \
  .f_ops = _ops,                                            \
}

struct char_device *alloc_chrdev(const char *name, uint32_t major, uint32_t minor, int32_t minorct, struct vfs_file_operations *ops);
int register_chrdev(struct char_device *cdev);
int unregister_chrdev(dev_t dev);
void chrdev_init();

extern struct vfs_file_operations def_chr_fops;

#endif 