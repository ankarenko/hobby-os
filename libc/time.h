#ifndef LIBC_TIME_H
#define LIBC_TIME_H

#include <sys/types.h>

struct timespec {
	time_t tv_sec;
	long tv_nsec;
};

#endif