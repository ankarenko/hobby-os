#ifndef KERNEL_DEVICES_CHAR_TTY_H
#define KERNEL_DEVICES_CHAR_TTY_H

#include <stdint.h>

#include "kernel/devices/char/termios.h"
#include "kernel/util/list.h"
#include "kernel/fs/vfs.h"

#define INTR_CHAR(tty) ((tty)->termios->c_cc[VINTR])
#define QUIT_CHAR(tty) ((tty)->termios->c_cc[VQUIT])
#define ERASE_CHAR(tty) ((tty)->termios->c_cc[VERASE])
#define KILL_CHAR(tty) ((tty)->termios->c_cc[VKILL])
#define EOF_CHAR(tty) ((tty)->termios->c_cc[VEOF])
#define TIME_CHAR(tty) ((tty)->termios->c_cc[VTIME])
#define MIN_CHAR(tty) ((tty)->termios->c_cc[VMIN])
#define SWTC_CHAR(tty) ((tty)->termios->c_cc[VSWTC])
#define START_CHAR(tty) ((tty)->termios->c_cc[VSTART])
#define STOP_CHAR(tty) ((tty)->termios->c_cc[VSTOP])
#define SUSP_CHAR(tty) ((tty)->termios->c_cc[VSUSP])
#define EOL_CHAR(tty) ((tty)->termios->c_cc[VEOL])
#define REPRINT_CHAR(tty) ((tty)->termios->c_cc[VREPRINT])
#define DISCARD_CHAR(tty) ((tty)->termios->c_cc[VDISCARD])
#define WERASE_CHAR(tty) ((tty)->termios->c_cc[VWERASE])
#define LNEXT_CHAR(tty) ((tty)->termios->c_cc[VLNEXT])
#define EOL2_CHAR(tty) ((tty)->termios->c_cc[VEOL2])
#define LINE_SEPARATOR(tty, ch) ({ \
    typeof(ch) _ch = (ch); \
    EOF_CHAR(tty) == _ch || EOL_CHAR(tty) == _ch || (EOL2_CHAR(tty) == _ch && L_IEXTEN(tty)) || _ch == '\n'; })

#define _I_FLAG(tty, f) ((tty)->termios->c_iflag & (f))
#define _O_FLAG(tty, f) ((tty)->termios->c_oflag & (f))
#define _C_FLAG(tty, f) ((tty)->termios->c_cflag & (f))
#define _L_FLAG(tty, f) ((tty)->termios->c_lflag & (f))

#define I_IGNBRK(tty) _I_FLAG((tty), IGNBRK)
#define I_BRKINT(tty) _I_FLAG((tty), BRKINT)
#define I_IGNPAR(tty) _I_FLAG((tty), IGNPAR)
#define I_PARMRK(tty) _I_FLAG((tty), PARMRK)
#define I_INPCK(tty) _I_FLAG((tty), INPCK)
#define I_ISTRIP(tty) _I_FLAG((tty), ISTRIP)
#define I_INLCR(tty) _I_FLAG((tty), INLCR)
#define I_IGNCR(tty) _I_FLAG((tty), IGNCR)
#define I_ICRNL(tty) _I_FLAG((tty), ICRNL)
#define I_IUCLC(tty) _I_FLAG((tty), IUCLC)
#define I_IXON(tty) _I_FLAG((tty), IXON)
#define I_IXANY(tty) _I_FLAG((tty), IXANY)
#define I_IXOFF(tty) _I_FLAG((tty), IXOFF)
#define I_IMAXBEL(tty) _I_FLAG((tty), IMAXBEL)
#define I_IUTF8(tty) _I_FLAG((tty), IUTF8)

#define O_OPOST(tty) _O_FLAG((tty), OPOST)
#define O_OLCUC(tty) _O_FLAG((tty), OLCUC)
#define O_ONLCR(tty) _O_FLAG((tty), ONLCR)
#define O_OCRNL(tty) _O_FLAG((tty), OCRNL)
#define O_ONOCR(tty) _O_FLAG((tty), ONOCR)
#define O_ONLRET(tty) _O_FLAG((tty), ONLRET)
#define O_OFILL(tty) _O_FLAG((tty), OFILL)
#define O_OFDEL(tty) _O_FLAG((tty), OFDEL)
#define O_NLDLY(tty) _O_FLAG((tty), NLDLY)
#define O_CRDLY(tty) _O_FLAG((tty), CRDLY)
#define O_TABDLY(tty) _O_FLAG((tty), TABDLY)
#define O_BSDLY(tty) _O_FLAG((tty), BSDLY)
#define O_VTDLY(tty) _O_FLAG((tty), VTDLY)
#define O_FFDLY(tty) _O_FLAG((tty), FFDLY)

#define C_BAUD(tty) _C_FLAG((tty), CBAUD)
#define C_CSIZE(tty) _C_FLAG((tty), CSIZE)
#define C_CSTOPB(tty) _C_FLAG((tty), CSTOPB)
#define C_CREAD(tty) _C_FLAG((tty), CREAD)
#define C_PARENB(tty) _C_FLAG((tty), PARENB)
#define C_PARODD(tty) _C_FLAG((tty), PARODD)
#define C_HUPCL(tty) _C_FLAG((tty), HUPCL)
#define C_CLOCAL(tty) _C_FLAG((tty), CLOCAL)
#define C_CIBAUD(tty) _C_FLAG((tty), CIBAUD)
#define C_CRTSCTS(tty) _C_FLAG((tty), CRTSCTS)

#define L_ISIG(tty) _L_FLAG((tty), ISIG)
#define L_ICANON(tty) _L_FLAG((tty), ICANON)
#define L_XCASE(tty) _L_FLAG((tty), XCASE)
#define L_ECHO(tty) _L_FLAG((tty), ECHO)
#define L_ECHOE(tty) _L_FLAG((tty), ECHOE)
#define L_ECHOK(tty) _L_FLAG((tty), ECHOK)
#define L_ECHONL(tty) _L_FLAG((tty), ECHONL)
#define L_NOFLSH(tty) _L_FLAG((tty), NOFLSH)
#define L_TOSTOP(tty) _L_FLAG((tty), TOSTOP)
#define L_ECHOCTL(tty) _L_FLAG((tty), ECHOCTL)
#define L_ECHOPRT(tty) _L_FLAG((tty), ECHOPRT)
#define L_ECHOKE(tty) _L_FLAG((tty), ECHOKE)
#define L_FLUSHO(tty) _L_FLAG((tty), FLUSHO)
#define L_PENDIN(tty) _L_FLAG((tty), PENDIN)
#define L_IEXTEN(tty) _L_FLAG((tty), IEXTEN)

/*
  UART driver (struct tty_driver) manages the physical transmission of bytes, including parity checks and flow control
  Line discipline (struct tty_lisc) is the software module sits in between and is incharge of handling special characters, echoing ...
  TTY device has the corresponding character device file under /dev/ttyX
  TTY driver manages session with controling terminal
*/
// https://www.kernel.org/doc/Documentation/admin-guide/devices.txt
// major and minor numbers define type of a character device
#define PTY_MASTER_MAJOR 2
#define PTY_SLAVE_MAJOR 3
#define TTY_MAJOR 4
#define TTYAUX_MAJOR 5
#define UNIX98_PTY_MASTER_MAJOR 128
#define UNIX98_PTY_MAJOR_COUNT 8
#define UNIX98_PTY_SLAVE_MAJOR (UNIX98_PTY_MASTER_MAJOR + UNIX98_PTY_MAJOR_COUNT)
#define N_TTY_BUF_SIZE 32
#define N_TTY_BUF_ALIGN(v) (v % N_TTY_BUF_SIZE)
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
	struct termios init_termios;
};

struct tty_struct {
	int magic;
	int index;
	char name[64];
	struct tty_driver *driver;
	struct tty_ldisc *ldisc;
	struct tty_struct *link;
	struct termios *termios;
	//struct winsize winsize;

	//pid_t pgrp;
	//pid_t session;

  struct semaphore *to_read;
  struct semaphore *to_write;
  struct semaphore *mutex;
  
	char* buffer;
  uint8_t read_tail;
  uint8_t read_head;

  struct list_head sibling;
};

struct tty_operations {
	int (*open)(struct tty_struct *tty, struct vfs_file *filp);
	void (*close)(struct tty_struct *tty, struct vfs_file *filp);
	int (*write)(struct tty_struct *tty, const char *buf, int count);
	void (*put_char)(struct tty_struct *tty, const char ch);
  char (*read)(struct tty_struct *tty);
	int (*write_room)(struct tty_struct *tty);
};

// tty.h
struct tty_driver *alloc_tty_driver(int32_t lines);
int tty_register_driver(struct tty_driver *driver);
void tty_init();
char read_serial_test();
extern struct termios tty_std_termios;

// serial.c
void serial_enable(int port);
void serial_output(int port, char a);
extern struct tty_driver *serial_driver;

// n_tty_canon.c
extern struct tty_ldisc tty_ldisc_N_TTY_canon;

// n_tty_raw.c
extern struct tty_ldisc tty_ldisc_N_TTY_raw;

// pty.c
extern struct tty_driver *ptm_driver, *pts_driver;
void pty_init();
int get_next_pty_number();

#endif