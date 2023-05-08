#ifndef KERNEL_INFO_H
#define KERNEL_INFO_H

#include <stdint.h>

extern void *kernel_start;
extern void *kernel_end;

#define KERNEL_START (uint32_t)(&kernel_start)
#define KERNEL_END (uint32_t)(&kernel_end)

#endif
