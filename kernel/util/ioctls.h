#ifndef KERNEL_UTIL_IOCTL_H
#define KERNEL_UTIL_IOCTL_H

#define TIOCSCTTY 0x5480 /* become controlling tty */
#define TIOCGSOFTCAR 0x5481
#define TIOCSSOFTCAR 0x5482
#define TIOCLINUX 0x5483
#define TIOCGSERIAL 0x5484
#define TIOCSSERIAL 0x5485
#define TCSBRKP 0x5486 /* Needed for POSIX tcsendbreak() */
#define TIOCSERCONFIG 0x5488
#define TIOCSERGWILD 0x5489
#define TIOCSERSWILD 0x548a
#define TIOCGLCKTRMIOS 0x548b
#define TIOCSLCKTRMIOS 0x548c
#define TIOCSERGSTRUCT 0x548d  /* For debugging only */
#define TIOCSERGETLSR 0x548e   /* Get line status register */
#define TIOCSERGETMULTI 0x548f /* Get multiport config	*/
#define TIOCSERSETMULTI 0x5490 /* Set multiport config */
#define TIOCMIWAIT 0x5491	   /* wait for a change on serial input line(s) */
#define TIOCGICOUNT 0x5492	   /* read serial port inline interrupt counts */

#endif