#ifndef _TTY_H
#define _TTY_H

#include <stddef.h>

void terminal_initialize(void);
void terminal_popchar();
void terminal_newline();
void terminal_write(const char* data, size_t size);
void terminal_writestring(const char* data);
void terminal_clrscr();
void terminal_clrline();

#endif
