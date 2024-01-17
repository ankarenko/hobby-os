#ifndef _KERNEL_TERMINAL_H
#define _KERNEL_TERMINAL_H

#include <stddef.h>

#include "kernel/devices/vga.h"

void terminal_initialize(void);
void terminal_popchar();
void terminal_newline();
void terminal_write(const char* data, size_t size);
void terminal_writestring(const char* data);
void terminal_clrscr();
void terminal_clrline();
void terminal_set_color(enum vga_color color);
void terminal_reset_color();
void terminal_run();

#endif
