#include "kernel/devices/char/tty.h"

#define NR_PTY_MAX (1 << MINORBITS)

struct tty_driver *ptm_driver, *pts_driver;
volatile int next_pty_number = 0;

int get_next_pty_number() {
	return next_pty_number++;
}

void pty_init() {

}