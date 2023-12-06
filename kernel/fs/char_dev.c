#include <sys/errno.h>

#include "kernel/memory/malloc.h"
#include "kernel/util/string/string.h"

#include "kernel/fs/char_dev.h"

struct list_head devlist = {};

struct char_device *alloc_chrdev(const char *name, uint32_t major, uint32_t minor, int32_t minorct/*, struct vfs_file_operations *ops*/) {
	struct char_device *cdev = kcalloc(1, sizeof(struct char_device));
	cdev->name = strdup(name);
	cdev->major = major;
	cdev->baseminor = minor;
	cdev->minorct = minorct;
	cdev->dev = MKDEV(major, minor);
	//cdev->f_ops = ops;

	return cdev;
}

static struct char_device *get_chrdev(dev_t dev) {
	struct char_device *iter = NULL;
	list_for_each_entry(iter, &devlist, sibling) {
		if (iter->dev == dev || (MKDEV(iter->major, iter->baseminor) <= dev && dev < MKDEV(iter->major, iter->baseminor + iter->minorct)))
			return iter;
	};
	return NULL;
}

int register_chrdev(struct char_device *new_cdev) {
	struct char_device *exist = get_chrdev(new_cdev->dev);
	if (exist == NULL) {
		list_add_tail(&new_cdev->sibling, &devlist);
		return 0;
	}
	else
		return -EEXIST;
}

char* TEST_VARIABLE = "TEST VARIABLE";

void chrdev_init() {
	INIT_LIST_HEAD(&devlist);
}