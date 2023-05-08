#ifndef KERNEL_INFO_H
#define KERNEL_INFO_H

#include <stdint.h>

extern void *kernel_start;
extern void *kernel_text_start;
extern void *kernel_text_end;
extern void *kernel_data_start;
extern void *kernel_data_end;
extern void *kernel_end;
extern void *stack_bottom;
extern void *stack_top;

#define KERNEL_START (uint32_t)(&kernel_start)
#define KERNEL_TEXT_START (uint32_t)(&kernel_text_start)
#define KERNEL_TEXT_END (uint32_t)(&kernel_text_end)
#define KERNEL_DATA_START (uint32_t)(&kernel_data_start)
#define KERNEL_DATA_END (uint32_t)(&kernel_data_end)
#define KERNEL_END (uint32_t)(&kernel_end)
#define STACK_BOTTOM (uint32_t)(&stack_bottom)
#define STACK_TOP (uint32_t)(&stack_top)


#endif
