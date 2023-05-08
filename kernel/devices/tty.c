#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "tty.h"
#include "vga.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t* const VGA_MEMORY = (uint16_t*) 0xB8000;
static const size_t VIDEO_MEMORY_SIZE = VGA_WIDTH * VGA_HEIGHT << 2;

static size_t video_memory_index;
static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer;
//static uint16_t video_memory[VIDEO_MEMORY_SIZE]; 

void terminal_initialize(void) {
	terminal_row = 0;
	terminal_column = 0;
  video_memory_index = 0;
	terminal_color = vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
	terminal_buffer = VGA_MEMORY;
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry(' ', terminal_color);
		}
	}

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
  //video_memory[video_memory_index] = terminal_buffer[index];
}

void scroll() {
  memmove(terminal_buffer, terminal_buffer + VGA_WIDTH, sizeof(uint16_t) * (VGA_WIDTH * (VGA_HEIGHT - 1)));

  for (int i = 0; i < VGA_WIDTH; ++i) {
    terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + i] = vga_entry(' ', terminal_color);
  }

  terminal_row = VGA_HEIGHT - 1;
  terminal_column = 0;
}

void terminal_popchar() {
  if (terminal_column == 0 && terminal_row == 0) {
    return;
  }

  terminal_buffer[terminal_row * VGA_WIDTH + terminal_column] = vga_entry(' ', terminal_color);
  if (terminal_column == 0 && terminal_row != 0) {
    terminal_row--;
    terminal_column = VGA_WIDTH;
  } else {
    terminal_column--;
  }
}

void terminal_putchar(char c) {
  const bool is_special = c == '\n';

  switch (c) 
  {
    case '\n': {
      terminal_column = 0;
      terminal_row++;
      if (terminal_row == VGA_HEIGHT) {
        scroll();
      }
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
}

void terminal_writestring(const char* data) {
	terminal_write(data, strlen(data));
}
