#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "kernel/util/string/string.h"

#include "kernel/util/ctype.h"
#include "kernel/util/vsprintf.h"
#include "kernel/fs/vfs.h"
#include "kernel/util/debug.h"
#include "kernel/devices/kybrd.h"
#include "kernel/proc/task.h"
#include "kernel/util/math.h"
#include "kernel/util/ansi_codes.h"
#include "kernel/util/fcntl.h"
#include "kernel/cpu/hal.h"
#include "kernel/memory/kernel_info.h"
#include "kernel/devices/terminal.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t* const VGA_MEMORY = (uint16_t*)(0xB8000);
static const size_t VIDEO_MEMORY_SIZE = VGA_WIDTH * VGA_HEIGHT << 2;
static uint16_t BLANK;
static size_t video_memory_index;
static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer;
// static uint16_t video_memory[VIDEO_MEMORY_SIZE];

void terminal_set_color(enum vga_color color) {
  terminal_color = vga_entry_color(color, VGA_COLOR_BLACK);
}

void terminal_reset_color() {
  terminal_color = vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
}

void disable_cursor() {
  outportb(0x3D4, 0x0A);
  outportb(0x3D5, 0x20);
}

void enable_cursor(uint8_t cursor_start, uint8_t cursor_end) {
  outportb(0x3D4, 0x0A);
  outportb(0x3D5, (inportb(0x3D5) & 0xC0) | cursor_start);

  outportb(0x3D4, 0x0B);
  outportb(0x3D5, (inportb(0x3D5) & 0xE0) | cursor_end);
}

void update_cursor(int x, int y) {
  uint16_t pos = y * VGA_WIDTH + x;

  outportb(0x3D4, 0x0F);
  outportb(0x3D5, (uint8_t)(pos & 0xFF));
  outportb(0x3D4, 0x0E);
  outportb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

uint16_t get_cursor_position(void) {
  uint16_t pos = 0;
  outportb(0x3D4, 0x0F);
  pos |= inportb(0x3D5);
  outportb(0x3D4, 0x0E);
  pos |= ((uint16_t)inportb(0x3D5)) << 8;
  return pos;
}

void clrscr() {
  terminal_row = 0;
  terminal_column = 0;
  for (size_t y = 0; y < VGA_HEIGHT; y++) {
    for (size_t x = 0; x < VGA_WIDTH; x++) {
      const size_t index = y * VGA_WIDTH + x;
      terminal_buffer[index] = BLANK;
    }
  }
  update_cursor(terminal_column, terminal_row);
}

void terminal_clrline() {
  for (size_t x = 0; x < VGA_WIDTH; x++) {
    const size_t index = terminal_row * VGA_WIDTH + x;
    terminal_buffer[index] = BLANK;
  }
  terminal_column = 0;
  update_cursor(terminal_column, terminal_row);
}

void terminal_initialize(void) {
  enable_cursor(13, 13);
  
  video_memory_index = 0;
  terminal_color = vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
  BLANK = vga_entry(' ', terminal_color);

  terminal_buffer = VGA_MEMORY;

  clrscr();

  /*
  for (size_t i = 0; i < VIDEO_MEMORY_SIZE; ++i) {
    video_memory[i] = 0;
  }
  */
}

void terminal_setcolor(uint8_t color) {
  terminal_color = color;
}

void terminal_putentryat(unsigned char c, uint8_t color, size_t x, size_t y) {
  const size_t index = y * VGA_WIDTH + x;
  terminal_buffer[index] = vga_entry(c, color);
  // video_memory[video_memory_index] = terminal_buffer[index];
}

void scroll() {
  memmove(terminal_buffer, terminal_buffer + VGA_WIDTH, sizeof(uint16_t) * (VGA_WIDTH * (VGA_HEIGHT - 1)));

  for (int i = 0; i < VGA_WIDTH; ++i) {
    terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + i] = BLANK;
  }

  terminal_row = VGA_HEIGHT - 1;
  terminal_column = 0;
}

void terminal_newline() {
  terminal_column = 0;
  terminal_row++;
  if (terminal_row == VGA_HEIGHT) {
    scroll();
  }
  update_cursor(terminal_column, terminal_row);
}

void terminal_popchar() {
  if (terminal_column == 0 && terminal_row == 0) {
    return;
  }

  do {
    if (terminal_column == 0 && terminal_row != 0) {
      terminal_row--;
      terminal_column = VGA_WIDTH;
    } else {
      terminal_column--;
    }
  } while (terminal_buffer[terminal_row * VGA_WIDTH + terminal_column] == BLANK);

  terminal_buffer[terminal_row * VGA_WIDTH + terminal_column] = BLANK;
  update_cursor(terminal_column, terminal_row);
}

void terminal_clrscr() {
  clrscr();
}

void terminal_putchar(char c) {
  const bool is_special = c == '\n';

  switch (c) {
    case '\n': {
      terminal_newline();
      break;
    }

    default: {

      if (terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        terminal_row++;
        if (terminal_row == VGA_HEIGHT) {
          scroll();
        }
      }

      unsigned char uc = c;
      terminal_putentryat(uc, terminal_color, terminal_column, terminal_row);
      terminal_column++;
    }
  }
}

void terminal_write(const char* data, size_t size) {
  for (size_t i = 0; i < size; i++)
    terminal_putchar(data[i]);

  // It would be faster to update cursor after printing an entire string.
  update_cursor(terminal_column, terminal_row);
}

void terminal_writestring(const char* data) {
  terminal_write(data, strlen(data));
}

extern void shell_start();

void terminal_run() {
  terminal_initialize();
  int tty_master = 0;
  // returns fd of a master terminal
  if ((tty_master = vfs_open("/dev/ptmx", O_RDONLY)) < 0) {
    err("Unable to open ptmx");
  }

  int input_fd;
  if ((input_fd = vfs_open("/dev/pts/0", O_RDONLY)) < 0) {
    err("Cannot open keyboard");
  }

  struct key_event ev;
  struct process *parent = get_current_process();
  
  log("ADDRESS OF PARENT: %x", parent);

  if (process_fork(parent) == 0) {
    shell_start();
  }

  int size = 20;
  char buf[size];
  
  char command[10];

  while (true) {
  newline:
    char key;
    

    kreadterminal(&buf, size);
    

    for (int i = 0; i < size; ++i) {
      key = buf[i];

      switch (key) {
        case '\e':
          // TODO: very dirty implementation, needs to be something like a det machine
          int command_index = 0;
          while (buf[i] != 'm' && buf[i] != '\0') {
            command[command_index] = buf[i];
            ++i;
            ++command_index;
            if (i >= size) { // read more
              kreadterminal(&buf, size);
              i = 0;
            }
          }
          
          if (buf[i] == '\0') {
            assert_not_reached("terminal: invalid parsing of color key");
          }
          
          command[command_index] = 'm';
          command[command_index + 1] = '\0';
          
          int com_size = strlen(command);

          if (memcmp(&command, COLOR_RESET, com_size) == 0) {
            terminal_reset_color();
            break;
          }
          
          enum vga_color color = VGA_COLOR_GREEN;

          if (memcmp(&command, BLK, com_size) == 0) {
            color = VGA_COLOR_BLACK;
          } else if (memcmp(&command, RED, com_size) == 0) {
            color = VGA_COLOR_LIGHT_RED;
          } else if (memcmp(&command, GRN, com_size) == 0) {
            color = VGA_COLOR_GREEN;
          } else if (memcmp(&command, YEL, com_size) == 0) {
            //color = VGA_COLOR_
          } else if (memcmp(&command, BLU, com_size) == 0) {
            color = VGA_COLOR_LIGHT_BLUE;
          } else if (memcmp(&command, MAG, com_size) == 0) {
            color = VGA_COLOR_MAGENTA;
          } else if (memcmp(&command, CYN, com_size) == 0) {
            color = VGA_COLOR_CYAN;
          } else if (memcmp(&command, WHT, com_size) == 0) {
            color = VGA_COLOR_WHITE;
          } else {
            err("terminal: nvalid terminal code, ignoring");
          }

          terminal_set_color(color);
            
          break;

        case '\n':
          terminal_newline();
          break;
        case '\0':
          goto newline;
          break;
        case KEY_BACKSPACE:
        case '\177':
          terminal_popchar();
          break;
        
        case KEY_RETURN:
        case KEY_UNKNOWN:
          break;
        default:
          char c = kkybrd_key_to_ascii(key);
          if (isascii(c)) {
            terminal_write(&c, 1);
          }
          break;
      }
    }
  }
}
