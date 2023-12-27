#ifndef KERNEL_DEVICES_CHAR_TTY_H
#define KERNEL_DEVICES_CHAR_TTY_H

#include <stdint.h>

#include "kernel/util/list.h"
#include "kernel/fs/vfs.h"

/*
  UART driver (struct tty_driver) manages the physical transmission of bytes, including parity checks and flow control
  Line discipline (struct tty_lisc) is the software module sits in between and is incharge of handling special characters, echoing ...
  TTY driver manages session with controling terminal
  TTY device has the corresponding character device file under /dev/ttyX
*/

#define PTY_MASTER_MAJOR 2
#define PTY_SLAVE_MAJOR 3
#define TTY_MAJOR 4
#define TTYAUX_MAJOR 5
#define UNIX98_PTY_MASTER_MAJOR 128
#define UNIX98_PTY_MAJOR_COUNT 8
#define UNIX98_PTY_SLAVE_MAJOR (UNIX98_PTY_MASTER_MAJOR + UNIX98_PTY_MAJOR_COUNT)
#define N_TTY_BUF_SIZE 4096
#define N_TTY_BUF_ALIGN(v) ((v) & (N_TTY_BUF_SIZE - 1))
#define NCCS 19
#define __DISABLED_CHAR '\0'
#define SERIAL_MINOR_BASE 64

#define TTY_DRIVER_INSTALLED 0x0001
#define TTY_DRIVER_RESET_TERMIOS 0x0002
#define TTY_DRIVER_REAL_RAW 0x0004
#define TTY_DRIVER_NO_DEVFS 0x0008
#define TTY_DRIVER_DEVPTS_MEM 0x0010

/* tty driver types */
#define TTY_DRIVER_TYPE_SYSTEM 0x0001
#define TTY_DRIVER_TYPE_CONSOLE 0x0002
#define TTY_DRIVER_TYPE_SERIAL 0x0003
#define TTY_DRIVER_TYPE_PTY 0x0004
#define TTY_DRIVER_TYPE_SCC 0x0005 /* scc driver */
#define TTY_DRIVER_TYPE_SYSCONS 0x0006

/* system subtypes (magic, used by tty_io.c) */
#define SYSTEM_TYPE_TTY 0x0001
#define SYSTEM_TYPE_CONSOLE 0x0002
#define SYSTEM_TYPE_SYSCONS 0x0003
#define SYSTEM_TYPE_SYSPTMX 0x0004

/* pty subtypes (magic, used by tty_io.c) */
#define PTY_TYPE_MASTER 0x0001
#define PTY_TYPE_SLAVE 0x0002

/* serial subtype definitions */
#define SERIAL_TYPE_NORMAL 1

#define TTY_MAGIC 0x5401
#define TTY_DRIVER_MAGIC 0x5402
#define TTY_LDISC_MAGIC 0x5403
#define N_TTY_BUF_SIZE 4096

struct tty_ldisc {
	int magic;
	int num;
	struct list_head sibling;

	int (*open)(struct tty_struct *);
	void (*close)(struct tty_struct *);
	ssize_t (*read)(struct tty_struct *tty, struct vfs_file *file, char *buf, size_t nr);
	ssize_t (*write)(struct tty_struct *tty, struct vfs_file *file, const char *buf, size_t nr);
	int (*receive_room)(struct tty_struct *tty);
	void (*receive_buf)(struct tty_struct *tty, const char *cp, int count);
	//unsigned int (*poll)(struct tty_struct *tty, struct vfs_file *, struct poll_table *);
};

struct tty_driver {
	int32_t magic;
	const char *driver_name;
	const char *name;
	struct char_device *cdev;
	int major;		 /* major device number */
	int minor_start; /* start of minor device number */
	int num;		 /* number of devices allocated */
	short type;		 /* type of tty driver */
	short subtype;	 /* subtype of tty driver */
	int flags;		 /* tty driver flags */
	struct tty_operations *tops;
	struct list_head ttys;
	struct list_head sibling;
	struct tty_driver *other;
	//struct termios init_termios;
};

struct tty_struct {
	int magic;
	int index;
	char name[64];
	struct tty_driver *driver;
	struct tty_ldisc *ldisc;
	struct tty_struct *link;
	//struct termios *termios;
	//struct winsize winsize;

	//pid_t pgrp;
	//pid_t session;

	//struct wait_queue_head write_wait;
	//struct wait_queue_head read_wait;
	int column;
	int read_head;
	int read_tail;
	int read_count;
	char *read_buf;
	char *write_buf;
	struct list_head sibling;
};

struct tty_operations {
	int (*open)(struct tty_struct *tty, struct vfs_file *filp);
	void (*close)(struct tty_struct *tty, struct vfs_file *filp);
	int (*write)(struct tty_struct *tty, const char *buf, int count);
	void (*put_char)(struct tty_struct *tty, const char ch);
	int (*write_room)(struct tty_struct *tty);
};

struct tty_driver *alloc_tty_driver(int32_t lines);
int tty_register_driver(struct tty_driver *driver);
void tty_init();

// serial.c
void serial_enable(int port);
void serial_output(int port, char a);
extern struct tty_driver *serial_driver;

// n_tty.c
extern struct tty_ldisc tty_ldisc_N_TTY;

#endif