#include "kernel/util/debug.h"

#include "kernel/devices/char/tty.h"

int ntty_open(struct tty_struct *tty) {
  assert_not_implemented("ntty_open is not implemented");
}

ssize_t ntty_write(struct tty_struct *tty, struct vfs_file *file, const char *buf, size_t nr) {
	assert_not_implemented("ntty_write is not implemented");
}

struct tty_ldisc tty_ldisc_N_TTY = {
	.magic = TTY_LDISC_MAGIC,
	.open = ntty_open,
	.write = ntty_write,
};
